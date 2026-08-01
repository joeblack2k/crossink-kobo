// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

namespace crossink::kobo {

struct RefreshRegion {
  std::uint16_t x = 0;
  std::uint16_t y = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::size_t changedBytes = 0;

  [[nodiscard]] bool empty() const { return width == 0 || height == 0; }
};

}  // namespace crossink::kobo
