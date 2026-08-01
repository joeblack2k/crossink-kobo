// SPDX-License-Identifier: GPL-3.0-or-later
#include "KoboPackedMono.h"

namespace crossink::kobo {
namespace {

bool packedPixelIsWhite(const std::uint8_t* packed, const std::uint16_t x, const std::uint16_t y) {
  const std::size_t offset = static_cast<std::size_t>(y) * (KoboFbInkDisplay::kPanelWidth / 8) + (x / 8);
  return (packed[offset] & static_cast<std::uint8_t>(0x80U >> (x % 8))) != 0;
}

}  // namespace

bool unpackPackedMono(const std::uint8_t* packed, const std::size_t packedSize, std::uint8_t* target,
                      const std::size_t targetSize, const std::uint32_t stride, const std::uint16_t targetWidth,
                      const std::uint16_t targetHeight, const SourceTransform transform) {
  if (packed == nullptr || target == nullptr || packedSize != KoboFbInkDisplay::kPackedFrameBytes ||
      static_cast<std::size_t>(stride) * targetHeight > targetSize) {
    return false;
  }
  const bool landscape = targetWidth == KoboFbInkDisplay::kPanelWidth &&
                         targetHeight == KoboFbInkDisplay::kPanelHeight && transform == SourceTransform::Identity;
  const bool portrait = targetWidth == KoboFbInkDisplay::kPortraitWidth &&
                        targetHeight == KoboFbInkDisplay::kPortraitHeight && transform != SourceTransform::Identity;
  if (!landscape && !portrait) {
    return false;
  }

  for (std::uint16_t y = 0; y < KoboFbInkDisplay::kPanelHeight; ++y) {
    for (std::uint16_t x = 0; x < KoboFbInkDisplay::kPanelWidth; ++x) {
      const std::uint8_t value = packedPixelIsWhite(packed, x, y) ? 0xFFU : 0x00U;
      std::uint16_t targetX = x;
      std::uint16_t targetY = y;
      if (transform == SourceTransform::RotateClockwise) {
        targetX = static_cast<std::uint16_t>(KoboFbInkDisplay::kPanelHeight - 1U - y);
        targetY = x;
      } else if (transform == SourceTransform::RotateCounterClockwise) {
        targetX = y;
        targetY = static_cast<std::uint16_t>(KoboFbInkDisplay::kPanelWidth - 1U - x);
      }
      target[static_cast<std::size_t>(targetY) * stride + targetX] = value;
    }
  }
  return true;
}

}  // namespace crossink::kobo
