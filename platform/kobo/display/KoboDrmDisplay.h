// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

#include <xf86drmMode.h>

#include "KoboFbInkDisplay.h"

namespace crossink::kobo {

// Native backend for akemnade/linux's DRM mxc-epdc driver. FBInk remains a
// fallback for legacy framebuffer kernels, whose MXCFB ioctls are different.
class KoboDrmDisplay {
 public:
  KoboDrmDisplay() = default;
  ~KoboDrmDisplay();
  KoboDrmDisplay(const KoboDrmDisplay&) = delete;
  KoboDrmDisplay& operator=(const KoboDrmDisplay&) = delete;

  bool open();
  void close();
  [[nodiscard]] bool isOpen() const { return fd_ >= 0; }
  [[nodiscard]] int lastError() const { return lastError_; }
  bool presentPackedMono(const std::uint8_t* packed, std::size_t packedSize, RefreshKind kind);

 private:
  bool selectOutput();
  bool createBuffer();
  bool requestRefresh(RefreshKind kind);
  void destroyBuffer();

  int fd_ = -1;
  int lastError_ = 0;
  std::uint32_t connectorId_ = 0;
  std::uint32_t crtcId_ = 0;
  drmModeModeInfo mode_{};
  drmModeCrtc* originalCrtc_ = nullptr;
  std::uint32_t handle_ = 0;
  std::uint32_t framebufferId_ = 0;
  std::uint32_t pitch_ = 0;
  std::uint64_t bufferSize_ = 0;
  std::uint8_t* map_ = nullptr;
};

}  // namespace crossink::kobo
