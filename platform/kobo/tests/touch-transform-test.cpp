#include <cstdlib>
#include <iostream>

#include "KoboTouchTransform.h"

using crossink::kobo::KoboTouchTransform;
using crossink::kobo::ScreenOrientation;
using crossink::kobo::TouchCalibration;
using crossink::kobo::TouchPoint;

namespace {

void expect(const TouchPoint actual, const TouchPoint expected, const char* label) {
  if (actual.x != expected.x || actual.y != expected.y) {
    std::cerr << label << ": expected " << expected.x << ',' << expected.y << ", got " << actual.x << ',' << actual.y
              << '\n';
    std::exit(EXIT_FAILURE);
  }
}

}  // namespace

int main() {
  KoboTouchTransform transform({{100, 1100}, {200, 2200}});
  if (!transform.valid()) {
    return EXIT_FAILURE;
  }
  expect(transform.map(100, 200), {0, 0}, "portrait minimum");
  expect(transform.map(1100, 2200), {1071, 1447}, "portrait maximum");
  expect(transform.map(-100, 4000), {0, 1447}, "clamping");

  transform.setOrientation(ScreenOrientation::LandscapeClockwise);
  expect(transform.map(100, 200), {1447, 0}, "clockwise minimum");
  expect(transform.map(1100, 2200), {0, 1071}, "clockwise maximum");
  if (transform.width() != 1448 || transform.height() != 1072) {
    return EXIT_FAILURE;
  }

  transform.setOrientation(ScreenOrientation::Inverted);
  expect(transform.map(100, 200), {1071, 1447}, "inverted minimum");

  KoboTouchTransform inverted({{0, 10}, {0, 10}, false, true, true});
  expect(inverted.map(0, 0), {1071, 1447}, "axis inversion");
  return EXIT_SUCCESS;
}
