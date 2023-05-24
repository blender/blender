/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

namespace blender::array_utils {

void copy(const GVArray &src, GMutableSpan dst, const int64_t grain_size)
{
  BLI_assert(src.type() == dst.type());
  BLI_assert(src.size() == dst.size());
  threading::parallel_for(src.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_to_uninitialized(range, dst.data());
  });
}

void copy(const GVArray &src,
          const IndexMask selection,
          GMutableSpan dst,
          const int64_t grain_size)
{
  BLI_assert(src.type() == dst.type());
  BLI_assert(src.size() >= selection.min_array_size());
  BLI_assert(dst.size() >= selection.min_array_size());
  threading::parallel_for(selection.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_to_uninitialized(selection.slice(range), dst.data());
  });
}

void gather(const GVArray &src,
            const IndexMask indices,
            GMutableSpan dst,
            const int64_t grain_size)
{
  BLI_assert(src.type() == dst.type());
  BLI_assert(indices.size() == dst.size());
  threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_compressed_to_uninitialized(indices.slice(range), dst.slice(range).data());
  });
}

void gather(const GSpan src, const IndexMask indices, GMutableSpan dst, const int64_t grain_size)
{
  gather(GVArray::ForSpan(src), indices, dst, grain_size);
}

void invert_booleans(MutableSpan<bool> span)
{
  threading::parallel_for(span.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      span[i] = !span[i];
    }
  });
}

BooleanMix booleans_mix_calc(const VArray<bool> &varray, const IndexRange range_to_check)
{
  if (varray.is_empty()) {
    return BooleanMix::None;
  }
  const CommonVArrayInfo info = varray.common_info();
  if (info.type == CommonVArrayInfo::Type::Single) {
    return *static_cast<const bool *>(info.data) ? BooleanMix::AllTrue : BooleanMix::AllFalse;
  }
  if (info.type == CommonVArrayInfo::Type::Span) {
    const Span<bool> span(static_cast<const bool *>(info.data), varray.size());
    return threading::parallel_reduce(
        range_to_check,
        4096,
        BooleanMix::None,
        [&](const IndexRange range, const BooleanMix init) {
          if (init == BooleanMix::Mixed) {
            return init;
          }

          const Span<bool> slice = span.slice(range);
          const bool first = slice.first();
          for (const bool value : slice.drop_front(1)) {
            if (value != first) {
              return BooleanMix::Mixed;
            }
          }
          return first ? BooleanMix::AllTrue : BooleanMix::AllFalse;
        },
        [&](BooleanMix a, BooleanMix b) { return (a == b) ? a : BooleanMix::Mixed; });
  }
  return threading::parallel_reduce(
      range_to_check,
      2048,
      BooleanMix::None,
      [&](const IndexRange range, const BooleanMix init) {
        if (init == BooleanMix::Mixed) {
          return init;
        }
        /* Alternatively, this could use #materialize to retrieve many values at once. */
        const bool first = varray[range.first()];
        for (const int64_t i : range.drop_front(1)) {
          if (varray[i] != first) {
            return BooleanMix::Mixed;
          }
        }
        return first ? BooleanMix::AllTrue : BooleanMix::AllFalse;
      },
      [&](BooleanMix a, BooleanMix b) { return (a == b) ? a : BooleanMix::Mixed; });
}

}  // namespace blender::array_utils
