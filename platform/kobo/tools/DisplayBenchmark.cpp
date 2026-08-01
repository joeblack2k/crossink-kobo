// SPDX-License-Identifier: GPL-3.0-or-later
#include "DisplayBenchmark.h"

#include <algorithm>

namespace crossink::kobo {
namespace {

std::size_t nearestRankIndex(const std::size_t count, const std::size_t percent) {
  // The caller guarantees count != 0.  Use nearest-rank so 100 samples map
  // p50/p95 to samples 50/95, which is easy to audit in the host test.
  return ((count * percent + 99U) / 100U) - 1U;
}

}  // namespace

DisplaySubmissionSummary summarizeDisplaySubmissionMicros(std::uint32_t* const samples, const std::size_t count) {
  DisplaySubmissionSummary summary{};
  if (samples == nullptr || count == 0) return summary;

  std::uint64_t total = 0;
  for (std::size_t index = 0; index < count; ++index) total += samples[index];
  std::sort(samples, samples + count);
  summary.totalMicros = total;
  summary.minimumMicros = samples[0];
  summary.p50Micros = samples[nearestRankIndex(count, 50U)];
  summary.p95Micros = samples[nearestRankIndex(count, 95U)];
  summary.maximumMicros = samples[count - 1U];
  summary.count = count;
  return summary;
}

}  // namespace crossink::kobo
