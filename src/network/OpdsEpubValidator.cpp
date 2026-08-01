#include "network/OpdsEpubValidator.h"

#include <ZipFile.h>

#include <cstdlib>
#include <cstring>

bool validateOpdsEpubArchive(const std::string& path, std::string& detail) {
  detail.clear();
  ZipFile archive(path);
  if (!archive.open()) {
    detail = "download is not a readable ZIP archive";
    return false;
  }

  size_t mimeSize = 0;
  size_t containerSize = 0;
  const bool hasMime = archive.getInflatedFileSize("mimetype", &mimeSize);
  const bool hasContainer = archive.getInflatedFileSize("META-INF/container.xml", &containerSize);
  archive.close();
  if (!hasMime || !hasContainer || mimeSize == 0 || mimeSize > 64 || containerSize == 0) {
    detail = "download is missing required EPUB files";
    return false;
  }

  size_t readSize = 0;
  uint8_t* mime = archive.readFileToMemory("mimetype", &readSize, true);
  if (!mime) {
    detail = "could not read EPUB mimetype";
    return false;
  }
  const bool valid =
      readSize == std::strlen("application/epub+zip") && std::memcmp(mime, "application/epub+zip", readSize) == 0;
  std::free(mime);
  if (!valid) {
    detail = "download is not an EPUB container";
    return false;
  }
  return true;
}
