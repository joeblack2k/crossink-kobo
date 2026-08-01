// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

#include "KoboFbInkDisplay.h"

namespace crossink::kobo {

// Converts the packed native-landscape buffer to an 8-bpp framebuffer. This
// pure function is kept separate from FBInk so every rotation can be tested
// without an e-ink device.
[[nodiscard]] bool unpackPackedMono(const std::uint8_t* packed, std::size_t packedSize, std::uint8_t* target,
                                    std::size_t targetSize, std::uint32_t stride, std::uint16_t targetWidth,
                                    std::uint16_t targetHeight, SourceTransform transform);

}  // namespace crossink::kobo
