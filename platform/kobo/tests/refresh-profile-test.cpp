#include <cstdlib>
#include <iostream>

#include "KoboRefreshProfile.h"

using crossink::kobo::KoboRefreshProfiles;
using crossink::kobo::RefreshKind;
using crossink::kobo::RefreshProfile;

[[noreturn]] void fail(const char* message) {
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

int main() {
  if (KoboRefreshProfiles::bootProfile() != RefreshProfile::Safe) fail("safe must be the boot profile");
  if (KoboRefreshProfiles::maySelect(RefreshProfile::Fast, false) ||
      KoboRefreshProfiles::maySelect(RefreshProfile::MaxBeta, false)) {
    fail("unsoaked profiles must be rejected");
  }
  const auto safe = KoboRefreshProfiles::planFor(RefreshProfile::Fast, false);
  if (safe.requestCpuBoost || safe.partialBudget != 5 || safe.fast != RefreshKind::Fast) {
    fail("unsoaked fast profile must resolve to safe");
  }
  const auto fast = KoboRefreshProfiles::planFor(RefreshProfile::Fast, true);
  if (!fast.requestCpuBoost || fast.allowsExperimentalWaveforms || fast.partialBudget != 8) {
    fail("soaked fast profile mapping failed");
  }
  const auto max = KoboRefreshProfiles::planFor(RefreshProfile::MaxBeta, true);
  if (!max.requestCpuBoost || !max.allowsExperimentalWaveforms || max.partialBudget != 10) {
    fail("soaked max profile mapping failed");
  }
  return EXIT_SUCCESS;
}
