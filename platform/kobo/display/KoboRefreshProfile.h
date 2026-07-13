// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

#include "KoboFbInkDisplay.h"

namespace crossink::kobo {

// User-visible names are deliberately separate from the underlying waveform
// choices. The latter are hardware capabilities and must never be inferred
// from a profile name alone.
enum class RefreshProfile : std::uint8_t {
  Safe = 0,
  Fast = 1,
  MaxBeta = 2,
};

struct RefreshProfilePlan {
  RefreshKind fast = RefreshKind::Fast;
  RefreshKind partial = RefreshKind::Partial;
  RefreshKind full = RefreshKind::Full;
  std::uint8_t partialBudget = 5;
  bool requestCpuBoost = false;
  bool allowsExperimentalWaveforms = false;
};

class KoboRefreshProfiles final {
 public:
  // Safe is the only boot profile. Fast and MaxBeta are intentionally
  // unavailable until a device-specific soak result is recorded.
  static RefreshProfile bootProfile();
  static bool maySelect(RefreshProfile profile, bool soakPassed);
  static RefreshProfilePlan planFor(RefreshProfile profile, bool soakPassed);
  static RefreshKind resolve(RefreshProfile profile, RefreshKind requested, bool soakPassed);
};

const char* refreshProfileName(RefreshProfile profile);
const char* refreshKindName(RefreshKind kind);

}  // namespace crossink::kobo
