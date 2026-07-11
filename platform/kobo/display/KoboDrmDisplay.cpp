// SPDX-License-Identifier: GPL-3.0-or-later
#include "KoboDrmDisplay.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <drm.h>
#include <drm_mode.h>
#include <xf86drm.h>

namespace crossink::kobo {
namespace {

constexpr std::uint32_t kRefreshCommand = 0;
constexpr std::uint32_t kWaveformAuto = 257;
constexpr std::uint32_t kWaveformDu = 1;
constexpr std::uint32_t kWaveformGc16 = 2;
constexpr std::uint32_t kUpdatePartial = 0;
constexpr std::uint32_t kUpdateFull = 1;

struct MxcEpdcRefresh {
  std::uint32_t waveformMode;
  std::uint32_t updateMode;
};

bool packedPixelIsWhite(const std::uint8_t* packed, const std::uint16_t x, const std::uint16_t y) {
  const std::size_t offset = static_cast<std::size_t>(y) * (KoboFbInkDisplay::kPanelWidth / 8U) + x / 8U;
  return (packed[offset] & static_cast<std::uint8_t>(0x80U >> (x % 8U))) != 0;
}

int koboDrmIoctl(const int fd, const unsigned long request, void* argument) {
  return ::ioctl(fd, static_cast<int>(request), argument);
}

}  // namespace

KoboDrmDisplay::~KoboDrmDisplay() { close(); }

bool KoboDrmDisplay::open() {
  close();
  fd_ = ::open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
  if (fd_ < 0) {
    lastError_ = errno;
    return false;
  }
  if (!selectOutput() || !createBuffer()) {
    close();
    return false;
  }
  std::memset(map_, 0xFF, static_cast<std::size_t>(bufferSize_));
  if (drmModeSetCrtc(fd_, crtcId_, framebufferId_, 0, 0, &connectorId_, 1, &mode_) != 0) {
    lastError_ = errno;
    close();
    return false;
  }
  lastError_ = 0;
  return true;
}

bool KoboDrmDisplay::selectOutput() {
  drmModeRes* resources = drmModeGetResources(fd_);
  if (resources == nullptr) {
    lastError_ = errno;
    return false;
  }
  bool found = false;
  for (int index = 0; index < resources->count_connectors && !found; ++index) {
    drmModeConnector* connector = drmModeGetConnector(fd_, resources->connectors[index]);
    if (connector == nullptr) continue;
    if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
      int selectedMode = 0;
      for (int mode = 0; mode < connector->count_modes; ++mode) {
        if ((connector->modes[mode].type & DRM_MODE_TYPE_PREFERRED) != 0) selectedMode = mode;
        if (connector->modes[mode].hdisplay == KoboFbInkDisplay::kPanelWidth &&
            connector->modes[mode].vdisplay == KoboFbInkDisplay::kPanelHeight) {
          selectedMode = mode;
          break;
        }
      }
      mode_ = connector->modes[selectedMode];
      connectorId_ = connector->connector_id;
      drmModeEncoder* encoder = connector->encoder_id != 0 ? drmModeGetEncoder(fd_, connector->encoder_id) : nullptr;
      if (encoder != nullptr && encoder->crtc_id != 0) {
        crtcId_ = encoder->crtc_id;
      } else {
        for (int encoderIndex = 0; encoderIndex < connector->count_encoders && crtcId_ == 0; ++encoderIndex) {
          drmModeEncoder* candidate = drmModeGetEncoder(fd_, connector->encoders[encoderIndex]);
          if (candidate != nullptr) {
            for (int crtc = 0; crtc < resources->count_crtcs; ++crtc) {
              if ((candidate->possible_crtcs & (1U << crtc)) != 0) {
                crtcId_ = resources->crtcs[crtc];
                break;
              }
            }
            drmModeFreeEncoder(candidate);
          }
        }
      }
      if (encoder != nullptr) drmModeFreeEncoder(encoder);
      found = crtcId_ != 0 && mode_.hdisplay == KoboFbInkDisplay::kPanelWidth &&
              mode_.vdisplay == KoboFbInkDisplay::kPanelHeight;
    }
    drmModeFreeConnector(connector);
  }
  if (found) originalCrtc_ = drmModeGetCrtc(fd_, crtcId_);
  drmModeFreeResources(resources);
  if (!found) lastError_ = ENODEV;
  return found;
}

bool KoboDrmDisplay::createBuffer() {
  drm_mode_create_dumb create{};
  create.width = mode_.hdisplay;
  create.height = mode_.vdisplay;
  create.bpp = 32;
  if (koboDrmIoctl(fd_, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
    lastError_ = errno;
    return false;
  }
  handle_ = create.handle;
  pitch_ = create.pitch;
  bufferSize_ = create.size;
  if (drmModeAddFB(fd_, create.width, create.height, 24, 32, pitch_, handle_, &framebufferId_) != 0) {
    lastError_ = errno;
    destroyBuffer();
    return false;
  }
  drm_mode_map_dumb map{};
  map.handle = handle_;
  if (koboDrmIoctl(fd_, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0) {
    lastError_ = errno;
    destroyBuffer();
    return false;
  }
  void* address = mmap(nullptr, bufferSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, map.offset);
  if (address == MAP_FAILED) {
    lastError_ = errno;
    map_ = nullptr;
    destroyBuffer();
    return false;
  }
  map_ = static_cast<std::uint8_t*>(address);
  return true;
}

bool KoboDrmDisplay::requestRefresh(const RefreshKind kind) {
  MxcEpdcRefresh refresh{};
  switch (kind) {
    case RefreshKind::Fast:
      refresh = {kWaveformDu, kUpdatePartial};
      break;
    case RefreshKind::Partial:
      refresh = {kWaveformAuto, kUpdatePartial};
      break;
    case RefreshKind::Full:
      refresh = {kWaveformGc16, kUpdateFull};
      break;
  }
  if (drmCommandWrite(fd_, kRefreshCommand, &refresh, sizeof(refresh)) != 0) {
    lastError_ = errno;
    return false;
  }
  drmModeClip clip{};
  clip.x1 = 0;
  clip.y1 = 0;
  clip.x2 = mode_.hdisplay;
  clip.y2 = mode_.vdisplay;
  if (drmModeDirtyFB(fd_, framebufferId_, &clip, 1) != 0) {
    lastError_ = errno;
    return false;
  }
  return true;
}

bool KoboDrmDisplay::presentPackedMono(const std::uint8_t* packed, const std::size_t packedSize,
                                       const RefreshKind kind) {
  if (!isOpen() || packed == nullptr || packedSize != KoboFbInkDisplay::kPackedFrameBytes || map_ == nullptr) {
    lastError_ = EINVAL;
    return false;
  }
  for (std::uint16_t y = 0; y < KoboFbInkDisplay::kPanelHeight; ++y) {
    auto* row = reinterpret_cast<std::uint32_t*>(map_ + static_cast<std::size_t>(y) * pitch_);
    for (std::uint16_t x = 0; x < KoboFbInkDisplay::kPanelWidth; ++x) {
      row[x] = packedPixelIsWhite(packed, x, y) ? 0x00FFFFFFU : 0x00000000U;
    }
  }
  if (!requestRefresh(kind)) return false;
  lastError_ = 0;
  return true;
}

void KoboDrmDisplay::destroyBuffer() {
  if (map_ != nullptr) {
    munmap(map_, bufferSize_);
    map_ = nullptr;
  }
  if (framebufferId_ != 0 && fd_ >= 0) drmModeRmFB(fd_, framebufferId_);
  framebufferId_ = 0;
  if (handle_ != 0 && fd_ >= 0) {
    drm_mode_destroy_dumb destroy{};
    destroy.handle = handle_;
    koboDrmIoctl(fd_, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
  }
  handle_ = 0;
  pitch_ = 0;
  bufferSize_ = 0;
}

void KoboDrmDisplay::close() {
  if (fd_ >= 0 && originalCrtc_ != nullptr) {
    drmModeSetCrtc(fd_, originalCrtc_->crtc_id, originalCrtc_->buffer_id, originalCrtc_->x, originalCrtc_->y,
                   &connectorId_, 1, &originalCrtc_->mode);
  }
  if (originalCrtc_ != nullptr) drmModeFreeCrtc(originalCrtc_);
  originalCrtc_ = nullptr;
  destroyBuffer();
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
  connectorId_ = 0;
  crtcId_ = 0;
  mode_ = {};
}

}  // namespace crossink::kobo
