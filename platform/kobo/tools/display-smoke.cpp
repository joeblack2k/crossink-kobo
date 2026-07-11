// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <cstdio>
#include <cstring>

#include "KoboDrmDisplay.h"
#include "KoboFbInkDisplay.h"

using crossink::kobo::KoboFbInkDisplay;
using crossink::kobo::KoboDrmDisplay;
using crossink::kobo::RefreshKind;
using crossink::kobo::SourceTransform;

namespace {

using Frame = std::array<uint8_t, KoboFbInkDisplay::kPackedFrameBytes>;

void setPixel(Frame& frame, uint16_t logicalX, uint16_t logicalY, bool white) {
  if (logicalX >= KoboFbInkDisplay::kPortraitWidth || logicalY >= KoboFbInkDisplay::kPortraitHeight) return;
  // Match GfxRenderer::Portrait: logical portrait is rotated clockwise into
  // the native landscape packed buffer.
  const uint16_t x = logicalY;
  const uint16_t y = static_cast<uint16_t>(KoboFbInkDisplay::kPanelHeight - 1U - logicalX);
  const size_t offset = static_cast<size_t>(y) * (KoboFbInkDisplay::kPanelWidth / 8) + x / 8;
  const uint8_t mask = static_cast<uint8_t>(0x80U >> (x % 8));
  if (white) {
    frame[offset] |= mask;
  } else {
    frame[offset] &= static_cast<uint8_t>(~mask);
  }
}

void horizontalLine(Frame& frame, uint16_t x, uint16_t y, uint16_t width) {
  for (uint16_t dx = 0; dx < width; ++dx) setPixel(frame, static_cast<uint16_t>(x + dx), y, false);
}

void verticalLine(Frame& frame, uint16_t x, uint16_t y, uint16_t height) {
  for (uint16_t dy = 0; dy < height; ++dy) setPixel(frame, x, static_cast<uint16_t>(y + dy), false);
}

void rectangle(Frame& frame, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  horizontalLine(frame, x, y, width);
  horizontalLine(frame, x, static_cast<uint16_t>(y + height - 1U), width);
  verticalLine(frame, x, y, height);
  verticalLine(frame, static_cast<uint16_t>(x + width - 1U), y, height);
}

SourceTransform parseTransform(const char* value, bool& ok) {
  ok = true;
  if (std::strcmp(value, "identity") == 0) return SourceTransform::Identity;
  if (std::strcmp(value, "clockwise") == 0) return SourceTransform::RotateClockwise;
  if (std::strcmp(value, "counterclockwise") == 0) return SourceTransform::RotateCounterClockwise;
  ok = false;
  return SourceTransform::Identity;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s identity|clockwise|counterclockwise\n", argv[0]);
    return 2;
  }
  bool valid = false;
  const SourceTransform transform = parseTransform(argv[1], valid);
  if (!valid) return 2;

  // Keep the 194 KiB packed framebuffer out of the small process stack.
  static Frame frame{};
  frame.fill(0xFFU);
  rectangle(frame, 0, 0, KoboFbInkDisplay::kPortraitWidth, KoboFbInkDisplay::kPortraitHeight);
  const uint16_t frameTop = KoboFbInkDisplay::kPortraitHeight - 96U;
  horizontalLine(frame, 0, frameTop, KoboFbInkDisplay::kPortraitWidth);
  verticalLine(frame, KoboFbInkDisplay::kPortraitWidth / 2U, frameTop, 96U);
  rectangle(frame, 32, 32, 240, 160);
  rectangle(frame, 400, 32, 240, 160);
  rectangle(frame, 768, 32, 240, 160);

  KoboDrmDisplay drm;
  if (drm.open()) {
    if (!drm.presentPackedMono(frame.data(), frame.size(), RefreshKind::Full)) {
      std::fprintf(stderr, "DRM display refresh failed: %d\n", drm.lastError());
      return 1;
    }
    std::printf("backend=drm geometry=%ux%u\n", KoboFbInkDisplay::kPortraitWidth,
                KoboFbInkDisplay::kPortraitHeight);
    return 0;
  }

  KoboFbInkDisplay display(transform);
  if (!display.open()) {
    std::fprintf(stderr, "FBInk open failed: %d\n", display.lastError());
    return 1;
  }
  const auto& geometry = display.geometry();
  std::printf("geometry=%ux%u stride=%u bpp=%u native_landscape=%u\n", geometry.width, geometry.height, geometry.stride,
              geometry.bitsPerPixel, geometry.nativeLandscape ? 1U : 0U);
  if (!display.presentPackedMono(frame.data(), frame.size(), RefreshKind::Full)) {
    std::fprintf(stderr, "display refresh failed: %d\n", display.lastError());
    return 1;
  }
  return 0;
}
