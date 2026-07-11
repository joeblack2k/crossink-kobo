#include "HalDisplay.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using crossink::kobo::RefreshKind;
using crossink::kobo::SourceTransform;

HalDisplay display;

SourceTransform HalDisplay::configuredTransform() {
  const char* configured = std::getenv("CROSSINK_FB_TRANSFORM");
  if (configured != nullptr && std::strcmp(configured, "identity") == 0) {
    return SourceTransform::Identity;
  }
  if (configured != nullptr && std::strcmp(configured, "counterclockwise") == 0) {
    return SourceTransform::RotateCounterClockwise;
  }
  // GfxRenderer writes a native 1448x1072 landscape buffer. The normal N437
  // FBInk geometry is portrait, so rotating clockwise restores logical
  // portrait. The environment override is retained for probe/recovery boots.
  return SourceTransform::RotateClockwise;
}

HalDisplay::HalDisplay() : fbInkDisplay_(configuredTransform()) { frameBuffer_.fill(0xFFU); }

RefreshKind HalDisplay::refreshKind(const RefreshMode mode) {
  switch (mode) {
    case FULL_REFRESH:
      return RefreshKind::Full;
    case HALF_REFRESH:
      return RefreshKind::Partial;
    case FAST_REFRESH:
      return RefreshKind::Fast;
  }
  return RefreshKind::Partial;
}

void HalDisplay::begin(bool /*seamless*/) {
  if (drmDisplay_.isOpen() || fbInkDisplay_.isOpen()) return;
  if (drmDisplay_.open()) {
    useDrm_ = true;
    std::fprintf(stderr, "[KOBO] modern DRM EPDC backend active\n");
    return;
  }
  const int drmError = drmDisplay_.lastError();
  useDrm_ = false;
  if (!fbInkDisplay_.open()) {
    std::fprintf(stderr, "[KOBO] display initialization failed (DRM=%d, FBInk=%d)\n", drmError,
                 fbInkDisplay_.lastError());
  }
}

void HalDisplay::clearScreen(const std::uint8_t color) const { frameBuffer_.fill(color); }

void HalDisplay::drawImage(const std::uint8_t* imageData, const std::uint16_t x, const std::uint16_t y,
                           const std::uint16_t width, const std::uint16_t height, bool /*fromProgmem*/) const {
  if (imageData == nullptr || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT || width == 0 || height == 0) return;
  const std::uint16_t sourceBytes = static_cast<std::uint16_t>((width + 7U) / 8U);
  const std::uint16_t destinationByte = x / 8U;
  const std::uint16_t copyBytes = std::min<std::uint16_t>(sourceBytes, DISPLAY_WIDTH_BYTES - destinationByte);
  const std::uint16_t rows = std::min<std::uint16_t>(height, DISPLAY_HEIGHT - y);
  for (std::uint16_t row = 0; row < rows; ++row) {
    std::memcpy(frameBuffer_.data() + static_cast<std::size_t>(y + row) * DISPLAY_WIDTH_BYTES + destinationByte,
                imageData + static_cast<std::size_t>(row) * sourceBytes, copyBytes);
  }
}

void HalDisplay::drawImageTransparent(const std::uint8_t* imageData, const std::uint16_t x, const std::uint16_t y,
                                      const std::uint16_t width, const std::uint16_t height,
                                      bool /*fromProgmem*/) const {
  if (imageData == nullptr || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT || width == 0 || height == 0) return;
  const std::uint16_t sourceBytes = static_cast<std::uint16_t>((width + 7U) / 8U);
  const std::uint16_t destinationByte = x / 8U;
  const std::uint16_t copyBytes = std::min<std::uint16_t>(sourceBytes, DISPLAY_WIDTH_BYTES - destinationByte);
  const std::uint16_t rows = std::min<std::uint16_t>(height, DISPLAY_HEIGHT - y);
  for (std::uint16_t row = 0; row < rows; ++row) {
    std::uint8_t* destination =
        frameBuffer_.data() + static_cast<std::size_t>(y + row) * DISPLAY_WIDTH_BYTES + destinationByte;
    const std::uint8_t* source = imageData + static_cast<std::size_t>(row) * sourceBytes;
    for (std::uint16_t byte = 0; byte < copyBytes; ++byte) {
      destination[byte] &= source[byte];
    }
  }
}

void HalDisplay::displayBuffer(const RefreshMode mode, bool /*turnOffScreen*/) { refreshDisplay(mode); }

void HalDisplay::refreshDisplay(const RefreshMode mode, bool /*turnOffScreen*/) {
  if (!drmDisplay_.isOpen() && !fbInkDisplay_.isOpen()) {
    begin();
  }
  if (!drmDisplay_.isOpen() && !fbInkDisplay_.isOpen()) return;
  const bool success = useDrm_ ? drmDisplay_.presentPackedMono(frameBuffer_.data(), frameBuffer_.size(), refreshKind(mode))
                               : fbInkDisplay_.presentPackedMono(frameBuffer_.data(), frameBuffer_.size(), refreshKind(mode));
  if (!success) {
    std::fprintf(stderr, "[KOBO] display refresh failed: %d\n",
                 useDrm_ ? drmDisplay_.lastError() : fbInkDisplay_.lastError());
  }
}

void HalDisplay::deepSleep() {
  drmDisplay_.close();
  fbInkDisplay_.close();
}

std::uint8_t* HalDisplay::getFrameBuffer() const { return const_cast<std::uint8_t*>(frameBuffer_.data()); }

void HalDisplay::preconditionGrayscale() {}

void HalDisplay::preconditionGrayscale(std::uint16_t /*x*/, std::uint16_t /*y*/, std::uint16_t /*width*/,
                                       std::uint16_t /*height*/) {}

void HalDisplay::displayGrayscaleBase(const RefreshMode fallback, const bool turnOffScreen) {
  displayBuffer(fallback, turnOffScreen);
}

void HalDisplay::copyGrayscaleBuffers(const std::uint8_t* /*lsbBuffer*/, const std::uint8_t* /*msbBuffer*/) {}

void HalDisplay::copyGrayscaleLsbBuffers(const std::uint8_t* /*lsbBuffer*/) {}

void HalDisplay::copyGrayscaleMsbBuffers(const std::uint8_t* /*msbBuffer*/) {}

void HalDisplay::cleanupGrayscaleBuffers(const std::uint8_t* /*bwBuffer*/) {}

void HalDisplay::displayGrayBuffer(const bool turnOffScreen) { displayBuffer(HALF_REFRESH, turnOffScreen); }

void HalDisplay::writeGrayscalePlaneStrip(bool /*lsbPlane*/, const std::uint8_t* /*rows*/, std::uint16_t /*yStart*/,
                                          std::uint16_t /*numRows*/) {}

bool HalDisplay::supportsStripGrayscale() const { return false; }
