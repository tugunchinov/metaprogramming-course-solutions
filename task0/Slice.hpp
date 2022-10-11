// TODO ----

#include <array>
#include <vector>

// -----

#include <concepts>
#include <cstdlib>
#include <iterator>
#include <span>

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

  std::strong_ordering operator<=>(const StrideIterator<T>& other) const =
      default;

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

template <typename T>
class SliceInt {
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
  [[nodiscard]] T* Data() const {
    return data_;
  }

  [[nodiscard]] bool operator==(const SliceInt& other) const = default;

 protected:
  explicit SliceInt(T* data) : data_(data) {
  }

 protected:
  T* data_;
};

};  // namespace utils

inline constexpr std::ptrdiff_t dynamic_stride = -1;

template <class T, std::size_t extent = std::dynamic_extent,
          std::ptrdiff_t stride = 1>
class Slice : public utils::SliceInt<T> {
  using Base = utils::SliceInt<T>;

 public:
  Slice() : Base(nullptr) {
  }

  explicit Slice(T* data) : Base(data) {
  }

  template <utils::SliceableContainer Container>
  Slice(Container& c) : Base(&(*c.begin())) {
  }

  template <std::convertible_to<T> U>
  Slice(const Slice<U, extent, stride>& other) : Base(other.Data()) {
  }

  // Data, Size, Stride, begin, end, casts, etc...

  // Slice<T, std::dynamic_extent, stride> DropFirst(std::size_t count) const;

  // template <std::size_t count>
  // Slice<T, /*?*/, stride> DropFirst() const;

  // Slice<T, std::dynamic_extent, stride> DropLast(std::size_t count) const;

  // template <std::size_t count>
  // Slice<T, /*?*/, stride> DropLast() const;

  Slice<T, std::dynamic_extent, dynamic_stride> Skip(
      std::ptrdiff_t skip) const {
    return Slice<T, std::dynamic_extent, dynamic_stride>(
        Base::Data(), (extent + skip - 1) / skip, skip * stride);
  }

  template <std::ptrdiff_t skip>
  Slice<T, (extent + skip - 1) / skip, skip * stride> Skip() const {
    return Slice<T, (extent + skip - 1) / skip, skip * stride>(Base::Data());
  }

  [[nodiscard]] bool operator==(const Slice& other) const = default;

  constexpr typename Base::reference operator[](std::size_t idx) const {
    return Base::Data()[idx * stride];
  }

  [[nodiscard]] typename Base::iterator begin() {
    return typename Base::iterator(Base::Data(), stride);
  }

  [[nodiscard]] typename Base::iterator end() {
    return typename Base::iterator(Base::Data() + extent * stride, stride);
  }

  [[nodiscard]] typename Base::reverse_iterator rbegin() {
    return Base::reverse_iterator(end());
  }

  [[nodiscard]] typename Base::reverse_iterator rend() {
    return Base::reverse_iterator(begin());
  }

  [[nodiscard]] constexpr std::size_t Size() const {
    return extent;
  }

  [[nodiscard]] constexpr std::ptrdiff_t Stride() const {
    return stride;
  }
};

template <class T>
class Slice<T, std::dynamic_extent, dynamic_stride>
    : public utils::SliceInt<T> {
  using Base = utils::SliceInt<T>;

 public:
  Slice() : Base(nullptr), extent_(0), stride_(0) {
  }

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

  template <std::ptrdiff_t skip>
  Slice Skip() const {
    return Slice(Base::Data(), (extent_ + skip - 1) / skip, skip * stride_);
  }

  [[nodiscard]] bool operator==(const Slice& other) const = default;

  typename Base::reference operator[](std::size_t idx) const {
    return Base::Data()[idx * stride_];
  }

  [[nodiscard]] typename Base::iterator begin() {
    return typename Base::iterator(Base::Data(), stride_);
  }

  [[nodiscard]] typename Base::iterator end() {
    return typename Base::iterator(Base::Data() + extent_ * stride_, stride_);
  }

  [[nodiscard]] typename Base::reverse_iterator rbegin() {
    return Base::reverse_iterator(end());
  }

  [[nodiscard]] typename Base::reverse_iterator rend() {
    return Base::reverse_iterator(begin());
  }

  [[nodiscard]] std::size_t Size() const {
    return extent_;
  }

  [[nodiscard]] std::ptrdiff_t Stride() const {
    return stride_;
  }

 private:
  std::size_t extent_;
  std::ptrdiff_t stride_;
};

template <class T, std::ptrdiff_t stride>
class Slice<T, std::dynamic_extent, stride> : public utils::SliceInt<T> {
  using Base = utils::SliceInt<T>;

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

  Slice<T, std::dynamic_extent, dynamic_stride> Skip(
      std::ptrdiff_t skip) const {
    return Slice<T, std::dynamic_extent, dynamic_stride>(
        Base::Data(), (extent_ + skip - 1) / skip, skip * stride);
  }

  template <std::ptrdiff_t skip>
  Slice<T, std::dynamic_extent, skip * stride> Skip() const {
    return Slice<T, std::dynamic_extent, skip * stride>(
        Base::Data(), (extent_ + skip - 1) / skip);
  }

  Slice First(std::size_t count) const {
    return Slice(Base::Data(), count);
  }

  template <std::size_t count>
  Slice<T, count, stride> First() const {
    return Slice<T, count, stride>(Base::Data());
  }

  Slice Last(std::size_t count) const {
    return Slice(Base::Data() + extent_ - stride * count, count);
  }

  template <std::size_t count>
  Slice<T, count, stride> Last() const {
    return Slice<T, count, stride>(Base::Data() + extent_ - stride * count);
  }

  Slice DropFirst(std::size_t count) const {
    return Slice(Base::Data() + count * stride, extent_ - count);
  }

  template <std::size_t count>
  Slice DropFirst() const {
    return Slice(Base::Data() + count * stride, extent_ - count);
  }

  Slice DropLast(std::size_t count) const {
    return Slice(Base::Data(), extent_ - count);
  }

  template <std::size_t count>
  Slice DropLast() const {
    return Slice(Base::Data(), extent_ - count);
  }

  [[nodiscard]] bool operator==(const Slice& other) const = default;

  constexpr typename Base::reference operator[](std::size_t idx) const {
    return Base::Data()[idx * stride];
  }

  [[nodiscard]] typename Base::iterator begin() {
    return typename Base::iterator(Base::Data(), stride);
  }

  [[nodiscard]] typename Base::iterator end() {
    return typename Base::iterator(Base::Data() + extent_ * stride, stride);
  }

  [[nodiscard]] typename Base::reverse_iterator rbegin() {
    return Base::reverse_iterator(end());
  }

  [[nodiscard]] typename Base::reverse_iterator rend() {
    return Base::reverse_iterator(begin());
  }

  [[nodiscard]] std::size_t Size() const {
    return extent_;
  }

  [[nodiscard]] constexpr std::ptrdiff_t Stride() const {
    return stride;
  }

 private:
  std::size_t extent_;
};

template <class T, std::size_t extent>
class Slice<T, extent, dynamic_stride> : public utils::SliceInt<T> {
  using Base = utils::SliceInt<T>;

 public:
  Slice() : Base(nullptr), stride_(0) {
  }

  template <std::convertible_to<T> U>  // TODO: same_size ?
  explicit Slice(U* data, std::ptrdiff_t stride) : Base(data), stride_(stride) {
  }

  template <std::convertible_to<T> U, std::ptrdiff_t stride>
  Slice(const Slice<U, extent, stride>& other)
      : Base(other.Data()), stride_(stride) {
  }

  [[nodiscard]] constexpr std::size_t Size() const {
    return extent;
  }

  [[nodiscard]] std::ptrdiff_t Stride() const {
    return stride_;
  }

  [[nodiscard]] bool operator==(const Slice& other) const = default;

  typename Base::reference operator[](std::size_t idx) const {
    return Base::Data()[idx * stride_];
  }

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
