#pragma once

#include <array>
#include <cstdint>

#include "KoboFbInkDisplay.h"
#include "KoboDrmDisplay.h"

class HalDisplay {
 public:
  enum RefreshMode { FULL_REFRESH, HALF_REFRESH, FAST_REFRESH };

  static constexpr std::uint16_t DISPLAY_WIDTH = crossink::kobo::KoboFbInkDisplay::kPanelWidth;
  static constexpr std::uint16_t DISPLAY_HEIGHT = crossink::kobo::KoboFbInkDisplay::kPanelHeight;
  static constexpr std::uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr std::uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;

  HalDisplay();
  ~HalDisplay() = default;

  void begin(bool seamless = false);
  void clearScreen(std::uint8_t color = 0xFF) const;
  void drawImage(const std::uint8_t* imageData, std::uint16_t x, std::uint16_t y, std::uint16_t width,
                 std::uint16_t height, bool fromProgmem = false) const;
  void drawImageTransparent(const std::uint8_t* imageData, std::uint16_t x, std::uint16_t y, std::uint16_t width,
                            std::uint16_t height, bool fromProgmem = false) const;
  void displayBuffer(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false);
  void refreshDisplay(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false);
  void deepSleep();
  std::uint8_t* getFrameBuffer() const;

  void preconditionGrayscale();
  void preconditionGrayscale(std::uint16_t x, std::uint16_t y, std::uint16_t width, std::uint16_t height);
  void displayGrayscaleBase(RefreshMode fallback = HALF_REFRESH, bool turnOffScreen = false);
  void copyGrayscaleBuffers(const std::uint8_t* lsbBuffer, const std::uint8_t* msbBuffer);
  void copyGrayscaleLsbBuffers(const std::uint8_t* lsbBuffer);
  void copyGrayscaleMsbBuffers(const std::uint8_t* msbBuffer);
  void cleanupGrayscaleBuffers(const std::uint8_t* bwBuffer);
  void displayGrayBuffer(bool turnOffScreen = false);
  void writeGrayscalePlaneStrip(bool lsbPlane, const std::uint8_t* rows, std::uint16_t yStart, std::uint16_t numRows);
  [[nodiscard]] bool supportsStripGrayscale() const;

  [[nodiscard]] std::uint16_t getDisplayWidth() const { return DISPLAY_WIDTH; }
  [[nodiscard]] std::uint16_t getDisplayHeight() const { return DISPLAY_HEIGHT; }
  [[nodiscard]] std::uint16_t getDisplayWidthBytes() const { return DISPLAY_WIDTH_BYTES; }
  [[nodiscard]] std::uint32_t getBufferSize() const { return BUFFER_SIZE; }

 private:
  static crossink::kobo::SourceTransform configuredTransform();
  static crossink::kobo::RefreshKind refreshKind(RefreshMode mode);

  mutable std::array<std::uint8_t, BUFFER_SIZE> frameBuffer_{};
  crossink::kobo::KoboDrmDisplay drmDisplay_;
  crossink::kobo::KoboFbInkDisplay fbInkDisplay_;
  bool useDrm_ = false;
};

extern HalDisplay display;
