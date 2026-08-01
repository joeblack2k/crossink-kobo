// SPDX-License-Identifier: GPL-3.0-or-later
#include "KoboRefreshScheduler.h"

#include <algorithm>
#include <limits>

namespace crossink::kobo {

RefreshRegion KoboDirtyRegion::full(const std::uint16_t width, const std::uint16_t height,
                                    const std::size_t changedBytes) {
  return {.x = 0, .y = 0, .width = width, .height = height, .changedBytes = changedBytes};
}

RefreshRegion KoboDirtyRegion::diff(const std::uint8_t* const previous, const std::uint8_t* const current,
                                    const std::size_t bytes, const std::uint16_t width, const std::uint16_t height,
                                    const bool havePrevious) {
  const std::size_t rowBytes = width / 8U;
  if (previous == nullptr || current == nullptr || width == 0 || height == 0 || rowBytes == 0 ||
      bytes != rowBytes * height) {
    return full(width, height);
  }
  if (!havePrevious) return full(width, height, bytes);

  std::size_t minXByte = rowBytes;
  std::size_t maxXByte = 0;
  std::size_t minY = height;
  std::size_t maxY = 0;
  std::size_t changedBytes = 0;
  for (std::size_t y = 0; y < height; ++y) {
    const std::size_t rowOffset = y * rowBytes;
    for (std::size_t xByte = 0; xByte < rowBytes; ++xByte) {
      if (previous[rowOffset + xByte] == current[rowOffset + xByte]) continue;
      ++changedBytes;
      minXByte = std::min(minXByte, xByte);
      maxXByte = std::max(maxXByte, xByte);
      minY = std::min(minY, y);
      maxY = std::max(maxY, y);
    }
  }
  if (changedBytes == 0) return {};

  RefreshRegion region{
      .x = static_cast<std::uint16_t>(minXByte * 8U),
      .y = static_cast<std::uint16_t>(minY),
      .width = static_cast<std::uint16_t>((maxXByte - minXByte + 1U) * 8U),
      .height = static_cast<std::uint16_t>(maxY - minY + 1U),
      .changedBytes = changedBytes,
  };
  // A partial update that covers most of the panel produces more ghosts than
  // it saves. Coalesce nearby bytes by using one bound, but make large bounds
  // a deterministic full-screen refresh instead.
  const std::size_t regionPixels = static_cast<std::size_t>(region.width) * region.height;
  const std::size_t panelPixels = static_cast<std::size_t>(width) * height;
  if (regionPixels * 100U >= panelPixels * 65U) return full(width, height, changedBytes);
  return region;
}

KoboRefreshScheduler::KoboRefreshScheduler(const std::uint16_t width, const std::uint16_t height)
    : width_(width), height_(height) {}

void KoboRefreshScheduler::reset() {
  profile_ = KoboRefreshProfiles::bootProfile();
  partialSinceFull_ = 0;
  cooldownRefreshes_ = 0;
  recoveryFullPending_ = false;
  cleanRefreshPending_ = false;
}

bool KoboRefreshScheduler::select(const RefreshProfile profile, const bool soakPassed) {
  if (!KoboRefreshProfiles::maySelect(profile, soakPassed)) return false;
  if (profile != RefreshProfile::Safe && cooldownRefreshes_ != 0) return false;
  profile_ = profile;
  partialSinceFull_ = 0;
  recoveryFullPending_ = false;
  return true;
}

void KoboRefreshScheduler::requestCleanRefresh() { cleanRefreshPending_ = true; }

RefreshDecision KoboRefreshScheduler::schedule(const RefreshKind requested, RefreshRegion region,
                                               const bool soakPassed) const {
  RefreshDecision decision{};
  decision.profile = KoboRefreshProfiles::maySelect(profile_, soakPassed) ? profile_ : RefreshProfile::Safe;
  decision.requested = requested;
  decision.region = region;
  if (region.empty()) {
    decision.skip = true;
    return decision;
  }
  const auto plan = KoboRefreshProfiles::planFor(decision.profile, soakPassed);
  decision.waveform = KoboRefreshProfiles::resolve(decision.profile, requested, soakPassed);
  decision.requestCpuBoost = plan.requestCpuBoost;
  // A caller that explicitly asks for a full refresh expects a real panel
  // recovery, not a GC16 waveform restricted to a stale dirty rectangle.
  if (decision.waveform == RefreshKind::Full) {
    decision.region = KoboDirtyRegion::full(width_, height_, region.changedBytes);
  }
  if (cleanRefreshPending_) {
    decision.waveform = RefreshKind::Full;
    decision.region = KoboDirtyRegion::full(width_, height_, region.changedBytes);
    decision.forcedFull = true;
    decision.cleanRefresh = true;
  }
  if (recoveryFullPending_ || (decision.waveform != RefreshKind::Full && partialSinceFull_ >= plan.partialBudget)) {
    decision.waveform = RefreshKind::Full;
    decision.region = KoboDirtyRegion::full(width_, height_, region.changedBytes);
    decision.forcedFull = true;
  }
  return decision;
}

void KoboRefreshScheduler::recordSuccess(const RefreshDecision& decision) {
  if (decision.skip) return;
  if (decision.waveform == RefreshKind::Full) {
    partialSinceFull_ = 0;
    recoveryFullPending_ = false;
    if (decision.cleanRefresh) cleanRefreshPending_ = false;
    return;
  }
  if (cooldownRefreshes_ != 0) --cooldownRefreshes_;
  if (partialSinceFull_ < std::numeric_limits<std::uint8_t>::max()) ++partialSinceFull_;
}

RefreshFallback KoboRefreshScheduler::recordFailure(const int errorNumber, const bool deadlineExceeded,
                                                    const bool qualityExceeded) {
  RefreshFallback fallback{};
  fallback.from = profile_;
  // A failed display submission is never retried at a faster profile. The
  // immediate recovery must be the proven Safe plan plus a GC16 full refresh;
  // a persisted preference is cleared by the application loop before it can
  // re-select Fast or Max beta on the next frame.
  fallback.to = RefreshProfile::Safe;
  fallback.activated = true;
  fallback.requiresRecoveryFull = true;
  fallback.reason = qualityExceeded    ? "quality"
                    : deadlineExceeded ? "deadline"
                    : errorNumber != 0 ? "ioctl"
                                       : "unknown";
  fallback.cooldownRefreshes = 10;
  profile_ = fallback.to;
  partialSinceFull_ = 0;
  cooldownRefreshes_ = fallback.cooldownRefreshes;
  recoveryFullPending_ = true;
  return fallback;
}

}  // namespace crossink::kobo
