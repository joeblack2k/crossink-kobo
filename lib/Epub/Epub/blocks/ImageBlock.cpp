#include "ImageBlock.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Serialization.h>
#include <fontIds.h>

#include "Epub/converters/DirectPixelWriter.h"
#include "Epub/converters/ImageDecoderFactory.h"
#include "Epub/converters/PixelCache.h"

// Cache file format:
// - uint32_t magic ("PXC1")
// - uint8_t format version
// - uint8_t bits per pixel
// - uint16_t width
// - uint16_t height
// - uint8_t pixels[...] - 2 bits per pixel, packed (4 pixels per byte), row-major order

ImageBlock::ImageBlock(const std::string& imagePath, int16_t width, int16_t height)
    : imagePath(imagePath), width(width), height(height) {}

bool ImageBlock::imageExists() const { return Storage.exists(imagePath.c_str()); }

namespace {

std::string getCachePath(const std::string& imagePath) {
  // Replace extension with .pxc (pixel cache)
  size_t dotPos = imagePath.rfind('.');
  if (dotPos != std::string::npos) {
    return imagePath.substr(0, dotPos) + ".pxc";
  }
  return imagePath + ".pxc";
}

void renderDecodeFailurePlaceholder(GfxRenderer& renderer, const int x, const int y, const int width,
                                    const int height) {
  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();
  const int left = x < 0 ? 0 : x;
  const int top = y < 0 ? 0 : y;
  const int right = x + width > screenWidth ? screenWidth : x + width;
  const int bottom = y + height > screenHeight ? screenHeight : y + height;
  if (left >= right || top >= bottom) return;

  const int visibleWidth = right - left;
  const int visibleHeight = bottom - top;
  // Clear the complete image rect first.  That makes a failed decode local and
  // prevents stale pixels from an earlier image from surviving a partial pass.
  renderer.fillRect(left, top, visibleWidth, visibleHeight, false);
  renderer.drawRect(left, top, visibleWidth, visibleHeight, true);
  renderer.drawLine(left, top, right - 1, bottom - 1, true);
  renderer.drawLine(right - 1, top, left, bottom - 1, true);

  const char* label = tr(STR_IMAGES_PLACEHOLDER);
  const int labelWidth = renderer.getTextWidth(SMALL_FONT_ID, label);
  const int labelX = left + (visibleWidth - labelWidth) / 2;
  const int labelY = top + (visibleHeight - renderer.getLineHeight(SMALL_FONT_ID)) / 2;
  renderer.fillRect(labelX - 4, labelY - 2, labelWidth + 8, renderer.getLineHeight(SMALL_FONT_ID) + 4, false);
  renderer.drawText(SMALL_FONT_ID, labelX, labelY, label, true);
}

void clampCachedRowsToLandscapeStrip(const GfxRenderer& renderer, const int imageY, int& rowStart, int& rowEnd) {
  if (!renderer.isStripTargetActive()) {
    return;
  }

  const int stripY0 = renderer.getWriteOriginY();
  const int stripY1Exclusive = stripY0 + renderer.getWriteRows();
  int logicalY0;
  int logicalY1Exclusive;

  switch (renderer.getOrientation()) {
    case GfxRenderer::LandscapeCounterClockwise:
      logicalY0 = stripY0;
      logicalY1Exclusive = stripY1Exclusive;
      break;
    case GfxRenderer::LandscapeClockwise:
      logicalY0 = renderer.getDisplayHeight() - stripY1Exclusive;
      logicalY1Exclusive = renderer.getDisplayHeight() - stripY0;
      break;
    default:
      return;
  }

  const int stripRowStart = logicalY0 - imageY;
  const int stripRowEnd = logicalY1Exclusive - imageY;
  if (rowStart < stripRowStart) rowStart = stripRowStart;
  if (rowEnd > stripRowEnd) rowEnd = stripRowEnd;
}

bool renderFromCache(GfxRenderer& renderer, const std::string& cachePath, int x, int y, int expectedWidth,
                     int expectedHeight) {
  FsFile cacheFile;
  if (!Storage.openFileForRead("IMG", cachePath, cacheFile)) {
    return false;
  }

  uint32_t magic = 0;
  uint8_t version = 0;
  uint8_t bitsPerPixel = 0;
  uint16_t cachedWidth = 0;
  uint16_t cachedHeight = 0;
  if (cacheFile.read(&magic, sizeof(magic)) != sizeof(magic) ||
      cacheFile.read(&version, sizeof(version)) != sizeof(version) ||
      cacheFile.read(&bitsPerPixel, sizeof(bitsPerPixel)) != sizeof(bitsPerPixel) ||
      cacheFile.read(&cachedWidth, sizeof(cachedWidth)) != sizeof(cachedWidth) ||
      cacheFile.read(&cachedHeight, sizeof(cachedHeight)) != sizeof(cachedHeight)) {
    LOG_ERR("IMG", "Cache header is truncated: %s", cachePath.c_str());
    cacheFile.close();
    Storage.remove(cachePath.c_str());
    return false;
  }
  if (magic != pixelcache::kMagic || version != pixelcache::kVersion || bitsPerPixel != pixelcache::kBitsPerPixel) {
    LOG_ERR("IMG", "Cache header mismatch: %s (magic=%08x version=%u bpp=%u)", cachePath.c_str(), magic,
            static_cast<unsigned>(version), static_cast<unsigned>(bitsPerPixel));
    cacheFile.close();
    Storage.remove(cachePath.c_str());
    return false;
  }

  // Verify dimensions are close (allow 1 pixel tolerance for rounding differences)
  int widthDiff = abs(cachedWidth - expectedWidth);
  int heightDiff = abs(cachedHeight - expectedHeight);
  if (widthDiff > 1 || heightDiff > 1) {
    LOG_ERR("IMG", "Cache dimension mismatch: %dx%d vs %dx%d", cachedWidth, cachedHeight, expectedWidth,
            expectedHeight);
    cacheFile.close();
    Storage.remove(cachePath.c_str());
    return false;
  }

  // Use cached dimensions for rendering (they're the actual decoded size)
  expectedWidth = cachedWidth;
  expectedHeight = cachedHeight;

  LOG_DBG("IMG", "Cache render: %s source=%dx%d target=(%d,%d %dx%d) orientation=%d", cachePath.c_str(),
          cachedWidth, cachedHeight, x, y, expectedWidth, expectedHeight, static_cast<int>(renderer.getOrientation()));

  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();
  int clipXStart = 0;
  int clipYStart = 0;
  int clipXEnd = cachedWidth;
  int clipYEnd = cachedHeight;
  if (x < 0) clipXStart = -x;
  if (y < 0) clipYStart = -y;
  if (screenWidth - x < clipXEnd) clipXEnd = screenWidth - x;
  if (screenHeight - y < clipYEnd) clipYEnd = screenHeight - y;

  if (clipXStart >= clipXEnd || clipYStart >= clipYEnd) {
    LOG_DBG("IMG", "Cached image is outside screen after clipping");
    cacheFile.close();
    return true;
  }

  int renderRowStart = clipYStart;
  int renderRowEnd = clipYEnd;
  clampCachedRowsToLandscapeStrip(renderer, y, renderRowStart, renderRowEnd);
  if (renderRowStart >= renderRowEnd) {
    cacheFile.close();
    return true;
  }

  // Read several rows per SD access. A full-page image is re-rendered on every
  // grayscale strip pass (~14x per page), and a one-row-per-read loop here means
  // cachedHeight (~728) tiny reads through the storage mutex + SdFat each time —
  // the dominant cost of displaying an image page. Batching rows into a ~4KB
  // buffer cuts that to ~20 reads per pass without holding the whole image.
  const int bytesPerRow = (cachedWidth + 3) / 4;  // 2 bits per pixel, 4 pixels per byte
  const std::size_t expectedCacheBytes = pixelcache::kHeaderSize + static_cast<std::size_t>(bytesPerRow) * cachedHeight;
  if (cacheFile.size() != expectedCacheBytes) {
    LOG_ERR("IMG", "Cache size mismatch: %s has %u bytes, expected %u", cachePath.c_str(),
            static_cast<unsigned>(cacheFile.size()), static_cast<unsigned>(expectedCacheBytes));
    cacheFile.close();
    Storage.remove(cachePath.c_str());
    return false;
  }
  const int rowsToRender = renderRowEnd - renderRowStart;
  int rowsPerRead = 4096 / bytesPerRow;
  if (rowsPerRead < 1) rowsPerRead = 1;
  if (rowsPerRead > rowsToRender) rowsPerRead = rowsToRender;
  uint8_t* readBuffer = (uint8_t*)malloc((size_t)rowsPerRead * bytesPerRow);
  if (!readBuffer) {
    // Fall back to a single-row buffer under memory pressure.
    rowsPerRead = 1;
    readBuffer = (uint8_t*)malloc(bytesPerRow);
  }
  if (!readBuffer) {
    LOG_ERR("IMG", "Failed to allocate row buffer");
    cacheFile.close();
    return false;
  }

  // DirectPixelWriter deliberately delegates to GfxRenderer on Kobo.  That
  // gives cache and first-decode paths the same portrait-to-panel transform
  // and, importantly, the same BW/gray-plane mapping.
  DirectPixelWriter pw;
  pw.init(renderer);

  const size_t dataOffset = pixelcache::kHeaderSize + static_cast<size_t>(renderRowStart) * static_cast<size_t>(bytesPerRow);
  if (!cacheFile.seek(dataOffset)) {
    LOG_ERR("IMG", "Cache seek error at row %d", renderRowStart);
    free(readBuffer);
    cacheFile.close();
    return false;
  }

  int rowsInBuffer = 0;
  int bufferRow = 0;
  for (int row = renderRowStart; row < renderRowEnd; row++) {
    if (bufferRow >= rowsInBuffer) {
      const int toRead = (renderRowEnd - row < rowsPerRead) ? (renderRowEnd - row) : rowsPerRead;
      const size_t bytes = (size_t)toRead * bytesPerRow;
      if (cacheFile.read(readBuffer, bytes) != static_cast<int>(bytes)) {
        LOG_ERR("IMG", "Cache read error at row %d", row);
        free(readBuffer);
        cacheFile.close();
        return false;
      }
      rowsInBuffer = toRead;
      bufferRow = 0;
    }

    const uint8_t* rowBuffer = readBuffer + (size_t)bufferRow * bytesPerRow;
    bufferRow++;

    if (row < clipYStart) continue;
    if (row >= clipYEnd) break;

    const int destY = y + row;
    pw.beginRow(destY);
    // Walk only the on-screen columns: writePixel drops off-band rows but does
    // not clip X, so this range is what keeps a partially off-screen image
    // inside the framebuffer.
    for (int col = clipXStart; col < clipXEnd; col++) {
      const int byteIdx = col >> 2;            // col / 4
      const int bitShift = 6 - (col & 3) * 2;  // MSB first within byte
      uint8_t pixelValue = (rowBuffer[byteIdx] >> bitShift) & 0x03;

      pw.writePixel(x + col, pixelValue);
    }
  }

  free(readBuffer);
  cacheFile.close();
  LOG_DBG("IMG", "Cache render complete");
  return true;
}

}  // namespace

void ImageBlock::render(GfxRenderer& renderer, const int x, const int y) {
  // The font-prewarm scan pass only accumulates glyphs; an image contributes
  // none, and its DirectPixelWriter output bypasses the renderer's scan-mode
  // suppression, so it would otherwise do a full (discarded) cache render every
  // page view. Skip it here. The image still draws in the real BW/grayscale
  // passes; on first view this just moves the one-time decode to the BW pass.
  FontCacheManager* fcm = renderer.getFontCacheManager();
  if (fcm && fcm->isScanning()) return;

  LOG_DBG("IMG", "Rendering image at %d,%d: %s (%dx%d)", x, y, imagePath.c_str(), width, height);

  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();

  if (width <= 0 || height <= 0) {
    LOG_ERR("IMG", "Invalid image size: %dx%d", width, height);
    return;
  }

  // Reject only fully off-screen images. Decoders and cache rendering clip
  // partially visible images to the logical screen bounds.
  if (x >= screenWidth || y >= screenHeight || x + width <= 0 || y + height <= 0) {
    LOG_ERR("IMG", "Invalid render position: (%d,%d) size (%dx%d) screen (%dx%d)", x, y, width, height, screenWidth,
            screenHeight);
    return;
  }
  const bool fullyOnScreen = x >= 0 && y >= 0 && x + width <= screenWidth && y + height <= screenHeight;

  // Tiled grayscale (#2190): skip the whole image when it doesn't touch the
  // active band. The per-pixel writer already clips off-band pixels, but without
  // this each of the ~7 bands per plane re-ran the full cache load / pixel walk
  // and discarded the result — the dominant cost of AA on image pages. The check
  // is orientation-aware and returns true when no strip is active, so the BW
  // pass and non-tiled controllers render the image exactly as before.
  if (!renderer.glyphIntersectsStrip(x, y, x + width - 1, y + height - 1)) {
    return;
  }

  // Try to render from cache first
  std::string cachePath = getCachePath(imagePath);
  if (renderFromCache(renderer, cachePath, x, y, width, height)) {
    return;  // Successfully rendered from cache
  }

  // No cache - need to decode the image
  // Check if image file exists
  FsFile file;
  if (!Storage.openFileForRead("IMG", imagePath, file)) {
    LOG_ERR("IMG", "Image file not found: %s", imagePath.c_str());
    renderDecodeFailurePlaceholder(renderer, x, y, width, height);
    return;
  }
  size_t fileSize = file.size();
  file.close();

  if (fileSize == 0) {
    LOG_ERR("IMG", "Image file is empty: %s", imagePath.c_str());
    renderDecodeFailurePlaceholder(renderer, x, y, width, height);
    return;
  }

  LOG_DBG("IMG", "Decoding and caching: %s", imagePath.c_str());

  RenderConfig config;
  config.x = x;
  config.y = y;
  config.maxWidth = width;
  config.maxHeight = height;
  config.useGrayscale = true;
  config.useDithering = true;
  config.performanceMode = false;
  config.useExactDimensions = true;  // Use pre-calculated dimensions to avoid rounding mismatches
  if (fullyOnScreen) {
    config.cachePath = cachePath;  // Enable caching during decode
  }

  ImageToFramebufferDecoder* decoder = ImageDecoderFactory::getDecoder(imagePath);
  if (!decoder) {
    LOG_ERR("IMG", "No decoder found for image: %s", imagePath.c_str());
    renderDecodeFailurePlaceholder(renderer, x, y, width, height);
    return;
  }

  LOG_DBG("IMG", "Using %s decoder", decoder->getFormatName());

  bool success = decoder->decodeToFramebuffer(imagePath, renderer, config);
  if (!success) {
    LOG_ERR("IMG", "Failed to decode image: %s", imagePath.c_str());
    renderDecodeFailurePlaceholder(renderer, x, y, width, height);
    return;
  }

  LOG_DBG("IMG", "Decode successful");
}

bool ImageBlock::serialize(FsFile& file) {
  return serialization::tryWriteString(file, imagePath) && serialization::tryWritePod(file, width) &&
         serialization::tryWritePod(file, height);
}

std::unique_ptr<ImageBlock> ImageBlock::deserialize(FsFile& file) {
  std::string path;
  if (!serialization::tryReadString(file, path)) {
    LOG_ERR("IMG", "Deserialization failed: could not read image path");
    return nullptr;
  }
  int16_t w, h;
  if (!serialization::tryReadPod(file, w) || !serialization::tryReadPod(file, h)) {
    LOG_ERR("IMG", "Deserialization failed: truncated image metadata");
    return nullptr;
  }

  auto* imageBlock = new (std::nothrow) ImageBlock(path, w, h);
  if (!imageBlock) {
    LOG_ERR("IMG", "Deserialization failed: could not allocate ImageBlock");
    return nullptr;
  }
  return std::unique_ptr<ImageBlock>(imageBlock);
}
