#pragma once

#include <cstdint>
#include <string>

#include "KoboTouchTransform.h"

namespace crossink::kobo {

struct TouchDeviceInfo {
  std::string path;
  std::string name;
  RawAxisRange x;
  RawAxisRange y;
  bool usesMultitouchAxes = false;
};

struct TouchFrame {
  TouchPoint point;
  bool down = false;
  bool positionChanged = false;
  std::uint64_t timestampMicros = 0;
  TouchPoint rawPoint;
};

class KoboEvdevTouch {
 public:
  KoboEvdevTouch() = default;
  ~KoboEvdevTouch();

  KoboEvdevTouch(const KoboEvdevTouch&) = delete;
  KoboEvdevTouch& operator=(const KoboEvdevTouch&) = delete;

  [[nodiscard]] static bool discover(TouchDeviceInfo& result, const std::string& inputDirectory = "/dev/input");
  [[nodiscard]] bool open(const TouchDeviceInfo& device, TouchCalibration calibration = {});
  void close();
  void setOrientation(ScreenOrientation orientation);

  // Returns one complete evdev SYN_REPORT frame. EAGAIN is reported as false.
  [[nodiscard]] bool readFrame(TouchFrame& frame);
  [[nodiscard]] bool isOpen() const { return fd_ >= 0; }
  [[nodiscard]] const TouchDeviceInfo& device() const { return device_; }

 private:
  int fd_ = -1;
  TouchDeviceInfo device_;
  KoboTouchTransform transform_{{}};
  std::int32_t rawX_ = 0;
  std::int32_t rawY_ = 0;
  bool down_ = false;
  bool positionChanged_ = false;
};

}  // namespace crossink::kobo
