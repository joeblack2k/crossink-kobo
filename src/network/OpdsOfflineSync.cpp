#include "network/OpdsOfflineSync.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include <utility>

#include "OpdsCatalogStore.h"
#include "OpdsServerStore.h"
#include "network/OpdsBookStorage.h"
#include "network/OpdsEpubValidator.h"
#include "network/OpdsSyncService.h"
#include "util/BookCacheUtils.h"
#include "util/UrlUtils.h"

struct OpdsOfflineSync::QueueItem {
  OpdsEntry entry;
  OpdsServer server;
};

OpdsOfflineSync& OpdsOfflineSync::getInstance() {
  static OpdsOfflineSync instance;
  return instance;
}

void OpdsOfflineSync::requestCatalogRefresh() {
  // A header tap while the worker is already refreshing is acknowledgement,
  // not a request to immediately repeat the complete catalog scan.  Keeping
  // this idempotent prevents boot/header races from spending WLAN and panel
  // time on an identical second refresh.
  if (catalogRefreshJobId == 0) refreshRequested = true;
}

void OpdsOfflineSync::pause() {
  paused = true;
  if (activeJobId != 0) {
    pauseCancellationPending = true;
    OPDS_SYNC.cancel(activeJobId);
  }
  if (catalogRefreshJobId != 0) {
    catalogPauseCancellationPending = true;
    OPDS_SYNC.cancel(catalogRefreshJobId);
  }
}

void OpdsOfflineSync::resume() {
  paused = false;
  if (catalogRefreshJobId == 0) refreshRequested = true;
}

void OpdsOfflineSync::cancel() {
  cancelRequested = true;
  pauseCancellationPending = false;
  catalogPauseCancellationPending = false;
  if (activeJobId != 0) {
    OPDS_SYNC.cancel(activeJobId);
  } else {
    queue.clear();
    running = false;
    cancelRequested = false;
  }
  if (catalogRefreshJobId != 0) OPDS_SYNC.cancel(catalogRefreshJobId);
  refreshRequested = false;
}

void OpdsOfflineSync::startPrimaryIfEnabled() {
  // A full snapshot is the source of truth for Sync All.  In particular after
  // boot, do not consume an older on-disk catalog while its replacement is in
  // flight: removed books and changed acquisition URLs must not be queued.
  if (paused || refreshRequested || catalogRefreshJobId != 0 || running || activeJobId != 0 || !queue.empty() ||
      WiFi.status() != WL_CONNECTED) {
    return;
  }
  const OpdsServer* server = OPDS_STORE.getServer(0);
  if (!server || !server->syncAllBooks) return;
  // A manual deletion or a pre-stable catalog may have left an offline bit
  // behind. Reconcile before the generation guard so missing books are
  // eligible for this same Sync All pass.
  if (OPDS_CATALOG.reconcileLocalFiles()) lastStartedGeneration = 0;
  const uint32_t generation = OPDS_CATALOG.snapshotGeneration();
  if (generation == 0) {
    requestCatalogRefresh();
    return;
  }
  if (generation == lastStartedGeneration) return;
  const auto catalog = OPDS_CATALOG.getBooksForServer(server->id);
  queue.reserve(catalog.size());
  for (const auto& book : catalog) {
    if ((book.availability == OpdsCatalogAvailability::AvailableOffline && !book.updateAvailable) ||
        book.acquisitionHref.empty()) {
      continue;
    }
    OpdsEntry entry;
    entry.type = OpdsEntryType::BOOK;
    entry.id = book.entryId;
    entry.title = book.title;
    entry.author = book.author;
    entry.href = book.acquisitionHref;
    entry.acquisitionType = book.acquisitionType;
    queue.push_back(QueueItem{std::move(entry), *server});
  }
  if (queue.empty()) return;
  running = true;
  lastStartedGeneration = generation;
  nextIndex = 0;
  lastError.clear();
  LOG_INF("OPDSALL", "Queued %zu offline OPDS books", queue.size());
  if (!queueNext()) {
    running = false;
    queue.clear();
  }
}

bool OpdsOfflineSync::queueNext() {
  if (nextIndex >= queue.size()) {
    running = false;
    LOG_INF("OPDSALL", "Offline OPDS sync complete");
    return false;
  }
  const QueueItem& item = queue[nextIndex];
  const std::string directory = "/Books/OPDS/" + OpdsCatalogStore::serverKeyForIdentity(item.server.id);
  if (!Storage.ensureDirectoryExists(directory.c_str())) {
    lastError = "OPDS storage directory unavailable";
    return false;
  }
  activePath =
      OpdsBookStorage::downloadPath(item.server.id, item.server.filenameFormat == OpdsFilenameFormat::TITLE_AUTHOR,
                                    OpdsCatalogStore::stableBookId(item.entry), item.entry.title, item.entry.author);
  activeTemporaryPath = activePath + ".part";
  const std::string url = item.entry.href.rfind("http://", 0) == 0 || item.entry.href.rfind("https://", 0) == 0
                              ? item.entry.href
                              : UrlUtils::buildUrl(item.server.url, item.entry.href);
  if (!OPDS_CATALOG.markAvailability(item.server.id, item.entry, OpdsCatalogAvailability::Downloading, activePath)) {
    lastError = "OPDS catalog update failed";
    return false;
  }
  activeJobId = OPDS_SYNC.enqueueBulkBookDownload(item.server, url, activeTemporaryPath);
  if (activeJobId == 0) {
    OPDS_CATALOG.markAvailability(item.server.id, item.entry, OpdsCatalogAvailability::DownloadFailed);
    lastError = "OPDS transfer queue unavailable";
    return false;
  }
  lastReportedProgressJobId = activeJobId;
  lastReportedProgressBytes = 0;
  return true;
}

bool OpdsOfflineSync::processCatalogRefresh() {
  if (catalogRefreshJobId == 0) return false;
  OpdsSyncService::Result result;
  if (!OPDS_SYNC.takeResult(catalogRefreshJobId, result)) return false;
  catalogRefreshJobId = 0;
  if (result.code == OpdsSyncService::ResultCode::Cancelled && catalogPauseCancellationPending) {
    catalogPauseCancellationPending = false;
    // resume() can run before this worker result is delivered.  Keep the
    // intent so the next unpaused tick actually retries metadata refresh.
    refreshRequested = true;
    return true;
  }
  catalogPauseCancellationPending = false;
  // An empty, successfully parsed All Books feed is a valid authoritative
  // snapshot.  Treating it as a failed refresh left every old remote card in
  // place when a server had removed its last title.  The parser/result code
  // already distinguishes an empty-but-valid feed from fetch and parse
  // failures, so snapshot replacement must run for both non-empty and empty
  // catalogs.
  if (result.code != OpdsSyncService::ResultCode::Ok) {
    lastError = result.detail.empty() ? "OPDS catalog refresh failed" : result.detail;
    LOG_ERR("OPDSALL", "%s", lastError.c_str());
    return true;
  }
  const OpdsServer* server = OPDS_STORE.getServer(0);
  if (!server || !OPDS_CATALOG.replaceServerSnapshot(server->id, result.catalogEntries)) {
    lastError = "OPDS catalog update failed";
    LOG_ERR("OPDSALL", "%s", lastError.c_str());
    return true;
  }
  lastError.clear();
  lastSuccessMs = millis();
  LOG_INF("OPDSALL", "Refreshed OPDS snapshot: %zu books", result.catalogEntries.size());
  startPrimaryIfEnabled();
  return true;
}

bool OpdsOfflineSync::validateAndPublishCurrent() {
  std::string detail;
  if (!validateOpdsEpubArchive(activeTemporaryPath, detail)) {
    lastError = detail;
    return false;
  }
  if (!Storage.rename(activeTemporaryPath.c_str(), activePath.c_str())) {
    lastError = "OPDS EPUB publish rename failed";
    return false;
  }
  return true;
}

bool OpdsOfflineSync::tick() {
  bool changed = processCatalogRefresh();
  if (paused) return changed;
  if (refreshRequested && catalogRefreshJobId == 0 && WiFi.status() == WL_CONNECTED) {
    const OpdsServer* server = OPDS_STORE.getServer(0);
    refreshRequested = false;
    if (server && !server->url.empty()) {
      catalogRefreshJobId = OPDS_SYNC.enqueueCatalogRefresh(*server, server->url, true);
      if (catalogRefreshJobId == 0) {
        lastError = "OPDS metadata queue unavailable";
        changed = true;
      } else {
        LOG_INF("OPDSALL", "Queued background OPDS refresh");
        changed = true;
      }
    }
  }
  if (!running && activeJobId == 0) startPrimaryIfEnabled();
  if (running && activeJobId == 0 && !paused) {
    if (!queueNext()) {
      running = false;
      queue.clear();
    }
    changed = true;
  }
  if (activeJobId == 0) {
    if (changed) ++catalogChangeSerial;
    return changed;
  }
  const auto progress = OPDS_SYNC.progress(activeJobId);
  // Request partial UI refreshes only after meaningful transfer progress. The
  // Kobo must never repaint on every network buffer, but a long EPUB should
  // still expose current bytes in the primary-server status row.
  constexpr size_t kProgressRefreshQuantum = 64 * 1024;
  if (progress.id == activeJobId && (lastReportedProgressJobId != activeJobId ||
                                     progress.completedBytes >= lastReportedProgressBytes + kProgressRefreshQuantum ||
                                     (progress.totalBytes != 0 && progress.completedBytes == progress.totalBytes))) {
    lastReportedProgressJobId = activeJobId;
    lastReportedProgressBytes = progress.completedBytes;
    changed = true;
  }
  OpdsSyncService::Result result;
  if (!OPDS_SYNC.takeResult(activeJobId, result)) return changed;
  activeJobId = 0;
  if (nextIndex >= queue.size()) {
    running = false;
    return false;
  }
  const QueueItem& item = queue[nextIndex];
  if (result.code == OpdsSyncService::ResultCode::Cancelled && cancelRequested) {
    Storage.remove(activeTemporaryPath.c_str());
    OPDS_CATALOG.markAvailability(item.server.id, item.entry, OpdsCatalogAvailability::RemoteOnly);
    queue.clear();
    running = false;
    cancelRequested = false;
    ++catalogChangeSerial;
    return true;
  }
  if (result.code == OpdsSyncService::ResultCode::Cancelled && (paused || pauseCancellationPending)) {
    // Suspend/pause keeps the `.part` file and retries the same item after
    // resume; no remote card is incorrectly labelled as a failed download.
    pauseCancellationPending = false;
    ++catalogChangeSerial;
    return true;
  }
  pauseCancellationPending = false;
  if (result.code == OpdsSyncService::ResultCode::Ok && Storage.exists(activeTemporaryPath.c_str()) &&
      validateAndPublishCurrent()) {
    clearBookCache(activePath);
    OPDS_CATALOG.markAvailability(item.server.id, item.entry, OpdsCatalogAvailability::AvailableOffline, activePath);
    // A prior card can remain DownloadFailed and is visible/retryable in the
    // grid, but it must not leave the whole worker in a false global failure
    // state after a later transfer completed successfully.
    lastError.clear();
  } else {
    OPDS_CATALOG.markAvailability(item.server.id, item.entry, OpdsCatalogAvailability::DownloadFailed);
    if (lastError.empty()) lastError = result.detail.empty() ? "OPDS book download failed" : result.detail;
    LOG_ERR("OPDSALL", "%s", lastError.c_str());
  }
  ++nextIndex;
  if (!queueNext()) {
    running = false;
    queue.clear();
  }
  ++catalogChangeSerial;
  return true;
}

OpdsOfflineSync::Status OpdsOfflineSync::status() const {
  Phase phase = Phase::Idle;
  if (paused)
    phase = Phase::Paused;
  else if (catalogRefreshJobId != 0)
    phase = Phase::SyncingMetadata;
  else if (running || activeJobId != 0)
    phase = Phase::Downloading;
  else if (!lastError.empty())
    phase = Phase::Failed;
  else if (WiFi.status() != WL_CONNECTED)
    phase = Phase::Offline;
  const auto progress = activeJobId == 0 ? OpdsSyncService::Progress{} : OPDS_SYNC.progress(activeJobId);
  return Status{phase,
                running,
                nextIndex,
                queue.size(),
                progress.completedBytes,
                progress.totalBytes,
                WiFi.status() == WL_CONNECTED,
                lastError,
                lastSuccessMs};
}
