#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "KoboCpuFreq.h"

namespace fs = std::filesystem;
using crossink::kobo::KoboCpuFreqGuard;

namespace {

void put(const fs::path& path, const char* value) {
  fs::create_directories(path.parent_path());
  std::ofstream stream(path);
  stream << value;
}

[[noreturn]] void fail(const char* message) {
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main() {
  const fs::path root = fs::temp_directory_path() / ("crossink-kobo-cpufreq-" + std::to_string(::getpid()));
  fs::remove_all(root);
  put(root / "scaling_available_frequencies", "396000 792000 996000\n");
  put(root / "scaling_available_governors", "ondemand userspace performance\n");
  put(root / "scaling_governor", "ondemand\n");
  put(root / "scaling_cur_freq", "792000\n");
  put(root / "cpuinfo_max_freq", "996000\n");

  {
    KoboCpuFreqGuard guard(root.string());
    if (!guard.beginPerformanceBoost() || !guard.active() || guard.currentFrequencyKhz() != 792000 ||
        guard.maximumFrequencyKhz() != 996000 || guard.currentGovernor() != "performance") {
      fail("valid cpufreq boost failed");
    }
    std::ifstream governor(root / "scaling_governor");
    std::string value;
    governor >> value;
    if (value != "performance") fail("performance governor was not selected");
    if (!guard.endPerformanceBoost() || guard.active()) fail("explicit governor restore failed");
    if (guard.currentGovernor() != "ondemand") fail("governor query did not report restored policy");
    std::ifstream explicitlyRestored(root / "scaling_governor");
    explicitlyRestored >> value;
    if (value != "ondemand") fail("explicit governor restore did not restore ondemand");
  }
  std::ifstream restored(root / "scaling_governor");
  std::string restoredValue;
  restored >> restoredValue;
  if (restoredValue != "ondemand") fail("governor was not restored");

  put(root / "scaling_available_frequencies", "396000 792000\n");
  KoboCpuFreqGuard invalid(root.string());
  if (invalid.beginPerformanceBoost()) fail("invalid maximum OPP was accepted");

  fs::remove_all(root);
  return EXIT_SUCCESS;
}
