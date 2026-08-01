#pragma once

#include <cstdint>
#include <string>

namespace crossink::kobo {

struct KoboSuspendProbe {
  bool memSupported = false;
  bool wakeupCountSupported = false;
  std::string states;
  std::string memSleep;
  std::string bootId;
  std::string uptime;
};

struct KoboSuspendRequest { std::string eventContext; };

struct KoboSuspendResult {
  bool entered = false;
  bool usedWakeupCount = false;
  bool wakeupCountRace = false;
  int errorNumber = 0;
  std::uint64_t elapsedMilliseconds = 0;
  std::string detail;
};

// Kernel-facing Kobo suspend boundary. It has no UI dependencies so that a
// wake/race never needs to be inferred from a redraw or app restart.
class KoboSuspendController {
 public:
  static KoboSuspendProbe probe();
  static KoboSuspendResult suspendToRam(const KoboSuspendRequest& request = {});
  static void recordEvent(const char* state, const std::string& detail = {});
};

}  // namespace crossink::kobo
