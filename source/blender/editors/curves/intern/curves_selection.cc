/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_array_utils.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_math_geom.h"
#include "BLI_rand.hh"
#include "BLI_rect.h"

#include "BKE_attribute.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"

#include "ED_curves.hh"
#include "ED_object.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

namespace blender::ed::curves {

IndexMask retrieve_selected_curves(const bke::CurvesGeometry &curves, IndexMaskMemory &memory)
{
  const IndexRange curves_range = curves.curves_range();
  const bke::AttributeAccessor attributes = curves.attributes();

  /* Interpolate from points to curves manually as a performance improvement, since we are only
   * interested in whether any point in each curve is selected. Retrieve meta data since
   * #lookup_or_default from the attribute API doesn't give the domain of the attribute. */
  std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(".selection");
  if (meta_data && meta_data->domain == bke::AttrDomain::Point) {
    /* Avoid the interpolation from interpolating the attribute to the
     * curve domain by retrieving the point domain values directly. */
    const VArray<bool> selection = *attributes.lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);
    if (selection.is_single()) {
      return selection.get_internal_single() ? IndexMask(curves_range) : IndexMask();
    }
    const OffsetIndices points_by_curve = curves.points_by_curve();
    return IndexMask::from_predicate(
        curves_range, GrainSize(512), memory, [&](const int64_t curve_i) {
          const IndexRange points = points_by_curve[curve_i];
          /* The curve is selected if any of its points are selected. */
          Array<bool, 32> point_selection(points.size());
          selection.materialize_compressed(points, point_selection);
          return point_selection.as_span().contains(true);
        });
  }
  const VArray<bool> selection = *attributes.lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Curve, true);
  return IndexMask::from_bools(curves_range, selection, memory);
}

IndexMask retrieve_selected_curves(const Curves &curves_id, IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  return retrieve_selected_curves(curves, memory);
}

IndexMask retrieve_selected_points(const bke::CurvesGeometry &curves, IndexMaskMemory &memory)
{
  return IndexMask::from_bools(
      *curves.attributes().lookup_or_default<bool>(".selection", bke::AttrDomain::Point, true),
      memory);
}

IndexMask retrieve_selected_points(const Curves &curves_id, IndexMaskMemory &memory)
{
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  return retrieve_selected_points(curves, memory);
}

bke::GSpanAttributeWriter ensure_selection_attribute(bke::CurvesGeometry &curves,
                                                     const bke::AttrDomain selection_domain,
                                                     const eCustomDataType create_type)
{
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  if (attributes.contains(".selection")) {
    bke::GSpanAttributeWriter selection_attr = attributes.lookup_for_write_span(".selection");
    /* Check domain type. */
    if (selection_attr.domain == selection_domain) {
      return selection_attr;
    }
    selection_attr.finish();
    attributes.remove(".selection");
  }
  const int domain_size = attributes.domain_size(selection_domain);
  switch (create_type) {
    case CD_PROP_BOOL:
      attributes.add(".selection",
                     selection_domain,
                     CD_PROP_BOOL,
                     bke::AttributeInitVArray(VArray<bool>::ForSingle(true, domain_size)));
      break;
    case CD_PROP_FLOAT:
      attributes.add(".selection",
                     selection_domain,
                     CD_PROP_FLOAT,
                     bke::AttributeInitVArray(VArray<float>::ForSingle(1.0f, domain_size)));
      break;
    default:
      BLI_assert_unreachable();
  }
  return attributes.lookup_for_write_span(".selection");
}

void fill_selection_false(GMutableSpan selection)
{
  if (selection.type().is<bool>()) {
    selection.typed<bool>().fill(false);
  }
  else if (selection.type().is<float>()) {
    selection.typed<float>().fill(0.0f);
  }
}

void fill_selection_true(GMutableSpan selection)
{
  if (selection.type().is<bool>()) {
    selection.typed<bool>().fill(true);
  }
  else if (selection.type().is<float>()) {
    selection.typed<float>().fill(1.0f);
  }
}

void fill_selection(GMutableSpan selection, bool value)
{
  if (selection.type().is<bool>()) {
    selection.typed<bool>().fill(value);
  }
  else if (selection.type().is<float>()) {
    selection.typed<float>().fill(value ? 1.0f : 0.0f);
  }
}

void fill_selection_false(GMutableSpan selection, const IndexMask &mask)
{
  if (selection.type().is<bool>()) {
    index_mask::masked_fill(selection.typed<bool>(), false, mask);
  }
  else if (selection.type().is<float>()) {
    index_mask::masked_fill(selection.typed<float>(), 0.0f, mask);
  }
}

void fill_selection_true(GMutableSpan selection, const IndexMask &mask)
{
  if (selection.type().is<bool>()) {
    index_mask::masked_fill(selection.typed<bool>(), true, mask);
  }
  else if (selection.type().is<float>()) {
    index_mask::masked_fill(selection.typed<float>(), 1.0f, mask);
  }
}

static bool contains(const VArray<bool> &varray,
                     const IndexMask &indices_to_check,
                     const bool value)
{
  const CommonVArrayInfo info = varray.common_info();
  if (info.type == CommonVArrayInfo::Type::Single) {
    return *static_cast<const bool *>(info.data) == value;
  }
  if (info.type == CommonVArrayInfo::Type::Span) {
    const Span<bool> span(static_cast<const bool *>(info.data), varray.size());
    return threading::parallel_reduce(
        indices_to_check.index_range(),
        4096,
        false,
        [&](const IndexRange range, const bool init) {
          if (init) {
            return init;
          }
          const IndexMask sliced_mask = indices_to_check.slice(range);
          if (std::optional<IndexRange> range = sliced_mask.to_range()) {
            return span.slice(*range).contains(value);
          }
          for (const int64_t segment_i : IndexRange(sliced_mask.segments_num())) {
            const IndexMaskSegment segment = sliced_mask.segment(segment_i);
            for (const int i : segment) {
              if (span[i] == value) {
                return true;
              }
            }
          }
          return false;
        },
        std::logical_or());
  }
  return threading::parallel_reduce(
      indices_to_check.index_range(),
      2048,
      false,
      [&](const IndexRange range, const bool init) {
        if (init) {
          return init;
        }
        constexpr int64_t MaxChunkSize = 512;
        const int64_t slice_end = range.one_after_last();
        for (int64_t start = range.start(); start < slice_end; start += MaxChunkSize) {
          const int64_t end = std::min<int64_t>(start + MaxChunkSize, slice_end);
          const int64_t size = end - start;
          const IndexMask sliced_mask = indices_to_check.slice(start, size);
          std::array<bool, MaxChunkSize> values;
          auto values_end = values.begin() + size;
          varray.materialize_compressed(sliced_mask, values);
          if (std::find(values.begin(), values_end, value) != values_end) {
            return true;
          }
        }
        return false;
      },
      std::logical_or());
}

static bool contains(const VArray<bool> &varray, const IndexRange range_to_check, const bool value)
{
  return contains(varray, IndexMask(range_to_check), value);
}

bool has_anything_selected(const VArray<bool> &varray, const IndexRange range_to_check)
{
  return contains(varray, range_to_check, true);
}

bool has_anything_selected(const VArray<bool> &varray, const IndexMask &indices_to_check)
{
  return contains(varray, indices_to_check, true);
}

bool has_anything_selected(const bke::CurvesGeometry &curves)
{
  const VArray<bool> selection = *curves.attributes().lookup<bool>(".selection");
  return !selection || contains(selection, selection.index_range(), true);
}

bool has_anything_selected(const bke::CurvesGeometry &curves, const IndexMask &mask)
{
  const VArray<bool> selection = *curves.attributes().lookup<bool>(".selection");
  return !selection || contains(selection, mask, true);
}

bool has_anything_selected(const GSpan selection)
{
  if (selection.type().is<bool>()) {
    return selection.typed<bool>().contains(true);
  }
  else if (selection.type().is<float>()) {
    for (const float elem : selection.typed<float>()) {
      if (elem > 0.0f) {
        return true;
      }
    }
  }
  return false;
}

static void invert_selection(MutableSpan<float> selection)
{
  threading::parallel_for(selection.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      selection[i] = 1.0f - selection[i];
    }
  });
}

static void invert_selection(GMutableSpan selection)
{
  if (selection.type().is<bool>()) {
    array_utils::invert_booleans(selection.typed<bool>());
  }
  else if (selection.type().is<float>()) {
    invert_selection(selection.typed<float>());
  }
}

static void invert_selection(MutableSpan<float> selection, const IndexMask &mask)
{
  mask.foreach_index_optimized<int64_t>(
      GrainSize(2048), [&](const int64_t i) { selection[i] = 1.0f - selection[i]; });
}

static void invert_selection(GMutableSpan selection, const IndexMask &mask)
{
  if (selection.type().is<bool>()) {
    array_utils::invert_booleans(selection.typed<bool>(), mask);
  }
  else if (selection.type().is<float>()) {
    invert_selection(selection.typed<float>(), mask);
  }
}

void select_all(bke::CurvesGeometry &curves,
                const IndexMask &mask,
                const bke::AttrDomain selection_domain,
                int action)
{
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  if (action == SEL_SELECT) {
    std::optional<IndexRange> range = mask.to_range();
    if (range.has_value() &&
        (*range == IndexRange(curves.attributes().domain_size(selection_domain))))
    {
      /* As an optimization, just remove the selection attributes when everything is selected. */
      attributes.remove(".selection");
      return;
    }
  }
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, selection_domain, CD_PROP_BOOL);
  if (action == SEL_SELECT) {
    fill_selection_true(selection.span, mask);
  }
  else if (action == SEL_DESELECT) {
    fill_selection_false(selection.span, mask);
  }
  else if (action == SEL_INVERT) {
    invert_selection(selection.span, mask);
  }
  selection.finish();
}

void select_all(bke::CurvesGeometry &curves, const bke::AttrDomain selection_domain, int action)
{
  const IndexRange selection(curves.attributes().domain_size(selection_domain));
  select_all(curves, selection, selection_domain, action);
}

void select_linked(bke::CurvesGeometry &curves, const IndexMask &curves_mask)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, bke::AttrDomain::Point, CD_PROP_BOOL);

  curves_mask.foreach_index(GrainSize(256), [&](const int64_t curve_i) {
    GMutableSpan selection_curve = selection.span.slice(points_by_curve[curve_i]);
    if (has_anything_selected(selection_curve)) {
      fill_selection_true(selection_curve);
    }
  });

  selection.finish();
}

void select_linked(bke::CurvesGeometry &curves)
{
  select_linked(curves, curves.curves_range());
}

void select_alternate(bke::CurvesGeometry &curves,
                      const IndexMask &curves_mask,
                      const bool deselect_ends)
{
  if (!has_anything_selected(curves)) {
    return;
  }

  const OffsetIndices points_by_curve = curves.points_by_curve();
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, bke::AttrDomain::Point, CD_PROP_BOOL);
  const VArray<bool> cyclic = curves.cyclic();

  MutableSpan<bool> selection_typed = selection.span.typed<bool>();
  curves_mask.foreach_index([&](const int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    if (!has_anything_selected(selection.span.slice(points))) {
      return;
    }

    const int half_of_size = points.size() / 2;
    const IndexRange selected = points.shift(deselect_ends ? 1 : 0);
    const IndexRange deselected = points.shift(deselect_ends ? 0 : 1);
    for (const int i : IndexRange(half_of_size)) {
      const int index = i * 2;
      selection_typed[selected[index]] = true;
      selection_typed[deselected[index]] = false;
    }

    selection_typed[points.first()] = !deselect_ends;
    const bool end_parity_to_selected = bool(points.size() % 2);
    const bool selected_end = cyclic[curve_i] || end_parity_to_selected;
    selection_typed[points.last()] = !deselect_ends && selected_end;

    /* Selected last one require to deselect pre-last one point which is not first. */
    const IndexRange curve_body = points.drop_front(1).drop_back(1);
    if (!deselect_ends && cyclic[curve_i] && !curve_body.is_empty()) {
      selection_typed[curve_body.last()] = false;
    }
  });

  selection.finish();
}

void select_alternate(bke::CurvesGeometry &curves, const bool deselect_ends)
{
  select_alternate(curves, curves.curves_range(), deselect_ends);
}

void select_adjacent(bke::CurvesGeometry &curves,
                     const IndexMask &curves_mask,
                     const bool deselect)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, bke::AttrDomain::Point, CD_PROP_BOOL);
  const VArray<bool> cyclic = curves.cyclic();

  if (deselect) {
    invert_selection(selection.span);
  }

  if (selection.span.type().is<bool>()) {
    MutableSpan<bool> selection_typed = selection.span.typed<bool>();
    curves_mask.foreach_index([&](const int64_t curve_i) {
      const IndexRange points = points_by_curve[curve_i];

      /* Handle all cases in the forward direction. */
      for (int point_i = points.first(); point_i < points.last(); point_i++) {
        if (!selection_typed[point_i] && selection_typed[point_i + 1]) {
          selection_typed[point_i] = true;
        }
      }

      /* Handle all cases in the backwards direction. */
      for (int point_i = points.last(); point_i > points.first(); point_i--) {
        if (!selection_typed[point_i] && selection_typed[point_i - 1]) {
          selection_typed[point_i] = true;
        }
      }

      /* Handle cyclic curve case. */
      if (cyclic[curve_i]) {
        if (selection_typed[points.first()] != selection_typed[points.last()]) {
          selection_typed[points.first()] = true;
          selection_typed[points.last()] = true;
        }
      }
    });
  }
  else if (selection.span.type().is<float>()) {
    MutableSpan<float> selection_typed = selection.span.typed<float>();
    curves_mask.foreach_index([&](const int64_t curve_i) {
      const IndexRange points = points_by_curve[curve_i];

      /* Handle all cases in the forward direction. */
      for (int point_i = points.first(); point_i < points.last(); point_i++) {
        if ((selection_typed[point_i] == 0.0f) && (selection_typed[point_i + 1] > 0.0f)) {
          selection_typed[point_i] = 1.0f;
        }
      }

      /* Handle all cases in the backwards direction. */
      for (int point_i = points.last(); point_i > points.first(); point_i--) {
        if ((selection_typed[point_i] == 0.0f) && (selection_typed[point_i - 1] > 0.0f)) {
          selection_typed[point_i] = 1.0f;
        }
      }

      /* Handle cyclic curve case. */
      if (cyclic[curve_i]) {
        if (selection_typed[points.first()] != selection_typed[points.last()]) {
          selection_typed[points.first()] = 1.0f;
          selection_typed[points.last()] = 1.0f;
        }
      }
    });
  }

  if (deselect) {
    invert_selection(selection.span);
  }

  selection.finish();
}

void select_adjacent(bke::CurvesGeometry &curves, const bool deselect)
{
  select_adjacent(curves, curves.curves_range(), deselect);
}

void apply_selection_operation_at_index(GMutableSpan selection,
                                        const int index,
                                        const eSelectOp sel_op)
{
  if (selection.type().is<bool>()) {
    MutableSpan<bool> selection_typed = selection.typed<bool>();
    switch (sel_op) {
      case SEL_OP_ADD:
      case SEL_OP_SET:
        selection_typed[index] = true;
        break;
      case SEL_OP_SUB:
        selection_typed[index] = false;
        break;
      case SEL_OP_XOR:
        selection_typed[index] = !selection_typed[index];
        break;
      default:
        break;
    }
  }
  else if (selection.type().is<float>()) {
    MutableSpan<float> selection_typed = selection.typed<float>();
    switch (sel_op) {
      case SEL_OP_ADD:
      case SEL_OP_SET:
        selection_typed[index] = 1.0f;
        break;
      case SEL_OP_SUB:
        selection_typed[index] = 0.0f;
        break;
      case SEL_OP_XOR:
        selection_typed[index] = 1.0f - selection_typed[index];
        break;
      default:
        break;
    }
  }
}

static std::optional<FindClosestData> find_closest_point_to_screen_co(
    const ARegion *region,
    const Span<float3> positions,
    const float4x4 &projection,
    const IndexMask &points_mask,
    const float2 mouse_pos,
    float radius,
    const FindClosestData &initial_closest)
{
  const float radius_sq = pow2f(radius);
  const FindClosestData new_closest_data = threading::parallel_reduce(
      points_mask.index_range(),
      1024,
      initial_closest,
      [&](const IndexRange point_indicies_range, const FindClosestData &init) {
        FindClosestData best_match = init;
        for (const int index : point_indicies_range) {
          const int point_i = points_mask[index];
          const float3 pos = positions[point_i];

          /* Find the position of the point in screen space. */
          const float2 pos_proj = ED_view3d_project_float_v2_m4(region, pos, projection);

          const float distance_proj_sq = math::distance_squared(pos_proj, mouse_pos);
          if (distance_proj_sq > radius_sq ||
              distance_proj_sq > best_match.distance * best_match.distance)
          {
            /* Ignore the point because it's too far away or there is already a better point. */
            continue;
          }

          FindClosestData better_candidate;
          better_candidate.index = point_i;
          better_candidate.distance = std::sqrt(distance_proj_sq);

          best_match = better_candidate;
        }
        return best_match;
      },
      [](const FindClosestData &a, const FindClosestData &b) {
        if (a.distance < b.distance) {
          return a;
        }
        return b;
      });

  if (new_closest_data.distance < initial_closest.distance) {
    return new_closest_data;
  }

  return {};
}

static std::optional<FindClosestData> find_closest_curve_to_screen_co(
    const ARegion *region,
    const OffsetIndices<int> points_by_curve,
    const Span<float3> positions,
    const float4x4 &projection,
    const IndexMask &curves_mask,
    const float2 mouse_pos,
    float radius,
    const FindClosestData &initial_closest)
{
  const float radius_sq = pow2f(radius);

  const FindClosestData new_closest_data = threading::parallel_reduce(
      curves_mask.index_range(),
      256,
      initial_closest,
      [&](const IndexRange curves_indices_range, const FindClosestData &init) {
        FindClosestData best_match = init;
        for (const int index : curves_indices_range) {
          const int curve_i = curves_mask[index];
          const IndexRange points = points_by_curve[curve_i];
          if (points.size() == 1) {
            const float3 pos = positions[points.first()];

            /* Find the position of the point in screen space. */
            const float2 pos_proj = ED_view3d_project_float_v2_m4(region, pos, projection);

            const float distance_proj_sq = math::distance_squared(pos_proj, mouse_pos);
            if (distance_proj_sq > radius_sq ||
                distance_proj_sq > best_match.distance * best_match.distance)
            {
              /* Ignore the point because it's too far away or there is already a better point.
               */
              continue;
            }

            FindClosestData better_candidate;
            better_candidate.index = curve_i;
            better_candidate.distance = std::sqrt(distance_proj_sq);

            best_match = better_candidate;
            continue;
          }

          for (const int segment_i : points.drop_back(1)) {
            const float3 pos1 = positions[segment_i];
            const float3 pos2 = positions[segment_i + 1];
            const float2 pos1_proj = ED_view3d_project_float_v2_m4(region, pos1, projection);
            const float2 pos2_proj = ED_view3d_project_float_v2_m4(region, pos2, projection);

            const float distance_proj_sq = dist_squared_to_line_segment_v2(
                mouse_pos, pos1_proj, pos2_proj);
            if (distance_proj_sq > radius_sq ||
                distance_proj_sq > best_match.distance * best_match.distance)
            {
              /* Ignore the segment because it's too far away or there is already a better point.
               */
              continue;
            }

            FindClosestData better_candidate;
            better_candidate.index = curve_i;
            better_candidate.distance = std::sqrt(distance_proj_sq);

            best_match = better_candidate;
          }
        }
        return best_match;
      },
      [](const FindClosestData &a, const FindClosestData &b) {
        if (a.distance < b.distance) {
          return a;
        }
        return b;
      });

  if (new_closest_data.distance < initial_closest.distance) {
    return new_closest_data;
  }

  return {};
}

std::optional<FindClosestData> closest_elem_find_screen_space(
    const ViewContext &vc,
    const OffsetIndices<int> points_by_curve,
    const Span<float3> positions,
    const float4x4 &projection,
    const IndexMask &mask,
    const bke::AttrDomain domain,
    const int2 coord,
    const FindClosestData &initial_closest)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return find_closest_point_to_screen_co(vc.region,
                                             positions,
                                             projection,
                                             mask,
                                             float2(coord),
                                             ED_view3d_select_dist_px(),
                                             initial_closest);
    case bke::AttrDomain::Curve:
      return find_closest_curve_to_screen_co(vc.region,
                                             points_by_curve,
                                             positions,
                                             projection,
                                             mask,
                                             float2(coord),
                                             ED_view3d_select_dist_px(),
                                             initial_closest);
    default:
      BLI_assert_unreachable();
      return {};
  }
}

bool select_box(const ViewContext &vc,
                bke::CurvesGeometry &curves,
                const Span<float3> positions,
                const float4x4 &projection,
                const IndexMask &mask,
                const bke::AttrDomain selection_domain,
                const rcti &rect,
                const eSelectOp sel_op)
{
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, selection_domain, CD_PROP_BOOL);

  bool changed = false;
  if (sel_op == SEL_OP_SET) {
    fill_selection_false(selection.span, mask);
    changed = true;
  }

  const OffsetIndices points_by_curve = curves.points_by_curve();
  if (selection_domain == bke::AttrDomain::Point) {
    mask.foreach_index(GrainSize(1024), [&](const int point_i) {
      const float2 pos_proj = ED_view3d_project_float_v2_m4(
          vc.region, positions[point_i], projection);
      if (BLI_rcti_isect_pt_v(&rect, int2(pos_proj))) {
        apply_selection_operation_at_index(selection.span, point_i, sel_op);
        changed = true;
      }
    });
  }
  else if (selection_domain == bke::AttrDomain::Curve) {
    mask.foreach_index(GrainSize(512), [&](const int curve_i) {
      const IndexRange points = points_by_curve[curve_i];
      if (points.size() == 1) {
        const float2 pos_proj = ED_view3d_project_float_v2_m4(
            vc.region, positions[points.first()], projection);
        if (BLI_rcti_isect_pt_v(&rect, int2(pos_proj))) {
          apply_selection_operation_at_index(selection.span, curve_i, sel_op);
          changed = true;
        }
        return;
      }
      for (const int segment_i : points.drop_back(1)) {
        const float3 pos1 = positions[segment_i];
        const float3 pos2 = positions[segment_i + 1];

        const float2 pos1_proj = ED_view3d_project_float_v2_m4(vc.region, pos1, projection);
        const float2 pos2_proj = ED_view3d_project_float_v2_m4(vc.region, pos2, projection);

        if (BLI_rcti_isect_segment(&rect, int2(pos1_proj), int2(pos2_proj))) {
          apply_selection_operation_at_index(selection.span, curve_i, sel_op);
          changed = true;
          break;
        }
      }
    });
  }
  selection.finish();

  return changed;
}

bool select_lasso(const ViewContext &vc,
                  bke::CurvesGeometry &curves,
                  const Span<float3> positions,
                  const float4x4 &projection_matrix,
                  const IndexMask &mask,
                  const bke::AttrDomain selection_domain,
                  const Span<int2> lasso_coords,
                  const eSelectOp sel_op)
{
  rcti bbox;
  BLI_lasso_boundbox(&bbox, lasso_coords);

  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, selection_domain, CD_PROP_BOOL);

  bool changed = false;
  if (sel_op == SEL_OP_SET) {
    fill_selection_false(selection.span, mask);
    changed = true;
  }

  const OffsetIndices points_by_curve = curves.points_by_curve();
  if (selection_domain == bke::AttrDomain::Point) {
    mask.foreach_index(GrainSize(1024), [&](const int point_i) {
      const float2 pos_proj = ED_view3d_project_float_v2_m4(
          vc.region, positions[point_i], projection_matrix);
      /* Check the lasso bounding box first as an optimization. */
      if (BLI_rcti_isect_pt_v(&bbox, int2(pos_proj)) &&
          BLI_lasso_is_point_inside(lasso_coords, int(pos_proj.x), int(pos_proj.y), IS_CLIPPED))
      {
        apply_selection_operation_at_index(selection.span, point_i, sel_op);
        changed = true;
      }
    });
  }
  else if (selection_domain == bke::AttrDomain::Curve) {
    mask.foreach_index(GrainSize(512), [&](const int curve_i) {
      const IndexRange points = points_by_curve[curve_i];
      if (points.size() == 1) {
        const float2 pos_proj = ED_view3d_project_float_v2_m4(
            vc.region, positions[points.first()], projection_matrix);
        /* Check the lasso bounding box first as an optimization. */
        if (BLI_rcti_isect_pt_v(&bbox, int2(pos_proj)) &&
            BLI_lasso_is_point_inside(lasso_coords, int(pos_proj.x), int(pos_proj.y), IS_CLIPPED))
        {
          apply_selection_operation_at_index(selection.span, curve_i, sel_op);
          changed = true;
        }
        return;
      }
      for (const int segment_i : points.drop_back(1)) {
        const float3 pos1 = positions[segment_i];
        const float3 pos2 = positions[segment_i + 1];

        const float2 pos1_proj = ED_view3d_project_float_v2_m4(vc.region, pos1, projection_matrix);
        const float2 pos2_proj = ED_view3d_project_float_v2_m4(vc.region, pos2, projection_matrix);

        /* Check the lasso bounding box first as an optimization. */
        if (BLI_rcti_isect_segment(&bbox, int2(pos1_proj), int2(pos2_proj)) &&
            BLI_lasso_is_edge_inside(lasso_coords,
                                     int(pos1_proj.x),
                                     int(pos1_proj.y),
                                     int(pos2_proj.x),
                                     int(pos2_proj.y),
                                     IS_CLIPPED))
        {
          apply_selection_operation_at_index(selection.span, curve_i, sel_op);
          changed = true;
          break;
        }
      }
    });
  }
  selection.finish();

  return changed;
}

bool select_circle(const ViewContext &vc,
                   bke::CurvesGeometry &curves,
                   const Span<float3> positions,
                   const float4x4 &projection,
                   const IndexMask &mask,
                   const bke::AttrDomain selection_domain,
                   const int2 coord,
                   const float radius,
                   const eSelectOp sel_op)
{
  const float radius_sq = pow2f(radius);
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(
      curves, selection_domain, CD_PROP_BOOL);

  bool changed = false;
  if (sel_op == SEL_OP_SET) {
    fill_selection_false(selection.span, mask);
    changed = true;
  }

  const OffsetIndices points_by_curve = curves.points_by_curve();
  if (selection_domain == bke::AttrDomain::Point) {
    mask.foreach_index(GrainSize(1024), [&](const int point_i) {
      const float2 pos_proj = ED_view3d_project_float_v2_m4(
          vc.region, positions[point_i], projection);
      if (math::distance_squared(pos_proj, float2(coord)) <= radius_sq) {
        apply_selection_operation_at_index(selection.span, point_i, sel_op);
        changed = true;
      }
    });
  }
  else if (selection_domain == bke::AttrDomain::Curve) {
    mask.foreach_index(GrainSize(512), [&](const int curve_i) {
      const IndexRange points = points_by_curve[curve_i];
      if (points.size() == 1) {
        const float2 pos_proj = ED_view3d_project_float_v2_m4(
            vc.region, positions[points.first()], projection);
        if (math::distance_squared(pos_proj, float2(coord)) <= radius_sq) {
          apply_selection_operation_at_index(selection.span, curve_i, sel_op);
          changed = true;
        }
        return;
      }
      for (const int segment_i : points.drop_back(1)) {
        const float3 pos1 = positions[segment_i];
        const float3 pos2 = positions[segment_i + 1];

        const float2 pos1_proj = ED_view3d_project_float_v2_m4(vc.region, pos1, projection);
        const float2 pos2_proj = ED_view3d_project_float_v2_m4(vc.region, pos2, projection);

        const float distance_proj_sq = dist_squared_to_line_segment_v2(
            float2(coord), pos1_proj, pos2_proj);
        if (distance_proj_sq <= radius_sq) {
          apply_selection_operation_at_index(selection.span, curve_i, sel_op);
          changed = true;
          break;
        }
      }
    });
  }
  selection.finish();

  return changed;
}

}  // namespace blender::ed::curves
