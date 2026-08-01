#pragma once

#include <cstdint>

namespace CrossPointOrientation {
enum Value : std::uint8_t { PORTRAIT = 0, LANDSCAPE_CW = 1, INVERTED = 2, LANDSCAPE_CCW = 3 };
}

namespace CrossPointTiltPageTurn {
enum Value : std::uint8_t { TILT_OFF = 0, TILT_NORMAL = 1, TILT_INVERTED = 2 };
}

class HalTiltSensor {
 public:
  void begin() {}
  [[nodiscard]] bool wake() { return false; }
  [[nodiscard]] bool deepSleep() { return false; }
  [[nodiscard]] bool isAvailable() const { return false; }
  void update(std::uint8_t mode, std::uint8_t orientation, bool inReader);
  void update(std::uint8_t mode, std::uint8_t direction, std::uint8_t orientation, bool inReader);
  [[nodiscard]] bool wasTiltedForward() const { return false; }
  [[nodiscard]] bool wasTiltedBack() const { return false; }
  [[nodiscard]] bool hadActivity() const { return false; }
  void clearPendingEvents() {}
};

extern HalTiltSensor halTiltSensor;
