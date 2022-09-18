/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_task.hh"

namespace blender::array_utils {

void copy(const GVArray &src,
          const IndexMask selection,
          GMutableSpan dst,
          const int64_t grain_size)
{
  BLI_assert(src.type() == dst.type());
  BLI_assert(src.size() == dst.size());
  threading::parallel_for(selection.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_to_uninitialized(selection.slice(range), dst.data());
  });
}

}  // namespace blender::array_utils
