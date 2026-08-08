#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "OpdsParser.h"

// Small app-thread coordinator for OPDS cover jobs.  Network and image
// conversion execute in OpdsSyncService's single Kobo worker; this class only
// schedules work and atomically publishes ready cache paths to the catalog.
class OpdsCoverCache {
 public:
  enum class State : uint8_t { Missing, Queued, Downloading, Ready, Failed };

  struct Change {
    std::string serverId;
    std::string entryId;
    State state = State::Missing;
    std::string bmpPath;
    std::string detail;
  };

  static OpdsCoverCache& getInstance();

  // Idempotent: repeated grid renders never enqueue a duplicate transfer.
  // A missing cover URL remains a local Failed state and never enters the
  // network queue.
  void request(const std::string& serverId, const OpdsEntry& entry);
  // Explicitly discard a failed source/cache artefact and put the card back in
  // the single-worker queue. This is intentionally separate from request():
  // ordinary renders retain exponential backoff instead of hammering a server.
  bool retry(const std::string& serverId, const std::string& entryId);
  // Settings uses this for an intentional user retry. Returns the number of
  // failed cards placed back in the low-priority queue.
  size_t retryFailedForServer(const std::string& serverId);
  void tick();
  bool takeChange(Change& change);
  State stateFor(const std::string& serverId, const std::string& entryId) const;

 private:
  struct Pending;

  OpdsCoverCache() = default;
  void queueFetch(size_t index);
  void queueConvert(size_t index);
  void publishChange(const Pending& item, const std::string& detail = {});

  std::vector<Pending> pending;
  std::vector<Change> changes;
};

#define OPDS_COVER_CACHE OpdsCoverCache::getInstance()
