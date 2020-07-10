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

#ifndef __FN_GENERIC_VECTOR_ARRAY_HH__
#define __FN_GENERIC_VECTOR_ARRAY_HH__

/** \file
 * \ingroup fn
 *
 * A `GVectorArray` is a container for a fixed amount of dynamically growing arrays with a generic
 * type. Its main use case is to store many small vectors with few separate allocations. Using this
 * structure is generally more efficient than allocating each small vector separately.
 *
 * `GVectorArrayRef<T>` is a typed reference to a GVectorArray and makes it easier and safer to
 * work with the class when the type is known at compile time.
 */

#include "FN_array_spans.hh"
#include "FN_cpp_type.hh"

#include "BLI_array.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_utility_mixins.hh"

namespace blender::fn {

template<typename T> class GVectorArrayRef;

class GVectorArray : NonCopyable, NonMovable {
 private:
  const CPPType &type_;
  uint element_size_;
  Array<void *, 1> starts_;
  Array<uint, 1> lengths_;
  Array<uint, 1> capacities_;
  LinearAllocator<> allocator_;

  template<typename T> friend class GVectorArrayRef;

 public:
  GVectorArray() = delete;

  GVectorArray(const CPPType &type, uint array_size)
      : type_(type),
        element_size_(type.size()),
        starts_(array_size),
        lengths_(array_size),
        capacities_(array_size)
  {
    starts_.fill(nullptr);
    lengths_.fill(0);
    capacities_.fill(0);
  }

  ~GVectorArray()
  {
    if (type_.is_trivially_destructible()) {
      return;
    }

    for (uint i : starts_.index_range()) {
      type_.destruct_n(starts_[i], lengths_[i]);
    }
  }

  operator GVArraySpan() const
  {
    return GVArraySpan(type_, starts_, lengths_);
  }

  bool is_empty() const
  {
    return starts_.size() == 0;
  }

  uint size() const
  {
    return starts_.size();
  }

  const CPPType &type() const
  {
    return type_;
  }

  Span<const void *> starts() const
  {
    return starts_;
  }

  Span<uint> lengths() const
  {
    return lengths_;
  }

  void append(uint index, const void *src)
  {
    uint old_length = lengths_[index];
    if (old_length == capacities_[index]) {
      this->grow_at_least_one(index);
    }

    void *dst = POINTER_OFFSET(starts_[index], element_size_ * old_length);
    type_.copy_to_uninitialized(src, dst);
    lengths_[index]++;
  }

  void extend(uint index, GVSpan span)
  {
    BLI_assert(type_ == span.type());
    for (uint i = 0; i < span.size(); i++) {
      this->append(index, span[i]);
    }
  }

  void extend(IndexMask mask, GVArraySpan array_span)
  {
    BLI_assert(type_ == array_span.type());
    BLI_assert(mask.min_array_size() <= array_span.size());
    for (uint i : mask) {
      this->extend(i, array_span[i]);
    }
  }

  GMutableSpan operator[](uint index)
  {
    BLI_assert(index < starts_.size());
    return GMutableSpan(type_, starts_[index], lengths_[index]);
  }
  template<typename T> GVectorArrayRef<T> typed()
  {
    return GVectorArrayRef<T>(*this);
  }

 private:
  void grow_at_least_one(uint index)
  {
    BLI_assert(lengths_[index] == capacities_[index]);
    uint new_capacity = lengths_[index] * 2 + 1;

    void *new_buffer = allocator_.allocate(element_size_ * new_capacity, type_.alignment());
    type_.relocate_to_uninitialized_n(starts_[index], new_buffer, lengths_[index]);

    starts_[index] = new_buffer;
    capacities_[index] = new_capacity;
  }
};

template<typename T> class GVectorArrayRef {
 private:
  GVectorArray *vector_array_;

 public:
  GVectorArrayRef(GVectorArray &vector_array) : vector_array_(&vector_array)
  {
    BLI_assert(vector_array.type_.is<T>());
  }

  void append(uint index, const T &value)
  {
    vector_array_->append(index, &value);
  }

  void extend(uint index, Span<T> values)
  {
    vector_array_->extend(index, values);
  }

  void extend(uint index, VSpan<T> values)
  {
    vector_array_->extend(index, GVSpan(values));
  }

  MutableSpan<T> operator[](uint index)
  {
    BLI_assert(index < vector_array_->starts_.size());
    return MutableSpan<T>((T *)vector_array_->starts_[index], vector_array_->lengths_[index]);
  }

  uint size() const
  {
    return vector_array_->size();
  }

  bool is_empty() const
  {
    return vector_array_->is_empty();
  }
};

}  // namespace blender::fn

#endif /* __FN_GENERIC_VECTOR_ARRAY_HH__ */
