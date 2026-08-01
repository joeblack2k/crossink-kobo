#pragma once

#include <HalGPIO.h>

#include <array>
#include <cstdint>

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };
  static constexpr size_t BUTTON_COUNT = static_cast<size_t>(Button::PageForward) + 1;

  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };

  explicit MappedInputManager(HalGPIO& gpio) : gpio(gpio) {}

  // Enable/disable reader-specific front button mapping.
  // Call with true in reader activity onEnter(), false in onExit().
  void setReaderMode(bool enabled) { readerMode = enabled; }
  [[nodiscard]] bool isReaderMode() const { return readerMode; }
  void setPowerAsConfirmInReaderMode(bool enabled) { powerAsConfirmInReaderMode = enabled; }

  void update() const { gpio.update(); }
  void suppressNextBackRelease() { suppressBackRelease = true; }
  void suppressNextConfirmRelease() { suppressConfirmRelease = true; }
  void suppressNextPowerRelease() { suppressPowerRelease = true; }
  void suppressNextPowerConfirmRelease() { suppressPowerConfirmRelease = true; }
  bool wasPressed(Button button) const;
  bool wasReleased(Button button) const;
  bool isPressed(Button button) const;
  bool wasAnyPressed() const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;
  Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next) const;
  // Returns the raw front button index that was pressed this frame (or -1 if none).
  int getPressedFrontButton() const;
  // Returns the raw front button index that was released this frame (or -1 if none).
  int getReleasedFrontButton() const;
  bool isFrontButtonPressed(uint8_t buttonIndex) const;

#if defined(SIMULATOR) || defined(KOBO_LINUX)
  struct TouchTarget {
    unsigned char kind = 0;
    int primary = 0;
    int secondary = 0;
    std::uint32_t generation = 0;
    int x = -1;
    int y = -1;
  };

  // Platform touch/keyboard adapters inject logical actions here; activities
  // remain independent of evdev, SDL and raw hardware button numbers.
  void injectPress(Button button);
  void injectRelease(Button button);
  void cancelInjectedPress(Button button);
  void clearInjectedInputFrame();
  void injectTouchTarget(unsigned char kind, int primary, int secondary, std::uint32_t generation, int x = -1,
                         int y = -1);
  bool consumeTouchTarget(TouchTarget& target);
  // A list row published by TouchUiRegistry.  Kobo activities consume this
  // directly instead of replaying X4-style Up/Down presses one frame at a
  // time.  `currentIndex` is retained only for the temporary legacy fallback.
  bool consumeNavigationTouchTarget(int& targetIndex, int& currentIndex);
#endif
#ifdef SIMULATOR
  void simulatorInjectPress(Button button) { injectPress(button); }
  void simulatorInjectRelease(Button button) { injectRelease(button); }
  void simulatorClearInputFrame() { clearInjectedInputFrame(); }
#endif

 private:
  HalGPIO& gpio;
  bool readerMode = false;
  bool powerAsConfirmInReaderMode = false;
  mutable bool suppressBackRelease = false;
  mutable bool suppressConfirmRelease = false;
  mutable bool suppressPowerRelease = false;
  mutable bool suppressPowerConfirmRelease = false;
#if defined(SIMULATOR) || defined(KOBO_LINUX)
  std::array<bool, BUTTON_COUNT> injectedPressed{};
  std::array<bool, BUTTON_COUNT> injectedReleased{};
  std::array<bool, BUTTON_COUNT> injectedHeld{};
  std::array<unsigned long, BUTTON_COUNT> injectedPressStart{};
  static constexpr std::size_t TOUCH_TARGET_QUEUE_CAPACITY = 8;
  std::array<TouchTarget, TOUCH_TARGET_QUEUE_CAPACITY> injectedTouchTargets{};
  std::size_t injectedTouchTargetHead = 0;
  std::size_t injectedTouchTargetCount = 0;
#endif

  bool mapButton(Button button, bool (HalGPIO::*fn)(uint8_t) const) const;
  bool shouldUsePowerAsConfirmFallback() const;
  bool shouldMirrorPowerAsConfirmHold() const;
};
