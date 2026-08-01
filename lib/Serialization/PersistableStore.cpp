#include "PersistableStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#if defined(KOBO_LINUX) || defined(SIMULATOR)
#include <mutex>
namespace {
std::mutex persistenceMutex;
}
#endif

bool PersistableStoreBase::writeDocToFile(const char* path, const JsonDocument& doc) {
#if defined(KOBO_LINUX) || defined(SIMULATOR)
  const std::lock_guard<std::mutex> lock(persistenceMutex);
#endif
  Storage.mkdir("/.crosspoint");
  // Never truncate the live JSON file in place.  An app restart, power loss,
  // or suspend can otherwise leave e.g. recent.json as a valid prefix only.
  // The target filesystem is POSIX/ext4, so a same-directory rename replaces
  // the old file atomically after the completed temporary file is synced.
  const std::string temporaryPath = std::string(path) + ".tmp";
  if (Storage.exists(temporaryPath.c_str()) && !Storage.remove(temporaryPath.c_str())) {
    LOG_ERR("PERSIST", "Failed to remove stale temporary file for %s", path);
    return false;
  }
  HalFile output;
  if (!Storage.openFileForWrite("PERSIST", temporaryPath, output)) {
    LOG_ERR("PERSIST", "Failed to open temporary file for %s", path);
    return false;
  }
  const size_t expected = measureJson(doc);
  const size_t written = serializeJson(doc, output);
  const bool completed = written == expected && output.sync() && output.close();
  if (!completed) {
    output.close();
    Storage.remove(temporaryPath.c_str());
    LOG_ERR("PERSIST", "Failed to sync temporary file for %s (%u/%u bytes)", path,
            static_cast<unsigned>(written), static_cast<unsigned>(expected));
    return false;
  }
  if (!Storage.rename(temporaryPath.c_str(), path)) {
    Storage.remove(temporaryPath.c_str());
    LOG_ERR("PERSIST", "Failed to atomically replace %s", path);
    return false;
  }
#ifdef KOBO_LINUX
  // The Kobo POSIX storage adapter fsyncs the containing directory after the
  // rename. The upstream desktop simulator storage interface has no matching
  // primitive; its regular close/rename semantics remain sufficient for its
  // regression role.
  if (!Storage.syncParentDirectory(path)) {
    LOG_ERR("PERSIST", "Failed to sync parent directory for %s", path);
    return false;
  }
#endif
  return true;
}

bool PersistableStoreBase::readDocFromFile(const char* path, JsonDocument& doc) {
#if defined(KOBO_LINUX) || defined(SIMULATOR)
  const std::lock_guard<std::mutex> lock(persistenceMutex);
#endif
  if (!Storage.exists(path)) {
    return false;  // Expected on first boot — not an error.
  }
  String json = Storage.readFile(path);
  if (json.isEmpty()) {
    LOG_ERR("PERSIST", "Failed to read %s (empty)", path);
    return false;
  }
  // Pass an explicit immutable byte range.  On the Linux simulator/Kobo
  // target the compatibility String also exposes stream-like methods; letting
  // ArduinoJson choose that adapter made complete on-disk JSON intermittently
  // look like an EOF-truncated stream after a process re-exec.  c_str()+length
  // is portable to Arduino String as well and has unambiguous ownership.
  auto error = deserializeJson(doc, json.c_str(), json.length());
  if (error) {
    LOG_ERR("PERSIST", "JSON parse error in %s: %s", path, error.c_str());
    return false;
  }
  return true;
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave) {
  obfuscation::DecodeStatus status = obfuscation::DecodeStatus::INVALID;
  std::string pass = obfuscation::deobfuscateFromBase64(doc["password_obf"] | "", &status);
  if (status == obfuscation::DecodeStatus::LEGACY && !pass.empty()) {
    needsResave = true;
  }
  if (status == obfuscation::DecodeStatus::INVALID || status == obfuscation::DecodeStatus::EMPTY || pass.empty()) {
    // Deobfuscation failed or no obfuscated password was stored; fall back to legacy plaintext.
    pass = doc["password"] | "";
    if (!pass.empty()) needsResave = true;
  }
  return pass;
}
