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

#include "BLI_span.hh"

#include "FN_cpp_type.hh"

namespace blender::fn {

/**
 * A generic span. It behaves just like a blender::Span<T>, but the type is only known at run-time.
 */
class GSpan {
 private:
  const CPPType *type_;
  const void *data_;
  int64_t size_;

 public:
  GSpan(const CPPType &type, const void *buffer, int64_t size)
      : type_(&type), data_(buffer), size_(size)
  {
    BLI_assert(size >= 0);
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
  }

  GSpan(const CPPType &type) : GSpan(type, nullptr, 0)
  {
  }

  template<typename T>
  GSpan(Span<T> array)
      : GSpan(CPPType::get<T>(), static_cast<const void *>(array.data()), array.size())
  {
  }

  const CPPType &type() const
  {
    return *type_;
  }

  bool is_empty() const
  {
    return size_ == 0;
  }

  int64_t size() const
  {
    return size_;
  }

  const void *data() const
  {
    return data_;
  }

  const void *operator[](int64_t index) const
  {
    BLI_assert(index < size_);
    return POINTER_OFFSET(data_, type_->size() * index);
  }

  template<typename T> Span<T> typed() const
  {
    BLI_assert(type_->is<T>());
    return Span<T>(static_cast<const T *>(data_), size_);
  }
};

/**
 * A generic mutable span. It behaves just like a blender::MutableSpan<T>, but the type is only
 * known at run-time.
 */
class GMutableSpan {
 private:
  const CPPType *type_;
  void *data_;
  int64_t size_;

 public:
  GMutableSpan(const CPPType &type, void *buffer, int64_t size)
      : type_(&type), data_(buffer), size_(size)
  {
    BLI_assert(size >= 0);
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type.pointer_has_valid_alignment(buffer));
  }

  GMutableSpan(const CPPType &type) : GMutableSpan(type, nullptr, 0)
  {
  }

  template<typename T>
  GMutableSpan(MutableSpan<T> array)
      : GMutableSpan(CPPType::get<T>(), static_cast<void *>(array.begin()), array.size())
  {
  }

  operator GSpan() const
  {
    return GSpan(*type_, data_, size_);
  }

  const CPPType &type() const
  {
    return *type_;
  }

  bool is_empty() const
  {
    return size_ == 0;
  }

  int64_t size() const
  {
    return size_;
  }

  void *data() const
  {
    return data_;
  }

  void *operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return POINTER_OFFSET(data_, type_->size() * index);
  }

  template<typename T> MutableSpan<T> typed() const
  {
    BLI_assert(type_->is<T>());
    return MutableSpan<T>(static_cast<T *>(data_), size_);
  }
};

}  // namespace blender::fn
