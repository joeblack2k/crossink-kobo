#pragma once

#include <cstdint>
#include <string>

namespace crossink::kobo {

struct KeyDeviceInfo {
  std::string path;
  std::string name;
};

class KoboEvdevKey {
 public:
  KoboEvdevKey() = default;
  ~KoboEvdevKey();

  KoboEvdevKey(const KoboEvdevKey&) = delete;
  KoboEvdevKey& operator=(const KoboEvdevKey&) = delete;

  [[nodiscard]] static bool discoverPowerKey(KeyDeviceInfo& result, const std::string& inputDirectory = "/dev/input");
  [[nodiscard]] bool open(const KeyDeviceInfo& device);
  void close();
  void beginFrame();
  void update();

  // Public for deterministic tests and replaying recorded hardware events.
  void ingest(std::uint16_t type, std::uint16_t code, std::int32_t value, std::uint64_t timestampMicros);

  [[nodiscard]] bool isPressed() const { return pressed_; }
  [[nodiscard]] bool wasPressed() const { return pressedEdge_; }
  [[nodiscard]] bool wasReleased() const { return releasedEdge_; }
  [[nodiscard]] unsigned long heldMilliseconds() const;

 private:
  int fd_ = -1;
  bool pressed_ = false;
  bool pressedEdge_ = false;
  bool releasedEdge_ = false;
  std::uint64_t pressedAtMicros_ = 0;
  std::uint64_t latestMicros_ = 0;
};

}  // namespace crossink::kobo
