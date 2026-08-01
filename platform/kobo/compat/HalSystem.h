#pragma once

#include <cstdint>
#include <string>

namespace HalSystem {

struct StackFrame {
  std::uint32_t sp;
  std::uint32_t spp[8];
};

// Linux does not expose a fragmented Arduino heap. `availableBytes` is the
// kernel's MemAvailable estimate and therefore the honest upper bound used for
// admission control. `maxAllocatableBytes` intentionally has the same value.
struct MemoryInfo {
  std::uint32_t totalBytes;
  std::uint32_t availableBytes;
  std::uint32_t minimumAvailableBytes;
  std::uint32_t maxAllocatableBytes;
};

void begin();
[[nodiscard]] MemoryInfo memoryInfo();
[[nodiscard]] std::uint64_t monotonicMicros();

// Re-execing preserves the supervisor's healthy process slot and avoids an
// ESP-style hardware reset. This function returns only if exec fails.
[[noreturn]] void restart();

void checkPanic();
void clearPanic();
[[nodiscard]] std::string getPanicInfo(bool full = false);
[[nodiscard]] bool isRebootFromPanic();

}  // namespace HalSystem
