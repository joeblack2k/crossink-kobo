// SPDX-License-Identifier: GPL-3.0-or-later
#include "KoboRefreshProfile.h"

namespace crossink::kobo {
namespace {

constexpr RefreshProfilePlan kSafePlan{
    .fast = RefreshKind::Fast,
    .partial = RefreshKind::Partial,
    .full = RefreshKind::Full,
    .partialBudget = 5,
    .requestCpuBoost = false,
    .allowsExperimentalWaveforms = false,
};

constexpr RefreshProfilePlan kFastPlan{
    .fast = RefreshKind::Fast,
    .partial = RefreshKind::Partial,
    .full = RefreshKind::Full,
    .partialBudget = 8,
    .requestCpuBoost = true,
    .allowsExperimentalWaveforms = false,
};

constexpr RefreshProfilePlan kMaxBetaPlan{
    .fast = RefreshKind::Fast,
    .partial = RefreshKind::Partial,
    .full = RefreshKind::Full,
    .partialBudget = 10,
    .requestCpuBoost = true,
    .allowsExperimentalWaveforms = true,
};

}  // namespace

RefreshProfile KoboRefreshProfiles::bootProfile() { return RefreshProfile::Safe; }

bool KoboRefreshProfiles::maySelect(const RefreshProfile profile, const bool soakPassed) {
  return profile == RefreshProfile::Safe || soakPassed;
}

RefreshProfilePlan KoboRefreshProfiles::planFor(const RefreshProfile profile, const bool soakPassed) {
  if (!maySelect(profile, soakPassed)) return kSafePlan;
  switch (profile) {
    case RefreshProfile::Fast:
      return kFastPlan;
    case RefreshProfile::MaxBeta:
      return kMaxBetaPlan;
    case RefreshProfile::Safe:
    default:
      return kSafePlan;
  }
}

RefreshKind KoboRefreshProfiles::resolve(const RefreshProfile profile, const RefreshKind requested,
                                         const bool soakPassed) {
  const RefreshProfilePlan plan = planFor(profile, soakPassed);
  switch (requested) {
    case RefreshKind::Fast:
      return plan.fast;
    case RefreshKind::Partial:
      return plan.partial;
    case RefreshKind::Full:
    default:
      return plan.full;
  }
}

const char* refreshProfileName(const RefreshProfile profile) {
  switch (profile) {
    case RefreshProfile::Fast:
      return "fast";
    case RefreshProfile::MaxBeta:
      return "max-beta";
    case RefreshProfile::Safe:
    default:
      return "safe";
  }
}

const char* refreshKindName(const RefreshKind kind) {
  switch (kind) {
    case RefreshKind::Fast:
      return "DU";
    case RefreshKind::Partial:
      return "AUTO";
    case RefreshKind::Full:
    default:
      return "GC16";
  }
}

}  // namespace crossink::kobo
