#include "HalClock.h"

#include <cstdio>
#include <ctime>

HalClock halClock;

namespace {
constexpr std::time_t kMinimumValidEpoch = 1'577'836'800;  // 2020-01-01 UTC

std::time_t adjustedNow(const std::uint8_t biasedQuarterHours) {
  const int bounded = biasedQuarterHours > 104 ? 104 : biasedQuarterHours;
  return std::time(nullptr) + static_cast<std::time_t>(bounded - 48) * 15 * 60;
}

}  // namespace

void HalClock::begin() { available_ = std::time(nullptr) >= kMinimumValidEpoch; }

bool HalClock::getTime(std::uint8_t& hour, std::uint8_t& minute) const {
  if (!available_) return false;
  const std::time_t now = std::time(nullptr);
  std::tm value{};
  if (gmtime_r(&now, &value) == nullptr) return false;
  hour = static_cast<std::uint8_t>(value.tm_hour);
  minute = static_cast<std::uint8_t>(value.tm_min);
  return true;
}

bool HalClock::getDateTime(std::uint16_t& year, std::uint8_t& month, std::uint8_t& day, std::uint8_t& hour,
                           std::uint8_t& minute) const {
  if (!available_) return false;
  const std::time_t now = std::time(nullptr);
  std::tm value{};
  if (gmtime_r(&now, &value) == nullptr) return false;
  year = static_cast<std::uint16_t>(value.tm_year + 1900);
  month = static_cast<std::uint8_t>(value.tm_mon + 1);
  day = static_cast<std::uint8_t>(value.tm_mday);
  hour = static_cast<std::uint8_t>(value.tm_hour);
  minute = static_cast<std::uint8_t>(value.tm_min);
  return true;
}

bool HalClock::formatTime(char* buffer, const std::size_t bufferSize, const std::uint8_t biasedQuarterHours,
                          const bool use12Hour) const {
  if (!available_ || buffer == nullptr || bufferSize < (use12Hour ? 9U : 6U)) return false;
  const std::time_t now = adjustedNow(biasedQuarterHours);
  std::tm value{};
  if (gmtime_r(&now, &value) == nullptr) return false;
  if (use12Hour) {
    const int hour = value.tm_hour % 12 == 0 ? 12 : value.tm_hour % 12;
    return std::snprintf(buffer, bufferSize, "%d:%02d %s", hour, value.tm_min, value.tm_hour >= 12 ? "PM" : "AM") > 0;
  }
  return std::snprintf(buffer, bufferSize, "%02d:%02d", value.tm_hour, value.tm_min) > 0;
}

bool HalClock::formatDate(char* buffer, const std::size_t bufferSize, const std::uint8_t biasedQuarterHours) const {
  if (!available_ || buffer == nullptr || bufferSize < 13U) return false;
  const std::time_t now = adjustedNow(biasedQuarterHours);
  std::tm value{};
  return gmtime_r(&now, &value) != nullptr && std::strftime(buffer, bufferSize, "%b %e, %Y", &value) != 0;
}

bool HalClock::syncFromNTP() {
  // Network time is managed by the Linux service layer; re-check whether it
  // has established a sane system clock without spawning a blocking command.
  available_ = std::time(nullptr) >= kMinimumValidEpoch;
  return available_;
}
