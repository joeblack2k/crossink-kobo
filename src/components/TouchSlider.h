#pragma once

#include <algorithm>

#include "MappedInputManager.h"
#include "components/TouchUiRegistry.h"

// Small platform-neutral bridge for settings/readers that expose a coarse,
// finger-sized slider.  The rendering activity owns its visuals; this helper
// only keeps the registered value segments and their input consumption
// consistent across screens.
namespace TouchSlider {

inline int segmentCount(const int minValue, const int maxValue, const int step) {
  return std::max(1, (maxValue - minValue + std::max(1, step) - 1) / std::max(1, step) + 1);
}

inline void registerSegments(const int trackX, const int trackY, const int trackWidth, const int trackHeight,
                             const int minValue, const int maxValue, const int step) {
#ifdef KOBO_LINUX
  const int count = segmentCount(minValue, maxValue, step);
  for (int segment = 0; segment < count; ++segment) {
    const int x = trackX + (segment * trackWidth) / count;
    const int nextX = trackX + ((segment + 1) * trackWidth) / count;
    const int value = std::min(maxValue, minValue + segment * std::max(1, step));
    TOUCH_UI.registerDirect(x, trackY, std::max(1, nextX - x), trackHeight, TouchUiRegistry::TargetKind::Slider,
                            value);
  }
#else
  (void)trackX;
  (void)trackY;
  (void)trackWidth;
  (void)trackHeight;
  (void)minValue;
  (void)maxValue;
  (void)step;
#endif
}

inline bool consume(MappedInputManager& input, const int minValue, const int maxValue, int& value) {
#if defined(SIMULATOR) || defined(KOBO_LINUX)
  MappedInputManager::TouchTarget target;
  if (!input.consumeTouchTarget(target) ||
      target.kind != static_cast<unsigned char>(TouchUiRegistry::TargetKind::Slider)) {
    return false;
  }
  value = std::clamp(target.primary, minValue, maxValue);
  return true;
#else
  (void)input;
  (void)minValue;
  (void)maxValue;
  (void)value;
  return false;
#endif
}

}  // namespace TouchSlider
