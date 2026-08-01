#include "OpdsServerStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include "CrossPointSettings.h"

namespace {
constexpr char FILENAME_FORMAT_AUTHOR_TITLE[] = "author_title";
constexpr char FILENAME_FORMAT_TITLE_AUTHOR[] = "title_author";
constexpr char kHexDigits[] = "0123456789abcdef";
}  // namespace

std::string opdsServerStableIdForUrl(const std::string& url) {
  uint32_t hash = 2166136261u;
  for (const unsigned char byte : url) {
    hash ^= byte;
    hash *= 16777619u;
  }
  std::string result(8, '0');
  for (int index = 7; index >= 0; --index) {
    result[static_cast<size_t>(index)] = kHexDigits[hash & 0x0fu];
    hash >>= 4u;
  }
  return result;
}

const char* opdsFilenameFormatToJson(const OpdsFilenameFormat format) {
  switch (format) {
    case OpdsFilenameFormat::TITLE_AUTHOR:
      return FILENAME_FORMAT_TITLE_AUTHOR;
    case OpdsFilenameFormat::AUTHOR_TITLE:
    default:
      return FILENAME_FORMAT_AUTHOR_TITLE;
  }
}

OpdsFilenameFormat opdsFilenameFormatFromJson(const char* value) {
  if (value && strcmp(value, FILENAME_FORMAT_TITLE_AUTHOR) == 0) {
    return OpdsFilenameFormat::TITLE_AUTHOR;
  }
  return OpdsFilenameFormat::AUTHOR_TITLE;
}

void OpdsServerStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["servers"].to<JsonArray>();
  for (const auto& server : servers) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = server.id;
    obj["name"] = server.name;
    obj["url"] = server.url;
    obj["username"] = server.username;
    obj["password_obf"] = obfuscation::obfuscateToBase64(server.password);
    obj["filenameFormat"] = opdsFilenameFormatToJson(server.filenameFormat);
    obj["syncAllBooks"] = server.syncAllBooks;
  }
}

bool OpdsServerStore::fromJson(JsonVariantConst doc) {
  // Tolerate a missing/invalid 'servers' key (treat as empty list); only a
  // JSON parse error is fatal. A null JsonArray iterates zero times.
  servers.clear();
  JsonArrayConst arr = doc["servers"].as<JsonArrayConst>();
  servers.reserve(std::min(arr.size(), MAX_SERVERS));
  bool needsResave = false;

  for (JsonObjectConst obj : arr) {
    if (servers.size() >= OpdsServerStore::MAX_SERVERS) break;
    OpdsServer server;
    server.id = obj["id"] | "";
    server.name = obj["name"] | "";
    server.url = obj["url"] | "";
    server.username = obj["username"] | "";
    server.filenameFormat = opdsFilenameFormatFromJson(obj["filenameFormat"] | "");
    server.syncAllBooks = obj["syncAllBooks"] | false;
    if (server.id.empty()) {
      server.id = opdsServerStableIdForUrl(server.url);
      needsResave = true;
    }
    obfuscation::DecodeStatus status = obfuscation::DecodeStatus::INVALID;
    server.password = obfuscation::deobfuscateFromBase64(obj["password_obf"] | "", &status);
    if (status == obfuscation::DecodeStatus::LEGACY && !server.password.empty()) {
      needsResave = true;
    }
    if (status == obfuscation::DecodeStatus::INVALID || status == obfuscation::DecodeStatus::EMPTY ||
        server.password.empty()) {
      server.password = obj["password"] | "";
      if (!server.password.empty()) needsResave = true;
    }
    if (status == obfuscation::DecodeStatus::INVALID && server.password.empty()) {
      LOG_ERR("OPS", "Ignoring unreadable password for OPDS server: %s", server.name.c_str());
    }
    servers.push_back(std::move(server));
  }

  LOG_DBG("OPS", "Loaded %zu OPDS servers from file", servers.size());

  if (needsResave) {
    LOG_DBG("OPS", "Resaving JSON with obfuscated passwords");
    saveToFile();
  }

  return true;
}

bool OpdsServerStore::loadFromFile() {
  const bool hasStoreFile = Storage.exists(getFilePath());
  if (PersistableStore<OpdsServerStore>::loadFromFile()) {
    return true;
  }
  if (hasStoreFile) {
    return false;
  }

  if (migrateFromSettings()) {
    LOG_DBG("OPS", "Migrated legacy OPDS settings");
    return true;
  }

  return false;
}

bool OpdsServerStore::migrateFromSettings() {
  if (strlen(SETTINGS.opdsServerUrl) == 0) {
    return false;
  }

  OpdsServer server;
  server.name = "OPDS Server";
  server.url = SETTINGS.opdsServerUrl;
  server.id = opdsServerStableIdForUrl(server.url);
  server.username = SETTINGS.opdsUsername;
  server.password = SETTINGS.opdsPassword;
  servers.push_back(std::move(server));

  if (saveToFile()) {
    // Clear legacy fields so migration won't run again on next boot.
    SETTINGS.opdsServerUrl[0] = '\0';
    SETTINGS.opdsUsername[0] = '\0';
    SETTINGS.opdsPassword[0] = '\0';
    SETTINGS.saveToFile();
    LOG_DBG("OPS", "Migrated single-server OPDS config to opds.json");
    return true;
  }

  // Save failed; roll back in-memory state so callers don't see a partial migration.
  servers.clear();
  return false;
}

bool OpdsServerStore::addServer(const OpdsServer& server) {
  if (servers.size() >= MAX_SERVERS) {
    LOG_DBG("OPS", "Cannot add more servers, limit of %zu reached", MAX_SERVERS);
    return false;
  }

  OpdsServer persisted = server;
  if (persisted.id.empty()) persisted.id = opdsServerStableIdForUrl(persisted.url);
  const auto previous = servers;
  servers.push_back(std::move(persisted));
  LOG_DBG("OPS", "Added server: %s", server.name.c_str());
  if (saveToFile()) return true;
  servers = previous;
  return false;
}

bool OpdsServerStore::updateServer(size_t index, const OpdsServer& server) {
  if (index >= servers.size()) {
    return false;
  }

  const auto previous = servers;
  // URLs are editable transport details, never the catalog primary key.
  OpdsServer persisted = server;
  persisted.id = servers[index].id.empty() ? opdsServerStableIdForUrl(servers[index].url) : servers[index].id;
  servers[index] = std::move(persisted);
  LOG_DBG("OPS", "Updated server: %s", server.name.c_str());
  if (saveToFile()) return true;
  servers = previous;
  return false;
}

bool OpdsServerStore::removeServer(size_t index) {
  if (index >= servers.size()) {
    return false;
  }

  const auto previous = servers;
  LOG_DBG("OPS", "Removed server: %s", servers[index].name.c_str());
  servers.erase(servers.begin() + static_cast<ptrdiff_t>(index));
  if (saveToFile()) return true;
  servers = previous;
  return false;
}

bool OpdsServerStore::makePrimary(const size_t index) {
  if (index >= servers.size()) return false;
  if (index == 0) return true;
  const auto previous = servers;
  OpdsServer selected = std::move(servers[index]);
  servers.erase(servers.begin() + static_cast<ptrdiff_t>(index));
  servers.insert(servers.begin(), std::move(selected));
  LOG_DBG("OPS", "Set primary server: %s", servers.front().name.c_str());
  if (saveToFile()) return true;
  servers = previous;
  return false;
}

const OpdsServer* OpdsServerStore::getServer(size_t index) const {
  if (index >= servers.size()) {
    return nullptr;
  }
  return &servers[index];
}

size_t OpdsServerStore::indexForId(const std::string& id) const {
  const auto found =
      std::find_if(servers.begin(), servers.end(), [&](const OpdsServer& server) { return server.id == id; });
  return found == servers.end() ? MAX_SERVERS : static_cast<size_t>(std::distance(servers.begin(), found));
}
