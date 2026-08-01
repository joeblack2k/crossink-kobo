#include "KoboSuspendController.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

namespace crossink::kobo {
namespace {
constexpr const char* kPowerState = "/sys/power/state";
constexpr const char* kWakeupCount = "/sys/power/wakeup_count";
constexpr const char* kEventDirectory = "/data/.crossink/power";
constexpr const char* kEventFile = "/data/.crossink/power/suspend-events.jsonl";
constexpr std::size_t kEventLimitBytes = 64 * 1024;

std::uint64_t monotonicMilliseconds() {
  timespec now{};
  return ::clock_gettime(CLOCK_MONOTONIC, &now) == 0
             ? static_cast<std::uint64_t>(now.tv_sec) * 1000ULL + now.tv_nsec / 1000000ULL
             : 0;
}

bool readText(const char* path, std::string& value) {
  const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;
  char buffer[512]{};
  const ssize_t count = ::read(fd, buffer, sizeof(buffer) - 1);
  const int savedErrno = errno;
  ::close(fd);
  errno = savedErrno;
  if (count < 0) return false;
  value.assign(buffer, static_cast<std::size_t>(count));
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ')) value.pop_back();
  return true;
}

bool writeText(const char* path, const char* value, const std::size_t length) {
  const int fd = ::open(path, O_WRONLY | O_CLOEXEC);
  if (fd < 0) return false;
  const ssize_t written = ::write(fd, value, length);
  const int savedErrno = errno;
  ::close(fd);
  errno = savedErrno;
  return written == static_cast<ssize_t>(length);
}

std::string escapeJson(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    if (ch == '"' || ch == '\\') escaped.push_back('\\');
    if (ch >= 0x20) escaped.push_back(ch);
  }
  return escaped;
}

void rotateEventsIfNeeded() {
  struct stat metadata{};
  if (::stat(kEventFile, &metadata) != 0 || metadata.st_size < static_cast<off_t>(kEventLimitBytes)) return;
  const std::string previous = std::string(kEventFile) + ".1";
  (void)::unlink(previous.c_str());
  (void)::rename(kEventFile, previous.c_str());
}

bool containsState(const std::string& states, const char* state) {
  const std::string needle(state);
  std::size_t position = 0;
  while ((position = states.find(needle, position)) != std::string::npos) {
    const bool before = position == 0 || states[position - 1] == ' ';
    const std::size_t end = position + needle.size();
    const bool after = end == states.size() || states[end] == ' ';
    if (before && after) return true;
    position = end;
  }
  return false;
}
}  // namespace

KoboSuspendProbe KoboSuspendController::probe() {
  KoboSuspendProbe result;
  (void)readText(kPowerState, result.states);
  result.memSupported = containsState(result.states, "mem");
  result.wakeupCountSupported = ::access(kWakeupCount, R_OK | W_OK) == 0;
  (void)readText("/sys/power/mem_sleep", result.memSleep);
  (void)readText("/proc/sys/kernel/random/boot_id", result.bootId);
  (void)readText("/proc/uptime", result.uptime);
  return result;
}

void KoboSuspendController::recordEvent(const char* state, const std::string& detail) {
  (void)::mkdir("/data/.crossink", 0755);
  (void)::mkdir(kEventDirectory, 0755);
  rotateEventsIfNeeded();
  const KoboSuspendProbe current = probe();
  const int fd = ::open(kEventFile, O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC, 0644);
  if (fd < 0) return;
  const std::string line = "{\"state\":\"" + escapeJson(state ? state : "unknown") +
                           "\",\"monotonic_ms\":" + std::to_string(monotonicMilliseconds()) +
                           ",\"boot_id\":\"" + escapeJson(current.bootId) + "\",\"uptime\":\"" +
                           escapeJson(current.uptime) + "\",\"detail\":\"" + escapeJson(detail) + "\"}\n";
  (void)::write(fd, line.data(), line.size());
  (void)::fsync(fd);
  ::close(fd);
}

KoboSuspendResult KoboSuspendController::suspendToRam(const KoboSuspendRequest& request) {
  KoboSuspendResult result;
  const KoboSuspendProbe capabilities = probe();
  if (!capabilities.memSupported) {
    result.errorNumber = ENOTSUP;
    result.detail = "mem is not offered by /sys/power/state";
    recordEvent("rejected", result.detail);
    return result;
  }
  recordEvent("suspending", request.eventContext);
  std::string wakeupCount;
  if (capabilities.wakeupCountSupported) {
    if (!readText(kWakeupCount, wakeupCount) || wakeupCount.empty()) {
      result.errorNumber = errno != 0 ? errno : EIO;
      result.detail = "could not read wakeup_count";
      recordEvent("wakeup_count_read_failed", result.detail);
      return result;
    }
    if (!writeText(kWakeupCount, wakeupCount.c_str(), wakeupCount.size())) {
      result.errorNumber = errno;
      result.wakeupCountRace = result.errorNumber == EBUSY;
      result.detail = result.wakeupCountRace ? "wakeup_count race" : "could not arm wakeup_count";
      recordEvent("wakeup_count_rejected", result.detail);
      return result;
    }
    result.usedWakeupCount = true;
  }
  const std::uint64_t startedAt = monotonicMilliseconds();
  constexpr char state[] = "mem\n";
  if (!writeText(kPowerState, state, sizeof(state) - 1)) {
    result.errorNumber = errno;
    result.detail = std::string("state write failed: ") + std::strerror(result.errorNumber);
    recordEvent("suspend_write_failed", result.detail);
    return result;
  }
  result.entered = true;
  const std::uint64_t endedAt = monotonicMilliseconds();
  result.elapsedMilliseconds = endedAt >= startedAt ? endedAt - startedAt : 0;
  result.detail = "kernel returned";
  recordEvent("resuming", result.detail + "; elapsed_ms=" + std::to_string(result.elapsedMilliseconds));
  return result;
}

}  // namespace crossink::kobo
