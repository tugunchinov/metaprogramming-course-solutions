// TODO ----

#include <array>
#include <vector>

// -----

#include <concepts>
#include <cstdlib>
#include <iterator>
#include <span>

inline constexpr std::ptrdiff_t dynamic_stride = -1;

namespace utils {
template <class Container>
concept SliceableContainer =
    requires(Container c) {
      { c.begin() } -> std::same_as<typename Container::iterator>;
      { c.size() } -> std::same_as<std::size_t>;
    } && std::contiguous_iterator<typename Container::iterator>;

template <class T>
class StrideIterator {
 public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;
  using reference = T&;

 public:
  explicit StrideIterator(pointer ptr, difference_type stride = 1)
      : ptr_(ptr), stride_(stride) {
  }

  StrideIterator(const StrideIterator& other) = default;
  StrideIterator(StrideIterator&& other) = default;

  StrideIterator& operator=(const StrideIterator& other) = default;
  StrideIterator& operator=(StrideIterator&& other) = default;

  StrideIterator() = default;

  StrideIterator& operator+=(difference_type n) {
    ptr_ += n * stride_;
    return *this;
  }

  StrideIterator& operator++() {
    return *this += 1;
  }

  StrideIterator operator++(int) {
    StrideIterator copy = *this;
    *this += 1;
    return copy;
  }

  StrideIterator& operator--() {
    return *this -= 1;
  }

  StrideIterator operator--(int) {
    StrideIterator copy = *this;
    *this -= 1;
    return copy;
  }

  StrideIterator& operator-=(difference_type n) {
    return *this += -n;
  }

  StrideIterator operator-(difference_type n) const {
    StrideIterator copy = *this;
    return copy -= n;
  }

  difference_type operator-(const StrideIterator& other) const {
    return (ptr_ - other.ptr_) / stride_;
  }

  reference operator[](std::size_t n) const {
    return ptr_[n * stride_];
  }

  auto operator<=>(const StrideIterator<T>& other) const = default;

  reference operator*() const {
    return *ptr_;
  }

 private:
  pointer ptr_;
  difference_type stride_;
};

template <class T>
StrideIterator<T> operator+(const StrideIterator<T>& it,
                            typename StrideIterator<T>::difference_type n) {
  StrideIterator copy = it;
  return copy += n;
}

template <class T>
StrideIterator<T> operator+(typename StrideIterator<T>::difference_type n,
                            const StrideIterator<T>& it) {
  return it + n;
}

template <template <class, std::size_t, std::ptrdiff_t> class Slice, class T,
          std::size_t extent, std::ptrdiff_t stride>
class SliceInt {
  using Derived = Slice<T, extent, stride>;

 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

  using iterator = StrideIterator<T>;
  using reverse_iterator = std::reverse_iterator<iterator>;

 public:
  SliceInt() = default;

  SliceInt(T* data) : data_(data) {
  }

  [[nodiscard]] constexpr iterator begin() const noexcept {
    return iterator(Data(), Stride());
  }

  [[nodiscard]] constexpr iterator end() const noexcept {
    return iterator(Data() + Size() * Stride(), Stride());
  }

  [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }

  [[nodiscard]] constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

  [[nodiscard]] constexpr reference Front() const {
    return data_[0];
  }

  [[nodiscard]] reference Back() const {
    return operator[](Size() - 1);
  }

  [[nodiscard]] reference operator[](size_type idx) const {
    return Data()[idx * Stride()];
  }

  [[nodiscard]] constexpr pointer Data() const noexcept {
    return data_;
  }

  [[nodiscard]] size_type Size() const noexcept {
    return static_cast<const Derived*>(this)->Size();
  }

  [[nodiscard]] difference_type Stride() const noexcept {
    return static_cast<const Derived*>(this)->Stride();
  }

  [[nodiscard]] constexpr bool IsEmpty() const noexcept {
    return Size() == 0;
  }

  Slice<T, std::dynamic_extent, dynamic_stride> Skip(
      std::ptrdiff_t skip) const {
    return Slice<T, std::dynamic_extent, dynamic_stride>(
        Data(), (Size() + skip - 1) / skip, skip * Stride());
  }

  template <std::ptrdiff_t skip>
    requires(extent == std::dynamic_extent && stride == dynamic_stride)
  Slice<T, std::dynamic_extent, dynamic_stride> Skip() const {
    return Slice<T, std::dynamic_extent, dynamic_stride>(
        Data(), (Size() + skip - 1) / skip, skip * Stride());
  }

  template <std::ptrdiff_t skip>
    requires(extent == std::dynamic_extent && stride != dynamic_stride)
  Slice<T, std::dynamic_extent, skip * stride> Skip() const {
    return Slice<T, std::dynamic_extent, skip * stride>(
        Data(), (Size() + skip - 1) / skip);
  }

  template <std::ptrdiff_t skip>
    requires(extent != std::dynamic_extent && stride != dynamic_stride)
  Slice<T, (extent + skip - 1) / skip, skip * stride> Skip() const {
    return Slice<T, (extent + skip - 1) / skip, skip * stride>(Data());
  }

  Slice<T, std::dynamic_extent, stride> First(std::size_t count) const {
    return Slice<T, std::dynamic_extent, stride>(Data(), count);
  }

  template <std::size_t count>
  Slice<T, count, stride> First() const {
    return Slice<T, count, stride>(Data());
  }

  Slice<T, std::dynamic_extent, stride> Last(std::size_t count) const {
    return Slice<T, std::dynamic_extent, stride>(
        Data() + Size() - Stride() * count, count);
  }

  template <std::size_t count>
  Slice<T, count, stride> Last() const {
    return Slice<T, count, stride>(Data() + Size() - Stride() * count);
  }

  Slice<T, std::dynamic_extent, stride> DropFirst(std::size_t count) const {
    return Slice<T, std::dynamic_extent, stride>(Data() + count * Stride(),
                                                 Size() - count);
  }

  template <std::size_t count>
    requires(extent == std::dynamic_extent)
  Slice<T, std::dynamic_extent, stride> DropFirst() const {
    return Slice<T, std::dynamic_extent, stride>(Data() + count * Stride(),
                                                 Size() - count);
  }

  template <std::size_t count>
    requires(extent != std::dynamic_extent)
  Slice<T, extent - count, stride> DropFirst() const {
    return Slice<T, extent - count, stride>(Data() + count * Stride());
  }

  Slice<T, std::dynamic_extent, stride> DropLast(std::size_t count) const {
    return Slice<T, std::dynamic_extent, stride>(Data(), Size() - count);
  }

  template <std::size_t count>
    requires(extent == std::dynamic_extent)
  Slice<T, std::dynamic_extent, stride> DropLast() const {
    return Slice<T, std::dynamic_extent, stride>(Data(), Size() - count);
  }

  template <std::size_t count>
    requires(extent != std::dynamic_extent)
  Slice<T, extent - count, stride> DropLast() const {
    return Slice<T, extent - count, stride>(Data());
  }

  [[nodiscard]] constexpr bool operator==(
      const SliceInt& other) const noexcept = default;

 protected:
  T* data_;
};

};  // namespace utils

template <class T, std::size_t extent = std::dynamic_extent,
          std::ptrdiff_t stride = 1>
class Slice : public utils::SliceInt<Slice, T, extent, stride> {
  using Base = utils::SliceInt<Slice, T, extent, stride>;

 public:
  Slice() = default;

  Slice(T* data) : Base(data) {
  }

  template <utils::SliceableContainer Container>
  Slice(Container& c) : Base(&(*c.begin())) {
  }

  template <std::convertible_to<T> U>
  Slice(const Slice<U, extent, stride>& other) : Base(other.Data()) {
  }

  [[nodiscard]] constexpr std::size_t Size() const {
    return extent;
  }

  [[nodiscard]] constexpr std::ptrdiff_t Stride() const {
    return stride;
  }

  [[nodiscard]] constexpr bool operator==(const Slice& other) const noexcept =
      default;
};

template <class T>
class Slice<T, std::dynamic_extent, dynamic_stride>
    : public utils::SliceInt<Slice, T, std::dynamic_extent, dynamic_stride> {
  using Base = utils::SliceInt<Slice, T, std::dynamic_extent, dynamic_stride>;

 public:
  Slice() = default;

  explicit Slice(T* data, std::size_t extent, std::ptrdiff_t stride)
      : Base(data), extent_(extent), stride_(stride) {
  }

  template <std::contiguous_iterator It>
  explicit Slice(It it, std::size_t count, std::ptrdiff_t skip)
      : Base(&*it), extent_(count), stride_(skip) {
  }

  Slice(const Slice& other) = default;

  template <std::convertible_to<T> U, std::size_t extent, std::ptrdiff_t stride>
  Slice(const Slice<U, extent, stride>& other)
      : Base(other.Data()), extent_(extent), stride_(stride) {
  }

  [[nodiscard]] std::size_t Size() const noexcept {
    return extent_;
  }

  [[nodiscard]] std::ptrdiff_t Stride() const noexcept {
    return stride_;
  }

  [[nodiscard]] bool operator==(const Slice& other) const noexcept = default;

 private:
  std::size_t extent_;
  std::ptrdiff_t stride_;
};

template <class T, std::ptrdiff_t stride>
class Slice<T, std::dynamic_extent, stride>
    : public utils::SliceInt<Slice, T, std::dynamic_extent, stride> {
  using Base = utils::SliceInt<Slice, T, std::dynamic_extent, stride>;

 public:
  Slice() : Base(nullptr), extent_(0) {
  }

  template <utils::SliceableContainer Container>
  Slice(Container& c) : Base(&(*c.begin())), extent_(c.size()) {
  }

  explicit Slice(T* data, std::size_t extent) : Base(data), extent_(extent) {
  }

  template <std::convertible_to<T> U, std::size_t extent>
  Slice(const Slice<U, extent, stride>& other)
      : Base(other.Data()), extent_(extent) {
  }

  [[nodiscard]] std::size_t Size() const noexcept {
    return extent_;
  }

  [[nodiscard]] constexpr std::ptrdiff_t Stride() const noexcept {
    return stride;
  }

  [[nodiscard]] bool operator==(const Slice& other) const = default;
  
 private:
  std::size_t extent_;
};

template <class T, std::size_t extent>
class Slice<T, extent, dynamic_stride>
    : public utils::SliceInt<Slice, T, extent, dynamic_stride> {
  using Base = utils::SliceInt<Slice, T, extent, dynamic_stride>;

 public:
  Slice() : Base(nullptr), stride_(0) {
  }

  template <std::convertible_to<T> U>
  explicit Slice(U* data, std::ptrdiff_t stride) : Base(data), stride_(stride) {
  }

  template <std::convertible_to<T> U, std::ptrdiff_t stride>
  Slice(const Slice<U, extent, stride>& other)
      : Base(other.Data()), stride_(stride) {
  }

  [[nodiscard]] constexpr std::size_t Size() const noexcept {
    return extent;
  }

  [[nodiscard]] std::ptrdiff_t Stride() const noexcept {
    return stride_;
  }

  [[nodiscard]] bool operator==(const Slice& other) const = default;

 private:
  std::ptrdiff_t stride_;
};

// template <utils::SliceableContainer Container>
// Slice(Container) -> Slice<typename Container::value_type>;

// template <utils::SliceableContainer Container>
// Slice(Container c) -> Slice<typename Container::value_type, c.size()>;

template <class T>
Slice(std::vector<T>) -> Slice<T>;

template <class T, std::size_t size>
Slice(std::array<T, size>) -> Slice<T, size>;

template <std::contiguous_iterator It>
Slice(It, std::size_t, std::ptrdiff_t)
    -> Slice<typename It::value_type, std::dynamic_extent, dynamic_stride>;
