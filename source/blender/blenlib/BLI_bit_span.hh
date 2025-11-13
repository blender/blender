/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include <optional>

#include "BLI_bit_ref.hh"
#include "BLI_index_range.hh"
#include "BLI_math_bits.h"
#include "BLI_memory_utils.hh"

namespace blender::bits {

/** Base class for a const and non-const bit-iterator. */
class BitIteratorBase {
 protected:
  const BitInt *data_;
  int64_t bit_index_;

 public:
  BitIteratorBase(const BitInt *data, const int64_t bit_index) : data_(data), bit_index_(bit_index)
  {
  }

  BitIteratorBase &operator++()
  {
    bit_index_++;
    return *this;
  }

  friend bool operator!=(const BitIteratorBase &a, const BitIteratorBase &b)
  {
    BLI_assert(a.data_ == b.data_);
    return a.bit_index_ != b.bit_index_;
  }
};

/** Allows iterating over the bits in a memory buffer. */
class BitIterator : public BitIteratorBase {
 public:
  BitIterator(const BitInt *data, const int64_t bit_index) : BitIteratorBase(data, bit_index) {}

  BitRef operator*() const
  {
    return BitRef(data_, bit_index_);
  }
};

/** Allows iterating over the bits in a memory buffer. */
class MutableBitIterator : public BitIteratorBase {
 public:
  MutableBitIterator(BitInt *data, const int64_t bit_index) : BitIteratorBase(data, bit_index) {}

  MutableBitRef operator*() const
  {
    return MutableBitRef(const_cast<BitInt *>(data_), bit_index_);
  }
};

class OneBitIterator {
 private:
  BitInt data_;
  int current_bit_;

 public:
  explicit OneBitIterator(BitInt data) : data_(data)
  {
    this->operator++();
  }

  int operator*() const
  {
    return current_bit_;
  }

  OneBitIterator &operator++()
  {
    if (data_ > 0) {
      current_bit_ = bitscan_forward_clear_uint64(&data_);
    }
    else {
      current_bit_ = -1;
    }
    return *this;
  }

  friend bool operator!=(const OneBitIterator &a, const OneBitIterator &b)
  {
    return a.current_bit_ != b.current_bit_;
  }
};

class OneBitIteratorRange {
 private:
  const BitInt data_;

 public:
  OneBitIteratorRange(BitInt data) : data_(data) {}

  OneBitIterator begin() const
  {
    return OneBitIterator(data_);
  }
  OneBitIterator end() const
  {
    return OneBitIterator(0);
  }
};

inline OneBitIteratorRange iter_1_indices(BitInt value)
{
  return OneBitIteratorRange(value);
}

/**
 * Similar to #Span, but references a range of bits instead of normal C++ types (which must be at
 * least one byte large). Use #MutableBitSpan if the values are supposed to be modified.
 *
 * The beginning and end of a #BitSpan does *not* have to be at byte/int boundaries. It can start
 * and end at any bit.
 */
class BitSpan {
 protected:
  /** Base pointer to the integers containing the bits. The actual bit span might start at a much
   * higher address when `bit_range_.start()` is large. */
  const BitInt *data_ = nullptr;
  /** The range of referenced bits. */
  IndexRange bit_range_ = {0, 0};

 public:
  /** Construct an empty span. */
  BitSpan() = default;

  BitSpan(const BitInt *data, const int64_t size_in_bits) : data_(data), bit_range_(size_in_bits)
  {
  }

  BitSpan(const BitInt *data, const IndexRange bit_range) : data_(data), bit_range_(bit_range) {}

  /** Number of bits referenced by the span. */
  int64_t size() const
  {
    return bit_range_.size();
  }

  bool is_empty() const
  {
    return bit_range_.is_empty();
  }

  IndexRange index_range() const
  {
    return IndexRange(bit_range_.size());
  }

  [[nodiscard]] BitRef operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < bit_range_.size());
    return {data_, bit_range_.start() + index};
  }

  [[nodiscard]] BitSpan slice(const IndexRange range) const
  {
    return {data_, bit_range_.slice(range)};
  }

  BitSpan take_front(const int64_t n) const
  {
    return {data_, bit_range_.take_front(n)};
  }

  BitSpan take_back(const int64_t n) const
  {
    return {data_, bit_range_.take_back(n)};
  }

  BitSpan drop_front(const int64_t n) const
  {
    return {data_, bit_range_.drop_front(n)};
  }

  BitSpan drop_back(const int64_t n) const
  {
    return {data_, bit_range_.drop_back(n)};
  }

  const BitInt *data() const
  {
    return data_;
  }

  const IndexRange &bit_range() const
  {
    return bit_range_;
  }

  BitIterator begin() const
  {
    return {data_, bit_range_.start()};
  }

  BitIterator end() const
  {
    return {data_, bit_range_.one_after_last()};
  }
};

/**
 * Checks if the span fulfills the requirements for a bounded span. Bounded spans can often be
 * processed more efficiently, because fewer cases have to be considered when aligning multiple
 * such spans.
 *
 * See comments in the function for the exact requirements.
 */
inline bool is_bounded_span(const BitSpan span)
{
  const int64_t offset = span.bit_range().start();
  const int64_t size = span.size();
  if (offset >= BitsPerInt) {
    /* The data pointer must point at the first int already. If the offset is a multiple of
     * #BitsPerInt, the bit span could theoretically become bounded as well if the data pointer is
     * adjusted. But that is not handled here. */
    return false;
  }
  if (size < BitsPerInt) {
    /** Don't allow small sized spans to cross `BitInt` boundaries. */
    return offset + size <= 64;
  }
  if (offset != 0) {
    /* Start of larger spans must be aligned to `BitInt` boundaries. */
    return false;
  }
  return true;
}

/**
 * Same as #BitSpan but fulfills the requirements mentioned on #is_bounded_span.
 */
class BoundedBitSpan : public BitSpan {
 public:
  BoundedBitSpan() = default;

  BoundedBitSpan(const BitInt *data, const int64_t size_in_bits) : BitSpan(data, size_in_bits)
  {
    BLI_assert(is_bounded_span(*this));
  }

  BoundedBitSpan(const BitInt *data, const IndexRange bit_range) : BitSpan(data, bit_range)
  {
    BLI_assert(is_bounded_span(*this));
  }

  explicit BoundedBitSpan(const BitSpan other) : BitSpan(other)
  {
    BLI_assert(is_bounded_span(*this));
  }

  int64_t offset() const
  {
    return bit_range_.start();
  }

  int64_t full_ints_num() const
  {
    return bit_range_.size() >> BitToIntIndexShift;
  }

  int64_t final_bits_num() const
  {
    return bit_range_.size() & BitIndexMask;
  }

  BoundedBitSpan take_front(const int64_t n) const
  {
    return {data_, bit_range_.take_front(n)};
  }
};

/** Same as #BitSpan, but also allows modifying the referenced bits. */
class MutableBitSpan {
 protected:
  BitInt *data_ = nullptr;
  IndexRange bit_range_ = {0, 0};

 public:
  MutableBitSpan() = default;

  MutableBitSpan(BitInt *data, const int64_t size) : data_(data), bit_range_(size) {}

  MutableBitSpan(BitInt *data, const IndexRange bit_range) : data_(data), bit_range_(bit_range) {}

  int64_t size() const
  {
    return bit_range_.size();
  }

  bool is_empty() const
  {
    return bit_range_.is_empty();
  }

  IndexRange index_range() const
  {
    return IndexRange(bit_range_.size());
  }

  MutableBitRef operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < bit_range_.size());
    return {data_, bit_range_.start() + index};
  }

  MutableBitSpan slice(const IndexRange range) const
  {
    return {data_, bit_range_.slice(range)};
  }

  MutableBitSpan take_front(const int64_t n) const
  {
    return {data_, bit_range_.take_front(n)};
  }

  MutableBitSpan take_back(const int64_t n) const
  {
    return {data_, bit_range_.take_back(n)};
  }

  BitInt *data() const
  {
    return data_;
  }

  const IndexRange &bit_range() const
  {
    return bit_range_;
  }

  MutableBitIterator begin() const
  {
    return {data_, bit_range_.start()};
  }

  MutableBitIterator end() const
  {
    return {data_, bit_range_.one_after_last()};
  }

  operator BitSpan() const
  {
    return {data_, bit_range_};
  }

  /** Sets all referenced bits to 1. */
  void set_all();

  /** Sets all referenced bits to 0. */
  void reset_all();

  void copy_from(const BitSpan other);
  void copy_from(const BoundedBitSpan other);

  /** Sets all referenced bits to either 0 or 1. */
  void set_all(const bool value)
  {
    if (value) {
      this->set_all();
    }
    else {
      this->reset_all();
    }
  }

  /** Same as #set_all to mirror #MutableSpan. */
  void fill(const bool value)
  {
    this->set_all(value);
  }
};

/**
 * Same as #MutableBitSpan but fulfills the requirements mentioned on #is_bounded_span.
 */
class MutableBoundedBitSpan : public MutableBitSpan {
 public:
  MutableBoundedBitSpan() = default;

  MutableBoundedBitSpan(BitInt *data, const int64_t size) : MutableBitSpan(data, size)
  {
    BLI_assert(is_bounded_span(*this));
  }

  MutableBoundedBitSpan(BitInt *data, const IndexRange bit_range) : MutableBitSpan(data, bit_range)
  {
    BLI_assert(is_bounded_span(*this));
  }

  explicit MutableBoundedBitSpan(const MutableBitSpan other) : MutableBitSpan(other)
  {
    BLI_assert(is_bounded_span(*this));
  }

  operator BoundedBitSpan() const
  {
    return BoundedBitSpan{BitSpan(*this)};
  }

  int64_t offset() const
  {
    return bit_range_.start();
  }

  int64_t full_ints_num() const
  {
    return bit_range_.size() >> BitToIntIndexShift;
  }

  int64_t final_bits_num() const
  {
    return bit_range_.size() & BitIndexMask;
  }

  MutableBoundedBitSpan take_front(const int64_t n) const
  {
    return {data_, bit_range_.take_front(n)};
  }

  BoundedBitSpan as_span() const
  {
    return BoundedBitSpan(data_, bit_range_);
  }

  void copy_from(const BitSpan other);
  void copy_from(const BoundedBitSpan other);
};

inline std::optional<BoundedBitSpan> try_get_bounded_span(const BitSpan span)
{
  if (is_bounded_span(span)) {
    return BoundedBitSpan(span);
  }
  if (span.bit_range().start() % BitsPerInt == 0) {
    return BoundedBitSpan(span.data() + (span.bit_range().start() >> BitToIntIndexShift),
                          span.size());
  }
  return std::nullopt;
}

/**
 * Overloaded in BLI_bit_vector.hh. The purpose is to make passing #BitVector into bit span
 * operations more efficient (interpreting it as `BoundedBitSpan` instead of just `BitSpan`).
 */
template<typename T> inline T to_best_bit_span(const T &data)
{
  static_assert(is_same_any_v<std::decay_t<T>,
                              BitSpan,
                              MutableBitSpan,
                              BoundedBitSpan,
                              MutableBoundedBitSpan>);
  return data;
}

template<typename... Args>
constexpr bool all_bounded_spans =
    (is_same_any_v<std::decay_t<Args>, BoundedBitSpan, MutableBoundedBitSpan> && ...);

std::ostream &operator<<(std::ostream &stream, const BitSpan &span);
std::ostream &operator<<(std::ostream &stream, const MutableBitSpan &span);

}  // namespace blender::bits

namespace blender {
using bits::BitSpan;
using bits::BoundedBitSpan;
using bits::MutableBitSpan;
using bits::MutableBoundedBitSpan;
}  // namespace blender
