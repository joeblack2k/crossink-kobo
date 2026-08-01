#pragma once

namespace crossink::kobo {

// Load the persisted CrossInk Wi-Fi credential store once after /data is
// mounted. A saved "last connected" network is brought up automatically;
// unknown networks deliberately leave the radio off.
void initializeWifiAutoConnect();

// Advance the non-blocking station state machine. Returns true when the
// visible connection state changed, so callers can repaint a status icon.
bool serviceWifiAutoConnect();

// The visible Wi-Fi selection flow owns scans, joins and forget operations.
// Suspend background retries while it is open so they never race its explicit
// wpa_supplicant lifecycle; resuming immediately re-evaluates saved state.
void setWifiAutoConnectSuspended(bool suspended);

}  // namespace crossink::kobo
