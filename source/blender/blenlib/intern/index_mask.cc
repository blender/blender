/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_mask.hh"

namespace blender {

IndexMask IndexMask::slice(IndexRange slice) const
{
  return IndexMask(indices_.slice(slice));
}

IndexMask IndexMask::slice_and_offset(const IndexRange slice, Vector<int64_t> &r_new_indices) const
{
  const int slice_size = slice.size();
  if (slice_size == 0) {
    return {};
  }
  IndexMask sliced_mask{indices_.slice(slice)};
  if (sliced_mask.is_range()) {
    return IndexMask(slice_size);
  }
  const int64_t offset = sliced_mask.indices().first();
  if (offset == 0) {
    return sliced_mask;
  }
  r_new_indices.resize(slice_size);
  for (const int i : IndexRange(slice_size)) {
    r_new_indices[i] = sliced_mask[i] - offset;
  }
  return IndexMask(r_new_indices.as_span());
}

}  // namespace blender
