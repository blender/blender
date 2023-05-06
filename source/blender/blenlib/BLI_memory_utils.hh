/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <algorithm>
#include <memory>
#include <new>
#include <type_traits>

#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

namespace blender {

/**
 * Under some circumstances #std::is_trivial_v<T> is false even though we know that the type is
 * actually trivial. Using that extra knowledge allows for some optimizations.
 */
template<typename T> inline constexpr bool is_trivial_extended_v = std::is_trivial_v<T>;
template<typename T>
inline constexpr bool is_trivially_destructible_extended_v = is_trivial_extended_v<T> ||
                                                             std::is_trivially_destructible_v<T>;
template<typename T>
inline constexpr bool is_trivially_copy_constructible_extended_v =
    is_trivial_extended_v<T> || std::is_trivially_copy_constructible_v<T>;
template<typename T>
inline constexpr bool is_trivially_move_constructible_extended_v =
    is_trivial_extended_v<T> || std::is_trivially_move_constructible_v<T>;

template<typename T> void destruct_n(T *ptr, int64_t n)
{
  if (is_trivially_destructible_extended_v<T>) {
    return;
  }

  std::destroy_n(ptr, n);
}

template<typename T> void default_construct_n(T *ptr, int64_t n)
{
  std::uninitialized_default_construct_n(ptr, n);
}

template<typename T> void initialized_copy_n(const T *src, int64_t n, T *dst)
{
  std::copy_n(src, n, dst);
}

template<typename T> void uninitialized_copy_n(const T *src, int64_t n, T *dst)
{
  std::uninitialized_copy_n(src, n, dst);
}

template<typename From, typename To>
void uninitialized_convert_n(const From *src, int64_t n, To *dst)
{
  std::uninitialized_copy_n(src, n, dst);
}

template<typename T> void initialized_move_n(T *src, int64_t n, T *dst)
{
  std::copy_n(std::make_move_iterator(src), n, dst);
}

template<typename T> void uninitialized_move_n(T *src, int64_t n, T *dst)
{
  std::uninitialized_copy_n(std::make_move_iterator(src), n, dst);
}

template<typename T> void initialized_relocate_n(T *src, int64_t n, T *dst)
{
  initialized_move_n(src, n, dst);
  destruct_n(src, n);
}

template<typename T> void uninitialized_relocate_n(T *src, int64_t n, T *dst)
{
  uninitialized_move_n(src, n, dst);
  destruct_n(src, n);
}

template<typename T> void initialized_fill_n(T *dst, int64_t n, const T &value)
{
  std::fill_n(dst, n, value);
}

template<typename T> void uninitialized_fill_n(T *dst, int64_t n, const T &value)
{
  std::uninitialized_fill_n(dst, n, value);
}

template<typename T> struct DestructValueAtAddress {
  DestructValueAtAddress() = default;

  template<typename U> DestructValueAtAddress(const U &) {}

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
template<size_t Size, size_t Alignment> class AlignedBuffer {
  struct Empty {
  };
  struct alignas(Alignment) Sized {
    /* Don't create an empty array. This causes problems with some compilers. */
    std::byte buffer_[Size > 0 ? Size : 1];
  };

  using BufferType = std::conditional_t<Size == 0, Empty, Sized>;
  BLI_NO_UNIQUE_ADDRESS BufferType buffer_;

 public:
  operator void *()
  {
    return this;
  }

  operator const void *() const
  {
    return this;
  }

  void *ptr()
  {
    return this;
  }

  const void *ptr() const
  {
    return this;
  }
};

/**
 * This can be used to reserve memory for C++ objects whose lifetime is different from the
 * lifetime of the object they are embedded in. It's used by containers with small buffer
 * optimization and hash table implementations.
 */
template<typename T, int64_t Size = 1> class TypedBuffer {
 private:
  /** Required so that `sizeof(T)` is not required when `Size` is 0. */
  static constexpr size_t get_size()
  {
    if constexpr (Size == 0) {
      return 0;
    }
    else {
      return sizeof(T) * size_t(Size);
    }
  }

  /** Required so that `alignof(T)` is not required when `Size` is 0. */
  static constexpr size_t get_alignment()
  {
    if constexpr (Size == 0) {
      return 1;
    }
    else {
      return alignof(T);
    }
  }

  BLI_NO_UNIQUE_ADDRESS AlignedBuffer<get_size(), get_alignment()> buffer_;

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
 * Same as #std::is_same_v but allows for checking multiple types at the same time.
 */
template<typename T, typename... Args>
inline constexpr bool is_same_any_v = (std::is_same_v<T, Args> || ...);

/**
 * Inline buffers for small-object-optimization should be disable by default. Otherwise we might
 * get large unexpected allocations on the stack.
 */
inline constexpr int64_t default_inline_buffer_capacity(size_t element_size)
{
  return (int64_t(element_size) < 100) ? 4 : 0;
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

/**
 * Returns true if the value is different and was assigned.
 */
template<typename T> inline bool assign_if_different(T &old_value, T new_value)
{
  if (old_value != new_value) {
    old_value = std::move(new_value);
    return true;
  }
  return false;
}

}  // namespace blender

namespace blender::detail {

template<typename Func> struct ScopedDeferHelper {
  Func func;

  ~ScopedDeferHelper()
  {
    func();
  }
};

}  // namespace blender::detail

#define BLI_SCOPED_DEFER_NAME1(a, b) a##b
#define BLI_SCOPED_DEFER_NAME2(a, b) BLI_SCOPED_DEFER_NAME1(a, b)
#define BLI_SCOPED_DEFER_NAME(a) BLI_SCOPED_DEFER_NAME2(_scoped_defer_##a##_, __LINE__)

/**
 * Execute the given function when the current scope ends. This can be used to cheaply implement
 * some RAII-like behavior for C types that don't support it. Long term, the types we want to use
 * this with should either be converted to C++ or get a proper C++ API. Until then, this function
 * can help avoid common resource leakages.
 */
#define BLI_SCOPED_DEFER(function_to_defer) \
  auto BLI_SCOPED_DEFER_NAME(func) = (function_to_defer); \
  blender::detail::ScopedDeferHelper<decltype(BLI_SCOPED_DEFER_NAME(func))> \
      BLI_SCOPED_DEFER_NAME(helper){std::move(BLI_SCOPED_DEFER_NAME(func))};
