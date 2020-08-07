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
 *
 * This file implements multiple variants of a span for different use cases. There are two
 * requirements of the function system that require span implementations other from
 * blender::Span<T>.
 * 1. The function system works with a run-time type system (see `CPPType`). Therefore, it has to
 *   deal with types in a generic way. The type of a Span<T> has to be known at compile time.
 * 2. Span<T> expects an underlying memory buffer that is as large as the span. However, sometimes
 *   we can save some memory and processing when we know that all elements are the same.
 *
 * The first requirement is solved with generic spans, which use the "G" prefix. Those
 * store a CPPType instance to keep track of the type that is currently stored.
 *
 * The second requirement is solved with virtual spans. A virtual span behaves like a normal span,
 * but it might not be backed up by an actual array. Elements in a virtual span are always
 * immutable.
 *
 * Different use cases require different combinations of these properties and therefore use
 * different data structures.
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

  void *data()
  {
    return data_;
  }

  void *operator[](int64_t index)
  {
    BLI_assert(index < size_);
    return POINTER_OFFSET(data_, type_->size() * index);
  }

  template<typename T> MutableSpan<T> typed()
  {
    BLI_assert(type_->is<T>());
    return MutableSpan<T>(static_cast<T *>(data_), size_);
  }
};

enum class VSpanCategory {
  Single,
  FullArray,
  FullPointerArray,
};

template<typename T> struct VSpanBase {
 protected:
  int64_t virtual_size_;
  VSpanCategory category_;
  union {
    struct {
      const T *data;
    } single;
    struct {
      const T *data;
    } full_array;
    struct {
      const T *const *data;
    } full_pointer_array;
  } data_;

 public:
  bool is_single_element() const
  {
    switch (category_) {
      case VSpanCategory::Single:
        return true;
      case VSpanCategory::FullArray:
        return virtual_size_ == 1;
      case VSpanCategory::FullPointerArray:
        return virtual_size_ == 1;
    }
    BLI_assert(false);
    return false;
  }

  bool is_full_array() const
  {
    switch (category_) {
      case VSpanCategory::Single:
        return virtual_size_ == 1;
      case VSpanCategory::FullArray:
        return true;
      case VSpanCategory::FullPointerArray:
        return virtual_size_ <= 1;
    }
    BLI_assert(false);
    return false;
  }

  bool is_empty() const
  {
    return this->virtual_size_ == 0;
  }

  int64_t size() const
  {
    return this->virtual_size_;
  }
};

BLI_STATIC_ASSERT((sizeof(VSpanBase<void>) == sizeof(VSpanBase<AlignedBuffer<64, 64>>)),
                  "should not depend on the size of the type");

/**
 * A virtual span. It behaves like a blender::Span<T>, but might not be backed up by an actual
 * array.
 */
template<typename T> class VSpan : public VSpanBase<T> {
  friend class GVSpan;

  VSpan(const VSpanBase<void> &values)
  {
    memcpy(this, &values, sizeof(VSpanBase<void>));
  }

 public:
  VSpan()
  {
    this->virtual_size_ = 0;
    this->category_ = VSpanCategory::FullArray;
    this->data_.full_array.data = nullptr;
  }

  VSpan(Span<T> values)
  {
    this->virtual_size_ = values.size();
    this->category_ = VSpanCategory::FullArray;
    this->data_.full_array.data = values.begin();
  }

  VSpan(MutableSpan<T> values) : VSpan(Span<T>(values))
  {
  }

  VSpan(Span<const T *> values)
  {
    this->virtual_size_ = values.size();
    this->category_ = VSpanCategory::FullPointerArray;
    this->data_.full_pointer_array.data = values.begin();
  }

  static VSpan FromSingle(const T *value, int64_t virtual_size)
  {
    VSpan ref;
    ref.virtual_size_ = virtual_size;
    ref.category_ = VSpanCategory::Single;
    ref.data_.single.data = value;
    return ref;
  }

  const T &operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->virtual_size_);
    switch (this->category_) {
      case VSpanCategory::Single:
        return *this->data_.single.data;
      case VSpanCategory::FullArray:
        return this->data_.full_array.data[index];
      case VSpanCategory::FullPointerArray:
        return *this->data_.full_pointer_array.data[index];
    }
    BLI_assert(false);
    return *this->data_.single.data;
  }

  const T &as_single_element() const
  {
    BLI_assert(this->is_single_element());
    return (*this)[0];
  }

  Span<T> as_full_array() const
  {
    BLI_assert(this->is_full_array());
    if (this->virtual_size_ == 0) {
      return Span<T>();
    }
    const T *data = &(*this)[0];
    return Span<T>(data, this->virtual_size_);
  }
};

/**
 * A generic virtual span. It behaves like a blender::Span<T>, but the type is only known at
 * run-time and it might not be backed up by an actual array.
 */
class GVSpan : public VSpanBase<void> {
 private:
  const CPPType *type_;

  GVSpan() = default;

 public:
  GVSpan(const CPPType &type)
  {
    this->type_ = &type;
    this->virtual_size_ = 0;
    this->category_ = VSpanCategory::FullArray;
    this->data_.full_array.data = nullptr;
  }

  GVSpan(GSpan values)
  {
    this->type_ = &values.type();
    this->virtual_size_ = values.size();
    this->category_ = VSpanCategory::FullArray;
    this->data_.full_array.data = values.data();
  }

  GVSpan(GMutableSpan values) : GVSpan(GSpan(values))
  {
  }

  template<typename T> GVSpan(const VSpanBase<T> &values)
  {
    this->type_ = &CPPType::get<T>();
    memcpy(this, &values, sizeof(VSpanBase<void>));
  }

  template<typename T> GVSpan(Span<T> values) : GVSpan(GSpan(values))
  {
  }

  template<typename T> GVSpan(MutableSpan<T> values) : GVSpan(GSpan(values))
  {
  }

  static GVSpan FromSingle(const CPPType &type, const void *value, int64_t virtual_size)
  {
    GVSpan ref;
    ref.type_ = &type;
    ref.virtual_size_ = virtual_size;
    ref.category_ = VSpanCategory::Single;
    ref.data_.single.data = value;
    return ref;
  }

  static GVSpan FromSingleWithMaxSize(const CPPType &type, const void *value)
  {
    return GVSpan::FromSingle(type, value, INT64_MAX);
  }

  static GVSpan FromDefault(const CPPType &type)
  {
    return GVSpan::FromSingleWithMaxSize(type, type.default_value());
  }

  static GVSpan FromFullPointerArray(const CPPType &type, const void *const *values, int64_t size)
  {
    GVSpan ref;
    ref.type_ = &type;
    ref.virtual_size_ = size;
    ref.category_ = VSpanCategory::FullPointerArray;
    ref.data_.full_pointer_array.data = values;
    return ref;
  }

  const CPPType &type() const
  {
    return *this->type_;
  }

  const void *operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->virtual_size_);
    switch (this->category_) {
      case VSpanCategory::Single:
        return this->data_.single.data;
      case VSpanCategory::FullArray:
        return POINTER_OFFSET(this->data_.full_array.data, index * type_->size());
      case VSpanCategory::FullPointerArray:
        return this->data_.full_pointer_array.data[index];
    }
    BLI_assert(false);
    return this->data_.single.data;
  }

  template<typename T> VSpan<T> typed() const
  {
    BLI_assert(type_->is<T>());
    return VSpan<T>(*this);
  }

  const void *as_single_element() const
  {
    BLI_assert(this->is_single_element());
    return (*this)[0];
  }

  GSpan as_full_array() const
  {
    BLI_assert(this->is_full_array());
    if (this->virtual_size_ == 0) {
      return GSpan(*this->type_);
    }
    const void *data = (*this)[0];
    return GSpan(*this->type_, data, this->virtual_size_);
  }

  void materialize_to_uninitialized(void *dst) const
  {
    this->materialize_to_uninitialized(IndexRange(virtual_size_), dst);
  }

  void materialize_to_uninitialized(IndexMask mask, void *dst) const
  {
    BLI_assert(this->size() >= mask.min_array_size());

    int64_t element_size = type_->size();
    for (int64_t i : mask) {
      type_->copy_to_uninitialized((*this)[i], POINTER_OFFSET(dst, element_size * i));
    }
  }
};

}  // namespace blender::fn
