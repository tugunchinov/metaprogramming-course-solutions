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
  using Tail = Cons<typename TL::Head, typename TL::Tail>;
};

template <class T, Empty E>
struct Cons<T, E> {
  using Head = T;
  using Tail = E;
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
struct FromTuple {
  using List = Nil;
};

template <class T, class... Ts>
struct FromTuple<TTuple<T, Ts...>> {
  using List = Cons<T, typename FromTuple<TTuple<Ts...>>::List>;
};

template <std::size_t N, TypeList TL>
struct Take {
  using List =
      Cons<typename TL::Head, typename Take<N - 1, typename TL::Tail>::List>;
};

template <std::size_t N, Empty E>
struct Take<N, E> {
  using List = Nil;
};

template <TypeList TL>
struct Take<0, TL> {
  using List = Nil;
};

template <std::size_t N, TypeList TL>
struct Drop {
  using List = typename Drop<N - 1, typename TL::Tail>::List;
};

template <TypeList TL>
struct Drop<0, TL> {
  using List = TL;
};

template <std::size_t N, Empty E>
struct Drop<N, E> {
  using List = Nil;
};

template <std::size_t N, class T>
struct Replicate {
  using List = Cons<T, typename Replicate<N - 1, T>::List>;
};

template <class T>
struct Replicate<0, T> {
  using List = Nil;
};

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
struct Pick {
  using Head = T;
  using Tail = Pick<P, typename TL::Head, typename TL::Tail,
                    P<typename TL::Head>::Value>;
};

template <template <class> class P, class T, TypeList TL>
struct Pick<P, T, TL, false> {
  using Next = Pick<P, typename TL::Head, typename TL::Tail,
                    P<typename TL::Head>::Value>;
  using Head = typename Next::Head;
  using Tail = typename Next::Tail;
};

template <template <class> class P, class T, Empty E>
struct Pick<P, T, E, true> {
  using Head = T;
  using Tail = Nil;
};

template <template <class> class P, class T, Empty E>
struct Pick<P, T, E, false> {
  using Head = Nil;
  using Tail = Nil;
};

template <template <class> class P, TypeList TL>
struct Filter {
  using Next = Pick<P, typename TL::Head, typename TL::Tail,
                    P<typename TL::Head>::Value>;
  using Head = typename Next::Head;
  using Tail = typename Next::Tail;
};

template <template <class> class P, Empty E>
struct Filter<P, E> {
  using Head = Nil;
  using Tail = Nil;
};

template <class T, TypeList TL>
struct MapNil {
  using Head = T;
  using Tail = TL;
};

template <TypeList TL>
struct MapNil<Nil, TL> : Nil {};

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
  using Head = typename Take<N, TL>::List;
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

template <TypeList... TL>
struct Zip {
  using Head = TTuple<typename TL::Head...>;
  using Tail = Zip<typename TL::Tail...>;
};

template <template <class, class> class Eq, class C, TypeList TL,
          bool take>
struct MakeGroup {
  using Next = MakeGroup<Eq, typename TL::Head, typename TL::Tail,
                         Eq<C, typename TL::Head>::Value>;
  using Result = Cons<C, typename Next::Result>;
  using Head = typename Next::Head;
  using Tail = typename Next::Tail;
};

template <template <class, class> class Eq, class C,
          TypeList TL>
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
using FromTuple = typename detail::FromTuple<TT>::List;

template <std::size_t N, TypeList TL>
using Take = typename detail::Take<N, TL>::List;

template <std::size_t N, TypeList TL>
using Drop = typename detail::Drop<N, TL>::List;

template <std::size_t N, class T>
using Replicate = typename detail::Replicate<N, T>::List;

template <template <class> class F, class T>
using Iterate = detail::Iterate<F, T>;

template <TypeList TL>
using Cycle = detail::Cycle<TL, TL>;

template <template <class> class F, TypeList TL>
using Map = detail::Map<F, TL>;

template <template <class> class P, TypeList TL>
using Filtered = typename detail::Filter<P, TL>;

template <template <class> class P, TypeList TL>
using Filter = typename detail::MapNil<typename Filtered<P, TL>::Head,
                                       typename Filtered<P, TL>::Tail>;

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
