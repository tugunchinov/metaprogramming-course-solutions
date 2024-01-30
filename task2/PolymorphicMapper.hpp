#pragma once

#include <iostream>
#include <optional>

template <class From, auto target>
struct Mapping {
  using Type = From;
  static constexpr auto Target = target;
};

template <class Base, class Target, class... Mappings>
  requires((std::derived_from<typename Mappings::Type, Base> &&
            std::same_as<decltype(Mappings::Target), const Target>) &&
           ...)
class PolymorphicMapper {
  template <class C, class Map, class... Maps>
  static std::optional<Target> getTarget(const Base& object) {
    if (dynamic_cast<const typename Map::Type*>(&object) != nullptr) {
      if constexpr (std::derived_from<typename Map::Type, typename C::Type>) {
        auto target = getTarget<Map, Maps...>(object);
        if (target) {
          return target;
        }
      } else {
        auto target = getTarget<C, Maps...>(object);
        if (target) {
          return target;
        }
      }
      
      return C::Target;
    } else {
      return getTarget<C, Maps...>(object);
    }
  }

  template <class C>
  static std::optional<Target> getTarget(const Base& object) {
    if (dynamic_cast<const typename C::Type*>(&object) != nullptr) {
      return C::Target;
    } else {
      return std::nullopt;
    }
  }

 public:
  static std::optional<Target> map(const Base& object) {
    return getTarget<Mapping<Base, std::nullopt>, Mappings...>(object);
  }
};
