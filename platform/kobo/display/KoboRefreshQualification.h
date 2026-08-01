// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace crossink::kobo {

// Fast is a measured scheduler profile, never a generic overclock. Its marker
// is valid only for this display-policy ABI, current kernel and N437 model.
bool koboFastRefreshQualified();

// Test/acceptance tooling calls this only after the complete physical soak has
// passed. It replaces the marker atomically; normal UI code never writes it.
bool recordKoboFastRefreshQualification();

}  // namespace crossink::kobo
