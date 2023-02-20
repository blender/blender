/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * A `blender::BitVector` is a dynamically growing contiguous arrays of bits. Its main purpose is
 * to provide a compact way to map indices to bools. It requires 8 times less memory compared to a
 * `blender::Vector<bool>`.
 *
 * Advantages of using a bit- instead of byte-vector are:
 * - Uses less memory.
 * - Allows checking the state of many elements at the same time (8 times more bits than bytes fit
 *   into a CPU register). This can improve performance.
 *
 * The compact nature of storing bools in individual bits has some downsides that have to be kept
 * in mind:
 * - Writing to separate bits in the same int is not thread-safe. Therefore, an existing vector of
 *   bool can't easily be replaced with a bit vector, if it is written to from multiple threads.
 *   Read-only access from multiple threads is fine though.
 * - Writing individual elements is more expensive when the array is in cache already. That is
 *   because changing a bit is always a read-modify-write operation on the int the bit resides in.
 * - Reading individual elements is more expensive when the array is in cache already. That is
 *   because additional bit-wise operations have to be applied after the corresponding int is
 *   read.
 *
 * Comparison to `std::vector<bool>`:
 * - `blender::BitVector` has an interface that is more optimized for dealing with bits.
 * - `blender::BitVector` has an inline buffer that is used to avoid allocations when the vector is
 *   small.
 *
 * Comparison to `BLI_bitmap`:
 * - `blender::BitVector` offers a more C++ friendly interface.
 * - `BLI_bitmap` should only be used in C code that can not use `blender::BitVector`.
 */

#include <cstring>

#include "BLI_allocator.hh"
#include "BLI_bit_span.hh"
#include "BLI_span.hh"

namespace blender::bits {

template<
    /**
     * Number of bits that can be stored in the vector without doing an allocation.
     */
    int64_t InlineBufferCapacity = 64,
    /**
     * The allocator used by this vector. Should rarely be changed, except when you don't want that
     * MEM_* is used internally.
     */
    typename Allocator = GuardedAllocator>
class BitVector {
 private:
  static constexpr int64_t required_ints_for_bits(const int64_t number_of_bits)
  {
    return (number_of_bits + BitsPerInt - 1) / BitsPerInt;
  }

  static constexpr int64_t IntsInInlineBuffer = required_ints_for_bits(InlineBufferCapacity);
  static constexpr int64_t BitsInInlineBuffer = IntsInInlineBuffer * BitsPerInt;
  static constexpr int64_t AllocationAlignment = alignof(BitInt);

  /**
   * Points to the first integer used by the vector. It might point to the memory in the inline
   * buffer.
   */
  BitInt *data_;

  /** Current size of the vector in bits. */
  int64_t size_in_bits_;

  /** Number of bits that fit into the vector until a reallocation has to occur. */
  int64_t capacity_in_bits_;

  /** Used for allocations when the inline buffer is too small. */
  BLI_NO_UNIQUE_ADDRESS Allocator allocator_;

  /** Contains the bits as long as the vector is small enough. */
  BLI_NO_UNIQUE_ADDRESS TypedBuffer<BitInt, IntsInInlineBuffer> inline_buffer_;

 public:
  BitVector(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    data_ = inline_buffer_;
    size_in_bits_ = 0;
    capacity_in_bits_ = BitsInInlineBuffer;
    uninitialized_fill_n(data_, IntsInInlineBuffer, BitInt(0));
  }

  BitVector(NoExceptConstructor, Allocator allocator = {}) noexcept : BitVector(allocator)
  {
  }

  BitVector(const BitVector &other) : BitVector(NoExceptConstructor(), other.allocator_)
  {
    const int64_t ints_to_copy = other.used_ints_amount();
    if (other.size_in_bits_ <= BitsInInlineBuffer) {
      /* The data is copied into the owned inline buffer. */
      data_ = inline_buffer_;
      capacity_in_bits_ = BitsInInlineBuffer;
    }
    else {
      /* Allocate a new array because the inline buffer is too small. */
      data_ = static_cast<BitInt *>(
          allocator_.allocate(ints_to_copy * sizeof(BitInt), AllocationAlignment, __func__));
      capacity_in_bits_ = ints_to_copy * BitsPerInt;
    }
    size_in_bits_ = other.size_in_bits_;
    uninitialized_copy_n(other.data_, ints_to_copy, data_);
  }

  BitVector(BitVector &&other) noexcept : BitVector(NoExceptConstructor(), other.allocator_)
  {
    if (other.is_inline()) {
      /* Copy the data into the inline buffer. */
      const int64_t ints_to_copy = other.used_ints_amount();
      data_ = inline_buffer_;
      uninitialized_copy_n(other.data_, ints_to_copy, data_);
    }
    else {
      /* Steal the pointer. */
      data_ = other.data_;
    }
    size_in_bits_ = other.size_in_bits_;
    capacity_in_bits_ = other.capacity_in_bits_;

    /* Clear the other vector because it has been moved from. */
    other.data_ = other.inline_buffer_;
    other.size_in_bits_ = 0;
    other.capacity_in_bits_ = BitsInInlineBuffer;
  }

  /**
   * Create a new vector with the given size and fill it with #value.
   */
  BitVector(const int64_t size_in_bits, const bool value = false, Allocator allocator = {})
      : BitVector(NoExceptConstructor(), allocator)
  {
    this->resize(size_in_bits, value);
  }

  /**
   * Create a bit vector based on an array of bools. Each byte of the input array maps to one bit.
   */
  explicit BitVector(const Span<bool> values, Allocator allocator = {})
      : BitVector(NoExceptConstructor(), allocator)
  {
    this->resize(values.size());
    for (const int64_t i : this->index_range()) {
      (*this)[i].set(values[i]);
    }
  }

  ~BitVector()
  {
    if (!this->is_inline()) {
      allocator_.deallocate(data_);
    }
  }

  BitVector &operator=(const BitVector &other)
  {
    return copy_assign_container(*this, other);
  }

  BitVector &operator=(BitVector &&other)
  {
    return move_assign_container(*this, std::move(other));
  }

  operator BitSpan() const
  {
    return {data_, IndexRange(size_in_bits_)};
  }

  operator MutableBitSpan()
  {
    return {data_, IndexRange(size_in_bits_)};
  }

  /**
   * Number of bits in the bit vector.
   */
  int64_t size() const
  {
    return size_in_bits_;
  }

  bool is_empty() const
  {
    return size_in_bits_ == 0;
  }

  /**
   * Get a read-only reference to a specific bit.
   */
  BitRef operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_in_bits_);
    return {data_, index};
  }

  /**
   * Get a mutable reference to a specific bit.
   */
  MutableBitRef operator[](const int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_in_bits_);
    return {data_, index};
  }

  IndexRange index_range() const
  {
    return {0, size_in_bits_};
  }

  /**
   * Add a new bit to the end of the vector.
   */
  void append(const bool value)
  {
    this->ensure_space_for_one();
    MutableBitRef bit{data_, size_in_bits_};
    bit.set(value);
    size_in_bits_++;
  }

  BitIterator begin() const
  {
    return {data_, 0};
  }

  BitIterator end() const
  {
    return {data_, size_in_bits_};
  }

  MutableBitIterator begin()
  {
    return {data_, 0};
  }

  MutableBitIterator end()
  {
    return {data_, size_in_bits_};
  }

  /**
   * Change the size of the vector. If the new vector is larger than the old one, the new elements
   * are filled with #value.
   */
  void resize(const int64_t new_size_in_bits, const bool value = false)
  {
    BLI_assert(new_size_in_bits >= 0);
    const int64_t old_size_in_bits = size_in_bits_;
    if (new_size_in_bits > old_size_in_bits) {
      this->reserve(new_size_in_bits);
    }
    size_in_bits_ = new_size_in_bits;
    if (old_size_in_bits < new_size_in_bits) {
      MutableBitSpan(data_, IndexRange(old_size_in_bits, new_size_in_bits - old_size_in_bits))
          .set_all(value);
    }
  }

  /**
   * Set #value on every element.
   */
  void fill(const bool value)
  {
    MutableBitSpan(data_, size_in_bits_).set_all(value);
  }

  /**
   * Make sure that the capacity of the vector is large enough to hold the given amount of bits.
   * The actual size is not changed.
   */
  void reserve(const int new_capacity_in_bits)
  {
    this->realloc_to_at_least(new_capacity_in_bits);
  }

  /**
   * Reset the size of the vector to zero elements, but keep the same memory capacity to be
   * refilled again.
   */
  void clear()
  {
    size_in_bits_ = 0;
  }

  /**
   * Free memory and reset the vector to zero elements.
   */
  void clear_and_shrink()
  {
    size_in_bits_ = 0;
    capacity_in_bits_ = 0;
    if (!this->is_inline()) {
      allocator_.deallocate(data_);
    }
    data_ = inline_buffer_;
  }

 private:
  void ensure_space_for_one()
  {
    if (UNLIKELY(size_in_bits_ >= capacity_in_bits_)) {
      this->realloc_to_at_least(size_in_bits_ + 1);
    }
  }

  BLI_NOINLINE void realloc_to_at_least(const int64_t min_capacity_in_bits,
                                        const BitInt initial_value_for_new_ints = 0)
  {
    if (capacity_in_bits_ >= min_capacity_in_bits) {
      return;
    }

    const int64_t min_capacity_in_ints = this->required_ints_for_bits(min_capacity_in_bits);

    /* At least double the size of the previous allocation. */
    const int64_t min_new_capacity_in_ints = 2 * this->required_ints_for_bits(capacity_in_bits_);

    const int64_t new_capacity_in_ints = std::max(min_capacity_in_ints, min_new_capacity_in_ints);
    const int64_t ints_to_copy = this->used_ints_amount();

    BitInt *new_data = static_cast<BitInt *>(
        allocator_.allocate(new_capacity_in_ints * sizeof(BitInt), AllocationAlignment, __func__));
    uninitialized_copy_n(data_, ints_to_copy, new_data);
    /* Always initialize new capacity even if it isn't used yet. That's necessary to avoid warnings
     * caused by using uninitialized memory. This happens when e.g. setting a clearing a bit in an
     * uninitialized int. */
    uninitialized_fill_n(
        new_data + ints_to_copy, new_capacity_in_ints - ints_to_copy, initial_value_for_new_ints);

    if (!this->is_inline()) {
      allocator_.deallocate(data_);
    }

    data_ = new_data;
    capacity_in_bits_ = new_capacity_in_ints * BitsPerInt;
  }

  bool is_inline() const
  {
    return data_ == inline_buffer_;
  }

  int64_t used_ints_amount() const
  {
    return this->required_ints_for_bits(size_in_bits_);
  }
};

}  // namespace blender::bits

namespace blender {
using bits::BitVector;
}  // namespace blender
