#include "TouchUiRegistry.h"

#include <algorithm>

TouchUiRegistry& TouchUiRegistry::instance() {
  static TouchUiRegistry registry;
  return registry;
}

void TouchUiRegistry::clear() {
#ifdef KOBO_LINUX
  const std::lock_guard<std::mutex> lock(mutex_);
#endif
  count_ = 0;
  ++generation_;
}

bool TouchUiRegistry::registerItem(const int x, const int y, const int width, const int height, const int currentIndex,
                                   const int targetIndex, const int itemCount) {
#ifdef KOBO_LINUX
  const std::lock_guard<std::mutex> lock(mutex_);
#endif
  if (count_ >= regions_.size() || width <= 0 || height <= 0 || itemCount <= 0 || currentIndex < -1 ||
      currentIndex >= itemCount || targetIndex < 0 || targetIndex >= itemCount) {
    return false;
  }
  regions_[count_++] = {x, y,    width, height, currentIndex, targetIndex, itemCount, TargetKind::NavigationItem,
                        0, false};
  return true;
}

bool TouchUiRegistry::registerDirect(const int x, const int y, const int width, const int height, const TargetKind kind,
                                     const int target, const int secondaryTarget, const bool overlapAllowed) {
#ifdef KOBO_LINUX
  const std::lock_guard<std::mutex> lock(mutex_);
#endif
  if (count_ >= regions_.size() || width <= 0 || height <= 0 || kind == TargetKind::NavigationItem) return false;
  regions_[count_++] = {x, y, width, height, 0, target, 0, kind, secondaryTarget, overlapAllowed};
  return true;
}

bool TouchUiRegistry::intersects(const Region& first, const Region& second) {
  return first.x < second.x + second.width && second.x < first.x + first.width && first.y < second.y + second.height &&
         second.y < first.y + first.height;
}

TouchUiRegistry::Resolution TouchUiRegistry::resolve(const int x, const int y) const {
#ifdef KOBO_LINUX
  const std::lock_guard<std::mutex> lock(mutex_);
#endif
  // Last drawn wins when regions overlap, matching normal painter ordering.
  for (std::size_t i = count_; i > 0; --i) {
    const Region& region = regions_[i - 1];
    if (x >= region.x && y >= region.y && x < region.x + region.width && y < region.y + region.height) {
      return {true,        region.currentIndex,    region.targetIndex, region.itemCount,
              region.kind, region.secondaryTarget, generation_};
    }
  }
  return {};
}

std::size_t TouchUiRegistry::size() const {
#ifdef KOBO_LINUX
  const std::lock_guard<std::mutex> lock(mutex_);
#endif
  return count_;
}

std::size_t TouchUiRegistry::forbiddenOverlapCount() const {
#ifdef KOBO_LINUX
  const std::lock_guard<std::mutex> lock(mutex_);
#endif
  std::size_t conflicts = 0;
  for (std::size_t firstIndex = 0; firstIndex < count_; ++firstIndex) {
    for (std::size_t secondIndex = firstIndex + 1; secondIndex < count_; ++secondIndex) {
      const Region& first = regions_[firstIndex];
      const Region& second = regions_[secondIndex];
      if (intersects(first, second) && !(first.overlapAllowed && second.overlapAllowed)) ++conflicts;
    }
  }
  return conflicts;
}

std::uint32_t TouchUiRegistry::generation() const {
#ifdef KOBO_LINUX
  const std::lock_guard<std::mutex> lock(mutex_);
#endif
  return generation_;
}

#ifdef KOBO_LINUX
std::size_t TouchUiRegistry::snapshot(std::array<Region, kMaxRegions>& destination) const {
  const std::lock_guard<std::mutex> lock(mutex_);
  std::copy_n(regions_.begin(), count_, destination.begin());
  return count_;
}
#endif
