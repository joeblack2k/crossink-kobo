#pragma once

#include <cstdint>

#include "HalGPIO.h"
#include "KoboSysfs.h"

class HalPowerManager {
 public:
  static constexpr int LOW_POWER_FREQ = 10;
  static constexpr unsigned long IDLE_POWER_SAVING_MS = 3000;

  void begin();
  void setPowerSaving(bool enabled);
  void startDeepSleep(HalGPIO& gpio) const;
  [[nodiscard]] std::uint16_t getBatteryPercentage() const;

  class Lock {
   public:
    Lock();
    ~Lock();
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock(Lock&&) = delete;
    Lock& operator=(Lock&&) = delete;
  };

 private:
  crossink::kobo::KoboBatterySysfs battery_;
};

extern HalPowerManager powerManager;
