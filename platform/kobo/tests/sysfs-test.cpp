#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "KoboSysfs.h"

namespace fs = std::filesystem;
using crossink::kobo::BatterySnapshot;
using crossink::kobo::BatteryState;
using crossink::kobo::KoboBatterySysfs;
using crossink::kobo::KoboFrontlightSysfs;

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
  const fs::path root = fs::temp_directory_path() / ("crossink-kobo-sysfs-" + std::to_string(::getpid()));
  fs::remove_all(root);
  put(root / "power/BAT0/type", "Battery\n");
  put(root / "power/BAT0/capacity", "73\n");
  put(root / "power/BAT0/status", "Charging\n");
  put(root / "power/usb/type", "USB\n");
  put(root / "power/usb/online", "1\n");
  put(root / "backlight/frontlight/max_brightness", "255\n");
  put(root / "backlight/frontlight/brightness", "0\n");

  KoboBatterySysfs battery;
  BatterySnapshot snapshot;
  if (!battery.discover((root / "power").string()) || !battery.read(snapshot) || snapshot.percentage != 73 ||
      snapshot.state != BatteryState::Charging || !snapshot.usbOnline) {
    fail("battery discovery/read failed");
  }

  KoboFrontlightSysfs frontlight;
  if (!frontlight.discover((root / "backlight").string()) || frontlight.maximum() != 255 ||
      !frontlight.setPercentage(50)) {
    fail("frontlight discovery/write failed");
  }
  std::ifstream brightness(root / "backlight/frontlight/brightness");
  int raw = -1;
  brightness >> raw;
  if (raw != 128 || frontlight.percentage() != 50) {
    fail("frontlight percentage mapping failed");
  }

  fs::remove_all(root);
  return EXIT_SUCCESS;
}
