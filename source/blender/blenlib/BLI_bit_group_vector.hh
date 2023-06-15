/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_bit_vector.hh"

namespace blender::bits {

/**
 * A #BitGroupVector is a compact data structure that allows storing an arbitrary but fixed number
 * of bits per element. For example, it could be used to compactly store 5 bits per vertex in a
 * mesh. The data structure stores the bits in a way so that the #BitSpan for every element is
 * bounded according to #is_bounded_span. The makes sure that operations on entire groups can be
 * implemented efficiently. For example, one can easy `or` one group into another.
 */
template<int64_t InlineBufferCapacity = 64, typename Allocator = GuardedAllocator>
class BitGroupVector {
 private:
  /**
   * Number of bits per group.
   */
  int64_t group_size_ = 0;
  /**
   * Actually stored number of bits per group so that individual groups are bounded according to
   * #is_bounded_span.
   */
  int64_t aligned_group_size_ = 0;
  BitVector<InlineBufferCapacity, Allocator> data_;

  static int64_t align_group_size(const int64_t group_size)
  {
    if (group_size < 64) {
      /* Align to next power of two so that a single group never spans across two ints. */
      return int64_t(power_of_2_max_u(uint32_t(group_size)));
    }
    /* Align to multiple of BitsPerInt. */
    return (group_size + BitsPerInt - 1) & ~(BitsPerInt - 1);
  }

 public:
  BitGroupVector() = default;

  BitGroupVector(const int64_t size_in_groups,
                 const int64_t group_size,
                 const bool value = false,
                 Allocator allocator = {})
      : group_size_(group_size),
        aligned_group_size_(align_group_size(group_size)),
        data_(size_in_groups * aligned_group_size_, value, allocator)
  {
    BLI_assert(group_size >= 0);
    BLI_assert(size_in_groups >= 0);
  }

  /** Get all the bits at an index. */
  BoundedBitSpan operator[](const int64_t i) const
  {
    const int64_t offset = aligned_group_size_ * i;
    return {data_.data() + (offset >> BitToIntIndexShift),
            IndexRange(offset & BitIndexMask, group_size_)};
  }

  /** Get all the bits at an index. */
  MutableBoundedBitSpan operator[](const int64_t i)
  {
    const int64_t offset = aligned_group_size_ * i;
    return {data_.data() + (offset >> BitToIntIndexShift),
            IndexRange(offset & BitIndexMask, group_size_)};
  }

  /** Number of groups. */
  int64_t size() const
  {
    return aligned_group_size_ == 0 ? 0 : data_.size() / aligned_group_size_;
  }

  /** Number of bits per group. */
  int64_t group_size() const
  {
    return group_size_;
  }

  IndexRange index_range() const
  {
    return IndexRange{this->size()};
  }

  /**
   * Get all stored bits. Note that this may also contain padding bits. This can be used to e.g.
   * mix multiple #BitGroupVector.
   */
  BoundedBitSpan all_bits() const
  {
    return data_;
  }

  MutableBoundedBitSpan all_bits()
  {
    return data_;
  }
};

}  // namespace blender::bits

namespace blender {
using bits::BitGroupVector;
}
