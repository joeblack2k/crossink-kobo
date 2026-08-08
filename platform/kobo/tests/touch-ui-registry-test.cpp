#include <cstdlib>
#include <iostream>

#include "components/TouchUiRegistry.h"

namespace {

[[noreturn]] void fail(const char* label) {
  std::cerr << label << '\n';
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main() {
  TOUCH_UI.clear();
  if (!TOUCH_UI.registerItem(20, 100, 500, 80, 1, 0, 4) || !TOUCH_UI.registerItem(20, 180, 500, 80, 1, 1, 4) ||
      !TOUCH_UI.registerItem(20, 260, 500, 80, 1, 2, 4)) {
    fail("valid rows must register");
  }
  if (TOUCH_UI.size() != 3) fail("row count");
  if (TOUCH_UI.forbiddenOverlapCount() != 0) fail("adjacent rows must not overlap");

  const auto second = TOUCH_UI.resolve(100, 220);
  if (!second.found || second.currentIndex != 1 || second.targetIndex != 1 || second.itemCount != 4) {
    fail("second row resolution");
  }
  const auto firstGeneration = second.generation;
  const auto third = TOUCH_UI.resolve(100, 300);
  if (!third.found || third.targetIndex != 2) fail("third row resolution");
  if (TOUCH_UI.resolve(700, 300).found) fail("outside rows");

  if (!TOUCH_UI.registerItem(20, 340, 500, 60, -1, 2, 4)) fail("pre-list focus registration");
  const auto fromTab = TOUCH_UI.resolve(100, 360);
  if (!fromTab.found || fromTab.currentIndex != -1 || fromTab.targetIndex != 2) fail("pre-list focus resolution");

  if (!TOUCH_UI.registerDirect(20, 400, 80, 60, TouchUiRegistry::TargetKind::KeyboardKey, 2, 7)) {
    fail("direct key registration");
  }
  const auto key = TOUCH_UI.resolve(40, 420);
  if (!key.found || key.kind != TouchUiRegistry::TargetKind::KeyboardKey || key.targetIndex != 2 ||
      key.secondaryTarget != 7) {
    fail("direct key resolution");
  }

  if (!TOUCH_UI.registerDirect(20, 480, 500, 120, TouchUiRegistry::TargetKind::TextSelectionSurface, 0)) {
    fail("text selection surface registration");
  }
  const auto textSurface = TOUCH_UI.resolve(300, 540);
  if (!textSurface.found || textSurface.kind != TouchUiRegistry::TargetKind::TextSelectionSurface) {
    fail("text selection surface resolution");
  }
  if (!TOUCH_UI.registerDirect(10, 610, 100, 100, TouchUiRegistry::TargetKind::Tab, 1) ||
      !TOUCH_UI.registerDirect(50, 650, 100, 100, TouchUiRegistry::TargetKind::Tab, 2)) {
    fail("overlapping targets must still register for audit");
  }
  if (TOUCH_UI.forbiddenOverlapCount() != 1) fail("unmarked overlap must fail audit");

  TOUCH_UI.clear();
  if (!TOUCH_UI.registerDirect(10, 610, 100, 100, TouchUiRegistry::TargetKind::Tab, 1, 0, true) ||
      !TOUCH_UI.registerDirect(50, 650, 100, 100, TouchUiRegistry::TargetKind::Tab, 2, 0, true)) {
    fail("intentional overlap registration");
  }
  if (TOUCH_UI.forbiddenOverlapCount() != 0) fail("explicit overlap must be exempt");

  TOUCH_UI.clear();
  if (!TOUCH_UI.registerDirect(10, 10, 100, 100, TouchUiRegistry::TargetKind::Tab, 11)) {
    fail("active frame registration");
  }
  const auto committedGeneration = TOUCH_UI.generation();
  TOUCH_UI.beginFrame();
  if (!TOUCH_UI.registerDirect(200, 10, 100, 100, TouchUiRegistry::TargetKind::Tab, 22)) {
    fail("staging registration");
  }
  if (!TOUCH_UI.resolve(50, 50).found || TOUCH_UI.resolve(250, 50).found) {
    fail("staging must not leak into active frame");
  }
  TOUCH_UI.commitFrame();
  const auto committed = TOUCH_UI.resolve(250, 50);
  if (!committed.found || committed.targetIndex != 22 || committed.generation == committedGeneration) {
    fail("committed frame");
  }

  TOUCH_UI.clear();
  if (TOUCH_UI.size() != 0 || TOUCH_UI.resolve(100, 220).found || TOUCH_UI.generation() == firstGeneration) {
    fail("clear stale rows or generation");
  }
  return EXIT_SUCCESS;
}
