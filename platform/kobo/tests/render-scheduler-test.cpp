// SPDX-License-Identifier: GPL-3.0-or-later
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>

#include "KoboRenderScheduler.h"

namespace {

[[noreturn]] void fail(const char* const message) {
  std::cerr << "render scheduler test failed: " << message << '\n';
  std::exit(1);
}

struct RenderProbe {
  crossink::kobo::KoboRenderScheduler* scheduler = nullptr;
  std::mutex mutex;
  std::condition_variable wake;
  unsigned int renderCount = 0;
  bool workerIdentityConfirmed = false;
};

void runRenderLoop(void* const context) {
  auto& probe = *static_cast<RenderProbe*>(context);
  {
    std::lock_guard lock(probe.mutex);
    probe.workerIdentityConfirmed = probe.scheduler->isRenderThread();
  }
  while (probe.scheduler->waitForRenderRequest()) {
    {
      std::lock_guard lock(probe.mutex);
      ++probe.renderCount;
    }
    probe.scheduler->finishRender();
    probe.wake.notify_all();
  }
}

bool waitForRender(RenderProbe& probe, const unsigned int expected) {
  std::unique_lock lock(probe.mutex);
  return probe.wake.wait_for(lock, std::chrono::seconds(1), [&] { return probe.renderCount >= expected; });
}

}  // namespace

int main() {
  RenderProbe probe;
  crossink::kobo::KoboRenderScheduler scheduler;
  probe.scheduler = &scheduler;
  if (!scheduler.start(&runRenderLoop, &probe)) fail("native worker did not start");

  scheduler.requestRender();
  scheduler.requestRender();
  if (!waitForRender(probe, 1)) fail("asynchronous render request was not delivered");
  {
    std::lock_guard lock(probe.mutex);
    if (!probe.workerIdentityConfirmed) fail("worker thread identity was not recorded");
  }

  if (!scheduler.requestRenderAndWait()) fail("synchronous render was not acknowledged");
  if (!waitForRender(probe, 2)) fail("synchronous render did not run after async render");

  scheduler.lockRender();
  if (!scheduler.isRenderLockHeld() || !scheduler.isRenderLockHeldByCurrentThread()) {
    fail("render lock ownership was not recorded");
  }
  scheduler.lockRender();
  if (!scheduler.isRenderLockHeldByCurrentThread()) fail("recursive render lock ownership was lost");
  scheduler.unlockRender();
  scheduler.unlockRender();
  if (scheduler.isRenderLockHeld()) fail("render lock ownership was not cleared");

  return 0;
}
