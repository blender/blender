/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_bit_span.hh"
#include "BLI_math_bits.h"

namespace blender::bits {

namespace detail {

/**
 * Evaluates the expression on one or more bit spans and stores the result in the first.
 *
 * The expected type for the expression is:
 *   (BitInt ...one_or_more_args) -> BitInt
 */
template<typename ExprFn, typename FirstBitSpanT, typename... BitSpanT>
inline void mix_into_first_expr(ExprFn &&expr,
                                const FirstBitSpanT &first_arg,
                                const BitSpanT &...args)
{
  const int64_t size = first_arg.size();
  BLI_assert(((size == args.size()) && ...));
  if (size == 0) {
    return;
  }

  if constexpr (all_bounded_spans<FirstBitSpanT, BitSpanT...>) {
    BitInt *first_data = first_arg.data();
    const int64_t first_offset = first_arg.offset();
    const int64_t full_ints_num = first_arg.full_ints_num();
    /* Compute expression without any masking, all the spans are expected to be aligned to the
     * beginning of a #BitInt. */
    for (const int64_t i : IndexRange(full_ints_num)) {
      first_data[i] = expr(first_data[i], args.data()[i]...);
    }
    /* Compute expression for the remaining bits. */
    if (const int64_t final_bits = first_arg.final_bits_num()) {
      const BitInt result = expr(first_data[full_ints_num] >> first_offset,
                                 (args.data()[full_ints_num] >> args.offset())...);
      const BitInt mask = mask_range_bits(first_offset, final_bits);
      first_data[full_ints_num] = ((result << first_offset) & mask) |
                                  (first_data[full_ints_num] & ~mask);
    }
  }
  else {
    /* Fallback or arbitrary bit spans. This could be implemented more efficiently but adds more
     * complexity and is not necessary yet. */
    for (const int64_t i : IndexRange(size)) {
      const bool result = expr(BitInt(first_arg[i].test()), BitInt(args[i].test())...) != 0;
      first_arg[i].set(result);
    }
  }
}

/**
 * Evaluates the expression on one or more bit spans and returns true when the result contains a 1
 * anywhere.
 *
 * The expected type for the expression is:
 *   (BitInt ...one_or_more_args) -> BitInt
 */
template<typename ExprFn, typename FirstBitSpanT, typename... BitSpanT>
inline bool any_set_expr(ExprFn &&expr, const FirstBitSpanT &first_arg, const BitSpanT &...args)
{
  const int64_t size = first_arg.size();
  BLI_assert(((size == args.size()) && ...));
  if (size == 0) {
    return false;
  }

  if constexpr (all_bounded_spans<FirstBitSpanT, BitSpanT...>) {
    const BitInt *first_data = first_arg.data();
    const int64_t full_ints_num = first_arg.full_ints_num();
    /* Compute expression without any masking, all the spans are expected to be aligned to the
     * beginning of a #BitInt. */
    for (const int64_t i : IndexRange(full_ints_num)) {
      if (expr(first_data[i], args.data()[i]...) != 0) {
        return true;
      }
    }
    /* Compute expression for the remaining bits. */
    if (const int64_t final_bits = first_arg.final_bits_num()) {
      const BitInt result = expr(first_data[full_ints_num] >> first_arg.offset(),
                                 (args.data()[full_ints_num] >> args.offset())...);
      const BitInt mask = mask_first_n_bits(final_bits);
      if ((result & mask) != 0) {
        return true;
      }
    }
    return false;
  }
  else {
    /* Fallback or arbitrary bit spans. This could be implemented more efficiently but adds more
     * complexity and is not necessary yet. */
    for (const int64_t i : IndexRange(size)) {
      const BitInt result = expr(BitInt(first_arg[i].test()), BitInt(args[i].test())...);
      if (result != 0) {
        return true;
      }
    }
    return false;
  }
}

/**
 * Evaluates the expression on one or more bit spans and calls the `handle` function for each bit
 * index where the result is 1.
 *
 * The expected type for the expression is:
 *   (BitInt ...one_or_more_args) -> BitInt
 */
template<typename ExprFn, typename HandleFn, typename FirstBitSpanT, typename... BitSpanT>
inline void foreach_1_index_expr(ExprFn &&expr,
                                 HandleFn &&handle,
                                 const FirstBitSpanT &first_arg,
                                 const BitSpanT &...args)
{
  const int64_t size = first_arg.size();
  BLI_assert(((size == args.size()) && ...));
  if (size == 0) {
    return;
  }

  if constexpr (all_bounded_spans<FirstBitSpanT, BitSpanT...>) {
    const BitInt *first_data = first_arg.data();
    const int64_t full_ints_num = first_arg.full_ints_num();
    /* Iterate over full ints without any bit masks. */
    for (const int64_t int_i : IndexRange(full_ints_num)) {
      BitInt tmp = expr(first_data[int_i], args.data()[int_i]...);
      const int64_t offset = int_i << BitToIntIndexShift;
      while (tmp != 0) {
        static_assert(std::is_same_v<BitInt, uint64_t>);
        const int index = bitscan_forward_uint64(tmp);
        handle(index + offset);
        tmp &= ~mask_single_bit(index);
      }
    }
    /* Iterate over remaining bits. */
    if (const int64_t final_bits = first_arg.final_bits_num()) {
      BitInt tmp = expr(first_data[full_ints_num] >> first_arg.offset(),
                        (args.data()[full_ints_num] >> args.offset())...) &
                   mask_first_n_bits(final_bits);
      const int64_t offset = full_ints_num << BitToIntIndexShift;
      while (tmp != 0) {
        static_assert(std::is_same_v<BitInt, uint64_t>);
        const int index = bitscan_forward_uint64(tmp);
        handle(index + offset);
        tmp &= ~mask_single_bit(index);
      }
    }
  }
  else {
    /* Fallback or arbitrary bit spans. This could be implemented more efficiently but adds more
     * complexity and is not necessary yet. */
    for (const int64_t i : IndexRange(size)) {
      const BitInt result = expr(BitInt(first_arg[i].test()), BitInt(args[i].test())...);
      if (result) {
        handle(i);
      }
    }
  }
}

}  // namespace detail

template<typename ExprFn, typename FirstBitSpanT, typename... BitSpanT>
inline void mix_into_first_expr(ExprFn &&expr,
                                const FirstBitSpanT &first_arg,
                                const BitSpanT &...args)
{
  detail::mix_into_first_expr(expr, to_best_bit_span(first_arg), to_best_bit_span(args)...);
}

template<typename ExprFn, typename FirstBitSpanT, typename... BitSpanT>
inline bool any_set_expr(ExprFn &&expr, const FirstBitSpanT &first_arg, const BitSpanT &...args)
{
  return detail::any_set_expr(expr, to_best_bit_span(first_arg), to_best_bit_span(args)...);
}

template<typename ExprFn, typename HandleFn, typename FirstBitSpanT, typename... BitSpanT>
inline void foreach_1_index_expr(ExprFn &&expr,
                                 HandleFn &&handle,
                                 const FirstBitSpanT &first_arg,
                                 const BitSpanT &...args)
{
  detail::foreach_1_index_expr(
      expr, handle, to_best_bit_span(first_arg), to_best_bit_span(args)...);
}

template<typename BitSpanT> inline void invert(const BitSpanT &data)
{
  mix_into_first_expr([](const BitInt x) { return ~x; }, data);
}

template<typename FirstBitSpanT, typename... BitSpanT>
inline void inplace_or(FirstBitSpanT &first_arg, const BitSpanT &...args)
{
  mix_into_first_expr([](const auto... x) { return (x | ...); }, first_arg, args...);
}

template<typename FirstBitSpanT, typename MaskBitSpanT, typename... BitSpanT>
inline void inplace_or_masked(FirstBitSpanT &first_arg,
                              const MaskBitSpanT &mask,
                              const BitSpanT &...args)
{
  mix_into_first_expr(
      [](const BitInt a, const BitInt mask, const auto... x) { return a | ((x | ...) & mask); },
      first_arg,
      mask,
      args...);
}

template<typename FirstBitSpanT, typename... BitSpanT>
inline void copy_from_or(FirstBitSpanT &first_arg, const BitSpanT &...args)
{
  mix_into_first_expr(
      [](auto /*first*/, auto... rest) { return (rest | ...); }, first_arg, args...);
}

template<typename FirstBitSpanT, typename... BitSpanT>
inline void inplace_and(FirstBitSpanT &first_arg, const BitSpanT &...args)
{
  mix_into_first_expr([](const auto... x) { return (x & ...); }, first_arg, args...);
}

template<typename... BitSpanT>
inline void operator|=(MutableBitSpan first_arg, const BitSpanT &...args)
{
  inplace_or(first_arg, args...);
}

template<typename... BitSpanT>
inline void operator|=(MutableBoundedBitSpan first_arg, const BitSpanT &...args)
{
  inplace_or(first_arg, args...);
}

template<typename... BitSpanT>
inline void operator&=(MutableBitSpan first_arg, const BitSpanT &...args)
{
  inplace_and(first_arg, args...);
}

template<typename... BitSpanT>
inline void operator&=(MutableBoundedBitSpan first_arg, const BitSpanT &...args)
{
  inplace_and(first_arg, args...);
}

template<typename... BitSpanT> inline bool has_common_set_bits(const BitSpanT &...args)
{
  return any_set_expr([](const auto... x) { return (x & ...); }, args...);
}

template<typename BitSpanT> inline bool any_bit_set(const BitSpanT &arg)
{
  return has_common_set_bits(arg);
}

template<typename... BitSpanT> inline bool has_common_unset_bits(const BitSpanT &...args)
{
  return any_set_expr([](const auto... x) { return ~(x | ...); }, args...);
}

template<typename BitSpanT> inline bool any_bit_unset(const BitSpanT &arg)
{
  return has_common_unset_bits(arg);
}

template<typename BitSpanT, typename Fn> inline void foreach_1_index(const BitSpanT &data, Fn &&fn)
{
  foreach_1_index_expr([](const BitInt x) { return x; }, fn, data);
}

template<typename BitSpanT, typename Fn> inline void foreach_0_index(const BitSpanT &data, Fn &&fn)
{
  foreach_1_index_expr([](const BitInt x) { return ~x; }, fn, data);
}

template<typename BitSpanT1, typename BitSpanT2>
inline bool spans_equal(const BitSpanT1 &a, const BitSpanT2 &b)
{
  if (a.size() != b.size()) {
    return false;
  }
  return !any_set_expr([](const BitInt a, const BitInt b) { return a ^ b; }, a, b);
}

template<typename BitSpanT1, typename BitSpanT2, typename BitSpanT3>
inline bool spans_equal_masked(const BitSpanT1 &a, const BitSpanT2 &b, const BitSpanT3 &mask)
{
  BLI_assert(mask.size() == a.size());
  BLI_assert(mask.size() == b.size());
  return !bits::any_set_expr(
      [](const BitInt a, const BitInt b, const BitInt mask) { return (a ^ b) & mask; },
      a,
      b,
      mask);
}

}  // namespace blender::bits
