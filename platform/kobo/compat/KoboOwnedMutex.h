#pragma once

#include <pthread.h>

#include <mutex>

namespace crossink::kobo {

// A small native replacement for FreeRTOS recursive mutex ownership queries.
// The renderer uses the owner check to reject unsafe synchronous redraws; no
// application code needs to know that Kobo is backed by pthreads.
class KoboOwnedMutex {
 public:
  KoboOwnedMutex() = default;
  KoboOwnedMutex(const KoboOwnedMutex&) = delete;
  KoboOwnedMutex& operator=(const KoboOwnedMutex&) = delete;

  void lock() {
    mutex_.lock();
    const pthread_t current = pthread_self();
    std::lock_guard lock(ownerMutex_);
    if (ownerSet_ && pthread_equal(owner_, current) != 0) {
      ++depth_;
    } else {
      owner_ = current;
      ownerSet_ = true;
      depth_ = 1;
    }
  }

  void unlock() {
    const pthread_t current = pthread_self();
    {
      std::lock_guard lock(ownerMutex_);
      if (ownerSet_ && pthread_equal(owner_, current) != 0 && depth_ > 0) {
        --depth_;
        if (depth_ == 0) ownerSet_ = false;
      }
    }
    mutex_.unlock();
  }

  bool isHeld() const {
    std::lock_guard lock(ownerMutex_);
    return ownerSet_ && depth_ != 0;
  }

  bool isHeldByCurrentThread() const {
    std::lock_guard lock(ownerMutex_);
    return ownerSet_ && depth_ != 0 && pthread_equal(owner_, pthread_self()) != 0;
  }

 private:
  std::recursive_mutex mutex_;
  mutable std::mutex ownerMutex_;
  pthread_t owner_{};
  bool ownerSet_ = false;
  unsigned int depth_ = 0;
};

}  // namespace crossink::kobo
