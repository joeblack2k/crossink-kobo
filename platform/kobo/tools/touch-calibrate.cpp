// SPDX-License-Identifier: GPL-3.0-or-later
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "KoboDrmDisplay.h"
#include "KoboEvdevTouch.h"
#include "KoboFbInkDisplay.h"

namespace {

using crossink::kobo::KoboDrmDisplay;
using crossink::kobo::KoboEvdevTouch;
using crossink::kobo::KoboFbInkDisplay;
using crossink::kobo::RefreshKind;
using crossink::kobo::TouchDeviceInfo;
using crossink::kobo::TouchFrame;
using crossink::kobo::TouchPoint;
using Frame = std::array<std::uint8_t, KoboFbInkDisplay::kPackedFrameBytes>;

constexpr std::array<TouchPoint, 5> kTargets{{{110, 110}, {961, 110}, {536, 724}, {110, 1337}, {961, 1337}}};
constexpr int kRadius = 62;
constexpr int kStroke = 5;
constexpr int kTimeoutSeconds = 600;

void setPixel(Frame& frame, const int logicalX, const int logicalY, const bool white) {
  if (logicalX < 0 || logicalY < 0 || logicalX >= KoboFbInkDisplay::kPortraitWidth ||
      logicalY >= KoboFbInkDisplay::kPortraitHeight) {
    return;
  }
  const auto x = static_cast<std::uint16_t>(logicalY);
  const auto y = static_cast<std::uint16_t>(KoboFbInkDisplay::kPanelHeight - 1 - logicalX);
  const std::size_t offset = static_cast<std::size_t>(y) * (KoboFbInkDisplay::kPanelWidth / 8) + x / 8;
  const std::uint8_t mask = static_cast<std::uint8_t>(0x80U >> (x % 8));
  if (white)
    frame[offset] |= mask;
  else
    frame[offset] &= static_cast<std::uint8_t>(~mask);
}

void filledRect(Frame& frame, const int x, const int y, const int width, const int height, const bool white = false) {
  for (int py = y; py < y + height; ++py) {
    for (int px = x; px < x + width; ++px) setPixel(frame, px, py, white);
  }
}

void ring(Frame& frame, const TouchPoint center, const bool completed) {
  const int outerSquared = kRadius * kRadius;
  const int innerRadius = kRadius - kStroke;
  const int innerSquared = innerRadius * innerRadius;
  for (int dy = -kRadius; dy <= kRadius; ++dy) {
    for (int dx = -kRadius; dx <= kRadius; ++dx) {
      const int distance = dx * dx + dy * dy;
      if ((completed && distance <= outerSquared) ||
          (!completed && distance <= outerSquared && distance >= innerSquared))
        setPixel(frame, center.x + dx, center.y + dy, false);
    }
  }
}

void digit(Frame& frame, const TouchPoint center, const int value, const bool completed) {
  constexpr std::array<std::uint8_t, 6> segments{{0, 0x06, 0x5B, 0x4F, 0x66, 0x6D}};
  constexpr int width = 34;
  constexpr int height = 52;
  constexpr int thick = 7;
  const int left = center.x - width / 2;
  const int top = center.y - height / 2;
  const bool white = completed;
  const auto draw = [&](const std::uint8_t bit, const int x, const int y, const int w, const int h) {
    if ((segments[value] & bit) != 0) filledRect(frame, x, y, w, h, white);
  };
  draw(0x01, left, top, width, thick);
  draw(0x02, left + width - thick, top, thick, height / 2);
  draw(0x04, left + width - thick, top + height / 2, thick, height / 2);
  draw(0x08, left, top + height - thick, width, thick);
  draw(0x10, left, top + height / 2, thick, height / 2);
  draw(0x20, left, top, thick, height / 2);
  draw(0x40, left, top + height / 2 - thick / 2, width, thick);
}

void drawTargets(Frame& frame, const std::size_t completed) {
  frame.fill(0xFFU);
  for (std::size_t index = 0; index < kTargets.size(); ++index) {
    const bool done = index < completed;
    ring(frame, kTargets[index], done);
    digit(frame, kTargets[index], static_cast<int>(index + 1), done);
  }
}

int distance(const TouchPoint first, const TouchPoint second) {
  const long dx = first.x - second.x;
  const long dy = first.y - second.y;
  return static_cast<int>(std::lround(std::sqrt(static_cast<double>(dx * dx + dy * dy))));
}

}  // namespace

int main(int argc, char** argv) {
  const char* outputPath = argc > 1 ? argv[1] : "/data/.crossink/calibration/touch-calibration.csv";
  if (argc > 2) {
    std::fprintf(stderr, "usage: %s [output.csv]\n", argv[0]);
    return 2;
  }

  TouchDeviceInfo device;
  if (!KoboEvdevTouch::discover(device)) {
    std::fputs("No absolute touch device found\n", stderr);
    return 1;
  }
  KoboEvdevTouch touch;
  if (!touch.open(device)) {
    std::fprintf(stderr, "Cannot open %s\n", device.path.c_str());
    return 1;
  }
  KoboDrmDisplay display;
  if (!display.open()) {
    std::fprintf(stderr, "Cannot open DRM display: %d\n", display.lastError());
    return 1;
  }

  static Frame frame{};
  drawTargets(frame, 0);
  if (!display.presentPackedMono(frame.data(), frame.size(), RefreshKind::Full)) {
    std::fprintf(stderr, "Cannot draw targets: %d\n", display.lastError());
    return 1;
  }

  const std::string output(outputPath);
  const std::size_t slash = output.find_last_of('/');
  if (slash != std::string::npos) {
    const std::string directory = output.substr(0, slash);
    ::mkdir("/data/.crossink", 0700);
    ::mkdir(directory.c_str(), 0700);
  }
  FILE* results = std::fopen(outputPath, "w");
  if (results == nullptr) {
    std::perror(outputPath);
    return 1;
  }
  std::fprintf(results, "target,expected_x,expected_y,raw_x,raw_y,mapped_x,mapped_y,dx,dy,distance\n");

  std::size_t target = 0;
  bool wasDown = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kTimeoutSeconds);
  while (target < kTargets.size() && std::chrono::steady_clock::now() < deadline) {
    TouchFrame event{};
    bool received = false;
    while (touch.readFrame(event)) {
      received = true;
      if (wasDown && !event.down) {
        const TouchPoint expected = kTargets[target];
        const int dx = event.point.x - expected.x;
        const int dy = event.point.y - expected.y;
        const int error = distance(event.point, expected);
        std::fprintf(results, "%zu,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", target + 1, expected.x, expected.y, event.rawPoint.x,
                     event.rawPoint.y, event.point.x, event.point.y, dx, dy, error);
        std::fflush(results);
        std::printf("target=%zu raw=%d,%d mapped=%d,%d expected=%d,%d error=%d\n", target + 1, event.rawPoint.x,
                    event.rawPoint.y, event.point.x, event.point.y, expected.x, expected.y, error);
        ++target;
        drawTargets(frame, target);
        if (!display.presentPackedMono(frame.data(), frame.size(), RefreshKind::Partial)) {
          std::fclose(results);
          return 1;
        }
      }
      wasDown = event.down;
    }
    if (!received) ::usleep(10'000);
  }
  std::fclose(results);
  if (target != kTargets.size()) {
    std::fprintf(stderr, "Calibration timed out after %zu/5 touches\n", target);
    return 3;
  }
  std::printf("complete output=%s\n", outputPath);
  return 0;
}
