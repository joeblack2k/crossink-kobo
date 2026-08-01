#pragma once

#include <array>
#include <cstddef>

template <typename T, std::size_t Capacity>
class BoundedFifo {
 public:
  static_assert(Capacity > 0);

  bool push(const T& value) {
    if (count_ == Capacity) return false;
    values_[(head_ + count_) % Capacity] = value;
    ++count_;
    return true;
  }

  bool pop(T& value) {
    if (count_ == 0) return false;
    value = values_[head_];
    head_ = (head_ + 1) % Capacity;
    --count_;
    return true;
  }

  [[nodiscard]] const T* front() const { return count_ == 0 ? nullptr : &values_[head_]; }
  void clear() {
    head_ = 0;
    count_ = 0;
  }
  [[nodiscard]] std::size_t size() const { return count_; }

 private:
  std::array<T, Capacity> values_{};
  std::size_t head_ = 0;
  std::size_t count_ = 0;
};
