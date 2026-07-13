#include "KoboTouchGesture.h"

#include <cstdlib>

namespace crossink::kobo {

TouchAction KoboTouchGesture::actionAt(const TouchPoint point, const TouchContext context,
                                       const std::int32_t screenWidth, const std::int32_t screenHeight) {
  if (screenWidth <= 0 || screenHeight <= kBottomFrameHeight || point.x < 0 || point.y < 0 || point.x >= screenWidth ||
      point.y >= screenHeight) {
    return TouchAction::None;
  }
  if (point.y >= screenHeight - kBottomFrameHeight) {
    if (context == TouchContext::Reader) {
      return point.x < screenWidth / 2 ? TouchAction::PageBack : TouchAction::PageForward;
    }
    return point.x < screenWidth / 2 ? TouchAction::Back : TouchAction::Confirm;
  }
  if (context != TouchContext::Reader) {
    return TouchAction::UiItem;
  }
  if (point.x < screenWidth * 3 / 10) {
    return TouchAction::PageBack;
  }
  if (point.x >= screenWidth * 7 / 10) {
    return TouchAction::PageForward;
  }
  return TouchAction::Confirm;
}

TouchDispatch KoboTouchGesture::update(const TouchFrame& frame, const TouchContext context,
                                       const std::int32_t screenWidth, const std::int32_t screenHeight) {
  if (frame.down && !active_) {
    active_ = true;
    longPressActive_ = false;
    start_ = frame.point;
    latest_ = frame.point;
    startedAt_ = frame.timestampMicros;
    heldAction_ = TouchAction::None;
    return {};
  }
  if (!active_) {
    return {};
  }

  latest_ = frame.point;
  const std::int32_t deltaX = latest_.x - start_.x;
  const std::int32_t deltaY = latest_.y - start_.y;
  const bool withinTapSlop = std::abs(deltaX) <= kTapSlop && std::abs(deltaY) <= kTapSlop;

  if (frame.down) {
    if (!longPressActive_ && withinTapSlop && frame.timestampMicros >= startedAt_ &&
        frame.timestampMicros - startedAt_ >= kLongPressMicros) {
      heldAction_ = actionAt(start_, context, screenWidth, screenHeight);
      if (heldAction_ != TouchAction::None) {
        longPressActive_ = true;
        return {heldAction_, true, false, start_};
      }
    }
    return {};
  }

  TouchDispatch dispatch{};
  if (longPressActive_) {
    dispatch = {heldAction_, false, true, start_};
  } else if (withinTapSlop) {
    const TouchAction action = actionAt(start_, context, screenWidth, screenHeight);
    dispatch = {action, action != TouchAction::None, action != TouchAction::None, start_};
  } else if (std::abs(deltaX) >= kSwipeDistance && std::abs(deltaX) > std::abs(deltaY)) {
    const TouchAction action = context == TouchContext::Reader
                                   ? (deltaX < 0 ? TouchAction::PageForward : TouchAction::PageBack)
                                   : (deltaX < 0 ? TouchAction::Right : TouchAction::Left);
    dispatch = {action, true, true, start_};
  } else if (context != TouchContext::Reader && std::abs(deltaY) >= kSwipeDistance) {
    const TouchAction action = deltaY < 0 ? TouchAction::Down : TouchAction::Up;
    dispatch = {action, true, true, start_};
  }
  reset();
  return dispatch;
}

void KoboTouchGesture::reset() {
  active_ = false;
  longPressActive_ = false;
  startedAt_ = 0;
  heldAction_ = TouchAction::None;
}

}  // namespace crossink::kobo
