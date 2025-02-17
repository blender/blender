/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edpointcloud
 */

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_math_vector.hh"
#include "BLI_rect.h"

#include "BKE_attribute.hh"

#include "ED_point_cloud.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "DNA_pointcloud_types.h"

namespace blender::ed::point_cloud {

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

bool has_anything_selected(const PointCloud &point_cloud)
{
  const VArray<bool> selection = *point_cloud.attributes().lookup<bool>(".selection");
  return !selection || contains(selection, selection.index_range(), true);
}

bke::GSpanAttributeWriter ensure_selection_attribute(PointCloud &point_cloud,
                                                     eCustomDataType create_type)
{
  const bke::AttrDomain selection_domain = bke::AttrDomain::Point;
  const StringRef attribute_name = ".selection";

  bke::MutableAttributeAccessor attributes = point_cloud.attributes_for_write();
  if (attributes.contains(attribute_name)) {
    return attributes.lookup_for_write_span(attribute_name);
  }
  const int domain_size = point_cloud.totpoint;
  switch (create_type) {
    case CD_PROP_BOOL:
      attributes.add(attribute_name,
                     selection_domain,
                     CD_PROP_BOOL,
                     bke::AttributeInitVArray(VArray<bool>::ForSingle(true, domain_size)));
      break;
    case CD_PROP_FLOAT:
      attributes.add(attribute_name,
                     selection_domain,
                     CD_PROP_FLOAT,
                     bke::AttributeInitVArray(VArray<float>::ForSingle(1.0f, domain_size)));
      break;
    default:
      BLI_assert_unreachable();
  }
  return attributes.lookup_for_write_span(attribute_name);
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

static void select_all(PointCloud &point_cloud, const IndexMask &mask, int action)
{
  if (action == SEL_SELECT) {
    std::optional<IndexRange> range = mask.to_range();
    if (range.has_value() && (*range == IndexRange(point_cloud.totpoint))) {
      bke::MutableAttributeAccessor attributes = point_cloud.attributes_for_write();
      /* As an optimization, just remove the selection attributes when everything is selected. */
      attributes.remove(".selection");
      return;
    }
  }

  bke::GSpanAttributeWriter selection = ensure_selection_attribute(point_cloud, CD_PROP_BOOL);
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

void select_all(PointCloud &point_cloud, int action)
{
  select_all(point_cloud, IndexRange(point_cloud.totpoint), action);
}

static bool apply_selection_operation(PointCloud &point_cloud,
                                      const IndexMask &mask,
                                      eSelectOp sel_op)
{
  bool changed = false;
  bke::GSpanAttributeWriter selection = ensure_selection_attribute(point_cloud, CD_PROP_BOOL);
  if (sel_op == SEL_OP_SET) {
    fill_selection_false(selection.span, IndexRange(selection.span.size()));
    changed = true;
  }
  switch (sel_op) {
    case SEL_OP_ADD:
    case SEL_OP_SET:
      fill_selection_true(selection.span, mask);
      break;
    case SEL_OP_SUB:
      fill_selection_false(selection.span, mask);
      break;
    case SEL_OP_XOR:
      invert_selection(selection.span, mask);
      break;
    default:
      break;
  }
  changed |= !mask.is_empty();
  selection.finish();
  return changed;
}

bool select_box(PointCloud &point_cloud,
                const ARegion &region,
                const float4x4 &projection,
                const rcti &rect,
                const eSelectOp sel_op)
{
  const Span<float3> positions = point_cloud.positions();

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_predicate(
      positions.index_range(), GrainSize(1024), memory, [&](const int point) {
        const float2 pos_proj = ED_view3d_project_float_v2_m4(
            &region, positions[point], projection);
        return BLI_rcti_isect_pt_v(&rect, int2(pos_proj));
      });

  return apply_selection_operation(point_cloud, mask, sel_op);
}

bool select_lasso(PointCloud &point_cloud,
                  const ARegion &region,
                  const float4x4 &projection,
                  const Span<int2> lasso_coords,
                  const eSelectOp sel_op)
{
  rcti bbox;
  BLI_lasso_boundbox(&bbox, lasso_coords);

  const Span<float3> positions = point_cloud.positions();

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_predicate(
      positions.index_range(), GrainSize(1024), memory, [&](const int point) {
        const float2 pos_proj = ED_view3d_project_float_v2_m4(
            &region, positions[point], projection);
        if (!BLI_rcti_isect_pt_v(&bbox, int2(pos_proj))) {
          return false;
        }
        if (!BLI_lasso_is_point_inside(lasso_coords, int(pos_proj.x), int(pos_proj.y), IS_CLIPPED))
        {
          return false;
        }
        return true;
      });

  return apply_selection_operation(point_cloud, mask, sel_op);
}

bool select_circle(PointCloud &point_cloud,
                   const ARegion &region,
                   const float4x4 &projection,
                   const int2 coord,
                   const float radius,
                   const eSelectOp sel_op)
{
  const float radius_sq = radius * radius;

  const Span<float3> positions = point_cloud.positions();

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_predicate(
      positions.index_range(), GrainSize(1024), memory, [&](const int point) {
        const float2 pos_proj = ED_view3d_project_float_v2_m4(
            &region, positions[point], projection);
        return math::distance_squared(pos_proj, float2(coord)) <= radius_sq;
      });

  return apply_selection_operation(point_cloud, mask, sel_op);
}

}  // namespace blender::ed::point_cloud
