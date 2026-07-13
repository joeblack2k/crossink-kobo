#include "KoboWifiAutoConnect.h"

#include <Arduino.h>
#include <Logging.h>
#include <WiFi.h>
#include <WifiCredentialStore.h>

#include <algorithm>
#include <string>

namespace crossink::kobo {
namespace {
constexpr unsigned long kInitialRetryMs = 15'000;
constexpr unsigned long kMaximumRetryMs = 5UL * 60UL * 1000UL;

bool initialized = false;
bool wasConnected = false;
bool suspended = false;
unsigned long nextAttemptAt = 0;
unsigned long retryDelayMs = kInitialRetryMs;
std::string configuredSsid;

void startSavedNetwork(const WifiCredential& credential) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  const wl_status_t result = credential.password.empty() ? WiFi.begin(credential.ssid.c_str())
                                                           : WiFi.begin(credential.ssid.c_str(), credential.password.c_str());
  configuredSsid = credential.ssid;
  nextAttemptAt = millis() + retryDelayMs;
  LOG_INF("WIFI", "Kobo saved-network connect started: ssid=%s result=%d retry=%lums", configuredSsid.c_str(),
          static_cast<int>(result), retryDelayMs);
  retryDelayMs = std::min(kMaximumRetryMs, retryDelayMs * 2UL);
}
}  // namespace

void initializeWifiAutoConnect() {
  if (initialized) return;
  initialized = true;
  WIFI_STORE.loadFromFile();
  // Run the first attempt immediately; serviceWifiAutoConnect owns later
  // retries and DHCP polling without blocking the UI or display loop.
  nextAttemptAt = 0;
}

bool serviceWifiAutoConnect() {
  if (!initialized) initializeWifiAutoConnect();

  const bool connected = WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
  bool changed = connected != wasConnected;
  if (suspended) {
    // Keep the header indicator truthful while the foreground activity owns
    // the radio, but never issue a competing reconnect attempt.
    wasConnected = connected;
    return changed;
  }
  if (connected) {
    if (!wasConnected) {
      LOG_INF("WIFI", "Kobo saved network connected: ssid=%s ip=%s rssi=%d", WiFi.SSID().c_str(),
              WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    wasConnected = true;
    retryDelayMs = kInitialRetryMs;
    return changed;
  }

  if (wasConnected) {
    LOG_INF("WIFI", "Kobo saved network disconnected; reconnecting when due");
    wasConnected = false;
    nextAttemptAt = 0;
  }

  const std::string& lastSsid = WIFI_STORE.getLastConnectedSsid();
  const WifiCredential* credential = lastSsid.empty() ? nullptr : WIFI_STORE.findCredential(lastSsid);
  if (credential == nullptr) {
    configuredSsid.clear();
    retryDelayMs = kInitialRetryMs;
    return changed;
  }

  if (configuredSsid != credential->ssid) {
    configuredSsid.clear();
    retryDelayMs = kInitialRetryMs;
    nextAttemptAt = 0;
  }
  if (millis() >= nextAttemptAt) startSavedNetwork(*credential);
  return changed;
}

void setWifiAutoConnectSuspended(const bool requested) {
  if (suspended == requested) return;
  suspended = requested;
  if (!suspended) {
    // The selection activity may have saved, forgotten or replaced the last
    // network. Re-read its in-memory store on the next main-loop tick.
    configuredSsid.clear();
    retryDelayMs = kInitialRetryMs;
    nextAttemptAt = 0;
  }
  LOG_INF("WIFI", "Kobo saved-network auto-connect %s", suspended ? "suspended for UI" : "resumed");
}

}  // namespace crossink::kobo
