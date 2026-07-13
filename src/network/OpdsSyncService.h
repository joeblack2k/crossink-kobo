#pragma once

#include <OpdsParser.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "OpdsServerStore.h"

// Kobo owns OPDS network I/O in this service, never in an Activity::loop().
// The worker only returns immutable results; catalog persistence and UI state
// stay on the app thread, which avoids concurrent access to PersistableStore.
class OpdsSyncService {
 public:
  // Cover jobs deliberately run behind catalog and EPUB transfers.  Kobo has
  // one worker, so this also gives the UI a hard upper bound of one active
  // cover decode/download at a time.
  enum class JobKind : uint8_t {
    CatalogRefresh,
    CoverFetch,
    CoverConvert,
    LocalCover,
    BookDownload,
    BulkBookDownload,
    Reconcile
  };
  enum class ResultCode : uint8_t { Ok, Cancelled, FetchFailed, ParseFailed, FileFailed };

  struct Result {
    uint64_t id = 0;
    JobKind kind = JobKind::CatalogRefresh;
    ResultCode code = ResultCode::FetchFailed;
    std::vector<OpdsEntry> entries;
    // All book entries from a complete paginated refresh. `entries` remains
    // the first feed page for the interactive browser.
    std::vector<OpdsEntry> catalogEntries;
    std::string searchTemplate;
    std::string nextUrl;
    std::string previousUrl;
    std::string destinationPath;
    std::string detail;
    bool truncated = false;
  };

  struct Progress {
    uint64_t id = 0;
    size_t completedBytes = 0;
    size_t totalBytes = 0;
    bool running = false;
  };

  // Internal queue payload. It is public only so the platform worker state can
  // own a standard container without exposing construction to callers.
  struct Job;

  static OpdsSyncService& getInstance();

  uint64_t enqueueCatalogRefresh(const OpdsServer& server, std::string url);
  uint64_t enqueueBookDownload(const OpdsServer& server, std::string url, std::string destinationPath);
  // A bulk offline copy must never queue-jump visible cover work or an
  // explicit user-initiated book open.
  uint64_t enqueueBulkBookDownload(const OpdsServer& server, std::string url, std::string destinationPath);
  uint64_t enqueueCoverFetch(const OpdsServer& server, std::string url, std::string destinationPath);
  uint64_t enqueueCoverConvert(std::string sourcePath, std::string destinationPath);
  uint64_t enqueueLocalCover(std::string sourcePath);
  uint64_t enqueueReconcile();
  bool takeResult(uint64_t id, Result& result);
  Progress progress(uint64_t id) const;
  void cancel(uint64_t id);

  // The suspend path pauses dequeueing before the kernel transition. An
  // in-flight HTTP client is cancelled cooperatively; resume keeps queued jobs.
  void prepareSuspend();
  void resumeAfterSuspend();

 private:
  OpdsSyncService();
  ~OpdsSyncService();
  OpdsSyncService(const OpdsSyncService&) = delete;
  OpdsSyncService& operator=(const OpdsSyncService&) = delete;

  uint64_t enqueue(Job job);
  void execute(Job job);
  void workerMain();

  mutable void* impl = nullptr;
};

#define OPDS_SYNC OpdsSyncService::getInstance()
