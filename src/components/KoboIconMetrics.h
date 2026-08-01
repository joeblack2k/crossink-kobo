#pragma once

#include <GfxRenderer.h>

#include <algorithm>
#include <cstdint>

#include "CrossPointSettings.h"
#include "components/themes/BaseTheme.h"

// Kobo-specific icon policy. Assets remain compact 1-bit 24/32px sources, but
// their painted size follows the user-facing interface scale and the actual
// role's available geometry. This avoids a large, blurry asset set or a second
// framebuffer while keeping the visible glyph and its surrounding row in
// lockstep on the 300ppi N437 panel.
namespace KoboIconMetrics {

inline int scaledAssetSize(const int sourceSize, const int maximumSize) {
  if (sourceSize <= 0 || maximumSize <= 0) return 0;
#ifdef KOBO_LINUX
  const int requested = SETTINGS.koboUiScalePercent;
  const int scale = requested == 100 || requested == 150 || requested == 200 || requested == 250 ? requested : 200;
  const int scaled = std::max(sourceSize, sourceSize * scale / 100);
  return std::min(scaled, maximumSize);
#else
  return std::min(sourceSize, maximumSize);
#endif
}

inline int listSize(const ThemeMetrics& metrics, const int sourceSize) {
  return scaledAssetSize(sourceSize,
                         std::max(sourceSize, metrics.listRowHeight - std::max(8, metrics.verticalSpacing)));
}

inline int menuSize(const ThemeMetrics& metrics, const int sourceSize) {
  return scaledAssetSize(sourceSize,
                         std::max(sourceSize, metrics.menuRowHeight - std::max(8, metrics.verticalSpacing)));
}

inline int tabSize(const ThemeMetrics& metrics, const int sourceSize) {
  return scaledAssetSize(sourceSize, std::max(sourceSize, metrics.tabBarHeight * 2 / 3));
}

inline int coverPlaceholderSize(const int sourceSize, const int coverWidth, const int coverHeight) {
  return scaledAssetSize(sourceSize, std::max(sourceSize, std::min(coverWidth, coverHeight) / 3));
}

inline void drawScaledSquare(const GfxRenderer& renderer, const uint8_t* bitmap, const int x, const int y,
                             const int sourceSize, const int destinationSize, const bool black = true) {
  if (bitmap == nullptr || sourceSize <= 0 || destinationSize <= 0) return;
  if (sourceSize == destinationSize) {
    if (black) {
      renderer.drawIcon(bitmap, x, y, sourceSize, sourceSize);
    } else {
      renderer.drawIconInverted(bitmap, x, y, sourceSize, sourceSize);
    }
    return;
  }

  const int sourceStride = (sourceSize + 7) / 8;
  for (int destinationY = 0; destinationY < destinationSize; ++destinationY) {
    const int sourceY = destinationY * sourceSize / destinationSize;
    for (int destinationX = 0; destinationX < destinationSize; ++destinationX) {
      const int sourceX = destinationX * sourceSize / destinationSize;
      const bool sourceBlack = ((bitmap[sourceY * sourceStride + sourceX / 8] >> (7 - (sourceX % 8))) & 1U) == 0;
      if (sourceBlack) renderer.drawPixel(x + destinationX, y + destinationY, black);
    }
  }
}

}  // namespace KoboIconMetrics
