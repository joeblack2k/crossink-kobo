// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace crossink::kobo {

// Restricts itself to frequencies the running kernel advertises. It never
// writes OPP tables, voltages, PLLs or a frequency above cpuinfo_max_freq.
class KoboCpuFreqGuard final {
 public:
  explicit KoboCpuFreqGuard(std::string policyRoot = "/sys/devices/system/cpu/cpufreq/policy0");
  ~KoboCpuFreqGuard();
  KoboCpuFreqGuard(const KoboCpuFreqGuard&) = delete;
  KoboCpuFreqGuard& operator=(const KoboCpuFreqGuard&) = delete;

  [[nodiscard]] bool beginPerformanceBoost();
  // Restore the governor captured by beginPerformanceBoost().  The explicit
  // result lets qualification tooling refuse to certify a run if cleanup
  // itself failed; destruction still makes a best-effort final attempt.
  [[nodiscard]] bool endPerformanceBoost();
  [[nodiscard]] bool active() const { return active_; }
  [[nodiscard]] int currentFrequencyKhz() const;
  [[nodiscard]] int maximumFrequencyKhz() const;
  [[nodiscard]] std::string currentGovernor() const;
  [[nodiscard]] const std::string& lastError() const { return lastError_; }

 private:
  bool readText(const char* leaf, std::string& value) const;
  bool writeText(const char* leaf, const std::string& value) const;
  bool supportsFrequency(int khz) const;
  bool restore();

  std::string root_;
  std::string previousGovernor_;
  bool active_ = false;
  std::string lastError_;
};

// Returns the i.MX SoC temperature in milli-Celsius when its thermal zone is
// available, otherwise -1. The caller must treat an unavailable sensor as an
// unavailable safety signal, never as a cool device.
int readSocTemperatureMilliC(const std::string& thermalRoot = "/sys/class/thermal");

}  // namespace crossink::kobo
