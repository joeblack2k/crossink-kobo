// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

namespace crossink::kobo {

// Submission timing is deliberately kept separate from panel-settle timing:
// DRM's dirty-FB call measures queueing latency, not the physical waveform.
// The caller owns the fixed-size sample buffer so the device tool has a hard,
// visible memory bound during a 1000-update qualification run.
struct DisplaySubmissionSummary {
  std::uint64_t totalMicros = 0;
  std::uint32_t minimumMicros = 0;
  std::uint32_t p50Micros = 0;
  std::uint32_t p95Micros = 0;
  std::uint32_t maximumMicros = 0;
  std::size_t count = 0;
};

// Sorts `samples` in place and reports nearest-rank percentiles. Empty input
// yields the all-zero summary, which is never a valid qualification result.
DisplaySubmissionSummary summarizeDisplaySubmissionMicros(std::uint32_t* samples, std::size_t count);

}  // namespace crossink::kobo
