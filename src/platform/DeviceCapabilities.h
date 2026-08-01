#pragma once

#include <HalGPIO.h>

namespace crossink::platform {

enum class DeviceFamily { X4, X3, N437 };

struct DeviceCapabilities {
  DeviceFamily family;
  bool hasTouch;
  bool hasFrontButtons;
  bool hasSideButtons;
  bool sideButtonsAreHorizontal;
  bool hasTilt;
  bool hasRtc;
  bool hasFrontlight;
  bool hasWifi;
  bool hasSuspend;

  [[nodiscard]] constexpr const char* familyName() const {
    switch (family) {
      case DeviceFamily::X3:
        return "X3";
      case DeviceFamily::N437:
        return "Kobo Glo HD N437";
      case DeviceFamily::X4:
      default:
        return "X4";
    }
  }
};

// This is the only shared-code bridge between device families.  Hardware
// probes stay in each HAL: the simulator uses its established X3/X4 probe,
// ESP exposes its native capability object, and Kobo exposes the fixed N437
// hardware inventory.  Activities must not call deviceIsX3/deviceIsX4.
[[nodiscard]] inline DeviceCapabilities deviceCapabilities(const HalGPIO& gpio) {
#if defined(SIMULATOR)
  return gpio.deviceIsX3()
             ? DeviceCapabilities{DeviceFamily::X3, false, true, true, true, true, true, false, true, true}
             : DeviceCapabilities{DeviceFamily::X4, false, true, true, false, false, false, false, true, true};
#else
  const auto raw = gpio.capabilities();
  DeviceFamily family = DeviceFamily::X4;
  switch (raw.family) {
    case HalGPIO::DeviceFamily::X3:
      family = DeviceFamily::X3;
      break;
    case HalGPIO::DeviceFamily::N437:
      family = DeviceFamily::N437;
      break;
    case HalGPIO::DeviceFamily::X4:
    default:
      break;
  }
  return {family,      raw.hasTouch, raw.hasFrontButtons, raw.hasSideButtons, raw.sideButtonsAreHorizontal,
          raw.hasTilt, raw.hasRtc,   raw.hasFrontlight,   raw.hasWifi,        raw.hasSuspend};
#endif
}

}  // namespace crossink::platform
