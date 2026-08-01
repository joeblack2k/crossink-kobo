#pragma once

#include <cstdint>

#include "KoboEvdevKey.h"
#include "KoboSuspendController.h"
#include "KoboSysfs.h"

class HalGPIO {
 public:
  enum class DeviceFamily : std::uint8_t { X4, X3, N437 };
  struct Capabilities {
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
  };
  enum class WakeupReason { PowerButton, AfterFlash, AfterUSBPower, Other };

  static constexpr std::uint8_t BTN_BACK = 0;
  static constexpr std::uint8_t BTN_CONFIRM = 1;
  static constexpr std::uint8_t BTN_LEFT = 2;
  static constexpr std::uint8_t BTN_RIGHT = 3;
  static constexpr std::uint8_t BTN_UP = 4;
  static constexpr std::uint8_t BTN_DOWN = 5;
  static constexpr std::uint8_t BTN_POWER = 6;

  [[nodiscard]] static constexpr Capabilities capabilities() {
    return {DeviceFamily::N437, true, false, false, false, false, false, true, true, true};
  }
  [[nodiscard]] static constexpr DeviceFamily deviceFamily() { return capabilities().family; }
  [[nodiscard]] static constexpr const char* deviceFamilyName() { return "Kobo Glo HD N437"; }
  [[nodiscard]] static constexpr bool hasTouch() { return capabilities().hasTouch; }
  [[nodiscard]] static constexpr bool hasFrontButtons() { return capabilities().hasFrontButtons; }
  [[nodiscard]] static constexpr bool hasSideButtons() { return capabilities().hasSideButtons; }
  [[nodiscard]] static constexpr bool sideButtonsAreHorizontal() { return capabilities().sideButtonsAreHorizontal; }
  [[nodiscard]] static constexpr bool hasTilt() { return capabilities().hasTilt; }
  [[nodiscard]] static constexpr bool hasRtc() { return capabilities().hasRtc; }
  [[nodiscard]] static constexpr bool hasFrontlight() { return capabilities().hasFrontlight; }
  [[nodiscard]] static constexpr bool hasWifi() { return capabilities().hasWifi; }
  [[nodiscard]] static constexpr bool hasSuspend() { return capabilities().hasSuspend; }
  void begin();
  void beginFrame();
  void update();
  [[nodiscard]] bool isPressed(std::uint8_t buttonIndex) const;
  [[nodiscard]] bool wasPressed(std::uint8_t buttonIndex) const;
  [[nodiscard]] bool wasAnyPressed() const;
  [[nodiscard]] bool wasReleased(std::uint8_t buttonIndex) const;
  [[nodiscard]] bool wasAnyReleased() const;
  [[nodiscard]] unsigned long getHeldTime() const;
  [[nodiscard]] unsigned long getPowerButtonHeldTime() const;
  [[nodiscard]] bool consumeSimulatorSleepRequest() const { return false; }
  [[nodiscard]] crossink::kobo::KoboSuspendResult startDeepSleep();
  void verifyPowerButtonWakeup(std::uint16_t requiredDurationMs, bool shortPressAllowed);
  [[nodiscard]] bool isUsbConnected() const { return usbConnected_; }
  [[nodiscard]] bool wasUsbStateChanged() const { return usbChanged_; }
  [[nodiscard]] WakeupReason getWakeupReason() const { return WakeupReason::Other; }

 private:
  crossink::kobo::KoboEvdevKey powerKey_;
  crossink::kobo::KoboBatterySysfs battery_;
  bool usbConnected_ = false;
  bool usbChanged_ = false;
};

extern HalGPIO gpio;
