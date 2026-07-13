// SPDX-License-Identifier: GPL-3.0-or-later
#include "KoboRefreshQualification.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace crossink::kobo {
namespace {

constexpr char kQualificationPath[] = "/data/.crossink/display-fast-qualification-v1";
constexpr char kTemporaryPath[] = "/data/.crossink/display-fast-qualification-v1.new";
constexpr char kModelPath[] = "/proc/device-tree/model";
constexpr char kActiveManifestPath[] = "/opt/crossink/current/build-manifest.txt";
constexpr char kRequiredModelPrefix[] = "Kobo Glo HD";
constexpr unsigned kPolicyAbi = 1;

bool writeAll(const int descriptor, const char* data, std::size_t size) {
  while (size != 0) {
    const ssize_t written = ::write(descriptor, data, size);
    if (written <= 0) return false;
    data += written;
    size -= static_cast<std::size_t>(written);
  }
  return true;
}

bool currentKernel(char* const destination, const std::size_t size) {
  utsname unameData{};
  if (::uname(&unameData) != 0 || std::snprintf(destination, size, "%s", unameData.release) >= static_cast<int>(size)) {
    return false;
  }
  return true;
}

bool n437Model() {
  char model[80]{};
  const int descriptor = ::open(kModelPath, O_RDONLY | O_CLOEXEC);
  if (descriptor < 0) return false;
  const ssize_t bytes = ::read(descriptor, model, sizeof(model) - 1U);
  (void)::close(descriptor);
  if (bytes <= 0) return false;
  model[bytes] = '\0';
  return std::strncmp(model, kRequiredModelPrefix, sizeof(kRequiredModelPrefix) - 1U) == 0;
}

bool readTextFile(const char* const path, char* const destination, const std::size_t size) {
  if (destination == nullptr || size < 2U) return false;
  const int descriptor = ::open(path, O_RDONLY | O_CLOEXEC);
  if (descriptor < 0) return false;
  const ssize_t bytes = ::read(descriptor, destination, size - 1U);
  (void)::close(descriptor);
  if (bytes <= 0) return false;
  destination[bytes] = '\0';
  return true;
}

bool containsLine(const char* const content, const char* const line) {
  const std::size_t lineLength = std::strlen(line);
  for (const char* cursor = content; *cursor != '\0';) {
    const char* next = std::strchr(cursor, '\n');
    const std::size_t length = next == nullptr ? std::strlen(cursor) : static_cast<std::size_t>(next - cursor);
    if (length == lineLength && std::strncmp(cursor, line, lineLength) == 0) return true;
    cursor = next == nullptr ? cursor + length : next + 1;
  }
  return false;
}

bool activeBinarySha(char* const destination, const std::size_t size) {
  char manifest[1024]{};
  if (!readTextFile(kActiveManifestPath, manifest, sizeof(manifest))) return false;
  constexpr char kPrefix[] = "binary_sha256=";
  const char* const line = std::strstr(manifest, kPrefix);
  if (line == nullptr) return false;
  const char* const value = line + sizeof(kPrefix) - 1U;
  std::size_t length = 0;
  while (value[length] != '\0' && value[length] != '\n') {
    const char character = value[length];
    const bool hex = (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    if (!hex || ++length >= size) return false;
  }
  // SHA-256 is deliberately fixed-width.  Do not accept an abbreviated or
  // malformed manifest value as evidence that a changed binary was soaked.
  if (length != 64U) return false;
  std::memcpy(destination, value, length);
  destination[length] = '\0';
  return true;
}

}  // namespace

bool koboFastRefreshQualified() {
  if (!n437Model()) return false;
  char kernel[96]{};
  char binarySha[65]{};
  if (!currentKernel(kernel, sizeof(kernel)) || !activeBinarySha(binarySha, sizeof(binarySha))) return false;
  const int descriptor = ::open(kQualificationPath, O_RDONLY | O_CLOEXEC);
  if (descriptor < 0) return false;
  char content[256]{};
  const ssize_t bytes = ::read(descriptor, content, sizeof(content) - 1U);
  (void)::close(descriptor);
  if (bytes <= 0) return false;
  content[bytes] = '\0';
  char expectedVersion[32]{};
  char expectedKernel[128]{};
  char expectedBinary[96]{};
  const int versionLength = std::snprintf(expectedVersion, sizeof(expectedVersion), "policy_abi=%u", kPolicyAbi);
  const int kernelLength = std::snprintf(expectedKernel, sizeof(expectedKernel), "kernel=%s", kernel);
  const int binaryLength = std::snprintf(expectedBinary, sizeof(expectedBinary), "binary_sha256=%s", binarySha);
  return versionLength > 0 && versionLength < static_cast<int>(sizeof(expectedVersion)) && kernelLength > 0 &&
         kernelLength < static_cast<int>(sizeof(expectedKernel)) && binaryLength > 0 &&
         binaryLength < static_cast<int>(sizeof(expectedBinary)) && containsLine(content, expectedVersion) &&
         containsLine(content, "profile=fast") && containsLine(content, "model=Kobo Glo HD") &&
         containsLine(content, expectedKernel) && containsLine(content, expectedBinary);
}

bool recordKoboFastRefreshQualification() {
  if (!n437Model()) return false;
  char kernel[96]{};
  char binarySha[65]{};
  if (!currentKernel(kernel, sizeof(kernel)) || !activeBinarySha(binarySha, sizeof(binarySha))) return false;
  if (::mkdir("/data/.crossink", S_IRWXU) != 0 && errno != EEXIST) return false;
  char content[256]{};
  const int length = std::snprintf(content, sizeof(content),
                                   "policy_abi=%u\nprofile=fast\nmodel=Kobo Glo HD\nkernel=%s\nbinary_sha256=%s\n",
                                   kPolicyAbi, kernel, binarySha);
  if (length <= 0 || length >= static_cast<int>(sizeof(content))) return false;
  const int descriptor = ::open(kTemporaryPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (descriptor < 0) return false;
  const bool complete = writeAll(descriptor, content, static_cast<std::size_t>(length)) && ::fsync(descriptor) == 0;
  (void)::close(descriptor);
  if (!complete) {
    (void)::unlink(kTemporaryPath);
    return false;
  }
  if (::rename(kTemporaryPath, kQualificationPath) != 0) {
    (void)::unlink(kTemporaryPath);
    return false;
  }
  return true;
}

}  // namespace crossink::kobo
