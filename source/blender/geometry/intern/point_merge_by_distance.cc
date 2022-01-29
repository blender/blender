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

#include "BLI_kdtree.h"
#include "BLI_task.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_geometry_set.hh"
#include "BKE_pointcloud.h"

#include "GEO_point_merge_by_distance.hh"

namespace blender::geometry {

PointCloud *point_merge_by_distance(const PointCloudComponent &src_points,
                                    const float merge_distance,
                                    const IndexMask selection)
{
  const PointCloud &src_pointcloud = *src_points.get_for_read();
  const int src_size = src_pointcloud.totpoint;
  Span<float3> positions{reinterpret_cast<float3 *>(src_pointcloud.co), src_size};

  /* Create the KD tree based on only the selected points, to speed up merge detection and
   * balancing. */
  KDTree_3d *tree = BLI_kdtree_3d_new(selection.size());
  for (const int i : selection.index_range()) {
    BLI_kdtree_3d_insert(tree, i, positions[selection[i]]);
  }
  BLI_kdtree_3d_balance(tree);

  /* Find the duplicates in the KD tree. Because the tree only contains the selected points, the
   * resulting indices are indices into the selection, rather than indices of the source point
   * cloud. */
  Array<int> selection_merge_indices(selection.size(), -1);
  const int duplicate_count = BLI_kdtree_3d_calc_duplicates_fast(
      tree, merge_distance, false, selection_merge_indices.data());
  BLI_kdtree_3d_free(tree);

  /* Create the new point cloud and add it to a temporary component for the attribute API. */
  const int dst_size = src_size - duplicate_count;
  PointCloud *dst_pointcloud = BKE_pointcloud_new_nomain(dst_size);
  PointCloudComponent dst_points;
  dst_points.replace(dst_pointcloud, GeometryOwnershipType::Editable);

  /* By default, every point is just "merged" with itself. Then fill in the results of the merge
   * finding, converting from indices into the selection to indices into the full input point
   * cloud. */
  Array<int> merge_indices(src_size);
  for (const int i : merge_indices.index_range()) {
    merge_indices[i] = i;
  }
  for (const int i : selection_merge_indices.index_range()) {
    const int merge_index = selection_merge_indices[i];
    if (merge_index != -1) {
      const int src_merge_index = selection[merge_index];
      const int src_index = selection[i];
      merge_indices[src_index] = src_merge_index;
    }
  }

  /* For every source index, find the corresponding index in the result by iterating through the
   * source indices and counting how many merges happened before that point. */
  int merged_points = 0;
  Array<int> src_to_dst_indices(src_size);
  for (const int i : IndexRange(src_size)) {
    src_to_dst_indices[i] = i - merged_points;
    if (merge_indices[i] != i) {
      merged_points++;
    }
  }

  /* In order to use a contiguous array as the storage for every destination point's source
   * indices, first the number of source points must be counted for every result point. */
  Array<int> point_merge_counts(dst_size, 0);
  for (const int i : IndexRange(src_size)) {
    const int merge_index = merge_indices[i];
    const int dst_index = src_to_dst_indices[merge_index];
    point_merge_counts[dst_index]++;
  }

  /* This array stores an offset into `merge_map` for every result point. */
  Array<int> map_offsets(dst_size + 1);
  int offset = 0;
  for (const int i : IndexRange(dst_size)) {
    map_offsets[i] = offset;
    offset += point_merge_counts[i];
  }
  map_offsets.last() = offset;

  point_merge_counts.fill(0);

  /* This array stores all of the source indices for every result point. The size is the source
   * size because every input point is either merged with another or copied directly. */
  Array<int> merge_map(src_size);
  for (const int i : IndexRange(src_size)) {
    const int merge_index = merge_indices[i];
    const int dst_index = src_to_dst_indices[merge_index];

    const IndexRange point_range(map_offsets[dst_index],
                                 map_offsets[dst_index + 1] - map_offsets[dst_index]);
    MutableSpan<int> point_merge_indices = merge_map.as_mutable_span().slice(point_range);
    point_merge_indices[point_merge_counts[dst_index]] = i;
    point_merge_counts[dst_index]++;
  }

  Set<bke::AttributeIDRef> attributes = src_points.attribute_ids();

  /* Transfer the ID attribute if it exists, using the ID of the first merged point. */
  if (attributes.contains("id")) {
    VArray<int> src = src_points.attribute_get_for_read<int>("id", ATTR_DOMAIN_POINT, 0);
    bke::OutputAttribute_Typed<int> dst = dst_points.attribute_try_get_for_output_only<int>(
        "id", ATTR_DOMAIN_POINT);
    Span<int> src_ids = src.get_internal_span();
    MutableSpan<int> dst_ids = dst.as_span();

    threading::parallel_for(IndexRange(dst_size), 1024, [&](IndexRange range) {
      for (const int i_dst : range) {
        const IndexRange point_range(map_offsets[i_dst],
                                     map_offsets[i_dst + 1] - map_offsets[i_dst]);
        dst_ids[i_dst] = src_ids[point_range.first()];
      }
    });

    dst.save();
    attributes.remove_contained("id");
  }

  /* Transfer all other attributes. */
  for (const bke::AttributeIDRef &id : attributes) {
    if (!id.should_be_kept()) {
      continue;
    }

    bke::ReadAttributeLookup src_attribute = src_points.attribute_try_get_for_read(id);
    attribute_math::convert_to_static_type(src_attribute.varray.type(), [&](auto dummy) {
      using T = decltype(dummy);
      if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
        bke::OutputAttribute_Typed<T> dst_attribute =
            dst_points.attribute_try_get_for_output_only<T>(id, ATTR_DOMAIN_POINT);
        Span<T> src = src_attribute.varray.get_internal_span().typed<T>();
        MutableSpan<T> dst = dst_attribute.as_span();

        threading::parallel_for(IndexRange(dst_size), 1024, [&](IndexRange range) {
          for (const int i_dst : range) {
            /* Create a separate mixer for every point to avoid allocating temporary buffers
             * in the mixer the size of the result point cloud and to improve memory locality. */
            attribute_math::DefaultMixer<T> mixer{dst.slice(i_dst, 1)};

            const IndexRange point_range(map_offsets[i_dst],
                                         map_offsets[i_dst + 1] - map_offsets[i_dst]);
            Span<int> src_merge_indices = merge_map.as_span().slice(point_range);
            for (const int i_src : src_merge_indices) {
              mixer.mix_in(0, src[i_src]);
            }

            mixer.finalize();
          }
        });

        dst_attribute.save();
      }
    });
  }

  return dst_pointcloud;
}

}  // namespace blender::geometry
