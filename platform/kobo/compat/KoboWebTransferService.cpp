#include "KoboWebTransferService.h"

#include <memory>

#include <Logging.h>

#include "CrossPointSettings.h"
#include "network/CrossPointWebServer.h"

namespace crossink::kobo {
namespace {
std::unique_ptr<CrossPointWebServer> service;
}

void reconcileWebTransfer() {
  const bool enabled = SETTINGS.koboWebTransferEnabled != 0;
  if (!enabled) {
    if (service) {
      LOG_INF("WEB", "Persistent Kobo web transfer disabled");
      service->stop();
      service.reset();
    }
    return;
  }

  if (service && service->isRunning()) {
    return;
  }

  service = std::make_unique<CrossPointWebServer>();
  service->begin();
  if (service->isRunning()) {
    LOG_INF("WEB", "Persistent Kobo web transfer available on USB and active Wi-Fi");
  } else {
    LOG_ERR("WEB", "Persistent Kobo web transfer failed to bind");
    service.reset();
  }
}

void stopWebTransfer() {
  if (!service) return;
  service->stop();
  service.reset();
}

void serviceWebTransfer() {
  if (service && service->isRunning()) {
    service->handleClient();
  }
}

bool webTransferRunning() { return service && service->isRunning(); }

}  // namespace crossink::kobo
