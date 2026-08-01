#pragma once

#include <cstdint>

#include "KoboEvdevTouch.h"

namespace crossink::kobo {

enum class TouchContext : std::uint8_t { Navigation, Reader, Dialog, Keyboard };
enum class TouchAction : std::uint8_t {
  None,
  UiItem,
  Cancelled,
  Back,
  Confirm,
  Left,
  Right,
  Up,
  Down,
  PageBack,
  PageForward
};

enum class TouchGesture : std::uint8_t { None, Start, Tap, LongPressStart, LongPressEnd, Swipe, Cancelled };

struct TouchDispatch {
  TouchAction action = TouchAction::None;
  bool press = false;
  bool release = false;
  TouchPoint point{};
  TouchGesture gesture = TouchGesture::None;
};

class KoboTouchGesture {
 public:
  static constexpr std::int32_t kBottomFrameHeight = 96;
  static constexpr std::int32_t kTapSlop = 24;
  static constexpr std::int32_t kSwipeDistance = 72;
  static constexpr std::uint64_t kLongPressMicros = 650'000;

  [[nodiscard]] TouchDispatch update(const TouchFrame& frame, TouchContext context, std::int32_t screenWidth,
                                     std::int32_t screenHeight);
  [[nodiscard]] bool isActive() const { return active_; }
  void reset();

 private:
  [[nodiscard]] static TouchAction actionAt(TouchPoint point, TouchContext context, std::int32_t screenWidth,
                                            std::int32_t screenHeight);

  bool active_ = false;
  bool longPressActive_ = false;
  TouchPoint start_{};
  TouchPoint latest_{};
  std::uint64_t startedAt_ = 0;
  TouchAction heldAction_ = TouchAction::None;
};

}  // namespace crossink::kobo
