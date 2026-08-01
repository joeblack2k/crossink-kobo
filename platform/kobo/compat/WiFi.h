#pragma once

#include <NetworkClient.h>
#include <WString.h>
#include <sys/types.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using WiFiClient = NetworkClient;

enum wl_status_t {
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL = 1,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_DISCONNECTED = 6
};

enum wifi_mode_t { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3, WIFI_MODE_NULL = 0 };
enum wifi_auth_mode_t { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3 };

#define WIFI_MODE_STA WIFI_STA
#define WIFI_MODE_AP WIFI_AP
#define WIFI_SCAN_RUNNING -1
#define WIFI_SCAN_FAILED -2

class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes_{a, b, c, d} {}
  [[nodiscard]] String toString() const;
  [[nodiscard]] uint8_t operator[](int index) const { return bytes_[index & 3]; }
  uint8_t& operator[](int index) { return bytes_[index & 3]; }
  [[nodiscard]] bool operator==(const IPAddress& other) const { return bytes_ == other.bytes_; }
  [[nodiscard]] bool operator!=(const IPAddress& other) const { return !(*this == other); }

 private:
  std::array<uint8_t, 4> bytes_{};
};

class WiFiClass {
 public:
  wl_status_t begin(const char* ssid = nullptr, const char* password = nullptr);
  wl_status_t status();
  IPAddress localIP() const;
  void persistent(bool) {}
  void disconnect(bool wifiOff = false, bool eraseAccessPoint = false);
  void mode(int mode);
  wifi_mode_t getMode() const;

  int scanNetworks(bool async = false, bool showHidden = false, bool passive = false,
                   uint32_t maxMillisecondsPerChannel = 300, uint8_t channel = 0);
  int scanComplete();
  void scanDelete();
  String SSID() const;
  String SSID(int index) const;
  int RSSI() const;
  int RSSI(int index) const;
  int encryptionType(int index) const;

  String macAddress() const;
  uint8_t* macAddress(uint8_t* destination) const;
  void setHostname(const char* hostname);
  String getHostname() const;
  void setSleep(bool enabled);
  void setAutoReconnect(bool enabled) { autoReconnect_ = enabled; }

  bool softAP(const char* ssid, const char* password = nullptr, int channel = 1, int hidden = 0, int maxConnection = 4);
  bool softAPdisconnect(bool wifiOff = false);
  IPAddress softAPIP() const;
  int softAPgetStationNum() const;

 private:
  struct Network {
    std::string ssid;
    int rssi = 0;
    int auth = WIFI_AUTH_OPEN;
  };

  wifi_mode_t mode_ = WIFI_OFF;
  wl_status_t status_ = WL_DISCONNECTED;
  std::string currentSsid_;
  std::string hostname_ = "crossink-n437";
  std::vector<Network> networks_;
  bool scanPending_ = false;
  // Keep an unsuccessful driver scan distinct from a successful scan with no
  // visible APs.  The activity can then leave the previous radio state alone
  // rather than treating an I/O failure as an empty neighbourhood.
  bool scanFailed_ = false;
  pid_t scanPid_ = -1;
  bool dhcpAttempted_ = false;
  pid_t dhcpPid_ = -1;
  bool autoReconnect_ = false;

  bool loadScanResults();
  void stopScan();
  void startScan();
  void stopDhcp();
  void startDhcp();
};

extern WiFiClass WiFi;
