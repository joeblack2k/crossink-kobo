#include <WiFi.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

WiFiClass WiFi;

int main(const int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "Usage: wifi-smoke {status|scan}\n");
    return 2;
  }
  if (std::strcmp(argv[1], "status") == 0) {
    const IPAddress address = WiFi.localIP();
    std::printf("status=%d mode=%d ssid=%s ip=%s rssi=%d\n", static_cast<int>(WiFi.status()),
                static_cast<int>(WiFi.getMode()), WiFi.SSID().c_str(), address.toString().c_str(), WiFi.RSSI());
    return 0;
  }
  if (std::strcmp(argv[1], "scan") != 0) return 2;

  const int start = WiFi.scanNetworks(true);
  if (start == WIFI_SCAN_FAILED) return 1;
  int count = WIFI_SCAN_RUNNING;
  for (int attempt = 0; attempt < 30 && count == WIFI_SCAN_RUNNING; ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    count = WiFi.scanComplete();
  }
  if (count < 0) return 1;
  std::printf("networks=%d\n", count);
  for (int index = 0; index < count; ++index) {
    std::printf("network=%s rssi=%d auth=%d\n", WiFi.SSID(index).c_str(), WiFi.RSSI(index), WiFi.encryptionType(index));
  }
  return 0;
}
