// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "KoboRefreshProfile.h"

namespace crossink::kobo {

// Refresh-profile markers are valid only for this display-policy ABI, current
// kernel, binary and N437 model. They never authorize an overclock or a panel
// waveform not exposed by the active driver.
bool koboRefreshProfileQualified(RefreshProfile profile);
bool recordKoboRefreshProfileQualification(RefreshProfile profile);

// Compatibility helpers keep settings and tooling explicit at their call
// sites. Fast and Max beta deliberately use different marker files.
bool koboFastRefreshQualified();
bool koboMaxBetaRefreshQualified();

// Test/acceptance tooling calls these only after the complete physical soak
// passes. Normal UI code never writes a marker.
bool recordKoboFastRefreshQualification();
bool recordKoboMaxBetaRefreshQualification();

}  // namespace crossink::kobo
