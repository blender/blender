/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

namespace blender::array_utils {

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

}  // namespace blender::array_utils
