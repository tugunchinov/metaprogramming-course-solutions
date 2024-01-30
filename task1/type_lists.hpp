#pragma once

#include <concepts>

#include <type_tuples.hpp>

namespace type_lists {

template <class TL>
concept TypeSequence = requires {
                         typename TL::Head;
                         typename TL::Tail;
                       };

struct Nil {};

template <class TL>
concept Empty = std::derived_from<TL, Nil>;

template <class TL>
concept TypeList = Empty<TL> || TypeSequence<TL>;

template <class T, TypeList TL>
struct Cons {
  using Head = T;
  using Tail = TL;
};

namespace detail {
using namespace type_tuples;

template <TypeList TL, class... Ts>
struct ToTuple {
  using Tuple =
      typename ToTuple<typename TL::Tail, Ts..., typename TL::Head>::Tuple;
};

template <Empty E, class... Ts>
struct ToTuple<E, Ts...> {
  using Tuple = TTuple<Ts...>;
};

template <TypeTuple>
struct FromTuple : public Nil {};

template <class T, class... Ts>
struct FromTuple<TTuple<T, Ts...>> {
  using Head = T;
  using Tail = FromTuple<TTuple<Ts...>>;
};

template <std::size_t N, TypeList TL>
struct Take {
  using Head = typename TL::Head;
  using Tail = Take<N - 1, typename TL::Tail>;
};

template <std::size_t N, Empty E>
struct Take<N, E> : public Nil {};

template <TypeList TL>
struct Take<0, TL> : public Nil {};

template <std::size_t N, TypeList TL>
struct Drop : public Drop<N - 1, typename TL::Tail> {};

template <TypeList TL>
struct Drop<0, TL> : public TL {};

template <std::size_t N, Empty E>
struct Drop<N, E> : public Nil {};

template <std::size_t N, class T>
struct Replicate {
  using Head = T;
  using Tail = Replicate<N - 1, T>;
};

template <class T>
struct Replicate<0, T> : public Nil {};

template <template <class> class F, class T>
struct Iterate {
  using Head = T;
  using Tail = Iterate<F, F<T>>;
};

template <TypeList CL, TypeList TL>
struct Cycle {
  using Head = typename TL::Head;
  using Tail = Cycle<CL, typename TL::Tail>;
};

template <TypeList CL, Empty E>
struct Cycle<CL, E> {
  using Head = typename CL::Head;
  using Tail = Cycle<CL, typename CL::Tail>;
};

template <template <class> class F, TypeList TL>
struct Map {
  using Head = F<typename TL::Head>;
  using Tail = Map<F, typename TL::Tail>;
};

template <template <class> class F, Empty E>
struct Map<F, E> : Nil {};

template <template <class> class P, class T, TypeList TL, bool pick>
struct Filter {
  using Head = T;
  using Tail = Filter<P, typename TL::Head, typename TL::Tail,
                      P<typename TL::Head>::Value>;
};

template <template <class> class P, class T, TypeList TL>
struct Filter<P, T, TL, false>
    : public Filter<P, typename TL::Head, typename TL::Tail,
                    P<typename TL::Head>::Value> {};

template <template <class> class P, class T, Empty E>
struct Filter<P, T, E, true> {
  using Head = T;
  using Tail = Nil;
};

template <template <class> class P, class T, Empty E>
struct Filter<P, T, E, false> : public Nil {};

template <template <class, class> class OP, class T, TypeList TL>
struct Scanl {
  using Head = OP<T, typename TL::Head>;
  using Tail = Scanl<OP, Head, typename TL::Tail>;
};

template <template <class, class> class OP, class T, Empty E>
struct Scanl<OP, T, E> : Nil {};

template <template <class, class> class OP, class T, TypeList TL>
struct Foldl {
  using Type =
      typename Foldl<OP, OP<T, typename TL::Head>, typename TL::Tail>::Type;
};

template <template <class, class> class OP, class T, Empty E>
struct Foldl<OP, T, E> {
  using Type = T;
};

template <std::size_t N, TypeList TL, TypeList LT>
struct Inits {
  using Head = Take<N, TL>;
  using Tail = Inits<N + 1, TL, typename LT::Tail>;
};

template <std::size_t N, TypeList TL, Empty E>
struct Inits<N, TL, E> {
  using Head = TL;
  using Tail = Nil;
};

template <TypeList TL>
struct Tails {
  using Head = TL;
  using Tail = Tails<typename TL::Tail>;
};

template <Empty E>
struct Tails<E> {
  using Head = Nil;
  using Tail = Nil;
};

template <TypeList L, TypeList R>
struct Zip2 {
  using Head = TTuple<typename L::Head, typename R::Head>;
  using Tail = Zip2<typename L::Tail, typename R::Tail>;
};

template <Empty E, TypeList R>
struct Zip2<E, R> : Nil {};

template <TypeList L, Empty E>
struct Zip2<L, E> : Nil {};

template <Empty EL, Empty ER>
struct Zip2<EL, ER> : Nil {};

template <class... TL>
struct Zip : public Nil {
};

template <TypeSequence... TS>
struct Zip<TS...> {
  using Head = TTuple<typename TS::Head...>;
  using Tail = Zip<typename TS::Tail...>;
};

template <template <class, class> class Eq, class C, TypeList TL, bool take>
struct MakeGroup {
  using Next = MakeGroup<Eq, typename TL::Head, typename TL::Tail,
                         Eq<C, typename TL::Head>::Value>;
  using Result = Cons<C, typename Next::Result>;
  using Head = typename Next::Head;
  using Tail = typename Next::Tail;
};

template <template <class, class> class Eq, class C, TypeList TL>
struct MakeGroup<Eq, C, TL, false> {
  using Result = Nil;
  using Head = C;
  using Tail = TL;
};

template <template <class, class> class Eq, class C, Empty E>
struct MakeGroup<Eq, C, E, false> {
  using Result = Nil;
  using Head = C;
  using Tail = Nil;
};

template <template <class, class> class Eq, class C, Empty E>
struct MakeGroup<Eq, C, E, true> {
  using Result = Cons<C, Nil>;
  using Head = Nil;
  using Tail = Nil;
};

template <template <class, class> class Eq, class C, TypeList TL>
struct GroupByInt {
  using Group = MakeGroup<Eq, C, TL, true>;
  using Head = typename Group::Result;
  using Tail = GroupByInt<Eq, typename Group::Head, typename Group::Tail>;
};

template <template <class, class> class Eq, class C, Empty E>
struct GroupByInt<Eq, C, E> {
  using Head = Cons<C, Nil>;
  using Tail = Nil;
};

template <template <class, class> class Eq, Empty E>
struct GroupByInt<Eq, Nil, E> : Nil {};

template <template <class, class> class Eq, TypeList TL>
struct GroupBy {
  using Groupped = GroupByInt<Eq, typename TL::Head, typename TL::Tail>;
  using Head = typename Groupped::Head;
  using Tail = typename Groupped::Tail;
};

template <template <class, class> class Eq, Empty E>
struct GroupBy<Eq, E> : Nil {};

}  // namespace detail

template <class T>
struct Repeat {
  using Head = T;
  using Tail = Repeat<T>;
};

template <TypeList TL>
using ToTuple = typename detail::ToTuple<TL>::Tuple;

template <type_tuples::TypeTuple TT>
using FromTuple = detail::FromTuple<TT>;

template <std::size_t N, TypeList TL>
using Take = detail::Take<N, TL>;

template <std::size_t N, TypeList TL>
using Drop = detail::Drop<N, TL>;

template <std::size_t N, class T>
using Replicate = detail::Replicate<N, T>;

template <template <class> class F, class T>
using Iterate = detail::Iterate<F, T>;

template <TypeList TL>
using Cycle = detail::Cycle<TL, TL>;

template <template <class> class F, TypeList TL>
using Map = detail::Map<F, TL>;

template <template <class> class P, TypeList TL>
struct Filter {
  using Filtered =
      typename detail::Filter<P, typename TL::Head, typename TL::Tail,
                              P<typename TL::Head>::Value>;
  using Head = typename Filtered::Head;
  using Tail = typename Filtered::Tail;
};

template <template <class> class P, Empty E>
struct Filter<P, E> : public Nil {};

template <template <class, class> class OP, class T, TypeList TL>
using Scanl = Cons<T, detail::Scanl<OP, T, TL>>;

template <template <class, class> class OP, class T, TypeList TL>
using Foldl = typename detail::Foldl<OP, T, TL>::Type;

template <TypeList TL>
using Inits = detail::Inits<0, TL, TL>;

template <TypeList TL>
using Tails = detail::Tails<TL>;

template <TypeList L, TypeList R>
using Zip2 = detail::Zip2<L, R>;

template <TypeList... TL>
using Zip = detail::Zip<TL...>;

template <template <class, class> class Eq, TypeList TL>
using GroupBy = detail::GroupBy<Eq, TL>;

}  // namespace type_lists
