// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "KoboFbInkDisplay.h"
#include "KoboRefreshScheduler.h"

namespace crossink::kobo {

class KoboDrmDisplay;

// Owns all policy and measurement around one submitted frame.  HalDisplay
// remains a framebuffer adapter; it must not decide waveforms, governors,
// refresh budgets or recovery behaviour.
class KoboRefreshExecutor final {
 public:
  KoboRefreshExecutor();

  void reset();
  bool present(KoboDrmDisplay& drm, KoboFbInkDisplay& fbink, bool useDrm, const std::uint8_t* packed,
               std::size_t packedSize, RefreshKind requested, bool soakPassed);

  [[nodiscard]] RefreshProfile profile() const { return scheduler_.profile(); }
  [[nodiscard]] const DisplayTelemetry& lastTelemetry() const { return lastTelemetry_; }
  bool selectProfile(RefreshProfile profile, bool soakPassed) { return scheduler_.select(profile, soakPassed); }
  void requestCleanRefresh() { scheduler_.requestCleanRefresh(); }

 private:
  static constexpr std::size_t kFrameBytes = KoboFbInkDisplay::kPackedFrameBytes;

  std::array<std::uint8_t, kFrameBytes> lastPresented_{};
  KoboRefreshScheduler scheduler_{KoboFbInkDisplay::kPanelWidth, KoboFbInkDisplay::kPanelHeight};
  DisplayTelemetry lastTelemetry_{};
  bool havePresented_ = false;
};

}  // namespace crossink::kobo
