#pragma once

#include <cstdint>

#include "KoboEvdevKey.h"
#include "KoboSysfs.h"

class HalGPIO {
 public:
  enum class DeviceType : std::uint8_t { X4, X3 };
  enum class WakeupReason { PowerButton, AfterFlash, AfterUSBPower, Other };

  static constexpr std::uint8_t BTN_BACK = 0;
  static constexpr std::uint8_t BTN_CONFIRM = 1;
  static constexpr std::uint8_t BTN_LEFT = 2;
  static constexpr std::uint8_t BTN_RIGHT = 3;
  static constexpr std::uint8_t BTN_UP = 4;
  static constexpr std::uint8_t BTN_DOWN = 5;
  static constexpr std::uint8_t BTN_POWER = 6;

  [[nodiscard]] bool deviceIsX3() const { return false; }
  [[nodiscard]] bool deviceIsX4() const { return true; }
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
  void startDeepSleep();
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
