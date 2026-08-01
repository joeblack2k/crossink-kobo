#include "WiFi.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

constexpr char kInterface[] = "wlan0";
constexpr char kStationConfig[] = "/run/crossink-wpa.conf";
constexpr char kHostapdConfig[] = "/run/crossink-hostapd.conf";

int run(const char* command) { return std::system(command); }

std::string capture(const char* command) {
  std::string output;
  FILE* pipe = popen(command, "r");
  if (pipe == nullptr) return output;
  char buffer[512];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
  pclose(pipe);
  return output;
}

std::string escapedQuoted(const char* value) {
  std::string result;
  if (value == nullptr) return result;
  for (const unsigned char character : std::string(value)) {
    if (character == '\\' || character == '"') result.push_back('\\');
    if (character >= 0x20 && character != 0x7f) result.push_back(static_cast<char>(character));
  }
  return result;
}

bool writePrivateFile(const char* path, const std::string& contents) {
  const int descriptor = ::open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (descriptor < 0) return false;
  const bool written = ::write(descriptor, contents.data(), contents.size()) == static_cast<ssize_t>(contents.size());
  const bool synced = ::fsync(descriptor) == 0;
  ::close(descriptor);
  return written && synced;
}

IPAddress interfaceAddress(const char* interfaceName) {
  ifaddrs* addresses = nullptr;
  if (getifaddrs(&addresses) != 0) return {};
  IPAddress result;
  for (const ifaddrs* entry = addresses; entry != nullptr; entry = entry->ifa_next) {
    if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET ||
        std::strcmp(entry->ifa_name, interfaceName) != 0)
      continue;
    const auto* address = reinterpret_cast<const sockaddr_in*>(entry->ifa_addr);
    const uint32_t host = ntohl(address->sin_addr.s_addr);
    result = IPAddress(static_cast<uint8_t>(host >> 24), static_cast<uint8_t>(host >> 16),
                       static_cast<uint8_t>(host >> 8), static_cast<uint8_t>(host));
    break;
  }
  freeifaddrs(addresses);
  return result;
}

}  // namespace

String IPAddress::toString() const {
  char value[16];
  std::snprintf(value, sizeof(value), "%u.%u.%u.%u", bytes_[0], bytes_[1], bytes_[2], bytes_[3]);
  return String(value);
}

void WiFiClass::mode(const int requestedMode) {
  mode_ = static_cast<wifi_mode_t>(requestedMode);
  if (mode_ == WIFI_OFF) {
    disconnect(true);
    return;
  }
  run("ip link set wlan0 up >/dev/null 2>&1");
}

int WiFiClass::scanNetworks(bool, bool, bool, uint32_t, uint8_t) {
  networks_.clear();
  scanFailed_ = false;
  mode(WIFI_STA);
  scanPending_ = true;
  return WIFI_SCAN_RUNNING;
}

bool WiFiClass::loadScanResults() {
  // Do not hide stderr here.  The Broadcom driver reports a dead SDIO bus as
  // "command failed"; presenting that as an empty scan made Settings claim
  // there were simply no networks and encouraged repeated scans.
  const std::string output = capture("iw dev wlan0 scan 2>&1");
  scanFailed_ = output.find("command failed:") != std::string::npos ||
                output.find("Network is down") != std::string::npos ||
                output.find("Operation not permitted") != std::string::npos;
  if (scanFailed_) {
    networks_.clear();
    return false;
  }
  std::istringstream lines(output);
  std::string line;
  networks_.clear();
  Network candidate;
  bool haveCandidate = false;
  bool encrypted = false;
  const auto storeCandidate = [&]() {
    if (!haveCandidate || candidate.ssid.empty()) return;
    candidate.auth = encrypted ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    auto existing = std::find_if(networks_.begin(), networks_.end(),
                                 [&](const Network& network) { return network.ssid == candidate.ssid; });
    if (existing == networks_.end())
      networks_.push_back(candidate);
    else if (candidate.rssi > existing->rssi)
      *existing = candidate;
  };
  while (std::getline(lines, line)) {
    const std::size_t first = line.find_first_not_of(" \t");
    const std::string trimmed = first == std::string::npos ? std::string() : line.substr(first);
    if (trimmed.rfind("BSS ", 0) == 0) {
      storeCandidate();
      candidate = {};
      haveCandidate = true;
      encrypted = false;
    } else if (haveCandidate && trimmed.rfind("signal:", 0) == 0) {
      candidate.rssi = static_cast<int>(std::strtol(trimmed.c_str() + 7, nullptr, 10));
    } else if (haveCandidate && trimmed.rfind("SSID:", 0) == 0) {
      const std::size_t value = trimmed.find_first_not_of(" \t", 5);
      candidate.ssid = value == std::string::npos ? std::string() : trimmed.substr(value);
    } else if (haveCandidate && (trimmed.rfind("RSN:", 0) == 0 || trimmed.rfind("WPA:", 0) == 0)) {
      encrypted = true;
    }
  }
  storeCandidate();
  return !networks_.empty();
}

int WiFiClass::scanComplete() {
  if (!scanPending_) return static_cast<int>(networks_.size());
  loadScanResults();
  scanPending_ = false;
  if (scanFailed_) return WIFI_SCAN_FAILED;
  return static_cast<int>(networks_.size());
}

void WiFiClass::scanDelete() {
  networks_.clear();
  scanPending_ = false;
}

wl_status_t WiFiClass::begin(const char* ssid, const char* password) {
  if (ssid == nullptr || ssid[0] == '\0') return status_ = WL_NO_SSID_AVAIL;
  mode(WIFI_STA);
  currentSsid_ = ssid;
  stopDhcp();
  dhcpAttempted_ = false;
  run("killall wpa_supplicant >/dev/null 2>&1");
  run("ip addr flush dev wlan0 >/dev/null 2>&1");

  std::string config = "ctrl_interface=/run/wpa_supplicant\nupdate_config=0\nnetwork={\n  ssid=\"";
  config += escapedQuoted(ssid);
  config += "\"\n";
  if (password != nullptr && password[0] != '\0') {
    config += "  psk=\"" + escapedQuoted(password) + "\"\n  key_mgmt=WPA-PSK\n";
  } else {
    config += "  key_mgmt=NONE\n";
  }
  config += "}\n";
  if (!writePrivateFile(kStationConfig, config) ||
      run("wpa_supplicant -B -i wlan0 -c /run/crossink-wpa.conf >/dev/null 2>&1") != 0)
    return status_ = WL_CONNECT_FAILED;
  return status_ = WL_IDLE_STATUS;
}

wl_status_t WiFiClass::status() {
  if (mode_ != WIFI_STA && mode_ != WIFI_AP_STA) return status_;
  const std::string output = capture("wpa_cli -i wlan0 status 2>/dev/null");
  if (output.find("wpa_state=COMPLETED") == std::string::npos) {
    // A previous DHCP lease can survive a crashed or stopped supplicant.  It
    // must never make the UI believe that station mode is live: that caused
    // scans to preserve a dead connection and made joining a network fail.
    if (localIP() != IPAddress()) run("ip addr flush dev wlan0 >/dev/null 2>&1");
    stopDhcp();
    dhcpAttempted_ = false;
    return status_ = WL_IDLE_STATUS;
  }
  if (dhcpPid_ > 0) {
    int childStatus = 0;
    const pid_t result = ::waitpid(dhcpPid_, &childStatus, WNOHANG);
    if (result == dhcpPid_) dhcpPid_ = -1;
  }
  if (!dhcpAttempted_) {
    dhcpAttempted_ = true;
    startDhcp();
  }
  status_ = localIP() == IPAddress() ? WL_IDLE_STATUS : WL_CONNECTED;
  return status_;
}

void WiFiClass::disconnect(const bool wifiOff, bool) {
  stopDhcp();
  run("killall wpa_supplicant >/dev/null 2>&1");
  run("ip addr flush dev wlan0 >/dev/null 2>&1");
  status_ = WL_DISCONNECTED;
  currentSsid_.clear();
  dhcpAttempted_ = false;
  if (wifiOff) {
    run("ip link set wlan0 down >/dev/null 2>&1");
    mode_ = WIFI_OFF;
  }
}

IPAddress WiFiClass::localIP() const { return interfaceAddress(kInterface); }

void WiFiClass::stopDhcp() {
  if (dhcpPid_ <= 0) return;
  (void)::kill(dhcpPid_, SIGTERM);
  for (int attempt = 0; attempt < 10; ++attempt) {
    const pid_t result = ::waitpid(dhcpPid_, nullptr, WNOHANG);
    if (result == dhcpPid_ || result < 0) {
      dhcpPid_ = -1;
      return;
    }
    ::usleep(1000);
  }
  (void)::kill(dhcpPid_, SIGKILL);
  (void)::waitpid(dhcpPid_, nullptr, 0);
  dhcpPid_ = -1;
}

void WiFiClass::startDhcp() {
  if (dhcpPid_ > 0) return;
  const pid_t child = ::fork();
  if (child < 0) return;
  if (child == 0) {
    const int descriptor = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (descriptor >= 0) {
      (void)::dup2(descriptor, STDOUT_FILENO);
      (void)::dup2(descriptor, STDERR_FILENO);
      ::close(descriptor);
    }
    ::execlp("udhcpc", "udhcpc", "-i", kInterface, "-n", "-q", "-t", "4", "-T", "2", static_cast<char*>(nullptr));
    _exit(127);
  }
  dhcpPid_ = child;
}

wifi_mode_t WiFiClass::getMode() const {
  if (mode_ == WIFI_OFF && localIP() != IPAddress()) return WIFI_STA;
  return mode_;
}
String WiFiClass::SSID() const {
  if (!currentSsid_.empty()) return String(currentSsid_.c_str());
  const std::string link = capture("iw dev wlan0 link 2>/dev/null");
  const std::size_t position = link.find("SSID: ");
  if (position == std::string::npos) return String();
  const std::size_t start = position + 6;
  const std::size_t end = link.find_first_of("\r\n", start);
  return String(link.substr(start, end - start).c_str());
}
String WiFiClass::SSID(const int index) const {
  return index >= 0 && index < static_cast<int>(networks_.size()) ? String(networks_[index].ssid.c_str()) : String();
}
int WiFiClass::RSSI() const {
  const std::string output = capture("wpa_cli -i wlan0 signal_poll 2>/dev/null");
  const std::size_t position = output.find("RSSI=");
  if (position != std::string::npos) return std::atoi(output.c_str() + position + 5);
  const std::string link = capture("iw dev wlan0 link 2>/dev/null");
  const std::size_t signal = link.find("signal:");
  return signal == std::string::npos ? 0 : std::atoi(link.c_str() + signal + 7);
}
int WiFiClass::RSSI(const int index) const {
  return index >= 0 && index < static_cast<int>(networks_.size()) ? networks_[index].rssi : 0;
}
int WiFiClass::encryptionType(const int index) const {
  return index >= 0 && index < static_cast<int>(networks_.size()) ? networks_[index].auth : WIFI_AUTH_OPEN;
}

uint8_t* WiFiClass::macAddress(uint8_t* destination) const {
  if (destination == nullptr) return nullptr;
  unsigned int values[6]{};
  const std::string value = capture("cat /sys/class/net/wlan0/address 2>/dev/null");
  if (std::sscanf(value.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4],
                  &values[5]) != 6)
    return destination;
  for (int i = 0; i < 6; ++i) destination[i] = static_cast<uint8_t>(values[i]);
  return destination;
}
String WiFiClass::macAddress() const {
  const std::string value = capture("cat /sys/class/net/wlan0/address 2>/dev/null");
  return String(value.empty() ? "00:00:00:00:00:00" : value.substr(0, value.find_first_of("\r\n")).c_str());
}
void WiFiClass::setHostname(const char* hostname) {
  if (hostname != nullptr && hostname[0] != '\0') hostname_ = hostname;
}
String WiFiClass::getHostname() const { return String(hostname_.c_str()); }
void WiFiClass::setSleep(const bool enabled) {
  run(enabled ? "iw dev wlan0 set power_save on >/dev/null 2>&1" : "iw dev wlan0 set power_save off >/dev/null 2>&1");
}

bool WiFiClass::softAP(const char* ssid, const char* password, const int channel, const int hidden, int) {
  if (ssid == nullptr || ssid[0] == '\0') return false;
  disconnect(false);
  mode_ = WIFI_AP;
  currentSsid_ = ssid;
  std::string config = "interface=wlan0\ndriver=nl80211\nssid=" + escapedQuoted(ssid) +
                       "\nchannel=" + std::to_string(channel) +
                       "\nhw_mode=g\nignore_broadcast_ssid=" + std::to_string(hidden ? 1 : 0) + "\n";
  if (password != nullptr && std::strlen(password) >= 8) {
    config += "wpa=2\nwpa_key_mgmt=WPA-PSK\nrsn_pairwise=CCMP\nwpa_passphrase=" + escapedQuoted(password) + "\n";
  }
  if (!writePrivateFile(kHostapdConfig, config)) return false;
  run("ip addr flush dev wlan0 >/dev/null 2>&1");
  run("ip addr add 192.168.4.1/24 dev wlan0 >/dev/null 2>&1");
  if (run("hostapd -B /run/crossink-hostapd.conf >/dev/null 2>&1") != 0) return false;
  if (run("dnsmasq --interface=wlan0 --bind-interfaces --except-interface=lo --dhcp-range=192.168.4.20,192.168.4.80,"
          "255.255.255.0,12h --pid-file=/run/crossink-dnsmasq.pid >/dev/null 2>&1") != 0) {
    run("killall hostapd >/dev/null 2>&1");
    return false;
  }
  status_ = WL_CONNECTED;
  return true;
}
bool WiFiClass::softAPdisconnect(const bool wifiOff) {
  run("killall dnsmasq >/dev/null 2>&1");
  run("killall hostapd >/dev/null 2>&1");
  run("ip addr flush dev wlan0 >/dev/null 2>&1");
  status_ = WL_DISCONNECTED;
  if (wifiOff) mode(WIFI_OFF);
  return true;
}
IPAddress WiFiClass::softAPIP() const { return mode_ == WIFI_AP ? IPAddress(192, 168, 4, 1) : IPAddress(); }
int WiFiClass::softAPgetStationNum() const {
  const std::string output = capture("iw dev wlan0 station dump 2>/dev/null | grep -c '^Station '");
  return std::atoi(output.c_str());
}
