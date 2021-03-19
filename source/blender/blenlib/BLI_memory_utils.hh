/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bli
 * Some of the functions below have very similar alternatives in the standard library. However, it
 * is rather annoying to use those when debugging. Therefore, some more specialized and easier to
 * debug functions are provided here.
 */

#include <memory>
#include <new>
#include <type_traits>

#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

namespace blender {

/**
 * Call the destructor on n consecutive values. For trivially destructible types, this does
 * nothing.
 *
 * Exception Safety: Destructors shouldn't throw exceptions.
 *
 * Before:
 *  ptr: initialized
 * After:
 *  ptr: uninitialized
 */
template<typename T> void destruct_n(T *ptr, int64_t n)
{
  BLI_assert(n >= 0);

  static_assert(std::is_nothrow_destructible_v<T>,
                "This should be true for all types. Destructors are noexcept by default.");

  /* This is not strictly necessary, because the loop below will be optimized away anyway. It is
   * nice to make behavior this explicitly, though. */
  if (std::is_trivially_destructible_v<T>) {
    return;
  }

  for (int64_t i = 0; i < n; i++) {
    ptr[i].~T();
  }
}

/**
 * Call the default constructor on n consecutive elements. For trivially constructible types, this
 * does nothing.
 *
 * Exception Safety: Strong.
 *
 * Before:
 *  ptr: uninitialized
 * After:
 *  ptr: initialized
 */
template<typename T> void default_construct_n(T *ptr, int64_t n)
{
  BLI_assert(n >= 0);

  /* This is not strictly necessary, because the loop below will be optimized away anyway. It is
   * nice to make behavior this explicitly, though. */
  if (std::is_trivially_constructible_v<T>) {
    return;
  }

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new (static_cast<void *>(ptr + current)) T;
    }
  }
  catch (...) {
    destruct_n(ptr, current);
    throw;
  }
}

/**
 * Copy n values from src to dst.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: initialized
 * After:
 *  src: initialized
 *  dst: initialized
 */
template<typename T> void initialized_copy_n(const T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  for (int64_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }
}

/**
 * Copy n values from src to dst.
 *
 * Exception Safety: Strong.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: initialized
 *  dst: initialized
 */
template<typename T> void uninitialized_copy_n(const T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new (static_cast<void *>(dst + current)) T(src[current]);
    }
  }
  catch (...) {
    destruct_n(dst, current);
    throw;
  }
}

/**
 * Convert n values from type `From` to type `To`.
 *
 * Exception Safety: Strong.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: initialized
 *  dst: initialized
 */
template<typename From, typename To>
void uninitialized_convert_n(const From *src, int64_t n, To *dst)
{
  BLI_assert(n >= 0);

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new (static_cast<void *>(dst + current)) To(static_cast<To>(src[current]));
    }
  }
  catch (...) {
    destruct_n(dst, current);
    throw;
  }
}

/**
 * Move n values from src to dst.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: initialized
 * After:
 *  src: initialized, moved-from
 *  dst: initialized
 */
template<typename T> void initialized_move_n(T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  for (int64_t i = 0; i < n; i++) {
    dst[i] = std::move(src[i]);
  }
}

/**
 * Move n values from src to dst.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: initialized, moved-from
 *  dst: initialized
 */
template<typename T> void uninitialized_move_n(T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new (static_cast<void *>(dst + current)) T(std::move(src[current]));
    }
  }
  catch (...) {
    destruct_n(dst, current);
    throw;
  }
}

/**
 * Relocate n values from src to dst. Relocation is a move followed by destruction of the src
 * value.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: initialized
 * After:
 *  src: uninitialized
 *  dst: initialized
 */
template<typename T> void initialized_relocate_n(T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  initialized_move_n(src, n, dst);
  destruct_n(src, n);
}

/**
 * Relocate n values from src to dst. Relocation is a move followed by destruction of the src
 * value.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  src: initialized
 *  dst: uninitialized
 * After:
 *  src: uninitialized
 *  dst: initialized
 */
template<typename T> void uninitialized_relocate_n(T *src, int64_t n, T *dst)
{
  BLI_assert(n >= 0);

  uninitialized_move_n(src, n, dst);
  destruct_n(src, n);
}

/**
 * Copy the value to n consecutive elements.
 *
 * Exception Safety: Basic.
 *
 * Before:
 *  dst: initialized
 * After:
 *  dst: initialized
 */
template<typename T> void initialized_fill_n(T *dst, int64_t n, const T &value)
{
  BLI_assert(n >= 0);

  for (int64_t i = 0; i < n; i++) {
    dst[i] = value;
  }
}

/**
 * Copy the value to n consecutive elements.
 *
 *  Exception Safety: Strong.
 *
 * Before:
 *  dst: uninitialized
 * After:
 *  dst: initialized
 */
template<typename T> void uninitialized_fill_n(T *dst, int64_t n, const T &value)
{
  BLI_assert(n >= 0);

  int64_t current = 0;
  try {
    for (; current < n; current++) {
      new (static_cast<void *>(dst + current)) T(value);
    }
  }
  catch (...) {
    destruct_n(dst, current);
    throw;
  }
}

template<typename T> struct DestructValueAtAddress {
  void operator()(T *ptr)
  {
    ptr->~T();
  }
};

/**
 * A destruct_ptr is like unique_ptr, but it will only call the destructor and will not free the
 * memory. This is useful when using custom allocators.
 */
template<typename T> using destruct_ptr = std::unique_ptr<T, DestructValueAtAddress<T>>;

/**
 * An `AlignedBuffer` is a byte array with at least the given size and alignment. The buffer will
 * not be initialized by the default constructor.
 */
template<size_t Size, size_t Alignment> class alignas(Alignment) AlignedBuffer {
 private:
  /* Don't create an empty array. This causes problems with some compilers. */
  char buffer_[(Size > 0) ? Size : 1];

 public:
  operator void *()
  {
    return buffer_;
  }

  operator const void *() const
  {
    return buffer_;
  }

  void *ptr()
  {
    return buffer_;
  }

  const void *ptr() const
  {
    return buffer_;
  }
};

/**
 * This can be used to reserve memory for C++ objects whose lifetime is different from the
 * lifetime of the object they are embedded in. It's used by containers with small buffer
 * optimization and hash table implementations.
 */
template<typename T, int64_t Size = 1> class TypedBuffer {
 private:
  AlignedBuffer<sizeof(T) * (size_t)Size, alignof(T)> buffer_;

 public:
  operator T *()
  {
    return static_cast<T *>(buffer_.ptr());
  }

  operator const T *() const
  {
    return static_cast<const T *>(buffer_.ptr());
  }

  T &operator*()
  {
    return *static_cast<T *>(buffer_.ptr());
  }

  const T &operator*() const
  {
    return *static_cast<const T *>(buffer_.ptr());
  }

  T *ptr()
  {
    return static_cast<T *>(buffer_.ptr());
  }

  const T *ptr() const
  {
    return static_cast<const T *>(buffer_.ptr());
  }

  T &ref()
  {
    return *static_cast<T *>(buffer_.ptr());
  }

  const T &ref() const
  {
    return *static_cast<const T *>(buffer_.ptr());
  }
};

/* A dynamic stack buffer can be used instead of #alloca when wants to allocate a dynamic amount of
 * memory on the stack. Using this class has some advantages:
 *  - It falls back to heap allocation, when the size is too large.
 *  - It can be used in loops safely.
 *  - If the buffer is heap allocated, it is free automatically in the destructor.
 */
template<size_t ReservedSize = 64, size_t ReservedAlignment = 64>
class alignas(ReservedAlignment) DynamicStackBuffer {
 private:
  /* Don't create an empty array. This causes problems with some compilers. */
  char reserved_buffer_[(ReservedSize > 0) ? ReservedSize : 1];
  void *buffer_;

 public:
  DynamicStackBuffer(const int64_t size, const int64_t alignment)
  {
    BLI_assert(size >= 0);
    BLI_assert(alignment >= 0);
    if (size <= ReservedSize && alignment <= ReservedAlignment) {
      buffer_ = reserved_buffer_;
    }
    else {
      buffer_ = MEM_mallocN_aligned(size, alignment, __func__);
    }
  }
  ~DynamicStackBuffer()
  {
    if (buffer_ != reserved_buffer_) {
      MEM_freeN(buffer_);
    }
  }

  /* Don't allow any copying or moving of this type. */
  DynamicStackBuffer(const DynamicStackBuffer &other) = delete;
  DynamicStackBuffer(DynamicStackBuffer &&other) = delete;
  DynamicStackBuffer &operator=(const DynamicStackBuffer &other) = delete;
  DynamicStackBuffer &operator=(DynamicStackBuffer &&other) = delete;

  void *buffer() const
  {
    return buffer_;
  }
};

/**
 * This can be used by container constructors. A parameter of this type should be used to indicate
 * that the constructor does not construct the elements.
 */
class NoInitialization {
};

/**
 * This can be used to mark a constructor of an object that does not throw exceptions. Other
 * constructors can delegate to this constructor to make sure that the object lifetime starts.
 * With this, the destructor of the object will be called, even when the remaining constructor
 * throws.
 */
class NoExceptConstructor {
};

/**
 * Helper variable that checks if a pointer type can be converted into another pointer type without
 * issues. Possible issues are casting away const and casting a pointer to a child class.
 * Adding const or casting to a parent class is fine.
 */
template<typename From, typename To>
inline constexpr bool is_convertible_pointer_v =
    std::is_convertible_v<From, To> &&std::is_pointer_v<From> &&std::is_pointer_v<To>;

/**
 * Helper variable that checks if a Span<From> can be converted to Span<To> safely, whereby From
 * and To are pointers. Adding const and casting to a void pointer is allowed.
 * Casting up and down a class hierarchy generally is not allowed, because this might change the
 * pointer under some circumstances.
 */
template<typename From, typename To>
inline constexpr bool is_span_convertible_pointer_v =
    /* Make sure we are working with pointers. */
    std::is_pointer_v<From> &&std::is_pointer_v<To> &&
    (/* No casting is necessary when both types are the same. */
     std::is_same_v<From, To> ||
     /* Allow adding const to the underlying type. */
     std::is_same_v<const std::remove_pointer_t<From>, std::remove_pointer_t<To>> ||
     /* Allow casting non-const pointers to void pointers. */
     (!std::is_const_v<std::remove_pointer_t<From>> && std::is_same_v<To, void *>) ||
     /* Allow casting any pointer to const void pointers. */
     std::is_same_v<To, const void *>);

/**
 * Inline buffers for small-object-optimization should be disable by default. Otherwise we might
 * get large unexpected allocations on the stack.
 */
inline constexpr int64_t default_inline_buffer_capacity(size_t element_size)
{
  return (static_cast<int64_t>(element_size) < 100) ? 4 : 0;
}

/**
 * This can be used by containers to implement an exception-safe copy-assignment-operator.
 * It assumes that the container has an exception safe copy constructor and an exception-safe
 * move-assignment-operator.
 */
template<typename Container> Container &copy_assign_container(Container &dst, const Container &src)
{
  if (&src == &dst) {
    return dst;
  }

  Container container_copy{src};
  dst = std::move(container_copy);
  return dst;
}

/**
 * This can be used by containers to implement an exception-safe move-assignment-operator.
 * It assumes that the container has an exception-safe move-constructor and a noexcept constructor
 * tagged with the NoExceptConstructor tag.
 */
template<typename Container>
Container &move_assign_container(Container &dst, Container &&src) noexcept(
    std::is_nothrow_move_constructible_v<Container>)
{
  if (&dst == &src) {
    return dst;
  }

  dst.~Container();
  if constexpr (std::is_nothrow_move_constructible_v<Container>) {
    new (&dst) Container(std::move(src));
  }
  else {
    try {
      new (&dst) Container(std::move(src));
    }
    catch (...) {
      new (&dst) Container(NoExceptConstructor());
      throw;
    }
  }
  return dst;
}

}  // namespace blender
