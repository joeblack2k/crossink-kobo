#pragma once

namespace crossink::kobo {

// Keep the native HTTP/WebSocket transfer service aligned with the persisted
// Kobo setting. The service binds the USB gadget and any active WLAN address.
void reconcileWebTransfer();

// Explicitly release the native listener before a controlled in-process
// re-exec. Normal shutdown gets this from process teardown; exec does not.
void stopWebTransfer();

// Give the service a main-loop opportunity. The native listener currently has
// its own accept worker, but this preserves one owner for future UI callbacks.
void serviceWebTransfer();

[[nodiscard]] bool webTransferRunning();

}  // namespace crossink::kobo
