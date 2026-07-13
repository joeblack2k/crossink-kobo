// SPDX-License-Identifier: GPL-3.0-or-later
// Policy-only E-ink refresh scheduler for Kobo N437.  This intentionally has
// no DRM or FBInk dependencies so its quality/fallback behaviour is testable
// on a host and cannot change panel voltage, clocks, or waveform blobs.
#pragma once

#include <cstdint>

#include "KoboDisplayTypes.h"
#include "KoboRefreshProfile.h"

namespace crossink::kobo {

class KoboDirtyRegion final {
 public:
  static RefreshRegion diff(const std::uint8_t* previous, const std::uint8_t* current, std::size_t bytes,
                            std::uint16_t width, std::uint16_t height, bool havePrevious);
  static RefreshRegion full(std::uint16_t width, std::uint16_t height, std::size_t changedBytes = 0);
};

struct RefreshDecision {
  RefreshProfile profile = RefreshProfile::Safe;
  RefreshKind requested = RefreshKind::Partial;
  RefreshKind waveform = RefreshKind::Partial;
  RefreshRegion region{};
  bool skip = false;
  bool forcedFull = false;
  // A full clean-up requested by a screen transition. This is deliberately
  // distinct from the periodic partial-refresh budget: it lets an activity
  // remove visible ghosting without promoting ordinary reader page turns.
  bool cleanRefresh = false;
  bool requestCpuBoost = false;
};

struct RefreshFallback {
  bool activated = false;
  RefreshProfile from = RefreshProfile::Safe;
  RefreshProfile to = RefreshProfile::Safe;
  bool requiresRecoveryFull = false;
  const char* reason = "none";
  std::uint8_t cooldownRefreshes = 0;
};

// Completion timing is intentionally -1 when the legacy N437 DRM UAPI offers
// only a submit acknowledgement. Callers must not label submit time as panel
// page-turn latency.
struct DisplayTelemetry {
  RefreshDecision decision{};
  std::int64_t submitMicros = -1;
  std::int64_t completionMicros = -1;
  int temperatureBeforeMilliC = -1;
  int temperatureAfterMilliC = -1;
  int cpuFrequencyKhz = -1;
  int errorNumber = 0;
  bool cpuBoosted = false;
  bool deadlineExceeded = false;
  bool fallbackAttempted = false;
  bool fallbackSucceeded = false;
};

class KoboRefreshScheduler final {
 public:
  KoboRefreshScheduler(std::uint16_t width, std::uint16_t height);

  [[nodiscard]] RefreshProfile profile() const { return profile_; }
  [[nodiscard]] std::uint8_t partialSinceFull() const { return partialSinceFull_; }
  [[nodiscard]] std::uint8_t cooldownRefreshes() const { return cooldownRefreshes_; }
  [[nodiscard]] bool recoveryFullPending() const { return recoveryFullPending_; }
  [[nodiscard]] bool cleanRefreshPending() const { return cleanRefreshPending_; }

  // Fast/MaxBeta are rejected unless the exact device/profile has an already
  // recorded soak PASS.  Every process start begins at Safe.
  bool select(RefreshProfile profile, bool soakPassed);
  // Request exactly one successful full-panel GC16 clean-up. The request is
  // retained through a failed present and consumed only after a successful
  // full refresh.
  void requestCleanRefresh();
  RefreshDecision schedule(RefreshKind requested, RefreshRegion region, bool soakPassed) const;
  void recordSuccess(const RefreshDecision& decision);
  RefreshFallback recordFailure(int errorNumber, bool deadlineExceeded, bool qualityExceeded);
  void reset();

 private:
  std::uint16_t width_;
  std::uint16_t height_;
  RefreshProfile profile_ = RefreshProfile::Safe;
  std::uint8_t partialSinceFull_ = 0;
  std::uint8_t cooldownRefreshes_ = 0;
  bool recoveryFullPending_ = false;
  bool cleanRefreshPending_ = false;
};

}  // namespace crossink::kobo
