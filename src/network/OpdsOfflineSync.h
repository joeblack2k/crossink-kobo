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
  enum class Phase : uint8_t { Idle, SyncingMetadata, Downloading, Offline, Failed, Paused };

  struct Status {
    Phase phase = Phase::Idle;
    bool running = false;
    size_t completed = 0;
    size_t total = 0;
    size_t currentBytes = 0;
    size_t currentTotalBytes = 0;
    bool wifiConnected = false;
    std::string lastError;
    uint32_t lastSuccessMs = 0;
  };

  static OpdsOfflineSync& getInstance();
  // Starts a complete primary-server metadata snapshot in the common worker.
  // It never changes the active Activity and is safe to invoke from header UI.
  void requestCatalogRefresh();
  void startPrimaryIfEnabled();
  void pause();
  void resume();
  void cancel();
  // Returns true only when a catalog availability record changed and a grid
  // should re-render.
  bool tick();
  Status status() const;
  uint32_t changeSerial() const { return catalogChangeSerial; }

 private:
  struct QueueItem;
  OpdsOfflineSync() = default;
  bool queueNext();
  bool processCatalogRefresh();
  bool validateAndPublishCurrent();

  std::vector<QueueItem> queue;
  size_t nextIndex = 0;
  uint64_t activeJobId = 0;
  uint64_t catalogRefreshJobId = 0;
  std::string activePath;
  std::string activeTemporaryPath;
  bool running = false;
  bool paused = false;
  // A worker cancellation arrives asynchronously.  Keep its reason until its
  // result is consumed: otherwise a quick Pause → Resume makes that expected
  // cancellation look like a failed EPUB transfer.
  bool pauseCancellationPending = false;
  bool catalogPauseCancellationPending = false;
  bool cancelRequested = false;
  bool refreshRequested = false;
  uint64_t lastReportedProgressJobId = 0;
  size_t lastReportedProgressBytes = 0;
  uint32_t lastStartedGeneration = 0;
  uint32_t lastSuccessMs = 0;
  uint32_t catalogChangeSerial = 0;
  std::string lastError;
};

#define OPDS_OFFLINE_SYNC OpdsOfflineSync::getInstance()
