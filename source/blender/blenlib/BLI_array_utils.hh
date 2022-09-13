/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_generic_span.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_task.hh"

namespace blender::array_utils {

/**
 * Fill the destination span by copying masked values from the src array. Threaded based on
 * grainsize.
 */
void copy(const GVArray &src, IndexMask selection, GMutableSpan dst, int64_t grain_size = 4096);

/**
 * Fill the destination span by copying values from the src array. Threaded based on
 * grainsize.
 */
template<typename T>
inline void copy(const Span<T> src,
                 const IndexMask selection,
                 MutableSpan<T> dst,
                 const int64_t grain_size = 4096)
{
  threading::parallel_for(selection.index_range(), grain_size, [&](const IndexRange range) {
    for (const int64_t index : selection.slice(range)) {
      dst[index] = src[index];
    }
  });
}

}  // namespace blender::array_utils
