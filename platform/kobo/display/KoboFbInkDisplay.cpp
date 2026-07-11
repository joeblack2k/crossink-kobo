// SPDX-License-Identifier: GPL-3.0-or-later
#include "KoboFbInkDisplay.h"

#include <cerrno>

#include "KoboPackedMono.h"

extern "C" {
#include <fbink.h>
}

namespace crossink::kobo {
namespace {

FBInkConfig configFor(RefreshKind kind) {
  FBInkConfig config{};
  config.is_quiet = true;
  switch (kind) {
    case RefreshKind::Fast:
      // DU is available on early i.MX EPDCs and is a safer first choice
      // than A2 while the N437 waveform table is being characterized.
      config.wfm_mode = WFM_DU;
      break;
    case RefreshKind::Partial:
      // Let the EPDC choose a waveform for mixed text/image regions.  GC4
      // is not guaranteed on the oldest Kobo i.MX controllers.
      config.wfm_mode = WFM_AUTO;
      break;
    case RefreshKind::Full:
      config.wfm_mode = WFM_GC16;
      config.is_flashing = true;
      break;
  }
  return config;
}

}  // namespace

KoboFbInkDisplay::~KoboFbInkDisplay() { close(); }

bool KoboFbInkDisplay::open() {
  close();
  FBInkConfig config{};
  config.is_quiet = true;
  fbfd_ = fbink_open();
  if (fbfd_ < 0) {
    lastError_ = fbfd_;
    fbfd_ = -1;
    return false;
  }
  const int result = fbink_init(fbfd_, &config);
  if (result != 0) {
    lastError_ = result;
    close();
    return false;
  }

  FBInkState state{};
  fbink_get_state(&config, &state);
  geometry_.width = static_cast<uint16_t>(state.screen_width);
  geometry_.height = static_cast<uint16_t>(state.screen_height);
  geometry_.stride = state.scanline_stride;
  geometry_.bitsPerPixel = static_cast<uint8_t>(state.bpp);
  geometry_.nativeLandscape = state.screen_width == kPanelWidth && state.screen_height == kPanelHeight;
  framebuffer_ = fbink_get_fb_pointer(fbfd_, &framebufferSize_);
  const bool landscapeMatches = geometry_.nativeLandscape && transform_ == SourceTransform::Identity;
  const bool portraitMatches = geometry_.width == kPortraitWidth && geometry_.height == kPortraitHeight &&
                               transform_ != SourceTransform::Identity;
  if (framebuffer_ == nullptr || geometry_.bitsPerPixel != 8 || (!landscapeMatches && !portraitMatches)) {
    lastError_ = EOPNOTSUPP;
    close();
    return false;
  }
  lastError_ = 0;
  return true;
}

void KoboFbInkDisplay::close() {
  if (fbfd_ >= 0) {
    fbink_close(fbfd_);
  }
  fbfd_ = -1;
  framebuffer_ = nullptr;
  framebufferSize_ = 0;
  geometry_ = {};
  partialSinceFull_ = 0;
}

bool KoboFbInkDisplay::copyPackedToFramebuffer(const uint8_t* packed) {
  if (framebuffer_ == nullptr) return false;
  return unpackPackedMono(packed, kPackedFrameBytes, framebuffer_, framebufferSize_, geometry_.stride, geometry_.width,
                          geometry_.height, transform_);
}

bool KoboFbInkDisplay::refresh(RefreshKind kind) {
  FBInkConfig config = configFor(kind);
  const int result = fbink_refresh(fbfd_, 0, 0, 0, 0, &config);
  if (result != 0) {
    lastError_ = result;
    return false;
  }
  return true;
}

bool KoboFbInkDisplay::presentPackedMono(const uint8_t* packed, size_t packedSize, RefreshKind kind) {
  if (!isOpen() || packed == nullptr || packedSize != kPackedFrameBytes) {
    lastError_ = EINVAL;
    return false;
  }
  if (partialSinceFull_ >= 5U) kind = RefreshKind::Full;
  if (!copyPackedToFramebuffer(packed) || !refresh(kind)) return false;
  partialSinceFull_ = kind == RefreshKind::Full ? 0U : static_cast<uint8_t>(partialSinceFull_ + 1U);
  lastError_ = 0;
  return true;
}

}  // namespace crossink::kobo
