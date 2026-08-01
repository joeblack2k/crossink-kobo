// SPDX-License-Identifier: GPL-3.0-or-later
#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "KoboRefreshScheduler.h"

using crossink::kobo::KoboDirtyRegion;
using crossink::kobo::KoboRefreshScheduler;
using crossink::kobo::RefreshKind;
using crossink::kobo::RefreshProfile;

namespace {
[[noreturn]] void fail(const char* const message) {
  std::cerr << "refresh scheduler test failed: " << message << '\n';
  std::exit(1);
}
}  // namespace

int main() {
  constexpr std::uint16_t width = 16;
  constexpr std::uint16_t height = 8;
  std::array<std::uint8_t, 16> previous{};
  std::array<std::uint8_t, 16> current{};
  current[4] = 0x80;
  const auto single = KoboDirtyRegion::diff(previous.data(), current.data(), current.size(), width, height, true);
  if (single.x != 0 || single.y != 2 || single.width != 8 || single.height != 1 || single.changedBytes != 1) {
    fail("single-byte dirty region");
  }
  const auto unchanged = KoboDirtyRegion::diff(current.data(), current.data(), current.size(), width, height, true);
  if (!unchanged.empty()) fail("unchanged frame must skip");
  const auto initial = KoboDirtyRegion::diff(nullptr, current.data(), current.size(), width, height, false);
  if (initial.width != width || initial.height != height) fail("first frame must be full");

  KoboRefreshScheduler scheduler(width, height);
  if (scheduler.select(RefreshProfile::Fast, false)) fail("fast needs soak pass");
  for (int update = 0; update < 5; ++update) {
    const auto decision = scheduler.schedule(RefreshKind::Partial, single, false);
    if (decision.waveform != RefreshKind::Partial || decision.forcedFull) fail("safe partial budget early full");
    scheduler.recordSuccess(decision);
  }
  const auto budgeted = scheduler.schedule(RefreshKind::Partial, single, false);
  if (budgeted.waveform != RefreshKind::Full || !budgeted.forcedFull || budgeted.region.width != width) {
    fail("safe partial budget recovery full");
  }
  scheduler.recordSuccess(budgeted);
  if (scheduler.partialSinceFull() != 0) fail("full must reset budget");

  const auto explicitFull = scheduler.schedule(RefreshKind::Full, single, false);
  if (explicitFull.waveform != RefreshKind::Full || explicitFull.region.width != width ||
      explicitFull.region.height != height) {
    fail("explicit full must cover the panel");
  }

  scheduler.requestCleanRefresh();
  const auto cleanEntry = scheduler.schedule(RefreshKind::Fast, single, false);
  if (cleanEntry.waveform != RefreshKind::Full || !cleanEntry.forcedFull || !cleanEntry.cleanRefresh ||
      cleanEntry.region.width != width || !scheduler.cleanRefreshPending()) {
    fail("screen-entry cleanup must force a full panel refresh");
  }
  scheduler.recordSuccess(cleanEntry);
  if (scheduler.cleanRefreshPending()) fail("successful screen-entry cleanup must be consumed");
  const auto normalAfterClean = scheduler.schedule(RefreshKind::Fast, single, false);
  if (normalAfterClean.cleanRefresh || normalAfterClean.waveform == RefreshKind::Full) {
    fail("screen-entry cleanup must not slow the next ordinary refresh");
  }

  scheduler.requestCleanRefresh();
  const auto failedClean = scheduler.schedule(RefreshKind::Fast, single, false);
  scheduler.recordFailure(5, false, false);
  if (!failedClean.cleanRefresh || !scheduler.cleanRefreshPending()) {
    fail("failed screen-entry cleanup must remain pending");
  }
  const auto retriedClean = scheduler.schedule(RefreshKind::Fast, single, false);
  if (retriedClean.waveform != RefreshKind::Full || !retriedClean.cleanRefresh) {
    fail("failed screen-entry cleanup must retry as full");
  }
  scheduler.recordSuccess(retriedClean);

  scheduler.reset();
  if (!scheduler.select(RefreshProfile::Fast, true)) fail("fast soak pass selection");
  const auto fallback = scheduler.recordFailure(5, false, false);
  if (!fallback.requiresRecoveryFull || scheduler.profile() != RefreshProfile::Safe) fail("failure fallback");
  const auto recovery = scheduler.schedule(RefreshKind::Fast, single, false);
  if (recovery.waveform != RefreshKind::Full || !recovery.forcedFull) fail("failure recovery full");
  if (scheduler.cooldownRefreshes() != 10) fail("failure must start cooldown");

  KoboRefreshScheduler maxScheduler(width, height);
  if (!maxScheduler.select(RefreshProfile::MaxBeta, true)) fail("max soak pass selection");
  const auto maxFallback = maxScheduler.recordFailure(0, true, false);
  if (maxFallback.to != RefreshProfile::Safe || maxScheduler.profile() != RefreshProfile::Safe ||
      maxFallback.reason != std::string_view("deadline")) {
    fail("max must degrade to safe with reason");
  }
  return 0;
}
