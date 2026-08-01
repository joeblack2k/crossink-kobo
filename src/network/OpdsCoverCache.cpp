#include "network/OpdsCoverCache.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstddef>
#include <utility>

#include "OpdsCatalogStore.h"
#include "OpdsServerStore.h"
#include "network/OpdsSyncService.h"
#include "util/UrlUtils.h"

struct OpdsCoverCache::Pending {
  enum class Stage : uint8_t { Fetch, Convert, Done };

  std::string serverId;
  std::string entryId;
  std::string coverUrl;
  std::string sourcePath;
  std::string bmpPath;
  uint64_t jobId = 0;
  uint32_t retryAfterMs = 0;
  uint8_t failures = 0;
  State state = State::Missing;
  Stage stage = Stage::Fetch;
};

namespace {
constexpr char kCacheRoot[] = "/.crosspoint/opds-covers";
constexpr uint32_t kFirstRetryMs = 15000;
constexpr uint32_t kMaximumRetryMs = 5 * 60 * 1000;
constexpr size_t kMaxChanges = 24;

std::string hashPathComponent(const std::string& value) {
  uint64_t hash = 14695981039346656037ull;
  for (const unsigned char byte : value) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result(16, '0');
  for (int index = 15; index >= 0; --index) {
    result[static_cast<size_t>(index)] = kHex[hash & 0x0fu];
    hash >>= 4u;
  }
  return result;
}

bool elapsed(const uint32_t now, const uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

uint32_t retryDelay(const uint8_t failures) {
  const uint8_t exponent = std::min<uint8_t>(failures, 5);
  return std::min<uint32_t>(kFirstRetryMs << exponent, kMaximumRetryMs);
}
}  // namespace

OpdsCoverCache& OpdsCoverCache::getInstance() {
  static OpdsCoverCache instance;
  return instance;
}

void OpdsCoverCache::request(const std::string& serverId, const OpdsEntry& entry) {
  if (serverId.empty() || entry.title.empty()) return;
  const std::string entryId = OpdsCatalogStore::stableBookId(entry);
  const auto existing = OPDS_CATALOG.find(serverId, entryId);
  if (existing && !existing->coverBmpPath.empty() && Storage.exists(existing->coverBmpPath.c_str())) return;

  const auto found = std::find_if(pending.begin(), pending.end(), [&](const Pending& candidate) {
    return candidate.serverId == serverId && candidate.entryId == entryId;
  });
  if (found != pending.end()) return;

  Pending next;
  next.serverId = serverId;
  next.entryId = entryId;
  if (entry.coverHref.empty()) {
    next.state = State::Failed;
    next.stage = Pending::Stage::Done;
    changes.push_back(Change{serverId, entryId, State::Failed, {}, "catalog entry has no cover"});
    pending.push_back(std::move(next));
    return;
  }
  const size_t serverIndex = OPDS_STORE.indexForId(serverId);
  const OpdsServer* server = OPDS_STORE.getServer(serverIndex);
  if (!server) {
    changes.push_back(Change{serverId, entryId, State::Failed, {}, "OPDS server is unavailable"});
    return;
  }
  next.coverUrl = entry.coverHref.rfind("http://", 0) == 0 || entry.coverHref.rfind("https://", 0) == 0
                      ? entry.coverHref
                      : UrlUtils::buildUrl(server->url, entry.coverHref);
  const std::string serverDirectory = std::string(kCacheRoot) + "/" + hashPathComponent(serverId);
  if (!Storage.mkdir(serverDirectory.c_str(), true)) {
    changes.push_back(Change{serverId, entryId, State::Failed, {}, "cover cache directory unavailable"});
    return;
  }
  const std::string stem = serverDirectory + "/" + hashPathComponent(entryId);
  next.sourcePath = stem + ".source";
  next.bmpPath = stem + ".bmp";
  next.state = State::Queued;
  pending.push_back(std::move(next));
  Pending& scheduled = pending.back();
  if (Storage.exists(scheduled.sourcePath.c_str())) {
    queueConvert(pending.size() - 1);
  } else {
    queueFetch(pending.size() - 1);
  }
}

void OpdsCoverCache::queueFetch(const size_t index) {
  if (index >= pending.size()) return;
  Pending& item = pending[index];
  const OpdsServer* server = OPDS_STORE.getServer(OPDS_STORE.indexForId(item.serverId));
  if (!server) {
    item.state = State::Failed;
    changes.push_back(Change{item.serverId, item.entryId, item.state, {}, "OPDS server removed"});
    return;
  }
  const std::string partPath = item.sourcePath + ".part";
  Storage.remove(partPath.c_str());
  item.stage = Pending::Stage::Fetch;
  item.state = State::Downloading;
  item.jobId = OPDS_SYNC.enqueueCoverFetch(*server, item.coverUrl, partPath);
  if (item.jobId == 0) {
    item.state = State::Failed;
    item.retryAfterMs = millis() + retryDelay(++item.failures);
    changes.push_back(Change{item.serverId, item.entryId, item.state, {}, "cover queue unavailable"});
  } else {
    LOG_DBG("OPDSCOV", "Queued fetch %s", item.entryId.c_str());
  }
}

void OpdsCoverCache::queueConvert(const size_t index) {
  if (index >= pending.size()) return;
  Pending& item = pending[index];
  item.stage = Pending::Stage::Convert;
  item.state = State::Downloading;
  item.jobId = OPDS_SYNC.enqueueCoverConvert(item.sourcePath, item.bmpPath);
  if (item.jobId == 0) {
    item.state = State::Failed;
    item.retryAfterMs = millis() + retryDelay(++item.failures);
    changes.push_back(Change{item.serverId, item.entryId, item.state, {}, "cover conversion queue unavailable"});
  } else {
    LOG_DBG("OPDSCOV", "Queued conversion %s", item.entryId.c_str());
  }
}

void OpdsCoverCache::tick() {
  const uint32_t now = millis();
  for (size_t index = 0; index < pending.size(); ++index) {
    Pending& item = pending[index];
    if (item.stage == Pending::Stage::Done) continue;
    if (item.jobId == 0) {
      if (item.state == State::Failed && elapsed(now, item.retryAfterMs)) {
        if (Storage.exists(item.sourcePath.c_str())) {
          queueConvert(index);
        } else {
          queueFetch(index);
        }
      }
      continue;
    }
    OpdsSyncService::Result result;
    if (!OPDS_SYNC.takeResult(item.jobId, result)) continue;
    item.jobId = 0;
    if (result.code != OpdsSyncService::ResultCode::Ok) {
      item.state = State::Failed;
      item.retryAfterMs = now + retryDelay(++item.failures);
      const std::string detail = result.detail.empty() ? "cover transfer failed" : result.detail;
      changes.push_back(Change{item.serverId, item.entryId, item.state, {}, detail});
      LOG_ERR("OPDSCOV", "Cover job failed for %s: %s", item.entryId.c_str(), detail.c_str());
      continue;
    }
    if (item.stage == Pending::Stage::Fetch) {
      const std::string partPath = item.sourcePath + ".part";
      if (!Storage.rename(partPath.c_str(), item.sourcePath.c_str())) {
        item.state = State::Failed;
        item.retryAfterMs = now + retryDelay(++item.failures);
        changes.push_back(Change{item.serverId, item.entryId, item.state, {}, "cover transfer finalize failed"});
      } else {
        queueConvert(index);
      }
      continue;
    }
    if (!OPDS_CATALOG.updateCoverBmpPath(item.serverId, item.entryId, item.bmpPath)) {
      item.state = State::Failed;
      item.retryAfterMs = now + retryDelay(++item.failures);
      changes.push_back(Change{item.serverId, item.entryId, item.state, {}, "cover catalog update failed"});
      continue;
    }
    item.state = State::Ready;
    item.stage = Pending::Stage::Done;
    changes.push_back(Change{item.serverId, item.entryId, item.state, item.bmpPath, {}});
    LOG_DBG("OPDSCOV", "Ready %s", item.entryId.c_str());
  }
  if (changes.size() > kMaxChanges) changes.erase(changes.begin(), changes.begin() + (changes.size() - kMaxChanges));
}

bool OpdsCoverCache::takeChange(Change& change) {
  if (changes.empty()) return false;
  change = std::move(changes.front());
  changes.erase(changes.begin());
  return true;
}

OpdsCoverCache::State OpdsCoverCache::stateFor(const std::string& serverId, const std::string& entryId) const {
  const auto found = std::find_if(pending.begin(), pending.end(), [&](const Pending& candidate) {
    return candidate.serverId == serverId && candidate.entryId == entryId;
  });
  return found == pending.end() ? State::Missing : found->state;
}
