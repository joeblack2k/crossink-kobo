#include "KoboTouchTransform.h"

#include <algorithm>

namespace crossink::kobo {

KoboTouchTransform::KoboTouchTransform(const TouchCalibration calibration, const ScreenOrientation orientation)
    : calibration_(calibration), orientation_(orientation) {}

bool KoboTouchTransform::valid() const {
  return calibration_.x.maximum > calibration_.x.minimum && calibration_.y.maximum > calibration_.y.minimum;
}

std::int32_t KoboTouchTransform::scale(const std::int32_t value, const RawAxisRange range, const std::int32_t extent) {
  const auto clamped = std::clamp(value, range.minimum, range.maximum);
  const std::int64_t numerator = static_cast<std::int64_t>(clamped - range.minimum) * (extent - 1);
  return static_cast<std::int32_t>(numerator / (range.maximum - range.minimum));
}

TouchPoint KoboTouchTransform::map(const std::int32_t rawX, const std::int32_t rawY) const {
  if (!valid()) {
    return {};
  }

  auto portraitX = scale(rawX, calibration_.x, kPortraitWidth);
  auto portraitY = scale(rawY, calibration_.y, kPortraitHeight);
  if (calibration_.swapAxes) {
    portraitX = scale(rawY, calibration_.y, kPortraitWidth);
    portraitY = scale(rawX, calibration_.x, kPortraitHeight);
  }
  if (calibration_.invertX) {
    portraitX = kPortraitWidth - 1 - portraitX;
  }
  if (calibration_.invertY) {
    portraitY = kPortraitHeight - 1 - portraitY;
  }

  switch (orientation_) {
    case ScreenOrientation::Portrait:
      return {portraitX, portraitY};
    case ScreenOrientation::LandscapeClockwise:
      return {kPortraitHeight - 1 - portraitY, portraitX};
    case ScreenOrientation::Inverted:
      return {kPortraitWidth - 1 - portraitX, kPortraitHeight - 1 - portraitY};
    case ScreenOrientation::LandscapeCounterClockwise:
      return {portraitY, kPortraitWidth - 1 - portraitX};
  }
  return {};
}

std::int32_t KoboTouchTransform::width() const {
  return orientation_ == ScreenOrientation::Portrait || orientation_ == ScreenOrientation::Inverted ? kPortraitWidth
                                                                                                    : kPortraitHeight;
}

std::int32_t KoboTouchTransform::height() const {
  return orientation_ == ScreenOrientation::Portrait || orientation_ == ScreenOrientation::Inverted ? kPortraitHeight
                                                                                                    : kPortraitWidth;
}

void KoboTouchTransform::setOrientation(const ScreenOrientation orientation) { orientation_ = orientation; }

}  // namespace crossink::kobo
