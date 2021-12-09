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
