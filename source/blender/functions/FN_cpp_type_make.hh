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

#include "FN_cpp_type.hh"

namespace blender::fn::cpp_type_util {

template<typename T> void construct_default_cb(void *ptr)
{
  new (ptr) T;
}
template<typename T> void construct_default_n_cb(void *ptr, int64_t n)
{
  blender::default_construct_n(static_cast<T *>(ptr), n);
}
template<typename T> void construct_default_indices_cb(void *ptr, IndexMask mask)
{
  mask.foreach_index([&](int64_t i) { new (static_cast<T *>(ptr) + i) T; });
}

template<typename T> void destruct_cb(void *ptr)
{
  (static_cast<T *>(ptr))->~T();
}
template<typename T> void destruct_n_cb(void *ptr, int64_t n)
{
  blender::destruct_n(static_cast<T *>(ptr), n);
}
template<typename T> void destruct_indices_cb(void *ptr, IndexMask mask)
{
  T *ptr_ = static_cast<T *>(ptr);
  mask.foreach_index([&](int64_t i) { ptr_[i].~T(); });
}

template<typename T> void copy_to_initialized_cb(const void *src, void *dst)
{
  *static_cast<T *>(dst) = *static_cast<const T *>(src);
}
template<typename T> void copy_to_initialized_n_cb(const void *src, void *dst, int64_t n)
{
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  for (int64_t i = 0; i < n; i++) {
    dst_[i] = src_[i];
  }
}
template<typename T>
void copy_to_initialized_indices_cb(const void *src, void *dst, IndexMask mask)
{
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { dst_[i] = src_[i]; });
}

template<typename T> void copy_to_uninitialized_cb(const void *src, void *dst)
{
  blender::uninitialized_copy_n(static_cast<const T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void copy_to_uninitialized_n_cb(const void *src, void *dst, int64_t n)
{
  blender::uninitialized_copy_n(static_cast<const T *>(src), n, static_cast<T *>(dst));
}
template<typename T>
void copy_to_uninitialized_indices_cb(const void *src, void *dst, IndexMask mask)
{
  const T *src_ = static_cast<const T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { new (dst_ + i) T(src_[i]); });
}

template<typename T> void move_to_initialized_cb(void *src, void *dst)
{
  blender::initialized_move_n(static_cast<T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void move_to_initialized_n_cb(void *src, void *dst, int64_t n)
{
  blender::initialized_move_n(static_cast<T *>(src), n, static_cast<T *>(dst));
}
template<typename T> void move_to_initialized_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { dst_[i] = std::move(src_[i]); });
}

template<typename T> void move_to_uninitialized_cb(void *src, void *dst)
{
  blender::uninitialized_move_n(static_cast<T *>(src), 1, static_cast<T *>(dst));
}
template<typename T> void move_to_uninitialized_n_cb(void *src, void *dst, int64_t n)
{
  blender::uninitialized_move_n(static_cast<T *>(src), n, static_cast<T *>(dst));
}
template<typename T> void move_to_uninitialized_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { new (dst_ + i) T(std::move(src_[i])); });
}

template<typename T> void relocate_to_initialized_cb(void *src, void *dst)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  *dst_ = std::move(*src_);
  src_->~T();
}
template<typename T> void relocate_to_initialized_n_cb(void *src, void *dst, int64_t n)
{
  blender::initialized_relocate_n(static_cast<T *>(src), n, static_cast<T *>(dst));
}
template<typename T> void relocate_to_initialized_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) {
    dst_[i] = std::move(src_[i]);
    src_[i].~T();
  });
}

template<typename T> void relocate_to_uninitialized_cb(void *src, void *dst)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  new (dst_) T(std::move(*src_));
  src_->~T();
}
template<typename T> void relocate_to_uninitialized_n_cb(void *src, void *dst, int64_t n)
{
  blender::uninitialized_relocate_n(static_cast<T *>(src), n, static_cast<T *>(dst));
}
template<typename T>
void relocate_to_uninitialized_indices_cb(void *src, void *dst, IndexMask mask)
{
  T *src_ = static_cast<T *>(src);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) {
    new (dst_ + i) T(std::move(src_[i]));
    src_[i].~T();
  });
}

template<typename T> void fill_initialized_cb(const void *value, void *dst, int64_t n)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  for (int64_t i = 0; i < n; i++) {
    dst_[i] = value_;
  }
}
template<typename T> void fill_initialized_indices_cb(const void *value, void *dst, IndexMask mask)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { dst_[i] = value_; });
}

template<typename T> void fill_uninitialized_cb(const void *value, void *dst, int64_t n)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  for (int64_t i = 0; i < n; i++) {
    new (dst_ + i) T(value_);
  }
}
template<typename T>
void fill_uninitialized_indices_cb(const void *value, void *dst, IndexMask mask)
{
  const T &value_ = *static_cast<const T *>(value);
  T *dst_ = static_cast<T *>(dst);

  mask.foreach_index([&](int64_t i) { new (dst_ + i) T(value_); });
}

template<typename T> void debug_print_cb(const void *value, std::stringstream &ss)
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

template<typename T>
inline std::unique_ptr<const CPPType> create_cpp_type(StringRef name, const T &default_value)
{
  using namespace cpp_type_util;
  const CPPType *type = new CPPType(name,
                                    sizeof(T),
                                    alignof(T),
                                    std::is_trivially_destructible_v<T>,
                                    construct_default_cb<T>,
                                    construct_default_n_cb<T>,
                                    construct_default_indices_cb<T>,
                                    destruct_cb<T>,
                                    destruct_n_cb<T>,
                                    destruct_indices_cb<T>,
                                    copy_to_initialized_cb<T>,
                                    copy_to_initialized_n_cb<T>,
                                    copy_to_initialized_indices_cb<T>,
                                    copy_to_uninitialized_cb<T>,
                                    copy_to_uninitialized_n_cb<T>,
                                    copy_to_uninitialized_indices_cb<T>,
                                    move_to_initialized_cb<T>,
                                    move_to_initialized_n_cb<T>,
                                    move_to_initialized_indices_cb<T>,
                                    move_to_uninitialized_cb<T>,
                                    move_to_uninitialized_n_cb<T>,
                                    move_to_uninitialized_indices_cb<T>,
                                    relocate_to_initialized_cb<T>,
                                    relocate_to_initialized_n_cb<T>,
                                    relocate_to_initialized_indices_cb<T>,
                                    relocate_to_uninitialized_cb<T>,
                                    relocate_to_uninitialized_n_cb<T>,
                                    relocate_to_uninitialized_indices_cb<T>,
                                    fill_initialized_cb<T>,
                                    fill_initialized_indices_cb<T>,
                                    fill_uninitialized_cb<T>,
                                    fill_uninitialized_indices_cb<T>,
                                    debug_print_cb<T>,
                                    is_equal_cb<T>,
                                    hash_cb<T>,
                                    static_cast<const void *>(&default_value));
  return std::unique_ptr<const CPPType>(type);
}

}  // namespace blender::fn

#define MAKE_CPP_TYPE(IDENTIFIER, TYPE_NAME) \
  template<> const blender::fn::CPPType &blender::fn::CPPType::get<TYPE_NAME>() \
  { \
    static TYPE_NAME default_value; \
    static std::unique_ptr<const CPPType> cpp_type = blender::fn::create_cpp_type<TYPE_NAME>( \
        STRINGIFY(IDENTIFIER), default_value); \
    return *cpp_type; \
  } \
  /* Support using `CPPType::get<const T>()`. Otherwise the caller would have to remove const. */ \
  template<> const blender::fn::CPPType &blender::fn::CPPType::get<const TYPE_NAME>() \
  { \
    return blender::fn::CPPType::get<TYPE_NAME>(); \
  }
