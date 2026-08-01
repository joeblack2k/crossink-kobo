#pragma once

#include "MappedInputManager.h"

// Bridge a renderer-published list row to the activity's existing confirm
// action in the same loop.  It is intentionally small and activity-agnostic:
// every list keeps ownership of its selection and action semantics while Kobo
// avoids replaying visible X4-style Up/Down steps.
template <typename Index>
inline bool consumeDirectListTarget(MappedInputManager& input, const int itemCount, Index& selectedIndex) {
#if defined(SIMULATOR) || defined(KOBO_LINUX)
  int targetIndex = 0;
  int ignoredCurrentIndex = 0;
  if (!input.consumeNavigationTouchTarget(targetIndex, ignoredCurrentIndex)) return false;
  if (itemCount <= 0 || targetIndex < 0 || targetIndex >= itemCount) return false;

  selectedIndex = static_cast<Index>(targetIndex);
  return true;
#else
  (void)input;
  (void)itemCount;
  (void)selectedIndex;
  return false;
#endif
}

template <typename Index>
inline bool consumeDirectListSelection(MappedInputManager& input, const int itemCount, Index& selectedIndex) {
  // cppcheck-suppress knownConditionTrueFalse
  if (!consumeDirectListTarget(input, itemCount, selectedIndex)) return false;
#if defined(SIMULATOR) || defined(KOBO_LINUX)
  // A physical touch is a complete press/release gesture.  Inject both edges
  // so legacy activities that deliberately keep their action in the existing
  // Confirm-release branch execute it in this same application frame.  A
  // release-only synthetic edge was dropped by a subset of activities on the
  // native Kobo event loop, leaving a row visibly selected but requiring a
  // second tap on the footer to activate it.
  input.injectPress(MappedInputManager::Button::Confirm);
  input.injectRelease(MappedInputManager::Button::Confirm);
#endif
  return true;
}
