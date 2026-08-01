#include "KoboSysfs.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>

namespace crossink::kobo {
namespace {

bool readText(const std::string& path, std::string& value) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return false;
  }
  char buffer[128]{};
  const ssize_t count = ::read(fd, buffer, sizeof(buffer) - 1);
  ::close(fd);
  if (count <= 0) {
    return false;
  }
  value.assign(buffer, static_cast<std::size_t>(count));
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  return true;
}

bool readInteger(const std::string& path, int& value) {
  std::string text;
  if (!readText(path, text) || text.empty()) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
    return false;
  }
  value = static_cast<int>(parsed);
  return true;
}

bool writeInteger(const std::string& path, const int value) {
  const int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
  if (fd < 0) {
    return false;
  }
  char buffer[32]{};
  const int length = std::snprintf(buffer, sizeof(buffer), "%d\n", value);
  const bool success = length > 0 && ::write(fd, buffer, static_cast<std::size_t>(length)) == length;
  ::close(fd);
  return success;
}

bool isDirectory(const std::string& path) {
  struct stat metadata{};
  return ::stat(path.c_str(), &metadata) == 0 && S_ISDIR(metadata.st_mode);
}

template <typename Visitor>
void visitDirectories(const std::string& root, Visitor visitor) {
  DIR* directory = opendir(root.c_str());
  if (directory == nullptr) {
    return;
  }
  while (const dirent* entry = readdir(directory)) {
    if (entry->d_name[0] == '.') {
      continue;
    }
    const std::string path = root + "/" + entry->d_name;
    if (isDirectory(path)) {
      visitor(path);
    }
  }
  closedir(directory);
}

BatteryState parseBatteryState(const std::string& state) {
  if (state == "Charging") {
    return BatteryState::Charging;
  }
  if (state == "Discharging") {
    return BatteryState::Discharging;
  }
  if (state == "Full") {
    return BatteryState::Full;
  }
  if (state == "Not charging") {
    return BatteryState::NotCharging;
  }
  return BatteryState::Unknown;
}

}  // namespace

bool KoboBatterySysfs::discover(const std::string& root) {
  batteryPath_.clear();
  usbPath_.clear();
  visitDirectories(root, [&](const std::string& path) {
    std::string type;
    if (!readText(path + "/type", type)) {
      return;
    }
    if (type == "Battery" && batteryPath_.empty()) {
      int capacity = -1;
      if (readInteger(path + "/capacity", capacity)) {
        batteryPath_ = path;
      }
    } else if ((type == "USB" || type == "Mains") && usbPath_.empty()) {
      int online = 0;
      if (readInteger(path + "/online", online)) {
        usbPath_ = path;
      }
    }
  });
  return !batteryPath_.empty();
}

bool KoboBatterySysfs::read(BatterySnapshot& snapshot) const {
  int capacity = -1;
  if (batteryPath_.empty() || !readInteger(batteryPath_ + "/capacity", capacity)) {
    return false;
  }
  snapshot.percentage = std::clamp(capacity, 0, 100);
  std::string status;
  snapshot.state = readText(batteryPath_ + "/status", status) ? parseBatteryState(status) : BatteryState::Unknown;
  int online = 0;
  snapshot.usbOnline = !usbPath_.empty() && readInteger(usbPath_ + "/online", online) && online != 0;
  return true;
}

bool KoboFrontlightSysfs::discover(const std::string& root) {
  devicePath_.clear();
  maximum_ = 0;
  visitDirectories(root, [&](const std::string& path) {
    int maximum = 0;
    int brightness = 0;
    if (devicePath_.empty() && readInteger(path + "/max_brightness", maximum) && maximum > 0 &&
        readInteger(path + "/brightness", brightness)) {
      devicePath_ = path;
      maximum_ = maximum;
    }
  });
  return !devicePath_.empty();
}

bool KoboFrontlightSysfs::setPercentage(const int percentage) const {
  if (devicePath_.empty() || maximum_ <= 0) {
    return false;
  }
  const int clamped = std::clamp(percentage, 0, 100);
  const int raw = static_cast<int>((static_cast<long long>(clamped) * maximum_ + 50) / 100);
  return writeInteger(devicePath_ + "/brightness", raw);
}

int KoboFrontlightSysfs::percentage() const {
  int raw = 0;
  const std::string actual = devicePath_ + "/actual_brightness";
  if (devicePath_.empty() || maximum_ <= 0 ||
      (!readInteger(actual, raw) && !readInteger(devicePath_ + "/brightness", raw))) {
    return -1;
  }
  return std::clamp(static_cast<int>((static_cast<long long>(raw) * 100 + maximum_ / 2) / maximum_), 0, 100);
}

}  // namespace crossink::kobo
