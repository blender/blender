/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_offset_indices.hh"
#include "BLI_task.hh"

namespace blender::offset_indices {

void accumulate_counts_to_offsets(MutableSpan<int> counts_to_offsets, const int start_offset)
{
  int offset = start_offset;
  for (const int i : counts_to_offsets.index_range().drop_back(1)) {
    const int count = counts_to_offsets[i];
    BLI_assert(count >= 0);
    counts_to_offsets[i] = offset;
    offset += count;
  }
  counts_to_offsets.last() = offset;
}

void build_reverse_map(OffsetIndices<int> offsets, MutableSpan<int> r_map)
{
  threading::parallel_for(offsets.index_range(), 1024, [&](const IndexRange range) {
    for (const int64_t i : range) {
      r_map.slice(offsets[i]).fill(i);
    }
  });
}

}  // namespace blender::offset_indices
