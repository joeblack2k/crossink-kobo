#include "ProgressiveJpegDecoder.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <stb_image.h>

#include <cstdint>
#include <cstdlib>
#include <limits>

#include "DirectPixelWriter.h"
#include "DitherUtils.h"
#include "ImageToFramebufferDecoder.h"
#include "PixelCache.h"

namespace {

constexpr size_t kMaxCompressedJpegBytes = 16U * 1024U * 1024U;
constexpr uint64_t kMaxDecodedPixels = 2048ULL * 3072ULL;

bool readWholeFile(const std::string& imagePath, uint8_t*& bytes, size_t& byteCount) {
  bytes = nullptr;
  byteCount = 0;

  FsFile file;
  if (!Storage.openFileForRead("JPG", imagePath, file)) return false;

  const size_t size = file.size();
  if (size == 0 || size > kMaxCompressedJpegBytes || size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    LOG_ERR("JPG", "Progressive JPEG file size rejected: %u bytes", static_cast<unsigned>(size));
    file.close();
    return false;
  }

  uint8_t* input = static_cast<uint8_t*>(malloc(size));
  if (!input) {
    LOG_ERR("JPG", "OOM reading progressive JPEG: %u bytes", static_cast<unsigned>(size));
    file.close();
    return false;
  }

  size_t offset = 0;
  while (offset < size) {
    const int read = file.read(input + offset, size - offset);
    if (read <= 0) {
      LOG_ERR("JPG", "Short read for progressive JPEG: %u/%u", static_cast<unsigned>(offset),
              static_cast<unsigned>(size));
      free(input);
      file.close();
      return false;
    }
    offset += static_cast<size_t>(read);
  }
  file.close();
  bytes = input;
  byteCount = size;
  return true;
}

bool calculateDestination(const RenderConfig& config, int sourceWidth, int sourceHeight, int& destinationWidth,
                          int& destinationHeight) {
  if (config.useExactDimensions && config.maxWidth > 0 && config.maxHeight > 0) {
    destinationWidth = config.maxWidth;
    destinationHeight = config.maxHeight;
  } else {
    const float scaleX = (config.maxWidth > 0 && sourceWidth > config.maxWidth)
                             ? static_cast<float>(config.maxWidth) / sourceWidth
                             : 1.0f;
    const float scaleY = (config.maxHeight > 0 && sourceHeight > config.maxHeight)
                             ? static_cast<float>(config.maxHeight) / sourceHeight
                             : 1.0f;
    const float scale = scaleX < scaleY ? scaleX : scaleY;
    destinationWidth = static_cast<int>(sourceWidth * scale);
    destinationHeight = static_cast<int>(sourceHeight * scale);
  }

  return destinationWidth > 0 && destinationHeight > 0;
}

}  // namespace

bool decodeProgressiveJpegToFramebuffer(const std::string& imagePath, GfxRenderer& renderer, const RenderConfig& config,
                                        int expectedWidth, int expectedHeight) {
  if (expectedWidth <= 0 || expectedHeight <= 0) return false;

  const uint64_t sourcePixels = static_cast<uint64_t>(expectedWidth) * expectedHeight;
  if (sourcePixels == 0 || sourcePixels > kMaxDecodedPixels) {
    LOG_ERR("JPG", "Progressive JPEG dimensions rejected: %dx%d", expectedWidth, expectedHeight);
    return false;
  }

  // stb_image retains the decoded grayscale raster while rendering.  Keep a
  // material free-heap margin for the EPUB layout and e-ink render planes.
  constexpr size_t kHeapSafetyMargin = 4U * 1024U * 1024U;
  const uint64_t requiredHeap = sourcePixels + kHeapSafetyMargin;
  if (requiredHeap > static_cast<uint64_t>(ESP.getFreeHeap())) {
    LOG_ERR("JPG", "Not enough heap for progressive JPEG: need %u, free %u", static_cast<unsigned>(requiredHeap),
            ESP.getFreeHeap());
    return false;
  }

  uint8_t* compressed = nullptr;
  size_t compressedSize = 0;
  if (!readWholeFile(imagePath, compressed, compressedSize)) return false;

  // SimulatorImageDecode.cpp already provides the sole stb_image
  // implementation in the Kobo target.  This translation unit imports only
  // the declarations, so it can keep the decoder-owned grayscale allocation
  // and avoid the temporary vector copy made by the simulator convenience API.
  int decodedWidth = 0;
  int decodedHeight = 0;
  int sourceComponents = 0;
  stbi_uc* pixels = stbi_load_from_memory(compressed, static_cast<int>(compressedSize), &decodedWidth, &decodedHeight,
                                          &sourceComponents, 1);
  free(compressed);
  compressed = nullptr;
  if (!pixels) {
    LOG_ERR("JPG", "Progressive JPEG decoder failed");
    return false;
  }

  if (decodedWidth != expectedWidth || decodedHeight != expectedHeight) {
    LOG_ERR("JPG", "Progressive JPEG dimensions changed while decoding: expected %dx%d, got %dx%d", expectedWidth,
            expectedHeight, decodedWidth, decodedHeight);
    stbi_image_free(pixels);
    return false;
  }

  int dstWidth = 0;
  int dstHeight = 0;
  if (!calculateDestination(config, decodedWidth, decodedHeight, dstWidth, dstHeight)) {
    LOG_ERR("JPG", "Degenerate progressive JPEG output for %s", imagePath.c_str());
    stbi_image_free(pixels);
    return false;
  }

  LOG_INF("JPG", "Progressive JPEG safe decode %dx%d -> %dx%d target=(%d,%d %dx%d)", decodedWidth, decodedHeight,
          dstWidth, dstHeight, config.x, config.y, config.maxWidth, config.maxHeight);

  PixelCache cache;
  bool caching = !config.cachePath.empty();
  if (caching && !cache.begin(config.cachePath, dstWidth, dstHeight, config.x, config.y, 1)) {
    LOG_ERR("JPG", "Failed to start progressive JPEG cache; rendering without cache");
    caching = false;
  }

  DirectPixelWriter pixelWriter;
  pixelWriter.init(renderer);
  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();
  const bool dither = config.useDithering;

  for (int dstY = 0; dstY < dstHeight; ++dstY) {
    const int sourceY = static_cast<int>((static_cast<int64_t>(dstY) * decodedHeight) / dstHeight);
    const int outY = config.y + dstY;
    const bool rowOnScreen = outY >= 0 && outY < screenHeight;
    if (rowOnScreen) pixelWriter.beginRow(outY);

    DirectCacheWriter cacheWriter;
    if (caching) {
      if (!cache.advanceTo(dstY)) {
        caching = false;
      } else {
        cacheWriter.init(cache.buffer, cache.bytesPerRow, cache.bandRows, cache.originX);
        cacheWriter.beginRow(outY, config.y + cache.bandStart);
      }
    }

    const uint8_t* sourceRow = pixels + static_cast<size_t>(sourceY) * decodedWidth;
    for (int dstX = 0; dstX < dstWidth; ++dstX) {
      const int sourceX = static_cast<int>((static_cast<int64_t>(dstX) * decodedWidth) / dstWidth);
      const int outX = config.x + dstX;
      const uint8_t gray = sourceRow[sourceX];
      const uint8_t output = dither ? applyBayerDither4Level(gray, outX, outY) : static_cast<uint8_t>(gray / 85);

      if (rowOnScreen && outX >= 0 && outX < screenWidth) {
        pixelWriter.writePixel(outX, output > 3 ? 3 : output);
        if (caching) cacheWriter.writePixel(outX, output > 3 ? 3 : output);
      }
    }
  }

  if (caching && !cache.finalize()) {
    LOG_ERR("JPG", "Progressive JPEG cache finalization failed");
  }
  stbi_image_free(pixels);
  return true;
}
