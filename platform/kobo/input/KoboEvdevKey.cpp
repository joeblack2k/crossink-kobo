#include "KoboEvdevKey.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstring>

namespace crossink::kobo {
namespace {

constexpr std::size_t bitsPerWord = sizeof(unsigned long) * CHAR_BIT;
constexpr std::size_t bitWords(const std::size_t maximum) { return (maximum + bitsPerWord) / bitsPerWord; }

bool bitSet(const unsigned long* bits, const std::size_t bit) {
  return (bits[bit / bitsPerWord] & (1UL << (bit % bitsPerWord))) != 0;
}

int evdevIoctl(const int fd, const unsigned long request, void* argument) {
  // musl follows the kernel ABI and declares ioctl's request as int, while
  // _IOC macros have unsigned-long type. The bit pattern is intentional.
  return ::ioctl(fd, static_cast<int>(request), argument);
}

}  // namespace

KoboEvdevKey::~KoboEvdevKey() { close(); }

bool KoboEvdevKey::discoverPowerKey(KeyDeviceInfo& result, const std::string& inputDirectory) {
  DIR* directory = opendir(inputDirectory.c_str());
  if (directory == nullptr) {
    return false;
  }
  bool found = false;
  while (const dirent* entry = readdir(directory)) {
    if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
    const std::string path = inputDirectory + "/" + entry->d_name;
    const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    unsigned long eventBits[bitWords(EV_MAX)]{};
    unsigned long keyBits[bitWords(KEY_MAX)]{};
    const bool supportsKeys = evdevIoctl(fd, EVIOCGBIT(0, sizeof(eventBits)), eventBits) >= 0 &&
                              bitSet(eventBits, EV_KEY) &&
                              evdevIoctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) >= 0 &&
                              bitSet(keyBits, KEY_POWER);
    if (supportsKeys) {
      char name[256]{};
      if (evdevIoctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) std::strncpy(name, "unknown", sizeof(name) - 1);
      result = {path, name};
      found = true;
      ::close(fd);
      break;
    }
    ::close(fd);
  }
  closedir(directory);
  return found;
}

bool KoboEvdevKey::open(const KeyDeviceInfo& device) {
  close();
  fd_ = ::open(device.path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  return fd_ >= 0;
}

void KoboEvdevKey::close() {
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
  pressed_ = false;
}

void KoboEvdevKey::beginFrame() {
  pressedEdge_ = false;
  releasedEdge_ = false;
}

void KoboEvdevKey::update() {
  if (fd_ < 0) return;
  input_event event{};
  while (::read(fd_, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event))) {
    timespec eventNow{};
    clock_gettime(CLOCK_MONOTONIC, &eventNow);
    const std::uint64_t timestamp =
        static_cast<std::uint64_t>(eventNow.tv_sec) * 1'000'000ULL + eventNow.tv_nsec / 1'000ULL;
    ingest(event.type, event.code, event.value, timestamp);
  }
  timespec now{};
  if (pressed_ && clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
    latestMicros_ = static_cast<std::uint64_t>(now.tv_sec) * 1'000'000ULL + now.tv_nsec / 1'000ULL;
  }
}

void KoboEvdevKey::ingest(const std::uint16_t type, const std::uint16_t code, const std::int32_t value,
                          const std::uint64_t timestampMicros) {
  latestMicros_ = timestampMicros;
  if (type != EV_KEY || code != KEY_POWER || value == 2) return;
  if (value != 0 && !pressed_) {
    pressed_ = true;
    pressedEdge_ = true;
    pressedAtMicros_ = timestampMicros;
  } else if (value == 0 && pressed_) {
    pressed_ = false;
    releasedEdge_ = true;
  }
}

unsigned long KoboEvdevKey::heldMilliseconds() const {
  if (!pressed_ || latestMicros_ < pressedAtMicros_) return 0;
  return static_cast<unsigned long>((latestMicros_ - pressedAtMicros_) / 1'000ULL);
}

}  // namespace crossink::kobo
