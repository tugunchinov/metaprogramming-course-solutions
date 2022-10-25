#pragma once

#include <optional>

template <class From, auto target>
struct Mapping {
  using Type = From;
  static constexpr auto Target = target;
};

template <class Base, class Target, class... Mappings>
class PolymorphicMapper {
  template <class C, class Map, class... Maps>
  static std::optional<Target> getTarget(const Base& object)
    requires std::derived_from<typename Map::Type, Base>
  {
    if (dynamic_cast<const typename Map::Type*>(&object) != nullptr) {
      return Map::Target;
    } else {
      return getTarget<C, Maps...>(object);
    }
  }

  template<class C>
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
