#pragma once

#include <cstdint>

#include "HalSystem.h"

namespace crossink::kobo {

// Arduino compatibility facade for shared code. It intentionally translates
// the handful of legacy ESP heap queries to actual Linux memory information.
// No Kobo path may obtain capacity from the simulator's ESPMock singleton.
class KoboSystemCompat {
 public:
  [[nodiscard]] std::uint32_t getFreeHeap() const { return HalSystem::memoryInfo().availableBytes; }
  [[nodiscard]] std::uint32_t getHeapSize() const { return HalSystem::memoryInfo().totalBytes; }
  [[nodiscard]] std::uint32_t getMinFreeHeap() const { return HalSystem::memoryInfo().minimumAvailableBytes; }
  [[nodiscard]] std::uint32_t getMaxAllocHeap() const { return HalSystem::memoryInfo().maxAllocatableBytes; }
  [[noreturn]] void restart() const { HalSystem::restart(); }
};

inline const KoboSystemCompat& systemCompatibility() {
  static const KoboSystemCompat instance{};
  return instance;
}

}  // namespace crossink::kobo
