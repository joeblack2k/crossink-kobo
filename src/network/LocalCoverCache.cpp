#include "network/LocalCoverCache.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <utility>

#include "network/OpdsSyncService.h"

struct LocalCoverCache::Pending {
  std::string bookPath;
  uint64_t jobId = 0;
  uint32_t retryAfterMs = 0;
  uint8_t failures = 0;
  State state = State::Missing;
};

namespace {
constexpr uint32_t kFirstRetryMs = 15000;
constexpr uint32_t kMaximumRetryMs = 5 * 60 * 1000;
constexpr size_t kMaxChanges = 24;

bool elapsed(const uint32_t now, const uint32_t deadline) { return static_cast<int32_t>(now - deadline) >= 0; }

uint32_t retryDelay(const uint8_t failures) {
  const uint8_t exponent = std::min<uint8_t>(failures, 5);
  return std::min<uint32_t>(kFirstRetryMs << exponent, kMaximumRetryMs);
}
}  // namespace

LocalCoverCache& LocalCoverCache::getInstance() {
  static LocalCoverCache instance;
  return instance;
}

void LocalCoverCache::request(const std::string& bookPath) {
  if (bookPath.empty() || !Storage.exists(bookPath.c_str())) return;
  const auto found =
      std::find_if(pending.begin(), pending.end(), [&](const Pending& item) { return item.bookPath == bookPath; });
  if (found != pending.end()) return;
  Pending item;
  item.bookPath = bookPath;
  item.state = State::Queued;
  item.jobId = OPDS_SYNC.enqueueLocalCover(bookPath);
  if (item.jobId == 0) {
    item.state = State::Failed;
    item.retryAfterMs = millis() + retryDelay(++item.failures);
  } else {
    LOG_DBG("LOCALCOV", "Queued %s", bookPath.c_str());
  }
  pending.push_back(std::move(item));
}

void LocalCoverCache::tick() {
  const uint32_t now = millis();
  for (auto& item : pending) {
    if (item.jobId == 0) {
      if (item.state == State::Failed && elapsed(now, item.retryAfterMs)) {
        item.state = State::Queued;
        item.jobId = OPDS_SYNC.enqueueLocalCover(item.bookPath);
      }
      continue;
    }
    OpdsSyncService::Result result;
    if (!OPDS_SYNC.takeResult(item.jobId, result)) continue;
    item.jobId = 0;
    if (result.code == OpdsSyncService::ResultCode::Ok && !result.destinationPath.empty() &&
        Storage.exists(result.destinationPath.c_str())) {
      item.state = State::Ready;
      changes.push_back(Change{item.bookPath, item.state, result.destinationPath, {}});
      LOG_DBG("LOCALCOV", "Ready %s", item.bookPath.c_str());
      continue;
    }
    item.state = State::Failed;
    item.retryAfterMs = now + retryDelay(++item.failures);
    const std::string detail = result.detail.empty() ? "local cover generation failed" : result.detail;
    changes.push_back(Change{item.bookPath, item.state, {}, detail});
    LOG_ERR("LOCALCOV", "Failed %s: %s", item.bookPath.c_str(), detail.c_str());
  }
  if (changes.size() > kMaxChanges) changes.erase(changes.begin(), changes.begin() + (changes.size() - kMaxChanges));
}

bool LocalCoverCache::takeChange(Change& change) {
  if (changes.empty()) return false;
  change = std::move(changes.front());
  changes.erase(changes.begin());
  return true;
}
