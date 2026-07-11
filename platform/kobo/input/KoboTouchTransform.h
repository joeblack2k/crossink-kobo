#pragma once

#include <cstdint>

namespace crossink::kobo {

enum class ScreenOrientation : std::uint8_t {
  Portrait,
  LandscapeClockwise,
  Inverted,
  LandscapeCounterClockwise,
};

struct RawAxisRange {
  std::int32_t minimum = 0;
  std::int32_t maximum = 0;
};

struct TouchCalibration {
  RawAxisRange x;
  RawAxisRange y;
  bool swapAxes = false;
  bool invertX = false;
  bool invertY = false;
};

struct TouchPoint {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

class KoboTouchTransform {
 public:
  static constexpr std::int32_t kPortraitWidth = 1072;
  static constexpr std::int32_t kPortraitHeight = 1448;

  explicit KoboTouchTransform(TouchCalibration calibration,
                              ScreenOrientation orientation = ScreenOrientation::Portrait);

  [[nodiscard]] bool valid() const;
  [[nodiscard]] TouchPoint map(std::int32_t rawX, std::int32_t rawY) const;
  [[nodiscard]] std::int32_t width() const;
  [[nodiscard]] std::int32_t height() const;

  void setOrientation(ScreenOrientation orientation);

 private:
  TouchCalibration calibration_;
  ScreenOrientation orientation_;

  [[nodiscard]] static std::int32_t scale(std::int32_t value, RawAxisRange range, std::int32_t extent);
};

}  // namespace crossink::kobo
