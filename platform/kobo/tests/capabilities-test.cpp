#include <HalGPIO.h>

#include <cassert>
#include <cstring>

#include "platform/DeviceCapabilities.h"

int main() {
  constexpr auto capabilities = HalGPIO::capabilities();
  static_assert(capabilities.family == HalGPIO::DeviceFamily::N437);
  static_assert(capabilities.hasTouch);
  static_assert(!capabilities.hasFrontButtons);
  static_assert(!capabilities.hasSideButtons);
  static_assert(!capabilities.sideButtonsAreHorizontal);
  static_assert(!capabilities.hasTilt);
  static_assert(!capabilities.hasRtc);
  static_assert(capabilities.hasFrontlight);
  static_assert(capabilities.hasWifi);
  static_assert(capabilities.hasSuspend);

  assert(std::strcmp(HalGPIO::deviceFamilyName(), "Kobo Glo HD N437") == 0);
  assert(HalGPIO::hasTouch());
  assert(!HalGPIO::hasFrontButtons());
  assert(!HalGPIO::hasSideButtons());
  assert(!HalGPIO::hasTilt());
  assert(!HalGPIO::hasRtc());
  assert(HalGPIO::hasFrontlight());
  assert(HalGPIO::hasWifi());
  assert(HalGPIO::hasSuspend());

  const auto shared = crossink::platform::deviceCapabilities(gpio);
  if (shared.family != crossink::platform::DeviceFamily::N437 ||
      std::strcmp(shared.familyName(), "Kobo Glo HD N437") != 0 || !shared.hasTouch || shared.hasFrontButtons ||
      shared.hasSideButtons || shared.sideButtonsAreHorizontal || shared.hasTilt || shared.hasRtc ||
      !shared.hasFrontlight || !shared.hasWifi || !shared.hasSuspend) {
    return 1;
  }
  return 0;
}
