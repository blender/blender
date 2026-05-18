/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_kdtree.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_pointcloud.hh"

#include "GEO_point_merge.hh"
#include "GEO_randomize.hh"

#include "atomic_ops.h"

namespace blender::geometry {

PointCloud *merge_points(const PointCloud &src_points,
                         const IndexMask &selection,
                         const Span<int> merge_ids,
                         const bke::AttributeFilter &attribute_filter)
{
  VectorSet<int> group_indices;
  selection.foreach_index_optimized<int32_t>(
      [&](const int i) { group_indices.add(merge_ids[i]); });
  const int groups_num = group_indices.size();

  Array<int> group_sizes(group_indices.size() + 1, 0);
  selection.foreach_index_optimized<int>(
      [&](const int i) {
        const int group_i = group_indices.index_of(merge_ids[i]);
        atomic_add_and_fetch_int32(&group_sizes[group_i], 1);
      },
      exec_mode::grain_size(8192));
  BLI_assert(!group_sizes.as_span().drop_back(1).contains(0));

  const OffsetIndices<int> group_offsets = offset_indices::accumulate_counts_to_offsets(
      group_sizes);

  Array<int> all_group_indices(group_offsets.total_size());
  Array<int> group_counts(groups_num, 0);
  selection.foreach_index_optimized<int>(
      [&](const int i) {
        const int group_i = group_indices.index_of(merge_ids[i]);
        const int index_in_group = atomic_fetch_and_add_int32(&group_counts[group_i], 1);
        all_group_indices[group_offsets[group_i][index_in_group]] = int(i);
      },
      exec_mode::grain_size(8192));
  group_counts = {};
  offset_indices::sort_groups(group_offsets, all_group_indices);
  const GroupedSpan<int> indices_by_group(group_offsets, all_group_indices);

  IndexMaskMemory memory;
  const IndexMask unselected = selection.complement(IndexMask(src_points.totpoint), memory);

  PointCloud *dst_pointcloud = BKE_pointcloud_new_nomain(unselected.size() + groups_num);
  bke::MutableAttributeAccessor dst_attributes = dst_pointcloud->attributes_for_write();

  src_points.attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    bke::GAttributeReader src = iter.get();
    const CommonVArrayInfo info = src.varray.common_info();
    if (info.type == CommonVArrayInfo::Type::Single) {
      const bke::AttributeInitValue init(GPointer(src.varray.type(), info.data));
      if (dst_attributes.add(iter.name, bke::AttrDomain::Point, iter.data_type, init)) {
        return;
      }
    }
    const GVArraySpan src_span(*src);
    bke::GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, bke::AttrDomain::Point, iter.data_type);
    array_utils::gather(src_span, unselected, dst_attribute.span.take_front(unselected.size()));
    if (iter.name == "id" && iter.data_type == bke::AttrType::Int32) {
      const Span<int> src_typed = src_span.typed<int>();
      MutableSpan<int> dst_typed = dst_attribute.span.typed<int>().take_back(groups_num);
      threading::parallel_for(dst_typed.index_range(), 4096, [&](const IndexRange range) {
        for (const int i : range) {
          dst_typed[i] = src_typed[indices_by_group[i].first()];
        }
      });
    }
    else {
      bke::attribute_math::mix_groups(
          src_span, indices_by_group, dst_attribute.span.take_back(groups_num));
    }
    dst_attribute.finish();
  });

  debug_randomize_point_order(dst_pointcloud);

  return dst_pointcloud;
}

PointCloud *point_merge_by_distance(const PointCloud &src_points,
                                    const float merge_distance,
                                    const IndexMask &selection,
                                    const bke::AttributeFilter &attribute_filter)
{
  const Span<float3> positions = src_points.positions();
  KDTree<float3> *tree = kdtree_new<float3>(selection.size());
  selection.foreach_index_optimized<int64_t>(
      [&](const int64_t i) { kdtree_insert<float3>(tree, i, positions[i]); });
  kdtree_balance<float3>(tree);
  Array<int> root_indices(src_points.totpoint, -1);
  kdtree_calc_duplicates_fast<float3>(tree, merge_distance, false, root_indices.data());
  kdtree_free<float3>(tree);
  threading::parallel_for(root_indices.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      if (root_indices[i] == -1) {
        root_indices[i] = i;
      }
    }
  });
  return merge_points(src_points, selection, root_indices.as_span(), attribute_filter);
}

}  // namespace blender::geometry
