#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "KoboPackedMono.h"

using crossink::kobo::KoboFbInkDisplay;
using crossink::kobo::SourceTransform;
using crossink::kobo::unpackPackedMono;

namespace {

using Packed = std::array<std::uint8_t, KoboFbInkDisplay::kPackedFrameBytes>;

void setBlack(Packed& packed, const std::uint16_t x, const std::uint16_t y) {
  const std::size_t offset = static_cast<std::size_t>(y) * (KoboFbInkDisplay::kPanelWidth / 8) + x / 8;
  packed[offset] &= static_cast<std::uint8_t>(~(0x80U >> (x % 8)));
}

[[noreturn]] void fail(const char* label) {
  std::cerr << label << '\n';
  std::exit(EXIT_FAILURE);
}

}  // namespace

int main() {
  static Packed packed{};
  packed.fill(0xFFU);
  setBlack(packed, 0, 0);
  setBlack(packed, KoboFbInkDisplay::kPanelWidth - 1, KoboFbInkDisplay::kPanelHeight - 1);

  std::vector<std::uint8_t> landscape(KoboFbInkDisplay::kPanelWidth * KoboFbInkDisplay::kPanelHeight);
  if (!unpackPackedMono(packed.data(), packed.size(), landscape.data(), landscape.size(), KoboFbInkDisplay::kPanelWidth,
                        KoboFbInkDisplay::kPanelWidth, KoboFbInkDisplay::kPanelHeight, SourceTransform::Identity) ||
      landscape.front() != 0 || landscape.back() != 0) {
    fail("identity corner mapping failed");
  }

  std::vector<std::uint8_t> portrait(KoboFbInkDisplay::kPortraitWidth * KoboFbInkDisplay::kPortraitHeight);
  if (!unpackPackedMono(packed.data(), packed.size(), portrait.data(), portrait.size(),
                        KoboFbInkDisplay::kPortraitWidth, KoboFbInkDisplay::kPortraitWidth,
                        KoboFbInkDisplay::kPortraitHeight, SourceTransform::RotateClockwise) ||
      portrait[KoboFbInkDisplay::kPortraitWidth - 1] != 0 ||
      portrait[(KoboFbInkDisplay::kPortraitHeight - 1) * KoboFbInkDisplay::kPortraitWidth] != 0) {
    fail("clockwise corner mapping failed");
  }

  portrait.assign(portrait.size(), 0xFFU);
  if (!unpackPackedMono(packed.data(), packed.size(), portrait.data(), portrait.size(),
                        KoboFbInkDisplay::kPortraitWidth, KoboFbInkDisplay::kPortraitWidth,
                        KoboFbInkDisplay::kPortraitHeight, SourceTransform::RotateCounterClockwise) ||
      portrait[(KoboFbInkDisplay::kPortraitHeight - 1) * KoboFbInkDisplay::kPortraitWidth] != 0 ||
      portrait[KoboFbInkDisplay::kPortraitWidth - 1] != 0) {
    fail("counterclockwise corner mapping failed");
  }
  return EXIT_SUCCESS;
}
