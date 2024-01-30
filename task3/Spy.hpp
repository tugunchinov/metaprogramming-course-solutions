#pragma once

#include <concepts>
#include <cstring>
#include <functional>
#include <utility>

namespace detail {

template <class... Args>
struct VoidCallable {
  virtual ~VoidCallable() {
  }

  virtual void operator()(Args...) = 0;
};

template <class F, class... Args>
  requires std::invocable<F, Args...>
struct Functor : VoidCallable<Args...> {
  Functor(F&& f) : f(std::forward<F>(f)) {
  }

  void operator()(Args... args) override {
    std::invoke(f, args...);
  }

  F f;
};

static constexpr std::size_t kSmallBufferSize = 64;

template <class Allocator>
class LoggerWrapper {
  using LogPtr = void (LoggerWrapper::*)(unsigned);
  using DestroyPtr = void (LoggerWrapper::*)();
  using CopyPtr = void (LoggerWrapper::*)(const LoggerWrapper&);
  using MovePtr = void (LoggerWrapper::*)(LoggerWrapper&);

 public:
  template <std::invocable<unsigned> Logger>
  void wrapLogger(Logger&& logger) {
    if (destroy_ptr_ != nullptr) {
      (this->*destroy_ptr_)();
    }

    destroy_ptr_ = &LoggerWrapper::destroyLogger<Logger>;

    if constexpr (std::copyable<Logger>) {
      copy_ptr_ = &LoggerWrapper::copyLogger<Logger>;
    }

    move_ptr_ = &LoggerWrapper::moveLogger<Logger>;

    createLogger(std::forward<Logger>(logger));
  }

  void wrapLogger() {
    if (destroy_ptr_ != nullptr) {
      (this->*destroy_ptr_)();
    }
  }

  LoggerWrapper(const Allocator& alloc) : alloc_(alloc) {
    logger_.head_ = nullptr;
  }

  LoggerWrapper(const LoggerWrapper& other)
      : alloc_(std::allocator_traits<
               Allocator>::select_on_container_copy_construction(other.alloc_)),
        destroy_ptr_(other.destroy_ptr_),
        copy_ptr_(other.copy_ptr_),
        move_ptr_(other.move_ptr_) {
    if (copy_ptr_ != nullptr) {
      (this->*copy_ptr_)(other);
    }
  }

  LoggerWrapper(LoggerWrapper&& other)
      : alloc_(std::move(other.alloc_)),
        destroy_ptr_(std::exchange(other.destroy_ptr_, nullptr)),
        copy_ptr_(std::exchange(other.copy_ptr_, nullptr)),
        move_ptr_(std::exchange(other.move_ptr_, nullptr)) {
    if (move_ptr_ != nullptr) {
      (this->*move_ptr_)(other);
    }
  }

  LoggerWrapper& operator=(const LoggerWrapper& other) {
    if (this == &other) {
      return *this;
    }

    if (destroy_ptr_ != nullptr) {
      (this->*destroy_ptr_)();
    }

    if constexpr (std::allocator_traits<Allocator>::
                      propagate_on_container_copy_assignment::value) {
      alloc_ = other.alloc_;
    }

    destroy_ptr_ = other.destroy_ptr_;
    copy_ptr_ = other.copy_ptr_;
    move_ptr_ = other.move_ptr_;

    if (copy_ptr_ != nullptr) {
      (this->*copy_ptr_)(other);
    }

    return *this;
  }

  LoggerWrapper& operator=(LoggerWrapper&& other) {
    if (this == &other) {
      return *this;
    }

    if (destroy_ptr_ != nullptr) {
      (this->*destroy_ptr_)();
    }

    destroy_ptr_ = std::exchange(other.destroy_ptr_, nullptr);
    copy_ptr_ = std::exchange(other.copy_ptr_, nullptr);
    move_ptr_ = std::exchange(other.move_ptr_, nullptr);

    if (move_ptr_ != nullptr) {
      (this->*move_ptr_)(other);
    }

    return *this;
  }

  ~LoggerWrapper() {
    if (destroy_ptr_ != nullptr) {
      (this->*destroy_ptr_)();
    }
  }

  void operator()(unsigned v) {
    if (call_ptr_ != nullptr) {
      (this->*call_ptr_)(v);
    }
  }

 private:
  template <std::invocable<unsigned> Logger>
  void callLogger(unsigned v) {
    if constexpr (sizeof(Logger) > kSmallBufferSize) {
      std::invoke(
          *reinterpret_cast<std::remove_reference_t<Logger>*>(logger_.head_),
          v);
    } else {
      std::invoke(
          *reinterpret_cast<std::remove_reference_t<Logger>*>(logger_.stack_),
          v);
    }
  }

  template <std::invocable<unsigned> Logger>
  void copyLogger(const LoggerWrapper& other) {
    if constexpr (sizeof(Logger) > kSmallBufferSize) {
      Logger copy = *reinterpret_cast<const Logger*>(other.logger_.head_);
      createLogger<Logger>(std::move(copy));
    } else {
      Logger copy = *reinterpret_cast<const Logger*>(other.logger_.stack_);
      createLogger<Logger>(std::move(copy));
    }
  }

  template <std::invocable<unsigned> Logger>
  void moveLogger(LoggerWrapper& other) {
    if constexpr (std::allocator_traits<Allocator>::
                      propagate_on_container_move_assignment::value) {
      alloc_ = std::move(other.alloc_);

      if constexpr (sizeof(Logger) > kSmallBufferSize) {
        logger_.head_ = other.logger_.head_;
        call_ptr_ = &LoggerWrapper::callLogger<Logger>;
      } else {
        createLogger(
            std::move(*reinterpret_cast<std::remove_reference_t<Logger>*>(
                other.logger_.stack_)));
      }
    } else {
      if (alloc_ == other.alloc_) {
        if constexpr (sizeof(Logger) > kSmallBufferSize) {
          logger_.head_ = other.logger_.head_;
          call_ptr_ = &LoggerWrapper::callLogger<Logger>;
        } else {
          createLogger(
              std::move(*reinterpret_cast<Logger*>(other.logger_.stack_)));
        }
      } else {
        alloc_ = std::move(other.alloc_);

        if constexpr (sizeof(Logger) > kSmallBufferSize) {
          createLogger(
              std::move(*reinterpret_cast<Logger*>(other.logger_.head_)));
          std::allocator_traits<Allocator>::deallocate(
              alloc_, other.logger_.head_, sizeof(Logger));
        } else {
          createLogger(
              std::move(*reinterpret_cast<Logger*>(other.logger_.stack_)));
        }
      }
    }

    other.logger_.head_ = nullptr;
  }

  template <std::invocable<unsigned> Logger>
  void createLogger(Logger&& logger) {
    logger_.head_ =
        std::allocator_traits<Allocator>::allocate(alloc_, sizeof(Logger));
    std::allocator_traits<Allocator>::construct(
        alloc_,
        reinterpret_cast<std::remove_reference_t<Logger>*>(logger_.head_),
        std::forward<Logger>(logger));

    call_ptr_ = &LoggerWrapper::callLogger<std::remove_reference_t<Logger>>;
  }

  template <std::invocable<unsigned> Logger>
    requires(sizeof(Logger) <= kSmallBufferSize)
  void createLogger(Logger&& logger) {
    std::allocator_traits<Allocator>::construct(
        alloc_,
        reinterpret_cast<std::remove_reference_t<Logger>*>(logger_.stack_),
        std::forward<Logger>(logger));

    call_ptr_ = &LoggerWrapper::callLogger<Logger>;
  }

  template <std::invocable<unsigned> Logger>
  void destroyLogger() {
    if (logger_.head_ != nullptr) {
      std::allocator_traits<Allocator>::destroy(
          alloc_,
          reinterpret_cast<std::remove_reference_t<Logger>*>(logger_.head_));
      std::allocator_traits<Allocator>::deallocate(alloc_, logger_.head_,
                                                   sizeof(Logger));
    }
    logger_.head_ = nullptr;
    call_ptr_ = nullptr;
  }

  template <std::invocable<unsigned> Logger>
    requires(sizeof(Logger) <= kSmallBufferSize)
  void destroyLogger() {
    if (call_ptr_ != nullptr) {
      std::allocator_traits<Allocator>::destroy(
          alloc_,
          reinterpret_cast<std::remove_reference_t<Logger>*>(logger_.stack_));
    }
    logger_.head_ = nullptr;
    call_ptr_ = nullptr;
  }

 private:
  union Buffer {
    std::byte* head_;
    std::byte stack_[kSmallBufferSize];
  };

 private:
  Allocator alloc_;

  DestroyPtr destroy_ptr_ = nullptr;
  CopyPtr copy_ptr_ = nullptr;
  MovePtr move_ptr_ = nullptr;
  LogPtr call_ptr_ = nullptr;

  Buffer logger_;
};

template <class T, std::invocable<unsigned> Logger>
class LifetimeWrapper {
 public:
  explicit LifetimeWrapper(T* value_ptr, Logger& logger, unsigned& ref_cnt,
                           bool& new_lifetime)
      : value_ptr_(value_ptr),
        logger_(logger),
        ref_cnt_(ref_cnt),
        new_lifetime_(new_lifetime) {
  }

  ~LifetimeWrapper() {
    if (!new_lifetime_) {
      logger_(ref_cnt_);
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
  Logger& logger_;
  unsigned& ref_cnt_;
  bool& new_lifetime_;
};

}  // namespace detail

template <class T, class Allocator = std::allocator<std::byte>>
class Spy {
 public:
  explicit Spy(T value, const Allocator& alloc = Allocator())
      : value_(std::move(value)), wrapper_(alloc) {
  }

  explicit Spy(const Allocator& alloc = Allocator())
    requires std::default_initializable<T>
      : value_(T()), wrapper_(alloc) {
  }

  Spy(const Spy& other)
    requires std::copyable<T>
      : value_(other.value_), wrapper_(other.wrapper_) {
  }

  Spy(Spy&& other)
    requires std::movable<T>
      : value_(std::move(other.value_)), wrapper_(std::move(other.wrapper_)) {
  }

  Spy& operator=(const Spy& other)
    requires std::copyable<T>
  {
    if (this == &other) {
      return *this;
    }

    value_ = other.value_;
    wrapper_ = other.wrapper_;

    return *this;
  };

  Spy& operator=(Spy&& other)
    requires std::movable<T>
  {
    if (this == &other) {
      return *this;
    }

    value_ = std::move(other.value_);
    wrapper_ = std::move(other.wrapper_);

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

  detail::LifetimeWrapper<T, detail::LoggerWrapper<Allocator>> operator->() {
    new_lifetime_ = false;
    return detail::LifetimeWrapper(&value_, wrapper_, ref_cnt_, new_lifetime_);
  }

  template <std::equality_comparable_with<T> U, class AllocatorOther>
  bool operator==(const Spy<U, AllocatorOther>& other) const {
    return value_ == other.value_;
  }

  // Resets logger
  void setLogger() {
    wrapper_.wrapLogger();
  }

  template <std::invocable<unsigned> Logger>
  void setLogger(Logger&& logger)
    requires(std::move_constructible<Logger> && std::move_constructible<T> &&
             !std::copy_constructible<T>) ||
            (std::copy_constructible<Logger> && std::copy_constructible<T>)
  {
    wrapper_.wrapLogger(std::forward<Logger>(logger));
  }

 private:
  T value_;
  detail::LoggerWrapper<Allocator> wrapper_;
  unsigned ref_cnt_ = 0;
  bool new_lifetime_ = true;
};