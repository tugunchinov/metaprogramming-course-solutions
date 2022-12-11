#pragma once

#include <array>
#include <type_traits>
#include <cstdint>
#include <string_view>
#include <limits>

namespace detail {
template <auto T>
constexpr auto helper() {
  return __PRETTY_FUNCTION__;
}

constexpr std::string_view extractT(const std::string_view& s) {
  auto result = s.substr(s.find("[T = ") + sizeof("[T = ") - 1);
  result.remove_suffix(1);

  if (auto pos = result.find("::"); pos != std::string_view::npos) {
    return result.substr(pos + sizeof("::") - 1);
  } else if (result.find("(") == std::string_view::npos) {
    return result;
  } else {
    return {};
  }
}

template <class Enum, int e, int MAXN>
  requires std::is_enum_v<Enum>
struct countEnumerators {
  static constexpr std::string_view name =
      extractT(helper<static_cast<Enum>(e)>());
  static constexpr auto next = countEnumerators<Enum, e + 1, MAXN>::name.empty()
                                   ? countEnumerators<Enum, e + 1, MAXN>::next
                                   : e + 1;
  static constexpr auto T = name.empty() ? 0 : 1;
  static constexpr auto value = T + countEnumerators<Enum, e + 1, MAXN>::value;

  static constexpr std::pair<Enum, std::string_view> getAt(
      std::size_t& i) noexcept {
    using Next = countEnumerators<Enum, next, MAXN>;

    if (name.empty()) {
      return Next::getAt(i);
    } else {
      if (i == 0) {
        return {static_cast<Enum>(e), name};
      } else if (i == 1) {
        --i;
        return {static_cast<Enum>(next), Next::name};
      } else {
        constexpr auto next_next = Next::next;

        if (next == MAXN || next_next == MAXN) {
          return Next::getAt(--i);
        } else {
          i -= 2;
          return countEnumerators<Enum, next_next, MAXN>::getAt(i);
        }
      }
    }
  }
};

template <class Enum, int MAXN>
  requires std::is_enum_v<Enum>
struct countEnumerators<Enum, MAXN, MAXN> {
  static constexpr std::string_view name =
      extractT(helper<static_cast<Enum>(MAXN)>());
  static constexpr auto next = MAXN;
  static constexpr auto value = name.empty() ? 0 : 1;

  static constexpr std::pair<Enum, std::string_view> getAt(
      std::size_t& i) noexcept {
    if (i == 0) {
      return {static_cast<Enum>(MAXN), name};
    } else {
      if (!name.empty()) {
        --i;
      }

      if (i == 0) {
        return {static_cast<Enum>(MAXN), ""};
      }

      return {static_cast<Enum>(MAXN), name};
    }
  }
};

}  // namespace detail

template <class Enum, std::size_t MAXN = 512>
  requires std::is_enum_v<Enum>
struct EnumeratorTraits {
  using T = std::underlying_type_t<Enum>;
  static constexpr auto UND_MAX = std::numeric_limits<T>::max();
  static constexpr auto UND_MIN = std::numeric_limits<T>::min();

  // TODO: better
  static constexpr std::size_t size() noexcept {
    if constexpr (UND_MIN == 0) {
      if constexpr (MAXN <= UND_MAX) {
        return detail::countEnumerators<Enum, 0, MAXN>::value;
      } else {
        return detail::countEnumerators<Enum, 0, UND_MAX>::value;
      }
    } else {
      if constexpr (MAXN <= UND_MAX) {
        constexpr auto pos_cnt = detail::countEnumerators<Enum, 0, MAXN>::value;

        if constexpr (UND_MIN >= -static_cast<int>(MAXN)) {
          return pos_cnt + detail::countEnumerators<Enum, UND_MIN, -1>::value;
        } else {
          return pos_cnt +
                 detail::countEnumerators<Enum, -static_cast<int>(MAXN),
                                          -1>::value;
        }
      } else {
        constexpr auto pos_cnt =
            detail::countEnumerators<Enum, 0, UND_MAX>::value;

        if constexpr (UND_MIN >= -static_cast<int>(MAXN)) {
          return pos_cnt + detail::countEnumerators<Enum, UND_MIN, -1>::value;
        } else {
          return pos_cnt +
                 detail::countEnumerators<Enum, -static_cast<int>(MAXN),
                                          -1>::value;
        }
      }
    }
  }

  static constexpr std::pair<Enum, std::string_view> getAt(
      std::size_t i) noexcept {
    if constexpr (UND_MIN == 0) {
      if constexpr (MAXN <= UND_MAX) {
        return detail::countEnumerators<Enum, 0, MAXN>::getAt(i);
      } else {
        return detail::countEnumerators<Enum, 0, UND_MAX>::getAt(i);
      }
    } else {
      if constexpr (MAXN <= UND_MAX) {
        if constexpr (UND_MIN >= -static_cast<int>(MAXN)) {
          auto [e, name] =
              detail::countEnumerators<Enum, UND_MIN, -1>::getAt(i);

          if ((name.empty() && i == 0) || i > 0) {
            return detail::countEnumerators<Enum, 0, MAXN>::getAt(i);
          } else {
            return {e, name};
          }
        } else {
          auto [e, name] =
              detail::countEnumerators<Enum, -static_cast<int>(MAXN),
                                       -1>::getAt(i);

          if ((name.empty() && i == 0) || i > 0) {
            return detail::countEnumerators<Enum, 0, MAXN>::getAt(i);
          } else {
            return {e, name};
          }
        }
      } else {
        if constexpr (UND_MIN >= -static_cast<int>(MAXN)) {
          auto [e, name] =
              detail::countEnumerators<Enum, UND_MIN, -1>::getAt(i);

          if ((name.empty() && i == 0) || i > 0) {
            return detail::countEnumerators<Enum, 0, UND_MAX>::getAt(i);
          } else {
            return {e, name};
          }
        } else {
          auto [e, name] =
              detail::countEnumerators<Enum, -static_cast<int>(MAXN),
                                       -1>::getAt(i);

          if ((name.empty() && i == 0) || i > 0) {
            return detail::countEnumerators<Enum, 0, UND_MAX>::getAt(i);
          } else {
            return {e, name};
          }
        }
      }
    }
  }

  static constexpr Enum at(std::size_t i) noexcept {
    return getAt(i).first;
  }

  static constexpr std::string_view nameAt(std::size_t i) noexcept {
    return getAt(i).second;
  }
};
