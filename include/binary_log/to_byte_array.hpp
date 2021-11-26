#pragma once
#include <array>

template<typename T>
constexpr static inline std::array<uint8_t, sizeof(T)> to_byte_array(
    const T& input)
{
  std::array<uint8_t, sizeof(T)> ret;
  std::memcpy(ret.data(), &input, sizeof(T));
  return ret;
}