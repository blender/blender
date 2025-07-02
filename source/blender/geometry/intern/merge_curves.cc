/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_stack.hh"

#include "BKE_curves_utils.hh"
#include "BKE_deform.hh"

#include "GEO_merge_curves.hh"

namespace blender::geometry {

enum Flag {
  OnStack = 1,
  Inserted = 2,
};

template<typename Fn>
static void foreach_connected_curve(const Span<int> connect_to_curve,
                                    MutableSpan<uint8_t> flags,
                                    const int start,
                                    Fn fn)
{
  const IndexRange range = connect_to_curve.index_range();

  Stack<int> stack;

  bool has_cycle = false;
  auto push_curve = [&](const int curve_i) -> bool {
    if ((flags[curve_i] & Inserted) != 0) {
      return false;
    }
    if ((flags[curve_i] & OnStack) != 0) {
      has_cycle = true;
      return false;
    }
    stack.push(curve_i);
    flags[curve_i] |= OnStack;
    fn(curve_i);
    return true;
  };

  push_curve(start);

  while (!stack.is_empty()) {
    const int current = stack.peek();

    const int next = connect_to_curve[current];
    if (range.contains(next)) {
      if (push_curve(next)) {
        continue;
      }
    }

    flags[current] |= Inserted;
    stack.pop();
  }

  UNUSED_VARS(has_cycle);
}

/* Topological sorting that puts connected curves into contiguous ranges. */
static Vector<int> toposort_connected_curves(const Span<int> connect_to_curve)
{
  const IndexRange range = connect_to_curve.index_range();

  Array<uint8_t> flags(connect_to_curve.size());

  /* First add all open chains by finding curves without a connection. */
  Array<bool> is_start_curve(range.size(), true);
  for (const int curve_i : range) {
    const int next = connect_to_curve[curve_i];
    if (range.contains(next)) {
      is_start_curve[next] = false;
    }
  }
  /* Mark all curves that can be reached from a start curve. These must not be added before the
   * start curve, or it can lead to gaps in curve ranges. */
  flags.fill(0);
  Array<bool> is_reachable(range.size(), false);
  for (const int curve_i : range) {
    if (is_start_curve[curve_i]) {
      foreach_connected_curve(
          connect_to_curve, flags, curve_i, [&](const int index) { is_reachable[index] = true; });
    }
  }

  Vector<int> sorted_curves;
  sorted_curves.reserve(connect_to_curve.size());

  flags.fill(0);
  for (const int curve_i : range) {
    if (is_start_curve[curve_i] || !is_reachable[curve_i]) {
      foreach_connected_curve(
          connect_to_curve, flags, curve_i, [&](const int index) { sorted_curves.append(index); });
    }
  }

  BLI_assert(sorted_curves.size() == range.size());
  return sorted_curves;
}

/* TODO Add an optimized function for reversing the order of spans. */
static void reverse_order(GMutableSpan span)
{
  const CPPType &cpptype = span.type();
  BUFFER_FOR_CPP_TYPE_VALUE(cpptype, buffer);
  cpptype.default_construct(buffer);

  for (const int i : IndexRange(span.size() / 2)) {
    const int mirror_i = span.size() - 1 - i;
    /* Swap. */
    cpptype.move_assign(span[i], buffer);
    cpptype.move_assign(span[mirror_i], span[i]);
    cpptype.move_assign(buffer, span[mirror_i]);
  }

  cpptype.destruct(buffer);
}

static void reorder_and_flip_attributes_group_to_group(
    const bke::AttributeAccessor src_attributes,
    const bke::AttrDomain domain,
    const OffsetIndices<int> src_offsets,
    const OffsetIndices<int> dst_offsets,
    const Span<int> old_by_new_map,
    const Span<bool> flip_direction,
    bke::MutableAttributeAccessor dst_attributes)
{
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != domain) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    const GVArray src = *iter.get(domain);
    bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, domain, iter.data_type);
    if (!dst) {
      return;
    }

    threading::parallel_for(old_by_new_map.index_range(), 1024, [&](const IndexRange range) {
      for (const int new_i : range) {
        const int old_i = old_by_new_map[new_i];
        const bool flip = flip_direction[old_i];

        GMutableSpan dst_span = dst.span.slice(dst_offsets[new_i]);
        array_utils::copy(src.slice(src_offsets[old_i]), dst_span);
        if (flip) {
          reverse_order(dst_span);
        }
      }
    });

    dst.finish();
  });
}

static bke::CurvesGeometry reorder_and_flip_curves(const bke::CurvesGeometry &src_curves,
                                                   const Span<int> old_by_new_map,
                                                   const Span<bool> flip_direction)
{
  bke::CurvesGeometry dst_curves = bke::CurvesGeometry(src_curves);

  bke::gather_attributes(src_curves.attributes(),
                         bke::AttrDomain::Curve,
                         bke::AttrDomain::Curve,
                         {},
                         old_by_new_map,
                         dst_curves.attributes_for_write());

  const Span<int> old_offsets = src_curves.offsets();
  MutableSpan<int> new_offsets = dst_curves.offsets_for_write();
  offset_indices::gather_group_sizes(old_offsets, old_by_new_map, new_offsets);
  offset_indices::accumulate_counts_to_offsets(new_offsets);

  reorder_and_flip_attributes_group_to_group(src_curves.attributes(),
                                             bke::AttrDomain::Point,
                                             old_offsets,
                                             new_offsets.as_span(),
                                             old_by_new_map,
                                             flip_direction,
                                             dst_curves.attributes_for_write());
  dst_curves.tag_topology_changed();
  return dst_curves;
}

/* Build new offsets array for connected ranges. */
static void find_connected_ranges(const bke::CurvesGeometry &src_curves,
                                  const Span<int> old_by_new_map,
                                  Span<int> connect_to_curve,
                                  Span<bool> cyclic,
                                  Vector<int> &r_joined_curve_offsets,
                                  Vector<bool> &r_joined_cyclic)
{
  const IndexRange curves_range = src_curves.curves_range();

  Array<int> new_by_old_map(old_by_new_map.size());
  for (const int dst_i : old_by_new_map.index_range()) {
    const int src_i = old_by_new_map[dst_i];
    new_by_old_map[src_i] = dst_i;
  }

  r_joined_curve_offsets.reserve(curves_range.size() + 1);
  r_joined_cyclic.reserve(curves_range.size());

  int start_index = -1;
  for (const int dst_i : curves_range) {
    const int src_i = old_by_new_map[dst_i];
    /* Strokes are cyclic if they are not connected and the original stroke is cyclic, or if the
     * the last stroke of a chain is merged with the first stroke. */
    const bool src_cyclic = cyclic[src_i];

    if (start_index < 0) {
      r_joined_curve_offsets.append(0);
      r_joined_cyclic.append(src_cyclic);
      start_index = dst_i;
    }

    ++r_joined_curve_offsets.last();

    const int src_connect_to = connect_to_curve[src_i];
    const bool is_connected = curves_range.contains(src_connect_to);
    const int dst_connect_to = is_connected ? new_by_old_map[src_connect_to] : -1;

    /* Check for end of chain. */
    if (dst_connect_to != dst_i + 1) {
      /* Set cyclic state for connected curves.
       * Becomes cyclic if connected to the start. */
      const bool is_chain = (is_connected || dst_i != start_index);
      if (is_chain) {
        r_joined_cyclic.last() = (dst_connect_to == start_index);
      }
      /* Start new curve. */
      start_index = -1;
    }
  }
  /* Offsets has one more entry for the overall size. */
  r_joined_curve_offsets.append(0);

  offset_indices::accumulate_counts_to_offsets(r_joined_curve_offsets);
}

static bke::CurvesGeometry join_curves_ranges(const bke::CurvesGeometry &src_curves,
                                              const OffsetIndices<int> old_curves_by_new)
{
  bke::CurvesGeometry dst_curves = bke::CurvesGeometry(src_curves.points_num(),
                                                       old_curves_by_new.size());
  /* Copy vertex group names. */
  BKE_defgroup_copy_list(&dst_curves.vertex_group_names, &src_curves.vertex_group_names);
  dst_curves.attributes_active_index = src_curves.attributes_active_index;

  /* NOTE: using the offsets as an index map means the first curve of each range is used for
   * attributes. */
  const Span<int> old_by_new_map = old_curves_by_new.data().drop_back(1);
  bke::gather_attributes(src_curves.attributes(),
                         bke::AttrDomain::Curve,
                         bke::AttrDomain::Curve,
                         bke::attribute_filter_from_skip_ref({"cyclic"}),
                         old_by_new_map,
                         dst_curves.attributes_for_write());

  const OffsetIndices old_points_by_curve = src_curves.points_by_curve();
  MutableSpan<int> new_offsets = dst_curves.offsets_for_write();
  new_offsets.fill(0);
  for (const int new_i : new_offsets.index_range().drop_back(1)) {
    const IndexRange old_curves = old_curves_by_new[new_i];
    new_offsets[new_i] = offset_indices::sum_group_sizes(old_points_by_curve, old_curves);
  }
  offset_indices::accumulate_counts_to_offsets(new_offsets);

  /* Point attributes copied without changes. */
  bke::copy_attributes(src_curves.attributes(),
                       bke::AttrDomain::Point,
                       bke::AttrDomain::Point,
                       {},
                       dst_curves.attributes_for_write());

  dst_curves.tag_topology_changed();
  return dst_curves;
}

bke::CurvesGeometry curves_merge_endpoints(const bke::CurvesGeometry &src_curves,
                                           Span<int> connect_to_curve,
                                           Span<bool> flip_direction,
                                           const bke::AttributeFilter & /*attribute_filter*/)
{
  BLI_assert(connect_to_curve.size() == src_curves.curves_num());
  const VArraySpan<bool> src_cyclic = src_curves.cyclic();

  Vector<int> old_by_new_map = toposort_connected_curves(connect_to_curve);

  Vector<int> joined_curve_offsets;
  Vector<bool> cyclic;
  find_connected_ranges(
      src_curves, old_by_new_map, connect_to_curve, src_cyclic, joined_curve_offsets, cyclic);

  bke::CurvesGeometry ordered_curves = reorder_and_flip_curves(
      src_curves, old_by_new_map, flip_direction);

  OffsetIndices joined_curves_by_new = OffsetIndices<int>(joined_curve_offsets);
  bke::CurvesGeometry merged_curves = join_curves_ranges(ordered_curves, joined_curves_by_new);
  merged_curves.cyclic_for_write().copy_from(cyclic);

  /**
   * `curves_merge_endpoints` seems to be working only with CURVE_TYPE_POLY, still adding this here
   * in advance.
   */
  if (src_curves.nurbs_has_custom_knots()) {
    bke::curves::nurbs::update_custom_knot_modes(merged_curves.curves_range(),
                                                 NURBS_KNOT_MODE_NORMAL,
                                                 NURBS_KNOT_MODE_NORMAL,
                                                 merged_curves);
  }
  return merged_curves;
}

}  // namespace blender::geometry
