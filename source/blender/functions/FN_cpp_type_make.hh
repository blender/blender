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
 * \ingroup fn
 */

#include "BLI_utildefines.h"
#include "FN_cpp_type.hh"

namespace blender::fn::cpp_type_util {

template<typename T> void default_construct_cb(void *ptr)
{
  new (ptr) T;
}
template<typename T> void default_construct_indices_cb(void *ptr, IndexMask mask)
{
  mask.foreach_index([&](int64_t i) { new (static_cast<T *>(ptr) + i) T; });
}

template<typename T> void destruct_cb(void *ptr)
{
  (static_cast<T *>(ptr))->~T();
}
template<typename T> void destruct_indices_cb(void *ptr, IndexMask mask)
{
  T *ptr_ = static_cast<T *>(ptr);
  mask.foreach_index([&](int64_t i) { ptr_[i].~T(); });
}

template<typename T> void copy_assign_cb(const void *src, void *dst)
{
  *static_cast<T *>(dst) = *static_cast<const T *>(src);
}
template<typename T> void copy_assign_indices_cb(const void *src, void *dst, IndexMask mask)
{
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { dst_[i] = src_[i]; });
}

template<typename T> void copy_construct_cb(const void *src, void *dst)
{
  blender::uninitialized_copy_n(static_cast<const T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void copy_construct_indices_cb(const void *src, void *dst, IndexMask mask)
{
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { new (dst_ + i) T(src_[i]); });
}

template<typename T> void move_assign_cb(void *src, void *dst)
{
  blender::initialized_move_n(static_cast<T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void move_assign_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { dst_[i] = std::move(src_[i]); });
}

template<typename T> void move_construct_cb(void *src, void *dst)
{
  blender::uninitialized_move_n(static_cast<T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void move_construct_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { new (dst_ + i) T(std::move(src_[i])); });
}

template<typename T> void relocate_assign_cb(void *src, void *dst)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  *dst_ = std::move(*src_);
  src_->~T();
}
template<typename T> void relocate_assign_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) {
    dst_[i] = std::move(src_[i]);
    src_[i].~T();
  });
}

template<typename T> void relocate_construct_cb(void *src, void *dst)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  new (dst_) T(std::move(*src_));
  src_->~T();
}
template<typename T> void relocate_construct_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) {
    new (dst_ + i) T(std::move(src_[i]));
    src_[i].~T();
  });
}

template<typename T> void fill_assign_cb(const void *value, void *dst, int64_t n)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  for (int64_t i = 0; i < n; i++) {
    dst_[i] = value_;
  }
}
template<typename T> void fill_assign_indices_cb(const void *value, void *dst, IndexMask mask)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { dst_[i] = value_; });
}

template<typename T> void fill_construct_cb(const void *value, void *dst, int64_t n)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  for (int64_t i = 0; i < n; i++) {
    new (dst_ + i) T(value_);
  }
}
template<typename T> void fill_construct_indices_cb(const void *value, void *dst, IndexMask mask)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { new (dst_ + i) T(value_); });
}

template<typename T> void print_cb(const void *value, std::stringstream &ss)
{
  const T &value_ = *static_cast<const T *>(value);
  ss << value_;
}

template<typename T> bool is_equal_cb(const void *a, const void *b)
{
  const T &a_ = *static_cast<const T *>(a);
  const T &b_ = *static_cast<const T *>(b);
  return a_ == b_;
}

template<typename T> uint64_t hash_cb(const void *value)
{
  const T &value_ = *static_cast<const T *>(value);
  return get_default_hash(value_);
}

}  // namespace blender::fn::cpp_type_util

namespace blender::fn {

template<typename T, CPPTypeFlags Flags>
CPPType::CPPType(CPPTypeParam<T, Flags> /* unused */, StringRef debug_name)
{
  using namespace cpp_type_util;

  debug_name_ = debug_name;
  size_ = (int64_t)sizeof(T);
  alignment_ = (int64_t)alignof(T);
  is_trivial_ = std::is_trivial_v<T>;
  is_trivially_destructible_ = std::is_trivially_destructible_v<T>;
  if constexpr (std::is_default_constructible_v<T>) {
    default_construct_ = default_construct_cb<T>;
    default_construct_indices_ = default_construct_indices_cb<T>;
    static T default_value;
    default_value_ = (void *)&default_value;
  }
  if constexpr (std::is_destructible_v<T>) {
    destruct_ = destruct_cb<T>;
    destruct_indices_ = destruct_indices_cb<T>;
  }
  if constexpr (std::is_copy_assignable_v<T>) {
    copy_assign_ = copy_assign_cb<T>;
    copy_assign_indices_ = copy_assign_indices_cb<T>;
  }
  if constexpr (std::is_copy_constructible_v<T>) {
    copy_construct_ = copy_construct_cb<T>;
    copy_construct_indices_ = copy_construct_indices_cb<T>;
  }
  if constexpr (std::is_move_assignable_v<T>) {
    move_assign_ = move_assign_cb<T>;
    move_assign_indices_ = move_assign_indices_cb<T>;
  }
  if constexpr (std::is_move_constructible_v<T>) {
    move_construct_ = move_construct_cb<T>;
    move_construct_indices_ = move_construct_indices_cb<T>;
  }
  if constexpr (std::is_destructible_v<T>) {
    if constexpr (std::is_move_assignable_v<T>) {
      relocate_assign_ = relocate_assign_cb<T>;
      relocate_assign_indices_ = relocate_assign_indices_cb<T>;
    }
    if constexpr (std::is_move_constructible_v<T>) {
      relocate_construct_ = relocate_construct_cb<T>;
      relocate_construct_indices_ = relocate_construct_indices_cb<T>;
    }
  }
  if constexpr (std::is_copy_assignable_v<T>) {
    fill_assign_indices_ = fill_assign_indices_cb<T>;
  }
  if constexpr (std::is_copy_constructible_v<T>) {
    fill_construct_indices_ = fill_construct_indices_cb<T>;
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

  alignment_mask_ = (uintptr_t)alignment_ - (uintptr_t)1;
  has_special_member_functions_ = (default_construct_ && copy_construct_ && copy_assign_ &&
                                   move_construct_ && move_assign_ && destruct_);
}

}  // namespace blender::fn

#define MAKE_CPP_TYPE(IDENTIFIER, TYPE_NAME, FLAGS) \
  template<> const blender::fn::CPPType &blender::fn::CPPType::get_impl<TYPE_NAME>() \
  { \
    static CPPType cpp_type{blender::fn::CPPTypeParam<TYPE_NAME, FLAGS>(), \
                            STRINGIFY(IDENTIFIER)}; \
    return cpp_type; \
  }
