#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <cstdint>
#include <string>
#include <vector>

enum class OpdsFilenameFormat : uint8_t {
  AUTHOR_TITLE = 0,
  TITLE_AUTHOR = 1,
};

const char* opdsFilenameFormatToJson(OpdsFilenameFormat format);
OpdsFilenameFormat opdsFilenameFormatFromJson(const char* value);

struct OpdsServer {
  // Stable catalog identity.  Unlike `url`, this survives a host, port, or
  // path change made while editing a server and therefore keeps downloads and
  // reading state attached to the same library.
  std::string id;
  std::string name;
  std::string url;
  std::string username;
  std::string password;  // Plaintext in memory; obfuscated with hardware key on disk
  OpdsFilenameFormat filenameFormat = OpdsFilenameFormat::AUTHOR_TITLE;
  bool syncAllBooks = false;
};

// Legacy catalog files key servers by a FNV-1a hash of their configured URL.
// Seed newly migrated IDs with that exact value so existing catalog entries
// and /Books/OPDS/<key>/ downloads remain addressable without a file move.
std::string opdsServerStableIdForUrl(const std::string& url);

/**
 * Singleton class for storing OPDS server configurations on the SD card.
 * Passwords are XOR-obfuscated with the device's unique hardware MAC address
 * and base64-encoded before writing to JSON.
 */
class OpdsServerStore : public PersistableStore<OpdsServerStore> {
 private:
  std::vector<OpdsServer> servers;

  static constexpr size_t MAX_SERVERS = 8;

  OpdsServerStore() = default;
  bool migrateFromSettings();

  friend class PersistableStore<OpdsServerStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/opds.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
  bool loadFromFile();

  bool addServer(const OpdsServer& server);
  bool updateServer(size_t index, const OpdsServer& server);
  bool removeServer(size_t index);
  // The primary catalog is persisted as the first entry. Existing reader code
  // already treats index zero as its default source, so this is migration-free.
  bool makePrimary(size_t index);

  const std::vector<OpdsServer>& getServers() const { return servers; }
  const OpdsServer* getServer(size_t index) const;
  size_t indexForId(const std::string& id) const;
  size_t getCount() const { return servers.size(); }
  bool hasServers() const { return !servers.empty(); }
};

#define OPDS_STORE OpdsServerStore::getInstance()
