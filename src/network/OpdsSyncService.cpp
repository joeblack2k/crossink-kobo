#include "OpdsSyncService.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <JpegToBmpConverter.h>
#include <Logging.h>
#include <Memory.h>
#include <OpdsStream.h>
#include <PngToBmpConverter.h>
#include <Xtc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <deque>
#include <memory>
#include <mutex>
#include <new>
#include <utility>

#if defined(KOBO_LINUX)
#include <condition_variable>
#include <thread>
#endif

#include "network/HttpDownloader.h"
#include "util/UrlUtils.h"

struct OpdsSyncService::Job {
  uint64_t id = 0;
  JobKind kind = JobKind::CatalogRefresh;
  OpdsServer server;
  std::string url;
  std::string destinationPath;
  std::string sourcePath;
  bool catalogOnly = false;
  bool resumePartial = false;
};

namespace {
struct ServiceState {
  std::mutex mutex;
#if defined(KOBO_LINUX)
  std::condition_variable wake;
#endif
  std::deque<OpdsSyncService::Job> queued;
  std::deque<OpdsSyncService::Result> completed;
  OpdsSyncService::Progress progress;
#if defined(KOBO_LINUX)
  std::thread worker;
#endif
  uint64_t nextId = 1;
  uint64_t cancelledId = 0;
  bool stop = false;
  bool suspendPending = false;
};

ServiceState& stateFor(void* impl) { return *static_cast<ServiceState*>(impl); }

OpdsSyncService::ResultCode downloadCode(const HttpDownloader::DownloadError value) {
  if (value == HttpDownloader::OK) return OpdsSyncService::ResultCode::Ok;
  if (value == HttpDownloader::ABORTED) return OpdsSyncService::ResultCode::Cancelled;
  return value == HttpDownloader::FILE_ERROR ? OpdsSyncService::ResultCode::FileFailed
                                             : OpdsSyncService::ResultCode::FetchFailed;
}

enum class CoverFormat : uint8_t { Unsupported, Jpeg, Png };

constexpr uint64_t kMaximumCoverSourceBytes = 16ULL * 1024ULL * 1024ULL;

CoverFormat coverFormatForPath(const std::string& path) {
  FsFile input;
  if (!Storage.openFileForRead("OPDSCOV", path, input)) return CoverFormat::Unsupported;
  const uint64_t size = input.size();
  if (size == 0 || size > kMaximumCoverSourceBytes) {
    LOG_ERR("OPDSCOV", "Rejected cover source size: %llu", static_cast<unsigned long long>(size));
    input.close();
    return CoverFormat::Unsupported;
  }
  std::array<uint8_t, 8> header{};
  const int read = input.read(header.data(), header.size());
  input.close();
  if (read >= 3 && header[0] == 0xff && header[1] == 0xd8 && header[2] == 0xff) return CoverFormat::Jpeg;
  static constexpr std::array<uint8_t, 8> kPngSignature = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
  if (read == static_cast<int>(kPngSignature.size()) && header == kPngSignature) return CoverFormat::Png;
  return CoverFormat::Unsupported;
}

bool convertCoverToBmp(const std::string& sourcePath, const std::string& destinationPath, std::string& detail) {
  const CoverFormat format = coverFormatForPath(sourcePath);
  if (format == CoverFormat::Unsupported) {
    detail = "unsupported or oversized cover media";
    return false;
  }
  const std::string temporaryPath = destinationPath + ".part";
  Storage.remove(temporaryPath.c_str());
  FsFile input;
  HalFile output;
  if (!Storage.openFileForRead("OPDSCOV", sourcePath, input) ||
      !Storage.openFileForWrite("OPDSCOV", temporaryPath, output)) {
    if (input.isOpen()) input.close();
    if (output.isOpen()) output.close();
    Storage.remove(temporaryPath.c_str());
    detail = "cover cache file open failed";
    return false;
  }
  constexpr int kCoverWidth = 300;
  constexpr int kCoverHeight = 450;
  const bool converted =
      format == CoverFormat::Jpeg
          ? JpegToBmpConverter::jpegFileTo1BitBmpStreamWithSize(input, output, kCoverWidth, kCoverHeight, true)
          : PngToBmpConverter::pngFileTo1BitBmpStreamWithSize(input, output, kCoverWidth, kCoverHeight, true);
  const bool durable = converted && output.sync();
  input.close();
  output.close();
  if (!durable || !Storage.rename(temporaryPath.c_str(), destinationPath.c_str())) {
    Storage.remove(temporaryPath.c_str());
    detail = converted ? "cover cache rename failed" : "cover decoder rejected image";
    return false;
  }
  return true;
}
}  // namespace

OpdsSyncService& OpdsSyncService::getInstance() {
  static OpdsSyncService instance;
  return instance;
}

OpdsSyncService::OpdsSyncService() {
  impl = new (std::nothrow) ServiceState();
  if (!impl) {
    LOG_ERR("OPDSJOB", "Could not allocate worker state");
    return;
  }
#if defined(KOBO_LINUX)
  auto& state = stateFor(impl);
  state.worker = std::thread([this] { workerMain(); });
#endif
}

OpdsSyncService::~OpdsSyncService() {
  if (!impl) return;
  auto& state = stateFor(impl);
  {
    std::lock_guard lock(state.mutex);
    state.stop = true;
  }
#if defined(KOBO_LINUX)
  state.wake.notify_all();
  if (state.worker.joinable()) state.worker.join();
#endif
  delete &state;
  impl = nullptr;
}

uint64_t OpdsSyncService::enqueueCatalogRefresh(const OpdsServer& server, std::string url, const bool catalogOnly) {
  Job job{0, JobKind::CatalogRefresh, server, std::move(url), {}, {}};
  job.catalogOnly = catalogOnly;
  return enqueue(std::move(job));
}

uint64_t OpdsSyncService::enqueueBookDownload(const OpdsServer& server, std::string url, std::string destinationPath) {
  return enqueue(Job{0, JobKind::BookDownload, server, std::move(url), std::move(destinationPath), {}});
}

uint64_t OpdsSyncService::enqueueBulkBookDownload(const OpdsServer& server, std::string url,
                                                  std::string destinationPath, const bool resumePartial) {
  Job job{0, JobKind::BulkBookDownload, server, std::move(url), std::move(destinationPath), {}};
  job.resumePartial = resumePartial;
  return enqueue(std::move(job));
}

uint64_t OpdsSyncService::enqueueCoverFetch(const OpdsServer& server, std::string url, std::string destinationPath) {
  return enqueue(Job{0, JobKind::CoverFetch, server, std::move(url), std::move(destinationPath), {}});
}

uint64_t OpdsSyncService::enqueueCoverConvert(std::string sourcePath, std::string destinationPath) {
  Job job;
  job.kind = JobKind::CoverConvert;
  job.sourcePath = std::move(sourcePath);
  job.destinationPath = std::move(destinationPath);
  return enqueue(std::move(job));
}

uint64_t OpdsSyncService::enqueueLocalCover(std::string sourcePath) {
  Job job;
  job.kind = JobKind::LocalCover;
  job.sourcePath = std::move(sourcePath);
  return enqueue(std::move(job));
}

uint64_t OpdsSyncService::enqueueReconcile() { return enqueue(Job{0, JobKind::Reconcile, {}, {}, {}, {}}); }

uint64_t OpdsSyncService::enqueue(Job job) {
  if (!impl) return 0;
  auto& state = stateFor(impl);
  uint64_t id = 0;
  {
    std::lock_guard lock(state.mutex);
    job.id = state.nextId++;
    id = job.id;
#if defined(KOBO_LINUX)
    const bool lowPriority = job.kind == JobKind::CoverFetch || job.kind == JobKind::CoverConvert ||
                             job.kind == JobKind::LocalCover || job.kind == JobKind::BulkBookDownload;
    if (lowPriority) {
      state.queued.push_back(std::move(job));
    } else {
      const auto firstCover = std::find_if(state.queued.begin(), state.queued.end(), [](const Job& pending) {
        return pending.kind == JobKind::CoverFetch || pending.kind == JobKind::CoverConvert ||
               pending.kind == JobKind::LocalCover || pending.kind == JobKind::BulkBookDownload;
      });
      state.queued.insert(firstCover, std::move(job));
    }
#endif
  }
#if defined(KOBO_LINUX)
  state.wake.notify_one();
#else
  // ESP/simulator retain their existing single-threaded behaviour. Kobo alone
  // enables the pthread worker, which keeps this shared class source portable.
  execute(std::move(job));
#endif
  return id;
}

bool OpdsSyncService::takeResult(const uint64_t id, Result& result) {
  if (!impl || id == 0) return false;
  auto& state = stateFor(impl);
  std::lock_guard lock(state.mutex);
  const auto found = std::find_if(state.completed.begin(), state.completed.end(),
                                  [id](const Result& candidate) { return candidate.id == id; });
  if (found == state.completed.end()) return false;
  result = std::move(*found);
  state.completed.erase(found);
  return true;
}

OpdsSyncService::Progress OpdsSyncService::progress(const uint64_t id) const {
  if (!impl || id == 0) return {};
  auto& state = stateFor(impl);
  std::lock_guard lock(state.mutex);
  return state.progress.id == id ? state.progress : Progress{};
}

void OpdsSyncService::cancel(const uint64_t id) {
  if (!impl || id == 0) return;
  auto& state = stateFor(impl);
  std::lock_guard lock(state.mutex);
  state.cancelledId = id;
}

void OpdsSyncService::prepareSuspend() {
  if (!impl) return;
  auto& state = stateFor(impl);
  {
    std::lock_guard lock(state.mutex);
    state.suspendPending = true;
    if (state.progress.running) state.cancelledId = state.progress.id;
  }
#if defined(KOBO_LINUX)
  state.wake.notify_all();
#endif
}

void OpdsSyncService::resumeAfterSuspend() {
  if (!impl) return;
  auto& state = stateFor(impl);
  {
    std::lock_guard lock(state.mutex);
    state.suspendPending = false;
  }
#if defined(KOBO_LINUX)
  state.wake.notify_all();
#endif
}

void OpdsSyncService::workerMain() {
#if defined(KOBO_LINUX)
  auto& state = stateFor(impl);
  for (;;) {
    Job job;
    {
      std::unique_lock lock(state.mutex);
      state.wake.wait(lock, [&] { return state.stop || (!state.suspendPending && !state.queued.empty()); });
      if (state.stop) return;
      job = std::move(state.queued.front());
      state.queued.pop_front();
    }
    execute(std::move(job));
  }
#endif
}

void OpdsSyncService::execute(Job job) {
  Result result;
  result.id = job.id;
  result.kind = job.kind;
  result.destinationPath = job.destinationPath;
  result.code = ResultCode::FetchFailed;

  auto cancelled = [this, id = job.id] {
    if (!impl) return true;
    auto& state = stateFor(impl);
    std::lock_guard lock(state.mutex);
    return state.stop || state.suspendPending || state.cancelledId == id;
  };
  {
    auto& state = stateFor(impl);
    std::lock_guard lock(state.mutex);
    state.progress = Progress{job.id, 0, 0, true};
  }

  if (job.kind == JobKind::CatalogRefresh) {
    std::string url = job.url;
    std::vector<std::string> visited;
    constexpr size_t kMaximumPages = 32;
    bool followedCatalogRoot = false;
    for (size_t page = 0; page < kMaximumPages && !url.empty(); ++page) {
      if (std::find(visited.begin(), visited.end(), url) != visited.end()) {
        result.code = ResultCode::ParseFailed;
        result.detail = "pagination loop";
        break;
      }
      visited.push_back(url);
      auto entries = makeUniqueNoThrow<OpdsEntry[]>(MAX_OPDS_FEED_ENTRIES);
      if (!entries) {
        result.code = ResultCode::FileFailed;
        break;
      }
      OpdsParser parser(entries.get(), MAX_OPDS_FEED_ENTRIES);
      OpdsParserStream stream{parser};
      if (!HttpDownloader::fetchUrl(url, stream, job.server.username, job.server.password)) {
        result.code = cancelled() ? ResultCode::Cancelled : ResultCode::FetchFailed;
        break;
      }
      if (!parser) {
        result.code = ResultCode::ParseFailed;
        break;
      }
      const size_t count = parser.getEntryCount();
      if (job.catalogOnly && !followedCatalogRoot) {
        const auto allBooks = std::find_if(entries.get(), entries.get() + count, [](const OpdsEntry& entry) {
          if (entry.type != OpdsEntryType::NAVIGATION) return false;
          std::string title = entry.title;
          std::transform(title.begin(), title.end(), title.begin(),
                         [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
          return title == "all books";
        });
        if (allBooks != entries.get() + count) {
          url = allBooks->href.rfind("http", 0) == 0 ? allBooks->href : UrlUtils::buildUrl(url, allBooks->href);
          followedCatalogRoot = true;
          continue;
        }
        followedCatalogRoot = true;
      }
      if (page == 0) {
        result.entries.assign(entries.get(), entries.get() + count);
        result.searchTemplate = parser.getSearchTemplate();
        result.nextUrl = parser.getNextPageUrl();
        result.previousUrl = parser.getPrevPageUrl();
        result.truncated = parser.wasTruncated();
      }
      for (size_t index = 0; index < count; ++index) {
        if (entries[index].type == OpdsEntryType::BOOK) result.catalogEntries.push_back(entries[index]);
      }
      const std::string next = parser.getNextPageUrl();
      if (next.empty()) {
        result.code = ResultCode::Ok;
        break;
      }
      url = next.rfind("http", 0) == 0 ? next : UrlUtils::buildUrl(url, next);
    }
  } else if (job.kind == JobKind::BookDownload || job.kind == JobKind::BulkBookDownload ||
             job.kind == JobKind::CoverFetch) {
    bool cancelledFlag = false;
    HttpDownloader::DownloadOptions options;
    if (job.kind == JobKind::BulkBookDownload && job.resumePartial) {
      options.preservePartial = true;
      options.resumePartial = true;
    }
    options.shouldCancel = [&] { return cancelled(); };
    options.bufferSize = 2048;
    const auto status = HttpDownloader::downloadToFile(
        job.url, job.destinationPath,
        [this, id = job.id](const size_t done, const size_t total) {
          if (!impl) return;
          auto& state = stateFor(impl);
          std::lock_guard lock(state.mutex);
          if (state.progress.id == id) {
            state.progress.completedBytes = done;
            state.progress.totalBytes = total;
          }
        },
        &cancelledFlag, job.server.username, job.server.password, options);
    result.code = downloadCode(status);
  } else if (job.kind == JobKind::CoverConvert) {
    result.code =
        cancelled() ? ResultCode::Cancelled
                    : (convertCoverToBmp(job.sourcePath, job.destinationPath, result.detail) ? ResultCode::Ok
                                                                                             : ResultCode::FileFailed);
  } else if (job.kind == JobKind::LocalCover) {
    bool generated = false;
    if (FsHelpers::hasEpubExtension(job.sourcePath)) {
      Epub epub(job.sourcePath, "/.crosspoint");
      generated = epub.load(true, true) && epub.generateThumbBmp(300, 450, nullptr);
      if (generated) result.destinationPath = epub.getThumbBmpPath(300, 450);
    } else if (FsHelpers::hasXtcExtension(job.sourcePath)) {
      Xtc xtc(job.sourcePath, "/.crosspoint");
      generated = xtc.load() && xtc.generateThumbBmp(300, 450);
      if (generated) result.destinationPath = xtc.getThumbBmpPath(300, 450);
    } else {
      result.detail = "local book has no cover generator";
    }
    if (!generated && result.detail.empty()) result.detail = "local cover generation failed";
    result.code = cancelled() ? ResultCode::Cancelled : (generated ? ResultCode::Ok : ResultCode::FileFailed);
  } else {
    result.code = cancelled() ? ResultCode::Cancelled : ResultCode::Ok;
  }

  auto& state = stateFor(impl);
  {
    std::lock_guard lock(state.mutex);
    if (state.progress.id == job.id) state.progress.running = false;
    if (state.cancelledId == job.id) state.cancelledId = 0;
    // Every submitted job has exactly one owner that waits for this result.
    // Dropping a completed result here strands that owner forever in
    // Downloading/Queued state. This was visible with a 55-book catalog: the
    // worker completed cover jobs faster than the app thread could consume
    // them, and the old 16-result cap silently lost the early completions.
    // The Kobo worker is single-flight and owners drain on every app loop, so
    // retaining results is bounded by the submitted work rather than by a
    // render-frame race.
    state.completed.push_back(std::move(result));
  }
}
