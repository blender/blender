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

/**
 * Different types support different features. Features like copy constructibility can be detected
 * automatically easily. For some features this is harder as of C++17. Those have flags in this
 * enum and need to be determined by the programmer.
 */
enum class CPPTypeFlags {
  None = 0,
  Hashable = 1 << 0,
  Printable = 1 << 1,
  EqualityComparable = 1 << 2,

  BasicType = Hashable | Printable | EqualityComparable,
};
ENUM_OPERATORS(CPPTypeFlags, CPPTypeFlags::EqualityComparable)

namespace blender::fn {

template<typename T, CPPTypeFlags flags>
inline std::unique_ptr<const CPPType> create_cpp_type(StringRef name)
{
  using namespace cpp_type_util;

  CPPTypeMembers m;
  m.name = name;
  m.size = (int64_t)sizeof(T);
  m.alignment = (int64_t)alignof(T);
  m.is_trivially_destructible = std::is_trivially_destructible_v<T>;
  if constexpr (std::is_default_constructible_v<T>) {
    m.default_construct = default_construct_cb<T>;
    m.default_construct_indices = default_construct_indices_cb<T>;
    static T default_value;
    m.default_value = (void *)&default_value;
  }
  if constexpr (std::is_destructible_v<T>) {
    m.destruct = destruct_cb<T>;
    m.destruct_indices = destruct_indices_cb<T>;
  }
  if constexpr (std::is_copy_assignable_v<T>) {
    m.copy_assign = copy_assign_cb<T>;
    m.copy_assign_indices = copy_assign_indices_cb<T>;
  }
  if constexpr (std::is_copy_constructible_v<T>) {
    m.copy_construct = copy_construct_cb<T>;
    m.copy_construct_indices = copy_construct_indices_cb<T>;
  }
  if constexpr (std::is_move_assignable_v<T>) {
    m.move_assign = move_assign_cb<T>;
    m.move_assign_indices = move_assign_indices_cb<T>;
  }
  if constexpr (std::is_move_constructible_v<T>) {
    m.move_construct = move_construct_cb<T>;
    m.move_construct_indices = move_construct_indices_cb<T>;
  }
  if constexpr (std::is_destructible_v<T>) {
    if constexpr (std::is_move_assignable_v<T>) {
      m.relocate_assign = relocate_assign_cb<T>;
      m.relocate_assign_indices = relocate_assign_indices_cb<T>;
    }
    if constexpr (std::is_move_constructible_v<T>) {
      m.relocate_construct = relocate_construct_cb<T>;
      m.relocate_construct_indices = relocate_construct_indices_cb<T>;
    }
  }
  if constexpr (std::is_copy_assignable_v<T>) {
    m.fill_assign_indices = fill_assign_indices_cb<T>;
  }
  if constexpr (std::is_copy_constructible_v<T>) {
    m.fill_construct_indices = fill_construct_indices_cb<T>;
  }
  if constexpr ((bool)(flags & CPPTypeFlags::Hashable)) {
    m.hash = hash_cb<T>;
  }
  if constexpr ((bool)(flags & CPPTypeFlags::Printable)) {
    m.print = print_cb<T>;
  }
  if constexpr ((bool)(flags & CPPTypeFlags::EqualityComparable)) {
    m.is_equal = is_equal_cb<T>;
  }

  const CPPType *type = new CPPType(std::move(m));
  return std::unique_ptr<const CPPType>(type);
}

}  // namespace blender::fn

#define MAKE_CPP_TYPE(IDENTIFIER, TYPE_NAME, FLAGS) \
  template<> const blender::fn::CPPType &blender::fn::CPPType::get<TYPE_NAME>() \
  { \
    static std::unique_ptr<const CPPType> cpp_type = \
        blender::fn::create_cpp_type<TYPE_NAME, FLAGS>(STRINGIFY(IDENTIFIER)); \
    return *cpp_type; \
  } \
  /* Support using `CPPType::get<const T>()`. Otherwise the caller would have to remove const. */ \
  template<> const blender::fn::CPPType &blender::fn::CPPType::get<const TYPE_NAME>() \
  { \
    return blender::fn::CPPType::get<TYPE_NAME>(); \
  }
