/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <sstream>

#include "BLI_cpp_type.hh"
#include "BLI_index_mask.hh"
#include "BLI_utildefines.h"

namespace blender::cpp_type_util {

template<typename T> inline bool pointer_has_valid_alignment(const void *ptr)
{
  return (uintptr_t(ptr) % alignof(T)) == 0;
}

template<typename T> inline bool pointer_can_point_to_instance(const void *ptr)
{
  return ptr != nullptr && pointer_has_valid_alignment<T>(ptr);
}

template<typename T> void default_construct_cb(void *ptr)
{
  BLI_assert(pointer_can_point_to_instance<T>(ptr));
  new (ptr) T;
}
template<typename T> void default_construct_indices_cb(void *ptr, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(ptr));
  if constexpr (std::is_trivially_constructible_v<T>) {
    return;
  }
  mask.foreach_index_optimized<int64_t>([&](int64_t i) { new (static_cast<T *>(ptr) + i) T; });
}
template<typename T> void default_construct_n_cb(void *ptr, const int64_t n)
{
  default_construct_indices_cb<T>(ptr, IndexMask(n));
}

template<typename T> void value_initialize_cb(void *ptr)
{
  BLI_assert(pointer_can_point_to_instance<T>(ptr));
  new (ptr) T();
}
template<typename T> void value_initialize_indices_cb(void *ptr, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(ptr));
  mask.foreach_index_optimized<int64_t>([&](int64_t i) { new (static_cast<T *>(ptr) + i) T(); });
}
template<typename T> void value_initialize_n_cb(void *ptr, const int64_t n)
{
  value_initialize_indices_cb<T>(ptr, IndexMask(n));
}

template<typename T> void destruct_cb(void *ptr)
{
  BLI_assert(pointer_can_point_to_instance<T>(ptr));
  (static_cast<T *>(ptr))->~T();
}
template<typename T> void destruct_indices_cb(void *ptr, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(ptr));
  if (std::is_trivially_destructible_v<T>) {
    return;
  }
  T *ptr_ = static_cast<T *>(ptr);
  mask.foreach_index_optimized<int64_t>([&](int64_t i) { ptr_[i].~T(); });
}
template<typename T> void destruct_n_cb(void *ptr, const int64_t n)
{
  destruct_indices_cb<T>(ptr, IndexMask(n));
}

template<typename T> void copy_assign_cb(const void *src, void *dst)
{
  BLI_assert(pointer_can_point_to_instance<T>(src));
  BLI_assert(pointer_can_point_to_instance<T>(dst));
  *static_cast<T *>(dst) = *static_cast<const T *>(src);
}
template<typename T> void copy_assign_indices_cb(const void *src, void *dst, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || src != dst);
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(src));
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(dst));
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>([&](int64_t i) { dst_[i] = src_[i]; });
}
template<typename T> void copy_assign_n_cb(const void *src, void *dst, const int64_t n)
{
  copy_assign_indices_cb<T>(src, dst, IndexMask(n));
}
template<typename T>
void copy_assign_compressed_cb(const void *src, void *dst, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || src != dst);
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(src));
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(dst));
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>(
      [&](const int64_t i, const int64_t pos) { dst_[pos] = src_[i]; });
}

template<typename T> void copy_construct_cb(const void *src, void *dst)
{
  BLI_assert(src != dst || std::is_trivially_copy_constructible_v<T>);
  BLI_assert(pointer_can_point_to_instance<T>(src));
  BLI_assert(pointer_can_point_to_instance<T>(dst));
  blender::uninitialized_copy_n(static_cast<const T *>(src), 1, static_cast<T *>(dst));
}
template<typename T>
void copy_construct_indices_cb(const void *src, void *dst, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || src != dst);
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(src));
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(dst));
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>([&](int64_t i) { new (dst_ + i) T(src_[i]); });
}
template<typename T> void copy_construct_n_cb(const void *src, void *dst, const int64_t n)
{
  copy_construct_indices_cb<T>(src, dst, IndexMask(n));
}
template<typename T>
void copy_construct_compressed_cb(const void *src, void *dst, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || src != dst);
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(src));
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(dst));
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>(
      [&](const int64_t i, const int64_t pos) { new (dst_ + pos) T(src_[i]); });
}

template<typename T> void move_assign_cb(void *src, void *dst)
{
  BLI_assert(pointer_can_point_to_instance<T>(src));
  BLI_assert(pointer_can_point_to_instance<T>(dst));
  blender::initialized_move_n(static_cast<T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void move_assign_indices_cb(void *src, void *dst, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || src != dst);
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(src));
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(dst));
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>([&](int64_t i) { dst_[i] = std::move(src_[i]); });
}
template<typename T> void move_assign_n_cb(void *src, void *dst, const int64_t n)
{
  move_assign_indices_cb<T>(src, dst, IndexMask(n));
}

template<typename T> void move_construct_cb(void *src, void *dst)
{
  BLI_assert(src != dst || std::is_trivially_move_constructible_v<T>);
  BLI_assert(pointer_can_point_to_instance<T>(src));
  BLI_assert(pointer_can_point_to_instance<T>(dst));

  blender::uninitialized_move_n(static_cast<T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void move_construct_indices_cb(void *src, void *dst, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || src != dst);
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(src));
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(dst));
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>([&](int64_t i) { new (dst_ + i) T(std::move(src_[i])); });
}
template<typename T> void move_construct_n_cb(void *src, void *dst, const int64_t n)
{
  move_construct_indices_cb<T>(src, dst, IndexMask(n));
}

template<typename T> void relocate_assign_cb(void *src, void *dst)
{
  BLI_assert(src != dst || std::is_trivially_move_constructible_v<T>);
  BLI_assert(pointer_can_point_to_instance<T>(src));
  BLI_assert(pointer_can_point_to_instance<T>(dst));
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  *dst_ = std::move(*src_);
  src_->~T();
}
template<typename T> void relocate_assign_indices_cb(void *src, void *dst, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || src != dst);
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(src));
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(dst));
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>([&](int64_t i) {
    dst_[i] = std::move(src_[i]);
    src_[i].~T();
  });
}
template<typename T> void relocate_assign_n_cb(void *src, void *dst, const int64_t n)
{
  relocate_assign_indices_cb<T>(src, dst, IndexMask(n));
}

template<typename T> void relocate_construct_cb(void *src, void *dst)
{
  BLI_assert(src != dst || std::is_trivially_move_constructible_v<T>);
  BLI_assert(pointer_can_point_to_instance<T>(src));
  BLI_assert(pointer_can_point_to_instance<T>(dst));
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  new (dst_) T(std::move(*src_));
  src_->~T();
}
template<typename T>
void relocate_construct_indices_cb(void *src, void *dst, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || src != dst);
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(src));
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(dst));
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>([&](int64_t i) {
    new (dst_ + i) T(std::move(src_[i]));
    src_[i].~T();
  });
}
template<typename T> void relocate_construct_n_cb(void *src, void *dst, const int64_t n)
{
  relocate_construct_indices_cb<T>(src, dst, IndexMask(n));
}

template<typename T>
void fill_assign_indices_cb(const void *value, void *dst, const IndexMask &mask)
{
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(value));
  BLI_assert(mask.size() == 0 || pointer_can_point_to_instance<T>(dst));
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>([&](int64_t i) { dst_[i] = value_; });
}
template<typename T> void fill_assign_n_cb(const void *value, void *dst, const int64_t n)
{
  fill_assign_indices_cb<T>(value, dst, IndexMask(n));
}

template<typename T> void fill_construct_cb(const void *value, void *dst, int64_t n)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  for (int64_t i = 0; i < n; i++) {
    new (dst_ + i) T(value_);
  }
}
template<typename T>
void fill_construct_indices_cb(const void *value, void *dst, const IndexMask &mask)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index_optimized<int64_t>([&](int64_t i) { new (dst_ + i) T(value_); });
}
template<typename T> void fill_construct_n_cb(const void *value, void *dst, const int64_t n)
{
  fill_construct_indices_cb<T>(value, dst, IndexMask(n));
}

template<typename T> void print_cb(const void *value, std::stringstream &ss)
{
  const T &value_ = *static_cast<const T *>(value);
  ss << value_;
}

template<typename T> bool is_equal_cb(const void *a, const void *b)
{
  BLI_assert(pointer_can_point_to_instance<T>(a));
  BLI_assert(pointer_can_point_to_instance<T>(b));
  const T &a_ = *static_cast<const T *>(a);
  const T &b_ = *static_cast<const T *>(b);
  return a_ == b_;
}

template<typename T> uint64_t hash_cb(const void *value)
{
  BLI_assert(pointer_can_point_to_instance<T>(value));
  const T &value_ = *static_cast<const T *>(value);
  return get_default_hash(value_);
}

}  // namespace blender::cpp_type_util

namespace blender {

template<typename T, CPPTypeFlags Flags>
CPPType::CPPType(TypeTag<T> /*type*/,
                 TypeForValue<CPPTypeFlags, Flags> /*flags*/,
                 const StringRef debug_name)
{
  using namespace cpp_type_util;

  debug_name_ = debug_name;
  this->size = int64_t(sizeof(T));
  this->alignment = int64_t(alignof(T));
  this->is_trivial = std::is_trivial_v<T>;
  this->is_trivially_destructible = std::is_trivially_destructible_v<T>;
  if constexpr (std::is_default_constructible_v<T>) {
    default_construct_ = default_construct_cb<T>;
    default_construct_n_ = default_construct_n_cb<T>;
    default_construct_indices_ = default_construct_indices_cb<T>;
    value_initialize_ = value_initialize_cb<T>;
    value_initialize_n_ = value_initialize_n_cb<T>;
    value_initialize_indices_ = value_initialize_indices_cb<T>;
    if constexpr (bool(Flags & CPPTypeFlags::IdentityDefaultValue)) {
      static const T default_value = T::identity();
      default_value_ = &default_value;
    }
    else {
      static const T default_value = T();
      default_value_ = &default_value;
    }
  }
  if constexpr (std::is_destructible_v<T>) {
    destruct_ = destruct_cb<T>;
    destruct_n_ = destruct_n_cb<T>;
    destruct_indices_ = destruct_indices_cb<T>;
  }
  if constexpr (std::is_copy_assignable_v<T>) {
    copy_assign_ = copy_assign_cb<T>;
    copy_assign_n_ = copy_assign_n_cb<T>;
    copy_assign_indices_ = copy_assign_indices_cb<T>;
    copy_assign_compressed_ = copy_assign_compressed_cb<T>;
  }
  if constexpr (std::is_copy_constructible_v<T>) {
    if constexpr (std::is_trivially_copy_constructible_v<T>) {
      copy_construct_ = copy_assign_;
      copy_construct_n_ = copy_assign_n_;
      copy_construct_indices_ = copy_assign_indices_;
      copy_construct_compressed_ = copy_assign_compressed_;
    }
    else {
      copy_construct_ = copy_construct_cb<T>;
      copy_construct_n_ = copy_construct_n_cb<T>;
      copy_construct_indices_ = copy_construct_indices_cb<T>;
      copy_construct_compressed_ = copy_construct_compressed_cb<T>;
    }
  }
  if constexpr (std::is_move_assignable_v<T>) {
    if constexpr (std::is_trivially_move_assignable_v<T>) {
      /* This casts away the const from the src pointer. This is fine for trivial types as moving
       * them does not change the original value. */
      move_assign_ = reinterpret_cast<decltype(move_assign_)>(copy_assign_);
      move_assign_n_ = reinterpret_cast<decltype(move_assign_n_)>(copy_assign_n_);
      move_assign_indices_ = reinterpret_cast<decltype(move_assign_indices_)>(
          copy_assign_indices_);
    }
    else {
      move_assign_ = move_assign_cb<T>;
      move_assign_n_ = move_assign_n_cb<T>;
      move_assign_indices_ = move_assign_indices_cb<T>;
    }
  }
  if constexpr (std::is_move_constructible_v<T>) {
    if constexpr (std::is_trivially_move_constructible_v<T>) {
      move_construct_ = move_assign_;
      move_construct_n_ = move_assign_n_;
      move_construct_indices_ = move_assign_indices_;
    }
    else {
      move_construct_ = move_construct_cb<T>;
      move_construct_n_ = move_construct_n_cb<T>;
      move_construct_indices_ = move_construct_indices_cb<T>;
    }
  }
  if constexpr (std::is_destructible_v<T>) {
    if constexpr (std::is_trivially_move_assignable_v<T> && std::is_trivially_destructible_v<T>) {
      relocate_assign_ = move_assign_;
      relocate_assign_n_ = move_assign_n_;
      relocate_assign_indices_ = move_assign_indices_;

      relocate_construct_ = move_assign_;
      relocate_construct_n_ = move_assign_n_;
      relocate_construct_indices_ = move_assign_indices_;
    }
    else {
      if constexpr (std::is_move_assignable_v<T>) {
        relocate_assign_ = relocate_assign_cb<T>;
        relocate_assign_n_ = relocate_assign_n_cb<T>;
        relocate_assign_indices_ = relocate_assign_indices_cb<T>;
      }
      if constexpr (std::is_move_constructible_v<T>) {
        relocate_construct_ = relocate_construct_cb<T>;
        relocate_construct_n_ = relocate_construct_n_cb<T>;
        relocate_construct_indices_ = relocate_construct_indices_cb<T>;
      }
    }
  }
  if constexpr (std::is_copy_assignable_v<T>) {
    fill_assign_n_ = fill_assign_n_cb<T>;
    fill_assign_indices_ = fill_assign_indices_cb<T>;
  }
  if constexpr (std::is_copy_constructible_v<T>) {
    if constexpr (std::is_trivially_constructible_v<T>) {
      fill_construct_n_ = fill_assign_n_;
      fill_construct_indices_ = fill_assign_indices_;
    }
    else {
      fill_construct_n_ = fill_construct_n_cb<T>;
      fill_construct_indices_ = fill_construct_indices_cb<T>;
    }
  }
  if constexpr ((bool)(Flags & CPPTypeFlags::Hashable)) {
    hash_ = hash_cb<T>;
  }
  if constexpr ((bool)(Flags & CPPTypeFlags::Printable)) {
    print_ = print_cb<T>;
  }
  if constexpr ((bool)(Flags & CPPTypeFlags::EqualityComparable)) {
    is_equal_ = is_equal_cb<T>;
  }

  alignment_mask_ = uintptr_t(this->alignment) - uintptr_t(1);
  this->has_special_member_functions = (default_construct_ && copy_construct_ && copy_assign_ &&
                                        move_construct_ && move_assign_ && destruct_);
  this->is_default_constructible = default_construct_ != nullptr;
  this->is_copy_constructible = copy_construct_ != nullptr;
  this->is_move_constructible = move_construct_ != nullptr;
  this->is_destructible = destruct_ != nullptr;
  this->is_copy_assignable = copy_assign_ != nullptr;
  this->is_move_assignable = move_assign_ != nullptr;
}

}  // namespace blender

/** Create a new #CPPType that can be accessed through `CPPType::get<T>()`. */
#define BLI_CPP_TYPE_MAKE(TYPE_NAME, FLAGS) \
  template<> const blender::CPPType &blender::CPPType::get_impl<TYPE_NAME>() \
  { \
    static CPPType type{blender::TypeTag<TYPE_NAME>(), \
                        TypeForValue<CPPTypeFlags, FLAGS>(), \
                        STRINGIFY(TYPE_NAME)}; \
    return type; \
  }

/** Register a #CPPType created with #BLI_CPP_TYPE_MAKE. */
#define BLI_CPP_TYPE_REGISTER(TYPE_NAME) blender::CPPType::get<TYPE_NAME>()
