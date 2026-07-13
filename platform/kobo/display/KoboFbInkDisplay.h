// SPDX-License-Identifier: GPL-3.0-or-later
// CrossInk's Kobo framebuffer/EPDC adapter.  This belongs to the Linux HAL,
// never in an activity or reader implementation.
#pragma once

#include <cstddef>
#include <cstdint>

#include "KoboDisplayTypes.h"

namespace crossink::kobo {

enum class RefreshKind : uint8_t {
  Fast,
  Partial,
  Full,
};

enum class SourceTransform : uint8_t {
  Identity,
  RotateClockwise,
  RotateCounterClockwise,
};

struct DisplayGeometry {
  uint16_t width = 0;
  uint16_t height = 0;
  uint32_t stride = 0;
  uint8_t bitsPerPixel = 0;
  bool nativeLandscape = false;
};

// Presents CrossInk/GfxRenderer's native landscape 1448x1072 packed buffer.
// GfxRenderer rotates its logical 1072x1448 portrait UI into this buffer. A
// one bit represents white; this matches HalDisplay::clearScreen(0xff).
class KoboFbInkDisplay {
 public:
  static constexpr uint16_t kPortraitWidth = 1072;
  static constexpr uint16_t kPortraitHeight = 1448;
  static constexpr uint16_t kPanelWidth = kPortraitHeight;
  static constexpr uint16_t kPanelHeight = kPortraitWidth;
  static constexpr size_t kPackedFrameBytes = static_cast<size_t>(kPanelWidth / 8) * kPanelHeight;

  explicit KoboFbInkDisplay(SourceTransform transform = SourceTransform::Identity) : transform_(transform) {}
  ~KoboFbInkDisplay();
  KoboFbInkDisplay(const KoboFbInkDisplay&) = delete;
  KoboFbInkDisplay& operator=(const KoboFbInkDisplay&) = delete;

  // Does not force a bit depth or rotation. The constructor transform must
  // come from the recorded N437 hardware probe; a mismatched transform fails
  // closed instead of guessing a landscape direction.
  bool open();
  void close();
  bool isOpen() const { return fbfd_ >= 0; }
  const DisplayGeometry& geometry() const { return geometry_; }

  // Copy a complete 1-bit frame to an already-probed 8-bpp framebuffer and
  // refresh it.  Full is used after boot and periodically; Fast/Partial are
  // safe non-flashing modes until waveform measurements permit faster tuning.
  bool presentPackedMono(const uint8_t* packed, size_t packedSize, RefreshKind kind);
  bool presentPackedMono(const uint8_t* packed, size_t packedSize, RefreshKind kind, const RefreshRegion& region);

  // The caller records every failure in /data/.crossink/crash; this avoids
  // mutating persistent state from a lowest-level display adapter.
  int lastError() const { return lastError_; }

 private:
  bool refresh(RefreshKind kind);
  bool copyPackedToFramebuffer(const uint8_t* packed);

  int fbfd_ = -1;
  unsigned char* framebuffer_ = nullptr;
  size_t framebufferSize_ = 0;
  DisplayGeometry geometry_{};
  SourceTransform transform_ = SourceTransform::Identity;
  uint8_t partialSinceFull_ = 0;
  int lastError_ = 0;
};

}  // namespace crossink::kobo
