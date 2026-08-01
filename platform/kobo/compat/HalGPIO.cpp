#include "HalGPIO.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>

HalGPIO gpio;

void HalGPIO::begin() {
  crossink::kobo::KeyDeviceInfo powerDevice;
  if (crossink::kobo::KoboEvdevKey::discoverPowerKey(powerDevice) && !powerKey_.open(powerDevice)) {
    std::fprintf(stderr, "[KOBO] failed to open power input %s\n", powerDevice.path.c_str());
  }
  if (!battery_.discover()) {
    std::fprintf(stderr, "[KOBO] no battery power_supply found\n");
  }
  crossink::kobo::BatterySnapshot snapshot;
  if (battery_.read(snapshot)) usbConnected_ = snapshot.usbOnline;
}

void HalGPIO::beginFrame() {
  powerKey_.beginFrame();
  usbChanged_ = false;
}

void HalGPIO::update() {
  powerKey_.update();
  crossink::kobo::BatterySnapshot snapshot;
  if (battery_.read(snapshot) && snapshot.usbOnline != usbConnected_) {
    usbConnected_ = snapshot.usbOnline;
    usbChanged_ = true;
  }
}

bool HalGPIO::isPressed(const std::uint8_t buttonIndex) const {
  return buttonIndex == BTN_POWER && powerKey_.isPressed();
}

bool HalGPIO::wasPressed(const std::uint8_t buttonIndex) const {
  return buttonIndex == BTN_POWER && powerKey_.wasPressed();
}

bool HalGPIO::wasAnyPressed() const { return powerKey_.wasPressed(); }

bool HalGPIO::wasReleased(const std::uint8_t buttonIndex) const {
  return buttonIndex == BTN_POWER && powerKey_.wasReleased();
}

bool HalGPIO::wasAnyReleased() const { return powerKey_.wasReleased(); }

unsigned long HalGPIO::getHeldTime() const { return powerKey_.heldMilliseconds(); }

unsigned long HalGPIO::getPowerButtonHeldTime() const { return powerKey_.heldMilliseconds(); }

crossink::kobo::KoboSuspendResult HalGPIO::startDeepSleep() {
  crossink::kobo::KoboFrontlightSysfs frontlight;
  const bool haveFrontlight = frontlight.discover();
  const int savedBrightness = haveFrontlight ? frontlight.percentage() : -1;
  if (haveFrontlight) (void)frontlight.setPercentage(0);
  auto result = crossink::kobo::KoboSuspendController::suspendToRam(
      {"frontlight_off=" + std::to_string(savedBrightness >= 0 ? 1 : 0)});
  if (!result.entered) std::fprintf(stderr, "[KOBO] suspend request failed: %s\n", result.detail.c_str());
  if (savedBrightness >= 0) (void)frontlight.setPercentage(savedBrightness);
  return result;
}

void HalGPIO::verifyPowerButtonWakeup(std::uint16_t /*requiredDurationMs*/, bool /*shortPressAllowed*/) {}
