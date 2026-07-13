// SPDX-License-Identifier: GPL-3.0-or-later
#include "KoboRefreshExecutor.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include "KoboCpuFreq.h"
#include "KoboDrmDisplay.h"

namespace crossink::kobo {
namespace {

constexpr std::int64_t kPartialSubmitDeadlineMicros = 2'000'000;
constexpr std::int64_t kFullSubmitDeadlineMicros = 3'000'000;
constexpr int kThermalStopMilliC = 75'000;
constexpr int kThermalRiseStopMilliC = 15'000;

bool presentBackend(KoboDrmDisplay& drm, KoboFbInkDisplay& fbink, const bool useDrm, const std::uint8_t* packed,
                    const std::size_t packedSize, const RefreshKind waveform, const RefreshRegion& region) {
  return useDrm ? drm.presentPackedMono(packed, packedSize, waveform, region)
                : fbink.presentPackedMono(packed, packedSize, waveform, region);
}

int backendError(const KoboDrmDisplay& drm, const KoboFbInkDisplay& fbink, const bool useDrm) {
  return useDrm ? drm.lastError() : fbink.lastError();
}

bool thermalQualityExceeded(const int before, const int after) {
  if (before < 0 || after < 0) return false;
  return after >= kThermalStopMilliC || after - before >= kThermalRiseStopMilliC;
}

}  // namespace

KoboRefreshExecutor::KoboRefreshExecutor() { lastPresented_.fill(0xFFU); }

void KoboRefreshExecutor::reset() {
  scheduler_.reset();
  lastPresented_.fill(0xFFU);
  lastTelemetry_ = {};
  havePresented_ = false;
}

bool KoboRefreshExecutor::present(KoboDrmDisplay& drm, KoboFbInkDisplay& fbink, const bool useDrm,
                                  const std::uint8_t* const packed, const std::size_t packedSize,
                                  const RefreshKind requested, const bool soakPassed) {
  if (packed == nullptr || packedSize != kFrameBytes) return false;
  const auto dirty = KoboDirtyRegion::diff(lastPresented_.data(), packed, packedSize, KoboFbInkDisplay::kPanelWidth,
                                           KoboFbInkDisplay::kPanelHeight, havePresented_);
  const auto decision = scheduler_.schedule(requested, dirty, soakPassed);
  lastTelemetry_ = {.decision = decision};
  if (decision.skip) {
    std::fprintf(stderr, "[KOBO][EPD] profile=%s requested=%s skip=unchanged\n", refreshProfileName(decision.profile),
                 refreshKindName(requested));
    return true;
  }

  KoboCpuFreqGuard cpuBoost;
  lastTelemetry_.cpuBoosted = decision.requestCpuBoost && cpuBoost.beginPerformanceBoost();
  lastTelemetry_.temperatureBeforeMilliC = readSocTemperatureMilliC();
  const auto started = std::chrono::steady_clock::now();
  const bool success = presentBackend(drm, fbink, useDrm, packed, packedSize, decision.waveform, decision.region);
  lastTelemetry_.submitMicros =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
  lastTelemetry_.temperatureAfterMilliC = readSocTemperatureMilliC();
  lastTelemetry_.cpuFrequencyKhz = cpuBoost.currentFrequencyKhz();
  lastTelemetry_.errorNumber = success ? 0 : backendError(drm, fbink, useDrm);
  // The pinned N437 DRM UAPI acknowledges queue submission but exposes no
  // reliable userspace completion fence. Never call this page-turn latency.
  const std::int64_t deadline =
      decision.waveform == RefreshKind::Full ? kFullSubmitDeadlineMicros : kPartialSubmitDeadlineMicros;
  lastTelemetry_.deadlineExceeded = lastTelemetry_.submitMicros > deadline;
  const bool qualityExceeded =
      thermalQualityExceeded(lastTelemetry_.temperatureBeforeMilliC, lastTelemetry_.temperatureAfterMilliC);

  if (success && !lastTelemetry_.deadlineExceeded && !qualityExceeded) {
    std::copy_n(packed, kFrameBytes, lastPresented_.begin());
    havePresented_ = true;
    scheduler_.recordSuccess(decision);
  } else {
    const auto fallback =
        scheduler_.recordFailure(lastTelemetry_.errorNumber, lastTelemetry_.deadlineExceeded, qualityExceeded);
    lastTelemetry_.fallbackAttempted = fallback.requiresRecoveryFull;
    if (fallback.requiresRecoveryFull) {
      const auto recovery =
          KoboDirtyRegion::full(KoboFbInkDisplay::kPanelWidth, KoboFbInkDisplay::kPanelHeight, packedSize);
      lastTelemetry_.fallbackSucceeded =
          presentBackend(drm, fbink, useDrm, packed, packedSize, RefreshKind::Full, recovery);
      if (lastTelemetry_.fallbackSucceeded) {
        std::copy_n(packed, kFrameBytes, lastPresented_.begin());
        havePresented_ = true;
        RefreshDecision recovered{};
        recovered.waveform = RefreshKind::Full;
        recovered.region = recovery;
        scheduler_.recordSuccess(recovered);
      }
    }
  }

  std::fprintf(
      stderr,
      "[KOBO][EPD] profile=%s requested=%s waveform=%s clean=%d backend=%s rect=%ux%u+%u+%u changed_bytes=%zu "
      "submit_us=%lld completion_us=unsupported cpu_khz=%d boosted=%d temp_before_mC=%d temp_after_mC=%d "
      "result=%d errno=%d deadline=%d quality=%d fallback=%d/%d reason=%s cooldown=%u\n",
      refreshProfileName(decision.profile), refreshKindName(requested), refreshKindName(decision.waveform),
      decision.cleanRefresh ? 1 : 0, useDrm ? "drm" : "fbink", decision.region.width, decision.region.height,
      decision.region.x, decision.region.y, decision.region.changedBytes,
      static_cast<long long>(lastTelemetry_.submitMicros), lastTelemetry_.cpuFrequencyKhz,
      lastTelemetry_.cpuBoosted ? 1 : 0, lastTelemetry_.temperatureBeforeMilliC, lastTelemetry_.temperatureAfterMilliC,
      success ? 1 : 0, lastTelemetry_.errorNumber, lastTelemetry_.deadlineExceeded ? 1 : 0, qualityExceeded ? 1 : 0,
      lastTelemetry_.fallbackAttempted ? 1 : 0, lastTelemetry_.fallbackSucceeded ? 1 : 0,
      success && !lastTelemetry_.deadlineExceeded && !qualityExceeded ? "none"
                                                                      : (qualityExceeded                   ? "quality"
                                                                         : lastTelemetry_.deadlineExceeded ? "deadline"
                                                                                                           : "ioctl"),
      scheduler_.cooldownRefreshes());
  if (!success) std::fprintf(stderr, "[KOBO] display refresh failed: %d\n", lastTelemetry_.errorNumber);
  return success && !lastTelemetry_.deadlineExceeded && !qualityExceeded;
}

}  // namespace crossink::kobo
