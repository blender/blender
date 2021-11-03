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
 * This is a generic counterpart to #blender::Array, used when the type is not known at runtime.
 *
 * `GArray` should generally only be used for passing data around in dynamic contexts.
 * It does not support a few things that #blender::Array supports:
 *  - Small object optimization / inline buffer.
 *  - Exception safety and various more specific constructors.
 */

#include "BLI_allocator.hh"

#include "FN_cpp_type.hh"
#include "FN_generic_span.hh"

namespace blender::fn {

template<
    /**
     * The allocator used by this array. Should rarely be changed, except when you don't want that
     * MEM_* functions are used internally.
     */
    typename Allocator = GuardedAllocator>
class GArray {
 protected:
  /** The type of the data in the array, will be null after the array is default constructed,
   * but a value should be assigned before any other interaction with the array. */
  const CPPType *type_ = nullptr;
  void *data_ = nullptr;
  int64_t size_ = 0;

  Allocator allocator_;

 public:
  /**
   * The default constructor creates an empty array, the only situation in which the type is
   * allowed to be null. This default constructor exists so `GArray` can be used in containers,
   * but the type should be supplied before doing anything else to the array.
   */
  GArray(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
  }

  GArray(NoExceptConstructor, Allocator allocator = {}) noexcept : GArray(allocator)
  {
  }

  /**
   * Create and allocate a new array, with elements default constructed
   * (which does not do anything for trivial types).
   */
  GArray(const CPPType &type, int64_t size, Allocator allocator = {}) : GArray(type, allocator)
  {
    BLI_assert(size >= 0);
    size_ = size;
    data_ = this->allocate(size_);
    type_->default_construct_n(data_, size_);
  }

  /**
   * Create an empty array with just a type.
   */
  GArray(const CPPType &type, Allocator allocator = {}) : GArray(allocator)
  {
    type_ = &type;
  }

  /**
   * Take ownership of a buffer with a provided size. The buffer should be
   * allocated with the same allocator provided to the constructor.
   */
  GArray(const CPPType &type, void *buffer, int64_t size, Allocator allocator = {})
      : GArray(type, allocator)
  {
    BLI_assert(size >= 0);
    BLI_assert(buffer != nullptr || size == 0);
    BLI_assert(type_->pointer_has_valid_alignment(buffer));

    data_ = buffer;
    size_ = size;
  }

  /**
   * Create an array by copying values from a generic span.
   */
  GArray(const GSpan span, Allocator allocator = {}) : GArray(span.type(), span.size(), allocator)
  {
    if (span.data() != nullptr) {
      BLI_assert(span.size() != 0);
      /* Use copy assign rather than construct since the memory is already initialized. */
      type_->copy_assign_n(span.data(), data_, size_);
    }
  }

  /**
   * Create an array by copying values from another generic array.
   */
  GArray(const GArray &other) : GArray(other.as_span(), other.allocator())
  {
  }

  /**
   * Create an array by taking ownership of another array's data, clearing the data in the other.
   */
  GArray(GArray &&other) : GArray(other.type(), other.data(), other.size(), other.allocator())
  {
    other.data_ = nullptr;
    other.size_ = 0;
  }

  ~GArray()
  {
    if (data_ != nullptr) {
      type_->destruct_n(data_, size_);
      this->deallocate(data_);
    }
  }

  GArray &operator=(const GArray &other)
  {
    return copy_assign_container(*this, other);
  }

  GArray &operator=(GArray &&other)
  {
    return move_assign_container(*this, std::move(other));
  }

  const CPPType &type() const
  {
    BLI_assert(type_ != nullptr);
    return *type_;
  }

  bool is_empty() const
  {
    return size_ == 0;
  }

  /**
   * Return the number of elements in the array (not the size in bytes).
   */
  int64_t size() const
  {
    return size_;
  }

  /**
   * Get a pointer to the beginning of the array.
   */
  const void *data() const
  {
    return data_;
  }
  void *data()
  {
    return data_;
  }

  const void *operator[](int64_t index) const
  {
    BLI_assert(index < size_);
    return POINTER_OFFSET(data_, type_->size() * index);
  }

  void *operator[](int64_t index)
  {
    BLI_assert(index < size_);
    return POINTER_OFFSET(data_, type_->size() * index);
  }

  operator GSpan() const
  {
    BLI_assert(type_ != nullptr);
    return GSpan(*type_, data_, size_);
  }

  operator GMutableSpan()
  {
    BLI_assert(type_ != nullptr);
    return GMutableSpan(*type_, data_, size_);
  }

  GSpan as_span() const
  {
    return *this;
  }

  GMutableSpan as_mutable_span()
  {
    return *this;
  }

  /**
   * Access the allocator used by this array.
   */
  Allocator &allocator()
  {
    return allocator_;
  }
  const Allocator &allocator() const
  {
    return allocator_;
  }

  /**
   * Destruct values and create a new array of the given size. The values in the new array are
   * default constructed.
   */
  void reinitialize(const int64_t new_size)
  {
    BLI_assert(new_size >= 0);
    int64_t old_size = size_;

    type_->destruct_n(data_, size_);
    size_ = 0;

    if (new_size <= old_size) {
      type_->default_construct_n(data_, new_size);
    }
    else {
      void *new_data = this->allocate(new_size);
      try {
        type_->default_construct_n(new_data, new_size);
      }
      catch (...) {
        this->deallocate(new_data);
        throw;
      }
      this->deallocate(data_);
      data_ = new_data;
    }

    size_ = new_size;
  }

 private:
  void *allocate(int64_t size)
  {
    const int64_t item_size = type_->size();
    const int64_t alignment = type_->alignment();
    return allocator_.allocate(static_cast<size_t>(size) * item_size, alignment, AT);
  }

  void deallocate(void *ptr)
  {
    allocator_.deallocate(ptr);
  }
};

}  // namespace blender::fn
