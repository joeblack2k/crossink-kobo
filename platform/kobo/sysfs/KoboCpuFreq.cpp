// SPDX-License-Identifier: GPL-3.0-or-later
#include "KoboCpuFreq.h"

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
#include <sstream>
#include <utility>

namespace crossink::kobo {
namespace {

bool parseInteger(const std::string& text, int& value) {
  if (text.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || *end != '\0' || parsed < 0 || parsed > INT_MAX) return false;
  value = static_cast<int>(parsed);
  return true;
}

bool tokenPresent(const std::string& values, const std::string& target) {
  std::istringstream input(values);
  std::string token;
  while (input >> token) {
    if (token == target) return true;
  }
  return false;
}

bool readFile(const std::string& path, std::string& value) {
  const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (descriptor < 0) return false;
  char buffer[128]{};
  const ssize_t bytes = ::read(descriptor, buffer, sizeof(buffer) - 1);
  ::close(descriptor);
  if (bytes <= 0) return false;
  value.assign(buffer, static_cast<std::size_t>(bytes));
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
  return true;
}

}  // namespace

KoboCpuFreqGuard::KoboCpuFreqGuard(std::string policyRoot) : root_(std::move(policyRoot)) {}

KoboCpuFreqGuard::~KoboCpuFreqGuard() { (void)restore(); }

bool KoboCpuFreqGuard::readText(const char* const leaf, std::string& value) const {
  return readFile(root_ + "/" + leaf, value);
}

bool KoboCpuFreqGuard::writeText(const char* const leaf, const std::string& value) const {
  const std::string path = root_ + "/" + leaf;
  const int descriptor = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
  if (descriptor < 0) return false;
  const std::string line = value + "\n";
  const ssize_t bytes = ::write(descriptor, line.data(), line.size());
  ::close(descriptor);
  return bytes == static_cast<ssize_t>(line.size());
}

int KoboCpuFreqGuard::currentFrequencyKhz() const {
  std::string value;
  int parsed = -1;
  return readText("scaling_cur_freq", value) && parseInteger(value, parsed) ? parsed : -1;
}

int KoboCpuFreqGuard::maximumFrequencyKhz() const {
  std::string value;
  int parsed = -1;
  return readText("cpuinfo_max_freq", value) && parseInteger(value, parsed) ? parsed : -1;
}

bool KoboCpuFreqGuard::supportsFrequency(const int khz) const {
  std::string values;
  return readText("scaling_available_frequencies", values) && tokenPresent(values, std::to_string(khz));
}

bool KoboCpuFreqGuard::beginPerformanceBoost() {
  if (active_) return true;
  lastError_.clear();
  const int maximum = maximumFrequencyKhz();
  if (maximum <= 0 || !supportsFrequency(maximum)) {
    lastError_ = "kernel does not advertise a valid maximum OPP";
    return false;
  }
  std::string governors;
  if (!readText("scaling_available_governors", governors) || !tokenPresent(governors, "performance")) {
    lastError_ = "performance governor is unavailable";
    return false;
  }
  if (!readText("scaling_governor", previousGovernor_)) {
    lastError_ = "could not read current governor";
    return false;
  }
  if (previousGovernor_ == "performance") {
    active_ = true;
    return true;
  }
  if (!writeText("scaling_governor", "performance")) {
    lastError_ = "could not select performance governor";
    previousGovernor_.clear();
    return false;
  }
  active_ = true;
  return true;
}

bool KoboCpuFreqGuard::endPerformanceBoost() { return restore(); }

bool KoboCpuFreqGuard::restore() {
  if (!active_) return true;
  bool restored = true;
  if (!previousGovernor_.empty() && previousGovernor_ != "performance" &&
      !writeText("scaling_governor", previousGovernor_)) {
    std::fprintf(stderr, "[KOBO] could not restore CPU governor %s\n", previousGovernor_.c_str());
    restored = false;
  }
  active_ = false;
  previousGovernor_.clear();
  return restored;
}

int readSocTemperatureMilliC(const std::string& thermalRoot) {
  DIR* directory = opendir(thermalRoot.c_str());
  if (directory == nullptr) return -1;
  int result = -1;
  while (const dirent* entry = readdir(directory)) {
    if (entry->d_name[0] == '.') continue;
    const std::string name = entry->d_name;
    if (name.rfind("thermal_zone", 0) != 0) continue;
    const std::string zone = thermalRoot + "/" + name;
    struct stat metadata{};
    if (::stat(zone.c_str(), &metadata) != 0 || !S_ISDIR(metadata.st_mode)) continue;
    std::string type;
    std::string temperature;
    int parsed = -1;
    if (readFile(zone + "/type", type) && type == "imx_thermal_zone" && readFile(zone + "/temp", temperature) &&
        parseInteger(temperature, parsed)) {
      result = parsed;
      break;
    }
  }
  closedir(directory);
  return result;
}

}  // namespace crossink::kobo
