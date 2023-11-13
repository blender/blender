/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * A generic virtual vector array is essentially the same as a virtual vector array, but its data
 * type is only known at runtime.
 */

#include "BLI_generic_virtual_array.hh"
#include "BLI_virtual_vector_array.hh"

namespace blender {

/* A generically typed version of `VVectorArray`. */
class GVVectorArray {
 protected:
  const CPPType *type_;
  int64_t size_;

 public:
  GVVectorArray(const CPPType &type, const int64_t size) : type_(&type), size_(size) {}

  virtual ~GVVectorArray() = default;

  /* Returns the number of vectors in the vector array. */
  int64_t size() const
  {
    return size_;
  }

  /* Returns true when there is no vector in the vector array. */
  bool is_empty() const
  {
    return size_ == 0;
  }

  const CPPType &type() const
  {
    return *type_;
  }

  /* Returns the size of the vector at the given index. */
  int64_t get_vector_size(const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return this->get_vector_size_impl(index);
  }

  /* Copies an element from one of the vectors into `r_value`, which is expected to point to
   * initialized memory. */
  void get_vector_element(const int64_t index, const int64_t index_in_vector, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    BLI_assert(index_in_vector >= 0);
    BLI_assert(index_in_vector < this->get_vector_size(index));
    this->get_vector_element_impl(index, index_in_vector, r_value);
  }

  /* Returns true when the same vector is used at every index. */
  bool is_single_vector() const
  {
    if (size_ == 1) {
      return true;
    }
    return this->is_single_vector_impl();
  }

 protected:
  virtual int64_t get_vector_size_impl(int64_t index) const = 0;

  virtual void get_vector_element_impl(int64_t index,
                                       int64_t index_in_vector,
                                       void *r_value) const = 0;

  virtual bool is_single_vector_impl() const
  {
    return false;
  }
};

class GVArray_For_GVVectorArrayIndex : public GVArrayImpl {
 private:
  const GVVectorArray &vector_array_;
  const int64_t index_;

 public:
  GVArray_For_GVVectorArrayIndex(const GVVectorArray &vector_array, const int64_t index)
      : GVArrayImpl(vector_array.type(), vector_array.get_vector_size(index)),
        vector_array_(vector_array),
        index_(index)
  {
  }

 protected:
  void get(int64_t index_in_vector, void *r_value) const override;
  void get_to_uninitialized(int64_t index_in_vector, void *r_value) const override;
};

class GVVectorArray_For_SingleGVArray : public GVVectorArray {
 private:
  GVArray varray_;

 public:
  GVVectorArray_For_SingleGVArray(GVArray varray, const int64_t size)
      : GVVectorArray(varray.type(), size), varray_(std::move(varray))
  {
  }

 protected:
  int64_t get_vector_size_impl(int64_t index) const override;
  void get_vector_element_impl(int64_t index,
                               int64_t index_in_vector,
                               void *r_value) const override;

  bool is_single_vector_impl() const override;
};

class GVVectorArray_For_SingleGSpan : public GVVectorArray {
 private:
  const GSpan span_;

 public:
  GVVectorArray_For_SingleGSpan(const GSpan span, const int64_t size)
      : GVVectorArray(span.type(), size), span_(span)
  {
  }

 protected:
  int64_t get_vector_size_impl(int64_t /*index*/) const override;
  void get_vector_element_impl(int64_t /*index*/,
                               int64_t index_in_vector,
                               void *r_value) const override;

  bool is_single_vector_impl() const override;
};

template<typename T> class VVectorArray_For_GVVectorArray : public VVectorArray<T> {
 private:
  const GVVectorArray &vector_array_;

 public:
  VVectorArray_For_GVVectorArray(const GVVectorArray &vector_array)
      : VVectorArray<T>(vector_array.size()), vector_array_(vector_array)
  {
  }

 protected:
  int64_t get_vector_size_impl(const int64_t index) const override
  {
    return vector_array_.get_vector_size(index);
  }

  T get_vector_element_impl(const int64_t index, const int64_t index_in_vector) const override
  {
    T value;
    vector_array_.get_vector_element(index, index_in_vector, &value);
    return value;
  }

  bool is_single_vector_impl() const override
  {
    return vector_array_.is_single_vector();
  }
};

}  // namespace blender
