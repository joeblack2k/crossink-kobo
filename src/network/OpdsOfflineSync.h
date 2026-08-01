#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Sequential offline EPUB sync for the primary OPDS catalog.  It is opt-in
// through OpdsServer::syncAllBooks and reuses the normal transfer worker so it
// cannot multiply downloads or contend with the reader thread.
class OpdsOfflineSync {
 public:
  struct Status {
    bool running = false;
    size_t completed = 0;
    size_t total = 0;
    std::string lastError;
  };

  static OpdsOfflineSync& getInstance();
  void startPrimaryIfEnabled();
  // Returns true only when a catalog availability record changed and a grid
  // should re-render.
  bool tick();
  Status status() const;

 private:
  struct QueueItem;
  OpdsOfflineSync() = default;
  bool queueNext();

  std::vector<QueueItem> queue;
  size_t nextIndex = 0;
  uint64_t activeJobId = 0;
  std::string activePath;
  std::string activeTemporaryPath;
  bool running = false;
  std::string lastError;
};

#define OPDS_OFFLINE_SYNC OpdsOfflineSync::getInstance()
