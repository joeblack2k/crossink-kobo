#include "KoboRenderScheduler.h"

namespace crossink::kobo {

KoboRenderScheduler::~KoboRenderScheduler() {
  {
    std::lock_guard lock(stateMutex);
    stopping = true;
  }
  requestWake.notify_all();
  completionWake.notify_all();
  if (workerStarted) (void)pthread_join(workerThread, nullptr);
}

bool KoboRenderScheduler::start(const Worker callback, void* const context) {
  if (callback == nullptr || workerStarted) return false;
  {
    std::lock_guard lock(stateMutex);
    workerCallback = callback;
    workerContext = context;
  }
  if (pthread_create(&workerThread, nullptr, &KoboRenderScheduler::workerEntry, this) != 0) return false;
  workerStarted = true;
  return true;
}

void* KoboRenderScheduler::workerEntry(void* const instance) {
  static_cast<KoboRenderScheduler*>(instance)->workerMain();
  return nullptr;
}

void KoboRenderScheduler::workerMain() {
  Worker callback = nullptr;
  void* context = nullptr;
  {
    std::lock_guard lock(stateMutex);
    workerId = pthread_self();
    workerIdSet = true;
    callback = workerCallback;
    context = workerContext;
  }
  if (callback != nullptr) callback(context);
}

bool KoboRenderScheduler::waitForRenderRequest() {
  std::unique_lock lock(stateMutex);
  requestWake.wait(lock, [this] { return stopping || consumedGeneration != requestedGeneration; });
  if (stopping) return false;
  consumedGeneration = requestedGeneration;
  return true;
}

void KoboRenderScheduler::finishRender() {
  {
    std::lock_guard lock(stateMutex);
    if (completedGeneration < consumedGeneration) completedGeneration = consumedGeneration;
  }
  completionWake.notify_all();
}

void KoboRenderScheduler::requestRender() {
  {
    std::lock_guard lock(stateMutex);
    if (stopping) return;
    ++requestedGeneration;
  }
  requestWake.notify_one();
}

bool KoboRenderScheduler::requestRenderAndWait() {
  if (isRenderThread() || isRenderLockHeldByCurrentThread()) return false;

  std::unique_lock lock(stateMutex);
  if (stopping || synchronousWaiter) return false;
  synchronousWaiter = true;
  const std::uint64_t targetGeneration = ++requestedGeneration;
  requestWake.notify_one();
  completionWake.wait(lock, [this, targetGeneration] { return stopping || completedGeneration >= targetGeneration; });
  synchronousWaiter = false;
  return !stopping && completedGeneration >= targetGeneration;
}

void KoboRenderScheduler::lockRender() { renderMutex.lock(); }

void KoboRenderScheduler::unlockRender() { renderMutex.unlock(); }

bool KoboRenderScheduler::isRenderThread() const {
  std::lock_guard lock(stateMutex);
  return workerIdSet && pthread_equal(workerId, pthread_self()) != 0;
}

bool KoboRenderScheduler::isRenderLockHeld() const { return renderMutex.isHeld(); }

bool KoboRenderScheduler::isRenderLockHeldByCurrentThread() const { return renderMutex.isHeldByCurrentThread(); }

}  // namespace crossink::kobo
