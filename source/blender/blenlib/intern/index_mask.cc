/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_index_mask.hh"

namespace blender {

IndexMask IndexMask::slice(IndexRange slice) const
{
  return IndexMask(indices_.slice(slice));
}

/**
 * Create a sub-mask that is also shifted to the beginning. The shifting to the beginning allows
 * code to work with smaller indices, which is more memory efficient.
 *
 * \return New index mask with the size of #slice. It is either empty or starts with 0. It might
 * reference indices that have been appended to #r_new_indices.
 *
 * Example:
 *  this:   [2, 3, 5, 7, 8, 9, 10]
 *  slice:      ^--------^
 *  output: [0, 2, 4, 5]
 *
 *  All the indices in the sub-mask are shifted by 3 towards zero, so that the first index in the
 *  output is zero.
 */
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
