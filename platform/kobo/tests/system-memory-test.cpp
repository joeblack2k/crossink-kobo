#include <HalSystem.h>

#include <cstdint>

int main() {
  HalSystem::begin();
  const auto first = HalSystem::memoryInfo();
  const std::uint64_t before = HalSystem::monotonicMicros();
  const std::uint64_t after = HalSystem::monotonicMicros();
  if (first.totalBytes == 0 || first.availableBytes == 0 || first.availableBytes > first.totalBytes ||
      first.maxAllocatableBytes != first.availableBytes || first.minimumAvailableBytes > first.availableBytes ||
      after < before) {
    return 1;
  }
  return 0;
}
