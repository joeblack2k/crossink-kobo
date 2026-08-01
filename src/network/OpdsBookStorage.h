#pragma once

#include <string>

// Shared OPDS EPUB-path contract. Catalog migration and the background
// downloader must derive exactly the same stable, human-readable pathname.
namespace OpdsBookStorage {

std::string downloadPath(const std::string& serverId, bool titleAuthorFormat, const std::string& entryId,
                         const std::string& title, const std::string& author);

}  // namespace OpdsBookStorage
