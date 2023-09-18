/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This file provides the basis for processing "indexed bits" (i.e. every bit has an index).
 * The main purpose of this file is to define how bits are indexed within a memory buffer.
 * For example, one has to define whether the first bit is the least or most significant bit and
 * how endianness affect the bit order.
 *
 * The order is defined as follows:
 * - Every indexed bit is part of an #BitInt. These ints are ordered by their address as usual.
 * - Within each #BitInt, the bits are ordered from least to most significant.
 */

#include "BLI_index_range.hh"
#include "BLI_utildefines.h"

#include <iosfwd>

namespace blender::bits {

/** Using a large integer type is better because then it's easier to process many bits at once. */
using BitInt = uint64_t;
/** Number of bits that fit into #BitInt. */
static constexpr int64_t BitsPerInt = int64_t(sizeof(BitInt) * 8);
/** Shift amount to get from a bit index to an int index. Equivalent to `log(BitsPerInt, 2)`. */
static constexpr int64_t BitToIntIndexShift = 3 + (sizeof(BitInt) >= 2) + (sizeof(BitInt) >= 4) +
                                              (sizeof(BitInt) >= 8);
/** Bit mask containing a 1 for the last few bits that index a bit inside of an #BitInt. */
static constexpr BitInt BitIndexMask = (BitInt(1) << BitToIntIndexShift) - 1;

inline BitInt mask_first_n_bits(const int64_t n)
{
  BLI_assert(n >= 0);
  BLI_assert(n <= BitsPerInt);
  if (n == BitsPerInt) {
    return BitInt(-1);
  }
  return (BitInt(1) << n) - 1;
}

inline BitInt mask_last_n_bits(const int64_t n)
{
  return ~mask_first_n_bits(BitsPerInt - n);
}

inline BitInt mask_range_bits(const int64_t start, const int64_t size)
{
  BLI_assert(start >= 0);
  BLI_assert(size >= 0);
  const int64_t end = start + size;
  BLI_assert(end <= BitsPerInt);
  if (end == BitsPerInt) {
    return mask_last_n_bits(size);
  }
  return ((BitInt(1) << end) - 1) & ~((BitInt(1) << start) - 1);
}

inline BitInt mask_single_bit(const int64_t bit_index)
{
  BLI_assert(bit_index >= 0);
  BLI_assert(bit_index < BitsPerInt);
  return BitInt(1) << bit_index;
}

inline BitInt *int_containing_bit(BitInt *data, const int64_t bit_index)
{
  return data + (bit_index >> BitToIntIndexShift);
}

inline const BitInt *int_containing_bit(const BitInt *data, const int64_t bit_index)
{
  return data + (bit_index >> BitToIntIndexShift);
}

/**
 * This is a read-only pointer to a specific bit. The value of the bit can be retrieved, but
 * not changed.
 */
class BitRef {
 private:
  /** Points to the exact integer that the bit is in. */
  const BitInt *int_;
  /** All zeros except for a single one at the bit that is referenced. */
  BitInt mask_;

  friend class MutableBitRef;

 public:
  BitRef() = default;

  /**
   * Reference a specific bit in an array. Note that #data does *not* have to point to the
   * exact integer the bit is in.
   */
  BitRef(const BitInt *data, const int64_t bit_index)
  {
    int_ = int_containing_bit(data, bit_index);
    mask_ = mask_single_bit(bit_index & BitIndexMask);
  }

  /**
   * Return true when the bit is currently 1 and false otherwise.
   */
  bool test() const
  {
    const BitInt value = *int_;
    const BitInt masked_value = value & mask_;
    return masked_value != 0;
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
  /** Points to the integer that the bit is in. */
  BitInt *int_;
  /** All zeros except for a single one at the bit that is referenced. */
  BitInt mask_;

 public:
  MutableBitRef() = default;

  /**
   * Reference a specific bit in an array. Note that #data does *not* have to point to the
   * exact int the bit is in.
   */
  MutableBitRef(BitInt *data, const int64_t bit_index)
  {
    int_ = int_containing_bit(data, bit_index);
    mask_ = mask_single_bit(bit_index & BitIndexMask);
  }

  /**
   * Support implicitly casting to a read-only #BitRef.
   */
  operator BitRef() const
  {
    BitRef bit_ref;
    bit_ref.int_ = int_;
    bit_ref.mask_ = mask_;
    return bit_ref;
  }

  /**
   * Return true when the bit is currently 1 and false otherwise.
   */
  bool test() const
  {
    const BitInt value = *int_;
    const BitInt masked_value = value & mask_;
    return masked_value != 0;
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
    *int_ |= mask_;
  }

  /**
   * Change the bit to a 0.
   */
  void reset()
  {
    *int_ &= ~mask_;
  }

  /**
   * Change the bit to a 1 if #value is true and 0 otherwise. If the value is highly unpredictable
   * by the CPU branch predictor, it can be faster to use #set_branchless instead.
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

  /**
   * Does the same as #set, but does not use a branch. This is faster when the input value is
   * unpredictable for the CPU branch predictor (best case for this function is a uniform random
   * distribution with 50% probability for true and false). If the value is predictable, this is
   * likely slower than #set.
   */
  void set_branchless(const bool value)
  {
    const BitInt value_int = BitInt(value);
    BLI_assert(ELEM(value_int, 0, 1));
    const BitInt old = *int_;
    *int_ =
        /* Unset bit. */
        (~mask_ & old)
        /* Optionally set it again. The -1 turns a 1 into `0x00...` and a 0 into `0xff...`. */
        | (mask_ & ~(value_int - 1));
  }

  MutableBitRef &operator|=(const bool value)
  {
    if (value) {
      this->set();
    }
    return *this;
  }

  MutableBitRef &operator&=(const bool value)
  {
    if (!value) {
      this->reset();
    }
    return *this;
  }
};

std::ostream &operator<<(std::ostream &stream, const BitRef &bit);
std::ostream &operator<<(std::ostream &stream, const MutableBitRef &bit);

}  // namespace blender::bits

namespace blender {
using bits::BitRef;
using bits::MutableBitRef;
}  // namespace blender
