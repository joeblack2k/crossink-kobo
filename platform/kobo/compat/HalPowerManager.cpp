#include "HalPowerManager.h"

#include <cstdio>

HalPowerManager powerManager;

void HalPowerManager::begin() {
  if (!battery_.discover()) {
    std::fprintf(stderr, "[KOBO] battery HAL unavailable\n");
  }
}

void HalPowerManager::setPowerSaving(bool /*enabled*/) {
  // Linux cpufreq/idle policy remains kernel-controlled. Suspend is the
  // meaningful low-power boundary on N437 and is handled separately.
}

crossink::kobo::KoboSuspendResult HalPowerManager::startDeepSleep(HalGPIO& input) const {
  return input.startDeepSleep();
}

std::uint16_t HalPowerManager::getBatteryPercentage() const {
  crossink::kobo::BatterySnapshot snapshot;
  if (!battery_.read(snapshot) || snapshot.percentage < 0) {
    return 0;
  }
  return static_cast<std::uint16_t>(snapshot.percentage);
}

HalPowerManager::Lock::Lock() { powerManager.setPowerSaving(false); }

HalPowerManager::Lock::~Lock() { powerManager.setPowerSaving(true); }
