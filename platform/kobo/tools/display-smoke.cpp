// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <unistd.h>

#include "KoboDrmDisplay.h"
#include "KoboFbInkDisplay.h"
#include "KoboCpuFreq.h"
#include "KoboRefreshQualification.h"

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

void filledRectangle(Frame& frame, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool white) {
  for (uint16_t dy = 0; dy < height; ++dy) {
    for (uint16_t dx = 0; dx < width; ++dx) {
      setPixel(frame, static_cast<uint16_t>(x + dx), static_cast<uint16_t>(y + dy), white);
    }
  }
}

SourceTransform parseTransform(const char* value, bool& ok) {
  ok = true;
  if (std::strcmp(value, "identity") == 0) return SourceTransform::Identity;
  if (std::strcmp(value, "clockwise") == 0) return SourceTransform::RotateClockwise;
  if (std::strcmp(value, "counterclockwise") == 0) return SourceTransform::RotateCounterClockwise;
  ok = false;
  return SourceTransform::Identity;
}

RefreshKind parseRefresh(const char* value, bool& ok) {
  ok = true;
  if (std::strcmp(value, "fast") == 0) return RefreshKind::Fast;
  if (std::strcmp(value, "partial") == 0) return RefreshKind::Partial;
  if (std::strcmp(value, "full") == 0) return RefreshKind::Full;
  ok = false;
  return RefreshKind::Full;
}

unsigned long parseUnsigned(const char* value, const unsigned long maximum, const bool allowZero, bool& ok) {
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  ok = end != value && *end == '\0' && (allowZero || parsed > 0) && parsed <= maximum;
  return parsed;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 6) {
    std::fprintf(stderr,
                 "usage: %s identity|clockwise|counterclockwise [fast|partial|full [iterations [delay-ms "
                 "[--qualify-fast]]]]\n",
                 argv[0]);
    return 2;
  }
  bool valid = false;
  const SourceTransform transform = parseTransform(argv[1], valid);
  if (!valid) return 2;
  const RefreshKind refresh = argc >= 3 ? parseRefresh(argv[2], valid) : RefreshKind::Full;
  if (!valid) return 2;
  const unsigned long iterations = argc >= 4 ? parseUnsigned(argv[3], 10000, false, valid) : 1;
  if (!valid) return 2;
  const unsigned long delayMs = argc >= 5 ? parseUnsigned(argv[4], 60000, true, valid) : 0;
  if (!valid) return 2;
  const bool qualifyFast = argc == 6;
  if (qualifyFast && std::strcmp(argv[5], "--qualify-fast") != 0) return 2;
  if (qualifyFast && (transform != SourceTransform::Identity || refresh != RefreshKind::Fast || iterations < 1000U)) {
    std::fputs("Fast qualification requires identity transform, fast refresh and at least 1000 iterations\n", stderr);
    return 2;
  }

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
    // Qualification is deliberately stricter than a smoke run: it may only
    // use the kernel-advertised performance policy and must abort on a missing
    // thermal signal or an unsafe rise.  KoboCpuFreqGuard restores the prior
    // governor on every return path.
    crossink::kobo::KoboCpuFreqGuard cpuBoost;
    const int baselineTemperature = crossink::kobo::readSocTemperatureMilliC();
    if (qualifyFast && (baselineTemperature < 0 || !cpuBoost.beginPerformanceBoost())) {
      std::fprintf(stderr, "Fast qualification unavailable: temperature=%d cpu=%s\n", baselineTemperature,
                   cpuBoost.lastError().c_str());
      return 1;
    }
    const auto start = std::chrono::steady_clock::now();
    for (unsigned long iteration = 0; iteration < iterations; ++iteration) {
      // Toggle a bounded central region so repeated refreshes exercise changed
      // pixels without destroying the orientation and button-frame markers.
      filledRectangle(frame, 336, 400, 400, 240, (iteration % 2U) != 0U);
      if (!drm.presentPackedMono(frame.data(), frame.size(), refresh)) {
        std::fprintf(stderr, "DRM display refresh failed at iteration %lu: %d\n", iteration + 1, drm.lastError());
        return 1;
      }
      if (qualifyFast && (iteration % 10U) == 0U) {
        const int temperature = crossink::kobo::readSocTemperatureMilliC();
        if (temperature < 0 || temperature >= 75000 || temperature - baselineTemperature >= 15000) {
          std::fprintf(stderr, "Fast qualification thermal stop at iteration %lu: baseline=%d current=%d\n",
                       iteration + 1, baselineTemperature, temperature);
          return 1;
        }
      }
      if (delayMs > 0) ::usleep(delayMs * 1000UL);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    if (qualifyFast && !cpuBoost.endPerformanceBoost()) {
      std::fputs("Fast qualification governor restore failed\n", stderr);
      return 1;
    }
    std::printf("backend=drm geometry=%ux%u iterations=%lu elapsed_ms=%lld\n", KoboFbInkDisplay::kPortraitWidth,
                KoboFbInkDisplay::kPortraitHeight, iterations, static_cast<long long>(elapsed.count()));
    if (qualifyFast) {
      // Persist only after all iterations succeeded and the prior governor
      // was restored.  A marker is device/kernel-bound and never created by
      // the normal UI.
      if (!crossink::kobo::recordKoboFastRefreshQualification()) {
        std::fputs("Fast qualification marker write failed\n", stderr);
        return 1;
      }
    }
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
  if (iterations != 1) {
    std::fputs("Repeated refresh benchmarking requires the DRM backend\n", stderr);
    return 1;
  }
  if (!display.presentPackedMono(frame.data(), frame.size(), refresh)) {
    std::fprintf(stderr, "display refresh failed: %d\n", display.lastError());
    return 1;
  }
  return 0;
}
