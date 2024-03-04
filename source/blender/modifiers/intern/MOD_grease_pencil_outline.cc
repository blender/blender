/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"
#include "BKE_material.h"
#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_range.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BLI_virtual_array.hh"
#include "DNA_defaults.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

#include "GEO_resample_curves.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

#include <iostream>

namespace blender {

static void init_data(ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(omd, modifier));

  MEMCPY_STRUCT_AFTER(omd, DNA_struct_default_get(GreasePencilOutlineModifierData), modifier);
  modifier::greasepencil::init_influence_data(&omd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *omd = reinterpret_cast<const GreasePencilOutlineModifierData *>(md);
  auto *tmmd = reinterpret_cast<GreasePencilOutlineModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tmmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&omd->influence, &tmmd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);
  modifier::greasepencil::free_influence_data(&omd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&omd->influence, ob, walk, user_data);
  walk(user_data, ob, (ID **)&omd->outline_material, IDWALK_CB_USER);
  walk(user_data, ob, (ID **)&omd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);
  if (ctx->scene->camera) {
    DEG_add_object_relation(
        ctx->node, ctx->scene->camera, DEG_OB_COMP_TRANSFORM, "Grease Pencil Outline Modifier");
    DEG_add_object_relation(
        ctx->node, ctx->scene->camera, DEG_OB_COMP_PARAMETERS, "Grease Pencil Outline Modifier");
  }
  if (omd->object != nullptr) {
    DEG_add_object_relation(
        ctx->node, omd->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Outline Modifier");
  }
  DEG_add_object_relation(
      ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Grease Pencil Outline Modifier");
}

/**
 * Rearrange curve buffers by moving points from the start to the back of each stroke.
 * \note This is an optional feature. The offset is determine by the closest point to an object.
 * \param curve_offsets Offset of each curve, indicating the point that becomes the new start.
 */
static bke::CurvesGeometry reorder_cyclic_curve_points(const bke::CurvesGeometry &src_curves,
                                                       const IndexMask &curve_selection,
                                                       const Span<int> curve_offsets)
{
  BLI_assert(curve_offsets.size() == src_curves.curves_num());

  OffsetIndices<int> src_offsets = src_curves.points_by_curve();
  bke::AttributeAccessor src_attributes = src_curves.attributes();

  Array<int> indices(src_curves.points_num());
  curve_selection.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = src_offsets[curve_i];
    const int point_num = points.size();
    const int point_start = points.start();
    MutableSpan<int> point_indices = indices.as_mutable_span().slice(points);
    if (points.size() < 2) {
      array_utils::fill_index_range(point_indices, point_start);
      return;
    }
    /* Offset can be negative or larger than the buffer. Use modulo to get an
     * equivalent offset within buffer size to simplify copying. */
    const int offset_raw = curve_offsets[curve_i];
    const int offset = offset_raw >= 0 ? offset_raw % points.size() :
                                         points.size() - ((-offset_raw) % points.size());
    BLI_assert(0 <= offset && offset < points.size());
    if (offset == 0) {
      array_utils::fill_index_range(point_indices, point_start);
      return;
    }

    const int point_middle = point_start + offset;
    array_utils::fill_index_range(point_indices.take_front(point_num - offset), point_middle);
    array_utils::fill_index_range(point_indices.take_back(offset), point_start);
  });

  /* Have to make a copy of the input geometry, gather_attributes does not work in-place when the
   * source indices are not ordered. */
  bke::CurvesGeometry dst_curves(src_curves);
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  bke::gather_attributes(src_attributes, bke::AttrDomain::Point, {}, {}, indices, dst_attributes);

  return dst_curves;
}

static int find_closest_point(const Span<float3> positions, const float3 &target)
{
  if (positions.is_empty()) {
    return 0;
  }

  int closest_i = 0;
  float min_dist_squared = math::distance_squared(positions.first(), target);
  for (const int i : positions.index_range().drop_front(1)) {
    const float dist_squared = math::distance_squared(positions[i], target);
    if (dist_squared < min_dist_squared) {
      closest_i = i;
      min_dist_squared = dist_squared;
    }
  }
  return closest_i;
}

/* Generate points in an arc between two directions. */
static void generate_arc_from_point_to_point(const float3 &from,
                                             const float3 &to,
                                             const float3 &center_pt,
                                             const int subdivisions,
                                             const int src_point_index,
                                             Vector<float3> &r_perimeter,
                                             Vector<int> &r_src_indices)
{
  const float3 vec_from = from - center_pt;
  const float3 vec_to = to - center_pt;
  if (math::is_zero(vec_from) || math::is_zero(vec_to)) {
    return;
  }

  const float dot = math::dot(vec_from.xy(), vec_to.xy());
  const float det = vec_from.x * vec_to.y - vec_from.y * vec_to.x;
  const float angle = math::atan2(-det, -dot) + M_PI;

  /* Number of points is 2^(n+1) + 1 on half a circle (n=subdivisions)
   * so we multiply by (angle / pi) to get the right amount of
   * points to insert. */
  const int num_points = std::max(int(((1 << (subdivisions + 1)) + 1) * (math::abs(angle) / M_PI)),
                                  2);
  const float delta_angle = angle / float(num_points - 1);
  const float delta_cos = math::cos(delta_angle);
  const float delta_sin = math::sin(delta_angle);

  float3 vec = vec_from;
  for ([[maybe_unused]] const int i : IndexRange(num_points)) {
    r_perimeter.append(center_pt + vec);
    r_src_indices.append(src_point_index);

    const float x = delta_cos * vec.x - delta_sin * vec.y;
    const float y = delta_sin * vec.x + delta_cos * vec.y;
    vec = float3(x, y, 0.0f);
  }
}

/* Generate a semi-circle around a point, opposite the direction. */
static void generate_cap(const float3 &point,
                         const float3 &tangent,
                         const float radius,
                         const int subdivisions,
                         const eGPDstroke_Caps cap_type,
                         const int src_point_index,
                         Vector<float3> &r_perimeter,
                         Vector<int> &r_src_indices)
{
  const float3 normal = {tangent.y, -tangent.x, 0.0f};
  switch (cap_type) {
    case GP_STROKE_CAP_ROUND:
      generate_arc_from_point_to_point(point - normal * radius,
                                       point + normal * radius,
                                       point,
                                       subdivisions,
                                       src_point_index,
                                       r_perimeter,
                                       r_src_indices);
      break;
    case GP_STROKE_CAP_FLAT:
      r_perimeter.append(point + normal * radius);
      r_src_indices.append(src_point_index);
      break;
    case GP_STROKE_CAP_MAX:
      BLI_assert_unreachable();
      break;
  }
}

/* Generate a corner between two segments, with a rounded outer perimeter.
 * Note: The perimeter is considered to be to the right hand side of the stroke. The left side
 * perimeter can be generated by reversing the order of points. */
static void generate_corner(const float3 &pt_a,
                            const float3 &pt_b,
                            const float3 &pt_c,
                            const float radius,
                            const int subdivisions,
                            const int src_point_index,
                            Vector<float3> &r_perimeter,
                            Vector<int> &r_src_indices)
{
  const float length = math::length(pt_c - pt_b);
  const float length_prev = math::length(pt_b - pt_a);
  const float3 tangent = math::normalize(pt_c - pt_b);
  const float3 tangent_prev = math::normalize(pt_b - pt_a);
  const float3 normal = {tangent.y, -tangent.x, 0.0f};
  const float3 normal_prev = {tangent_prev.y, -tangent_prev.x, 0.0f};

  const float sin_angle = tangent_prev.x * tangent.y - tangent_prev.y * tangent.x;
  /* Whether the corner is an inside or outside corner.
   * This determines whether an arc is added or a single miter point. */
  const bool is_outside_corner = (sin_angle >= 0.0f);
  if (is_outside_corner) {
    generate_arc_from_point_to_point(pt_b + normal_prev * radius,
                                     pt_b + normal * radius,
                                     pt_b,
                                     subdivisions,
                                     src_point_index,
                                     r_perimeter,
                                     r_src_indices);
  }
  else {
    const float3 avg_tangent = math::normalize(tangent_prev + tangent);
    const float3 miter = {avg_tangent.y, -avg_tangent.x, 0.0f};
    const float miter_invscale = math::dot(normal, miter);

    /* Avoid division by tiny values for steep angles. */
    const float3 miter_point = (radius < length * miter_invscale &&
                                radius < length_prev * miter_invscale) ?
                                   pt_b + miter * radius / miter_invscale :
                                   pt_b + miter * radius;

    r_perimeter.append(miter_point);
    r_src_indices.append(src_point_index);
  }
}

static void generate_stroke_perimeter(const Span<float3> all_positions,
                                      const VArray<float> all_radii,
                                      const IndexRange points,
                                      const int subdivisions,
                                      const bool is_cyclic,
                                      const bool use_caps,
                                      const eGPDstroke_Caps start_cap_type,
                                      const eGPDstroke_Caps end_cap_type,
                                      const float radius_offset,
                                      Vector<float3> &r_perimeter,
                                      Vector<int> &r_point_counts,
                                      Vector<int> &r_point_indices)
{
  const Span<float3> positions = all_positions.slice(points);
  const int point_num = points.size();
  if (point_num < 2) {
    return;
  }

  auto add_corner = [&](const int a, const int b, const int c) {
    const int point = points[b];
    const float3 pt_a = positions[a];
    const float3 pt_b = positions[b];
    const float3 pt_c = positions[c];
    const float radius = all_radii[point] + radius_offset;
    generate_corner(pt_a, pt_b, pt_c, radius, subdivisions, point, r_perimeter, r_point_indices);
  };
  auto add_cap = [&](const int center_i, const int next_i, const eGPDstroke_Caps cap_type) {
    const int point = points[center_i];
    const float3 &center = positions[center_i];
    const float3 dir = math::normalize(positions[next_i] - center);
    const float radius = all_radii[point] + radius_offset;
    generate_cap(center, dir, radius, subdivisions, cap_type, point, r_perimeter, r_point_indices);
  };

  /* Creates a single cyclic curve with end caps. */
  if (use_caps) {
    /* Open curves generate a start and end cap and a connecting stroke on either side. */
    const int perimeter_start = r_perimeter.size();

    /* Start cap. */
    add_cap(0, 1, start_cap_type);

    /* Left perimeter half. */
    for (const int i : points.index_range().drop_front(1).drop_back(1)) {
      add_corner(i - 1, i, i + 1);
    }
    if (is_cyclic) {
      add_corner(point_num - 2, point_num - 1, 0);
    }

    /* End cap. */
    if (is_cyclic) {
      /* End point is same as start point. */
      add_cap(0, point_num - 1, end_cap_type);
    }
    else {
      /* End point is last point of the curve. */
      add_cap(point_num - 1, point_num - 2, end_cap_type);
    }

    /* Right perimeter half. */
    if (is_cyclic) {
      add_corner(0, point_num - 1, point_num - 2);
    }
    for (const int i : points.index_range().drop_front(1).drop_back(1)) {
      add_corner(point_num - i, point_num - i - 1, point_num - i - 2);
    }

    const int perimeter_count = r_perimeter.size() - perimeter_start;
    if (perimeter_count > 0) {
      r_point_counts.append(perimeter_count);
    }
  }
  else {
    /* Generate separate "inside" and an "outside" perimeter curves.
     * The distinction is arbitrary, called left/right here. */

    /* Left side perimeter. */
    const int left_perimeter_start = r_perimeter.size();
    add_corner(point_num - 1, 0, 1);
    for (const int i : points.index_range().drop_front(1).drop_back(1)) {
      add_corner(i - 1, i, i + 1);
    }
    add_corner(point_num - 2, point_num - 1, 0);
    const int left_perimeter_count = r_perimeter.size() - left_perimeter_start;
    if (left_perimeter_count > 0) {
      r_point_counts.append(left_perimeter_count);
    }

    /* Right side perimeter. */
    const int right_perimeter_start = r_perimeter.size();
    add_corner(0, point_num - 1, point_num - 2);
    for (const int i : points.index_range().drop_front(1).drop_back(1)) {
      add_corner(point_num - i, point_num - i - 1, point_num - i - 2);
    }
    add_corner(1, 0, point_num - 1);
    const int right_perimeter_count = r_perimeter.size() - right_perimeter_start;
    if (right_perimeter_count > 0) {
      r_point_counts.append(right_perimeter_count);
    }
  }
}

struct PerimeterData {
  /* New points per curve count. */
  Vector<int> point_counts;
  /* New point coordinates. */
  Vector<float3> positions;
  /* Source curve index. */
  Vector<int> curve_indices;
  /* Source point index. */
  Vector<int> point_indices;
};

static bke::CurvesGeometry create_curves_outline(const bke::greasepencil::Drawing &drawing,
                                                 const float4x4 &viewmat,
                                                 const IndexMask &curves_mask,
                                                 const int subdivisions,
                                                 const float stroke_radius,
                                                 int stroke_mat_nr,
                                                 const bool keep_shape)
{
  const bke::CurvesGeometry &src_curves = drawing.strokes();
  Span<float3> src_positions = src_curves.positions();
  bke::AttributeAccessor src_attributes = src_curves.attributes();
  const VArray<float> src_radii = drawing.radii();
  const VArray<bool> src_cyclic = *src_attributes.lookup_or_default(
      "cyclic", bke::AttrDomain::Curve, false);
  const VArray<int8_t> src_start_caps = *src_attributes.lookup_or_default<int8_t>(
      "start_cap", bke::AttrDomain::Curve, GP_STROKE_CAP_ROUND);
  const VArray<int8_t> src_end_caps = *src_attributes.lookup_or_default<int8_t>(
      "end_cap", bke::AttrDomain::Curve, GP_STROKE_CAP_ROUND);
  const VArray<int> src_material_index = *src_attributes.lookup_or_default(
      "material_index", bke::AttrDomain::Curve, -1);

  /* Transform positions into view space. */
  Array<float3> view_positions(src_positions.size());
  threading::parallel_for(view_positions.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      view_positions[i] = math::transform_point(viewmat, src_positions[i]);
    }
  });

  const float4x4 viewinv = math::invert(viewmat);
  threading::EnumerableThreadSpecific<PerimeterData> thread_data;
  curves_mask.foreach_index([&](const int64_t curve_i) {
    PerimeterData &data = thread_data.local();

    const bool is_cyclic_curve = src_cyclic[curve_i];
    /* Note: Cyclic curves would better be represented by a cyclic perimeter without end caps, but
     * we always generate caps for compatibility with GPv2. Fill materials cannot create holes, so
     * a cyclic outline does not work well. */
    const bool use_caps = true /*!is_cyclic_curve*/;

    const int prev_point_num = data.positions.size();
    const int prev_curve_num = data.point_counts.size();
    const IndexRange points = src_curves.points_by_curve()[curve_i];
    /* Offset the strokes by the radius so the outside aligns with the input stroke. */
    const float radius_offset = keep_shape ? -stroke_radius : 0.0f;
    generate_stroke_perimeter(view_positions,
                              src_radii,
                              points,
                              subdivisions,
                              is_cyclic_curve,
                              use_caps,
                              eGPDstroke_Caps(src_start_caps[curve_i]),
                              eGPDstroke_Caps(src_end_caps[curve_i]),
                              radius_offset,
                              data.positions,
                              data.point_counts,
                              data.point_indices);

    /* Transform perimeter positions back into object space. */
    for (float3 &pos : data.positions.as_mutable_span().drop_front(prev_point_num)) {
      pos = math::transform_point(viewinv, pos);
    }

    data.curve_indices.append_n_times(curve_i, data.point_counts.size() - prev_curve_num);
  });

  int dst_curve_num = 0;
  int dst_point_num = 0;
  for (const PerimeterData &data : thread_data) {
    BLI_assert(data.point_counts.size() == data.curve_indices.size());
    BLI_assert(data.positions.size() == data.point_indices.size());
    dst_curve_num += data.point_counts.size();
    dst_point_num += data.positions.size();
  }

  bke::CurvesGeometry dst_curves(dst_point_num, dst_curve_num);
  if (dst_point_num == 0 || dst_curve_num == 0) {
    return dst_curves;
  }

  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  bke::SpanAttributeWriter<bool> dst_cyclic = dst_attributes.lookup_or_add_for_write_span<bool>(
      "cyclic", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<int> dst_material = dst_attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<float> dst_radius = dst_attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);
  const MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  const MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  /* Source indices for attribute mapping. */
  Array<int> dst_curve_map(dst_curve_num);
  Array<int> dst_point_map(dst_point_num);

  IndexRange curves;
  IndexRange points;
  for (const PerimeterData &data : thread_data) {
    curves = curves.after(data.point_counts.size());
    points = points.after(data.positions.size());

    /* Append curve data. */
    dst_curve_map.as_mutable_span().slice(curves).copy_from(data.curve_indices);
    /* Curve offsets are accumulated below. */
    dst_offsets.slice(curves).copy_from(data.point_counts);
    dst_cyclic.span.slice(curves).fill(true);
    if (stroke_mat_nr >= 0) {
      dst_material.span.slice(curves).fill(stroke_mat_nr);
    }
    else {
      for (const int i : curves.index_range()) {
        dst_material.span[curves[i]] = src_material_index[data.curve_indices[i]];
      }
    }

    /* Append point data. */
    dst_positions.slice(points).copy_from(data.positions);
    dst_point_map.as_mutable_span().slice(points).copy_from(data.point_indices);
    dst_radius.span.slice(points).fill(stroke_radius);
  }
  offset_indices::accumulate_counts_to_offsets(dst_curves.offsets_for_write());

  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Point,
                         {},
                         {"position", "radius"},
                         dst_point_map,
                         dst_attributes);
  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         {},
                         {"cyclic", "material_index"},
                         dst_curve_map,
                         dst_attributes);

  dst_cyclic.finish();
  dst_material.finish();
  dst_radius.finish();
  dst_curves.update_curve_types();

  return dst_curves;
}

static void modify_drawing(const GreasePencilOutlineModifierData &omd,
                           const ModifierEvalContext &ctx,
                           bke::greasepencil::Drawing &drawing,
                           const float4x4 &viewmat)
{
  if (drawing.strokes().curve_num == 0) {
    return;
  }

  /* Selected source curves. */
  IndexMaskMemory curve_mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, drawing.strokes(), omd.influence, curve_mask_memory);

  /* Unit object scale is applied to the stroke radius. */
  const float object_scale = math::length(
      math::transform_direction(ctx.object->object_to_world(), float3(M_SQRT1_3)));
  /* Legacy thickness setting is diameter in pixels, divide by 2000 to get radius. */
  const float radius = math::max(omd.thickness * object_scale, 1.0f) * 0.0005f;
  const bool keep_shape = omd.flag & MOD_GREASE_PENCIL_OUTLINE_KEEP_SHAPE;
  const int mat_nr = (omd.outline_material ?
                          BKE_object_material_index_get(ctx.object, omd.outline_material) :
                          -1);

  bke::CurvesGeometry curves = create_curves_outline(
      drawing, viewmat, curves_mask, omd.subdiv, radius, mat_nr, keep_shape);

  /* Cyclic curve reordering feature. */
  if (omd.object) {
    const OffsetIndices points_by_curve = curves.points_by_curve();

    /* Computes the offset of the closest point to the object from the curve start. */
    Array<int> offset_by_curve(curves.curves_num());
    for (const int i : curves.curves_range()) {
      const IndexRange points = points_by_curve[i];
      /* Closest point index is already relative to the point range and can be used as offset. */
      offset_by_curve[i] = find_closest_point(curves.positions().slice(points), omd.object->loc);
    }

    curves = reorder_cyclic_curve_points(curves, curves.curves_range(), offset_by_curve);
  }

  /* Resampling feature. */
  if (omd.sample_length > 0.0f) {
    VArray<float> sample_lengths = VArray<float>::ForSingle(omd.sample_length,
                                                            curves.curves_num());
    curves = geometry::resample_to_length(curves, curves.curves_range(), sample_lengths);
  }

  drawing.strokes_for_write() = std::move(curves);
  drawing.tag_topology_changed();
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;
  using modifier::greasepencil::LayerDrawingInfo;

  const auto &omd = *reinterpret_cast<const GreasePencilOutlineModifierData *>(md);

  const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  if (!scene->camera) {
    return;
  }
  const float4x4 viewinv = scene->camera->world_to_object();

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, omd.influence, mask_memory);

  const Vector<LayerDrawingInfo> drawings = modifier::greasepencil::get_drawing_infos_by_layer(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings, [&](const LayerDrawingInfo &info) {
    const Layer &layer = *grease_pencil.layers()[info.layer_index];
    const float4x4 viewmat = viewinv * layer.to_world_space(*ctx->object);
    modify_drawing(omd, *ctx, *info.drawing, viewmat);
  });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "thickness", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "use_keep_shape", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "subdivision", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "sample_length", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "outline_material", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "object", UI_ITEM_NONE, nullptr, ICON_NONE);

  Scene *scene = CTX_data_scene(C);
  if (scene->camera == nullptr) {
    uiItemL(layout, RPT_("Outline requires an active camera"), ICON_ERROR);
  }

  if (uiLayout *influence_panel = uiLayoutPanelProp(
          C, layout, ptr, "open_influence_panel", "Influence"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilOutline, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *omd = reinterpret_cast<const GreasePencilOutlineModifierData *>(md);

  BLO_write_struct(writer, GreasePencilOutlineModifierData, omd);
  modifier::greasepencil::write_influence_data(writer, &omd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *omd = reinterpret_cast<GreasePencilOutlineModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &omd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilOutline = {
    /*idname*/ "GreasePencilOutline",
    /*name*/ N_("Outline"),
    /*struct_name*/ "GreasePencilOutlineModifierData",
    /*struct_size*/ sizeof(GreasePencilOutlineModifierData),
    /*srna*/ &RNA_GreasePencilOutlineModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_OUTLINE,

    /*copy_data*/ blender::copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ blender::modify_geometry_set,

    /*init_data*/ blender::init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ blender::free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ blender::update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
};
