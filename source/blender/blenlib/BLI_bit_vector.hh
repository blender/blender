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
 * - Writing to separate bits in the same byte is not thread-safe. Therefore, an existing vector of
 *   bool can't easily be replaced with a bit vector, if it is written to from multiple threads.
 *   Read-only access from multiple threads is fine though.
 * - Writing individual elements is more expensive when the array is in cache already. That is
 *   because changing a bit is always a read-modify-write operation on the byte the bit resides in.
 * - Reading individual elements is more expensive when the array is in cache already. That is
 *   because additional bit-wise operations have to be applied after the corresponding byte is
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
#include "BLI_index_range.hh"
#include "BLI_memory_utils.hh"
#include "BLI_span.hh"

namespace blender {

/**
 * This is a read-only pointer to a specific bit. The value of the bit can be retrieved, but not
 * changed.
 */
class BitRef {
 private:
  /** Points to the exact byte that the bit is in. */
  const uint8_t *byte_ptr_;
  /** All zeros except for a single one at the bit that is referenced. */
  uint8_t mask_;

  friend class MutableBitRef;

 public:
  BitRef() = default;

  /**
   * Reference a specific bit in a byte array. Note that #byte_ptr does *not* have to point to the
   * exact byte the bit is in.
   */
  BitRef(const uint8_t *byte_ptr, const int64_t bit_index)
  {
    byte_ptr_ = byte_ptr + (bit_index >> 3);
    mask_ = 1 << (bit_index & 7);
  }

  /**
   * Return true when the bit is currently 1 and false otherwise.
   */
  bool test() const
  {
    const uint8_t byte = *byte_ptr_;
    const uint8_t masked_byte = byte & mask_;
    return masked_byte != 0;
  }

  operator bool() const
  {
    return this->test();
  }
};

/**
 * Similar to #BitRef, but also allows changing the referenced bit.
 */
class MutableBitRef {
 private:
  /** Points to the exact byte that the bit is in. */
  uint8_t *byte_ptr_;
  /** All zeros except for a single one at the bit that is referenced. */
  uint8_t mask_;

 public:
  MutableBitRef() = default;

  /**
   * Reference a specific bit in a byte array. Note that #byte_ptr does *not* have to point to the
   * exact byte the bit is in.
   */
  MutableBitRef(uint8_t *byte_ptr, const int64_t bit_index)
  {
    byte_ptr_ = byte_ptr + (bit_index >> 3);
    mask_ = 1 << static_cast<uint8_t>(bit_index & 7);
  }

  /**
   * Support implicitly casting to a read-only #BitRef.
   */
  operator BitRef() const
  {
    BitRef bit_ref;
    bit_ref.byte_ptr_ = byte_ptr_;
    bit_ref.mask_ = mask_;
    return bit_ref;
  }

  /**
   * Return true when the bit is currently 1 and false otherwise.
   */
  bool test() const
  {
    const uint8_t byte = *byte_ptr_;
    const uint8_t masked_byte = byte & mask_;
    return masked_byte != 0;
  }

  operator bool() const
  {
    return this->test();
  }

  /**
   * Change the bit to a 1.
   */
  void set()
  {
    *byte_ptr_ |= mask_;
  }

  /**
   * Change the bit to a 0.
   */
  void reset()
  {
    *byte_ptr_ &= ~mask_;
  }

  /**
   * Change the bit to a 1 if #value is true and 0 otherwise.
   */
  void set(const bool value)
  {
    if (value) {
      this->set();
    }
    else {
      this->reset();
    }
  }
};

template<
    /**
     * Number of bits that can be stored in the vector without doing an allocation.
     */
    int64_t InlineBufferCapacity = 32,
    /**
     * The allocator used by this vector. Should rarely be changed, except when you don't want that
     * MEM_* is used internally.
     */
    typename Allocator = GuardedAllocator>
class BitVector {
 private:
  static constexpr int64_t required_bytes_for_bits(const int64_t number_of_bits)
  {
    return (number_of_bits + BitsPerByte - 1) / BitsPerByte;
  }

  static constexpr int64_t BitsPerByte = 8;
  static constexpr int64_t BytesInInlineBuffer = required_bytes_for_bits(InlineBufferCapacity);
  static constexpr int64_t BitsInInlineBuffer = BytesInInlineBuffer * BitsPerByte;
  static constexpr int64_t AllocationAlignment = 8;

  /**
   * Points to the first byte used by the vector. It might point to the memory in the inline
   * buffer.
   */
  uint8_t *data_;

  /** Current size of the vector in bits. */
  int64_t size_in_bits_;

  /** Number of bits that fit into the vector until a reallocation has to occure. */
  int64_t capacity_in_bits_;

  /** Used for allocations when the inline buffer is too small. */
  Allocator allocator_;

  /** Contains the bits as long as the vector is small enough. */
  TypedBuffer<uint8_t, BytesInInlineBuffer> inline_buffer_;

 public:
  BitVector(Allocator allocator = {}) noexcept : allocator_(allocator)
  {
    data_ = inline_buffer_;
    size_in_bits_ = 0;
    capacity_in_bits_ = BitsInInlineBuffer;
    uninitialized_fill_n(data_, BytesInInlineBuffer, static_cast<uint8_t>(0));
  }

  BitVector(NoExceptConstructor, Allocator allocator = {}) noexcept : BitVector(allocator)
  {
  }

  BitVector(const BitVector &other) : BitVector(NoExceptConstructor(), other.allocator_)
  {
    const int64_t bytes_to_copy = other.used_bytes_amount();
    if (other.size_in_bits_ <= BitsInInlineBuffer) {
      /* The data is copied into the owned inline buffer. */
      data_ = inline_buffer_;
      capacity_in_bits_ = BitsInInlineBuffer;
    }
    else {
      /* Allocate a new byte array because the inline buffer is too small. */
      data_ = static_cast<uint8_t *>(
          allocator_.allocate(bytes_to_copy, AllocationAlignment, __func__));
      capacity_in_bits_ = bytes_to_copy * BitsPerByte;
    }
    size_in_bits_ = other.size_in_bits_;
    uninitialized_copy_n(other.data_, bytes_to_copy, data_);
  }

  BitVector(BitVector &&other) noexcept : BitVector(NoExceptConstructor(), other.allocator_)
  {
    if (other.is_inline()) {
      /* Copy the data into the inline buffer. */
      const int64_t bytes_to_copy = other.used_bytes_amount();
      data_ = inline_buffer_;
      uninitialized_copy_n(other.data_, bytes_to_copy, data_);
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

  /**
   * Number of bits in the bit vector.
   */
  int64_t size() const
  {
    return size_in_bits_;
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

  class Iterator {
   private:
    const BitVector *vector_;
    int64_t index_;

   public:
    Iterator(const BitVector &vector, const int64_t index) : vector_(&vector), index_(index)
    {
    }

    Iterator &operator++()
    {
      index_++;
      return *this;
    }

    friend bool operator!=(const Iterator &a, const Iterator &b)
    {
      BLI_assert(a.vector_ == b.vector_);
      return a.index_ != b.index_;
    }

    BitRef operator*() const
    {
      return (*vector_)[index_];
    }
  };

  class MutableIterator {
   private:
    BitVector *vector_;
    int64_t index_;

   public:
    MutableIterator(BitVector &vector, const int64_t index) : vector_(&vector), index_(index)
    {
    }

    MutableIterator &operator++()
    {
      index_++;
      return *this;
    }

    friend bool operator!=(const MutableIterator &a, const MutableIterator &b)
    {
      BLI_assert(a.vector_ == b.vector_);
      return a.index_ != b.index_;
    }

    MutableBitRef operator*() const
    {
      return (*vector_)[index_];
    }
  };

  Iterator begin() const
  {
    return {*this, 0};
  }

  Iterator end() const
  {
    return {*this, size_in_bits_};
  }

  MutableIterator begin()
  {
    return {*this, 0};
  }

  MutableIterator end()
  {
    return {*this, size_in_bits_};
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
      this->fill_range(IndexRange(old_size_in_bits, new_size_in_bits - old_size_in_bits), value);
    }
  }

  /**
   * Set #value for every element in #range.
   */
  void fill_range(const IndexRange range, const bool value)
  {
    const AlignedIndexRanges aligned_ranges = split_index_range_by_alignment(range, BitsPerByte);

    /* Fill first few bits. */
    for (const int64_t i : aligned_ranges.prefix) {
      (*this)[i].set(value);
    }

    /* Fill entire bytes at once. */
    const int64_t start_fill_byte_index = aligned_ranges.aligned.start() / BitsPerByte;
    const int64_t bytes_to_fill = aligned_ranges.aligned.size() / BitsPerByte;
    const uint8_t fill_value = value ? (uint8_t)0xff : (uint8_t)0x00;
    initialized_fill_n(data_ + start_fill_byte_index, bytes_to_fill, fill_value);

    /* Fill bits in the end that don't cover a full byte. */
    for (const int64_t i : aligned_ranges.suffix) {
      (*this)[i].set(value);
    }
  }

  /**
   * Set #value on every element.
   */
  void fill(const bool value)
  {
    this->fill_range(IndexRange(0, size_in_bits_), value);
  }

  /**
   * Make sure that the capacity of the vector is large enough to hold the given amount of bits.
   * The actual size is not changed.
   */
  void reserve(const int new_capacity_in_bits)
  {
    this->realloc_to_at_least(new_capacity_in_bits);
  }

 private:
  void ensure_space_for_one()
  {
    if (UNLIKELY(size_in_bits_ >= capacity_in_bits_)) {
      this->realloc_to_at_least(size_in_bits_ + 1);
    }
  }

  BLI_NOINLINE void realloc_to_at_least(const int64_t min_capacity_in_bits,
                                        const uint8_t initial_value_for_new_bytes = 0x00)
  {
    if (capacity_in_bits_ >= min_capacity_in_bits) {
      return;
    }

    const int64_t min_capacity_in_bytes = this->required_bytes_for_bits(min_capacity_in_bits);

    /* At least double the size of the previous allocation. */
    const int64_t min_new_capacity_in_bytes = capacity_in_bits_ * 2;

    const int64_t new_capacity_in_bytes = std::max(min_capacity_in_bytes,
                                                   min_new_capacity_in_bytes);
    const int64_t bytes_to_copy = this->used_bytes_amount();

    uint8_t *new_data = static_cast<uint8_t *>(
        allocator_.allocate(new_capacity_in_bytes, AllocationAlignment, __func__));
    uninitialized_copy_n(data_, bytes_to_copy, new_data);
    /* Always initialize new capacity even if it isn't used yet. That's necessary to avoid warnings
     * caused by using uninitialized memory. This happens when e.g. setting a clearing a bit in an
     * uninitialized byte. */
    uninitialized_fill_n(new_data + bytes_to_copy,
                         new_capacity_in_bytes - bytes_to_copy,
                         (uint8_t)initial_value_for_new_bytes);

    if (!this->is_inline()) {
      allocator_.deallocate(data_);
    }

    data_ = new_data;
    capacity_in_bits_ = new_capacity_in_bytes * BitsPerByte;
  }

  bool is_inline() const
  {
    return data_ == inline_buffer_;
  }

  int64_t used_bytes_amount() const
  {
    return this->required_bytes_for_bits(size_in_bits_);
  }
};

}  // namespace blender
