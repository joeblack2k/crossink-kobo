#include "DeviceInfoActivity.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>

#include <cstdio>
#include <string>

#include "AppVersion.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/CompactHeader.h"
#include "components/UITheme.h"
#include "fontIds.h"

#ifdef KOBO_LINUX
#include <WiFi.h>

#include <sys/statvfs.h>
#include <sys/utsname.h>
#endif

namespace {
std::string bytesToGiB(const unsigned long long bytes) {
  char text[32];
  std::snprintf(text, sizeof(text), "%.1f GiB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
  return text;
}
}  // namespace

void DeviceInfoActivity::onEnter() {
  Activity::onEnter();
  refreshLines();
  requestUpdate();
}

void DeviceInfoActivity::refreshLines() {
  lines.clear();
  lines.emplace_back("Model: Kobo Glo HD (N437 / Alyssum)");
  lines.emplace_back(std::string("Software: ") + CROSSINK_VERSION);
  lines.emplace_back("Display: 1072 x 1448  |  300 ppi");
#ifdef KOBO_LINUX
  lines.emplace_back("Frontlight: " + std::to_string(SETTINGS.frontlightBrightness) + "%");
#endif

  std::string battery = "Battery: " + std::to_string(powerManager.getBatteryPercentage()) + "%";
  if (gpio.isUsbConnected()) battery += " (charging)";
  lines.emplace_back(std::move(battery));

#ifdef KOBO_LINUX
  utsname kernel{};
  if (uname(&kernel) == 0) lines.emplace_back(std::string("Kernel: ") + kernel.release);

  struct statvfs storage {};
  if (statvfs("/data", &storage) == 0) {
    const unsigned long long total = static_cast<unsigned long long>(storage.f_blocks) * storage.f_frsize;
    const unsigned long long free = static_cast<unsigned long long>(storage.f_bavail) * storage.f_frsize;
    lines.emplace_back("Storage: " + bytesToGiB(free) + " free / " + bytesToGiB(total));
  }

  if (WiFi.status() == WL_CONNECTED) {
    lines.emplace_back(std::string("Wi-Fi: ") + WiFi.SSID().c_str());
    lines.emplace_back(std::string("IPv4: ") + WiFi.localIP().toString().c_str() + "  |  " +
                       std::to_string(WiFi.RSSI()) + " dBm");
  } else {
    lines.emplace_back("Wi-Fi: not connected");
  }
#endif
}

void DeviceInfoActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) finishAfterBackPress();
}

void DeviceInfoActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  CompactHeader::drawTitle(renderer, "Kobo Glo HD");

  const int x = metrics.contentSidePadding;
  const int width = renderer.getScreenWidth() - x * 2;
  int y = CompactHeader::contentTop(metrics) + metrics.verticalSpacing;
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  for (const auto& line : lines) {
    for (const auto& wrapped : renderer.wrappedText(UI_10_FONT_ID, line.c_str(), width, 2)) {
      renderer.drawText(UI_10_FONT_ID, x, y, wrapped.c_str());
      y += lineHeight;
    }
    y += metrics.verticalSpacing;
  }

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
