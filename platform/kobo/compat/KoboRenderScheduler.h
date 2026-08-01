#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "KoboOwnedMutex.h"

namespace crossink::kobo {

// Native N437 render-worker primitive.  ActivityManager uses this instead of
// the desktop simulator's FreeRTOS emulation so render wakeups, synchronous
// repaints and lock-owner checks have ordinary pthread/C++ semantics.
class KoboRenderScheduler {
 public:
  using Worker = void (*)(void*);

  KoboRenderScheduler() = default;
  KoboRenderScheduler(const KoboRenderScheduler&) = delete;
  KoboRenderScheduler& operator=(const KoboRenderScheduler&) = delete;
  ~KoboRenderScheduler();

  bool start(Worker worker, void* context);
  bool waitForRenderRequest();
  void finishRender();
  void requestRender();
  bool requestRenderAndWait();

  void lockRender();
  void unlockRender();
  bool isRenderThread() const;
  bool isRenderLockHeld() const;
  bool isRenderLockHeldByCurrentThread() const;

 private:
  static void* workerEntry(void* instance);
  void workerMain();

  mutable std::mutex stateMutex;
  std::condition_variable requestWake;
  std::condition_variable completionWake;
  pthread_t workerThread{};
  pthread_t workerId{};
  Worker workerCallback = nullptr;
  void* workerContext = nullptr;
  bool workerStarted = false;
  bool workerIdSet = false;
  std::uint64_t requestedGeneration = 0;
  std::uint64_t consumedGeneration = 0;
  std::uint64_t completedGeneration = 0;
  bool synchronousWaiter = false;
  bool stopping = false;

  KoboOwnedMutex renderMutex;
};

}  // namespace crossink::kobo
