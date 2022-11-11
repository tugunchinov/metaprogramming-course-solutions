#pragma once

#include <concepts>
#include <cstring>
#include <functional>

namespace detail {
template <class Allocator, class... Args>
struct VoidCallable {
  virtual ~VoidCallable() {
  }

  virtual void operator()(Args...) = 0;

  virtual void construct(std::byte* place, Allocator& alloc) = 0;
};

template <class Allocator, class F, class... Args>
  requires std::invocable<F, Args...>
struct Functor : VoidCallable<Allocator, Args...> {
  Functor(F&& f) : f(std::move(f)) {
  }

  void operator()(Args... args) override {
    std::invoke(f, args...);
  }

  void construct(std::byte* place, Allocator& alloc) override {
    std::allocator_traits<Allocator>::construct(
        alloc, reinterpret_cast<Functor*>(place), std::move(f));
  }

  F f;
};

template <class Allocator, std::copy_constructible F, class... Args>
  requires std::invocable<F, Args...>
struct Functor<Allocator, F, Args...> : VoidCallable<Allocator, Args...> {
  Functor(const F& f) : f(f) {
  }

  void operator()(Args... args) override {
    std::invoke(f, args...);
  }

  void construct(std::byte* place, Allocator& alloc) override {
    std::allocator_traits<Allocator>::construct(
        alloc, reinterpret_cast<Functor*>(place), f);
  }

  F f;
};

}  // namespace detail

template <class T, class Allocator = std::allocator<std::byte>>
class Spy {
  using LogPtr = detail::VoidCallable<Allocator, unsigned>*;

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
        allocator_(std::allocator_traits<Allocator>::
                       select_on_container_copy_construction(other.allocator_)),
        logger_size_(other.logger_size_) {
    if (other.getLogger() != nullptr) {
      reinterpret_cast<LogPtr>(other.getLogger())
          ->construct(allocateLogger(other.logger_size_), allocator_);
    }
  }

  Spy(Spy&& other)
    requires std::movable<T> && std::move_constructible<T>
      : value_(std::move(other.value_)),
        allocator_(std::move(other.allocator_)),
        logger_size_(other.logger_size_) {
    if (logger_size_ > kSmallBufferSize) {
      logger_ = other.logger_;
    } else if (logger_size_ > 0) {
      reinterpret_cast<LogPtr>(other.getLogger())
          ->construct(getLogger(), allocator_);
    }
    other.logger_size_ = 0;
  }

  Spy& operator=(const Spy& other)
    requires std::copyable<T> && std::copy_constructible<T>
  {
    if (this == &other) {
      return *this;
    }

    setLogger();

    value_ = other.value_;

    if constexpr (std::allocator_traits<Allocator>::
                      propagate_on_container_copy_assignment::value) {
      allocator_ = other.allocator_;
    }

    if (other.getLogger() != nullptr) {
      reinterpret_cast<LogPtr>(other.getLogger())
          ->construct(allocateLogger(other.logger_size_), allocator_);
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
    logger_size_ = other.logger_size_;

    if constexpr (std::allocator_traits<Allocator>::
                      propagate_on_container_move_assignment::value) {
      allocator_ = std::move(other.allocator_);

      if (logger_size_ > kSmallBufferSize) {
        logger_ = other.logger_;
      } else if (logger_size_ > 0) {
        reinterpret_cast<LogPtr>(other.getLogger())
            ->construct(getLogger(), allocator_);
      }
    } else {
      if (allocator_ == other.allocator_) {
        if (logger_size_ > kSmallBufferSize) {
          logger_ = other.logger_;
        } else if (logger_size_ > 0) {
          reinterpret_cast<LogPtr>(other.getLogger())
              ->construct(getLogger(), allocator_);
        }
      } else {
        allocator_ = std::move(other.allocator_);
        reinterpret_cast<LogPtr>(other.getLogger())
            ->construct(allocateLogger(logger_size_), allocator_);

        if (logger_size_ > kSmallBufferSize) {
          std::allocator_traits<Allocator>::deallocate(
              allocator_, other.getLogger(), logger_size_);
        }
      }
    }

    other.logger_size_ = 0;

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
    return LifetimeWrapper(&value_, reinterpret_cast<LogPtr>(getLogger()),
                           ref_cnt_, new_lifetime_);
    ;
  }

  template <std::equality_comparable_with<T> U, class AllocatorOther>
  bool operator==(const Spy<U, AllocatorOther>& other) const {
    return value_ == other.value_;
  }

  // Resets logger
  void setLogger() {
    if (getLogger() != nullptr) {
      std::allocator_traits<Allocator>::destroy(allocator_, getLogger());
    }
    deallocateLogger();
  }

  template <std::invocable<unsigned> Logger>
  void setLogger(Logger&& logger)
    requires(std::move_constructible<Logger> && std::move_constructible<T> &&
             !std::copy_constructible<T>) ||
            (std::copy_constructible<Logger> && std::copy_constructible<T>)
  {
    using Functor = detail::Functor<Allocator, Logger, unsigned>;

    std::size_t new_logger_size = sizeof(Functor);

    setLogger();

    if constexpr (std::copy_constructible<Logger>) {
      std::allocator_traits<Allocator>::construct(
          allocator_,
          reinterpret_cast<Functor*>(allocateLogger(new_logger_size)), logger);
    } else {
      std::allocator_traits<Allocator>::construct(
          allocator_,
          reinterpret_cast<Functor*>(allocateLogger(new_logger_size)),
          std::move(logger));
    }
  }

 private:
  std::byte* getLogger() const {
    if (logger_size_ > kSmallBufferSize) {
      return logger_.head_;
    } else if (logger_size_ > 0) {
      return const_cast<std::byte*>(logger_.stack_);
    } else {
      return nullptr;
    }
  }

  std::byte* allocateLogger(std::size_t size) {
    logger_size_ = size;

    if (logger_size_ > kSmallBufferSize) {
      return logger_.head_ = std::allocator_traits<Allocator>::allocate(
                 allocator_, logger_size_);
    } else {
      return logger_.stack_;
    }
  }

  void deallocateLogger() {
    if (logger_size_ > kSmallBufferSize) {
      std::allocator_traits<Allocator>::deallocate(allocator_, logger_.head_,
                                                   logger_size_);
    }

    logger_size_ = 0;
  }

 private:
  static constexpr std::size_t kSmallBufferSize = 64;

 private:
  union Buffer {
    std::byte* head_;
    std::byte stack_[kSmallBufferSize];
  };

 private:
  T value_;
  Allocator allocator_;
  Buffer logger_;
  std::size_t logger_size_ = 0;
  unsigned ref_cnt_ = 0;
  bool new_lifetime_ = true;
};