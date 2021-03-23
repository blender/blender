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
 * A`GVectorArray` is a container for a fixed amount of dynamically growing vectors with a generic
 * data type. Its main use case is to store many small vectors with few separate allocations. Using
 * this structure is generally more efficient than allocating each vector separately.
 */

#include "BLI_array.hh"
#include "BLI_linear_allocator.hh"

#include "FN_generic_virtual_vector_array.hh"

namespace blender::fn {

/* An array of vectors containing elements of a generic type. */
class GVectorArray : NonCopyable, NonMovable {
 private:
  struct Item {
    void *start = nullptr;
    int64_t length = 0;
    int64_t capacity = 0;
  };

  /* Use a linear allocator to pack many small vectors together. Currently, memory from reallocated
   * vectors is not reused. This can be improved in the future. */
  LinearAllocator<> allocator_;
  /* The data type of individual elements. */
  const CPPType &type_;
  /* The size of an individual element. This is inlined from `type_.size()` for easier access. */
  const int64_t element_size_;
  /* The individual vectors. */
  Array<Item> items_;

 public:
  GVectorArray() = delete;

  GVectorArray(const CPPType &type, int64_t array_size);

  ~GVectorArray();

  int64_t size() const
  {
    return items_.size();
  }

  bool is_empty() const
  {
    return items_.is_empty();
  }

  const CPPType &type() const
  {
    return type_;
  }

  void append(int64_t index, const void *value);

  /* Add multiple elements to a single vector. */
  void extend(int64_t index, const GVArray &values);
  void extend(int64_t index, GSpan values);

  /* Add multiple elements to multiple vectors. */
  void extend(IndexMask mask, const GVVectorArray &values);
  void extend(IndexMask mask, const GVectorArray &values);

  GMutableSpan operator[](int64_t index);
  GSpan operator[](int64_t index) const;

 private:
  void realloc_to_at_least(Item &item, int64_t min_capacity);
};

/* A non-owning typed mutable reference to an `GVectorArray`. It simplifies access when the type of
 * the data is known at compile time. */
template<typename T> class GVectorArray_TypedMutableRef {
 private:
  GVectorArray *vector_array_;

 public:
  GVectorArray_TypedMutableRef(GVectorArray &vector_array) : vector_array_(&vector_array)
  {
    BLI_assert(vector_array_->type().is<T>());
  }

  int64_t size() const
  {
    return vector_array_->size();
  }

  bool is_empty() const
  {
    return vector_array_->is_empty();
  }

  void append(const int64_t index, const T &value)
  {
    vector_array_->append(index, &value);
  }

  void extend(const int64_t index, const Span<T> values)
  {
    vector_array_->extend(index, values);
  }

  void extend(const int64_t index, const VArray<T> &values)
  {
    GVArrayForVArray<T> array{values};
    this->extend(index, array);
  }

  MutableSpan<T> operator[](const int64_t index)
  {
    return (*vector_array_)[index].typed<T>();
  }
};

/* A generic virtual vector array implementation for a `GVectorArray`. */
class GVVectorArrayForGVectorArray : public GVVectorArray {
 private:
  const GVectorArray &vector_array_;

 public:
  GVVectorArrayForGVectorArray(const GVectorArray &vector_array)
      : GVVectorArray(vector_array.type(), vector_array.size()), vector_array_(vector_array)
  {
  }

 protected:
  int64_t get_vector_size_impl(const int64_t index) const override
  {
    return vector_array_[index].size();
  }

  void get_vector_element_impl(const int64_t index,
                               const int64_t index_in_vector,
                               void *r_value) const override
  {
    type_->copy_to_initialized(vector_array_[index][index_in_vector], r_value);
  }
};

}  // namespace blender::fn
