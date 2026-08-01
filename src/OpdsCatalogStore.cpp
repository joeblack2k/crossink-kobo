#include "OpdsCatalogStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <array>
#include <utility>

#include "util/StringUtils.h"

namespace {
const char* availabilityToJson(const OpdsCatalogAvailability value) {
  switch (value) {
    case OpdsCatalogAvailability::Downloading:
      return "downloading";
    case OpdsCatalogAvailability::AvailableOffline:
      return "offline";
    case OpdsCatalogAvailability::DownloadFailed:
      return "failed";
    case OpdsCatalogAvailability::RemoteOnly:
    default:
      return "remote";
  }
}

OpdsCatalogAvailability availabilityFromJson(const char* value) {
  if (value && std::string(value) == "offline") return OpdsCatalogAvailability::AvailableOffline;
  if (value && std::string(value) == "downloading") return OpdsCatalogAvailability::Downloading;
  if (value && std::string(value) == "failed") return OpdsCatalogAvailability::DownloadFailed;
  return OpdsCatalogAvailability::RemoteOnly;
}

std::string stableHash(const std::string& value) {
  uint64_t hash = 14695981039346656037ull;
  for (const unsigned char byte : value) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  static constexpr char hex[] = "0123456789abcdef";
  std::string result(16, '0');
  for (int index = 15; index >= 0; --index) {
    result[static_cast<size_t>(index)] = hex[hash & 0x0fu];
    hash >>= 4u;
  }
  return result;
}

std::string legacyLocalPathFor(const OpdsCatalogBook& book) {
  // Pre-catalog releases wrote OPDS downloads into the storage root. Keep
  // those EPUBs where they are and attach them by their deterministic legacy
  // filename; this migration must never move or delete user files.
  const std::array<std::string, 2> candidates = {
      "/" + StringUtils::sanitizeFilename(book.author + " - " + book.title) + ".epub",
      "/" + StringUtils::sanitizeFilename(book.title + " - " + book.author) + ".epub"};
  for (const auto& path : candidates) {
    if (Storage.exists(path.c_str())) return path;
  }
  return {};
}
}  // namespace

void OpdsCatalogStore::toJson(JsonDocument& doc) const {
  doc["version"] = 3;
  doc["latestSnapshotGeneration"] = latestSnapshotGeneration;
  JsonArray arr = doc["books"].to<JsonArray>();
  for (const auto& book : books) {
    JsonObject obj = arr.add<JsonObject>();
    obj["serverKey"] = book.serverKey;
    obj["entryId"] = book.entryId;
    obj["title"] = book.title;
    obj["author"] = book.author;
    obj["series"] = book.series;
    obj["seriesIndex"] = book.seriesIndex;
    obj["acquisitionHref"] = book.acquisitionHref;
    obj["acquisitionType"] = book.acquisitionType;
    obj["coverHref"] = book.coverHref;
    obj["coverRel"] = book.coverRel;
    obj["updated"] = book.updated;
    obj["localPath"] = book.localPath;
    obj["coverBmpPath"] = book.coverBmpPath;
    obj["availability"] = availabilityToJson(book.availability);
    obj["remotePresent"] = book.remotePresent;
    obj["snapshotGeneration"] = book.snapshotGeneration;
  }
}

bool OpdsCatalogStore::fromJson(JsonVariantConst doc) {
  books.clear();
  const uint32_t storedVersion = doc["version"] | 0u;
  migrationPending = storedVersion < 3;
  latestSnapshotGeneration = doc["latestSnapshotGeneration"] | 0u;
  JsonArrayConst arr = doc["books"].as<JsonArrayConst>();
  books.reserve(std::min(arr.size(), MAX_BOOKS));
  for (JsonObjectConst obj : arr) {
    if (books.size() >= MAX_BOOKS) break;
    OpdsCatalogBook book;
    book.serverKey = obj["serverKey"] | "";
    book.entryId = obj["entryId"] | "";
    book.title = obj["title"] | "";
    book.author = obj["author"] | "";
    book.series = obj["series"] | "";
    book.seriesIndex = obj["seriesIndex"] | "";
    book.acquisitionHref = obj["acquisitionHref"] | "";
    book.acquisitionType = obj["acquisitionType"] | "";
    book.coverHref = obj["coverHref"] | "";
    book.coverRel = obj["coverRel"] | "";
    book.updated = obj["updated"] | "";
    book.localPath = obj["localPath"] | "";
    book.coverBmpPath = obj["coverBmpPath"] | "";
    book.availability = availabilityFromJson(obj["availability"] | "remote");
    book.remotePresent = obj["remotePresent"] | true;
    book.snapshotGeneration = obj["snapshotGeneration"] | 0u;
    if (book.serverKey.empty() || book.entryId.empty() || book.title.empty() || book.acquisitionHref.empty()) continue;
    books.push_back(std::move(book));
  }
  reconcileLocalFiles();
  LOG_DBG("OPDSCAT", "Loaded %zu cached OPDS books", books.size());
  return true;
}

bool OpdsCatalogStore::loadFromFile() {
  migrationPending = false;
  if (!PersistableStore<OpdsCatalogStore>::loadFromFile()) return false;
  if (migrationPending && !saveToFile()) {
    LOG_ERR("OPDSCAT", "Could not atomically migrate OPDS catalog to v3");
    return false;
  }
  return true;
}

std::string OpdsCatalogStore::stableBookId(const OpdsEntry& entry) {
  if (!entry.id.empty()) return entry.id;
  // Acquisition URLs often contain ephemeral download tokens or change after
  // an OPDS server move. Title/author is the least surprising stable fallback
  // available in an ID-less OPDS entry; namespace it to avoid colliding with
  // real server-provided IDs.
  return "fallback-" + stableHash(entry.title + "\x1f" + entry.author);
}

std::string OpdsCatalogStore::serverKeyForIdentity(const std::string& serverIdentity) {
  // Current persistent IDs intentionally equal the legacy URL-derived key.
  // Accepting the identity verbatim avoids a double hash and makes an edited
  // server URL retain all existing catalog/download associations.
  return serverIdentity;
}

std::vector<OpdsCatalogBook> OpdsCatalogStore::getBooksForServer(const std::string& serverIdentity) const {
  const std::string key = serverKeyForIdentity(serverIdentity);
  std::vector<OpdsCatalogBook> result;
  for (const auto& book : books) {
    if (book.serverKey == key) result.push_back(book);
  }
  return result;
}

const OpdsCatalogBook* OpdsCatalogStore::find(const std::string& serverIdentity, const std::string& entryId) const {
  const std::string key = serverKeyForIdentity(serverIdentity);
  const auto it = std::find_if(books.begin(), books.end(), [&](const OpdsCatalogBook& book) {
    return book.serverKey == key && book.entryId == entryId;
  });
  return it == books.end() ? nullptr : &*it;
}

bool OpdsCatalogStore::mergeFeed(const std::string& serverIdentity, const OpdsEntry* entries, const size_t count) {
  if (!entries) return false;
  const std::string key = serverKeyForIdentity(serverIdentity);
  bool changed = false;
  for (size_t index = 0; index < count; ++index) {
    const OpdsEntry& entry = entries[index];
    if (entry.type != OpdsEntryType::BOOK || entry.title.empty() || entry.href.empty()) continue;
    const std::string entryId = stableBookId(entry);
    auto existing = std::find_if(books.begin(), books.end(), [&](const OpdsCatalogBook& book) {
      return book.serverKey == key && book.entryId == entryId;
    });
    if (existing == books.end()) {
      if (books.size() >= MAX_BOOKS) {
        LOG_ERR("OPDSCAT", "Catalog capacity %zu reached", MAX_BOOKS);
        break;
      }
      OpdsCatalogBook book;
      book.serverKey = key;
      book.entryId = entryId;
      book.title = entry.title;
      book.author = entry.author;
      book.series = entry.series;
      book.seriesIndex = entry.seriesIndex;
      book.acquisitionHref = entry.href;
      book.acquisitionType = entry.acquisitionType;
      book.coverHref = entry.coverHref;
      book.coverRel = entry.coverRel;
      book.updated = entry.updated;
      book.remotePresent = true;
      book.snapshotGeneration = latestSnapshotGeneration;
      book.localPath = legacyLocalPathFor(book);
      if (!book.localPath.empty()) book.availability = OpdsCatalogAvailability::AvailableOffline;
      books.push_back(std::move(book));
      changed = true;
      continue;
    }
    if (!existing->remotePresent || existing->snapshotGeneration != latestSnapshotGeneration ||
        existing->title != entry.title || existing->author != entry.author || existing->series != entry.series ||
        existing->seriesIndex != entry.seriesIndex || existing->acquisitionHref != entry.href ||
        existing->acquisitionType != entry.acquisitionType || existing->coverHref != entry.coverHref ||
        existing->coverRel != entry.coverRel || existing->updated != entry.updated) {
      existing->title = entry.title;
      existing->author = entry.author;
      existing->series = entry.series;
      existing->seriesIndex = entry.seriesIndex;
      existing->acquisitionHref = entry.href;
      existing->acquisitionType = entry.acquisitionType;
      existing->coverHref = entry.coverHref;
      existing->coverRel = entry.coverRel;
      existing->updated = entry.updated;
      existing->remotePresent = true;
      existing->snapshotGeneration = latestSnapshotGeneration;
      changed = true;
    }
  }
  if (changed && !saveToFile()) {
    LOG_ERR("OPDSCAT", "Could not persist merged OPDS feed");
    return false;
  }
  return true;
}

bool OpdsCatalogStore::replaceServerSnapshot(const std::string& serverIdentity, const std::vector<OpdsEntry>& entries) {
  const std::string key = serverKeyForIdentity(serverIdentity);
  const auto previous = books;
  const uint32_t previousGeneration = latestSnapshotGeneration;
  ++latestSnapshotGeneration;
  if (latestSnapshotGeneration == 0) latestSnapshotGeneration = 1;
  std::vector<OpdsCatalogBook> nextBooks;
  nextBooks.reserve(previous.size());
  for (const auto& book : previous) {
    if (book.serverKey != key) nextBooks.push_back(book);
  }
  for (const auto& entry : entries) {
    if (entry.type != OpdsEntryType::BOOK || entry.title.empty() || entry.href.empty()) continue;
    const std::string id = stableBookId(entry);
    const auto old = std::find_if(previous.begin(), previous.end(), [&](const OpdsCatalogBook& book) {
      return book.serverKey == key && book.entryId == id;
    });
    const auto duplicate = std::find_if(nextBooks.begin(), nextBooks.end(), [&](const OpdsCatalogBook& book) {
      return book.serverKey == key && book.entryId == id;
    });
    if (duplicate != nextBooks.end()) continue;
    OpdsCatalogBook next;
    next.serverKey = key;
    next.entryId = id;
    next.title = entry.title;
    next.author = entry.author;
    next.series = entry.series;
    next.seriesIndex = entry.seriesIndex;
    next.acquisitionHref = entry.href;
    next.acquisitionType = entry.acquisitionType;
    next.coverHref = entry.coverHref;
    next.coverRel = entry.coverRel;
    next.updated = entry.updated;
    next.remotePresent = true;
    next.snapshotGeneration = latestSnapshotGeneration;
    if (old != previous.end()) {
      next.localPath = old->localPath;
      next.coverBmpPath = old->coverBmpPath;
      next.availability = old->availability;
    } else {
      next.localPath = legacyLocalPathFor(next);
      if (!next.localPath.empty()) next.availability = OpdsCatalogAvailability::AvailableOffline;
    }
    if (nextBooks.size() >= MAX_BOOKS) return false;
    nextBooks.push_back(std::move(next));
  }
  // A completed snapshot is authoritative for remote availability. Retain an
  // already-downloaded book that disappeared from the server, but remove
  // stale remote-only cards. This never touches manual EPUBs because they are
  // never stored in this catalog.
  for (const auto& old : previous) {
    if (old.serverKey != key) continue;
    const auto stillPresent = std::find_if(nextBooks.begin(), nextBooks.end(), [&](const OpdsCatalogBook& book) {
      return book.serverKey == key && book.entryId == old.entryId;
    });
    if (stillPresent != nextBooks.end()) continue;
    if (old.availability == OpdsCatalogAvailability::AvailableOffline && !old.localPath.empty() &&
        Storage.exists(old.localPath.c_str())) {
      OpdsCatalogBook retained = old;
      retained.remotePresent = false;
      retained.snapshotGeneration = latestSnapshotGeneration;
      if (nextBooks.size() >= MAX_BOOKS) return false;
      nextBooks.push_back(std::move(retained));
    }
  }
  books = std::move(nextBooks);
  if (!saveToFile()) {
    books = previous;
    latestSnapshotGeneration = previousGeneration;
    return false;
  }
  return true;
}

bool OpdsCatalogStore::markAvailability(const std::string& serverIdentity, const OpdsEntry& entry,
                                        const OpdsCatalogAvailability availability, const std::string& localPath) {
  const std::string id = stableBookId(entry);
  const std::string key = serverKeyForIdentity(serverIdentity);
  auto it = std::find_if(books.begin(), books.end(),
                         [&](const OpdsCatalogBook& book) { return book.serverKey == key && book.entryId == id; });
  if (it == books.end()) {
    if (!mergeFeed(serverIdentity, &entry, 1)) return false;
    it = std::find_if(books.begin(), books.end(),
                      [&](const OpdsCatalogBook& book) { return book.serverKey == key && book.entryId == id; });
    if (it == books.end()) return false;
  }
  it->availability = availability;
  if (!localPath.empty()) it->localPath = localPath;
  if (availability != OpdsCatalogAvailability::AvailableOffline &&
      availability != OpdsCatalogAvailability::Downloading) {
    it->localPath.clear();
  }
  return saveToFile();
}

bool OpdsCatalogStore::updateCoverBmpPath(const std::string& serverIdentity, const std::string& entryId,
                                          const std::string& coverBmpPath) {
  const std::string key = serverKeyForIdentity(serverIdentity);
  const auto it = std::find_if(books.begin(), books.end(), [&](const OpdsCatalogBook& book) {
    return book.serverKey == key && book.entryId == entryId;
  });
  if (it == books.end()) return false;
  if (it->coverBmpPath == coverBmpPath) return true;
  const std::string previous = it->coverBmpPath;
  it->coverBmpPath = coverBmpPath;
  if (saveToFile()) return true;
  it->coverBmpPath = previous;
  return false;
}

bool OpdsCatalogStore::reconcileLocalFiles() {
  bool changed = false;
  for (auto& book : books) {
    if (book.availability == OpdsCatalogAvailability::RemoteOnly && book.localPath.empty()) {
      const std::string legacyPath = legacyLocalPathFor(book);
      if (!legacyPath.empty()) {
        book.localPath = legacyPath;
        book.availability = OpdsCatalogAvailability::AvailableOffline;
        changed = true;
      }
    }
    if (book.availability == OpdsCatalogAvailability::AvailableOffline &&
        (book.localPath.empty() || !Storage.exists(book.localPath.c_str()))) {
      book.availability = OpdsCatalogAvailability::RemoteOnly;
      book.localPath.clear();
      changed = true;
    }
    if (book.availability == OpdsCatalogAvailability::Downloading) {
      book.availability = OpdsCatalogAvailability::RemoteOnly;
      changed = true;
    }
  }
  if (changed && !saveToFile()) {
    LOG_ERR("OPDSCAT", "Could not persist reconciled OPDS catalog");
  }
  return changed;
}
