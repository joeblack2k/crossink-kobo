#pragma once

#include <cstddef>
#include <cstdint>

class HalClock {
 public:
  void begin();
  [[nodiscard]] bool isAvailable() const { return available_; }
  [[nodiscard]] bool getTime(std::uint8_t& hour, std::uint8_t& minute) const;
  [[nodiscard]] bool getDateTime(std::uint16_t& year, std::uint8_t& month, std::uint8_t& day, std::uint8_t& hour,
                                 std::uint8_t& minute) const;
  [[nodiscard]] bool formatTime(char* buffer, std::size_t bufferSize, std::uint8_t utcOffsetQuarterHoursBiased = 48,
                                bool use12Hour = false) const;
  [[nodiscard]] bool formatDate(char* buffer, std::size_t bufferSize,
                                std::uint8_t utcOffsetQuarterHoursBiased = 48) const;
  [[nodiscard]] bool syncFromNTP();

 private:
  bool available_ = false;
};

extern HalClock halClock;
