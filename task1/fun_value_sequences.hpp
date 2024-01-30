#pragma once

#include <value_types.hpp>
#include <type_lists.hpp>

namespace detail {

template <class V1, class V2>
struct Add {
  using Type = value_types::ValueTag<V1::Value + V2::Value>;
};

template <int N>
struct Fibonacci {
  static const int Value = Fibonacci<N - 1>::Value + Fibonacci<N - 2>::Value;
  using Type = value_types::ValueTag<Value>;
};

template <>
struct Fibonacci<1> {
  static const int Value = 1;
  using Type = value_types::ValueTag<Value>;
};

template <>
struct Fibonacci<0> {
  static const int Value = 0;
  using Type = value_types::ValueTag<Value>;
};

template <int N, int D>
struct IsDivisibleBy {
  static const bool Value = (N % D) && IsDivisibleBy<N, D - 1>::Value;
};

template <int N>
struct IsDivisibleBy<N, 1> {
  static const bool Value = true;
};

template <int N>
struct IsPrime {
  static const bool Value = IsDivisibleBy<N, N - 1>::Value;
};

}  // namespace detail

template <class V1, class V2>
using Add = typename detail::Add<V1, V2>::Type;

using Nats = type_lists::Scanl<Add, value_types::ValueTag<0>,
                               type_lists::Repeat<value_types::ValueTag<1>>>;

template <class T>
using GetFib = typename detail::Fibonacci<T::Value>::Type;

using Fib = type_lists::Map<GetFib, Nats>;

template <class T>
using IsPrime = typename detail::IsPrime<T::Value>;

using Primes = type_lists::Filter<IsPrime, type_lists::Drop<2, Nats>>;
