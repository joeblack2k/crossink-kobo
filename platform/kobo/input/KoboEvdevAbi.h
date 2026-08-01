#pragma once

#include <cstdint>

namespace crossink::kobo {

// The i.MX6SL N437 exposes the native 32-bit evdev ABI: two 32-bit timeval
// fields followed by type/code/value. Keep the on-wire record explicit so
// host-side tests and future libc time_t changes cannot alter device reads.
struct KoboEvdevEvent {
  std::int32_t seconds;
  std::int32_t microseconds;
  std::uint16_t type;
  std::uint16_t code;
  std::int32_t value;
};

static_assert(sizeof(KoboEvdevEvent) == 16, "N437 evdev records must be 16 bytes");

}  // namespace crossink::kobo
