/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_curves_utils.hh"

namespace blender::bke::curves {

void fill_curve_counts(const bke::CurvesGeometry &curves,
                       const Span<IndexRange> curve_ranges,
                       MutableSpan<int> counts)
{
  threading::parallel_for(curve_ranges.index_range(), 512, [&](IndexRange ranges_range) {
    for (const IndexRange curves_range : curve_ranges.slice(ranges_range)) {
      threading::parallel_for(curves_range, 4096, [&](IndexRange range) {
        for (const int i : range) {
          counts[i] = curves.points_for_curve(i).size();
        }
      });
    }
  });
}

void accumulate_counts_to_offsets(MutableSpan<int> counts_to_offsets, const int start_offset)
{
  int offset = start_offset;
  for (const int i : counts_to_offsets.index_range().drop_back(1)) {
    const int count = counts_to_offsets[i];
    BLI_assert(count > 0);
    counts_to_offsets[i] = offset;
    offset += count;
  }
  counts_to_offsets.last() = offset;
}

}  // namespace blender::bke::curves
