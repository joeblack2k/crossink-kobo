#include <cstdlib>
#include <iostream>

#include "DisplayBenchmark.h"

namespace {

[[noreturn]] void fail(const char* message) {
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main() {
  std::uint32_t samples[]{100U, 10U, 80U, 20U, 90U, 30U, 70U, 40U, 60U, 50U};
  const auto summary = crossink::kobo::summarizeDisplaySubmissionMicros(samples, sizeof(samples) / sizeof(samples[0]));
  if (summary.count != 10U || summary.totalMicros != 550U || summary.minimumMicros != 10U ||
      summary.p50Micros != 50U || summary.p95Micros != 100U || summary.maximumMicros != 100U) {
    fail("unexpected benchmark summary");
  }
  if (crossink::kobo::summarizeDisplaySubmissionMicros(nullptr, 0U).count != 0U) fail("empty benchmark must be zero");
  return EXIT_SUCCESS;
}
