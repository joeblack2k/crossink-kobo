#pragma once

#include <string>

// Validates the minimum EPUB container contract before a downloaded `.part`
// file becomes visible as an offline book. This is deliberately independent
// of the reader cache: a broken download must never create a cache entry or
// be published by an atomic rename.
bool validateOpdsEpubArchive(const std::string& path, std::string& detail);
