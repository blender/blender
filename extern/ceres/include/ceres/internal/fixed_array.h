// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: fixed_array.h
// -----------------------------------------------------------------------------
//
// A `FixedArray<T>` represents a non-resizable array of `T` where the length of
// the array can be determined at run-time. It is a good replacement for
// non-standard and deprecated uses of `alloca()` and variable length arrays
// within the GCC extension. (See
// https://gcc.gnu.org/onlinedocs/gcc/Variable-Length.html).
//
// `FixedArray` allocates small arrays inline, keeping performance fast by
// avoiding heap operations. It also helps reduce the chances of
// accidentally overflowing your stack if large input is passed to
// your function.

#ifndef CERES_PUBLIC_INTERNAL_FIXED_ARRAY_H_
#define CERES_PUBLIC_INTERNAL_FIXED_ARRAY_H_

#include <Eigen/Core>  // For Eigen::aligned_allocator
#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>

#include "ceres/internal/memory.h"
#include "glog/logging.h"

namespace ceres::internal {

constexpr static auto kFixedArrayUseDefault = static_cast<size_t>(-1);

// The default fixed array allocator.
//
// As one can not easily detect if a struct contains or inherits from a fixed
// size Eigen type, to be safe the Eigen::aligned_allocator is used by default.
// But trivial types can never contain Eigen types, so std::allocator is used to
// safe some heap memory.
template <typename T>
using FixedArrayDefaultAllocator =
    typename std::conditional<std::is_trivial<T>::value,
                              std::allocator<T>,
                              Eigen::aligned_allocator<T>>::type;

// -----------------------------------------------------------------------------
// FixedArray
// -----------------------------------------------------------------------------
//
// A `FixedArray` provides a run-time fixed-size array, allocating a small array
// inline for efficiency.
//
// Most users should not specify an `inline_elements` argument and let
// `FixedArray` automatically determine the number of elements
// to store inline based on `sizeof(T)`. If `inline_elements` is specified, the
// `FixedArray` implementation will use inline storage for arrays with a
// length <= `inline_elements`.
//
// Note that a `FixedArray` constructed with a `size_type` argument will
// default-initialize its values by leaving trivially constructible types
// uninitialized (e.g. int, int[4], double), and others default-constructed.
// This matches the behavior of c-style arrays and `std::array`, but not
// `std::vector`.
//
// Note that `FixedArray` does not provide a public allocator; if it requires a
// heap allocation, it will do so with global `::operator new[]()` and
// `::operator delete[]()`, even if T provides class-scope overrides for these
// operators.
template <typename T,
          size_t N = kFixedArrayUseDefault,
          typename A = FixedArrayDefaultAllocator<T>>
class FixedArray {
  static_assert(!std::is_array<T>::value || std::extent<T>::value > 0,
                "Arrays with unknown bounds cannot be used with FixedArray.");

  static constexpr size_t kInlineBytesDefault = 256;

  using AllocatorTraits = std::allocator_traits<A>;
  // std::iterator_traits isn't guaranteed to be SFINAE-friendly until C++17,
  // but this seems to be mostly pedantic.
  template <typename Iterator>
  using EnableIfForwardIterator = typename std::enable_if<std::is_convertible<
      typename std::iterator_traits<Iterator>::iterator_category,
      std::forward_iterator_tag>::value>::type;
  static constexpr bool DefaultConstructorIsNonTrivial() {
    return !std::is_trivially_default_constructible<StorageElement>::value;
  }

 public:
  using allocator_type = typename AllocatorTraits::allocator_type;
  using value_type = typename AllocatorTraits::value_type;
  using pointer = typename AllocatorTraits::pointer;
  using const_pointer = typename AllocatorTraits::const_pointer;
  using reference = value_type&;
  using const_reference = const value_type&;
  using size_type = typename AllocatorTraits::size_type;
  using difference_type = typename AllocatorTraits::difference_type;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  static constexpr size_type inline_elements =
      (N == kFixedArrayUseDefault ? kInlineBytesDefault / sizeof(value_type)
                                  : static_cast<size_type>(N));

  FixedArray(const FixedArray& other,
             const allocator_type& a = allocator_type())
      : FixedArray(other.begin(), other.end(), a) {}

  FixedArray(FixedArray&& other, const allocator_type& a = allocator_type())
      : FixedArray(std::make_move_iterator(other.begin()),
                   std::make_move_iterator(other.end()),
                   a) {}

  // Creates an array object that can store `n` elements.
  // Note that trivially constructible elements will be uninitialized.
  explicit FixedArray(size_type n, const allocator_type& a = allocator_type())
      : storage_(n, a) {
    if (DefaultConstructorIsNonTrivial()) {
      ConstructRange(storage_.alloc(), storage_.begin(), storage_.end());
    }
  }

  // Creates an array initialized with `n` copies of `val`.
  FixedArray(size_type n,
             const value_type& val,
             const allocator_type& a = allocator_type())
      : storage_(n, a) {
    ConstructRange(storage_.alloc(), storage_.begin(), storage_.end(), val);
  }

  // Creates an array initialized with the size and contents of `init_list`.
  FixedArray(std::initializer_list<value_type> init_list,
             const allocator_type& a = allocator_type())
      : FixedArray(init_list.begin(), init_list.end(), a) {}

  // Creates an array initialized with the elements from the input
  // range. The array's size will always be `std::distance(first, last)`.
  // REQUIRES: Iterator must be a forward_iterator or better.
  template <typename Iterator, EnableIfForwardIterator<Iterator>* = nullptr>
  FixedArray(Iterator first,
             Iterator last,
             const allocator_type& a = allocator_type())
      : storage_(std::distance(first, last), a) {
    CopyRange(storage_.alloc(), storage_.begin(), first, last);
  }

  ~FixedArray() noexcept {
    for (auto* cur = storage_.begin(); cur != storage_.end(); ++cur) {
      AllocatorTraits::destroy(storage_.alloc(), cur);
    }
  }

  // Assignments are deleted because they break the invariant that the size of a
  // `FixedArray` never changes.
  void operator=(FixedArray&&) = delete;
  void operator=(const FixedArray&) = delete;

  // FixedArray::size()
  //
  // Returns the length of the fixed array.
  size_type size() const { return storage_.size(); }

  // FixedArray::max_size()
  //
  // Returns the largest possible value of `std::distance(begin(), end())` for a
  // `FixedArray<T>`. This is equivalent to the most possible addressable bytes
  // over the number of bytes taken by T.
  constexpr size_type max_size() const {
    return (std::numeric_limits<difference_type>::max)() / sizeof(value_type);
  }

  // FixedArray::empty()
  //
  // Returns whether or not the fixed array is empty.
  bool empty() const { return size() == 0; }

  // FixedArray::memsize()
  //
  // Returns the memory size of the fixed array in bytes.
  size_t memsize() const { return size() * sizeof(value_type); }

  // FixedArray::data()
  //
  // Returns a const T* pointer to elements of the `FixedArray`. This pointer
  // can be used to access (but not modify) the contained elements.
  const_pointer data() const { return AsValueType(storage_.begin()); }

  // Overload of FixedArray::data() to return a T* pointer to elements of the
  // fixed array. This pointer can be used to access and modify the contained
  // elements.
  pointer data() { return AsValueType(storage_.begin()); }

  // FixedArray::operator[]
  //
  // Returns a reference the ith element of the fixed array.
  // REQUIRES: 0 <= i < size()
  reference operator[](size_type i) {
    DCHECK_LT(i, size());
    return data()[i];
  }

  // Overload of FixedArray::operator()[] to return a const reference to the
  // ith element of the fixed array.
  // REQUIRES: 0 <= i < size()
  const_reference operator[](size_type i) const {
    DCHECK_LT(i, size());
    return data()[i];
  }

  // FixedArray::front()
  //
  // Returns a reference to the first element of the fixed array.
  reference front() { return *begin(); }

  // Overload of FixedArray::front() to return a reference to the first element
  // of a fixed array of const values.
  const_reference front() const { return *begin(); }

  // FixedArray::back()
  //
  // Returns a reference to the last element of the fixed array.
  reference back() { return *(end() - 1); }

  // Overload of FixedArray::back() to return a reference to the last element
  // of a fixed array of const values.
  const_reference back() const { return *(end() - 1); }

  // FixedArray::begin()
  //
  // Returns an iterator to the beginning of the fixed array.
  iterator begin() { return data(); }

  // Overload of FixedArray::begin() to return a const iterator to the
  // beginning of the fixed array.
  const_iterator begin() const { return data(); }

  // FixedArray::cbegin()
  //
  // Returns a const iterator to the beginning of the fixed array.
  const_iterator cbegin() const { return begin(); }

  // FixedArray::end()
  //
  // Returns an iterator to the end of the fixed array.
  iterator end() { return data() + size(); }

  // Overload of FixedArray::end() to return a const iterator to the end of the
  // fixed array.
  const_iterator end() const { return data() + size(); }

  // FixedArray::cend()
  //
  // Returns a const iterator to the end of the fixed array.
  const_iterator cend() const { return end(); }

  // FixedArray::rbegin()
  //
  // Returns a reverse iterator from the end of the fixed array.
  reverse_iterator rbegin() { return reverse_iterator(end()); }

  // Overload of FixedArray::rbegin() to return a const reverse iterator from
  // the end of the fixed array.
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }

  // FixedArray::crbegin()
  //
  // Returns a const reverse iterator from the end of the fixed array.
  const_reverse_iterator crbegin() const { return rbegin(); }

  // FixedArray::rend()
  //
  // Returns a reverse iterator from the beginning of the fixed array.
  reverse_iterator rend() { return reverse_iterator(begin()); }

  // Overload of FixedArray::rend() for returning a const reverse iterator
  // from the beginning of the fixed array.
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  // FixedArray::crend()
  //
  // Returns a reverse iterator from the beginning of the fixed array.
  const_reverse_iterator crend() const { return rend(); }

  // FixedArray::fill()
  //
  // Assigns the given `value` to all elements in the fixed array.
  void fill(const value_type& val) { std::fill(begin(), end(), val); }

  // Relational operators. Equality operators are elementwise using
  // `operator==`, while order operators order FixedArrays lexicographically.
  friend bool operator==(const FixedArray& lhs, const FixedArray& rhs) {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
  }

  friend bool operator!=(const FixedArray& lhs, const FixedArray& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator<(const FixedArray& lhs, const FixedArray& rhs) {
    return std::lexicographical_compare(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
  }

  friend bool operator>(const FixedArray& lhs, const FixedArray& rhs) {
    return rhs < lhs;
  }

  friend bool operator<=(const FixedArray& lhs, const FixedArray& rhs) {
    return !(rhs < lhs);
  }

  friend bool operator>=(const FixedArray& lhs, const FixedArray& rhs) {
    return !(lhs < rhs);
  }

 private:
  // StorageElement
  //
  // For FixedArrays with a C-style-array value_type, StorageElement is a POD
  // wrapper struct called StorageElementWrapper that holds the value_type
  // instance inside. This is needed for construction and destruction of the
  // entire array regardless of how many dimensions it has. For all other cases,
  // StorageElement is just an alias of value_type.
  //
  // Maintainer's Note: The simpler solution would be to simply wrap value_type
  // in a struct whether it's an array or not. That causes some paranoid
  // diagnostics to misfire, believing that 'data()' returns a pointer to a
  // single element, rather than the packed array that it really is.
  // e.g.:
  //
  //     FixedArray<char> buf(1);
  //     sprintf(buf.data(), "foo");
  //
  //     error: call to int __builtin___sprintf_chk(etc...)
  //     will always overflow destination buffer [-Werror]
  //
  template <typename OuterT,
            typename InnerT = typename std::remove_extent<OuterT>::type,
            size_t InnerN = std::extent<OuterT>::value>
  struct StorageElementWrapper {
    InnerT array[InnerN];
  };

  using StorageElement =
      typename std::conditional<std::is_array<value_type>::value,
                                StorageElementWrapper<value_type>,
                                value_type>::type;

  static pointer AsValueType(pointer ptr) { return ptr; }
  static pointer AsValueType(StorageElementWrapper<value_type>* ptr) {
    return std::addressof(ptr->array);
  }

  static_assert(sizeof(StorageElement) == sizeof(value_type));
  static_assert(alignof(StorageElement) == alignof(value_type));

  class NonEmptyInlinedStorage {
   public:
    StorageElement* data() { return reinterpret_cast<StorageElement*>(buff_); }
    void AnnotateConstruct(size_type) {}
    void AnnotateDestruct(size_type) {}

    // #ifdef ADDRESS_SANITIZER
    //     void* RedzoneBegin() { return &redzone_begin_; }
    //     void* RedzoneEnd() { return &redzone_end_ + 1; }
    // #endif  // ADDRESS_SANITIZER

   private:
    // ADDRESS_SANITIZER_REDZONE(redzone_begin_);
    alignas(StorageElement) char buff_[sizeof(StorageElement[inline_elements])];
    // ADDRESS_SANITIZER_REDZONE(redzone_end_);
  };

  class EmptyInlinedStorage {
   public:
    StorageElement* data() { return nullptr; }
    void AnnotateConstruct(size_type) {}
    void AnnotateDestruct(size_type) {}
  };

  using InlinedStorage =
      typename std::conditional<inline_elements == 0,
                                EmptyInlinedStorage,
                                NonEmptyInlinedStorage>::type;

  // Storage
  //
  // An instance of Storage manages the inline and out-of-line memory for
  // instances of FixedArray. This guarantees that even when construction of
  // individual elements fails in the FixedArray constructor body, the
  // destructor for Storage will still be called and out-of-line memory will be
  // properly deallocated.
  //
  class Storage : public InlinedStorage {
   public:
    Storage(size_type n, const allocator_type& a)
        : size_alloc_(n, a), data_(InitializeData()) {}

    ~Storage() noexcept {
      if (UsingInlinedStorage(size())) {
        InlinedStorage::AnnotateDestruct(size());
      } else {
        AllocatorTraits::deallocate(alloc(), AsValueType(begin()), size());
      }
    }

    size_type size() const { return std::get<0>(size_alloc_); }
    StorageElement* begin() const { return data_; }
    StorageElement* end() const { return begin() + size(); }
    allocator_type& alloc() { return std::get<1>(size_alloc_); }

   private:
    static bool UsingInlinedStorage(size_type n) {
      return n <= inline_elements;
    }

    StorageElement* InitializeData() {
      if (UsingInlinedStorage(size())) {
        InlinedStorage::AnnotateConstruct(size());
        return InlinedStorage::data();
      } else {
        return reinterpret_cast<StorageElement*>(
            AllocatorTraits::allocate(alloc(), size()));
      }
    }

    // Using std::tuple and not absl::CompressedTuple, as it has a lot of
    // dependencies to other absl headers.
    std::tuple<size_type, allocator_type> size_alloc_;
    StorageElement* data_;
  };

  Storage storage_;
};

template <typename T, size_t N, typename A>
constexpr size_t FixedArray<T, N, A>::kInlineBytesDefault;

template <typename T, size_t N, typename A>
constexpr typename FixedArray<T, N, A>::size_type
    FixedArray<T, N, A>::inline_elements;

}  // namespace ceres::internal

#endif  // CERES_PUBLIC_INTERNAL_FIXED_ARRAY_H_
