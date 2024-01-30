#pragma once

#include <string_view>

template <size_t max_length>
struct FixedString {
  constexpr FixedString(const char string[], const size_t length)
      : length(length) {
    std::copy(string, string + length, this->string);
  }

  constexpr operator std::string_view() const {
    return {string, length};
  }

  char string[max_length];
  size_t length;
};

constexpr FixedString<256> operator""_cstr(const char string[], size_t length) {
  return {string, length};
}
