#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Coordinates background thumbnail generation for manually copied books.
// It shares the single low-priority OPDS worker with remote cover jobs, so a
// large local EPUB can never run concurrently with a download or UI render.
class LocalCoverCache {
 public:
  enum class State : uint8_t { Missing, Queued, Generating, Ready, Failed };
  struct Change {
    std::string bookPath;
    State state = State::Missing;
    std::string bmpPath;
    std::string detail;
  };

  static LocalCoverCache& getInstance();
  void request(const std::string& bookPath);
  void tick();
  bool takeChange(Change& change);

 private:
  LocalCoverCache() = default;
  struct Pending;
  std::vector<Pending> pending;
  std::vector<Change> changes;
};

#define LOCAL_COVER_CACHE LocalCoverCache::getInstance()
