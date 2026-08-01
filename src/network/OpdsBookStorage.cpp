#include "network/OpdsBookStorage.h"

#include <cstdint>

#include "util/StringUtils.h"

namespace {

std::string filenameBase(const std::string& title, const std::string& author, const bool titleAuthorFormat) {
  if (author.empty()) return title;
  if (title.empty()) return author;
  return titleAuthorFormat ? title + " - " + author : author + " - " + title;
}

std::string stableFileSuffix(const std::string& entryId) {
  uint64_t hash = 14695981039346656037ull;
  for (const unsigned char byte : entryId) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  static constexpr char hex[] = "0123456789abcdef";
  std::string result(12, '0');
  for (int index = static_cast<int>(result.size()) - 1; index >= 0; --index) {
    result[static_cast<size_t>(index)] = hex[hash & 0x0fu];
    hash >>= 4u;
  }
  return result;
}

}  // namespace

namespace OpdsBookStorage {

std::string downloadPath(const std::string& serverId, const bool titleAuthorFormat, const std::string& entryId,
                         const std::string& title, const std::string& author) {
  const std::string readableName = StringUtils::sanitizeFilename(filenameBase(title, author, titleAuthorFormat), 100);
  return "/Books/OPDS/" + serverId + "/" + readableName + "-" + stableFileSuffix(entryId) + ".epub";
}

}  // namespace OpdsBookStorage
