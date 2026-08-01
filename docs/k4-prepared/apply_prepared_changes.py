#!/usr/bin/env python3
'''Apply prepared CrossInk-Kobo Beta 4 changes in reviewable phases.

This file is intentionally baseline-bound. It refuses to guess through source
changes and is idempotent: an already applied phase is reported, not repeated.
Run from anywhere inside the repository.
'''

from __future__ import annotations

import argparse
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

BASELINE = "bbe2f05d4a587d55fa2e6391f9825e376122a76f"

TOUCH_UI_REGISTRY_H = '#pragma once\n\n#include <array>\n#include <cstddef>\n#include <cstdint>\n#ifdef KOBO_LINUX\n#include <mutex>\n#endif\n\n// Platform-neutral description of interactive UI regions. Renderers publish\n// what they drew; platform adapters translate a resolved target to their own\n// input mechanism. No evdev or Kobo hardware detail belongs here.\nclass TouchUiRegistry final {\n public:\n  enum class TargetKind : unsigned char { NavigationItem, OptionItem, KeyboardKey, Tab, Slider, TextSelectionSurface };\n\n  struct Region {\n    int x = 0;\n    int y = 0;\n    int width = 0;\n    int height = 0;\n    int currentIndex = 0;\n    int targetIndex = 0;\n    int itemCount = 0;\n    TargetKind kind = TargetKind::NavigationItem;\n    int secondaryTarget = 0;\n    // Overlap is forbidden by default. Set this only for a deliberately\n    // layered target pair whose painter/z-order is documented at the callsite.\n    bool overlapAllowed = false;\n  };\n\n  struct Resolution {\n    bool found = false;\n    int currentIndex = 0;\n    int targetIndex = 0;\n    int itemCount = 0;\n    TargetKind kind = TargetKind::NavigationItem;\n    int secondaryTarget = 0;\n    std::uint32_t generation = 0;\n  };\n\n  static constexpr std::size_t kMaxRegions = 64;\n\n  static TouchUiRegistry& instance();\n\n  // Compatibility invalidation for existing callers. Rendering code should\n  // use beginFrame()/commitFrame() so a partially rebuilt registry is never\n  // visible to the input thread.\n  void clear();\n  void beginFrame();\n  void commitFrame();\n  void abortFrame();\n  void invalidate();\n\n  bool registerItem(int x, int y, int width, int height, int currentIndex, int targetIndex, int itemCount);\n  bool registerDirect(int x, int y, int width, int height, TargetKind kind, int target, int secondaryTarget = 0,\n                      bool overlapAllowed = false);\n  [[nodiscard]] Resolution resolve(int x, int y) const;\n  [[nodiscard]] std::size_t size() const;\n  // Counts intersecting active hitboxes that were not both explicitly marked\n  // as an intentional painter-ordered overlap. This is a diagnostic/audit\n  // primitive; resolving behaviour remains last-drawn-wins for compatibility.\n  [[nodiscard]] std::size_t forbiddenOverlapCount() const;\n  // Incremented only when the active visual frame changes or is invalidated.\n  [[nodiscard]] std::uint32_t generation() const;\n  [[nodiscard]] bool isActiveGeneration(std::uint32_t generation) const;\n#ifdef KOBO_LINUX\n  [[nodiscard]] std::size_t snapshot(std::array<Region, kMaxRegions>& destination) const;\n#endif\n\n private:\n  using RegionBuffer = std::array<Region, kMaxRegions>;\n\n  [[nodiscard]] static bool intersects(const Region& first, const Region& second);\n  [[nodiscard]] RegionBuffer& writableRegions();\n  [[nodiscard]] std::size_t& writableCount();\n\n  RegionBuffer activeRegions_{};\n  RegionBuffer stagingRegions_{};\n  std::size_t activeCount_ = 0;\n  std::size_t stagingCount_ = 0;\n  std::uint32_t activeGeneration_ = 0;\n  std::uint32_t stagingBaseGeneration_ = 0;\n  bool frameOpen_ = false;\n#ifdef KOBO_LINUX\n  mutable std::mutex mutex_;\n#endif\n};\n\n#define TOUCH_UI TouchUiRegistry::instance()\n'
TOUCH_UI_REGISTRY_CPP = '#include "TouchUiRegistry.h"\n\n#include <algorithm>\n\nTouchUiRegistry& TouchUiRegistry::instance() {\n  static TouchUiRegistry registry;\n  return registry;\n}\n\nvoid TouchUiRegistry::clear() {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  // Existing renderers use clear() to remove lower z-order targets before\n  // drawing a modal. During a render transaction that must clear staging,\n  // never the last committed frame still visible to the user.\n  if (frameOpen_) {\n    stagingCount_ = 0;\n    return;\n  }\n  activeCount_ = 0;\n  ++activeGeneration_;\n}\n\nvoid TouchUiRegistry::beginFrame() {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  stagingCount_ = 0;\n  stagingBaseGeneration_ = activeGeneration_;\n  frameOpen_ = true;\n}\n\nvoid TouchUiRegistry::commitFrame() {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  if (!frameOpen_) return;\n  // Input consumption or an activity transition may invalidate the screen\n  // while this render is in progress. Never resurrect that stale frame.\n  if (stagingBaseGeneration_ != activeGeneration_) {\n    stagingCount_ = 0;\n    frameOpen_ = false;\n    return;\n  }\n  activeRegions_.swap(stagingRegions_);\n  activeCount_ = stagingCount_;\n  stagingCount_ = 0;\n  frameOpen_ = false;\n  ++activeGeneration_;\n}\n\nvoid TouchUiRegistry::abortFrame() {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  stagingCount_ = 0;\n  frameOpen_ = false;\n}\n\nvoid TouchUiRegistry::invalidate() {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  activeCount_ = 0;\n  ++activeGeneration_;\n}\n\nTouchUiRegistry::RegionBuffer& TouchUiRegistry::writableRegions() {\n  return frameOpen_ ? stagingRegions_ : activeRegions_;\n}\n\nstd::size_t& TouchUiRegistry::writableCount() { return frameOpen_ ? stagingCount_ : activeCount_; }\n\nbool TouchUiRegistry::registerItem(const int x, const int y, const int width, const int height, const int currentIndex,\n                                   const int targetIndex, const int itemCount) {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  auto& regions = writableRegions();\n  auto& count = writableCount();\n  if (count >= regions.size() || width <= 0 || height <= 0 || itemCount <= 0 || currentIndex < -1 ||\n      currentIndex >= itemCount || targetIndex < 0 || targetIndex >= itemCount) {\n    return false;\n  }\n  regions[count++] = {x, y,    width, height, currentIndex, targetIndex, itemCount, TargetKind::NavigationItem,\n                      0, false};\n  return true;\n}\n\nbool TouchUiRegistry::registerDirect(const int x, const int y, const int width, const int height, const TargetKind kind,\n                                     const int target, const int secondaryTarget, const bool overlapAllowed) {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  auto& regions = writableRegions();\n  auto& count = writableCount();\n  if (count >= regions.size() || width <= 0 || height <= 0 || kind == TargetKind::NavigationItem) return false;\n  regions[count++] = {x, y, width, height, 0, target, 0, kind, secondaryTarget, overlapAllowed};\n  return true;\n}\n\nbool TouchUiRegistry::intersects(const Region& first, const Region& second) {\n  return first.x < second.x + second.width && second.x < first.x + first.width && first.y < second.y + second.height &&\n         second.y < first.y + first.height;\n}\n\nTouchUiRegistry::Resolution TouchUiRegistry::resolve(const int x, const int y) const {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  // Last drawn wins when regions overlap, matching normal painter ordering.\n  for (std::size_t i = activeCount_; i > 0; --i) {\n    const Region& region = activeRegions_[i - 1];\n    if (x >= region.x && y >= region.y && x < region.x + region.width && y < region.y + region.height) {\n      return {true,        region.currentIndex,    region.targetIndex, region.itemCount,\n              region.kind, region.secondaryTarget, activeGeneration_};\n    }\n  }\n  return {};\n}\n\nstd::size_t TouchUiRegistry::size() const {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  return activeCount_;\n}\n\nstd::size_t TouchUiRegistry::forbiddenOverlapCount() const {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  std::size_t conflicts = 0;\n  for (std::size_t firstIndex = 0; firstIndex < activeCount_; ++firstIndex) {\n    for (std::size_t secondIndex = firstIndex + 1; secondIndex < activeCount_; ++secondIndex) {\n      const Region& first = activeRegions_[firstIndex];\n      const Region& second = activeRegions_[secondIndex];\n      if (intersects(first, second) && !(first.overlapAllowed && second.overlapAllowed)) ++conflicts;\n    }\n  }\n  return conflicts;\n}\n\nstd::uint32_t TouchUiRegistry::generation() const {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  return activeGeneration_;\n}\n\nbool TouchUiRegistry::isActiveGeneration(const std::uint32_t generation) const {\n#ifdef KOBO_LINUX\n  const std::lock_guard<std::mutex> lock(mutex_);\n#endif\n  return generation == activeGeneration_;\n}\n\n#ifdef KOBO_LINUX\nstd::size_t TouchUiRegistry::snapshot(std::array<Region, kMaxRegions>& destination) const {\n  const std::lock_guard<std::mutex> lock(mutex_);\n  std::copy_n(activeRegions_.begin(), activeCount_, destination.begin());\n  return activeCount_;\n}\n#endif\n'
TOUCH_GESTURE_H = '#pragma once\n\n#include <cstdint>\n\n#include "KoboEvdevTouch.h"\n\nnamespace crossink::kobo {\n\nenum class TouchContext : std::uint8_t { Navigation, Reader, Dialog, Keyboard };\nenum class TouchAction : std::uint8_t { None, UiItem, Back, Confirm, Left, Right, Up, Down, PageBack, PageForward };\nenum class TouchGesture : std::uint8_t { None, Start, Tap, LongPressStart, LongPressEnd, Swipe, Cancelled };\n\nstruct TouchDispatch {\n  TouchAction action = TouchAction::None;\n  bool press = false;\n  bool release = false;\n  TouchPoint point{};\n  TouchGesture gesture = TouchGesture::None;\n};\n\nclass KoboTouchGesture {\n public:\n  static constexpr std::int32_t kBottomFrameHeight = 96;\n  static constexpr std::int32_t kTapSlop = 24;\n  static constexpr std::int32_t kSwipeDistance = 72;\n  static constexpr std::uint64_t kLongPressMicros = 650\'000;\n\n  [[nodiscard]] TouchDispatch update(const TouchFrame& frame, TouchContext context, std::int32_t screenWidth,\n                                     std::int32_t screenHeight);\n  void reset();\n\n private:\n  [[nodiscard]] static TouchAction actionAt(TouchPoint point, TouchContext context, std::int32_t screenWidth,\n                                            std::int32_t screenHeight);\n\n  bool active_ = false;\n  bool longPressActive_ = false;\n  TouchPoint start_{};\n  TouchPoint latest_{};\n  std::uint64_t startedAt_ = 0;\n  TouchAction heldAction_ = TouchAction::None;\n};\n\n}  // namespace crossink::kobo\n'
TOUCH_GESTURE_CPP = '#include "KoboTouchGesture.h"\n\n#include <cstdlib>\n\nnamespace crossink::kobo {\n\nTouchAction KoboTouchGesture::actionAt(const TouchPoint point, const TouchContext context,\n                                       const std::int32_t screenWidth, const std::int32_t screenHeight) {\n  if (screenWidth <= 0 || screenHeight <= kBottomFrameHeight || point.x < 0 || point.y < 0 || point.x >= screenWidth ||\n      point.y >= screenHeight) {\n    return TouchAction::None;\n  }\n  if (point.y >= screenHeight - kBottomFrameHeight) {\n    if (context == TouchContext::Reader) {\n      return point.x < screenWidth / 2 ? TouchAction::PageBack : TouchAction::PageForward;\n    }\n    return point.x < screenWidth / 2 ? TouchAction::Back : TouchAction::Confirm;\n  }\n  if (context != TouchContext::Reader) {\n    return TouchAction::UiItem;\n  }\n  if (point.x < screenWidth * 3 / 10) {\n    return TouchAction::PageBack;\n  }\n  if (point.x >= screenWidth * 7 / 10) {\n    return TouchAction::PageForward;\n  }\n  return TouchAction::Confirm;\n}\n\nTouchDispatch KoboTouchGesture::update(const TouchFrame& frame, const TouchContext context,\n                                       const std::int32_t screenWidth, const std::int32_t screenHeight) {\n  if (frame.down && !active_) {\n    active_ = true;\n    longPressActive_ = false;\n    start_ = frame.point;\n    latest_ = frame.point;\n    startedAt_ = frame.timestampMicros;\n    heldAction_ = TouchAction::None;\n    return {TouchAction::None, false, false, start_, TouchGesture::Start};\n  }\n  if (!active_) {\n    return {};\n  }\n\n  latest_ = frame.point;\n  const std::int32_t deltaX = latest_.x - start_.x;\n  const std::int32_t deltaY = latest_.y - start_.y;\n  const bool withinTapSlop = std::abs(deltaX) <= kTapSlop && std::abs(deltaY) <= kTapSlop;\n\n  if (frame.down) {\n    if (!longPressActive_ && withinTapSlop && frame.timestampMicros >= startedAt_ &&\n        frame.timestampMicros - startedAt_ >= kLongPressMicros) {\n      heldAction_ = actionAt(start_, context, screenWidth, screenHeight);\n      if (heldAction_ != TouchAction::None) {\n        longPressActive_ = true;\n        return {heldAction_, true, false, start_, TouchGesture::LongPressStart};\n      }\n    }\n    return {};\n  }\n\n  TouchDispatch dispatch{};\n  if (longPressActive_) {\n    dispatch = {heldAction_, false, true, start_, TouchGesture::LongPressEnd};\n  } else if (withinTapSlop) {\n    const TouchAction action = actionAt(start_, context, screenWidth, screenHeight);\n    dispatch = {action, action != TouchAction::None, action != TouchAction::None, start_, TouchGesture::Tap};\n  } else if (std::abs(deltaX) >= kSwipeDistance && std::abs(deltaX) > std::abs(deltaY)) {\n    const TouchAction action = context == TouchContext::Reader\n                                   ? (deltaX < 0 ? TouchAction::PageForward : TouchAction::PageBack)\n                                   : (deltaX < 0 ? TouchAction::Right : TouchAction::Left);\n    dispatch = {action, true, true, start_, TouchGesture::Swipe};\n  } else if (context != TouchContext::Reader && std::abs(deltaY) >= kSwipeDistance) {\n    const TouchAction action = deltaY < 0 ? TouchAction::Down : TouchAction::Up;\n    dispatch = {action, true, true, start_, TouchGesture::Swipe};\n  } else {\n    // A gesture outside tap slop but below the swipe threshold is deliberate\n    // cancellation, not a silently lost tap.\n    dispatch = {TouchAction::None, false, false, start_, TouchGesture::Cancelled};\n  }\n  reset();\n  return dispatch;\n}\n\nvoid KoboTouchGesture::reset() {\n  active_ = false;\n  longPressActive_ = false;\n  startedAt_ = 0;\n  heldAction_ = TouchAction::None;\n}\n\n}  // namespace crossink::kobo\n'
DIRECT_LIST_TOUCH_H = '#pragma once\n\n#include "MappedInputManager.h"\n\n// Bridge a renderer-published list row to the activity\'s existing action in\n// the same loop. Touch remains semantic long enough for native Kobo long-press\n// handling; only a Tap is converted to legacy Confirm edges.\nenum class DirectListTouchKind { None, Tap, LongPress };\n\ntemplate <typename Index>\ninline DirectListTouchKind consumeDirectListEvent(MappedInputManager& input, const int itemCount, Index& selectedIndex) {\n#if defined(SIMULATOR) || defined(KOBO_LINUX)\n  int targetIndex = 0;\n  int ignoredCurrentIndex = 0;\n  MappedInputManager::TouchTargetGesture gesture = MappedInputManager::TouchTargetGesture::Tap;\n  if (!input.consumeNavigationTouchTarget(targetIndex, ignoredCurrentIndex, &gesture)) return DirectListTouchKind::None;\n  if (itemCount <= 0 || targetIndex < 0 || targetIndex >= itemCount) return DirectListTouchKind::None;\n\n  selectedIndex = static_cast<Index>(targetIndex);\n  return gesture == MappedInputManager::TouchTargetGesture::LongPress ? DirectListTouchKind::LongPress\n                                                                      : DirectListTouchKind::Tap;\n#else\n  (void)input;\n  (void)itemCount;\n  (void)selectedIndex;\n  return DirectListTouchKind::None;\n#endif\n}\n\ntemplate <typename Index>\ninline bool consumeDirectListTarget(MappedInputManager& input, const int itemCount, Index& selectedIndex) {\n  return consumeDirectListEvent(input, itemCount, selectedIndex) != DirectListTouchKind::None;\n}\n\ntemplate <typename Index>\ninline bool consumeDirectListSelection(MappedInputManager& input, const int itemCount, Index& selectedIndex) {\n  if (consumeDirectListEvent(input, itemCount, selectedIndex) != DirectListTouchKind::Tap) return false;\n#if defined(SIMULATOR) || defined(KOBO_LINUX)\n  // Legacy activities still execute their action in the existing Confirm\n  // release branch. A native long-press is intentionally not converted here.\n  input.injectPress(MappedInputManager::Button::Confirm);\n  input.injectRelease(MappedInputManager::Button::Confirm);\n#endif\n  return true;\n}\n'
KOBO_TOUCH_ROUTER_H = '#pragma once\n\n#include <array>\n#include <cstddef>\n#include <cstdint>\n\n#include "KoboTouchGesture.h"\n#include "components/TouchUiRegistry.h"\n\nnamespace crossink::kobo {\n\nenum class RoutedTouchKind : std::uint8_t { None, Target, Action, Cancelled };\nenum class RoutedTargetGesture : std::uint8_t { Tap, LongPress };\n\nstruct RoutedTouchInput {\n  RoutedTouchKind kind = RoutedTouchKind::None;\n  TouchAction action = TouchAction::None;\n  bool press = false;\n  bool release = false;\n  TouchPoint point{};\n  TouchUiRegistry::Resolution target{};\n  RoutedTargetGesture targetGesture = RoutedTargetGesture::Tap;\n  std::uint64_t activityGeneration = 0;\n};\n\n// Converts one gesture into either one committed visual target or one logical\n// action. Pointer capture lives here so the same implementation is exercised\n// by the N437 runtime and the routing E2E test.\nclass KoboTouchRouter final {\n public:\n  [[nodiscard]] RoutedTouchInput route(const TouchDispatch& event, const TouchUiRegistry& registry,\n                                       std::uint64_t activityGeneration);\n  void reset();\n\n private:\n  [[nodiscard]] static RoutedTouchInput actionInput(const TouchDispatch& event,\n                                                    std::uint64_t activityGeneration);\n\n  bool active_ = false;\n  bool longPressDelivered_ = false;\n  TouchUiRegistry::Resolution capturedTarget_{};\n  std::uint64_t capturedActivityGeneration_ = 0;\n};\n\n// Bounded semantic queue between evdev/gesture parsing and the legacy\n// MappedInputManager adapter. It preserves order, never overwrites an older\n// action and exposes overflow rather than failing silently.\nclass KoboSemanticInputQueue final {\n public:\n  static constexpr std::size_t kCapacity = 16;\n\n  [[nodiscard]] bool push(const RoutedTouchInput& input);\n  [[nodiscard]] bool pop(RoutedTouchInput& input);\n  void clear();\n  [[nodiscard]] std::size_t size() const { return size_; }\n  [[nodiscard]] std::uint32_t takeDroppedCount();\n\n private:\n  std::array<RoutedTouchInput, kCapacity> entries_{};\n  std::size_t head_ = 0;\n  std::size_t size_ = 0;\n  std::uint32_t dropped_ = 0;\n};\n\n}  // namespace crossink::kobo\n'
KOBO_TOUCH_ROUTER_CPP = '#include "KoboTouchRouter.h"\n\nnamespace crossink::kobo {\n\nRoutedTouchInput KoboTouchRouter::actionInput(const TouchDispatch& event,\n                                              const std::uint64_t activityGeneration) {\n  if (event.action == TouchAction::None || event.action == TouchAction::UiItem) return {};\n  return {RoutedTouchKind::Action, event.action, event.press, event.release, event.point, {},\n          RoutedTargetGesture::Tap, activityGeneration};\n}\n\nRoutedTouchInput KoboTouchRouter::route(const TouchDispatch& event, const TouchUiRegistry& registry,\n                                        const std::uint64_t activityGeneration) {\n  switch (event.gesture) {\n    case TouchGesture::Start:\n      active_ = true;\n      longPressDelivered_ = false;\n      capturedTarget_ = registry.resolve(event.point.x, event.point.y);\n      capturedActivityGeneration_ = activityGeneration;\n      return {};\n\n    case TouchGesture::Cancelled:\n      reset();\n      return {RoutedTouchKind::Cancelled};\n\n    case TouchGesture::Swipe: {\n      const std::uint64_t capturedGeneration = active_ ? capturedActivityGeneration_ : activityGeneration;\n      reset();\n      // A swipe is always a navigation action. It can never activate the\n      // visual target where the pointer went down.\n      return actionInput(event, capturedGeneration);\n    }\n\n    case TouchGesture::Tap: {\n      const auto target = capturedTarget_;\n      const std::uint64_t capturedGeneration = active_ ? capturedActivityGeneration_ : activityGeneration;\n      reset();\n      if (target.found && registry.isActiveGeneration(target.generation)) {\n        return {RoutedTouchKind::Target, TouchAction::None, false, false, event.point, target,\n                RoutedTargetGesture::Tap, capturedGeneration};\n      }\n      return actionInput(event, capturedGeneration);\n    }\n\n    case TouchGesture::LongPressStart:\n      if (active_ && capturedTarget_.found) {\n        const auto target = capturedTarget_;\n        longPressDelivered_ = true;\n        if (registry.isActiveGeneration(target.generation)) {\n          return {RoutedTouchKind::Target, TouchAction::None, false, false, event.point, target,\n                  RoutedTargetGesture::LongPress, capturedActivityGeneration_};\n        }\n        return {};\n      }\n      return actionInput(event, active_ ? capturedActivityGeneration_ : activityGeneration);\n\n    case TouchGesture::LongPressEnd: {\n      const bool swallowTargetRelease = longPressDelivered_ || (active_ && capturedTarget_.found);\n      const std::uint64_t capturedGeneration = active_ ? capturedActivityGeneration_ : activityGeneration;\n      reset();\n      return swallowTargetRelease ? RoutedTouchInput{} : actionInput(event, capturedGeneration);\n    }\n\n    case TouchGesture::None:\n    default:\n      return actionInput(event, active_ ? capturedActivityGeneration_ : activityGeneration);\n  }\n}\n\nvoid KoboTouchRouter::reset() {\n  active_ = false;\n  longPressDelivered_ = false;\n  capturedTarget_ = {};\n  capturedActivityGeneration_ = 0;\n}\n\nbool KoboSemanticInputQueue::push(const RoutedTouchInput& input) {\n  if (input.kind != RoutedTouchKind::Target && input.kind != RoutedTouchKind::Action) return true;\n  if (size_ >= entries_.size()) {\n    ++dropped_;\n    return false;\n  }\n  entries_[(head_ + size_) % entries_.size()] = input;\n  ++size_;\n  return true;\n}\n\nbool KoboSemanticInputQueue::pop(RoutedTouchInput& input) {\n  if (size_ == 0) return false;\n  input = entries_[head_];\n  head_ = (head_ + 1U) % entries_.size();\n  --size_;\n  return true;\n}\n\nvoid KoboSemanticInputQueue::clear() {\n  head_ = 0;\n  size_ = 0;\n  dropped_ = 0;\n}\n\nstd::uint32_t KoboSemanticInputQueue::takeDroppedCount() {\n  const std::uint32_t result = dropped_;\n  dropped_ = 0;\n  return result;\n}\n\n}  // namespace crossink::kobo\n'
TOUCH_ROUTING_E2E_TEST = '#include <linux/input.h>\n\n#include <cstdlib>\n#include <iostream>\n#include <vector>\n\n#include "KoboEvdevTouch.h"\n#include "KoboTouchGesture.h"\n#include "app/KoboTouchRouter.h"\n#include "components/TouchUiRegistry.h"\n\nnamespace {\nusing namespace crossink::kobo;\n\n[[noreturn]] void fail(const char* label) {\n  std::cerr << "touch routing E2E failed: " << label << \'\\n\';\n  std::exit(EXIT_FAILURE);\n}\n\nstruct FakeActivity {\n  int taps = 0;\n  int longPresses = 0;\n  std::vector<TouchAction> actions;\n\n  bool consume(KoboSemanticInputQueue& queue, const TouchUiRegistry& registry,\n               const std::uint64_t currentActivityGeneration) {\n    RoutedTouchInput input;\n    if (!queue.pop(input)) return false;\n    if (input.activityGeneration != currentActivityGeneration) return false;\n    if (input.kind == RoutedTouchKind::Target) {\n      if (!registry.isActiveGeneration(input.target.generation)) return false;\n      if (input.targetGesture == RoutedTargetGesture::LongPress)\n        ++longPresses;\n      else\n        ++taps;\n      return true;\n    }\n    if (input.kind == RoutedTouchKind::Action) {\n      actions.push_back(input.action);\n      return true;\n    }\n    return false;\n  }\n};\n\nvoid publishTarget(const int target) {\n  TOUCH_UI.beginFrame();\n  if (!TOUCH_UI.registerItem(0, 0, 100, 100, 0, target, 4)) fail("publish target");\n  TOUCH_UI.commitFrame();\n}\n\nvoid routeFrame(KoboTouchGesture& gesture, KoboTouchRouter& router, KoboSemanticInputQueue& queue,\n                const TouchFrame& frame, const std::uint64_t activityGeneration) {\n  const auto dispatch = gesture.update(frame, TouchContext::Navigation, 1072, 1448);\n  const auto routed = router.route(dispatch, TOUCH_UI, activityGeneration);\n  if (!queue.push(routed)) fail("unexpected queue overflow");\n}\n\n}  // namespace\n\nint main() {\n  constexpr std::uint64_t activity = 7;\n  TOUCH_UI.invalidate();\n  publishTarget(2);\n\n  // Raw evdev -> parser -> gesture -> captured registry target -> queue -> fake activity.\n  KoboEvdevTouch parser;\n  KoboTouchGesture gesture;\n  KoboTouchRouter router;\n  KoboSemanticInputQueue queue;\n  TouchFrame frame{};\n  if (parser.ingest(EV_ABS, ABS_MT_TRACKING_ID, 1, 100, frame) != TouchIngestResult::NoFrame ||\n      parser.ingest(EV_SYN, SYN_REPORT, 0, 1\'000, frame) != TouchIngestResult::FrameReady) {\n    fail("raw touch down");\n  }\n  routeFrame(gesture, router, queue, frame, activity);\n  if (parser.ingest(EV_ABS, ABS_MT_TRACKING_ID, -1, 2\'000, frame) != TouchIngestResult::NoFrame ||\n      parser.ingest(EV_SYN, SYN_REPORT, 0, 3\'000, frame) != TouchIngestResult::FrameReady) {\n    fail("raw touch up");\n  }\n  routeFrame(gesture, router, queue, frame, activity);\n  FakeActivity sink;\n  if (!sink.consume(queue, TOUCH_UI, activity) || sink.taps != 1) fail("raw target delivery");\n\n  // Swipe precedence: starting over a target yields a navigation action only.\n  routeFrame(gesture, router, queue, {{10, 10}, true, true, 10\'000, {10, 10}}, activity);\n  routeFrame(gesture, router, queue, {{200, 10}, false, true, 20\'000, {200, 10}}, activity);\n  if (!sink.consume(queue, TOUCH_UI, activity) || sink.actions.size() != 1 ||\n      sink.actions.back() != TouchAction::Left || sink.taps != 1) {\n    fail("swipe must not activate target");\n  }\n\n  // Native long-press target appears once; release is swallowed.\n  routeFrame(gesture, router, queue, {{10, 10}, true, true, 30\'000, {10, 10}}, activity);\n  routeFrame(gesture, router, queue, {{10, 10}, true, false, 700\'000, {10, 10}}, activity);\n  routeFrame(gesture, router, queue, {{10, 10}, false, false, 710\'000, {10, 10}}, activity);\n  if (!sink.consume(queue, TOUCH_UI, activity) || sink.longPresses != 1 || queue.size() != 0) {\n    fail("semantic target long press");\n  }\n\n  // Activity and visual generations are both hard rejection gates.\n  routeFrame(gesture, router, queue, {{10, 10}, true, true, 800\'000, {10, 10}}, activity);\n  routeFrame(gesture, router, queue, {{10, 10}, false, false, 810\'000, {10, 10}}, activity);\n  if (sink.consume(queue, TOUCH_UI, activity + 1)) fail("stale activity accepted");\n  routeFrame(gesture, router, queue, {{10, 10}, true, true, 820\'000, {10, 10}}, activity);\n  routeFrame(gesture, router, queue, {{10, 10}, false, false, 830\'000, {10, 10}}, activity);\n  TOUCH_UI.invalidate();\n  if (sink.consume(queue, TOUCH_UI, activity)) fail("stale visual generation accepted");\n\n  // The bounded queue preserves all earlier entries and reports overflow.\n  RoutedTouchInput action{};\n  action.kind = RoutedTouchKind::Action;\n  action.action = TouchAction::Right;\n  action.press = true;\n  action.release = true;\n  action.activityGeneration = activity;\n  for (std::size_t index = 0; index < KoboSemanticInputQueue::kCapacity; ++index) {\n    if (!queue.push(action)) fail("queue filled too early");\n  }\n  if (queue.push(action) || queue.takeDroppedCount() != 1) fail("queue overflow not reported");\n  for (std::size_t index = 0; index < KoboSemanticInputQueue::kCapacity; ++index) {\n    if (!sink.consume(queue, TOUCH_UI, activity)) fail("queue order delivery");\n  }\n  if (queue.size() != 0) fail("queue did not drain");\n  return EXIT_SUCCESS;\n}\n'
TOUCH_REGISTRY_TEST = '#include <cstdlib>\n#include <iostream>\n\n#include "components/TouchUiRegistry.h"\n\nnamespace {\n\n[[noreturn]] void fail(const char* label) {\n  std::cerr << label << \'\\n\';\n  std::exit(EXIT_FAILURE);\n}\n\n}  // namespace\n\nint main() {\n  TOUCH_UI.clear();\n  if (!TOUCH_UI.registerItem(20, 100, 500, 80, 1, 0, 4) || !TOUCH_UI.registerItem(20, 180, 500, 80, 1, 1, 4) ||\n      !TOUCH_UI.registerItem(20, 260, 500, 80, 1, 2, 4)) {\n    fail("valid rows must register");\n  }\n  if (TOUCH_UI.size() != 3) fail("row count");\n  if (TOUCH_UI.forbiddenOverlapCount() != 0) fail("adjacent rows must not overlap");\n\n  const auto second = TOUCH_UI.resolve(100, 220);\n  if (!second.found || second.currentIndex != 1 || second.targetIndex != 1 || second.itemCount != 4) {\n    fail("second row resolution");\n  }\n  const auto firstGeneration = second.generation;\n  if (!TOUCH_UI.isActiveGeneration(firstGeneration)) fail("active generation");\n\n  // Build a replacement frame without exposing it incrementally.\n  TOUCH_UI.beginFrame();\n  if (!TOUCH_UI.registerDirect(600, 400, 120, 80, TouchUiRegistry::TargetKind::Tab, 9)) {\n    fail("staged target registration");\n  }\n  if (!TOUCH_UI.resolve(100, 220).found) fail("active frame must remain visible while staging");\n  if (TOUCH_UI.resolve(650, 430).found) fail("staging frame must not be visible before commit");\n  TOUCH_UI.commitFrame();\n  if (TOUCH_UI.resolve(100, 220).found) fail("old frame must disappear after commit");\n  const auto committed = TOUCH_UI.resolve(650, 430);\n  if (!committed.found || committed.targetIndex != 9 || committed.generation == firstGeneration) {\n    fail("committed frame resolution");\n  }\n  if (TOUCH_UI.isActiveGeneration(firstGeneration)) fail("old generation must be stale");\n\n  // clear() during rendering clears staging for modal z-order while keeping\n  // the visible committed frame available until the modal commits.\n  TOUCH_UI.beginFrame();\n  if (!TOUCH_UI.registerDirect(10, 10, 20, 20, TouchUiRegistry::TargetKind::Tab, 10)) {\n    fail("pre-modal staged target registration");\n  }\n  TOUCH_UI.clear();\n  if (!TOUCH_UI.registerDirect(40, 40, 80, 80, TouchUiRegistry::TargetKind::Tab, 11)) {\n    fail("modal staged target registration");\n  }\n  if (!TOUCH_UI.resolve(650, 430).found || TOUCH_UI.resolve(50, 50).found) {\n    fail("modal staging visibility contract");\n  }\n  TOUCH_UI.commitFrame();\n  if (TOUCH_UI.resolve(15, 15).found || !TOUCH_UI.resolve(50, 50).found) fail("modal staging clear contract");\n\n  // Aborting a failed render must retain the last committed frame.\n  TOUCH_UI.beginFrame();\n  if (!TOUCH_UI.registerDirect(10, 10, 20, 20, TouchUiRegistry::TargetKind::Tab, 12)) {\n    fail("aborted target registration");\n  }\n  TOUCH_UI.abortFrame();\n  if (!TOUCH_UI.resolve(50, 50).found || TOUCH_UI.resolve(15, 15).found) fail("abort frame contract");\n\n  // Invalidation racing an in-flight render must prevent its later commit\n  // from resurrecting stale activity targets.\n  TOUCH_UI.beginFrame();\n  if (!TOUCH_UI.registerDirect(200, 200, 40, 40, TouchUiRegistry::TargetKind::Tab, 13)) {\n    fail("racing staged target registration");\n  }\n  const auto beforeRaceGeneration = TOUCH_UI.generation();\n  TOUCH_UI.invalidate();\n  TOUCH_UI.commitFrame();\n  if (TOUCH_UI.size() != 0 || TOUCH_UI.resolve(210, 210).found ||\n      TOUCH_UI.isActiveGeneration(beforeRaceGeneration)) {\n    fail("invalidate versus commit race contract");\n  }\n\n  if (!TOUCH_UI.registerItem(20, 100, 500, 80, 1, 0, 4) || !TOUCH_UI.registerItem(20, 180, 500, 80, 1, 1, 4) ||\n      !TOUCH_UI.registerItem(20, 260, 500, 80, 1, 2, 4)) {\n    fail("rows after invalidation");\n  }\n  if (!TOUCH_UI.registerItem(20, 340, 500, 60, -1, 2, 4)) fail("pre-list focus registration");\n  const auto fromTab = TOUCH_UI.resolve(100, 360);\n  if (!fromTab.found || fromTab.currentIndex != -1 || fromTab.targetIndex != 2) fail("pre-list focus resolution");\n\n  if (!TOUCH_UI.registerDirect(20, 400, 80, 60, TouchUiRegistry::TargetKind::KeyboardKey, 2, 7)) {\n    fail("direct key registration");\n  }\n  const auto key = TOUCH_UI.resolve(40, 420);\n  if (!key.found || key.kind != TouchUiRegistry::TargetKind::KeyboardKey || key.targetIndex != 2 ||\n      key.secondaryTarget != 7) {\n    fail("direct key resolution");\n  }\n\n  TOUCH_UI.clear();\n  if (!TOUCH_UI.registerDirect(10, 610, 100, 100, TouchUiRegistry::TargetKind::Tab, 1) ||\n      !TOUCH_UI.registerDirect(50, 650, 100, 100, TouchUiRegistry::TargetKind::Tab, 2)) {\n    fail("overlapping targets must still register for audit");\n  }\n  if (TOUCH_UI.forbiddenOverlapCount() != 1) fail("unmarked overlap must fail audit");\n\n  TOUCH_UI.clear();\n  if (!TOUCH_UI.registerDirect(10, 610, 100, 100, TouchUiRegistry::TargetKind::Tab, 1, 0, true) ||\n      !TOUCH_UI.registerDirect(50, 650, 100, 100, TouchUiRegistry::TargetKind::Tab, 2, 0, true)) {\n    fail("intentional overlap registration");\n  }\n  if (TOUCH_UI.forbiddenOverlapCount() != 0) fail("explicit overlap must be exempt");\n  return EXIT_SUCCESS;\n}\n'
TOUCH_GESTURE_TEST = '#include <cstdlib>\n#include <iostream>\n\n#include "KoboTouchGesture.h"\n\nusing crossink::kobo::KoboTouchGesture;\nusing crossink::kobo::TouchAction;\nusing crossink::kobo::TouchContext;\nusing crossink::kobo::TouchDispatch;\nusing crossink::kobo::TouchFrame;\nusing crossink::kobo::TouchGesture;\n\nnamespace {\n\nconstexpr int width = 1072;\nconstexpr int height = 1448;\n\n[[noreturn]] void fail(const char* label) {\n  std::cerr << label << \'\\n\';\n  std::exit(EXIT_FAILURE);\n}\n\nvoid expect(const TouchDispatch dispatch, const TouchAction action, const bool press, const bool release,\n            const TouchGesture gesture, const char* label) {\n  if (dispatch.action != action || dispatch.press != press || dispatch.release != release ||\n      dispatch.gesture != gesture) {\n    fail(label);\n  }\n}\n\nTouchFrame frame(const int x, const int y, const bool down, const std::uint64_t timestamp) {\n  return {{x, y}, down, true, timestamp, {x, y}};\n}\n\n}  // namespace\n\nint main() {\n  KoboTouchGesture gesture;\n  expect(gesture.update(frame(100, 1400, true, 1\'000), TouchContext::Navigation, width, height), TouchAction::None,\n         false, false, TouchGesture::Start, "down publishes capture start");\n  expect(gesture.update(frame(100, 1400, false, 100\'000), TouchContext::Navigation, width, height), TouchAction::Back,\n         true, true, TouchGesture::Tap, "bottom left navigation tap");\n\n  expect(gesture.update(frame(500, 900, true, 1\'000), TouchContext::Navigation, width, height), TouchAction::None,\n         false, false, TouchGesture::Start, "vertical swipe start");\n  expect(gesture.update(frame(500, 700, false, 100\'000), TouchContext::Navigation, width, height), TouchAction::Down,\n         true, true, TouchGesture::Swipe, "upward swipe advances focus");\n\n  expect(gesture.update(frame(700, 500, true, 1\'000), TouchContext::Navigation, width, height), TouchAction::None,\n         false, false, TouchGesture::Start, "horizontal swipe start");\n  expect(gesture.update(frame(450, 500, false, 100\'000), TouchContext::Navigation, width, height), TouchAction::Right,\n         true, true, TouchGesture::Swipe, "left swipe adjusts right");\n\n  expect(gesture.update(frame(500, 500, true, 1\'000), TouchContext::Navigation, width, height), TouchAction::None,\n         false, false, TouchGesture::Start, "deadband start");\n  expect(gesture.update(frame(548, 500, false, 100\'000), TouchContext::Navigation, width, height), TouchAction::None,\n         false, false, TouchGesture::Cancelled, "25-71px movement is explicit cancellation");\n\n  expect(gesture.update(frame(900, 500, true, 1\'000), TouchContext::Reader, width, height), TouchAction::None, false,\n         false, TouchGesture::Start, "reader start");\n  expect(gesture.update(frame(900, 500, false, 100\'000), TouchContext::Reader, width, height), TouchAction::PageForward,\n         true, true, TouchGesture::Tap, "reader right zone");\n\n  expect(gesture.update(frame(500, 500, true, 1\'000), TouchContext::Reader, width, height), TouchAction::None, false,\n         false, TouchGesture::Start, "reader swipe start");\n  expect(gesture.update(frame(300, 500, false, 100\'000), TouchContext::Reader, width, height), TouchAction::PageForward,\n         true, true, TouchGesture::Swipe, "left swipe advances");\n\n  expect(gesture.update(frame(100, 500, true, 1\'000), TouchContext::Reader, width, height), TouchAction::None, false,\n         false, TouchGesture::Start, "long press start");\n  expect(gesture.update(frame(100, 500, true, 700\'000), TouchContext::Reader, width, height), TouchAction::PageBack,\n         true, false, TouchGesture::LongPressStart, "long press publishes semantic start");\n  expect(gesture.update(frame(100, 500, false, 800\'000), TouchContext::Reader, width, height), TouchAction::PageBack,\n         false, true, TouchGesture::LongPressEnd, "long press publishes semantic end");\n  return EXIT_SUCCESS;\n}\n'
KOBO_FBINK_DISPLAY_H = '// SPDX-License-Identifier: GPL-3.0-or-later\n// CrossInk\'s Kobo framebuffer/EPDC adapter. This belongs to the Linux HAL,\n// never in an activity or reader implementation.\n#pragma once\n\n#include <cstddef>\n#include <cstdint>\n\n#include "KoboDisplayTypes.h"\n\nnamespace crossink::kobo {\n\nenum class RefreshKind : uint8_t {\n  Fast,\n  Partial,\n  Full,\n};\n\nenum class SourceTransform : uint8_t {\n  Identity,\n  RotateClockwise,\n  RotateCounterClockwise,\n};\n\nstruct DisplayGeometry {\n  uint16_t width = 0;\n  uint16_t height = 0;\n  uint32_t stride = 0;\n  uint8_t bitsPerPixel = 0;\n  bool nativeLandscape = false;\n};\n\n// Presents CrossInk/GfxRenderer\'s native landscape 1448x1072 packed buffer.\n// GfxRenderer rotates its logical 1072x1448 portrait UI into this buffer. A\n// one bit represents white; this matches HalDisplay::clearScreen(0xff).\nclass KoboFbInkDisplay {\n public:\n  static constexpr uint16_t kPortraitWidth = 1072;\n  static constexpr uint16_t kPortraitHeight = 1448;\n  static constexpr uint16_t kPanelWidth = kPortraitHeight;\n  static constexpr uint16_t kPanelHeight = kPortraitWidth;\n  static constexpr size_t kPackedFrameBytes = static_cast<size_t>(kPanelWidth / 8) * kPanelHeight;\n\n  explicit KoboFbInkDisplay(SourceTransform transform = SourceTransform::Identity) : transform_(transform) {}\n  ~KoboFbInkDisplay();\n  KoboFbInkDisplay(const KoboFbInkDisplay&) = delete;\n  KoboFbInkDisplay& operator=(const KoboFbInkDisplay&) = delete;\n\n  bool open();\n  void close();\n  bool isOpen() const { return fbfd_ >= 0; }\n  const DisplayGeometry& geometry() const { return geometry_; }\n\n  bool presentPackedMono(const uint8_t* packed, size_t packedSize, RefreshKind kind);\n  bool presentPackedMono(const uint8_t* packed, size_t packedSize, RefreshKind kind, const RefreshRegion& region);\n\n  // Convert the dirty rectangle from CrossInk\'s native 1448x1072 packed\n  // coordinate space into the active FBInk framebuffer orientation.\n  [[nodiscard]] static RefreshRegion mapRegionToFramebuffer(const RefreshRegion region,\n                                                                      const SourceTransform transform) {\n    if (region.empty()) return {};\n    if (transform == SourceTransform::RotateClockwise) {\n      if (static_cast<std::uint32_t>(region.y) + region.height > kPanelHeight ||\n          static_cast<std::uint32_t>(region.x) + region.width > kPanelWidth) {\n        return {};\n      }\n      return {.x = static_cast<std::uint16_t>(kPanelHeight - region.y - region.height),\n              .y = region.x,\n              .width = region.height,\n              .height = region.width,\n              .changedBytes = region.changedBytes};\n    }\n    if (transform == SourceTransform::RotateCounterClockwise) {\n      if (static_cast<std::uint32_t>(region.y) + region.height > kPanelHeight ||\n          static_cast<std::uint32_t>(region.x) + region.width > kPanelWidth) {\n        return {};\n      }\n      return {.x = region.y,\n              .y = static_cast<std::uint16_t>(kPanelWidth - region.x - region.width),\n              .width = region.height,\n              .height = region.width,\n              .changedBytes = region.changedBytes};\n    }\n    return region;\n  }\n\n  int lastError() const { return lastError_; }\n\n private:\n  bool refresh(RefreshKind kind, const RefreshRegion& sourceRegion);\n  bool copyPackedToFramebuffer(const uint8_t* packed);\n\n  int fbfd_ = -1;\n  unsigned char* framebuffer_ = nullptr;\n  size_t framebufferSize_ = 0;\n  DisplayGeometry geometry_{};\n  SourceTransform transform_ = SourceTransform::Identity;\n  int lastError_ = 0;\n};\n\n}  // namespace crossink::kobo\n'
KOBO_FBINK_DISPLAY_CPP = '// SPDX-License-Identifier: GPL-3.0-or-later\n#include "KoboFbInkDisplay.h"\n\n#include <cerrno>\n\n#include "KoboPackedMono.h"\n\nextern "C" {\n#include <fbink.h>\n}\n\nnamespace crossink::kobo {\nnamespace {\n\nFBInkConfig configFor(RefreshKind kind) {\n  FBInkConfig config{};\n  config.is_quiet = true;\n  switch (kind) {\n    case RefreshKind::Fast:\n      config.wfm_mode = WFM_DU;\n      break;\n    case RefreshKind::Partial:\n      config.wfm_mode = WFM_AUTO;\n      break;\n    case RefreshKind::Full:\n      config.wfm_mode = WFM_GC16;\n      config.is_flashing = true;\n      break;\n  }\n  return config;\n}\n\n}  // namespace\n\nKoboFbInkDisplay::~KoboFbInkDisplay() { close(); }\n\nbool KoboFbInkDisplay::open() {\n  close();\n  FBInkConfig config{};\n  config.is_quiet = true;\n  fbfd_ = fbink_open();\n  if (fbfd_ < 0) {\n    lastError_ = fbfd_;\n    fbfd_ = -1;\n    return false;\n  }\n  const int result = fbink_init(fbfd_, &config);\n  if (result != 0) {\n    lastError_ = result;\n    close();\n    return false;\n  }\n\n  FBInkState state{};\n  fbink_get_state(&config, &state);\n  geometry_.width = static_cast<uint16_t>(state.screen_width);\n  geometry_.height = static_cast<uint16_t>(state.screen_height);\n  geometry_.stride = state.scanline_stride;\n  geometry_.bitsPerPixel = static_cast<uint8_t>(state.bpp);\n  geometry_.nativeLandscape = state.screen_width == kPanelWidth && state.screen_height == kPanelHeight;\n  framebuffer_ = fbink_get_fb_pointer(fbfd_, &framebufferSize_);\n  const bool landscapeMatches = geometry_.nativeLandscape && transform_ == SourceTransform::Identity;\n  const bool portraitMatches = geometry_.width == kPortraitWidth && geometry_.height == kPortraitHeight &&\n                               transform_ != SourceTransform::Identity;\n  if (framebuffer_ == nullptr || geometry_.bitsPerPixel != 8 || (!landscapeMatches && !portraitMatches)) {\n    lastError_ = EOPNOTSUPP;\n    close();\n    return false;\n  }\n  lastError_ = 0;\n  return true;\n}\n\nvoid KoboFbInkDisplay::close() {\n  if (fbfd_ >= 0) fbink_close(fbfd_);\n  fbfd_ = -1;\n  framebuffer_ = nullptr;\n  framebufferSize_ = 0;\n  geometry_ = {};\n}\n\nbool KoboFbInkDisplay::copyPackedToFramebuffer(const uint8_t* packed) {\n  if (framebuffer_ == nullptr) return false;\n  return unpackPackedMono(packed, kPackedFrameBytes, framebuffer_, framebufferSize_, geometry_.stride, geometry_.width,\n                          geometry_.height, transform_);\n}\n\nbool KoboFbInkDisplay::refresh(const RefreshKind kind, const RefreshRegion& sourceRegion) {\n  const RefreshRegion region = mapRegionToFramebuffer(sourceRegion, transform_);\n  if (region.empty() || static_cast<std::uint32_t>(region.x) + region.width > geometry_.width ||\n      static_cast<std::uint32_t>(region.y) + region.height > geometry_.height) {\n    lastError_ = EINVAL;\n    return false;\n  }\n  FBInkConfig config = configFor(kind);\n  const int result = fbink_refresh(fbfd_, region.x, region.y, region.width, region.height, &config);\n  if (result != 0) {\n    lastError_ = result;\n    return false;\n  }\n  return true;\n}\n\nbool KoboFbInkDisplay::presentPackedMono(const uint8_t* packed, const size_t packedSize, const RefreshKind kind,\n                                         const RefreshRegion& region) {\n  if (!isOpen() || packed == nullptr || packedSize != kPackedFrameBytes) {\n    lastError_ = EINVAL;\n    return false;\n  }\n  if (region.empty()) return true;\n  // The central refresh scheduler owns the partial/full budget. The backend\n  // executes exactly that decision and must not silently promote a waveform.\n  if (!copyPackedToFramebuffer(packed) || !refresh(kind, region)) return false;\n  lastError_ = 0;\n  return true;\n}\n\nbool KoboFbInkDisplay::presentPackedMono(const uint8_t* const packed, const size_t packedSize, const RefreshKind kind) {\n  return presentPackedMono(packed, packedSize, kind, {.x = 0, .y = 0, .width = kPanelWidth, .height = kPanelHeight});\n}\n\n}  // namespace crossink::kobo\n'
PACKED_MONO_TEST = '#include <array>\n#include <cstdlib>\n#include <iostream>\n#include <vector>\n\n#include "KoboPackedMono.h"\n\nusing crossink::kobo::KoboFbInkDisplay;\nusing crossink::kobo::RefreshRegion;\nusing crossink::kobo::SourceTransform;\nusing crossink::kobo::unpackPackedMono;\n\nnamespace {\n\nusing Packed = std::array<std::uint8_t, KoboFbInkDisplay::kPackedFrameBytes>;\n\nvoid setBlack(Packed& packed, const std::uint16_t x, const std::uint16_t y) {\n  const std::size_t offset = static_cast<std::size_t>(y) * (KoboFbInkDisplay::kPanelWidth / 8) + x / 8;\n  packed[offset] &= static_cast<std::uint8_t>(~(0x80U >> (x % 8)));\n}\n\n[[noreturn]] void fail(const char* label) {\n  std::cerr << label << \'\\n\';\n  std::exit(EXIT_FAILURE);\n}\n\nvoid expectRegion(const RefreshRegion actual, const RefreshRegion expected, const char* label) {\n  if (actual.x != expected.x || actual.y != expected.y || actual.width != expected.width ||\n      actual.height != expected.height || actual.changedBytes != expected.changedBytes) {\n    fail(label);\n  }\n}\n\n}  // namespace\n\nint main() {\n  static Packed packed{};\n  packed.fill(0xFFU);\n  setBlack(packed, 0, 0);\n  setBlack(packed, KoboFbInkDisplay::kPanelWidth - 1, KoboFbInkDisplay::kPanelHeight - 1);\n\n  std::vector<std::uint8_t> landscape(KoboFbInkDisplay::kPanelWidth * KoboFbInkDisplay::kPanelHeight);\n  if (!unpackPackedMono(packed.data(), packed.size(), landscape.data(), landscape.size(), KoboFbInkDisplay::kPanelWidth,\n                        KoboFbInkDisplay::kPanelWidth, KoboFbInkDisplay::kPanelHeight, SourceTransform::Identity) ||\n      landscape.front() != 0 || landscape.back() != 0) {\n    fail("identity corner mapping failed");\n  }\n\n  std::vector<std::uint8_t> portrait(KoboFbInkDisplay::kPortraitWidth * KoboFbInkDisplay::kPortraitHeight);\n  if (!unpackPackedMono(packed.data(), packed.size(), portrait.data(), portrait.size(),\n                        KoboFbInkDisplay::kPortraitWidth, KoboFbInkDisplay::kPortraitWidth,\n                        KoboFbInkDisplay::kPortraitHeight, SourceTransform::RotateClockwise) ||\n      portrait[KoboFbInkDisplay::kPortraitWidth - 1] != 0 ||\n      portrait[(KoboFbInkDisplay::kPortraitHeight - 1) * KoboFbInkDisplay::kPortraitWidth] != 0) {\n    fail("clockwise corner mapping failed");\n  }\n\n  portrait.assign(portrait.size(), 0xFFU);\n  if (!unpackPackedMono(packed.data(), packed.size(), portrait.data(), portrait.size(),\n                        KoboFbInkDisplay::kPortraitWidth, KoboFbInkDisplay::kPortraitWidth,\n                        KoboFbInkDisplay::kPortraitHeight, SourceTransform::RotateCounterClockwise) ||\n      portrait[(KoboFbInkDisplay::kPortraitHeight - 1) * KoboFbInkDisplay::kPortraitWidth] != 0 ||\n      portrait[KoboFbInkDisplay::kPortraitWidth - 1] != 0) {\n    fail("counterclockwise corner mapping failed");\n  }\n\n  constexpr RefreshRegion source{.x = 100, .y = 200, .width = 80, .height = 40, .changedBytes = 7};\n  expectRegion(KoboFbInkDisplay::mapRegionToFramebuffer(source, SourceTransform::Identity), source,\n               "identity dirty region");\n  expectRegion(KoboFbInkDisplay::mapRegionToFramebuffer(source, SourceTransform::RotateClockwise),\n               {.x = 832, .y = 100, .width = 40, .height = 80, .changedBytes = 7},\n               "clockwise dirty region");\n  expectRegion(KoboFbInkDisplay::mapRegionToFramebuffer(source, SourceTransform::RotateCounterClockwise),\n               {.x = 200, .y = 1268, .width = 40, .height = 80, .changedBytes = 7},\n               "counterclockwise dirty region");\n  return EXIT_SUCCESS;\n}\n'
KOBO_EVDEV_TOUCH_H = '#pragma once\n\n#include <cstdint>\n#include <string>\n\n#include "KoboTouchTransform.h"\n\nnamespace crossink::kobo {\n\nstruct TouchDeviceInfo {\n  std::string path;\n  std::string name;\n  RawAxisRange x;\n  RawAxisRange y;\n  bool usesMultitouchAxes = false;\n};\n\nstruct TouchFrame {\n  TouchPoint point;\n  bool down = false;\n  bool positionChanged = false;\n  std::uint64_t timestampMicros = 0;\n  TouchPoint rawPoint;\n};\n\nenum class TouchReadResult : std::uint8_t {\n  FrameReady,\n  WouldBlock,\n  Interrupted,\n  Resynchronized,\n  DeviceLost,\n  ProtocolError,\n};\n\nenum class TouchIngestResult : std::uint8_t { NoFrame, FrameReady, ResyncRequired };\n\nclass KoboEvdevTouch {\n public:\n  KoboEvdevTouch() = default;\n  ~KoboEvdevTouch();\n\n  KoboEvdevTouch(const KoboEvdevTouch&) = delete;\n  KoboEvdevTouch& operator=(const KoboEvdevTouch&) = delete;\n\n  [[nodiscard]] static bool discover(TouchDeviceInfo& result, const std::string& inputDirectory = "/dev/input");\n  [[nodiscard]] bool open(const TouchDeviceInfo& device, TouchCalibration calibration = {});\n  void close();\n  void setOrientation(ScreenOrientation orientation);\n\n  // Detailed result keeps EAGAIN separate from device/protocol failure. The\n  // bool wrapper remains for simple tools such as touch-calibrate.\n  [[nodiscard]] TouchReadResult readFrameDetailed(TouchFrame& frame);\n  [[nodiscard]] bool readFrame(TouchFrame& frame);\n\n  // Public for deterministic parser tests and replaying recorded N437 events.\n  [[nodiscard]] TouchIngestResult ingest(std::uint16_t type, std::uint16_t code, std::int32_t value,\n                                         std::uint64_t timestampMicros, TouchFrame& frame);\n\n  [[nodiscard]] bool isOpen() const { return fd_ >= 0; }\n  [[nodiscard]] bool isDown() const { return down_; }\n  [[nodiscard]] const TouchDeviceInfo& device() const { return device_; }\n\n private:\n  [[nodiscard]] bool resyncState(TouchFrame& frame, std::uint64_t timestampMicros);\n  [[nodiscard]] std::uint64_t sanitizeTimestamp(std::uint64_t timestampMicros);\n\n  int fd_ = -1;\n  TouchDeviceInfo device_;\n  KoboTouchTransform transform_{{}};\n  std::int32_t rawX_ = 0;\n  std::int32_t rawY_ = 0;\n  bool down_ = false;\n  bool positionChanged_ = false;\n  bool discardUntilSynReport_ = false;\n  bool useKernelEventTimestamps_ = false;\n  std::uint64_t lastTimestampMicros_ = 0;\n};\n\n}  // namespace crossink::kobo\n'
KOBO_EVDEV_TOUCH_CPP = '#include "KoboEvdevTouch.h"\n\n#include <dirent.h>\n#include <fcntl.h>\n#include <linux/input.h>\n#include <sys/ioctl.h>\n#include <time.h>\n#include <unistd.h>\n\n#include <cerrno>\n#include <climits>\n#include <cstring>\n\n#include "KoboEvdevAbi.h"\n\nnamespace crossink::kobo {\nnamespace {\n\nconstexpr std::size_t bitsPerWord = sizeof(unsigned long) * CHAR_BIT;\nconstexpr std::size_t bitWords(const std::size_t maximum) { return maximum / bitsPerWord + 1U; }\n\nbool bitSet(const unsigned long* bits, const std::size_t bit) {\n  return (bits[bit / bitsPerWord] & (1UL << (bit % bitsPerWord))) != 0;\n}\n\nint evdevIoctl(const int fd, const unsigned long request, void* argument) {\n  return ::ioctl(fd, static_cast<int>(request), argument);\n}\n\nstd::uint64_t monotonicMicros() {\n  timespec now{};\n  return clock_gettime(CLOCK_MONOTONIC, &now) == 0\n             ? static_cast<std::uint64_t>(now.tv_sec) * 1\'000\'000ULL + now.tv_nsec / 1\'000ULL\n             : 0;\n}\n\nbool readAxis(const int fd, const unsigned int code, RawAxisRange& range) {\n  input_absinfo info{};\n  if (evdevIoctl(fd, EVIOCGABS(code), &info) < 0 || info.maximum <= info.minimum) return false;\n  range = {info.minimum, info.maximum};\n  return true;\n}\n\nbool readAxisValue(const int fd, const unsigned int code, std::int32_t& value) {\n  input_absinfo info{};\n  if (evdevIoctl(fd, EVIOCGABS(code), &info) < 0) return false;\n  value = info.value;\n  return true;\n}\n\nint deviceScore(const char* name, const bool direct, const bool hasTouchSignal) {\n  std::string lower = name == nullptr ? std::string() : std::string(name);\n  for (char& character : lower) {\n    if (character >= \'A\' && character <= \'Z\') character = static_cast<char>(character - \'A\' + \'a\');\n  }\n  int score = lower.find("zforce") != std::string::npos ? 100 : 10;\n  if (direct) score += 20;\n  if (hasTouchSignal) score += 20;\n  return score;\n}\n\nbool inspectDevice(const std::string& path, TouchDeviceInfo& result, int& score) {\n  const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);\n  if (fd < 0) return false;\n\n  unsigned long eventBits[bitWords(EV_MAX)]{};\n  unsigned long absoluteBits[bitWords(ABS_MAX)]{};\n  unsigned long keyBits[bitWords(KEY_MAX)]{};\n  unsigned long propertyBits[bitWords(INPUT_PROP_MAX)]{};\n  const bool hasEventBits = evdevIoctl(fd, EVIOCGBIT(0, sizeof(eventBits)), eventBits) >= 0;\n  const bool hasAbsoluteBits = evdevIoctl(fd, EVIOCGBIT(EV_ABS, sizeof(absoluteBits)), absoluteBits) >= 0;\n  if (!hasEventBits || !hasAbsoluteBits || !bitSet(eventBits, EV_ABS)) {\n    ::close(fd);\n    return false;\n  }\n\n  const bool multi = bitSet(absoluteBits, ABS_MT_POSITION_X) && bitSet(absoluteBits, ABS_MT_POSITION_Y);\n  const unsigned int xCode = multi ? ABS_MT_POSITION_X : ABS_X;\n  const unsigned int yCode = multi ? ABS_MT_POSITION_Y : ABS_Y;\n  RawAxisRange x{};\n  RawAxisRange y{};\n  if (!bitSet(absoluteBits, xCode) || !bitSet(absoluteBits, yCode) || !readAxis(fd, xCode, x) ||\n      !readAxis(fd, yCode, y)) {\n    ::close(fd);\n    return false;\n  }\n\n  const bool haveKeys = bitSet(eventBits, EV_KEY) &&\n                        evdevIoctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) >= 0;\n  const bool hasBtnTouch = haveKeys && bitSet(keyBits, BTN_TOUCH);\n  const bool hasTrackingId = multi && bitSet(absoluteBits, ABS_MT_TRACKING_ID);\n  const bool direct = evdevIoctl(fd, EVIOCGPROP(sizeof(propertyBits)), propertyBits) >= 0 &&\n                      bitSet(propertyBits, INPUT_PROP_DIRECT);\n\n  char name[256]{};\n  if (evdevIoctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) std::strncpy(name, "unknown", sizeof(name) - 1);\n  const int candidateScore = deviceScore(name, direct, hasBtnTouch || hasTrackingId);\n  if (candidateScore > score) {\n    score = candidateScore;\n    result = {path, name, x, y, multi};\n  }\n  ::close(fd);\n  return true;\n}\n\nbool deviceLostErrno(const int errorNumber) {\n  return errorNumber == ENODEV || errorNumber == ENXIO || errorNumber == EIO || errorNumber == EBADF;\n}\n\n}  // namespace\n\nKoboEvdevTouch::~KoboEvdevTouch() { close(); }\n\nbool KoboEvdevTouch::discover(TouchDeviceInfo& result, const std::string& inputDirectory) {\n  DIR* directory = opendir(inputDirectory.c_str());\n  if (directory == nullptr) return false;\n\n  int bestScore = -1;\n  while (const dirent* entry = readdir(directory)) {\n    if (std::strncmp(entry->d_name, "event", 5) != 0) continue;\n    (void)inspectDevice(inputDirectory + "/" + entry->d_name, result, bestScore);\n  }\n  closedir(directory);\n  return bestScore >= 0;\n}\n\nbool KoboEvdevTouch::open(const TouchDeviceInfo& device, TouchCalibration calibration) {\n  close();\n  fd_ = ::open(device.path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);\n  if (fd_ < 0) return false;\n  device_ = device;\n\n  const bool useDeviceCalibration =\n      calibration.x.maximum <= calibration.x.minimum && calibration.y.maximum <= calibration.y.minimum;\n  if (calibration.x.maximum <= calibration.x.minimum) calibration.x = device.x;\n  if (calibration.y.maximum <= calibration.y.minimum) calibration.y = device.y;\n\n  // N437 zForce coordinates follow the native 1448x1072 panel axes while\n  // CrossInk\'s user-facing coordinate system is 1072x1448 portrait.\n  const std::int32_t xSpan = calibration.x.maximum - calibration.x.minimum + 1;\n  const std::int32_t ySpan = calibration.y.maximum - calibration.y.minimum + 1;\n  if (useDeviceCalibration && xSpan == KoboTouchTransform::kPortraitHeight &&\n      ySpan == KoboTouchTransform::kPortraitWidth) {\n    calibration.swapAxes = true;\n    calibration.invertX = true;\n  }\n  transform_ = KoboTouchTransform(calibration);\n  if (!transform_.valid()) {\n    close();\n    return false;\n  }\n\n  int clockId = CLOCK_MONOTONIC;\n  useKernelEventTimestamps_ = evdevIoctl(fd_, EVIOCSCLOCKID, &clockId) == 0;\n  TouchFrame current{};\n  if (!resyncState(current, monotonicMicros())) {\n    close();\n    return false;\n  }\n  return true;\n}\n\nvoid KoboEvdevTouch::close() {\n  if (fd_ >= 0) ::close(fd_);\n  fd_ = -1;\n  device_ = {};\n  rawX_ = 0;\n  rawY_ = 0;\n  down_ = false;\n  positionChanged_ = false;\n  discardUntilSynReport_ = false;\n  useKernelEventTimestamps_ = false;\n  lastTimestampMicros_ = 0;\n}\n\nvoid KoboEvdevTouch::setOrientation(const ScreenOrientation orientation) { transform_.setOrientation(orientation); }\n\nstd::uint64_t KoboEvdevTouch::sanitizeTimestamp(std::uint64_t timestampMicros) {\n  if (timestampMicros == 0 || timestampMicros < lastTimestampMicros_) timestampMicros = monotonicMicros();\n  if (timestampMicros < lastTimestampMicros_) timestampMicros = lastTimestampMicros_ + 1U;\n  lastTimestampMicros_ = timestampMicros;\n  return timestampMicros;\n}\n\nTouchIngestResult KoboEvdevTouch::ingest(const std::uint16_t type, const std::uint16_t code, const std::int32_t value,\n                                         const std::uint64_t timestampMicros, TouchFrame& frame) {\n  if (discardUntilSynReport_) {\n    if (type == EV_SYN && code == SYN_REPORT) {\n      discardUntilSynReport_ = false;\n      return TouchIngestResult::ResyncRequired;\n    }\n    return TouchIngestResult::NoFrame;\n  }\n  if (type == EV_SYN && code == SYN_DROPPED) {\n    discardUntilSynReport_ = true;\n    positionChanged_ = false;\n    return TouchIngestResult::NoFrame;\n  }\n\n  if (type == EV_ABS) {\n    if (code == ABS_X || code == ABS_MT_POSITION_X) {\n      rawX_ = value;\n      positionChanged_ = true;\n    } else if (code == ABS_Y || code == ABS_MT_POSITION_Y) {\n      rawY_ = value;\n      positionChanged_ = true;\n    } else if (code == ABS_MT_TRACKING_ID) {\n      down_ = value >= 0;\n    }\n  } else if (type == EV_KEY && code == BTN_TOUCH) {\n    down_ = value != 0;\n  } else if (type == EV_SYN && code == SYN_REPORT) {\n    frame.point = transform_.map(rawX_, rawY_);\n    frame.rawPoint = {rawX_, rawY_};\n    frame.down = down_;\n    frame.positionChanged = positionChanged_;\n    frame.timestampMicros = sanitizeTimestamp(timestampMicros);\n    positionChanged_ = false;\n    return TouchIngestResult::FrameReady;\n  }\n  return TouchIngestResult::NoFrame;\n}\n\nbool KoboEvdevTouch::resyncState(TouchFrame& frame, const std::uint64_t timestampMicros) {\n  if (fd_ < 0) return false;\n  const unsigned int xCode = device_.usesMultitouchAxes ? ABS_MT_POSITION_X : ABS_X;\n  const unsigned int yCode = device_.usesMultitouchAxes ? ABS_MT_POSITION_Y : ABS_Y;\n  if (!readAxisValue(fd_, xCode, rawX_) || !readAxisValue(fd_, yCode, rawY_)) return false;\n\n  bool haveDownState = false;\n  unsigned long keyBits[bitWords(KEY_MAX)]{};\n  if (evdevIoctl(fd_, EVIOCGKEY(sizeof(keyBits)), keyBits) >= 0) {\n    down_ = bitSet(keyBits, BTN_TOUCH);\n    haveDownState = true;\n  }\n  if (!haveDownState && device_.usesMultitouchAxes) {\n    std::int32_t trackingId = -1;\n    if (readAxisValue(fd_, ABS_MT_TRACKING_ID, trackingId)) {\n      down_ = trackingId >= 0;\n      haveDownState = true;\n    }\n  }\n  if (!haveDownState) down_ = false;\n\n  frame.point = transform_.map(rawX_, rawY_);\n  frame.rawPoint = {rawX_, rawY_};\n  frame.down = down_;\n  frame.positionChanged = true;\n  frame.timestampMicros = sanitizeTimestamp(timestampMicros);\n  positionChanged_ = false;\n  return true;\n}\n\nTouchReadResult KoboEvdevTouch::readFrameDetailed(TouchFrame& frame) {\n  if (fd_ < 0) return TouchReadResult::DeviceLost;\n\n  while (true) {\n    KoboEvdevEvent event{};\n    const ssize_t count = ::read(fd_, &event, sizeof(event));\n    if (count < 0) {\n      const int errorNumber = errno;\n      if (errorNumber == EAGAIN || errorNumber == EWOULDBLOCK) return TouchReadResult::WouldBlock;\n      if (errorNumber == EINTR) return TouchReadResult::Interrupted;\n      return deviceLostErrno(errorNumber) ? TouchReadResult::DeviceLost : TouchReadResult::ProtocolError;\n    }\n    if (count == 0) return TouchReadResult::DeviceLost;\n    if (count != static_cast<ssize_t>(sizeof(event))) return TouchReadResult::ProtocolError;\n\n    std::uint64_t timestampMicros = monotonicMicros();\n    if (useKernelEventTimestamps_ && event.seconds >= 0 && event.microseconds >= 0 && event.microseconds < 1\'000\'000) {\n      timestampMicros = static_cast<std::uint64_t>(event.seconds) * 1\'000\'000ULL +\n                        static_cast<std::uint64_t>(event.microseconds);\n    }\n    const TouchIngestResult result = ingest(event.type, event.code, event.value, timestampMicros, frame);\n    if (result == TouchIngestResult::FrameReady) return TouchReadResult::FrameReady;\n    if (result == TouchIngestResult::ResyncRequired) {\n      if (!resyncState(frame, timestampMicros)) {\n        return deviceLostErrno(errno) ? TouchReadResult::DeviceLost : TouchReadResult::ProtocolError;\n      }\n      return TouchReadResult::Resynchronized;\n    }\n  }\n}\n\nbool KoboEvdevTouch::readFrame(TouchFrame& frame) {\n  while (true) {\n    const TouchReadResult result = readFrameDetailed(frame);\n    if (result == TouchReadResult::FrameReady) return true;\n    if (result == TouchReadResult::Interrupted || result == TouchReadResult::Resynchronized) continue;\n    return false;\n  }\n}\n\n}  // namespace crossink::kobo\n'
EVDEV_TOUCH_TEST = '#include <linux/input.h>\n\n#include <cstdlib>\n#include <iostream>\n\n#include "KoboEvdevTouch.h"\n\nusing crossink::kobo::KoboEvdevTouch;\nusing crossink::kobo::TouchFrame;\nusing crossink::kobo::TouchIngestResult;\n\nnamespace {\n[[noreturn]] void fail(const char* label) {\n  std::cerr << label << \'\\n\';\n  std::exit(EXIT_FAILURE);\n}\n}  // namespace\n\nint main() {\n  KoboEvdevTouch touch;\n  TouchFrame frame{};\n  if (touch.ingest(EV_ABS, ABS_MT_POSITION_X, 144, 100, frame) != TouchIngestResult::NoFrame ||\n      touch.ingest(EV_ABS, ABS_MT_POSITION_Y, 288, 200, frame) != TouchIngestResult::NoFrame ||\n      touch.ingest(EV_ABS, ABS_MT_TRACKING_ID, 7, 300, frame) != TouchIngestResult::NoFrame ||\n      touch.ingest(EV_SYN, SYN_REPORT, 0, 1\'000\'000, frame) != TouchIngestResult::FrameReady) {\n    fail("complete multitouch frame");\n  }\n  if (!frame.down || frame.rawPoint.x != 144 || frame.rawPoint.y != 288 || frame.timestampMicros != 1\'000\'000) {\n    fail("multitouch frame values");\n  }\n\n  if (touch.ingest(EV_SYN, SYN_DROPPED, 0, 1\'100\'000, frame) != TouchIngestResult::NoFrame ||\n      touch.ingest(EV_ABS, ABS_MT_POSITION_X, 999, 1\'200\'000, frame) != TouchIngestResult::NoFrame ||\n      touch.ingest(EV_SYN, SYN_REPORT, 0, 1\'300\'000, frame) != TouchIngestResult::ResyncRequired) {\n    fail("SYN_DROPPED discard contract");\n  }\n\n  if (touch.ingest(EV_KEY, BTN_TOUCH, 0, 1\'400\'000, frame) != TouchIngestResult::NoFrame ||\n      touch.ingest(EV_SYN, SYN_REPORT, 0, 1\'500\'000, frame) != TouchIngestResult::FrameReady || frame.down) {\n    fail("touch release frame");\n  }\n  // A backwards timestamp is clamped to a monotone value.\n  if (touch.ingest(EV_SYN, SYN_REPORT, 0, 10, frame) != TouchIngestResult::FrameReady ||\n      frame.timestampMicros < 1\'500\'000) {\n    fail("monotone timestamp contract");\n  }\n  return EXIT_SUCCESS;\n}\n'
WIFI_ASYNC_H = '#pragma once\n\n#include <NetworkClient.h>\n#include <WString.h>\n\n#include <array>\n#include <atomic>\n#include <cstdint>\n#include <mutex>\n#include <string>\n#include <thread>\n#include <vector>\n\nusing WiFiClient = NetworkClient;\n\nenum wl_status_t {\n  WL_IDLE_STATUS = 0,\n  WL_NO_SSID_AVAIL = 1,\n  WL_CONNECTED = 3,\n  WL_CONNECT_FAILED = 4,\n  WL_DISCONNECTED = 6\n};\n\nenum wifi_mode_t { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3, WIFI_MODE_NULL = 0 };\nenum wifi_auth_mode_t { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3 };\n\n#define WIFI_MODE_STA WIFI_STA\n#define WIFI_MODE_AP WIFI_AP\n#define WIFI_SCAN_RUNNING -1\n#define WIFI_SCAN_FAILED -2\n\nclass IPAddress {\n public:\n  IPAddress() = default;\n  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes_{a, b, c, d} {}\n  [[nodiscard]] String toString() const;\n  [[nodiscard]] uint8_t operator[](int index) const { return bytes_[index & 3]; }\n  uint8_t& operator[](int index) { return bytes_[index & 3]; }\n  [[nodiscard]] bool operator==(const IPAddress& other) const { return bytes_ == other.bytes_; }\n  [[nodiscard]] bool operator!=(const IPAddress& other) const { return !(*this == other); }\n\n private:\n  std::array<uint8_t, 4> bytes_{};\n};\n\nclass WiFiClass {\n public:\n  WiFiClass() = default;\n  ~WiFiClass();\n  WiFiClass(const WiFiClass&) = delete;\n  WiFiClass& operator=(const WiFiClass&) = delete;\n\n  wl_status_t begin(const char* ssid = nullptr, const char* password = nullptr);\n  wl_status_t status();\n  IPAddress localIP() const;\n  void persistent(bool) {}\n  void disconnect(bool wifiOff = false, bool eraseAccessPoint = false);\n  void mode(int mode);\n  wifi_mode_t getMode() const;\n\n  int scanNetworks(bool async = false, bool showHidden = false, bool passive = false,\n                   uint32_t maxMillisecondsPerChannel = 300, uint8_t channel = 0);\n  int scanComplete();\n  void scanDelete();\n  String SSID() const;\n  String SSID(int index) const;\n  int RSSI() const;\n  int RSSI(int index) const;\n  int encryptionType(int index) const;\n\n  String macAddress() const;\n  uint8_t* macAddress(uint8_t* destination) const;\n  void setHostname(const char* hostname);\n  String getHostname() const;\n  void setSleep(bool enabled);\n  void setAutoReconnect(bool enabled) { autoReconnect_ = enabled; }\n\n  bool softAP(const char* ssid, const char* password = nullptr, int channel = 1, int hidden = 0, int maxConnection = 4);\n  bool softAPdisconnect(bool wifiOff = false);\n  IPAddress softAPIP() const;\n  int softAPgetStationNum() const;\n\n private:\n  struct Network {\n    std::string ssid;\n    int rssi = 0;\n    int auth = WIFI_AUTH_OPEN;\n  };\n\n  wifi_mode_t mode_ = WIFI_OFF;\n  wl_status_t status_ = WL_DISCONNECTED;\n  std::string currentSsid_;\n  std::string hostname_ = "crossink-n437";\n  std::vector<Network> networks_;\n  bool scanPending_ = false;\n  bool autoReconnect_ = false;\n\n  std::thread scanThread_;\n  std::atomic<bool> scanRunning_{false};\n  std::atomic<bool> scanFinished_{false};\n  std::atomic<std::uint64_t> scanGeneration_{0};\n  std::mutex scanMutex_;\n  std::string scanOutput_;\n  std::uint64_t activeScanGeneration_ = 0;\n\n  std::thread dhcpThread_;\n  std::atomic<bool> dhcpRunning_{false};\n  std::atomic<bool> dhcpFinished_{false};\n  std::atomic<int> dhcpResult_{0};\n  std::atomic<std::uint64_t> dhcpGeneration_{0};\n  std::uint64_t activeDhcpGeneration_ = 0;\n  std::uint64_t nextDhcpAttemptAtMs_ = 0;\n\n  bool associatedCached_ = false;\n  std::uint64_t nextAssociationPollAtMs_ = 0;\n\n  [[nodiscard]] bool parseScanResults(const std::string& output);\n  void startScanWorker();\n  void reapScanWorker(bool consumeResult);\n  void startDhcpWorker();\n  void reapDhcpWorker();\n  [[nodiscard]] bool refreshAssociation(std::uint64_t nowMs);\n};\n\nextern WiFiClass WiFi;\n'
WIFI_ASYNC_CPP = '#include "WiFi.h"\n\n#include <arpa/inet.h>\n#include <fcntl.h>\n#include <ifaddrs.h>\n#include <sys/stat.h>\n#include <unistd.h>\n\n#include <algorithm>\n#include <chrono>\n#include <cstdio>\n#include <cstdlib>\n#include <cstring>\n#include <sstream>\n#include <utility>\n\nnamespace {\n\nconstexpr char kInterface[] = "wlan0";\nconstexpr char kStationConfig[] = "/run/crossink-wpa.conf";\nconstexpr char kHostapdConfig[] = "/run/crossink-hostapd.conf";\nconstexpr std::uint64_t kAssociationPollMs = 500;\nconstexpr std::uint64_t kDhcpRetryMs = 5\'000;\n\nint run(const char* command) { return std::system(command); }\n\nstd::string capture(const char* command) {\n  std::string output;\n  FILE* pipe = popen(command, "r");\n  if (pipe == nullptr) return output;\n  char buffer[512];\n  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;\n  (void)pclose(pipe);\n  return output;\n}\n\nstd::uint64_t monotonicMilliseconds() {\n  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(\n                                        std::chrono::steady_clock::now().time_since_epoch())\n                                        .count());\n}\n\nstd::string escapedQuoted(const char* value) {\n  std::string result;\n  if (value == nullptr) return result;\n  for (const unsigned char character : std::string(value)) {\n    if (character == \'\\\\\' || character == \'"\') result.push_back(\'\\\\\');\n    if (character >= 0x20 && character != 0x7f) result.push_back(static_cast<char>(character));\n  }\n  return result;\n}\n\nbool writePrivateFile(const char* path, const std::string& contents) {\n  const int descriptor = ::open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);\n  if (descriptor < 0) return false;\n  const bool written = ::write(descriptor, contents.data(), contents.size()) == static_cast<ssize_t>(contents.size());\n  const bool synced = ::fsync(descriptor) == 0;\n  ::close(descriptor);\n  return written && synced;\n}\n\nIPAddress interfaceAddress(const char* interfaceName) {\n  ifaddrs* addresses = nullptr;\n  if (getifaddrs(&addresses) != 0) return {};\n  IPAddress result;\n  for (const ifaddrs* entry = addresses; entry != nullptr; entry = entry->ifa_next) {\n    if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET ||\n        std::strcmp(entry->ifa_name, interfaceName) != 0)\n      continue;\n    const auto* address = reinterpret_cast<const sockaddr_in*>(entry->ifa_addr);\n    const uint32_t host = ntohl(address->sin_addr.s_addr);\n    result = IPAddress(static_cast<uint8_t>(host >> 24), static_cast<uint8_t>(host >> 16),\n                       static_cast<uint8_t>(host >> 8), static_cast<uint8_t>(host));\n    break;\n  }\n  freeifaddrs(addresses);\n  return result;\n}\n\n}  // namespace\n\nWiFiClass WiFi;\n\nWiFiClass::~WiFiClass() {\n  // The process supervisor normally execs/restarts after workers are idle. On\n  // a controlled exit, wait for the bounded BusyBox/iw commands to finish so\n  // no thread observes a destroyed WiFiClass.\n  if (scanThread_.joinable()) scanThread_.join();\n  if (dhcpThread_.joinable()) dhcpThread_.join();\n}\n\nString IPAddress::toString() const {\n  char value[16];\n  std::snprintf(value, sizeof(value), "%u.%u.%u.%u", bytes_[0], bytes_[1], bytes_[2], bytes_[3]);\n  return String(value);\n}\n\nvoid WiFiClass::mode(const int requestedMode) {\n  mode_ = static_cast<wifi_mode_t>(requestedMode);\n  if (mode_ == WIFI_OFF) {\n    disconnect(true);\n    return;\n  }\n  (void)run("ip link set wlan0 up >/dev/null 2>&1");\n}\n\nvoid WiFiClass::reapScanWorker(const bool consumeResult) {\n  if (!scanFinished_.load(std::memory_order_acquire)) return;\n  if (scanThread_.joinable()) scanThread_.join();\n  std::string output;\n  {\n    std::lock_guard lock(scanMutex_);\n    output.swap(scanOutput_);\n  }\n  const bool current = activeScanGeneration_ == scanGeneration_.load(std::memory_order_acquire);\n  scanFinished_.store(false, std::memory_order_release);\n  if (consumeResult && current) (void)parseScanResults(output);\n}\n\nvoid WiFiClass::startScanWorker() {\n  reapScanWorker(false);\n  activeScanGeneration_ = scanGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1U;\n  const std::uint64_t generation = activeScanGeneration_;\n  scanRunning_.store(true, std::memory_order_release);\n  scanFinished_.store(false, std::memory_order_release);\n  scanThread_ = std::thread([this, generation] {\n    std::string output = capture("iw dev wlan0 scan 2>/dev/null");\n    if (scanGeneration_.load(std::memory_order_acquire) == generation) {\n      std::lock_guard lock(scanMutex_);\n      scanOutput_ = std::move(output);\n    }\n    scanRunning_.store(false, std::memory_order_release);\n    scanFinished_.store(true, std::memory_order_release);\n  });\n}\n\nint WiFiClass::scanNetworks(bool, bool, bool, uint32_t, uint8_t) {\n  networks_.clear();\n  mode(WIFI_STA);\n  if (scanRunning_.load(std::memory_order_acquire)) {\n    scanPending_ = true;\n    return WIFI_SCAN_RUNNING;\n  }\n  reapScanWorker(false);\n  scanPending_ = true;\n  startScanWorker();\n  return WIFI_SCAN_RUNNING;\n}\n\nbool WiFiClass::parseScanResults(const std::string& output) {\n  std::istringstream lines(output);\n  std::string line;\n  networks_.clear();\n  Network candidate;\n  bool haveCandidate = false;\n  bool encrypted = false;\n  const auto storeCandidate = [&]() {\n    if (!haveCandidate || candidate.ssid.empty()) return;\n    candidate.auth = encrypted ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;\n    auto existing = std::find_if(networks_.begin(), networks_.end(),\n                                 [&](const Network& network) { return network.ssid == candidate.ssid; });\n    if (existing == networks_.end())\n      networks_.push_back(candidate);\n    else if (candidate.rssi > existing->rssi)\n      *existing = candidate;\n  };\n  while (std::getline(lines, line)) {\n    const std::size_t first = line.find_first_not_of(" \\t");\n    const std::string trimmed = first == std::string::npos ? std::string() : line.substr(first);\n    if (trimmed.rfind("BSS ", 0) == 0) {\n      storeCandidate();\n      candidate = {};\n      haveCandidate = true;\n      encrypted = false;\n    } else if (haveCandidate && trimmed.rfind("signal:", 0) == 0) {\n      candidate.rssi = static_cast<int>(std::strtol(trimmed.c_str() + 7, nullptr, 10));\n    } else if (haveCandidate && trimmed.rfind("SSID:", 0) == 0) {\n      const std::size_t value = trimmed.find_first_not_of(" \\t", 5);\n      candidate.ssid = value == std::string::npos ? std::string() : trimmed.substr(value);\n    } else if (haveCandidate && (trimmed.rfind("RSN:", 0) == 0 || trimmed.rfind("WPA:", 0) == 0)) {\n      encrypted = true;\n    }\n  }\n  storeCandidate();\n  return !networks_.empty();\n}\n\nint WiFiClass::scanComplete() {\n  if (!scanPending_) {\n    reapScanWorker(false);\n    return static_cast<int>(networks_.size());\n  }\n  if (scanRunning_.load(std::memory_order_acquire) || !scanFinished_.load(std::memory_order_acquire)) {\n    return WIFI_SCAN_RUNNING;\n  }\n  reapScanWorker(true);\n  scanPending_ = false;\n  return static_cast<int>(networks_.size());\n}\n\nvoid WiFiClass::scanDelete() {\n  networks_.clear();\n  scanPending_ = false;\n  (void)scanGeneration_.fetch_add(1, std::memory_order_acq_rel);\n  reapScanWorker(false);\n}\n\nvoid WiFiClass::reapDhcpWorker() {\n  if (!dhcpFinished_.load(std::memory_order_acquire)) return;\n  if (dhcpThread_.joinable()) dhcpThread_.join();\n  const int result = dhcpResult_.load(std::memory_order_acquire);\n  dhcpFinished_.store(false, std::memory_order_release);\n  if (result != 0 && localIP() == IPAddress()) nextDhcpAttemptAtMs_ = monotonicMilliseconds() + kDhcpRetryMs;\n}\n\nvoid WiFiClass::startDhcpWorker() {\n  reapDhcpWorker();\n  if (dhcpRunning_.load(std::memory_order_acquire)) return;\n  activeDhcpGeneration_ = dhcpGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1U;\n  const std::uint64_t generation = activeDhcpGeneration_;\n  dhcpRunning_.store(true, std::memory_order_release);\n  dhcpFinished_.store(false, std::memory_order_release);\n  dhcpThread_ = std::thread([this, generation] {\n    const int result = run("udhcpc -i wlan0 -n -q -t 4 -T 2 >/dev/null 2>&1");\n    if (dhcpGeneration_.load(std::memory_order_acquire) == generation) {\n      dhcpResult_.store(result, std::memory_order_release);\n    } else {\n      dhcpResult_.store(-1, std::memory_order_release);\n    }\n    dhcpRunning_.store(false, std::memory_order_release);\n    dhcpFinished_.store(true, std::memory_order_release);\n  });\n}\n\nbool WiFiClass::refreshAssociation(const std::uint64_t nowMs) {\n  if (nowMs < nextAssociationPollAtMs_) return associatedCached_;\n  nextAssociationPollAtMs_ = nowMs + kAssociationPollMs;\n  const std::string output = capture("wpa_cli -i wlan0 status 2>/dev/null");\n  associatedCached_ = output.find("wpa_state=COMPLETED") != std::string::npos;\n  return associatedCached_;\n}\n\nwl_status_t WiFiClass::begin(const char* ssid, const char* password) {\n  if (ssid == nullptr || ssid[0] == \'\\0\') return status_ = WL_NO_SSID_AVAIL;\n  mode(WIFI_STA);\n  currentSsid_ = ssid;\n  associatedCached_ = false;\n  nextAssociationPollAtMs_ = 0;\n  nextDhcpAttemptAtMs_ = 0;\n  (void)dhcpGeneration_.fetch_add(1, std::memory_order_acq_rel);\n  (void)run("killall udhcpc >/dev/null 2>&1");\n  (void)run("killall wpa_supplicant >/dev/null 2>&1");\n  (void)run("ip addr flush dev wlan0 >/dev/null 2>&1");\n\n  std::string config = "ctrl_interface=/run/wpa_supplicant\\nupdate_config=0\\nnetwork={\\n  ssid=\\"";\n  config += escapedQuoted(ssid);\n  config += "\\"\\n";\n  if (password != nullptr && password[0] != \'\\0\') {\n    config += "  psk=\\"" + escapedQuoted(password) + "\\"\\n  key_mgmt=WPA-PSK\\n";\n  } else {\n    config += "  key_mgmt=NONE\\n";\n  }\n  config += "}\\n";\n  if (!writePrivateFile(kStationConfig, config) ||\n      run("wpa_supplicant -B -i wlan0 -c /run/crossink-wpa.conf >/dev/null 2>&1") != 0)\n    return status_ = WL_CONNECT_FAILED;\n  return status_ = WL_IDLE_STATUS;\n}\n\nwl_status_t WiFiClass::status() {\n  if (mode_ != WIFI_STA && mode_ != WIFI_AP_STA && mode_ != WIFI_OFF) return status_;\n  reapDhcpWorker();\n  const std::uint64_t now = monotonicMilliseconds();\n  const bool associated = refreshAssociation(now);\n  if (!associated) {\n    if (mode_ == WIFI_STA || mode_ == WIFI_AP_STA) status_ = WL_IDLE_STATUS;\n    return status_;\n  }\n  if (mode_ == WIFI_OFF) mode_ = WIFI_STA;\n  if (localIP() != IPAddress()) return status_ = WL_CONNECTED;\n\n  if (!dhcpRunning_.load(std::memory_order_acquire) && now >= nextDhcpAttemptAtMs_) {\n    nextDhcpAttemptAtMs_ = now + kDhcpRetryMs;\n    startDhcpWorker();\n  }\n  return status_ = WL_IDLE_STATUS;\n}\n\nvoid WiFiClass::disconnect(const bool wifiOff, bool) {\n  (void)scanGeneration_.fetch_add(1, std::memory_order_acq_rel);\n  (void)dhcpGeneration_.fetch_add(1, std::memory_order_acq_rel);\n  (void)run("killall udhcpc >/dev/null 2>&1");\n  (void)run("killall wpa_supplicant >/dev/null 2>&1");\n  (void)run("ip addr flush dev wlan0 >/dev/null 2>&1");\n  status_ = WL_DISCONNECTED;\n  currentSsid_.clear();\n  associatedCached_ = false;\n  nextAssociationPollAtMs_ = 0;\n  nextDhcpAttemptAtMs_ = 0;\n  if (wifiOff) {\n    (void)run("ip link set wlan0 down >/dev/null 2>&1");\n    mode_ = WIFI_OFF;\n  }\n}\n\nIPAddress WiFiClass::localIP() const { return interfaceAddress(kInterface); }\nwifi_mode_t WiFiClass::getMode() const {\n  if (mode_ == WIFI_OFF && localIP() != IPAddress()) return WIFI_STA;\n  return mode_;\n}\nString WiFiClass::SSID() const {\n  if (!currentSsid_.empty()) return String(currentSsid_.c_str());\n  const std::string link = capture("iw dev wlan0 link 2>/dev/null");\n  const std::size_t position = link.find("SSID: ");\n  if (position == std::string::npos) return String();\n  const std::size_t start = position + 6;\n  const std::size_t end = link.find_first_of("\\r\\n", start);\n  return String(link.substr(start, end - start).c_str());\n}\nString WiFiClass::SSID(const int index) const {\n  return index >= 0 && index < static_cast<int>(networks_.size()) ? String(networks_[index].ssid.c_str()) : String();\n}\nint WiFiClass::RSSI() const {\n  const std::string output = capture("wpa_cli -i wlan0 signal_poll 2>/dev/null");\n  const std::size_t position = output.find("RSSI=");\n  if (position != std::string::npos) return std::atoi(output.c_str() + position + 5);\n  const std::string link = capture("iw dev wlan0 link 2>/dev/null");\n  const std::size_t signal = link.find("signal:");\n  return signal == std::string::npos ? 0 : std::atoi(link.c_str() + signal + 7);\n}\nint WiFiClass::RSSI(const int index) const {\n  return index >= 0 && index < static_cast<int>(networks_.size()) ? networks_[index].rssi : 0;\n}\nint WiFiClass::encryptionType(const int index) const {\n  return index >= 0 && index < static_cast<int>(networks_.size()) ? networks_[index].auth : WIFI_AUTH_OPEN;\n}\n\nuint8_t* WiFiClass::macAddress(uint8_t* destination) const {\n  if (destination == nullptr) return nullptr;\n  unsigned int values[6]{};\n  const std::string value = capture("cat /sys/class/net/wlan0/address 2>/dev/null");\n  if (std::sscanf(value.c_str(), "%x:%x:%x:%x:%x:%x", &values[0], &values[1], &values[2], &values[3], &values[4],\n                  &values[5]) != 6)\n    return destination;\n  for (int i = 0; i < 6; ++i) destination[i] = static_cast<uint8_t>(values[i]);\n  return destination;\n}\nString WiFiClass::macAddress() const {\n  const std::string value = capture("cat /sys/class/net/wlan0/address 2>/dev/null");\n  return String(value.empty() ? "00:00:00:00:00:00" : value.substr(0, value.find_first_of("\\r\\n")).c_str());\n}\nvoid WiFiClass::setHostname(const char* hostname) {\n  if (hostname != nullptr && hostname[0] != \'\\0\') hostname_ = hostname;\n}\nString WiFiClass::getHostname() const { return String(hostname_.c_str()); }\nvoid WiFiClass::setSleep(const bool enabled) {\n  (void)run(enabled ? "iw dev wlan0 set power_save on >/dev/null 2>&1"\n                    : "iw dev wlan0 set power_save off >/dev/null 2>&1");\n}\n\nbool WiFiClass::softAP(const char* ssid, const char* password, const int channel, const int hidden, int) {\n  if (ssid == nullptr || ssid[0] == \'\\0\') return false;\n  disconnect(false);\n  mode_ = WIFI_AP;\n  currentSsid_ = ssid;\n  std::string config = "interface=wlan0\\ndriver=nl80211\\nssid=" + escapedQuoted(ssid) +\n                       "\\nchannel=" + std::to_string(channel) +\n                       "\\nhw_mode=g\\nignore_broadcast_ssid=" + std::to_string(hidden ? 1 : 0) + "\\n";\n  if (password != nullptr && std::strlen(password) >= 8) {\n    config += "wpa=2\\nwpa_key_mgmt=WPA-PSK\\nrsn_pairwise=CCMP\\nwpa_passphrase=" + escapedQuoted(password) + "\\n";\n  }\n  if (!writePrivateFile(kHostapdConfig, config)) return false;\n  (void)run("ip addr flush dev wlan0 >/dev/null 2>&1");\n  (void)run("ip addr add 192.168.4.1/24 dev wlan0 >/dev/null 2>&1");\n  if (run("hostapd -B /run/crossink-hostapd.conf >/dev/null 2>&1") != 0) return false;\n  if (run("dnsmasq --interface=wlan0 --bind-interfaces --except-interface=lo --dhcp-range=192.168.4.20,192.168.4.80,"\n          "255.255.255.0,12h --pid-file=/run/crossink-dnsmasq.pid >/dev/null 2>&1") != 0) {\n    (void)run("killall hostapd >/dev/null 2>&1");\n    return false;\n  }\n  status_ = WL_CONNECTED;\n  return true;\n}\nbool WiFiClass::softAPdisconnect(const bool wifiOff) {\n  (void)run("killall dnsmasq >/dev/null 2>&1");\n  (void)run("killall hostapd >/dev/null 2>&1");\n  (void)run("ip addr flush dev wlan0 >/dev/null 2>&1");\n  status_ = WL_DISCONNECTED;\n  if (wifiOff) mode(WIFI_OFF);\n  return true;\n}\nIPAddress WiFiClass::softAPIP() const { return mode_ == WIFI_AP ? IPAddress(192, 168, 4, 1) : IPAddress(); }\nint WiFiClass::softAPgetStationNum() const {\n  const std::string output = capture("iw dev wlan0 station dump 2>/dev/null | grep -c \'^Station \'");\n  return std::atoi(output.c_str());\n}\n'
HAL_GPIO_H = '#pragma once\n\n#include <cstdint>\n\n#include "KoboEvdevKey.h"\n#include "KoboSuspendController.h"\n#include "KoboSysfs.h"\n\nclass HalGPIO {\n public:\n  enum class DeviceFamily : std::uint8_t { X4, X3, N437 };\n  struct Capabilities {\n    DeviceFamily family;\n    bool hasTouch;\n    bool hasFrontButtons;\n    bool hasSideButtons;\n    bool sideButtonsAreHorizontal;\n    bool hasTilt;\n    bool hasRtc;\n    bool hasFrontlight;\n    bool hasWifi;\n    bool hasSuspend;\n  };\n  enum class WakeupReason { PowerButton, AfterFlash, AfterUSBPower, Other };\n\n  static constexpr std::uint8_t BTN_BACK = 0;\n  static constexpr std::uint8_t BTN_CONFIRM = 1;\n  static constexpr std::uint8_t BTN_LEFT = 2;\n  static constexpr std::uint8_t BTN_RIGHT = 3;\n  static constexpr std::uint8_t BTN_UP = 4;\n  static constexpr std::uint8_t BTN_DOWN = 5;\n  static constexpr std::uint8_t BTN_POWER = 6;\n\n  [[nodiscard]] static constexpr Capabilities capabilities() {\n    return {DeviceFamily::N437, true, false, false, false, false, false, true, true, true};\n  }\n  [[nodiscard]] static constexpr DeviceFamily deviceFamily() { return capabilities().family; }\n  [[nodiscard]] static constexpr const char* deviceFamilyName() { return "Kobo Glo HD N437"; }\n  [[nodiscard]] static constexpr bool hasTouch() { return capabilities().hasTouch; }\n  [[nodiscard]] static constexpr bool hasFrontButtons() { return capabilities().hasFrontButtons; }\n  [[nodiscard]] static constexpr bool hasSideButtons() { return capabilities().hasSideButtons; }\n  [[nodiscard]] static constexpr bool sideButtonsAreHorizontal() { return capabilities().sideButtonsAreHorizontal; }\n  [[nodiscard]] static constexpr bool hasTilt() { return capabilities().hasTilt; }\n  [[nodiscard]] static constexpr bool hasRtc() { return capabilities().hasRtc; }\n  [[nodiscard]] static constexpr bool hasFrontlight() { return capabilities().hasFrontlight; }\n  [[nodiscard]] static constexpr bool hasWifi() { return capabilities().hasWifi; }\n  [[nodiscard]] static constexpr bool hasSuspend() { return capabilities().hasSuspend; }\n  void begin();\n  void beginFrame();\n  void update();\n  [[nodiscard]] bool isPressed(std::uint8_t buttonIndex) const;\n  [[nodiscard]] bool wasPressed(std::uint8_t buttonIndex) const;\n  [[nodiscard]] bool wasAnyPressed() const;\n  [[nodiscard]] bool wasReleased(std::uint8_t buttonIndex) const;\n  [[nodiscard]] bool wasAnyReleased() const;\n  [[nodiscard]] unsigned long getHeldTime() const;\n  [[nodiscard]] unsigned long getPowerButtonHeldTime() const;\n  [[nodiscard]] bool consumeSimulatorSleepRequest() const { return false; }\n  [[nodiscard]] crossink::kobo::KoboSuspendResult startDeepSleep();\n  void verifyPowerButtonWakeup(std::uint16_t requiredDurationMs, bool shortPressAllowed);\n  [[nodiscard]] bool isUsbConnected() const { return usbConnected_; }\n  [[nodiscard]] bool wasUsbStateChanged() const { return usbChanged_; }\n  [[nodiscard]] WakeupReason getWakeupReason() const { return WakeupReason::Other; }\n\n private:\n  crossink::kobo::KoboEvdevKey powerKey_;\n  crossink::kobo::KoboBatterySysfs battery_;\n  bool usbConnected_ = false;\n  bool usbChanged_ = false;\n  std::uint64_t nextBatteryPollMs_ = 0;\n};\n\nextern HalGPIO gpio;\n'
HAL_GPIO_CPP = '#include "HalGPIO.h"\n\n#include <fcntl.h>\n#include <time.h>\n#include <unistd.h>\n\n#include <cstdio>\n\nnamespace {\nconstexpr std::uint64_t kBatteryPollIntervalMs = 1000;\n\nstd::uint64_t monotonicMilliseconds() {\n  timespec now{};\n  return clock_gettime(CLOCK_MONOTONIC, &now) == 0\n             ? static_cast<std::uint64_t>(now.tv_sec) * 1000ULL + now.tv_nsec / 1\'000\'000ULL\n             : 0;\n}\n}  // namespace\n\nHalGPIO gpio;\n\nvoid HalGPIO::begin() {\n  crossink::kobo::KeyDeviceInfo powerDevice;\n  if (crossink::kobo::KoboEvdevKey::discoverPowerKey(powerDevice) && !powerKey_.open(powerDevice)) {\n    std::fprintf(stderr, "[KOBO] failed to open power input %s\\n", powerDevice.path.c_str());\n  }\n  if (!battery_.discover()) {\n    std::fprintf(stderr, "[KOBO] no battery power_supply found\\n");\n  }\n  crossink::kobo::BatterySnapshot snapshot;\n  if (battery_.read(snapshot)) usbConnected_ = snapshot.usbOnline;\n  nextBatteryPollMs_ = monotonicMilliseconds() + kBatteryPollIntervalMs;\n}\n\nvoid HalGPIO::beginFrame() {\n  powerKey_.beginFrame();\n  usbChanged_ = false;\n}\n\nvoid HalGPIO::update() {\n  powerKey_.update();\n  const std::uint64_t now = monotonicMilliseconds();\n  if (now != 0 && now < nextBatteryPollMs_) return;\n  nextBatteryPollMs_ = now + kBatteryPollIntervalMs;\n\n  crossink::kobo::BatterySnapshot snapshot;\n  if (battery_.read(snapshot) && snapshot.usbOnline != usbConnected_) {\n    usbConnected_ = snapshot.usbOnline;\n    usbChanged_ = true;\n  }\n}\n\nbool HalGPIO::isPressed(const std::uint8_t buttonIndex) const {\n  return buttonIndex == BTN_POWER && powerKey_.isPressed();\n}\n\nbool HalGPIO::wasPressed(const std::uint8_t buttonIndex) const {\n  return buttonIndex == BTN_POWER && powerKey_.wasPressed();\n}\n\nbool HalGPIO::wasAnyPressed() const { return powerKey_.wasPressed(); }\n\nbool HalGPIO::wasReleased(const std::uint8_t buttonIndex) const {\n  return buttonIndex == BTN_POWER && powerKey_.wasReleased();\n}\n\nbool HalGPIO::wasAnyReleased() const { return powerKey_.wasReleased(); }\n\nunsigned long HalGPIO::getHeldTime() const { return powerKey_.heldMilliseconds(); }\n\nunsigned long HalGPIO::getPowerButtonHeldTime() const { return powerKey_.heldMilliseconds(); }\n\ncrossink::kobo::KoboSuspendResult HalGPIO::startDeepSleep() {\n  crossink::kobo::KoboFrontlightSysfs frontlight;\n  const bool haveFrontlight = frontlight.discover();\n  const int savedBrightness = haveFrontlight ? frontlight.percentage() : -1;\n  if (haveFrontlight) (void)frontlight.setPercentage(0);\n  auto result = crossink::kobo::KoboSuspendController::suspendToRam(\n      {"frontlight_off=" + std::to_string(savedBrightness >= 0 ? 1 : 0)});\n  if (!result.entered) std::fprintf(stderr, "[KOBO] suspend request failed: %s\\n", result.detail.c_str());\n  if (savedBrightness >= 0) (void)frontlight.setPercentage(savedBrightness);\n  return result;\n}\n\nvoid HalGPIO::verifyPowerButtonWakeup(std::uint16_t /*requiredDurationMs*/, bool /*shortPressAllowed*/) {}\n'
KOBOWIFI_AUTOCONNECT_CPP = '#include "KoboWifiAutoConnect.h"\n\n#include <Arduino.h>\n#include <Logging.h>\n#include <WiFi.h>\n#include <WifiCredentialStore.h>\n\n#include <algorithm>\n#include <string>\n\nnamespace crossink::kobo {\nnamespace {\nconstexpr unsigned long kInitialRetryMs = 15\'000;\nconstexpr unsigned long kMaximumRetryMs = 5UL * 60UL * 1000UL;\nconstexpr unsigned long kConnectingPollMs = 500;\nconstexpr unsigned long kConnectedPollMs = 2\'000;\n\nbool initialized = false;\nbool wasConnected = false;\nbool suspended = false;\nunsigned long nextAttemptAt = 0;\nunsigned long nextStatusPollAt = 0;\nunsigned long retryDelayMs = kInitialRetryMs;\nstd::string configuredSsid;\n\nbool deadlineReached(const unsigned long now, const unsigned long deadline) {\n  return static_cast<long>(now - deadline) >= 0;\n}\n\nvoid startSavedNetwork(const WifiCredential& credential) {\n  WiFi.persistent(false);\n  WiFi.setAutoReconnect(true);\n  WiFi.mode(WIFI_STA);\n  const wl_status_t result = credential.password.empty() ? WiFi.begin(credential.ssid.c_str())\n                                                           : WiFi.begin(credential.ssid.c_str(), credential.password.c_str());\n  configuredSsid = credential.ssid;\n  nextAttemptAt = millis() + retryDelayMs;\n  LOG_INF("WIFI", "Kobo saved-network connect started: ssid=%s result=%d retry=%lums", configuredSsid.c_str(),\n          static_cast<int>(result), retryDelayMs);\n  retryDelayMs = std::min(kMaximumRetryMs, retryDelayMs * 2UL);\n}\n}  // namespace\n\nvoid initializeWifiAutoConnect() {\n  if (initialized) return;\n  initialized = true;\n  WIFI_STORE.loadFromFile();\n  // Run the first attempt immediately; serviceWifiAutoConnect owns later\n  // retries and DHCP polling without spinning shell commands every UI frame.\n  nextAttemptAt = 0;\n  nextStatusPollAt = 0;\n}\n\nbool serviceWifiAutoConnect() {\n  if (!initialized) initializeWifiAutoConnect();\n\n  const unsigned long now = millis();\n  if (!deadlineReached(now, nextStatusPollAt)) return false;\n  nextStatusPollAt = now + (wasConnected ? kConnectedPollMs : kConnectingPollMs);\n\n  const bool connected = WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);\n  bool changed = connected != wasConnected;\n  if (suspended) {\n    // Keep the header indicator truthful while the foreground activity owns\n    // the radio, but never issue a competing reconnect attempt.\n    wasConnected = connected;\n    return changed;\n  }\n  if (connected) {\n    if (!wasConnected) {\n      LOG_INF("WIFI", "Kobo saved network connected: ssid=%s ip=%s rssi=%d", WiFi.SSID().c_str(),\n              WiFi.localIP().toString().c_str(), WiFi.RSSI());\n    }\n    wasConnected = true;\n    retryDelayMs = kInitialRetryMs;\n    nextStatusPollAt = now + kConnectedPollMs;\n    return changed;\n  }\n\n  if (wasConnected) {\n    LOG_INF("WIFI", "Kobo saved network disconnected; reconnecting when due");\n    wasConnected = false;\n    nextAttemptAt = 0;\n  }\n\n  const std::string& lastSsid = WIFI_STORE.getLastConnectedSsid();\n  const WifiCredential* credential = lastSsid.empty() ? nullptr : WIFI_STORE.findCredential(lastSsid);\n  if (credential == nullptr) {\n    configuredSsid.clear();\n    retryDelayMs = kInitialRetryMs;\n    return changed;\n  }\n\n  if (configuredSsid != credential->ssid) {\n    configuredSsid.clear();\n    retryDelayMs = kInitialRetryMs;\n    nextAttemptAt = 0;\n  }\n  if (deadlineReached(now, nextAttemptAt)) startSavedNetwork(*credential);\n  return changed;\n}\n\nvoid setWifiAutoConnectSuspended(const bool requested) {\n  if (suspended == requested) return;\n  suspended = requested;\n  if (!suspended) {\n    // The selection activity may have saved, forgotten or replaced the last\n    // network. Re-read its in-memory store on the next main-loop tick.\n    configuredSsid.clear();\n    retryDelayMs = kInitialRetryMs;\n    nextAttemptAt = 0;\n    nextStatusPollAt = 0;\n  }\n  LOG_INF("WIFI", "Kobo saved-network auto-connect %s", suspended ? "suspended for UI" : "resumed");\n}\n\n}  // namespace crossink::kobo\n'
CI_YML = 'name: CI (build)\n\non:\n  push:\n    branches: [main]\n  pull_request:\n\npermissions:\n  contents: read\n\njobs:\n  clang-format:\n    runs-on: ubuntu-latest\n    steps:\n      - uses: actions/checkout@v6\n        with:\n          submodules: recursive\n\n      - uses: actions/setup-python@v6\n        with:\n          python-version: "3.14"\n\n      - name: Install clang-format-21\n        run: |\n          wget https://apt.llvm.org/llvm.sh\n          chmod +x llvm.sh\n          sudo ./llvm.sh 21\n          sudo apt-get update\n          sudo apt-get install -y clang-format-21\n\n      - name: Run clang-format\n        run: |\n          PATH="/usr/lib/llvm-21/bin:$PATH" ./bin/clang-format-fix\n          git diff --exit-code || (echo "Please run \'bin/clang-format-fix\' to fix formatting issues" && exit 1)\n\n  cppcheck:\n    runs-on: ubuntu-latest\n    steps:\n      - uses: actions/checkout@v6\n        with:\n          submodules: recursive\n\n      - uses: actions/setup-python@v6\n        with:\n          python-version: "3.14"\n\n      - name: Install uv\n        uses: astral-sh/setup-uv@v7\n        with:\n          version: "latest"\n          enable-cache: true\n\n      - name: Cache PlatformIO packages\n        uses: actions/cache@v4\n        with:\n          path: ~/.platformio\n          key: ${{ runner.os }}-platformio-${{ hashFiles(\'platformio.ini\', \'platformio.local.example.ini\') }}\n          restore-keys: |\n            ${{ runner.os }}-platformio-\n\n      - name: Install PlatformIO Core\n        run: uv pip install --system -U https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip\n\n      - name: Run cppcheck\n        run: pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high\n\n  kobo-host-tests:\n    runs-on: ubuntu-latest\n    steps:\n      - uses: actions/checkout@v6\n        with:\n          submodules: recursive\n\n      - name: Install native build tools\n        run: |\n          sudo apt-get update\n          sudo apt-get install -y cmake ninja-build g++\n\n      - name: Configure Kobo host tests\n        run: |\n          cmake -S platform/kobo -B build/kobo-host -G Ninja \\\n            -DBUILD_TESTING=ON \\\n            -DKOBO_BUILD_DISPLAY=OFF \\\n            -DCROSSINK_ROOT="$GITHUB_WORKSPACE"\n\n      - name: Build Kobo host tests\n        run: cmake --build build/kobo-host --parallel\n\n      - name: Run Kobo host tests\n        run: ctest --test-dir build/kobo-host --output-on-failure\n\n  build:\n    # Never run untrusted fork PR code on the self-hosted runner.\n    if: github.event_name != \'pull_request\' || github.event.pull_request.head.repo.full_name == github.repository\n    runs-on: [self-hosted, linux, crossink-build]\n    steps:\n      - uses: actions/checkout@v6\n        with:\n          submodules: recursive\n\n      - name: Verify PlatformIO Core\n        run: pio --version\n\n      - name: Build CrossInk tiny\n        run: |\n          set -euo pipefail\n          pio run -e tiny | tee pio.log\n\n      - name: Extract firmware stats\n        run: |\n          set -euo pipefail\n          {\n            echo "## Firmware build stats"\n            grep -E "RAM:\\s|Flash:\\s" pio.log | while read -r line; do echo "- ${line}"; done\n          } >> "$GITHUB_STEP_SUMMARY"\n\n      - name: Upload firmware artifacts\n        uses: actions/upload-artifact@v7\n        with:\n          name: firmware-tiny\n          path: |\n            .pio/build/tiny/firmware-tiny.bin\n          if-no-files-found: error\n\n  # This job is used as the PR required actions check, allows for changes to other steps in the future without breaking\n  # PR requirements.\n  test-status:\n    name: Test Status\n    needs:\n      - build\n      - clang-format\n      - cppcheck\n      - kobo-host-tests\n    if: always()\n    runs-on: ubuntu-latest\n    steps:\n      - name: Fail because needed jobs failed\n        # Fail if any job failed or was cancelled (skipped jobs are ok)\n        if: ${{ contains(needs.*.result, \'failure\') || contains(needs.*.result, \'cancelled\') }}\n        run: exit 1\n      - name: Success\n        run: exit 0\n'


class ApplyError(RuntimeError):
    pass


@dataclass(frozen=True)
class Operation:
    path: str
    description: str
    transform: Callable[[str], str]
    allow_missing: bool = False


def run(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, check=check, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def repository_root() -> Path:
    result = run("git", "rev-parse", "--show-toplevel")
    return Path(result.stdout.strip())


def ensure_baseline(root: Path) -> None:
    result = run("git", "merge-base", "--is-ancestor", BASELINE, "HEAD", check=False)
    if result.returncode != 0:
        raise ApplyError(f"Baseline {BASELINE} is not an ancestor of HEAD")


def replace_once(old: str, new: str, present_marker: str | None = None) -> Callable[[str], str]:
    def transform(content: str) -> str:
        # Prefer an exact, local match. Markers are only an idempotency fallback
        # after a later phase has deliberately refined this fragment; a marker
        # elsewhere in the same large file must never suppress the first apply.
        count = content.count(old)
        if count == 1:
            index = content.index(old)
            if content.startswith(new, index):
                return content
            if present_marker is not None:
                start = max(0, index - 160)
                end = min(len(content), index + max(len(old), len(new)) + 320)
                if present_marker in content[start:end]:
                    return content
            return content[:index] + new + content[index + len(old):]
        if count == 0:
            if new in content or (present_marker is not None and present_marker in content):
                return content
        raise ApplyError(f"Expected source fragment exactly once, found {count}")

    return transform


def insert_before_once(anchor: str, insertion: str, present_marker: str) -> Callable[[str], str]:
    def transform(content: str) -> str:
        if present_marker in content:
            return content
        count = content.count(anchor)
        if count != 1:
            raise ApplyError(f"Expected insertion anchor exactly once, found {count}")
        index = content.index(anchor)
        return content[:index] + insertion + content[index:]

    return transform


def replace_entire(prepared: str) -> Callable[[str], str]:
    def transform(content: str) -> str:
        return content if content == prepared else prepared

    return transform


def create_or_replace(prepared: str) -> Callable[[str], str]:
    # New tracked files are represented by an empty synthetic/current input.
    # On a real repository an existing different file is deliberately replaced
    # only after baseline and diff review, matching replace_entire semantics.
    return replace_entire(prepared)


def touch_registry_operations() -> list[Operation]:
    render_old = '''    RenderLock lock;
    if (currentActivity) {
      HalPowerManager::Lock powerLock;  // Ensure we don't go into low-power mode while rendering
#ifdef KOBO_LINUX
      TOUCH_UI.clear();
#endif
      currentActivity->render(std::move(lock));
    }
'''
    render_new = '''    RenderLock lock;
    if (currentActivity) {
      HalPowerManager::Lock powerLock;  // Ensure we don't go into low-power mode while rendering
#ifdef KOBO_LINUX
      TOUCH_UI.beginFrame();
#endif
      currentActivity->render(std::move(lock));
#ifdef KOBO_LINUX
      TOUCH_UI.commitFrame();
#endif
    } else {
#ifdef KOBO_LINUX
      TOUCH_UI.invalidate();
#endif
    }
'''
    pending_old = '''  while (pendingAction != PendingAction::None) {
'''
    pending_new = '''  while (pendingAction != PendingAction::None) {
#ifdef KOBO_LINUX
    // Disable the outgoing frame before mutating the activity stack. The old
    // e-ink pixels may remain visible briefly, but they must no longer accept
    // input while a new activity is being entered.
    TOUCH_UI.invalidate();
#endif
'''
    consume_old = '''bool MappedInputManager::consumeTouchTarget(TouchTarget& target) {
  if (!injectedTouchTargetAvailable) return false;
  target = injectedTouchTarget;
  injectedTouchTargetAvailable = false;
  // A direct target represents one rendered visual state. Once an activity
  // acts on it, invalidate every remaining hitbox immediately rather than
  // waiting for the asynchronous e-ink render to begin. This closes the
  // interval in which a rapid second tap could resolve an old screen.
  TOUCH_UI.clear();
  return true;
}

bool MappedInputManager::consumeNavigationTouchTarget(int& targetIndex, int& currentIndex) {
  if (!injectedTouchTargetAvailable ||
      injectedTouchTarget.kind != static_cast<unsigned char>(TouchUiRegistry::TargetKind::NavigationItem)) return false;
  targetIndex = injectedTouchTarget.primary;
  currentIndex = injectedTouchTarget.secondary;
  injectedTouchTargetAvailable = false;
  // See consumeTouchTarget(): no second activation may use a previous render.
  TOUCH_UI.clear();
  return true;
}
'''
    consume_new = '''bool MappedInputManager::consumeTouchTarget(TouchTarget& target) {
  if (!injectedTouchTargetAvailable) return false;
  if (!TOUCH_UI.isActiveGeneration(injectedTouchTarget.generation)) {
    injectedTouchTargetAvailable = false;
    return false;
  }
  target = injectedTouchTarget;
  injectedTouchTargetAvailable = false;
  // A direct target represents one committed visual state. Once an activity
  // acts on it, invalidate the active frame without touching a render thread's
  // staging frame.
  TOUCH_UI.invalidate();
  return true;
}

bool MappedInputManager::consumeNavigationTouchTarget(int& targetIndex, int& currentIndex) {
  if (!injectedTouchTargetAvailable ||
      injectedTouchTarget.kind != static_cast<unsigned char>(TouchUiRegistry::TargetKind::NavigationItem)) return false;
  if (!TOUCH_UI.isActiveGeneration(injectedTouchTarget.generation)) {
    injectedTouchTargetAvailable = false;
    return false;
  }
  targetIndex = injectedTouchTarget.primary;
  currentIndex = injectedTouchTarget.secondary;
  injectedTouchTargetAvailable = false;
  // See consumeTouchTarget(): no second activation may use a previous frame.
  TOUCH_UI.invalidate();
  return true;
}
'''
    return [
        Operation("src/components/TouchUiRegistry.h", "atomic registry header", replace_entire(TOUCH_UI_REGISTRY_H)),
        Operation("src/components/TouchUiRegistry.cpp", "atomic registry implementation", replace_entire(TOUCH_UI_REGISTRY_CPP)),
        Operation("src/activities/ActivityManager.cpp", "publish touch frame atomically", replace_once(render_old, render_new, "TOUCH_UI.beginFrame();")),
        Operation("src/activities/ActivityManager.cpp", "invalidate activity transitions", replace_once(pending_old, pending_new, "Disable the outgoing frame before mutating")),
        Operation("src/MappedInputManager.cpp", "enforce target generation", replace_once(consume_old, consume_new, "A direct target represents one committed visual state")),
        Operation("platform/kobo/tests/touch-ui-registry-test.cpp", "registry transaction tests", replace_entire(TOUCH_REGISTRY_TEST)),
    ]


def touch_gesture_operations() -> list[Operation]:
    header_old = '''  struct TouchTarget {
    unsigned char kind = 0;
    int primary = 0;
    int secondary = 0;
    std::uint32_t generation = 0;
    int x = -1;
    int y = -1;
  };
'''
    header_new = '''  enum class TouchTargetGesture : std::uint8_t { Tap, LongPress };

  struct TouchTarget {
    unsigned char kind = 0;
    int primary = 0;
    int secondary = 0;
    std::uint32_t generation = 0;
    int x = -1;
    int y = -1;
    TouchTargetGesture gesture = TouchTargetGesture::Tap;
  };
'''
    sig_old = '''  void injectTouchTarget(unsigned char kind, int primary, int secondary, std::uint32_t generation, int x = -1,
                         int y = -1);
  bool consumeTouchTarget(TouchTarget& target);
  // A list row published by TouchUiRegistry.  Kobo activities consume this
  // directly instead of replaying X4-style Up/Down presses one frame at a
  // time.  `currentIndex` is retained only for the temporary legacy fallback.
  bool consumeNavigationTouchTarget(int& targetIndex, int& currentIndex);
'''
    sig_new = '''  void injectTouchTarget(unsigned char kind, int primary, int secondary, std::uint32_t generation, int x = -1,
                         int y = -1, TouchTargetGesture gesture = TouchTargetGesture::Tap);
  bool consumeTouchTarget(TouchTarget& target);
  // A list row published by TouchUiRegistry. Kobo activities consume this
  // directly instead of replaying X4-style Up/Down presses one frame at a
  // time. The optional gesture preserves native long-press semantics.
  bool consumeNavigationTouchTarget(int& targetIndex, int& currentIndex, TouchTargetGesture* gesture = nullptr);
'''
    impl_old = '''void MappedInputManager::injectTouchTarget(const unsigned char kind, const int primary, const int secondary,
                                           const std::uint32_t generation, const int x, const int y) {
  injectedTouchTarget = {kind, primary, secondary, generation, x, y};
  injectedTouchTargetAvailable = true;
}
'''
    impl_new = '''void MappedInputManager::injectTouchTarget(const unsigned char kind, const int primary, const int secondary,
                                           const std::uint32_t generation, const int x, const int y,
                                           const TouchTargetGesture gesture) {
  injectedTouchTarget = {kind, primary, secondary, generation, x, y, gesture};
  injectedTouchTargetAvailable = true;
}
'''
    nav_sig_old = '''bool MappedInputManager::consumeNavigationTouchTarget(int& targetIndex, int& currentIndex) {
  if (!injectedTouchTargetAvailable ||
      injectedTouchTarget.kind != static_cast<unsigned char>(TouchUiRegistry::TargetKind::NavigationItem)) return false;
  if (!TOUCH_UI.isActiveGeneration(injectedTouchTarget.generation)) {
    injectedTouchTargetAvailable = false;
    return false;
  }
  targetIndex = injectedTouchTarget.primary;
  currentIndex = injectedTouchTarget.secondary;
  injectedTouchTargetAvailable = false;
  // See consumeTouchTarget(): no second activation may use a previous frame.
  TOUCH_UI.invalidate();
  return true;
}
'''
    nav_sig_new = '''bool MappedInputManager::consumeNavigationTouchTarget(int& targetIndex, int& currentIndex,
                                                           TouchTargetGesture* const gesture) {
  if (!injectedTouchTargetAvailable ||
      injectedTouchTarget.kind != static_cast<unsigned char>(TouchUiRegistry::TargetKind::NavigationItem)) return false;
  if (!TOUCH_UI.isActiveGeneration(injectedTouchTarget.generation)) {
    injectedTouchTargetAvailable = false;
    return false;
  }
  targetIndex = injectedTouchTarget.primary;
  currentIndex = injectedTouchTarget.secondary;
  if (gesture != nullptr) *gesture = injectedTouchTarget.gesture;
  injectedTouchTargetAvailable = false;
  // See consumeTouchTarget(): no second activation may use a previous frame.
  TOUCH_UI.invalidate();
  return true;
}
'''
    globals_old = '''crossink::kobo::TouchFrame lastTouchFrame{};
bool haveTouchFrame = false;
crossink::kobo::KoboFrontlightSysfs frontlight;
'''
    globals_new = '''crossink::kobo::TouchFrame lastTouchFrame{};
bool haveTouchFrame = false;
struct TouchCaptureState {
  bool active = false;
  bool longPressDelivered = false;
  TouchUiRegistry::Resolution target{};
};
TouchCaptureState touchCapture;
crossink::kobo::KoboFrontlightSysfs frontlight;
'''
    dispatch_old = '''void dispatch(const crossink::kobo::TouchDispatch event) {
  if (event.action == crossink::kobo::TouchAction::None) return;

  // A renderer-published hitbox is the authoritative action for the pixels
  // the user touched. In particular, Home places Settings in the left half
  // of the permanent bottom frame; the generic gesture mapper calls that
  // half Back. Resolve the visual target first so a visible button can never
  // be shadowed by the X4-compatible Back/Confirm fallback.
  const auto visualTarget = TOUCH_UI.resolve(event.point.x, event.point.y);
  if (visualTarget.found) {
    if (event.release) {
      std::fprintf(stderr, "[KOBO] touch resolve x=%d y=%d found=1 kind=%u current=%d target=%d count=%d\\n",
                   event.point.x, event.point.y, static_cast<unsigned int>(visualTarget.kind),
                   visualTarget.currentIndex, visualTarget.targetIndex, visualTarget.itemCount);
      mappedInputManager.injectTouchTarget(static_cast<unsigned char>(visualTarget.kind), visualTarget.targetIndex,
                                           visualTarget.kind == TouchUiRegistry::TargetKind::NavigationItem
                                               ? visualTarget.currentIndex
                                               : visualTarget.secondaryTarget,
                                           visualTarget.generation, event.point.x, event.point.y);
    }
    return;
  }

  if (event.action == crossink::kobo::TouchAction::UiItem) {
    if (event.release) {
      std::fprintf(stderr, "[KOBO] touch resolve x=%d y=%d found=0\\n", event.point.x, event.point.y);
    }
    return;
  }
  const auto button = mappedButton(event.action);
  if (event.press) mappedInputManager.injectPress(button);
  if (event.release) mappedInputManager.injectRelease(button);
}
'''
    dispatch_new = '''void dispatch(const crossink::kobo::TouchDispatch event) {
  using Gesture = crossink::kobo::TouchGesture;
  using TargetGesture = MappedInputManager::TouchTargetGesture;

  if (event.gesture == Gesture::None && event.action == crossink::kobo::TouchAction::None) return;

  if (event.gesture == Gesture::Start) {
    touchCapture.active = true;
    touchCapture.longPressDelivered = false;
    touchCapture.target = TOUCH_UI.resolve(event.point.x, event.point.y);
    return;
  }
  if (event.gesture == Gesture::Cancelled) {
    touchCapture = {};
    return;
  }
  if (event.gesture == Gesture::Swipe) {
    // A swipe is navigation, never activation of the hitbox where it began.
    touchCapture = {};
  } else if (event.gesture == Gesture::Tap) {
    if (touchCapture.active && touchCapture.target.found) {
      const auto target = touchCapture.target;
      touchCapture = {};
      if (TOUCH_UI.isActiveGeneration(target.generation)) {
        mappedInputManager.injectTouchTarget(
            static_cast<unsigned char>(target.kind), target.targetIndex,
            target.kind == TouchUiRegistry::TargetKind::NavigationItem ? target.currentIndex : target.secondaryTarget,
            target.generation, event.point.x, event.point.y, TargetGesture::Tap);
      }
      return;
    }
    touchCapture = {};
  } else if (event.gesture == Gesture::LongPressStart) {
    if (touchCapture.active && touchCapture.target.found) {
      const auto target = touchCapture.target;
      if (TOUCH_UI.isActiveGeneration(target.generation)) {
        mappedInputManager.injectTouchTarget(
            static_cast<unsigned char>(target.kind), target.targetIndex,
            target.kind == TouchUiRegistry::TargetKind::NavigationItem ? target.currentIndex : target.secondaryTarget,
            target.generation, event.point.x, event.point.y, TargetGesture::LongPress);
      }
      touchCapture.longPressDelivered = true;
      return;
    }
  } else if (event.gesture == Gesture::LongPressEnd) {
    if (touchCapture.longPressDelivered || (touchCapture.active && touchCapture.target.found)) {
      touchCapture = {};
      return;
    }
    touchCapture = {};
  }

  if (event.action == crossink::kobo::TouchAction::None ||
      event.action == crossink::kobo::TouchAction::UiItem) {
    return;
  }
  const auto button = mappedButton(event.action);
  if (event.press) mappedInputManager.injectPress(button);
  if (event.release) mappedInputManager.injectRelease(button);
}
'''
    orientation_old = '''  if (appliedOrientation != SETTINGS.orientation) {
    touch.setOrientation(screenOrientation());
    appliedOrientation = SETTINGS.orientation;
  }
'''
    orientation_new = '''  if (appliedOrientation != SETTINGS.orientation) {
    // An orientation switch cancels every injected edge and held gesture.
    touch.setOrientation(screenOrientation());
    gestures.reset();
    touchCapture = {};
    lastTouchFrame = {};
    haveTouchFrame = false;
    appliedOrientation = SETTINGS.orientation;
  }
'''
    recent_old = '''  if (!navigationOverlayOpen) {
    consumeDirectListSelection(mappedInput, static_cast<int>(recentBooks.size()), selectorIndex);
  }
'''
    recent_new = '''  if (!navigationOverlayOpen) {
    const auto directTouch = consumeDirectListEvent(mappedInput, static_cast<int>(recentBooks.size()), selectorIndex);
    if (directTouch == DirectListTouchKind::LongPress) {
      if (!recentBooks.empty() && selectorIndex >= 0 && selectorIndex < static_cast<int>(recentBooks.size())) {
        showBookActionMenu(selectorIndex, true);
      }
      return;
    }
    if (directTouch == DirectListTouchKind::Tap) {
      mappedInput.injectPress(MappedInputManager::Button::Confirm);
      mappedInput.injectRelease(MappedInputManager::Button::Confirm);
    }
  }
'''
    return [
        Operation("platform/kobo/input/KoboTouchGesture.h", "semantic gesture API", replace_entire(TOUCH_GESTURE_H)),
        Operation("platform/kobo/input/KoboTouchGesture.cpp", "semantic gesture implementation", replace_entire(TOUCH_GESTURE_CPP)),
        Operation("src/MappedInputManager.h", "touch target gesture field", replace_once(header_old, header_new, "enum class TouchTargetGesture")),
        Operation("src/MappedInputManager.h", "touch target gesture signatures", replace_once(sig_old, sig_new, "bool consumeNavigationTouchTarget(int& targetIndex, int& currentIndex, TouchTargetGesture* gesture = nullptr);")),
        Operation("src/MappedInputManager.cpp", "inject semantic target", replace_once(impl_old, impl_new, "const TouchTargetGesture gesture")),
        Operation("src/MappedInputManager.cpp", "consume semantic navigation target", replace_once(nav_sig_old, nav_sig_new, "TouchTargetGesture* const gesture")),
        Operation("src/components/DirectListTouch.h", "preserve native long press", replace_entire(DIRECT_LIST_TOUCH_H)),
        Operation("platform/kobo/app/kobo-main.cpp", "add touch capture state", replace_once(globals_old, globals_new, "KoboSemanticInputQueue touchInputQueue")),
        Operation("platform/kobo/app/kobo-main.cpp", "route captures taps long press and swipes", replace_once(dispatch_old, dispatch_new, "void queueTouchDispatch")),
        Operation("platform/kobo/app/kobo-main.cpp", "reset capture on orientation change", replace_once(orientation_old, orientation_new, "Orientation changes invalidate both capture")),
        Operation("src/activities/home/RecentBooksGridActivity.cpp", "native book-card long press", replace_once(recent_old, recent_new, "const auto directTouch = consumeDirectListEvent")),
        Operation("platform/kobo/tests/touch-gesture-test.cpp", "gesture boundary tests", replace_entire(TOUCH_GESTURE_TEST)),
    ]



def input_routing_operations() -> list[Operation]:
    activity_include_old = '''#include <atomic>\n#include <cassert>\n'''
    activity_include_new = '''#include <atomic>\n#include <cassert>\n#include <cstdint>\n'''
    include_old = '''#include <KoboTouchGesture.h>\n'''
    include_new = '''#include <KoboTouchGesture.h>\n#include "KoboTouchRouter.h"\n'''
    cmake_app_old = '''\t"${CMAKE_CURRENT_SOURCE_DIR}/kobo-main.cpp"\n\t"${CROSSINK_ROOT}/platform/kobo/compat/WiFi.cpp"\n'''
    cmake_app_new = '''\t"${CMAKE_CURRENT_SOURCE_DIR}/kobo-main.cpp"\n\t"${CMAKE_CURRENT_SOURCE_DIR}/KoboTouchRouter.cpp"\n\t"${CROSSINK_ROOT}/platform/kobo/compat/WiFi.cpp"\n'''
    activity_field_old = '''  std::atomic<bool> requestedUpdate{false};\n\n public:\n'''
    activity_field_new = '''  std::atomic<bool> requestedUpdate{false};\n  // Incremented as soon as an activity transition is requested. Input that\n  // began on the outgoing screen is rejected even before the new activity is\n  // installed on the next loop.\n  std::atomic<std::uint64_t> activityGeneration{0};\n\n public:\n'''
    activity_api_old = '''  bool skipLoopDelay() const;\n  std::string getCurrentBookPath() const;\n'''
    activity_api_new = '''  bool skipLoopDelay() const;\n  [[nodiscard]] std::uint64_t currentActivityGeneration() const {\n    return activityGeneration.load(std::memory_order_acquire);\n  }\n  [[nodiscard]] bool acceptsInput() const { return pendingAction == PendingAction::None; }\n  std::string getCurrentBookPath() const;\n'''
    replace_start_old = '''void ActivityManager::replaceActivity(std::unique_ptr<Activity>&& newActivity) {\n  // Note: no lock here, this is usually called by loop() and we may run into deadlock\n'''
    replace_start_new = '''void ActivityManager::replaceActivity(std::unique_ptr<Activity>&& newActivity) {\n#ifdef KOBO_LINUX\n  TOUCH_UI.invalidate();\n  activityGeneration.fetch_add(1, std::memory_order_release);\n#endif\n  // Note: no lock here, this is usually called by loop() and we may run into deadlock\n'''
    push_start_old = '''void ActivityManager::pushActivity(std::unique_ptr<Activity>&& activity) {\n  if (pendingActivity) {\n'''
    push_start_new = '''void ActivityManager::pushActivity(std::unique_ptr<Activity>&& activity) {\n#ifdef KOBO_LINUX\n  // A pushed activity makes every queued event from the outgoing screen stale.\n  TOUCH_UI.invalidate();\n  activityGeneration.fetch_add(1, std::memory_order_release);\n#endif\n  if (pendingActivity) {\n'''
    pop_start_old = '''void ActivityManager::popActivity() {\n  if (pendingActivity) {\n'''
    pop_start_new = '''void ActivityManager::popActivity() {\n#ifdef KOBO_LINUX\n  // A popped activity changes the input owner before the stack is mutated.\n  TOUCH_UI.invalidate();\n  activityGeneration.fetch_add(1, std::memory_order_release);\n#endif\n  if (pendingActivity) {\n'''
    globals_old = '''struct TouchCaptureState {\n  bool active = false;\n  bool longPressDelivered = false;\n  TouchUiRegistry::Resolution target{};\n};\nTouchCaptureState touchCapture;\n'''
    globals_new = '''crossink::kobo::KoboTouchRouter touchRouter;\ncrossink::kobo::KoboSemanticInputQueue touchInputQueue;\n'''
    dispatch_old = '''void dispatch(const crossink::kobo::TouchDispatch event) {\n  using Gesture = crossink::kobo::TouchGesture;\n  using TargetGesture = MappedInputManager::TouchTargetGesture;\n\n  if (event.gesture == Gesture::None && event.action == crossink::kobo::TouchAction::None) return;\n\n  if (event.gesture == Gesture::Start) {\n    touchCapture.active = true;\n    touchCapture.longPressDelivered = false;\n    touchCapture.target = TOUCH_UI.resolve(event.point.x, event.point.y);\n    return;\n  }\n  if (event.gesture == Gesture::Cancelled) {\n    touchCapture = {};\n    return;\n  }\n  if (event.gesture == Gesture::Swipe) {\n    // A swipe is navigation, never activation of the hitbox where it began.\n    touchCapture = {};\n  } else if (event.gesture == Gesture::Tap) {\n    if (touchCapture.active && touchCapture.target.found) {\n      const auto target = touchCapture.target;\n      touchCapture = {};\n      if (TOUCH_UI.isActiveGeneration(target.generation)) {\n        mappedInputManager.injectTouchTarget(\n            static_cast<unsigned char>(target.kind), target.targetIndex,\n            target.kind == TouchUiRegistry::TargetKind::NavigationItem ? target.currentIndex : target.secondaryTarget,\n            target.generation, event.point.x, event.point.y, TargetGesture::Tap);\n      }\n      return;\n    }\n    touchCapture = {};\n  } else if (event.gesture == Gesture::LongPressStart) {\n    if (touchCapture.active && touchCapture.target.found) {\n      const auto target = touchCapture.target;\n      if (TOUCH_UI.isActiveGeneration(target.generation)) {\n        mappedInputManager.injectTouchTarget(\n            static_cast<unsigned char>(target.kind), target.targetIndex,\n            target.kind == TouchUiRegistry::TargetKind::NavigationItem ? target.currentIndex : target.secondaryTarget,\n            target.generation, event.point.x, event.point.y, TargetGesture::LongPress);\n      }\n      touchCapture.longPressDelivered = true;\n      return;\n    }\n  } else if (event.gesture == Gesture::LongPressEnd) {\n    if (touchCapture.longPressDelivered || (touchCapture.active && touchCapture.target.found)) {\n      touchCapture = {};\n      return;\n    }\n    touchCapture = {};\n  }\n\n  if (event.action == crossink::kobo::TouchAction::None ||\n      event.action == crossink::kobo::TouchAction::UiItem) {\n    return;\n  }\n  const auto button = mappedButton(event.action);\n  if (event.press) mappedInputManager.injectPress(button);\n  if (event.release) mappedInputManager.injectRelease(button);\n}\n'''
    dispatch_new = '''void queueTouchDispatch(const crossink::kobo::TouchDispatch& event) {\n  if (!activityManager.acceptsInput()) return;\n  const auto routed = touchRouter.route(event, TOUCH_UI, activityManager.currentActivityGeneration());\n  (void)touchInputQueue.push(routed);\n}\n\nbool dispatchOneQueuedTouch() {\n  crossink::kobo::RoutedTouchInput routed;\n  if (!touchInputQueue.pop(routed)) return false;\n  if (!activityManager.acceptsInput() ||\n      routed.activityGeneration != activityManager.currentActivityGeneration()) {\n    return true;\n  }\n\n  if (routed.kind == crossink::kobo::RoutedTouchKind::Target) {\n    if (!TOUCH_UI.isActiveGeneration(routed.target.generation)) return true;\n    const auto gesture = routed.targetGesture == crossink::kobo::RoutedTargetGesture::LongPress\n                             ? MappedInputManager::TouchTargetGesture::LongPress\n                             : MappedInputManager::TouchTargetGesture::Tap;\n    mappedInputManager.injectTouchTarget(\n        static_cast<unsigned char>(routed.target.kind), routed.target.targetIndex,\n        routed.target.kind == TouchUiRegistry::TargetKind::NavigationItem ? routed.target.currentIndex\n                                                                          : routed.target.secondaryTarget,\n        routed.target.generation, routed.point.x, routed.point.y, gesture);\n    return true;\n  }\n  if (routed.kind != crossink::kobo::RoutedTouchKind::Action) return true;\n  const auto button = mappedButton(routed.action);\n  if (routed.press) mappedInputManager.injectPress(button);\n  if (routed.release) mappedInputManager.injectRelease(button);\n  return true;\n}\n\nvoid reportTouchQueueOverflow() {\n  const std::uint32_t dropped = touchInputQueue.takeDroppedCount();\n  if (dropped != 0) {\n    std::fprintf(stderr, "[KOBO] semantic touch queue overflow dropped=%u\\n", dropped);\n  }\n}\n'''
    dev_tap_old = '''    dispatch(gestures.update({{x, y}, true, true, timestamp, {x, y}}, context, screenWidth, screenHeight));\n    dispatch(gestures.update({{x, y}, false, false, timestamp + 1, {x, y}}, context, screenWidth, screenHeight));\n'''
    dev_tap_new = '''    queueTouchDispatch(\n        gestures.update({{x, y}, true, true, timestamp, {x, y}}, context, screenWidth, screenHeight));\n    queueTouchDispatch(\n        gestures.update({{x, y}, false, false, timestamp + 1, {x, y}}, context, screenWidth, screenHeight));\n'''
    reset_old = '''  gestures.reset();\n  touchCapture = {};\n  lastTouchFrame = {};\n  haveTouchFrame = false;\n  suppressTouchUntilAllUp = false;\n  mappedInputManager.resetInjectedInput();\n'''
    reset_new = '''  // Full input-pipeline reset: no queued or held event survives.\n  gestures.reset();\n  touchRouter.reset();\n  touchInputQueue.clear();\n  lastTouchFrame = {};\n  haveTouchFrame = false;\n  suppressTouchUntilAllUp = false;\n  mappedInputManager.resetInjectedInput();\n'''
    open_old = '''  gestures.reset();\n  touchCapture = {};\n  lastTouchFrame = {};\n  haveTouchFrame = false;\n'''
    open_new = '''  // A re-open starts a fresh pointer/queue epoch.\n  gestures.reset();\n  touchRouter.reset();\n  touchInputQueue.clear();\n  lastTouchFrame = {};\n  haveTouchFrame = false;\n'''
    orientation_old = '''    gestures.reset();\n    touchCapture = {};\n    lastTouchFrame = {};\n    haveTouchFrame = false;\n    suppressTouchUntilAllUp = touch.isDown();\n    mappedInputManager.resetInjectedInput();\n'''
    orientation_new = '''    // Orientation changes invalidate both capture and queued semantics.\n    gestures.reset();\n    touchRouter.reset();\n    touchInputQueue.clear();\n    lastTouchFrame = {};\n    haveTouchFrame = false;\n    suppressTouchUntilAllUp = touch.isDown();\n    mappedInputManager.resetInjectedInput();\n'''
    read_old = '''  if (processDevInput(context, screenWidth, screenHeight)) return;\n  if (!ensureTouchDeviceOpen()) return;\n\n  crossink::kobo::TouchFrame frame{};\n  bool received = false;\n  while (true) {\n    const auto result = touch.readFrameDetailed(frame);\n    if (result == crossink::kobo::TouchReadResult::WouldBlock) break;\n    if (result == crossink::kobo::TouchReadResult::Interrupted) continue;\n    if (result == crossink::kobo::TouchReadResult::Resynchronized) {\n      // SYN_DROPPED means the gesture stream is discontinuous. Keep the UI,\n      // cancel pointer capture and ignore a contact that was already held.\n      received = true;\n      gestures.reset();\n      touchCapture = {};\n      lastTouchFrame = frame;\n      haveTouchFrame = true;\n      suppressTouchUntilAllUp = frame.down;\n      continue;\n    }\n    if (result == crossink::kobo::TouchReadResult::DeviceLost ||\n        result == crossink::kobo::TouchReadResult::ProtocolError) {\n      std::fprintf(stderr, "[KOBO] touch input lost/protocol error result=%u; reopening\\n",\n                   static_cast<unsigned int>(result));\n      resetTouchPipeline(true);\n      nextTouchDiscoveryAtMicros = monotonicMicros() + kTouchReconnectDelayMicros;\n      return;\n    }\n\n    received = true;\n    lastTouchFrame = frame;\n    haveTouchFrame = true;\n    if (suppressTouchUntilAllUp) {\n      if (!frame.down) suppressTouchUntilAllUp = false;\n      continue;\n    }\n    const auto touchEvent = gestures.update(frame, context, screenWidth, screenHeight);\n    dispatch(touchEvent);\n    // Process at most one completed semantic gesture per application frame.\n    // Remaining evdev records stay ordered in the kernel queue instead of\n    // overwriting a second direct target or collapsing two button pulses.\n    if (touchEvent.gesture != crossink::kobo::TouchGesture::None &&\n        touchEvent.gesture != crossink::kobo::TouchGesture::Start) {\n      break;\n    }\n  }\n  if (!received && haveTouchFrame && lastTouchFrame.down && !suppressTouchUntilAllUp) {\n    lastTouchFrame.timestampMicros = monotonicMicros();\n    lastTouchFrame.positionChanged = false;\n    dispatch(gestures.update(lastTouchFrame, context, screenWidth, screenHeight));\n  }\n}\n'''
    read_new = '''  const bool dispatchedBeforeRead = dispatchOneQueuedTouch();\n  reportTouchQueueOverflow();\n  if (processDevInput(context, screenWidth, screenHeight)) {\n    if (!dispatchedBeforeRead) (void)dispatchOneQueuedTouch();\n    reportTouchQueueOverflow();\n    return;\n  }\n  if (!ensureTouchDeviceOpen()) return;\n\n  crossink::kobo::TouchFrame frame{};\n  bool received = false;\n  while (true) {\n    const auto result = touch.readFrameDetailed(frame);\n    if (result == crossink::kobo::TouchReadResult::WouldBlock) break;\n    if (result == crossink::kobo::TouchReadResult::Interrupted) continue;\n    if (result == crossink::kobo::TouchReadResult::Resynchronized) {\n      // A discontinuous stream invalidates every incomplete and queued input.\n      received = true;\n      gestures.reset();\n      touchRouter.reset();\n      touchInputQueue.clear();\n      lastTouchFrame = frame;\n      haveTouchFrame = true;\n      suppressTouchUntilAllUp = frame.down;\n      continue;\n    }\n    if (result == crossink::kobo::TouchReadResult::DeviceLost ||\n        result == crossink::kobo::TouchReadResult::ProtocolError) {\n      std::fprintf(stderr, "[KOBO] touch input lost/protocol error result=%u; reopening\\n",\n                   static_cast<unsigned int>(result));\n      resetTouchPipeline(true);\n      nextTouchDiscoveryAtMicros = monotonicMicros() + kTouchReconnectDelayMicros;\n      return;\n    }\n\n    received = true;\n    lastTouchFrame = frame;\n    haveTouchFrame = true;\n    if (suppressTouchUntilAllUp) {\n      if (!frame.down) suppressTouchUntilAllUp = false;\n      continue;\n    }\n    queueTouchDispatch(gestures.update(frame, context, screenWidth, screenHeight));\n  }\n  if (!received && haveTouchFrame && lastTouchFrame.down && !suppressTouchUntilAllUp) {\n    lastTouchFrame.timestampMicros = monotonicMicros();\n    lastTouchFrame.positionChanged = false;\n    queueTouchDispatch(gestures.update(lastTouchFrame, context, screenWidth, screenHeight));\n  }\n  if (!dispatchedBeforeRead) (void)dispatchOneQueuedTouch();\n  reportTouchQueueOverflow();\n}\n'''
    cmake_test_old = '''\tadd_executable(crossink-kobo-evdev-key-test tests/evdev-key-test.cpp)\n'''
    cmake_test_new = '''\tadd_executable(crossink-kobo-touch-routing-e2e-test tests/touch-routing-e2e-test.cpp\n\t\tapp/KoboTouchRouter.cpp "${CMAKE_CURRENT_SOURCE_DIR}/../../src/components/TouchUiRegistry.cpp")\n\ttarget_include_directories(crossink-kobo-touch-routing-e2e-test PRIVATE\n\t\t"${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/app"\n\t\t"${CMAKE_CURRENT_SOURCE_DIR}/input" "${CMAKE_CURRENT_SOURCE_DIR}/../../src")\n\ttarget_compile_options(crossink-kobo-touch-routing-e2e-test PRIVATE -Wall -Wextra -Wpedantic -Werror)\n\ttarget_compile_definitions(crossink-kobo-touch-routing-e2e-test PRIVATE KOBO_LINUX)\n\ttarget_link_libraries(crossink-kobo-touch-routing-e2e-test PRIVATE crossink_kobo_input pthread)\n\tadd_test(NAME crossink-kobo-touch-routing-e2e COMMAND crossink-kobo-touch-routing-e2e-test)\n\n\tadd_executable(crossink-kobo-evdev-key-test tests/evdev-key-test.cpp)\n'''
    return [
        Operation("platform/kobo/app/KoboTouchRouter.h", "semantic routing and bounded queue API", create_or_replace(KOBO_TOUCH_ROUTER_H), True),
        Operation("platform/kobo/app/KoboTouchRouter.cpp", "semantic routing and bounded queue implementation", create_or_replace(KOBO_TOUCH_ROUTER_CPP), True),
        Operation("platform/kobo/tests/touch-routing-e2e-test.cpp", "raw evdev through fake activity routing E2E", create_or_replace(TOUCH_ROUTING_E2E_TEST), True),
        Operation("platform/kobo/app/CMakeLists.txt", "build touch router into Kobo app", replace_once(cmake_app_old, cmake_app_new, "KoboTouchRouter.cpp")),
        Operation("src/activities/ActivityManager.h", "include fixed-width generation type", replace_once(activity_include_old, activity_include_new, "#include <cstdint>")),
        Operation("platform/kobo/app/kobo-main.cpp", "include touch router", replace_once(include_old, include_new, "KoboTouchRouter.h")),
        Operation("src/activities/ActivityManager.h", "add activity generation and input gate", replace_once(activity_field_old, activity_field_new, "activityGeneration{0}")),
        Operation("src/activities/ActivityManager.h", "expose activity generation and input gate", replace_once(activity_api_old, activity_api_new, "currentActivityGeneration() const")),
        Operation("src/activities/ActivityManager.cpp", "invalidate input when replacement requested", replace_once(replace_start_old, replace_start_new, "activityGeneration.fetch_add")),
        Operation("src/activities/ActivityManager.cpp", "invalidate input when push requested", replace_once(push_start_old, push_start_new, "A pushed activity makes every queued event")),
        Operation("src/activities/ActivityManager.cpp", "invalidate input when pop requested", replace_once(pop_start_old, pop_start_new, "A popped activity changes the input owner")),
        Operation("platform/kobo/app/kobo-main.cpp", "replace ad-hoc pointer capture with router queue", replace_once(globals_old, globals_new, "KoboSemanticInputQueue touchInputQueue")),
        Operation("platform/kobo/app/kobo-main.cpp", "queue semantic touch events", replace_once(dispatch_old, dispatch_new, "semantic touch queue overflow")),
        Operation("platform/kobo/app/kobo-main.cpp", "queue development taps", replace_once(dev_tap_old, dev_tap_new, "queueTouchDispatch(")),
        Operation("platform/kobo/app/kobo-main.cpp", "clear router queue on pipeline reset", replace_once(reset_old, reset_new, "Full input-pipeline reset")),
        Operation("platform/kobo/app/kobo-main.cpp", "clear router queue after reconnect", replace_once(open_old, open_new, "A re-open starts a fresh pointer/queue epoch")),
        Operation("platform/kobo/app/kobo-main.cpp", "clear router queue on orientation", replace_once(orientation_old, orientation_new, "Orientation changes invalidate both capture")),
        Operation("platform/kobo/app/kobo-main.cpp", "drain bounded semantic queue in order", replace_once(read_old, read_new, "const bool dispatchedBeforeRead")),
        Operation("platform/kobo/CMakeLists.txt", "register touch routing E2E", replace_once(cmake_test_old, cmake_test_new, "crossink-kobo-touch-routing-e2e-test")),
    ]

def evdev_hardening_operations() -> list[Operation]:
    globals_old = '''TouchCaptureState touchCapture;
crossink::kobo::KoboFrontlightSysfs frontlight;
'''
    globals_new = '''TouchCaptureState touchCapture;
bool suppressTouchUntilAllUp = false;
bool touchMissingLogged = false;
std::uint64_t nextTouchDiscoveryAtMicros = 0;
constexpr std::uint64_t kTouchReconnectDelayMicros = 2'000'000ULL;
crossink::kobo::KoboFrontlightSysfs frontlight;
'''
    helper_old = '''void updateTouch() {
'''
    helper_new = '''void resetTouchPipeline(const bool closeDevice) {
  gestures.reset();
  touchCapture = {};
  lastTouchFrame = {};
  haveTouchFrame = false;
  suppressTouchUntilAllUp = false;
  mappedInputManager.resetInjectedInput();
  TOUCH_UI.invalidate();
  if (closeDevice) touch.close();
}

bool ensureTouchDeviceOpen() {
  if (touch.isOpen()) return true;
  const std::uint64_t now = monotonicMicros();
  if (nextTouchDiscoveryAtMicros != 0 && now < nextTouchDiscoveryAtMicros) return false;
  nextTouchDiscoveryAtMicros = now + kTouchReconnectDelayMicros;

  crossink::kobo::TouchDeviceInfo touchDevice;
  if (!crossink::kobo::KoboEvdevTouch::discover(touchDevice)) {
    if (!touchMissingLogged) {
      std::fprintf(stderr, "[KOBO] no absolute touch input discovered; retrying\\n");
      touchMissingLogged = true;
    }
    return false;
  }
  if (!touch.open(touchDevice)) {
    std::fprintf(stderr, "[KOBO] failed to open touch input %s; retrying\\n", touchDevice.path.c_str());
    return false;
  }

  touchMissingLogged = false;
  nextTouchDiscoveryAtMicros = 0;
  touch.setOrientation(screenOrientation());
  appliedOrientation = SETTINGS.orientation;
  gestures.reset();
  touchCapture = {};
  lastTouchFrame = {};
  haveTouchFrame = false;
  suppressTouchUntilAllUp = touch.isDown();
  std::fprintf(stderr, "[KOBO] touch input %s (%s), raw x=%d..%d y=%d..%d\\n",
               touchDevice.path.c_str(), touchDevice.name.c_str(), touchDevice.x.minimum, touchDevice.x.maximum,
               touchDevice.y.minimum, touchDevice.y.maximum);
  // A disconnect invalidates the prior hitbox frame. Republish the current
  // activity after reconnect before accepting a fresh tap.
  activityManager.requestUpdate();
  return true;
}

void updateTouch() {
'''
    read_old = '''  if (processDevInput(context, screenWidth, screenHeight)) return;
  crossink::kobo::TouchFrame frame{};
  bool received = false;
  while (touch.readFrame(frame)) {
    received = true;
    lastTouchFrame = frame;
    haveTouchFrame = true;
    dispatch(gestures.update(frame, context, screenWidth, screenHeight));
  }
  if (!received && haveTouchFrame && lastTouchFrame.down) {
    lastTouchFrame.timestampMicros = monotonicMicros();
    lastTouchFrame.positionChanged = false;
    dispatch(gestures.update(lastTouchFrame, context, screenWidth, screenHeight));
  }
}
'''
    read_new = '''  if (processDevInput(context, screenWidth, screenHeight)) return;
  if (!ensureTouchDeviceOpen()) return;

  crossink::kobo::TouchFrame frame{};
  bool received = false;
  while (true) {
    const auto result = touch.readFrameDetailed(frame);
    if (result == crossink::kobo::TouchReadResult::WouldBlock) break;
    if (result == crossink::kobo::TouchReadResult::Interrupted) continue;
    if (result == crossink::kobo::TouchReadResult::Resynchronized) {
      // SYN_DROPPED means the gesture stream is discontinuous. Keep the UI,
      // cancel pointer capture and ignore a contact that was already held.
      received = true;
      gestures.reset();
      touchCapture = {};
      lastTouchFrame = frame;
      haveTouchFrame = true;
      suppressTouchUntilAllUp = frame.down;
      continue;
    }
    if (result == crossink::kobo::TouchReadResult::DeviceLost ||
        result == crossink::kobo::TouchReadResult::ProtocolError) {
      std::fprintf(stderr, "[KOBO] touch input lost/protocol error result=%u; reopening\\n",
                   static_cast<unsigned int>(result));
      resetTouchPipeline(true);
      nextTouchDiscoveryAtMicros = monotonicMicros() + kTouchReconnectDelayMicros;
      return;
    }

    received = true;
    lastTouchFrame = frame;
    haveTouchFrame = true;
    if (suppressTouchUntilAllUp) {
      if (!frame.down) suppressTouchUntilAllUp = false;
      continue;
    }
    const auto touchEvent = gestures.update(frame, context, screenWidth, screenHeight);
    dispatch(touchEvent);
    // Process at most one completed semantic gesture per application frame.
    // Remaining evdev records stay ordered in the kernel queue instead of
    // overwriting a second direct target or collapsing two button pulses.
    if (touchEvent.gesture != crossink::kobo::TouchGesture::None &&
        touchEvent.gesture != crossink::kobo::TouchGesture::Start) {
      break;
    }
  }
  if (!received && haveTouchFrame && lastTouchFrame.down && !suppressTouchUntilAllUp) {
    lastTouchFrame.timestampMicros = monotonicMicros();
    lastTouchFrame.positionChanged = false;
    dispatch(gestures.update(lastTouchFrame, context, screenWidth, screenHeight));
  }
}
'''
    startup_old = '''  crossink::kobo::TouchDeviceInfo touchDevice;
  if (crossink::kobo::KoboEvdevTouch::discover(touchDevice)) {
    if (!touch.open(touchDevice)) {
      std::fprintf(stderr, "[KOBO] failed to open touch input %s\\n", touchDevice.path.c_str());
    } else {
      std::fprintf(stderr, "[KOBO] touch input %s (%s), raw x=%d..%d y=%d..%d\\n", touchDevice.path.c_str(),
                   touchDevice.name.c_str(), touchDevice.x.minimum, touchDevice.x.maximum, touchDevice.y.minimum,
                   touchDevice.y.maximum);
    }
  } else {
    std::fprintf(stderr, "[KOBO] no absolute touch input discovered\\n");
  }
'''
    startup_new = '''  (void)ensureTouchDeviceOpen();
'''
    wrapper_old = '''}  // namespace

int main() {
'''
    wrapper_new = '''}  // namespace

void resetKoboTouchInputForSuspend() {
  resetTouchPipeline(true);
  nextTouchDiscoveryAtMicros = 0;
}

int main() {
'''
    input_header_old = '''  void clearInjectedInputFrame();
  void injectTouchTarget(unsigned char kind, int primary, int secondary, std::uint32_t generation, int x = -1,
'''
    input_header_new = '''  void clearInjectedInputFrame();
  // Cancel all injected edges, held state and pending touch targets after an
  // evdev discontinuity, orientation change or suspend transition.
  void resetInjectedInput();
  void injectTouchTarget(unsigned char kind, int primary, int secondary, std::uint32_t generation, int x = -1,
'''
    input_impl_old = '''void MappedInputManager::clearInjectedInputFrame() {
  injectedPressed.fill(false);
  injectedReleased.fill(false);
  // Touch targets are one-frame messages.  Button edges and a direct target
  // are both injected before Activity::loop(); keeping an unconsumed target
  // for another frame could execute an action after the activity/modal that
  // published it has already changed.
  injectedTouchTargetAvailable = false;
}
'''
    input_impl_new = '''void MappedInputManager::clearInjectedInputFrame() {
  injectedPressed.fill(false);
  injectedReleased.fill(false);
  // Touch targets are one-frame messages. Button edges and a direct target
  // are both injected before Activity::loop(); keeping an unconsumed target
  // for another frame could execute an action after the activity/modal that
  // published it has already changed.
  injectedTouchTargetAvailable = false;
}

void MappedInputManager::resetInjectedInput() {
  injectedPressed.fill(false);
  injectedReleased.fill(false);
  injectedHeld.fill(false);
  injectedPressStart.fill(0);
  injectedTouchTarget = {};
  injectedTouchTargetAvailable = false;
}
'''
    orientation_old = '''  if (appliedOrientation != SETTINGS.orientation) {
    touch.setOrientation(screenOrientation());
    gestures.reset();
    touchCapture = {};
    lastTouchFrame = {};
    haveTouchFrame = false;
    appliedOrientation = SETTINGS.orientation;
  }
'''
    orientation_new = '''  if (appliedOrientation != SETTINGS.orientation) {
    touch.setOrientation(screenOrientation());
    gestures.reset();
    touchCapture = {};
    lastTouchFrame = {};
    haveTouchFrame = false;
    suppressTouchUntilAllUp = touch.isDown();
    mappedInputManager.resetInjectedInput();
    appliedOrientation = SETTINGS.orientation;
  }
'''
    main_decl_old = '''#include <WiFi.h>
#include <unistd.h>
#endif
'''
    main_decl_new = '''#include <WiFi.h>
#include <unistd.h>

void resetKoboTouchInputForSuspend();
#endif
'''
    suspend_old = '''  // Keep the DRM/FBInk handles alive across suspend. Closing them here forced
  // the old code to exec a fresh application on every wake, which looked like
  // a reboot even though the kernel had merely resumed from RAM.
  waitForPowerRelease();
'''
    suspend_new = '''  // Keep the DRM/FBInk handles alive across suspend. Close only evdev input:
  // a stale pre-suspend contact or a driver reset must never become a wake tap.
  resetKoboTouchInputForSuspend();
  waitForPowerRelease();
'''
    cmake_old = '''\tadd_executable(crossink-kobo-touch-gesture-test tests/touch-gesture-test.cpp)
\ttarget_compile_options(crossink-kobo-touch-gesture-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
\ttarget_link_libraries(crossink-kobo-touch-gesture-test PRIVATE crossink_kobo_input)
\tadd_test(NAME crossink-kobo-touch-gesture COMMAND crossink-kobo-touch-gesture-test)
'''
    cmake_new = '''\tadd_executable(crossink-kobo-evdev-touch-test tests/evdev-touch-test.cpp)
\ttarget_compile_options(crossink-kobo-evdev-touch-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
\ttarget_link_libraries(crossink-kobo-evdev-touch-test PRIVATE crossink_kobo_input)
\tadd_test(NAME crossink-kobo-evdev-touch COMMAND crossink-kobo-evdev-touch-test)

\tadd_executable(crossink-kobo-touch-gesture-test tests/touch-gesture-test.cpp)
\ttarget_compile_options(crossink-kobo-touch-gesture-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
\ttarget_link_libraries(crossink-kobo-touch-gesture-test PRIVATE crossink_kobo_input)
\tadd_test(NAME crossink-kobo-touch-gesture COMMAND crossink-kobo-touch-gesture-test)
'''
    return [
        Operation("platform/kobo/input/KoboEvdevTouch.h", "typed evdev touch API", replace_entire(KOBO_EVDEV_TOUCH_H)),
        Operation("platform/kobo/input/KoboEvdevTouch.cpp", "timestamps SYN_DROPPED and typed failures", replace_entire(KOBO_EVDEV_TOUCH_CPP)),
        Operation("platform/kobo/tests/evdev-touch-test.cpp", "deterministic evdev parser tests", create_or_replace(EVDEV_TOUCH_TEST), True),
        Operation("platform/kobo/CMakeLists.txt", "register evdev touch test", replace_once(cmake_old, cmake_new, "crossink-kobo-evdev-touch-test")),
        Operation("src/MappedInputManager.h", "declare injected-input reset", replace_once(input_header_old, input_header_new, "void resetInjectedInput();")),
        Operation("src/MappedInputManager.cpp", "reset injected held state", replace_once(input_impl_old, input_impl_new, "void MappedInputManager::resetInjectedInput()")),
        Operation("platform/kobo/app/kobo-main.cpp", "touch reconnect state", replace_once(globals_old, globals_new, "kTouchReconnectDelayMicros")),
        Operation("platform/kobo/app/kobo-main.cpp", "touch reopen and reset helpers", insert_before_once(helper_old, helper_new.removesuffix(helper_old), "void resetTouchPipeline(const bool closeDevice)")),
        Operation("platform/kobo/app/kobo-main.cpp", "consume typed touch read results", replace_once(read_old, read_new, "TouchReadResult::Resynchronized")),
        Operation("platform/kobo/app/kobo-main.cpp", "reset held input on orientation", replace_once(orientation_old, orientation_new, "An orientation switch cancels every injected edge")),
        Operation("platform/kobo/app/kobo-main.cpp", "use reconnect helper at startup", replace_once(startup_old, startup_new, "(void)ensureTouchDeviceOpen();")),
        Operation("platform/kobo/app/kobo-main.cpp", "export suspend reset hook", replace_once(wrapper_old, wrapper_new, "void resetKoboTouchInputForSuspend()")),
        Operation("src/main.cpp", "declare suspend touch reset hook", replace_once(main_decl_old, main_decl_new, "void resetKoboTouchInputForSuspend();")),
        Operation("src/main.cpp", "reset evdev before suspend", replace_once(suspend_old, suspend_new, "a stale pre-suspend contact")),
    ]

def loop_power_operations() -> list[Operation]:
    activity_old = '''  if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || halTiltSensor.hadActivity() ||
      activityManager.preventAutoSleep()) {
'''
    activity_new = '''  if (mappedInputManager.wasAnyPressed() || mappedInputManager.wasAnyReleased() || halTiltSensor.hadActivity() ||
      activityManager.preventAutoSleep()) {
'''
    refresh_old = '''void syncRefreshProfilePreference() {
  const auto requested = SETTINGS.koboRefreshProfile == CrossPointSettings::KOBO_REFRESH_FAST
                             ? crossink::kobo::RefreshProfile::Fast
                             : crossink::kobo::RefreshProfile::Safe;
  const bool qualified = crossink::kobo::koboFastRefreshQualified();
  if (!display.setRefreshProfile(requested, qualified) && requested == crossink::kobo::RefreshProfile::Fast) {
    SETTINGS.koboRefreshProfile = CrossPointSettings::KOBO_REFRESH_SAFE;
    (void)SETTINGS.saveToFile();
    std::fprintf(stderr, "[KOBO][EPD] rejected unqualified Fast profile; restored Safe\\n");
  }
}
'''
    refresh_new = '''void syncRefreshProfilePreference() {
  static int appliedPreference = -1;
  const int requestedPreference = SETTINGS.koboRefreshProfile;
  const auto requested = requestedPreference == CrossPointSettings::KOBO_REFRESH_FAST
                             ? crossink::kobo::RefreshProfile::Fast
                             : crossink::kobo::RefreshProfile::Safe;
  if (requestedPreference == appliedPreference && display.refreshProfile() == requested) return;

  // Qualification is stable for the running binary and only needs disk I/O
  // when Fast is actually selected, not in every main-loop iteration.
  const bool qualified = requested == crossink::kobo::RefreshProfile::Fast &&
                         crossink::kobo::koboFastRefreshQualified();
  if (!display.setRefreshProfile(requested, qualified) && requested == crossink::kobo::RefreshProfile::Fast) {
    SETTINGS.koboRefreshProfile = CrossPointSettings::KOBO_REFRESH_SAFE;
    (void)SETTINGS.saveToFile();
    appliedPreference = CrossPointSettings::KOBO_REFRESH_SAFE;
    std::fprintf(stderr, "[KOBO][EPD] rejected unqualified Fast profile; restored Safe\\n");
    return;
  }
  appliedPreference = requestedPreference;
}
'''
    wifi_status_old = '''wl_status_t WiFiClass::status() {
  if (localIP() != IPAddress()) {
    status_ = WL_CONNECTED;
    if (mode_ == WIFI_OFF) mode_ = WIFI_STA;
    return status_;
  }
  if (mode_ != WIFI_STA && mode_ != WIFI_AP_STA) return status_;
  const std::string output = capture("wpa_cli -i wlan0 status 2>/dev/null");
  if (output.find("wpa_state=COMPLETED") == std::string::npos) return status_ = WL_IDLE_STATUS;
  if (!dhcpAttempted_) {
    dhcpAttempted_ = true;
    run("udhcpc -i wlan0 -n -q -t 4 -T 2 >/dev/null 2>&1");
  }
  status_ = localIP() == IPAddress() ? WL_IDLE_STATUS : WL_CONNECTED;
  return status_;
}
'''
    wifi_status_new = '''wl_status_t WiFiClass::status() {
  if (mode_ != WIFI_STA && mode_ != WIFI_AP_STA && mode_ != WIFI_OFF) return status_;

  const std::string output = capture("wpa_cli -i wlan0 status 2>/dev/null");
  const bool associated = output.find("wpa_state=COMPLETED") != std::string::npos;
  if (!associated) {
    if (mode_ == WIFI_STA || mode_ == WIFI_AP_STA) status_ = WL_IDLE_STATUS;
    return status_;
  }
  if (mode_ == WIFI_OFF) mode_ = WIFI_STA;
  if (localIP() != IPAddress()) return status_ = WL_CONNECTED;

  if (!dhcpAttempted_) {
    dhcpAttempted_ = true;
    run("udhcpc -i wlan0 -n -q -t 4 -T 2 >/dev/null 2>&1");
  }
  status_ = localIP() == IPAddress() ? WL_IDLE_STATUS : WL_CONNECTED;
  return status_;
}
'''
    return [
        Operation("src/main.cpp", "touch resets autosleep", replace_once(activity_old, activity_new, "mappedInputManager.wasAnyPressed()")),
        Operation("platform/kobo/app/kobo-main.cpp", "cache refresh profile selection", replace_once(refresh_old, refresh_new, "static int appliedPreference")),
        Operation("platform/kobo/compat/HalGPIO.h", "paced battery polling state", replace_entire(HAL_GPIO_H)),
        Operation("platform/kobo/compat/HalGPIO.cpp", "pace battery sysfs polling", replace_entire(HAL_GPIO_CPP)),
        Operation("platform/kobo/compat/KoboWifiAutoConnect.cpp", "throttle shell status polling", replace_entire(KOBOWIFI_AUTOCONNECT_CPP)),
        Operation("platform/kobo/compat/WiFi.cpp", "require association before connected", replace_once(wifi_status_old, wifi_status_new, "const bool associated")),
    ]


def suspend_resume_operations() -> list[Operation]:
    wake_old = '''  if (APP_STATE.lastSleepFromReader && !APP_STATE.openEpubPath.empty() &&
      Storage.exists(APP_STATE.openEpubPath.c_str())) {
    activityManager.goToReader(APP_STATE.openEpubPath, /*suppressBackRelease=*/true);
  } else {
    activityManager.goHome();
  }
  activityManager.requestUpdateAndWait();
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  return;
'''
    wake_new = '''  if (APP_STATE.lastSleepFromReader && !APP_STATE.openEpubPath.empty() &&
      Storage.exists(APP_STATE.openEpubPath.c_str())) {
    activityManager.goToReader(APP_STATE.openEpubPath, /*suppressBackRelease=*/true);
  } else {
    activityManager.goHome();
  }
  // goToReader/goHome defer replacement while SleepActivity is current.
  // Commit that transition before asking the render worker for the wake frame;
  // otherwise the old sleep screen is rendered and submitted twice.
  activityManager.loop();
  display.requestCleanRefresh();
  const auto wakeRender = activityManager.requestUpdateAndWait();
  if (wakeRender == RequestUpdateResult::Rejected) {
    RenderLock lock;
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  }
  return;
'''
    sync_old = '''RequestUpdateResult ActivityManager::requestUpdateAndWait() {
#if defined(KOBO_LINUX)
  if (!renderScheduler.requestRenderAndWait()) {
'''
    sync_new = '''RequestUpdateResult ActivityManager::requestUpdateAndWait() {
#if defined(KOBO_LINUX)
  // A synchronous request supersedes a deferred request already queued by
  // onEnter(). Clearing it avoids a redundant second presentation on the next
  // main-loop iteration, especially on wake.
  requestedUpdate.store(false, std::memory_order_release);
  if (!renderScheduler.requestRenderAndWait()) {
'''
    return [
        Operation("src/main.cpp", "render destination activity once on wake", replace_once(wake_old, wake_new, "Commit that transition before asking the render worker")),
        Operation("src/activities/ActivityManager.cpp", "coalesce deferred and synchronous render", replace_once(sync_old, sync_new, "A synchronous request supersedes a deferred request")),
    ]

def display_recovery_operations() -> list[Operation]:
    hal_old = '''void HalDisplay::refreshDisplay(const RefreshMode mode, bool /*turnOffScreen*/) {
  if (!drmDisplay_.isOpen() && !fbInkDisplay_.isOpen()) {
    begin();
  }
  if (!drmDisplay_.isOpen() && !fbInkDisplay_.isOpen()) return;
  const RefreshKind requested = refreshKind(mode);
  (void)refreshExecutor_.present(drmDisplay_, fbInkDisplay_, useDrm_, frameBuffer_.data(), frameBuffer_.size(),
                                 requested, refreshProfileQualified_);
}
'''
    hal_new = '''void HalDisplay::refreshDisplay(const RefreshMode mode, bool /*turnOffScreen*/) {
  if (!drmDisplay_.isOpen() && !fbInkDisplay_.isOpen()) begin();
  if (!drmDisplay_.isOpen() && !fbInkDisplay_.isOpen()) return;

  const RefreshKind requested = refreshKind(mode);
  const bool presented = refreshExecutor_.present(drmDisplay_, fbInkDisplay_, useDrm_, frameBuffer_.data(),
                                                   frameBuffer_.size(), requested, refreshProfileQualified_);
  if (presented || !useDrm_) return;

  const auto telemetry = refreshExecutor_.lastTelemetry();
  // Deadline/temperature fallback is policy, not backend loss. Switch only
  // after a real DRM error whose same-backend recovery full also failed.
  if (telemetry.errorNumber == 0 || telemetry.fallbackSucceeded) return;

  std::fprintf(stderr, "[KOBO][EPD] DRM backend failed errno=%d; attempting live FBInk fallback\\n",
               telemetry.errorNumber);
  drmDisplay_.close();
  if (!fbInkDisplay_.open()) {
    const int fbInkError = fbInkDisplay_.lastError();
    // Preserve a chance of recovery on the next frame if FBInk is unavailable.
    useDrm_ = drmDisplay_.open();
    std::fprintf(stderr, "[KOBO][EPD] FBInk fallback open failed=%d DRM_reopen=%d\\n", fbInkError,
                 useDrm_ ? 1 : 0);
    return;
  }

  useDrm_ = false;
  refreshExecutor_.reset();
  refreshProfileQualified_ = false;
  const auto recovery = crossink::kobo::KoboDirtyRegion::full(
      crossink::kobo::KoboFbInkDisplay::kPanelWidth, crossink::kobo::KoboFbInkDisplay::kPanelHeight,
      frameBuffer_.size());
  if (!fbInkDisplay_.presentPackedMono(frameBuffer_.data(), frameBuffer_.size(), RefreshKind::Full, recovery)) {
    std::fprintf(stderr, "[KOBO][EPD] live FBInk recovery full failed=%d\\n", fbInkDisplay_.lastError());
  } else {
    std::fprintf(stderr, "[KOBO][EPD] live backend switched to FBInk after DRM failure\\n");
  }
}
'''
    return [
        Operation("platform/kobo/display/KoboFbInkDisplay.h", "map dirty regions for FBInk orientation", replace_entire(KOBO_FBINK_DISPLAY_H)),
        Operation("platform/kobo/display/KoboFbInkDisplay.cpp", "execute central dirty-region decision in FBInk", replace_entire(KOBO_FBINK_DISPLAY_CPP)),
        Operation("platform/kobo/tests/packed-mono-test.cpp", "dirty-region transform tests", replace_entire(PACKED_MONO_TEST)),
        Operation("platform/kobo/compat/HalDisplay.cpp", "live DRM to FBInk recovery", replace_once(hal_old, hal_new, "attempting live FBInk fallback")),
    ]

def websocket_operations() -> list[Operation]:
    font_state_old = '''  struct FontUploadState {
    HalFile file;
    std::string familyName;
    std::string filePath;
    bool valid = false;
    bool magicChecked = false;
    size_t bytesWritten = 0;
    static constexpr size_t BUFFER_SIZE = 4096;
    std::vector<uint8_t> buffer;
    size_t bufferPos = 0;

    FontUploadState() { buffer.resize(BUFFER_SIZE); }
  } fontUpload;
'''
    font_state_new = '''  struct FontUploadState {
    HalFile file;
    std::string familyName;
    std::string filePath;
    std::string temporaryPath;
    bool valid = false;
    bool activated = false;
    size_t bytesWritten = 0;
    static constexpr size_t BUFFER_SIZE = 4096;
    std::vector<uint8_t> buffer;
    size_t bufferPos = 0;

    FontUploadState() { buffer.resize(BUFFER_SIZE); }
  } fontUpload;
'''
    font_data_old = '''void CrossPointWebServer::handleFontUploadData() {
  HTTPUpload& upload = server->upload();

  switch (upload.status) {
    case UPLOAD_FILE_START: {
      esp_task_wdt_reset();
      String family = server->arg("family");
      fontUpload.file = HalFile();
      fontUpload.familyName.clear();
      fontUpload.filePath.clear();
      fontUpload.valid = false;
      fontUpload.magicChecked = false;
      fontUpload.bytesWritten = 0;
      fontUpload.bufferPos = 0;

      if (!FontInstaller::isValidFamilyName(family.c_str())) {
        LOG_ERR("WEB", "Invalid font family name: %s", family.c_str());
        break;
      }

      String filename = upload.filename;
      filename.replace(' ', '_');
      // Validate filename: rejects path traversal (../, /, \\) and enforces
      // a .cpfont basename of alphanumeric + hyphen + underscore. Without
      // this an attacker could supply "../../.crosspoint/settings.json" as
      // a "filename" and have it written outside the fonts directory.
      if (!FontInstaller::isValidCpfontFilename(filename.c_str())) {
        LOG_ERR("WEB", "Invalid font filename: %s", filename.c_str());
        break;
      }

      fontUpload.familyName = family.c_str();

      // Create a temporary FontInstaller for directory creation
      FontInstaller installer(sdFontSystem.registry());
      if (!installer.ensureFamilyDir(family.c_str())) {
        LOG_ERR("WEB", "Failed to create font family dir");
        break;
      }

      char path[192];
      FontInstaller::buildFontPath(family.c_str(), filename.c_str(), path, sizeof(path));
      fontUpload.filePath = path;

      if (!Storage.openFileForWrite("WEB", path, fontUpload.file)) {
        LOG_ERR("WEB", "Failed to open font file for write: %s", path);
        break;
      }

      fontUpload.valid = true;
      LOG_DBG("WEB", "Font upload started: %s -> %s", filename.c_str(), path);
      break;
    }

    case UPLOAD_FILE_WRITE: {
      if (!fontUpload.valid) break;
      esp_task_wdt_reset();

      // Validate magic bytes on first chunk only
      if (!fontUpload.magicChecked && upload.currentSize >= 8) {
        if (memcmp(upload.buf, "CPFONT\\0\\0", 8) != 0) {
          LOG_ERR("WEB", "Invalid .cpfont magic bytes");
          fontUpload.valid = false;
          break;
        }
        fontUpload.magicChecked = true;
      }

      // Buffer writes for efficiency
      size_t remaining = upload.currentSize;
      const uint8_t* src = upload.buf;
      while (remaining > 0) {
        size_t space = FontUploadState::BUFFER_SIZE - fontUpload.bufferPos;
        size_t chunk = (remaining < space) ? remaining : space;
        memcpy(fontUpload.buffer.data() + fontUpload.bufferPos, src, chunk);
        fontUpload.bufferPos += chunk;
        src += chunk;
        remaining -= chunk;

        if (fontUpload.bufferPos >= FontUploadState::BUFFER_SIZE) {
          fontUpload.file.write(fontUpload.buffer.data(), fontUpload.bufferPos);
          fontUpload.bytesWritten += fontUpload.bufferPos;
          fontUpload.bufferPos = 0;
          esp_task_wdt_reset();
        }
      }
      break;
    }

    case UPLOAD_FILE_END: {
      // Flush remaining buffer
      if (fontUpload.valid && fontUpload.bufferPos > 0) {
        fontUpload.file.write(fontUpload.buffer.data(), fontUpload.bufferPos);
        fontUpload.bytesWritten += fontUpload.bufferPos;
        fontUpload.bufferPos = 0;
      }
      if (fontUpload.file.isOpen()) {
        fontUpload.file.close();
      }

      if (!fontUpload.valid && !fontUpload.filePath.empty()) {
        Storage.remove(fontUpload.filePath.c_str());
      }

      LOG_DBG("WEB", "Font upload end: valid=%d, %zu bytes", fontUpload.valid, fontUpload.bytesWritten);
      break;
    }

    case UPLOAD_FILE_ABORTED: {
      if (fontUpload.file) {
        fontUpload.file.close();
      }
      if (!fontUpload.filePath.empty()) {
        Storage.remove(fontUpload.filePath.c_str());
      }
      fontUpload.valid = false;
      LOG_DBG("WEB", "Font upload aborted");
      break;
    }
  }
}

void CrossPointWebServer::handleFontUpload() {
  if (fontUpload.valid) {
    sdFontSystem.markRegistryDirty();
    server->send(200, "application/json", "{\\"ok\\":true}");
    LOG_DBG("WEB", "Font upload complete: %s", fontUpload.filePath.c_str());
  } else {
    server->send(400, "application/json", "{\\"error\\":\\"Invalid .cpfont file\\"}");
  }
}
'''
    font_data_new = '''void CrossPointWebServer::handleFontUploadData() {
  HTTPUpload& upload = server->upload();

  switch (upload.status) {
    case UPLOAD_FILE_START: {
      esp_task_wdt_reset();
      String family = server->arg("family");
      fontUpload.file = HalFile();
      fontUpload.familyName.clear();
      fontUpload.filePath.clear();
      fontUpload.temporaryPath.clear();
      fontUpload.valid = false;
      fontUpload.activated = false;
      fontUpload.bytesWritten = 0;
      fontUpload.bufferPos = 0;

      if (!FontInstaller::isValidFamilyName(family.c_str())) {
        LOG_ERR("WEB", "Invalid font family name: %s", family.c_str());
        break;
      }

      String filename = upload.filename;
      filename.replace(' ', '_');
      if (!FontInstaller::isValidCpfontFilename(filename.c_str())) {
        LOG_ERR("WEB", "Invalid font filename: %s", filename.c_str());
        break;
      }

      fontUpload.familyName = family.c_str();
      FontInstaller installer(sdFontSystem.registry());
      if (!installer.ensureFamilyDir(family.c_str())) {
        LOG_ERR("WEB", "Failed to create font family dir");
        break;
      }

      char path[192];
      FontInstaller::buildFontPath(family.c_str(), filename.c_str(), path, sizeof(path));
      fontUpload.filePath = path;
      fontUpload.temporaryPath = fontUpload.filePath + ".part";
      if (Storage.exists(fontUpload.temporaryPath.c_str())) Storage.remove(fontUpload.temporaryPath.c_str());

      if (!Storage.openFileForWrite("WEB", fontUpload.temporaryPath.c_str(), fontUpload.file)) {
        LOG_ERR("WEB", "Failed to open font staging file: %s", fontUpload.temporaryPath.c_str());
        break;
      }

      fontUpload.valid = true;
      LOG_DBG("WEB", "Font upload staged: %s -> %s", filename.c_str(), fontUpload.temporaryPath.c_str());
      break;
    }

    case UPLOAD_FILE_WRITE: {
      if (!fontUpload.valid) break;
      esp_task_wdt_reset();

      size_t remaining = upload.currentSize;
      const uint8_t* src = upload.buf;
      while (remaining > 0 && fontUpload.valid) {
        const size_t space = FontUploadState::BUFFER_SIZE - fontUpload.bufferPos;
        const size_t chunk = remaining < space ? remaining : space;
        memcpy(fontUpload.buffer.data() + fontUpload.bufferPos, src, chunk);
        fontUpload.bufferPos += chunk;
        src += chunk;
        remaining -= chunk;

        if (fontUpload.bufferPos >= FontUploadState::BUFFER_SIZE) {
          const size_t expected = fontUpload.bufferPos;
          const size_t written = fontUpload.file.write(fontUpload.buffer.data(), expected);
          fontUpload.bufferPos = 0;
          if (written != expected) {
            LOG_ERR("WEB", "Short font upload write: expected=%zu written=%zu", expected, written);
            fontUpload.valid = false;
            break;
          }
          fontUpload.bytesWritten += written;
          esp_task_wdt_reset();
        }
      }
      break;
    }

    case UPLOAD_FILE_END: {
      if (fontUpload.valid && fontUpload.bufferPos > 0) {
        const size_t expected = fontUpload.bufferPos;
        const size_t written = fontUpload.file.write(fontUpload.buffer.data(), expected);
        fontUpload.bufferPos = 0;
        if (written != expected) {
          LOG_ERR("WEB", "Short final font upload write: expected=%zu written=%zu", expected, written);
          fontUpload.valid = false;
        } else {
          fontUpload.bytesWritten += written;
        }
      }
      if (fontUpload.file.isOpen()) {
        if (fontUpload.valid && !fontUpload.file.sync()) fontUpload.valid = false;
        fontUpload.file.close();
      }

      // Validate from the completed staging file so split multipart chunks
      // cannot bypass the CPFONT magic check.
      if (fontUpload.valid && fontUpload.bytesWritten >= 8) {
        HalFile verify;
        std::uint8_t magic[8]{};
        if (!Storage.openFileForRead("WEB", fontUpload.temporaryPath.c_str(), verify) ||
            verify.read(magic, sizeof(magic)) != sizeof(magic) || memcmp(magic, "CPFONT\\0\\0", 8) != 0) {
          fontUpload.valid = false;
        }
        if (verify.isOpen()) verify.close();
      } else {
        fontUpload.valid = false;
      }

      if (fontUpload.valid && Storage.rename(fontUpload.temporaryPath.c_str(), fontUpload.filePath.c_str())) {
        fontUpload.activated = true;
      } else {
        Storage.remove(fontUpload.temporaryPath.c_str());
      }

      LOG_DBG("WEB", "Font upload end: valid=%d activated=%d bytes=%zu", fontUpload.valid,
              fontUpload.activated, fontUpload.bytesWritten);
      break;
    }

    case UPLOAD_FILE_ABORTED: {
      if (fontUpload.file) fontUpload.file.close();
      if (!fontUpload.temporaryPath.empty()) Storage.remove(fontUpload.temporaryPath.c_str());
      fontUpload.valid = false;
      fontUpload.activated = false;
      LOG_DBG("WEB", "Font upload aborted; existing destination preserved");
      break;
    }
  }
}

void CrossPointWebServer::handleFontUpload() {
  if (fontUpload.activated) {
    sdFontSystem.markRegistryDirty();
    server->send(200, "application/json", "{\\"ok\\":true}");
    LOG_DBG("WEB", "Font upload complete: %s", fontUpload.filePath.c_str());
  } else {
    server->send(400, "application/json", "{\\"error\\":\\"Invalid or incomplete .cpfont file\\"}");
  }
}
'''
    websocket_scope_old = '''          wsUploadSize = sizeToken.toInt();
          wsUploadPath = normalizeWebPath(msg.substring(secondColon + 1));
          wsUploadReceived = 0;
'''
    websocket_scope_new = '''          wsUploadSize = sizeToken.toInt();
          wsUploadPath = normalizeWebPath(msg.substring(secondColon + 1));
#ifdef KOBO_LINUX
          // Persistent Kobo WebSocket transfer is a book ingress path, not a
          // general filesystem writer. Match the safe HTTP upload contract.
          wsUploadPath = "/Books";
          if (!FsHelpers::hasEpubExtension(wsUploadFileName.c_str())) {
            wsServer->sendTXT(num, "ERROR:Only EPUB files can be uploaded");
            return;
          }
#endif
          wsUploadReceived = 0;
'''
    globals_old = '''String wsUploadFileName;
String wsUploadPath;
size_t wsUploadSize = 0;
'''
    globals_new = '''String wsUploadFileName;
String wsUploadPath;
String wsUploadTargetPath;
String wsUploadTemporaryPath;
size_t wsUploadSize = 0;
'''
    abort_old = '''void CrossPointWebServer::abortWsUpload(const char* tag) {
  // Explicit close() required: file-scope global persists beyond function scope
  wsUploadFile.close();
  String filePath = wsUploadPath;
  if (!filePath.endsWith("/")) filePath += "/";
  filePath += wsUploadFileName;
  if (Storage.remove(filePath.c_str())) {
    LOG_DBG(tag, "Deleted incomplete upload: %s", filePath.c_str());
  } else {
    LOG_DBG(tag, "Failed to delete incomplete upload: %s", filePath.c_str());
  }
  wsUploadInProgress = false;
  wsUploadClientNum = 255;
  wsLastProgressSent = 0;
}
'''
    abort_new = '''void CrossPointWebServer::abortWsUpload(const char* tag) {
  // Explicit close() required: file-scope global persists beyond function scope.
  // Remove only the staging file; an existing destination must survive every
  // disconnect, overflow and write failure.
  wsUploadFile.close();
  if (!wsUploadTemporaryPath.isEmpty()) {
    if (Storage.remove(wsUploadTemporaryPath.c_str())) {
      LOG_DBG(tag, "Deleted incomplete staging upload: %s", wsUploadTemporaryPath.c_str());
    } else {
      LOG_DBG(tag, "No incomplete staging upload removed: %s", wsUploadTemporaryPath.c_str());
    }
  }
  wsUploadTargetPath = "";
  wsUploadTemporaryPath = "";
  wsUploadInProgress = false;
  wsUploadClientNum = 255;
  wsLastProgressSent = 0;
}
'''
    start_old = '''          LOG_DBG("WS", "Starting upload: %s (%d bytes) to %s", wsUploadFileName.c_str(), wsUploadSize,
                  filePath.c_str());

          // Check if file exists and remove it
          esp_task_wdt_reset();
          if (Storage.exists(filePath.c_str())) {
            Storage.remove(filePath.c_str());
          }

          // Open file for writing
          esp_task_wdt_reset();
          if (!Storage.openFileForWrite("WS", filePath, wsUploadFile)) {
            wsServer->sendTXT(num, "ERROR:Failed to create file");
            wsUploadInProgress = false;
            wsUploadClientNum = 255;
            return;
          }
          esp_task_wdt_reset();

          // Zero-byte upload: complete immediately without waiting for BIN frames
          if (wsUploadSize == 0) {
            // Explicit close() required: file-scope global persists beyond function scope
            wsUploadFile.close();
            wsLastCompleteName = wsUploadFileName;
            wsLastCompleteSize = 0;
            wsLastCompleteAt = millis();
            LOG_DBG("WS", "Zero-byte upload complete: %s", filePath.c_str());
            clearBookCachePreservingUserState(filePath.c_str());
            wsServer->sendTXT(num, "DONE");
            wsLastProgressSent = 0;
            break;
          }
'''
    start_new = '''          LOG_DBG("WS", "Starting staged upload: %s (%d bytes) to %s", wsUploadFileName.c_str(),
                  wsUploadSize, filePath.c_str());

          wsUploadTargetPath = filePath;
          wsUploadTemporaryPath = filePath + ".part";
          esp_task_wdt_reset();
          if (Storage.exists(wsUploadTemporaryPath.c_str())) Storage.remove(wsUploadTemporaryPath.c_str());

          // Open a same-directory staging file. The destination is activated
          // only after all bytes have been synced successfully.
          esp_task_wdt_reset();
          if (!Storage.openFileForWrite("WS", wsUploadTemporaryPath, wsUploadFile)) {
            wsServer->sendTXT(num, "ERROR:Failed to create staging file");
            wsUploadTargetPath = "";
            wsUploadTemporaryPath = "";
            wsUploadInProgress = false;
            wsUploadClientNum = 255;
            return;
          }
          esp_task_wdt_reset();

          // Zero-byte upload: sync and atomically activate immediately.
          if (wsUploadSize == 0) {
            const bool synced = wsUploadFile.sync();
            wsUploadFile.close();
            if (!synced || !Storage.rename(wsUploadTemporaryPath.c_str(), wsUploadTargetPath.c_str())) {
              Storage.remove(wsUploadTemporaryPath.c_str());
              wsUploadTargetPath = "";
              wsUploadTemporaryPath = "";
              wsServer->sendTXT(num, "ERROR:Failed to activate upload");
              return;
            }
            wsLastCompleteName = wsUploadFileName;
            wsLastCompleteSize = 0;
            wsLastCompleteAt = millis();
            LOG_DBG("WS", "Zero-byte upload complete: %s", wsUploadTargetPath.c_str());
            clearBookCachePreservingUserState(wsUploadTargetPath.c_str());
            wsUploadTargetPath = "";
            wsUploadTemporaryPath = "";
            wsServer->sendTXT(num, "DONE");
            wsLastProgressSent = 0;
            break;
          }
'''
    complete_old = '''      // Check if upload complete
      if (wsUploadReceived >= wsUploadSize) {
        // Explicit close() required: file-scope global persists beyond function scope
        wsUploadFile.close();
        wsUploadInProgress = false;
        wsUploadClientNum = 255;

        wsLastCompleteName = wsUploadFileName;
        wsLastCompleteSize = wsUploadSize;
        wsLastCompleteAt = millis();

        unsigned long elapsed = millis() - wsUploadStartTime;
        float kbps = (elapsed > 0) ? (wsUploadSize / 1024.0) / (elapsed / 1000.0) : 0;

        LOG_DBG("WS", "Upload complete: %s (%d bytes in %lu ms, %.1f KB/s)", wsUploadFileName.c_str(), wsUploadSize,
                elapsed, kbps);

        // Clear epub cache to prevent stale metadata issues when overwriting files
        String filePath = wsUploadPath;
        if (!filePath.endsWith("/")) filePath += "/";
        filePath += wsUploadFileName;
        clearBookCachePreservingUserState(filePath.c_str());

        wsServer->sendTXT(num, "DONE");
        wsLastProgressSent = 0;
      }
'''
    complete_new = '''      // Check if upload complete
      if (wsUploadReceived >= wsUploadSize) {
        if (!wsUploadFile.sync()) {
          abortWsUpload("WS");
          wsServer->sendTXT(num, "ERROR:Failed to sync upload");
          return;
        }
        wsUploadFile.close();
        if (!Storage.rename(wsUploadTemporaryPath.c_str(), wsUploadTargetPath.c_str())) {
          Storage.remove(wsUploadTemporaryPath.c_str());
          wsUploadTargetPath = "";
          wsUploadTemporaryPath = "";
          wsUploadInProgress = false;
          wsUploadClientNum = 255;
          wsServer->sendTXT(num, "ERROR:Failed to activate upload");
          return;
        }
        wsUploadInProgress = false;
        wsUploadClientNum = 255;

        wsLastCompleteName = wsUploadFileName;
        wsLastCompleteSize = wsUploadSize;
        wsLastCompleteAt = millis();

        unsigned long elapsed = millis() - wsUploadStartTime;
        float kbps = (elapsed > 0) ? (wsUploadSize / 1024.0) / (elapsed / 1000.0) : 0;

        LOG_DBG("WS", "Upload complete: %s (%d bytes in %lu ms, %.1f KB/s)", wsUploadFileName.c_str(), wsUploadSize,
                elapsed, kbps);

        clearBookCachePreservingUserState(wsUploadTargetPath.c_str());
        wsUploadTargetPath = "";
        wsUploadTemporaryPath = "";
        wsServer->sendTXT(num, "DONE");
        wsLastProgressSent = 0;
      }
'''
    return [
        Operation("src/network/CrossPointWebServer.h", "atomic font upload state", replace_once(font_state_old, font_state_new, "bool activated = false;")),
        Operation("src/network/CrossPointWebServer.cpp", "stage validate and atomically activate fonts", replace_once(font_data_old, font_data_new, "existing destination preserved")),
        Operation("src/network/CrossPointWebServer.cpp", "scope Kobo websocket uploads to EPUB books", replace_once(websocket_scope_old, websocket_scope_new, "Persistent Kobo WebSocket transfer is a book ingress path")),
        Operation("src/network/CrossPointWebServer.cpp", "track websocket staging paths", replace_once(globals_old, globals_new, "String wsUploadTargetPath")),
        Operation("src/network/CrossPointWebServer.cpp", "abort only staging upload", replace_once(abort_old, abort_new, "Deleted incomplete staging upload")),
        Operation("src/network/CrossPointWebServer.cpp", "stage websocket destination", replace_once(start_old, start_new, "Starting staged upload")),
        Operation("src/network/CrossPointWebServer.cpp", "sync and atomically activate websocket upload", replace_once(complete_old, complete_new, "ERROR:Failed to sync upload")),
    ]



def network_async_operations() -> list[Operation]:
    server_begin_old = '''  // Disable WiFi sleep to improve responsiveness and prevent 'unreachable' errors.\n  // This is critical for reliable web server operation on ESP32.\n  WiFi.setSleep(false);\n'''
    server_begin_new = '''#ifdef KOBO_LINUX\n  // The persistent Kobo server is normally idle. Keep WLAN powersave enabled\n  // and disable it only for an active data transfer.\n  WiFi.setSleep(true);\n#else\n  // ESP32 keeps the historical always-responsive server policy.\n  WiFi.setSleep(false);\n#endif\n'''
    stop_old = '''  // Note: Static upload variables (uploadFileName, uploadPath, uploadError) are declared\n  // later in the file and will be cleared when they go out of scope or on next upload\n  LOG_DBG("WEB", "[MEM] Free heap final: %d bytes", ESP.getFreeHeap());\n'''
    stop_new = '''#ifdef KOBO_LINUX\n  // Restore the normal idle-server WLAN powersave policy on shutdown.\n  WiFi.setSleep(true);\n#endif\n  // Note: Static upload variables (uploadFileName, uploadPath, uploadError) are declared\n  // later in the file and will be cleared when they go out of scope or on next upload\n  LOG_DBG("WEB", "[MEM] Free heap final: %d bytes", ESP.getFreeHeap());\n'''
    http_start_old = '''    LOG_DBG("WEB", "[UPLOAD] START: %s to path: %s", state.fileName.c_str(), state.path.c_str());\n'''
    http_start_new = '''#ifdef KOBO_LINUX\n    // Active HTTP upload temporarily disables WLAN powersave.\n    WiFi.setSleep(false);\n#endif\n    LOG_DBG("WEB", "[UPLOAD] START: %s to path: %s", state.fileName.c_str(), state.path.c_str());\n'''
    http_end_old = '''    state.error = "Upload aborted";\n    LOG_DBG("WEB", "Upload aborted");\n  }\n}\n\nvoid CrossPointWebServer::handleUploadPost'''
    http_end_new = '''    state.error = "Upload aborted";\n    LOG_DBG("WEB", "Upload aborted");\n  }\n#ifdef KOBO_LINUX\n  // Every terminal HTTP upload state restores the idle WLAN policy.\n  if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED) WiFi.setSleep(true);\n#endif\n}\n\nvoid CrossPointWebServer::handleUploadPost'''
    ws_abort_old = '''  wsUploadInProgress = false;\n  wsUploadClientNum = 255;\n  wsLastProgressSent = 0;\n}\n'''
    ws_abort_new = '''  wsUploadInProgress = false;\n  wsUploadClientNum = 255;\n  wsLastProgressSent = 0;\n#ifdef KOBO_LINUX\n  // Abort restores the persistent server's idle WLAN policy.\n  WiFi.setSleep(true);\n#endif\n}\n'''
    ws_start_old = '''          LOG_DBG("WS", "Starting staged upload: %s (%d bytes) to %s", wsUploadFileName.c_str(),\n                  wsUploadSize, filePath.c_str());\n'''
    ws_start_new = '''#ifdef KOBO_LINUX\n          // Active WebSocket upload temporarily disables WLAN powersave.\n          WiFi.setSleep(false);\n#endif\n          LOG_DBG("WS", "Starting staged upload: %s (%d bytes) to %s", wsUploadFileName.c_str(),\n                  wsUploadSize, filePath.c_str());\n'''
    ws_zero_old = '''            wsServer->sendTXT(num, "DONE");\n            wsLastProgressSent = 0;\n            break;\n'''
    ws_zero_new = '''            wsServer->sendTXT(num, "DONE");\n            wsLastProgressSent = 0;\n#ifdef KOBO_LINUX\n            // Zero-byte completion restores the idle WLAN policy.\n            WiFi.setSleep(true);\n#endif\n            break;\n'''
    ws_complete_old = '''        wsServer->sendTXT(num, "DONE");\n        wsLastProgressSent = 0;\n      }\n'''
    ws_complete_new = '''        wsServer->sendTXT(num, "DONE");\n        wsLastProgressSent = 0;\n#ifdef KOBO_LINUX\n        // Completed WebSocket upload restores the idle WLAN policy.\n        WiFi.setSleep(true);\n#endif\n      }\n'''
    return [
        Operation("platform/kobo/compat/WiFi.h", "add asynchronous scan and DHCP state", replace_entire(WIFI_ASYNC_H)),
        Operation("platform/kobo/compat/WiFi.cpp", "move scan and DHCP commands off the main loop", replace_entire(WIFI_ASYNC_CPP)),
        Operation("src/network/CrossPointWebServer.cpp", "default idle Kobo server to WLAN powersave", replace_once(server_begin_old, server_begin_new, "persistent Kobo server is normally idle")),
        Operation("src/network/CrossPointWebServer.cpp", "restore WLAN powersave on server stop", replace_once(stop_old, stop_new, "Restore the normal idle-server WLAN powersave policy")),
        Operation("src/network/CrossPointWebServer.cpp", "disable powersave during HTTP upload", replace_once(http_start_old, http_start_new, "Active HTTP upload temporarily disables WLAN powersave")),
        Operation("src/network/CrossPointWebServer.cpp", "restore powersave after HTTP upload", replace_once(http_end_old, http_end_new, "Every terminal HTTP upload state restores")),
        Operation("src/network/CrossPointWebServer.cpp", "restore powersave after websocket abort", replace_once(ws_abort_old, ws_abort_new, "Abort restores the persistent server")),
        Operation("src/network/CrossPointWebServer.cpp", "disable powersave during websocket upload", replace_once(ws_start_old, ws_start_new, "Active WebSocket upload temporarily disables")),
        Operation("src/network/CrossPointWebServer.cpp", "restore powersave after zero-byte websocket upload", replace_once(ws_zero_old, ws_zero_new, "Zero-byte completion restores")),
        Operation("src/network/CrossPointWebServer.cpp", "restore powersave after websocket completion", replace_once(ws_complete_old, ws_complete_new, "Completed WebSocket upload restores")),
    ]

def release_hygiene_operations() -> list[Operation]:
    changelog_old = "## [Kobo Glo HD Beta 1] - Unreleased\n"
    changelog_new = "## [Kobo Glo HD Beta 3] - Unreleased\n"
    changelog_calibration_old = "- Added a five-point N437 touch-calibration utility that records raw and mapped coordinates for hardware verification.\n"
    changelog_calibration_new = "- Added a five-point N437 touch-validation utility that records raw and mapped coordinates for hardware verification.\n"
    cmake_version_old = '\tCROSSINK_VERSION="1.4.0-kobo-beta1"\n'
    cmake_version_new = '\tCROSSINK_VERSION="1.4.0-kobo-beta3"\n'
    package_version_old = "CROSSINK_KOBO_APP_VERSION = 1.4.0-beta1\n"
    package_version_new = "CROSSINK_KOBO_APP_VERSION = 1.4.0-beta3\n"
    readme_old = "**Note**: This firmware is confirmed to be working on both the X3 and X4.\n"
    readme_new = ("**Kobo status:** This repository targets the Kobo Glo HD N437. "
                  "The current public hardening line is Beta 3; Beta 4 is reserved for the completed "
                  "software, ARM/Buildroot and physical N437 gates documented under `docs/K4_*`.\n")
    issue_form = r'''name: Kobo N437 Bug Report
description: Report a CrossInk-Kobo problem with enough release and hardware evidence to reproduce it
title: "[Kobo N437] "
labels: ["bug", "kobo"]
body:
  - type: markdown
    attributes:
      value: |
        Use this form only for the Kobo Glo HD N437 port. Do not mark a hardware gate as passed without the exact binary and device evidence.
  - type: input
    id: release
    attributes:
      label: CrossInk release / commit
      placeholder: 1.4.0-kobo-beta3 or full commit SHA
    validations:
      required: true
  - type: input
    id: binary-sha
    attributes:
      label: Installed binary SHA-256
      description: Copy binary_sha256 from /opt/crossink/current/build-manifest.txt.
    validations:
      required: true
  - type: input
    id: kernel
    attributes:
      label: Kernel
      placeholder: Output of uname -r
    validations:
      required: true
  - type: dropdown
    id: display-backend
    attributes:
      label: Display backend
      options:
        - DRM
        - FBInk fallback
        - Unknown
    validations:
      required: true
  - type: dropdown
    id: orientation
    attributes:
      label: Orientation
      options:
        - Portrait
        - Landscape clockwise
        - Inverted portrait
        - Landscape counter-clockwise
    validations:
      required: true
  - type: dropdown
    id: ui-scale
    attributes:
      label: Kobo UI scale
      options:
        - 100%
        - 150%
        - 200%
        - 250%
    validations:
      required: true
  - type: textarea
    id: description
    attributes:
      label: Problem
      description: State what happened, what was visible and whether touch, Wi-Fi, suspend, web transfer or display refresh was involved.
    validations:
      required: true
  - type: textarea
    id: reproduction
    attributes:
      label: Exact reproduction steps
      placeholder: |
        1. Cold boot the N437
        2. Open ...
        3. Tap/swipe ...
        4. Observe ...
    validations:
      required: true
  - type: textarea
    id: expected
    attributes:
      label: Expected behavior
    validations:
      required: true
  - type: textarea
    id: evidence
    attributes:
      label: Logs and evidence
      description: Attach the relevant crossink.log excerpt, last-signal.txt when present, touch-region dump, screenshot/photo and power/suspend JSONL rows. Remove Wi-Fi passwords and private book content.
      render: shell
    validations:
      required: true
'''
    return [
        Operation("CHANGELOG.md", "label the current Kobo line Beta 3", replace_once(changelog_old, changelog_new, "Kobo Glo HD Beta 3")),
        Operation("CHANGELOG.md", "describe the five-point tool as validation", replace_once(changelog_calibration_old, changelog_calibration_new, "touch-validation utility")),
        Operation("platform/kobo/app/CMakeLists.txt", "align runtime version with Beta 3", replace_once(cmake_version_old, cmake_version_new, "1.4.0-kobo-beta3")),
        Operation("buildroot-external/package/crossink-kobo-app/crossink-kobo-app.mk", "align package version with Beta 3", replace_once(package_version_old, package_version_new, "1.4.0-beta3")),
        Operation("README.md", "replace inherited X3/X4 status with Kobo status", replace_once(readme_old, readme_new, "**Kobo status:**")),
        Operation(".github/ISSUE_TEMPLATE/kobo_bug_report.yml", "add N437 evidence-oriented issue form", create_or_replace(issue_form), True),
    ]


def ci_operations() -> list[Operation]:
    return [Operation(".github/workflows/ci.yml", "run Kobo host tests in CI", replace_entire(CI_YML))]


PHASES: dict[str, Callable[[], list[Operation]]] = {
    "touch-registry": touch_registry_operations,
    "touch-gestures": touch_gesture_operations,
    "evdev-hardening": evdev_hardening_operations,
    "input-routing-e2e": input_routing_operations,
    "loop-power": loop_power_operations,
    "suspend-resume": suspend_resume_operations,
    "display-recovery": display_recovery_operations,
    "websocket-atomic": websocket_operations,
    "network-async": network_async_operations,
    "ci": ci_operations,
    "release-hygiene": release_hygiene_operations,
}


def apply_operations(root: Path, operations: list[Operation], write: bool) -> None:
    contents: dict[str, str] = {}
    changed: set[str] = set()

    for operation in operations:
        path = root / operation.path
        if operation.path in contents:
            current = contents[operation.path]
        elif path.is_file():
            current = path.read_text(encoding="utf-8")
        elif operation.allow_missing:
            current = ""
        else:
            raise ApplyError(f"Missing tracked file: {operation.path}")
        try:
            updated = operation.transform(current)
        except ApplyError as error:
            raise ApplyError(f"{operation.path}: {operation.description}: {error}") from error
        contents[operation.path] = updated
        if updated != current:
            changed.add(operation.path)
            print(f"PREPARED  {operation.path}: {operation.description}")
        else:
            print(f"PRESENT   {operation.path}: {operation.description}")

    if not write:
        return
    for relative in sorted(changed):
        destination = root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(contents[relative], encoding="utf-8")

    result = run("git", "diff", "--check", check=False)
    if result.returncode != 0:
        raise ApplyError("git diff --check failed after writing:\n" + result.stdout + result.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--phase", choices=[*PHASES, "all"], default="all")
    parser.add_argument("--apply", action="store_true", help="write changes; default is check-only")
    parser.add_argument("--list", action="store_true", help="list prepared phases")
    args = parser.parse_args()

    if args.list:
        for phase in PHASES:
            print(phase)
        return 0

    root = repository_root()
    ensure_baseline(root)
    selected = list(PHASES) if args.phase == "all" else [args.phase]
    operations: list[Operation] = []
    for phase in selected:
        print(f"\n== {phase} ==")
        operations.extend(PHASES[phase]())
    # Keep one in-memory file view across phases. This makes `--phase all` a
    # true dry-run even where a later prepared phase builds on an earlier one.
    apply_operations(root, operations, args.apply)
    print("\nCheck complete." if not args.apply else "\nPrepared changes written; inspect git diff before committing.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ApplyError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(2)
