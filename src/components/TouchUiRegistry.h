#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#ifdef KOBO_LINUX
#include <mutex>
#endif

// Platform-neutral description of interactive UI regions. Renderers publish
// what they drew; platform adapters translate a resolved target to their own
// input mechanism. No evdev or Kobo hardware detail belongs here.
class TouchUiRegistry final {
 public:
  enum class TargetKind : unsigned char { NavigationItem, OptionItem, KeyboardKey, Tab, Slider, TextSelectionSurface };

  struct Region {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int currentIndex = 0;
    int targetIndex = 0;
    int itemCount = 0;
    TargetKind kind = TargetKind::NavigationItem;
    int secondaryTarget = 0;
    // Overlap is forbidden by default. Set this only for a deliberately
    // layered target pair whose painter/z-order is documented at the callsite.
    bool overlapAllowed = false;
  };

  struct Resolution {
    bool found = false;
    int currentIndex = 0;
    int targetIndex = 0;
    int itemCount = 0;
    TargetKind kind = TargetKind::NavigationItem;
    int secondaryTarget = 0;
    std::uint32_t generation = 0;
  };

  static constexpr std::size_t kMaxRegions = 64;

  static TouchUiRegistry& instance();

  void clear();
  bool registerItem(int x, int y, int width, int height, int currentIndex, int targetIndex, int itemCount);
  bool registerDirect(int x, int y, int width, int height, TargetKind kind, int target, int secondaryTarget = 0,
                      bool overlapAllowed = false);
  [[nodiscard]] Resolution resolve(int x, int y) const;
  [[nodiscard]] std::size_t size() const;
  // Counts intersecting active hitboxes that were not both explicitly marked
  // as an intentional painter-ordered overlap. This is a diagnostic/audit
  // primitive; resolving behaviour remains last-drawn-wins for compatibility.
  [[nodiscard]] std::size_t forbiddenOverlapCount() const;
  // Incremented before every render pass. A platform adapter may only route a
  // hit resolved from this exact visual generation.
  [[nodiscard]] std::uint32_t generation() const;
#ifdef KOBO_LINUX
  [[nodiscard]] std::size_t snapshot(std::array<Region, kMaxRegions>& destination) const;
#endif

 private:
  [[nodiscard]] static bool intersects(const Region& first, const Region& second);
  std::array<Region, kMaxRegions> regions_{};
  std::size_t count_ = 0;
  std::uint32_t generation_ = 0;
#ifdef KOBO_LINUX
  mutable std::mutex mutex_;
#endif
};

#define TOUCH_UI TouchUiRegistry::instance()
