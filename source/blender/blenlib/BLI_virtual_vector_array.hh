/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * A virtual vector array gives access to an array of vectors. The individual vectors in the array
 * can have different sizes.
 *
 * The tradeoffs here are similar to virtual arrays.
 */

#include "BLI_virtual_array.hh"

namespace blender {

/** A read-only virtual array of vectors. */
template<typename T> class VVectorArray {
 protected:
  int64_t size_;

 public:
  VVectorArray(const int64_t size) : size_(size)
  {
    BLI_assert(size >= 0);
  }

  virtual ~VVectorArray() = default;

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

  /* Returns the size of the vector at the given index. */
  int64_t get_vector_size(const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return this->get_vector_size_impl(index);
  }

  /* Returns an element from one of the vectors. */
  T get_vector_element(const int64_t index, const int64_t index_in_vector) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    BLI_assert(index_in_vector >= 0);
    BLI_assert(index_in_vector < this->get_vector_size(index));
    return this->get_vector_element_impl(index, index_in_vector);
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

  virtual T get_vector_element_impl(int64_t index, int64_t index_in_vetor) const = 0;

  virtual bool is_single_vector_impl() const
  {
    return false;
  }
};

}  // namespace blender
