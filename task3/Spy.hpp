#pragma once

#include <concepts>
#include <cstring>
#include <functional>

namespace detail {
template <class... Args>
struct VoidCallable {
  virtual ~VoidCallable() {
  }

  virtual void operator()(Args...) = 0;

  virtual VoidCallable* clone() = 0;
};

template <class F, class Allocator, class... Args>
  requires std::invocable<F, Args...>
struct Functor : VoidCallable<Args...> {
  Functor(F&& f) : f(std::move(f)) {
  }

  void operator()(Args... args) override {
    std::invoke(f, args...);
  }

  Functor* clone() override {
    return nullptr;
  }

  F f;
};

template <std::copy_constructible F, class Allocator, class... Args>
  requires std::invocable<F, Args...>
struct Functor<F, Allocator, Args...> : VoidCallable<Args...> {
  Functor(const F& f, const Allocator& alloc = Allocator())
      : f(f), alloc(alloc) {
  }

  void operator()(Args... args) override {
    std::invoke(f, args...);
  }

  Functor* clone() override {
    auto size = sizeof(Functor);
    auto copy = reinterpret_cast<Functor*>(
        std::allocator_traits<Allocator>::allocate(alloc, size));
    std::allocator_traits<Allocator>::construct(alloc, copy, f, alloc);

    return copy;
  }

  F f;
  Allocator alloc;
};

}  // namespace detail

template <class T, class Allocator = std::allocator<std::byte>>
class Spy {
  using LogPtr = detail::VoidCallable<unsigned>*;

  class LifetimeWrapper {
   public:
    explicit LifetimeWrapper(T* value_ptr, LogPtr logger, unsigned& ref_cnt,
                             bool& new_lifetime)
        : value_ptr_(value_ptr),
          logger_(logger),
          ref_cnt_(ref_cnt),
          new_lifetime_(new_lifetime) {
    }

    ~LifetimeWrapper() {
      if (!new_lifetime_) {
        if (logger_ != nullptr) {
          (*logger_)(ref_cnt_);
        }
        new_lifetime_ = true;
        ref_cnt_ = 0;
      }
    }

    T* operator->() {
      ++ref_cnt_;
      return value_ptr_;
    }

   private:
    T* value_ptr_;
    LogPtr logger_;
    unsigned& ref_cnt_;
    bool& new_lifetime_;
  };

 public:
  explicit Spy(T value, const Allocator& alloc = Allocator())
      : value_(std::move(value)), allocator_(alloc) {
  }

  explicit Spy(const Allocator& alloc = Allocator())
    requires std::default_initializable<T>
      : value_(T()), allocator_(alloc) {
  }

  Spy(const Spy& other)
    requires std::copyable<T> && std::copy_constructible<T>
      : value_(other.value_),
        allocator_(other.allocator_),
        logger_size_(other.logger_size_) {
    if (other.logger_ != nullptr) {
      // TODO: better
      logger_ = reinterpret_cast<std::byte*>(
          reinterpret_cast<LogPtr>(other.logger_)->clone());
    }
  }

  Spy(Spy&& other)
    requires std::movable<T> && std::move_constructible<T>
      : value_(std::move(other.value_)),
        allocator_(std::move(other.allocator_)),
        logger_size_(other.logger_size_) {
    if (other.logger_ != nullptr) {
      logger_ =
          std::allocator_traits<Allocator>::allocate(allocator_, logger_size_);

      std::memmove(logger_, other.logger_, logger_size_);

      other.setLogger();
    }
  }

  Spy& operator=(const Spy& other)
    requires std::copyable<T> && std::copy_constructible<T>
  {
    if (this == &other) {
      return *this;
    }

    setLogger();

    value_ = other.value_;
    allocator_ = other.allocator_;
    logger_size_ = other.logger_size_;

    if (other.logger_ != nullptr) {
      logger_ = reinterpret_cast<std::byte*>(
          reinterpret_cast<LogPtr>(other.logger_)->clone());
    } else {
      logger_ = nullptr;
    }

    return *this;
  };

  Spy& operator=(Spy&& other)
    requires std::movable<T> && std::move_constructible<T>
  {
    if (this == &other) {
      return *this;
    }

    setLogger();

    value_ = std::move(other.value_);
    allocator_ = std::move(other.allocator_);
    logger_size_ = other.logger_size_;

    if (other.logger_ != nullptr) {
      logger_ =
          std::allocator_traits<Allocator>::allocate(allocator_, logger_size_);

      std::memmove(logger_, other.logger_, logger_size_);

      other.setLogger();
    } else {
      logger_ = nullptr;
    }

    return *this;
  };

  ~Spy() {
    setLogger();
  }

  T& operator*() {
    return value_;
  }

  const T& operator*() const {
    return value_;
  }

  LifetimeWrapper operator->() {
    new_lifetime_ = false;
    return LifetimeWrapper(&value_, reinterpret_cast<LogPtr>(logger_), ref_cnt_,
                           new_lifetime_);
    ;
  }

  template <class U, class AllocatorOther>
    requires std::equality_comparable_with<U, T> bool
  operator==(const Spy<U, AllocatorOther>& other) const {
    return value_ == other.value_;
  }

  // Resets logger
  // TODO: add with_destruct
  void setLogger() {
    if (logger_ != nullptr) {
      std::allocator_traits<Allocator>::destroy(allocator_, logger_);
      std::allocator_traits<Allocator>::deallocate(allocator_, logger_,
                                                   logger_size_);
      logger_size_ = 0;
      logger_ = nullptr;
    }
  }

  template <std::invocable<unsigned> Logger>
  void setLogger(Logger&& logger)
    requires(std::move_constructible<Logger> && std::move_constructible<T> &&
             !std::copy_constructible<T>) ||
            (std::copy_constructible<Logger> && std::copy_constructible<T>)
  {
    setLogger();

    logger_size_ = sizeof(detail::Functor<Logger, Allocator, unsigned>);
    logger_ =
        std::allocator_traits<Allocator>::allocate(allocator_, logger_size_);

    if constexpr (std::copy_constructible<Logger>) {
      std::allocator_traits<Allocator>::construct(
          allocator_,
          reinterpret_cast<detail::Functor<Logger, Allocator, unsigned>*>(
              logger_),
          logger, allocator_);
    } else {
      std::allocator_traits<Allocator>::construct(
          allocator_,
          reinterpret_cast<detail::Functor<Logger, Allocator, unsigned>*>(
              logger_),
          std::move(logger));
    }
  }

 private:
  T value_;
  Allocator allocator_;
  std::byte* logger_ = nullptr;
  std::size_t logger_size_ = 0;
  unsigned ref_cnt_ = 0;
  bool new_lifetime_ = true;
};