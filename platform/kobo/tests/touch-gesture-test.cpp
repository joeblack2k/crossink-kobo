#include <cstdlib>
#include <iostream>

#include "KoboTouchGesture.h"

using crossink::kobo::KoboTouchGesture;
using crossink::kobo::TouchAction;
using crossink::kobo::TouchContext;
using crossink::kobo::TouchDispatch;
using crossink::kobo::TouchFrame;

namespace {

constexpr int width = 1072;
constexpr int height = 1448;

[[noreturn]] void fail(const char* label) {
  std::cerr << label << '\n';
  std::exit(EXIT_FAILURE);
}

void expect(const TouchDispatch dispatch, const TouchAction action, const bool press, const bool release,
            const char* label) {
  if (dispatch.action != action || dispatch.press != press || dispatch.release != release) {
    fail(label);
  }
}

TouchFrame frame(const int x, const int y, const bool down, const std::uint64_t timestamp) {
  return {{x, y}, down, true, timestamp, {x, y}};
}

}  // namespace

int main() {
  KoboTouchGesture gesture;
  expect(gesture.update(frame(100, 1400, true, 1'000), TouchContext::Navigation, width, height), TouchAction::None,
         false, false, "down must wait");
  expect(gesture.update(frame(100, 1400, false, 100'000), TouchContext::Navigation, width, height), TouchAction::Back,
         true, true, "bottom left navigation");

  expect(gesture.update(frame(500, 300, true, 1'000), TouchContext::Navigation, width, height), TouchAction::None,
         false, false, "navigation upper down must wait");
  expect(gesture.update(frame(500, 300, false, 100'000), TouchContext::Navigation, width, height), TouchAction::UiItem,
         true, true, "navigation upper item");
  expect(gesture.update(frame(500, 1000, true, 1'000), TouchContext::Navigation, width, height), TouchAction::None,
         false, false, "navigation lower down must wait");
  expect(gesture.update(frame(500, 1000, false, 100'000), TouchContext::Navigation, width, height), TouchAction::UiItem,
         true, true, "navigation lower item");

  expect(gesture.update(frame(500, 900, true, 1'000), TouchContext::Navigation, width, height), TouchAction::None,
         false, false, "vertical swipe down must wait");
  expect(gesture.update(frame(500, 700, false, 100'000), TouchContext::Navigation, width, height), TouchAction::Down,
         true, true, "upward swipe advances focus");

  expect(gesture.update(frame(700, 500, true, 1'000), TouchContext::Navigation, width, height), TouchAction::None,
         false, false, "horizontal navigation swipe down must wait");
  expect(gesture.update(frame(450, 500, false, 100'000), TouchContext::Navigation, width, height), TouchAction::Right,
         true, true, "left swipe adjusts right");

  expect(gesture.update(frame(900, 500, true, 1'000), TouchContext::Reader, width, height), TouchAction::None, false,
         false, "reader down must wait");
  expect(gesture.update(frame(900, 500, false, 100'000), TouchContext::Reader, width, height), TouchAction::PageForward,
         true, true, "reader right zone");

  expect(gesture.update(frame(500, 500, true, 1'000), TouchContext::Reader, width, height), TouchAction::None, false,
         false, "swipe down must wait");
  expect(gesture.update(frame(300, 500, false, 100'000), TouchContext::Reader, width, height), TouchAction::PageForward,
         true, true, "left swipe advances");

  expect(gesture.update(frame(100, 500, true, 1'000), TouchContext::Reader, width, height), TouchAction::None, false,
         false, "long down must wait");
  expect(gesture.update(frame(100, 500, true, 700'000), TouchContext::Reader, width, height), TouchAction::PageBack,
         true, false, "long press starts hold");
  expect(gesture.update(frame(100, 500, false, 800'000), TouchContext::Reader, width, height), TouchAction::PageBack,
         false, true, "long press releases hold");
  return EXIT_SUCCESS;
}
