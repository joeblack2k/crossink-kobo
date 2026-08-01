#include "KoboEvdevTouch.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstring>

#include "KoboEvdevAbi.h"

namespace crossink::kobo {
namespace {

constexpr std::size_t bitsPerWord = sizeof(unsigned long) * CHAR_BIT;
constexpr std::size_t bitWords(const std::size_t maximum) { return (maximum + bitsPerWord) / bitsPerWord; }

bool bitSet(const unsigned long* bits, const std::size_t bit) {
  return (bits[bit / bitsPerWord] & (1UL << (bit % bitsPerWord))) != 0;
}

int evdevIoctl(const int fd, const unsigned long request, void* argument) {
  return ::ioctl(fd, static_cast<int>(request), argument);
}

bool readAxis(const int fd, const unsigned int code, RawAxisRange& range) {
  input_absinfo info{};
  if (evdevIoctl(fd, EVIOCGABS(code), &info) < 0 || info.maximum <= info.minimum) {
    return false;
  }
  range = {info.minimum, info.maximum};
  return true;
}

int deviceScore(const char* name) {
  if (name == nullptr) {
    return 0;
  }
  std::string lower(name);
  for (char& character : lower) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    }
  }
  return lower.find("zforce") != std::string::npos ? 100 : 10;
}

bool inspectDevice(const std::string& path, TouchDeviceInfo& result, int& score) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    return false;
  }

  unsigned long eventBits[bitWords(EV_MAX)]{};
  unsigned long absoluteBits[bitWords(ABS_MAX)]{};
  const bool hasEventBits = evdevIoctl(fd, EVIOCGBIT(0, sizeof(eventBits)), eventBits) >= 0;
  const bool hasAbsoluteBits = evdevIoctl(fd, EVIOCGBIT(EV_ABS, sizeof(absoluteBits)), absoluteBits) >= 0;
  if (!hasEventBits || !hasAbsoluteBits || !bitSet(eventBits, EV_ABS)) {
    ::close(fd);
    return false;
  }

  const bool multi = bitSet(absoluteBits, ABS_MT_POSITION_X) && bitSet(absoluteBits, ABS_MT_POSITION_Y);
  const unsigned int xCode = multi ? ABS_MT_POSITION_X : ABS_X;
  const unsigned int yCode = multi ? ABS_MT_POSITION_Y : ABS_Y;
  RawAxisRange x{};
  RawAxisRange y{};
  if (!bitSet(absoluteBits, xCode) || !bitSet(absoluteBits, yCode) || !readAxis(fd, xCode, x) ||
      !readAxis(fd, yCode, y)) {
    ::close(fd);
    return false;
  }

  char name[256]{};
  if (evdevIoctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
    std::strncpy(name, "unknown", sizeof(name) - 1);
  }
  const int candidateScore = deviceScore(name);
  if (candidateScore > score) {
    score = candidateScore;
    result = {path, name, x, y, multi};
  }
  ::close(fd);
  return true;
}

}  // namespace

KoboEvdevTouch::~KoboEvdevTouch() { close(); }

bool KoboEvdevTouch::discover(TouchDeviceInfo& result, const std::string& inputDirectory) {
  DIR* directory = opendir(inputDirectory.c_str());
  if (directory == nullptr) {
    return false;
  }

  int bestScore = -1;
  while (const dirent* entry = readdir(directory)) {
    if (std::strncmp(entry->d_name, "event", 5) != 0) {
      continue;
    }
    inspectDevice(inputDirectory + "/" + entry->d_name, result, bestScore);
  }
  closedir(directory);
  return bestScore >= 0;
}

bool KoboEvdevTouch::open(const TouchDeviceInfo& device, TouchCalibration calibration) {
  close();
  fd_ = ::open(device.path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd_ < 0) {
    return false;
  }
  device_ = device;
  const bool useDeviceCalibration =
      calibration.x.maximum <= calibration.x.minimum && calibration.y.maximum <= calibration.y.minimum;
  if (calibration.x.maximum <= calibration.x.minimum) {
    calibration.x = device.x;
  }
  if (calibration.y.maximum <= calibration.y.minimum) {
    calibration.y = device.y;
  }
  // N437 zForce coordinates follow the native 1448x1072 panel axes while
  // CrossInk's user-facing coordinate system is 1072x1448 portrait. Detect
  // that exact geometry relationship instead of hard-coding an event node.
  const std::int32_t xSpan = calibration.x.maximum - calibration.x.minimum + 1;
  const std::int32_t ySpan = calibration.y.maximum - calibration.y.minimum + 1;
  if (useDeviceCalibration && xSpan == KoboTouchTransform::kPortraitHeight &&
      ySpan == KoboTouchTransform::kPortraitWidth) {
    calibration.swapAxes = true;
    // N437's native landscape zForce origin is at the portrait top-right.
    // After swapping native axes, portrait X must therefore be mirrored.
    calibration.invertX = true;
  }
  transform_ = KoboTouchTransform(calibration);
  rawX_ = calibration.x.minimum;
  rawY_ = calibration.y.minimum;
  down_ = false;
  positionChanged_ = false;
  discarding_ = false;
  lastTimestampMicros_ = 0;
  if (!transform_.valid()) {
    close();
    return false;
  }
  clockid_t clockId = CLOCK_MONOTONIC;
  (void)evdevIoctl(fd_, EVIOCSCLOCKID, &clockId);
  return true;
}

void KoboEvdevTouch::close() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
  fd_ = -1;
  down_ = false;
  positionChanged_ = false;
  discarding_ = false;
  lastTimestampMicros_ = 0;
}

void KoboEvdevTouch::setOrientation(const ScreenOrientation orientation) { transform_.setOrientation(orientation); }

bool KoboEvdevTouch::readFrame(TouchFrame& frame) {
  if (fd_ < 0) {
    return false;
  }

  KoboEvdevEvent event{};
  while (true) {
    const ssize_t count = ::read(fd_, &event, sizeof(event));
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return false;
    }
    if (count != static_cast<ssize_t>(sizeof(event))) {
      return false;
    }

    if (event.type == EV_SYN && event.code == SYN_DROPPED) {
      down_ = false;
      positionChanged_ = false;
      discarding_ = true;
      continue;
    }
    if (event.type == EV_ABS) {
      const unsigned int xCode = device_.usesMultitouchAxes ? ABS_MT_POSITION_X : ABS_X;
      const unsigned int yCode = device_.usesMultitouchAxes ? ABS_MT_POSITION_Y : ABS_Y;
      if (event.code == xCode) {
        rawX_ = event.value;
        positionChanged_ = true;
      } else if (event.code == yCode) {
        rawY_ = event.value;
        positionChanged_ = true;
      } else if (event.code == ABS_MT_TRACKING_ID) {
        down_ = event.value >= 0;
      }
    } else if (event.type == EV_KEY && event.code == BTN_TOUCH) {
      down_ = event.value != 0;
    } else if (event.type == EV_SYN && event.code == SYN_REPORT) {
      const bool discontinuity = discarding_;
      discarding_ = false;
      frame.point = transform_.map(rawX_, rawY_);
      frame.rawPoint = {rawX_, rawY_};
      frame.down = discontinuity ? false : down_;
      frame.positionChanged = positionChanged_;
      frame.discontinuity = discontinuity;
      const bool validTimestamp = event.seconds >= 0 && event.microseconds >= 0 && event.microseconds < 1'000'000;
      const std::uint64_t eventTimestamp =
          validTimestamp ? static_cast<std::uint64_t>(event.seconds) * 1'000'000ULL +
                               static_cast<std::uint64_t>(event.microseconds)
                         : 0;
      if (validTimestamp && eventTimestamp >= lastTimestampMicros_) {
        frame.timestampMicros = eventTimestamp;
      } else {
        timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);
        frame.timestampMicros =
            static_cast<std::uint64_t>(now.tv_sec) * 1'000'000ULL + now.tv_nsec / 1'000ULL;
      }
      lastTimestampMicros_ = frame.timestampMicros;
      positionChanged_ = false;
      return true;
    }
  }
}

}  // namespace crossink::kobo
