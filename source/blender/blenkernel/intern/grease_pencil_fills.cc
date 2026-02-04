/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_map.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_grease_pencil_fills.hh"

namespace blender::bke::greasepencil {

std::optional<FillCache> fill_cache_from_fill_ids(const VArray<int> &fill_ids)
{
  if (!fill_ids || fill_ids.is_empty()) {
    return std::nullopt;
  }

  /* The size of each fill. */
  Vector<int> fill_sizes;
  /* Contains the curve indices for each fill. */
  Vector<Vector<int>> curve_indices_by_fill;
  /* Maps the fill id to the index in the #curve_indices_by_fill vector. */
  Map<int, int> fill_indexing;

  for (const int curve : fill_ids.index_range()) {
    const int fill_id = fill_ids[curve];

    /* Unfilled curves are skipped. */
    if (fill_id == 0) {
      continue;
    }

    /* Try adding new fill id to the map or update existing fill id info. */
    fill_indexing.add_or_modify(
        fill_id,
        [&](int *value) {
          *value = fill_sizes.size();
          fill_sizes.append(1);
          curve_indices_by_fill.append(Vector<int>({curve}));
        },
        [&](int *value) {
          const int fill_index = *value;
          fill_sizes[fill_index]++;
          curve_indices_by_fill[fill_index].append(curve);
        });
  }

  if (fill_sizes.is_empty()) {
    return std::nullopt;
  }

  fill_sizes.append(0);
  OffsetIndices<int> fill_offsets = offset_indices::accumulate_counts_to_offsets(fill_sizes);

  Vector<int> fill_map(fill_offsets.total_size());
  threading::parallel_for(fill_offsets.index_range(), 4096, [&](const IndexRange range) {
    for (const int fill_i : range) {
      const IndexRange fill_range = fill_offsets[fill_i];
      const Span<int> curve_indices = curve_indices_by_fill[fill_i].as_span();
      fill_map.as_mutable_span().slice(fill_range).copy_from(curve_indices);
    }
  });

  FillCache fill_cache;
  fill_cache.fill_map = std::move(fill_map);
  fill_cache.fill_offsets = std::move(fill_sizes);

  return fill_cache;
}

int get_next_available_fill_id(const VArray<int> &fill_ids)
{
  if (std::optional<int64_t> max_i = array_utils::max_element_index(fill_ids)) {
    const int max_fill_id = fill_ids[*max_i];
    /* Make sure the fill ID is greater than zero. This avoids the issue of hitting an invalid fill
     * ID of zero when creating multiple IDs at once. */
    return max_fill_id <= 0 ? 1 : max_fill_id + 1;
  }
  return 1;
}

void gather_next_available_fill_ids(const VArray<int> &fill_ids, MutableSpan<int> r_new_fill_ids)
{
  const int next_fill_id = get_next_available_fill_id(fill_ids);
  array_utils::fill_index_range(r_new_fill_ids, next_fill_id);
}

void gather_next_available_fill_ids(const VArray<int> &fill_ids,
                                    const IndexMask &curve_mask,
                                    MutableSpan<int> r_new_fill_ids)
{
  const int next_fill_id = get_next_available_fill_id(fill_ids);
  curve_mask.foreach_index(GrainSize(1024), [&](const int index, const int pos) {
    r_new_fill_ids[index] = next_fill_id + pos;
  });
}

IndexMask selected_mask_to_fills(const IndexMask &selected_mask,
                                 const CurvesGeometry &curves,
                                 const AttrDomain domain,
                                 IndexMaskMemory &memory)
{
  const AttributeAccessor attributes = curves.attributes();
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<int> fill_ids = *attributes.lookup<int>("fill_id", AttrDomain::Curve);

  /* If the attribute does not exist then each curves is its own fill. */
  if (!fill_ids) {
    if (domain == AttrDomain::Curve) {
      return selected_mask;
    }
    BLI_assert(domain == AttrDomain::Point);

    Array<bool> selected_points(curves.points_num());
    selected_mask.to_bools(selected_points);

    const IndexMask selected_curves = IndexMask::from_predicate(
        curves.curves_range(), GrainSize(512), memory, [&](const int curve_i) {
          const IndexRange points = points_by_curve[curve_i];
          const Span<bool> selected_curve_points = selected_points.as_span().slice(points);
          return std::any_of(selected_curve_points.begin(),
                             selected_curve_points.end(),
                             [](const bool value) { return value; });
        });

    return curves::curve_to_point_selection(points_by_curve, selected_curves, memory);
  }

  VectorSet<int> selected_fill_ids;
  Array<bool> src_selected_curves(curves.curves_num());

  if (domain == AttrDomain::Point) {
    Array<bool> selected_points(curves.points_num());
    selected_mask.to_bools(selected_points);

    const IndexMask selected_curves = IndexMask::from_predicate(
        curves.curves_range(), GrainSize(512), memory, [&](const int curve_i) {
          const IndexRange points = points_by_curve[curve_i];
          const Span<bool> selected_curve_points = selected_points.as_span().slice(points);
          return std::any_of(selected_curve_points.begin(),
                             selected_curve_points.end(),
                             [](const bool value) { return value; });
        });

    selected_curves.foreach_index([&](const int64_t curve_i) {
      const int fill_id = fill_ids[curve_i];
      if (fill_id != 0) {
        selected_fill_ids.add(fill_id);
      }
    });
    selected_curves.to_bools(src_selected_curves);
  }
  else {
    selected_mask.foreach_index([&](const int64_t curve_i) {
      const int fill_id = fill_ids[curve_i];
      if (fill_id != 0) {
        selected_fill_ids.add(fill_id);
      }
    });
    selected_mask.to_bools(src_selected_curves);
  }

  const IndexMask selected_curves = IndexMask::from_predicate(
      curves.curves_range(), GrainSize(4096), memory, [&](const int64_t curve_i) {
        const int fill_id = fill_ids[curve_i];
        if (fill_id == 0) {
          return src_selected_curves[curve_i];
        }
        return selected_fill_ids.contains(fill_id);
      });

  if (domain == AttrDomain::Curve) {
    return selected_curves;
  }
  BLI_assert(domain == AttrDomain::Point);

  return curves::curve_to_point_selection(curves.points_by_curve(), selected_curves, memory);
}

void separate_fill_ids(CurvesGeometry &curves, const IndexMask &strokes_to_keep)
{
  IndexMaskMemory memory;
  const IndexMask strokes_to_change = strokes_to_keep.complement(curves.curves_range(), memory);

  if (strokes_to_change.is_empty() || strokes_to_keep.is_empty()) {
    return;
  }

  MutableAttributeAccessor attributes = curves.attributes_for_write();
  SpanAttributeWriter<int> fill_ids = attributes.lookup_for_write_span<int>("fill_id");

  if (!fill_ids) {
    return;
  }

  int max_id = 0;
  strokes_to_keep.foreach_index(
      [&](const int curve_i) { max_id = math::max(max_id, fill_ids.span[curve_i]); });

  if (max_id == 0) {
    return;
  }

  VectorSet<int> fill_indexing;
  strokes_to_change.foreach_index(
      [&](const int curve_i) { fill_indexing.add(fill_ids.span[curve_i]); });

  strokes_to_change.foreach_index(GrainSize(1024), [&](const int curve_i) {
    if (fill_ids.span[curve_i] == 0) {
      return;
    }
    fill_ids.span[curve_i] = fill_indexing.index_of(fill_ids.span[curve_i]) + max_id + 1;
  });

  fill_ids.finish();
}

}  // namespace blender::bke::greasepencil
