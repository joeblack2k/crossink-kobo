#include "HalSystem.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

namespace {

constexpr char kCrashDirectory[] = "/data/.crossink/crash";
constexpr char kLastSignalPath[] = "/data/.crossink/crash/last-signal.txt";
constexpr char kLastStartPath[] = "/data/.crossink/crash/last-start";
constexpr char kCrashReportPath[] = "/data/.crossink/crash/crash_report.txt";
std::atomic<std::uint64_t> minimumAvailableBytes{std::numeric_limits<std::uint64_t>::max()};
std::atomic<bool> rebootFromPanic{false};

std::uint32_t clampToUint32(const std::uint64_t value) {
  return value > std::numeric_limits<std::uint32_t>::max() ? std::numeric_limits<std::uint32_t>::max()
                                                           : static_cast<std::uint32_t>(value);
}

std::uint64_t memAvailableBytes() {
  FILE* const file = std::fopen("/proc/meminfo", "re");
  if (file != nullptr) {
    char line[128]{};
    while (std::fgets(line, sizeof(line), file) != nullptr) {
      unsigned long long kib = 0;
      if (std::sscanf(line, "MemAvailable: %llu kB", &kib) == 1) {
        std::fclose(file);
        return static_cast<std::uint64_t>(kib) * 1024ULL;
      }
    }
    std::fclose(file);
  }

  struct sysinfo info{};
  if (::sysinfo(&info) != 0) return 0;
  return (static_cast<std::uint64_t>(info.freeram) + static_cast<std::uint64_t>(info.bufferram)) * info.mem_unit;
}

std::uint64_t totalMemoryBytes() {
  struct sysinfo info{};
  if (::sysinfo(&info) != 0) return 0;
  return static_cast<std::uint64_t>(info.totalram) * info.mem_unit;
}

void updateMinimum(const std::uint64_t available) {
  std::uint64_t observed = minimumAvailableBytes.load(std::memory_order_relaxed);
  while (available < observed &&
         !minimumAvailableBytes.compare_exchange_weak(observed, available, std::memory_order_relaxed)) {
  }
}

bool signalIsNewerThanLastStart() {
  struct stat signal{};
  if (::stat(kLastSignalPath, &signal) != 0) return false;
  struct stat start{};
  if (::stat(kLastStartPath, &start) != 0) return true;
  return signal.st_mtime > start.st_mtime ||
         (signal.st_mtime == start.st_mtime && signal.st_mtim.tv_nsec > start.st_mtim.tv_nsec);
}

void recordStartMarker() {
  (void)::mkdir("/data/.crossink", S_IRWXU);
  (void)::mkdir(kCrashDirectory, S_IRWXU);
  const int descriptor = ::open(kLastStartPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (descriptor < 0) return;
  const char marker[] = "started\n";
  (void)::write(descriptor, marker, sizeof(marker) - 1);
  (void)::fsync(descriptor);
  (void)::close(descriptor);
}

std::string readSignalRecord() {
  FILE* const file = std::fopen(kLastSignalPath, "re");
  if (file == nullptr) return {};
  std::string record;
  char line[160]{};
  while (std::fgets(line, sizeof(line), file) != nullptr) record += line;
  std::fclose(file);
  return record;
}

}  // namespace

namespace HalSystem {

void begin() {
  rebootFromPanic.store(signalIsNewerThanLastStart(), std::memory_order_relaxed);
  (void)memoryInfo();
  recordStartMarker();
}

MemoryInfo memoryInfo() {
  const std::uint64_t available = memAvailableBytes();
  updateMinimum(available);
  const std::uint64_t minimum = minimumAvailableBytes.load(std::memory_order_relaxed);
  return {clampToUint32(totalMemoryBytes()), clampToUint32(available), clampToUint32(minimum),
          clampToUint32(available)};
}

std::uint64_t monotonicMicros() {
  timespec now{};
  if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
  return static_cast<std::uint64_t>(now.tv_sec) * 1'000'000ULL + now.tv_nsec / 1'000ULL;
}

[[noreturn]] void restart() {
  ::execl("/proc/self/exe", "crossink-kobo", static_cast<char*>(nullptr));
  std::fprintf(stderr, "[KOBO] controlled re-exec failed: errno=%d (%s)\n", errno, std::strerror(errno));
  std::_Exit(127);
}

void checkPanic() {
  if (!isRebootFromPanic()) return;
  const std::string record = getPanicInfo(true);
  const int descriptor = ::open(kCrashReportPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (descriptor < 0) return;
  (void)::write(descriptor, record.data(), record.size());
  (void)::fsync(descriptor);
  (void)::close(descriptor);
}

void clearPanic() {
  rebootFromPanic.store(false, std::memory_order_relaxed);
  (void)::unlink(kLastSignalPath);
}

std::string getPanicInfo(const bool full) {
  std::string record = readSignalRecord();
  if (!full) return record;
  const MemoryInfo memory = memoryInfo();
  record += "CrossInk Kobo Linux crash report\n";
  record += "available_bytes=" + std::to_string(memory.availableBytes) + "\n";
  record += "monotonic_us=" + std::to_string(monotonicMicros()) + "\n";
  return record;
}

bool isRebootFromPanic() { return rebootFromPanic.load(std::memory_order_relaxed); }

}  // namespace HalSystem
