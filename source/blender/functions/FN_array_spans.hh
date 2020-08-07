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
 * An ArraySpan is a span where every element contains an array (instead of a single element as is
 * the case in a normal span). It's main use case is to reference many small arrays.
 */

#include "FN_spans.hh"

namespace blender::fn {

/**
 * Depending on the use case, the referenced data might have a different structure. More
 * categories can be added when necessary.
 */
enum class VArraySpanCategory {
  SingleArray,
  StartsAndSizes,
};

template<typename T> class VArraySpanBase {
 protected:
  int64_t virtual_size_;
  VArraySpanCategory category_;

  union {
    struct {
      const T *start;
      int64_t size;
    } single_array;
    struct {
      const T *const *starts;
      const int64_t *sizes;
    } starts_and_sizes;
  } data_;

 public:
  bool is_single_array() const
  {
    switch (category_) {
      case VArraySpanCategory::SingleArray:
        return true;
      case VArraySpanCategory::StartsAndSizes:
        return virtual_size_ == 1;
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

/**
 * A virtual array span. Every element of this span contains a virtual span. So it behaves like
 * a blender::Span, but might not be backed up by an actual array.
 */
template<typename T> class VArraySpan : public VArraySpanBase<T> {
 private:
  friend class GVArraySpan;

  VArraySpan(const VArraySpanBase<void> &other)
  {
    memcpy(this, &other, sizeof(VArraySpanBase<void>));
  }

 public:
  VArraySpan()
  {
    this->virtual_size_ = 0;
    this->category_ = VArraySpanCategory::StartsAndSizes;
    this->data_.starts_and_sizes.starts = nullptr;
    this->data_.starts_and_sizes.sizes = nullptr;
  }

  VArraySpan(Span<T> span, int64_t virtual_size)
  {
    BLI_assert(virtual_size >= 0);
    this->virtual_size_ = virtual_size;
    this->category_ = VArraySpanCategory::SingleArray;
    this->data_.single_array.start = span.data();
    this->data_.single_array.size = span.size();
  }

  VArraySpan(Span<const T *> starts, Span<int64_t> sizes)
  {
    BLI_assert(starts.size() == sizes.size());
    this->virtual_size_ = starts.size();
    this->category_ = VArraySpanCategory::StartsAndSizes;
    this->data_.starts_and_sizes.starts = starts.begin();
    this->data_.starts_and_sizes.sizes = sizes.begin();
  }

  VSpan<T> operator[](int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->virtual_size_);
    switch (this->category_) {
      case VArraySpanCategory::SingleArray:
        return VSpan<T>(Span<T>(this->data_.single_array.start, this->data_.single_array.size));
      case VArraySpanCategory::StartsAndSizes:
        return VSpan<T>(Span<T>(this->data_.starts_and_sizes.starts[index],
                                this->data_.starts_and_sizes.sizes[index]));
    }
    BLI_assert(false);
    return {};
  }
};

/**
 * A generic virtual array span. It's just like a VArraySpan, but the type is only known at
 * run-time.
 */
class GVArraySpan : public VArraySpanBase<void> {
 private:
  const CPPType *type_;

  GVArraySpan() = default;

 public:
  GVArraySpan(const CPPType &type)
  {
    this->type_ = &type;
    this->virtual_size_ = 0;
    this->category_ = VArraySpanCategory::StartsAndSizes;
    this->data_.starts_and_sizes.starts = nullptr;
    this->data_.starts_and_sizes.sizes = nullptr;
  }

  GVArraySpan(GSpan array, int64_t virtual_size)
  {
    this->type_ = &array.type();
    this->virtual_size_ = virtual_size;
    this->category_ = VArraySpanCategory::SingleArray;
    this->data_.single_array.start = array.data();
    this->data_.single_array.size = array.size();
  }

  GVArraySpan(const CPPType &type, Span<const void *> starts, Span<int64_t> sizes)
  {
    BLI_assert(starts.size() == sizes.size());
    this->type_ = &type;
    this->virtual_size_ = starts.size();
    this->category_ = VArraySpanCategory::StartsAndSizes;
    this->data_.starts_and_sizes.starts = (void **)starts.begin();
    this->data_.starts_and_sizes.sizes = sizes.begin();
  }

  template<typename T> GVArraySpan(VArraySpan<T> other)
  {
    this->type_ = &CPPType::get<T>();
    memcpy(this, &other, sizeof(VArraySpanBase<void>));
  }

  const CPPType &type() const
  {
    return *this->type_;
  }

  template<typename T> VArraySpan<T> typed() const
  {
    BLI_assert(type_->is<T>());
    return VArraySpan<T>(*this);
  }

  GVSpan operator[](int64_t index) const
  {
    BLI_assert(index < virtual_size_);
    switch (category_) {
      case VArraySpanCategory::SingleArray:
        return GVSpan(GSpan(*type_, data_.single_array.start, data_.single_array.size));
      case VArraySpanCategory::StartsAndSizes:
        return GVSpan(GSpan(
            *type_, data_.starts_and_sizes.starts[index], data_.starts_and_sizes.sizes[index]));
    }
    BLI_assert(false);
    return GVSpan(*type_);
  }
};

}  // namespace blender::fn
