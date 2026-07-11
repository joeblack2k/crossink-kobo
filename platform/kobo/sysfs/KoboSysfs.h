#pragma once

#include <cstdint>
#include <string>

namespace crossink::kobo {

enum class BatteryState : std::uint8_t { Unknown, Charging, Discharging, Full, NotCharging };

struct BatterySnapshot {
  int percentage = -1;
  BatteryState state = BatteryState::Unknown;
  bool usbOnline = false;
};

class KoboBatterySysfs {
 public:
  [[nodiscard]] bool discover(const std::string& root = "/sys/class/power_supply");
  [[nodiscard]] bool read(BatterySnapshot& snapshot) const;
  [[nodiscard]] const std::string& batteryPath() const { return batteryPath_; }
  [[nodiscard]] const std::string& usbPath() const { return usbPath_; }

 private:
  std::string batteryPath_;
  std::string usbPath_;
};

class KoboFrontlightSysfs {
 public:
  [[nodiscard]] bool discover(const std::string& root = "/sys/class/backlight");
  [[nodiscard]] bool setPercentage(int percentage) const;
  [[nodiscard]] int percentage() const;
  [[nodiscard]] int maximum() const { return maximum_; }
  [[nodiscard]] const std::string& devicePath() const { return devicePath_; }

 private:
  std::string devicePath_;
  int maximum_ = 0;
};

}  // namespace crossink::kobo
