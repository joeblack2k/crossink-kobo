// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#include <unistd.h>

#include "DisplayBenchmark.h"
#include "KoboDrmDisplay.h"
#include "KoboFbInkDisplay.h"
#include "KoboCpuFreq.h"
#include "KoboRefreshQualification.h"

using crossink::kobo::KoboFbInkDisplay;
using crossink::kobo::KoboDrmDisplay;
using crossink::kobo::RefreshRegion;
using crossink::kobo::RefreshKind;
using crossink::kobo::SourceTransform;

namespace {

using Frame = std::array<uint8_t, KoboFbInkDisplay::kPackedFrameBytes>;

struct LogicalRegion {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

// These are representative reader/UI dirty regions in logical portrait
// coordinates: a text block, header action, list card and footer control.
constexpr std::array<LogicalRegion, 4> kMixedRegions{{
    {336U, 400U, 400U, 240U},
    {32U, 48U, 176U, 64U},
    {96U, 320U, 880U, 80U},
    {704U, 840U, 256U, 320U},
}};

// These are queue-submission safety limits, not physical panel-settle times.
// The pinned EPDC DRM UAPI offers no completion fence in userspace, therefore
// no benchmark output may call them page-turn latency.
constexpr std::int64_t kPartialSubmitDeadlineMicros = 2'000'000;
constexpr std::int64_t kFullSubmitDeadlineMicros = 3'000'000;
constexpr int kThermalStopMilliC = 75'000;
constexpr int kThermalRiseStopMilliC = 15'000;

enum class BenchmarkProfile : std::uint8_t {
  Safe,
  Fast,
};

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

RefreshRegion toPanelRegion(const LogicalRegion& logical) {
  // GfxRenderer portrait rotates clockwise into the DRM-native 1448x1072
  // buffer. Keep the dirty rectangle in the same coordinate space as the
  // changed pixels; otherwise benchmarked partial refreshes would be fake.
  return {
      .x = logical.y,
      .y = static_cast<uint16_t>(KoboFbInkDisplay::kPanelHeight - logical.x - logical.width),
      .width = logical.height,
      .height = logical.width,
      .changedBytes = static_cast<std::size_t>(logical.width) * logical.height / 8U,
  };
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

BenchmarkProfile parseProfile(const char* value, bool& ok) {
  ok = true;
  if (std::strcmp(value, "safe") == 0) return BenchmarkProfile::Safe;
  if (std::strcmp(value, "fast") == 0) return BenchmarkProfile::Fast;
  ok = false;
  return BenchmarkProfile::Safe;
}

const char* profileName(const BenchmarkProfile profile) {
  return profile == BenchmarkProfile::Fast ? "Fast" : "Safe";
}

unsigned long parseUnsigned(const char* value, const unsigned long maximum, const bool allowZero, bool& ok) {
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  ok = end != value && *end == '\0' && (allowZero || parsed > 0) && parsed <= maximum;
  return parsed;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 9) {
    std::fprintf(stderr,
                 "usage: %s identity|clockwise|counterclockwise [fast|partial|full [iterations [delay-ms "
                 "[--mixed] [--profile safe|fast] [--qualify-fast]]]]\n",
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
  bool qualifyFast = false;
  bool mixedPattern = false;
  BenchmarkProfile profile = BenchmarkProfile::Safe;
  for (int index = 5; index < argc; ++index) {
    if (std::strcmp(argv[index], "--qualify-fast") == 0) {
      qualifyFast = true;
    } else if (std::strcmp(argv[index], "--mixed") == 0) {
      mixedPattern = true;
    } else if (std::strcmp(argv[index], "--profile") == 0 && index + 1 < argc) {
      profile = parseProfile(argv[++index], valid);
      if (!valid) return 2;
    } else {
      return 2;
    }
  }
  if (mixedPattern && transform != SourceTransform::Identity) {
    std::fputs("Mixed qualification only supports logical portrait identity\n", stderr);
    return 2;
  }
  if (qualifyFast && (profile != BenchmarkProfile::Fast || transform != SourceTransform::Identity ||
                      refresh != RefreshKind::Fast || iterations < 1000U || !mixedPattern)) {
    std::fputs("Fast qualification requires identity transform, fast refresh, mixed regions and at least 1000 iterations\n",
               stderr);
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
    // Fast runs are transient test runs, not a user-visible profile
    // activation. They exercise exactly the kernel-advertised performance
    // policy that Fast would request; the persistent marker remains reserved
    // for a successful 1000-update physical qualification below.
    crossink::kobo::KoboCpuFreqGuard cpuBoost;
    const int baselineTemperature = crossink::kobo::readSocTemperatureMilliC();
    const int cpuBefore = cpuBoost.currentFrequencyKhz();
    const std::string governorBefore = cpuBoost.currentGovernor();
    if (profile == BenchmarkProfile::Fast && (baselineTemperature < 0 || !cpuBoost.beginPerformanceBoost())) {
      std::fprintf(stderr, "Fast benchmark unavailable: temperature=%d cpu=%s\n", baselineTemperature,
                   cpuBoost.lastError().c_str());
      return 1;
    }
    const int cpuDuring = cpuBoost.currentFrequencyKhz();
    const std::string governorDuring = cpuBoost.currentGovernor();
    const auto start = std::chrono::steady_clock::now();
    static std::array<std::uint32_t, 10000> submitMicros{};
    for (unsigned long iteration = 0; iteration < iterations; ++iteration) {
      // Toggle a bounded central region so repeated refreshes exercise changed
      // pixels without destroying the orientation and button-frame markers.
      const LogicalRegion logical = mixedPattern ? kMixedRegions[iteration % kMixedRegions.size()] : kMixedRegions[0];
      filledRectangle(frame, logical.x, logical.y, logical.width, logical.height, (iteration % 2U) != 0U);
      const RefreshRegion region = toPanelRegion(logical);
      const auto submitted = std::chrono::steady_clock::now();
      const bool presented = drm.presentPackedMono(frame.data(), frame.size(), refresh, region);
      const auto completed = std::chrono::steady_clock::now();
      const auto submitMicrosValue =
          std::chrono::duration_cast<std::chrono::microseconds>(completed - submitted).count();
      if (submitMicrosValue < 0 || submitMicrosValue > static_cast<long long>(UINT32_MAX)) {
        std::fputs("DRM display submit timing overflow\n", stderr);
        return 1;
      }
      submitMicros[iteration] = static_cast<std::uint32_t>(submitMicrosValue);
      const std::int64_t deadline = refresh == RefreshKind::Full ? kFullSubmitDeadlineMicros : kPartialSubmitDeadlineMicros;
      if (!presented || submitMicrosValue > deadline) {
        std::fprintf(stderr,
                     "profile=%s pattern=%s waveform=%s failed_iteration=%lu ioctl_failures=%u deadline_exceeded=%u "
                     "submit_us=%lld errno=%d\n",
                     profileName(profile), mixedPattern ? "mixed" : "fixed", refresh == RefreshKind::Fast ? "DU" :
                     (refresh == RefreshKind::Partial ? "AUTO" : "GC16"), iteration + 1, presented ? 0U : 1U,
                     submitMicrosValue > deadline ? 1U : 0U, static_cast<long long>(submitMicrosValue), drm.lastError());
        return 1;
      }
      if (profile == BenchmarkProfile::Fast && (iteration % 10U) == 0U) {
        const int temperature = crossink::kobo::readSocTemperatureMilliC();
        if (temperature < 0 || temperature >= kThermalStopMilliC || temperature - baselineTemperature >= kThermalRiseStopMilliC) {
          std::fprintf(stderr, "Fast benchmark thermal stop at iteration %lu: baseline=%d current=%d\n",
                       iteration + 1, baselineTemperature, temperature);
          return 1;
        }
      }
      if (delayMs > 0) ::usleep(delayMs * 1000UL);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    if (profile == BenchmarkProfile::Fast && !cpuBoost.endPerformanceBoost()) {
      std::fputs("Fast benchmark governor restore failed\n", stderr);
      return 1;
    }
    const int cpuAfter = cpuBoost.currentFrequencyKhz();
    const std::string governorAfter = cpuBoost.currentGovernor();
    const int finalTemperature = crossink::kobo::readSocTemperatureMilliC();
    std::printf("backend=drm geometry=%ux%u iterations=%lu elapsed_ms=%lld\n", KoboFbInkDisplay::kPortraitWidth,
                KoboFbInkDisplay::kPortraitHeight, iterations, static_cast<long long>(elapsed.count()));
    const auto summary = crossink::kobo::summarizeDisplaySubmissionMicros(submitMicros.data(), iterations);
    std::printf("profile=%s pattern=%s waveform=%s submit_us_min=%u p50=%u p95=%u max=%u total=%llu "
                "ioctl_failures=0 deadline_exceeded=0 driver_waveforms=DU,AUTO,GC16 gc4_a2=not_exposed "
                "cpu_before_khz=%d "
                "cpu_during_khz=%d cpu_after_khz=%d governor_before=%s governor_during=%s governor_after=%s "
                "temp_before_millic=%d temp_after_millic=%d\n",
                profileName(profile), mixedPattern ? "mixed" : "fixed", refresh == RefreshKind::Fast ? "DU" :
                (refresh == RefreshKind::Partial ? "AUTO" : "GC16"), summary.minimumMicros, summary.p50Micros,
                summary.p95Micros, summary.maximumMicros, static_cast<unsigned long long>(summary.totalMicros), cpuBefore,
                cpuDuring, cpuAfter, governorBefore.c_str(), governorDuring.c_str(), governorAfter.c_str(),
                baselineTemperature, finalTemperature);
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
