#pragma once

#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <string>
#include <vector>

#include "OpdsParser.h"

enum class OpdsCatalogAvailability : uint8_t {
  RemoteOnly = 0,
  Downloading = 1,
  AvailableOffline = 2,
  DownloadFailed = 3,
};

struct OpdsCatalogBook {
  std::string serverKey;
  std::string entryId;
  std::string title;
  std::string author;
  std::string series;
  std::string seriesIndex;
  std::string acquisitionHref;
  std::string acquisitionType;
  std::string coverHref;
  std::string coverRel;
  std::string updated;
  std::string localPath;
  std::string coverBmpPath;
  OpdsCatalogAvailability availability = OpdsCatalogAvailability::RemoteOnly;
  // False only when the last complete server snapshot no longer contained the
  // title. Keep a locally downloaded EPUB visible/offline rather than silently
  // orphaning it; remote-only entries are removed instead.
  bool remotePresent = true;
  uint32_t snapshotGeneration = 0;
};

// Persistent, offline-readable metadata cache for OPDS catalog books.  EPUB
// files remain separate: a catalog entry is never considered offline until a
// completed local EPUB path has been recorded and verified.
class OpdsCatalogStore : public PersistableStore<OpdsCatalogStore> {
 private:
  std::vector<OpdsCatalogBook> books;
  uint32_t latestSnapshotGeneration = 0;
  // Older catalog files did not contain snapshot state and still used the
  // pre-v3 schema.  Keep this transient so loadFromFile() can rewrite the
  // fully parsed state atomically once, after the base store has finished
  // reading the old file.
  bool migrationPending = false;
  static constexpr size_t MAX_BOOKS = 512;

  OpdsCatalogStore() = default;
  friend class PersistableStore<OpdsCatalogStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/opds-catalog.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
  bool loadFromFile();

  static std::string stableBookId(const OpdsEntry& entry);
  static std::string serverKeyForIdentity(const std::string& serverIdentity);
  std::vector<OpdsCatalogBook> getBooksForServer(const std::string& serverIdentity) const;
  const OpdsCatalogBook* find(const std::string& serverIdentity, const std::string& entryId) const;

  // Merge only book entries from one fetched feed. Existing local paths and
  // offline availability survive metadata refreshes; manually copied EPUBs
  // are intentionally never inserted here.
  bool mergeFeed(const std::string& serverIdentity, const OpdsEntry* entries, size_t count);
  // Replaces only one server's remote snapshot after every page has parsed.
  // Existing local EPUB paths/availability are preserved by stable book ID.
  bool replaceServerSnapshot(const std::string& serverIdentity, const std::vector<OpdsEntry>& entries);
  bool markAvailability(const std::string& serverIdentity, const OpdsEntry& entry, OpdsCatalogAvailability availability,
                        const std::string& localPath = {});
  // Cover decoding is independent from EPUB availability.  Persist only a
  // fully generated BMP path, so interrupted cover jobs never make a card
  // point at a partial artifact.
  bool updateCoverBmpPath(const std::string& serverIdentity, const std::string& entryId,
                          const std::string& coverBmpPath);
  bool reconcileLocalFiles();
};

#define OPDS_CATALOG OpdsCatalogStore::getInstance()
