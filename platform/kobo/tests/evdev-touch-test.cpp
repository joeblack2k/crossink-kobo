#include <linux/input.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include "KoboEvdevAbi.h"
#include "KoboEvdevTouch.h"

using crossink::kobo::KoboEvdevEvent;
using crossink::kobo::KoboEvdevTouch;
using crossink::kobo::RawAxisRange;
using crossink::kobo::TouchDeviceInfo;
using crossink::kobo::TouchFrame;
using crossink::kobo::TouchReadResult;

namespace {

void appendEvent(FILE* stream, const std::uint16_t type, const std::uint16_t code, const std::int32_t value,
                 const std::int32_t seconds, const std::int32_t micros) {
  const KoboEvdevEvent event{seconds, micros, type, code, value};
  if (std::fwrite(&event, sizeof(event), 1, stream) != 1) std::exit(EXIT_FAILURE);
}

}  // namespace

int main() {
  char path[] = "/tmp/crossink-evdev-touch-XXXXXX";
  const int descriptor = mkstemp(path);
  if (descriptor < 0) return EXIT_FAILURE;
  FILE* stream = fdopen(descriptor, "w+b");
  if (stream == nullptr) return EXIT_FAILURE;

  appendEvent(stream, EV_SYN, SYN_DROPPED, 0, 10, 0);
  appendEvent(stream, EV_SYN, SYN_REPORT, 0, 10, 100);
  appendEvent(stream, EV_KEY, BTN_TOUCH, 1, 11, 0);
  appendEvent(stream, EV_ABS, ABS_X, 500, 11, 0);
  appendEvent(stream, EV_ABS, ABS_Y, 700, 11, 0);
  appendEvent(stream, EV_SYN, SYN_REPORT, 0, 11, 250);
  appendEvent(stream, EV_KEY, BTN_TOUCH, 0, 11, 500);
  appendEvent(stream, EV_SYN, SYN_REPORT, 0, 11, 750);
  std::fflush(stream);
  std::fclose(stream);

  KoboEvdevTouch touch;
  const TouchDeviceInfo device{path, "test", RawAxisRange{0, 1000}, RawAxisRange{0, 1000}, false};
  if (!touch.open(device)) {
    std::remove(path);
    return EXIT_FAILURE;
  }

  TouchFrame frame{};
  if (!touch.readFrame(frame) || !frame.discontinuity || frame.down || frame.timestampMicros != 10'000'100) {
    std::remove(path);
    return EXIT_FAILURE;
  }
  if (!touch.readFrame(frame) || frame.discontinuity || !frame.down || frame.timestampMicros != 11'000'250 ||
      frame.rawPoint.x != 500 || frame.rawPoint.y != 700) {
    std::remove(path);
    return EXIT_FAILURE;
  }
  if (!touch.readFrame(frame) || frame.discontinuity || frame.down || frame.timestampMicros != 11'000'750) {
    std::remove(path);
    return EXIT_FAILURE;
  }
  touch.close();
  if (touch.readFrameDetailed(frame) != TouchReadResult::DeviceLost) {
    std::remove(path);
    return EXIT_FAILURE;
  }
  std::remove(path);
  return EXIT_SUCCESS;
}
