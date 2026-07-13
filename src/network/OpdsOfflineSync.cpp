#include "network/OpdsOfflineSync.h"

#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include <utility>

#include "OpdsCatalogStore.h"
#include "OpdsServerStore.h"
#include "network/OpdsSyncService.h"
#include "util/BookCacheUtils.h"
#include "util/StringUtils.h"
#include "util/UrlUtils.h"

struct OpdsOfflineSync::QueueItem {
  OpdsEntry entry;
  OpdsServer server;
};

namespace {
std::string filenameBase(const OpdsEntry& book, const OpdsFilenameFormat format) {
  if (book.author.empty()) return book.title;
  if (book.title.empty()) return book.author;
  return format == OpdsFilenameFormat::TITLE_AUTHOR ? book.title + " - " + book.author
                                                     : book.author + " - " + book.title;
}
}  // namespace

OpdsOfflineSync& OpdsOfflineSync::getInstance() {
  static OpdsOfflineSync instance;
  return instance;
}

void OpdsOfflineSync::startPrimaryIfEnabled() {
  if (running || activeJobId != 0 || !queue.empty() || WiFi.status() != WL_CONNECTED) return;
  const OpdsServer* server = OPDS_STORE.getServer(0);
  if (!server || !server->syncAllBooks) return;
  const auto catalog = OPDS_CATALOG.getBooksForServer(server->id);
  queue.reserve(catalog.size());
  for (const auto& book : catalog) {
    if (book.availability == OpdsCatalogAvailability::AvailableOffline || book.acquisitionHref.empty()) continue;
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
  activePath = directory + "/" + StringUtils::sanitizeFilename(filenameBase(item.entry, item.server.filenameFormat)) +
               ".epub";
  activeTemporaryPath = activePath + ".part";
  const std::string url = item.entry.href.rfind("http://", 0) == 0 || item.entry.href.rfind("https://", 0) == 0
                              ? item.entry.href
                              : UrlUtils::buildUrl(item.server.url, item.entry.href);
  if (!OPDS_CATALOG.markAvailability(item.server.id, item.entry, OpdsCatalogAvailability::Downloading, activePath)) {
    lastError = "OPDS catalog update failed";
    return false;
  }
  Storage.remove(activeTemporaryPath.c_str());
  activeJobId = OPDS_SYNC.enqueueBulkBookDownload(item.server, url, activeTemporaryPath);
  if (activeJobId == 0) {
    OPDS_CATALOG.markAvailability(item.server.id, item.entry, OpdsCatalogAvailability::DownloadFailed);
    lastError = "OPDS transfer queue unavailable";
    return false;
  }
  return true;
}

bool OpdsOfflineSync::tick() {
  if (activeJobId == 0) return false;
  OpdsSyncService::Result result;
  if (!OPDS_SYNC.takeResult(activeJobId, result)) return false;
  activeJobId = 0;
  if (nextIndex >= queue.size()) {
    running = false;
    return false;
  }
  const QueueItem& item = queue[nextIndex];
  if (result.code == OpdsSyncService::ResultCode::Ok && Storage.exists(activeTemporaryPath.c_str()) &&
      Storage.rename(activeTemporaryPath.c_str(), activePath.c_str())) {
    clearBookCache(activePath);
    OPDS_CATALOG.markAvailability(item.server.id, item.entry, OpdsCatalogAvailability::AvailableOffline, activePath);
  } else {
    OPDS_CATALOG.markAvailability(item.server.id, item.entry, OpdsCatalogAvailability::DownloadFailed);
    lastError = result.detail.empty() ? "OPDS book download failed" : result.detail;
    LOG_ERR("OPDSALL", "%s", lastError.c_str());
  }
  ++nextIndex;
  if (!queueNext()) {
    if (nextIndex >= queue.size()) queue.clear();
  }
  return true;
}

OpdsOfflineSync::Status OpdsOfflineSync::status() const {
  return Status{running, nextIndex, queue.size(), lastError};
}
