#pragma once

#include <string>

class GfxRenderer;
struct RenderConfig;

// Decode a progressive JPEG through stb_image instead of JPEGDEC's DC-only
// MCU path.  The latter is known to fault on the N437 ARM build for valid
// progressive images.  This helper is deliberately only used for SOF2 JPEGs;
// baseline JPEGs retain the streaming JPEGDEC path.
bool decodeProgressiveJpegToFramebuffer(const std::string& imagePath, GfxRenderer& renderer, const RenderConfig& config,
                                        int expectedWidth, int expectedHeight);
