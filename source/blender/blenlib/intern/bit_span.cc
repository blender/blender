/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bit_span.hh"

namespace blender::bits {

void MutableBitSpan::set_all()
{
  const AlignedIndexRanges ranges = split_index_range_by_alignment(bit_range_, BitsPerInt);
  {
    BitInt &first_int = *int_containing_bit(data_, bit_range_.start());
    const BitInt first_int_mask = mask_range_bits(ranges.prefix.start() & BitIndexMask,
                                                  ranges.prefix.size());
    first_int |= first_int_mask;
  }
  {
    BitInt *start = int_containing_bit(data_, ranges.aligned.start());
    const int64_t ints_to_fill = ranges.aligned.size() / BitsPerInt;
    constexpr BitInt fill_value = BitInt(-1);
    initialized_fill_n(start, ints_to_fill, fill_value);
  }
  {
    BitInt &last_int = *int_containing_bit(data_, bit_range_.one_after_last() - 1);
    const BitInt last_int_mask = mask_first_n_bits(ranges.suffix.size());
    last_int |= last_int_mask;
  }
}

void MutableBitSpan::reset_all()
{
  const AlignedIndexRanges ranges = split_index_range_by_alignment(bit_range_, BitsPerInt);
  {
    BitInt &first_int = *int_containing_bit(data_, bit_range_.start());
    const BitInt first_int_mask = mask_range_bits(ranges.prefix.start() & BitIndexMask,
                                                  ranges.prefix.size());
    first_int &= ~first_int_mask;
  }
  {
    BitInt *start = int_containing_bit(data_, ranges.aligned.start());
    const int64_t ints_to_fill = ranges.aligned.size() / BitsPerInt;
    constexpr BitInt fill_value = 0;
    initialized_fill_n(start, ints_to_fill, fill_value);
  }
  {
    BitInt &last_int = *int_containing_bit(data_, bit_range_.one_after_last() - 1);
    const BitInt last_int_mask = mask_first_n_bits(ranges.suffix.size());
    last_int &= ~last_int_mask;
  }
}

std::ostream &operator<<(std::ostream &stream, const BitSpan &span)
{
  stream << "(Size: " << span.size() << ", ";
  for (const BitRef bit : span) {
    stream << bit;
  }
  stream << ")";
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const MutableBitSpan &span)
{
  return stream << BitSpan(span);
}

}  // namespace blender::bits
