/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"

#include "BLI_math_geom.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_query.hh"
#include "BKE_material.h"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "GEO_realize_instances.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "MOD_grease_pencil_util.hh"
#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

namespace blender {

static void init_data(ModifierData *md)
{
  auto *emd = reinterpret_cast<GreasePencilEnvelopeModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(emd, modifier));

  MEMCPY_STRUCT_AFTER(emd, DNA_struct_default_get(GreasePencilEnvelopeModifierData), modifier);
  modifier::greasepencil::init_influence_data(&emd->influence, false);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *emd = reinterpret_cast<const GreasePencilEnvelopeModifierData *>(md);
  auto *temd = reinterpret_cast<GreasePencilEnvelopeModifierData *>(target);

  modifier::greasepencil::free_influence_data(&temd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&emd->influence, &temd->influence, flag);
}

static void free_data(ModifierData *md)
{
  auto *emd = reinterpret_cast<GreasePencilEnvelopeModifierData *>(md);
  modifier::greasepencil::free_influence_data(&emd->influence);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *emd = reinterpret_cast<GreasePencilEnvelopeModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&emd->influence, ob, walk, user_data);
}

static inline float3 calculate_plane(const float3 &center, const float3 &prev, const float3 &next)
{
  const float3 v1 = math::normalize(prev - center);
  const float3 v2 = math::normalize(next - center);
  return math::normalize(v1 - v2);
}

static inline std::optional<float3> find_plane_intersection(const float3 &plane_point,
                                                            const float3 &plane_normal,
                                                            const float3 &from,
                                                            const float3 &to)
{
  const float lambda = line_plane_factor_v3(plane_point, plane_normal, from, to);
  if (lambda <= 0.0f || lambda >= 1.0f) {
    return std::nullopt;
  }
  return math::interpolate(from, to, lambda);
}

/* "Infinite" radius in case no limit is applied. */
static const float unlimited_radius = FLT_MAX;

/**
 * Compute the minimal radius of a circle centered on the direction vector,
 * going through the origin and touching the line (p1, p2).
 *
 * Use plane-conic-intersections to choose the minimal radius.
 * The conic is defined in 4D as f({x,y,z,t}) = x*x + y*y + z*z - t*t = 0
 * Then a plane is defined parametrically as
 * {p}(u, v) = {p1,0}*u + {p2,0}*(1-u) + {dir,1}*v with 0 <= u <= 1 and v >= 0
 * Now compute the intersection point with the smallest t.
 * To do so, compute the parameters u, v such that f(p(u, v)) = 0 and v is minimal.
 * This can be done analytically and the solution is:
 * u = -dot(p2,dir) / dot(p1-p2, dir) +/- sqrt((dot(p2,dir) / dot(p1-p2, dir))^2 -
 * (2*dot(p1-p2,p2)*dot(p2,dir)-dot(p2,p2)*dot(p1-p2,dir))/(dot(p1-p2,dir)*dot(p1-p2,p1-p2)));
 * v = ({p1}u + {p2}*(1-u))^2 / (2*(dot(p1,dir)*u + dot(p2,dir)*(1-u)));
 */
static float calc_min_radius_v3v3(const float3 &p1, const float3 &p2, const float3 &dir)
{
  const float p1_dir = math::dot(p1, dir);
  const float p2_dir = math::dot(p2, dir);
  const float p2_sqr = math::length_squared(p2);
  const float diff_dir = p1_dir - p2_dir;

  const float u = [=]() {
    if (diff_dir == 0.0f) {
      const float p1_sqr = math::length_squared(p1);
      return p1_sqr < p2_sqr ? 1.0f : 0.0f;
    }

    const float p = p2_dir / diff_dir;
    const float3 diff = p1 - p2;
    const float diff_sqr = math::length_squared(diff);
    const float diff_p2 = math::dot(diff, p2);
    const float q = (2 * diff_p2 * p2_dir - p2_sqr * diff_dir) / (diff_dir * diff_sqr);
    if (p * p - q < 0) {
      return 0.5f - std::copysign(0.5f, p);
    }

    return math::clamp(-p - math::sqrt(p * p - q) * std::copysign(1.0f, p), 0.0f, 1.0f);
  }();

  /* v is the determined minimal radius. In case p1 and p2 are the same, there is a
   * simple proof for the following formula using the geometric mean theorem and Thales theorem. */
  const float v = math::length_squared(math::interpolate(p2, p1, u)) /
                  (2 * math::interpolate(p2_dir, p1_dir, u));
  if (v < 0 || !isfinite(v)) {
    /* No limit to the radius from this segment. */
    return unlimited_radius;
  }
  return v;
}

static float calc_radius_limit(const Span<float3> positions,
                               const bool is_cyclic,
                               const int spread,
                               const int point,
                               const float3 &direction)
{
  if (math::is_zero(direction)) {
    return unlimited_radius;
  }

  const int point_num = positions.size();
  const float3 &center = positions[point];

  float result = unlimited_radius;
  if (is_cyclic) {
    /* Spread should be limited to half the points in the cyclic case. */
    BLI_assert(spread <= point_num / 2);
    /* Left side. */
    for (const int line_i : IndexRange(spread)) {
      const int from_i = (point - line_i - 2 + point_num) % point_num;
      const int to_i = (point - line_i - 1 + point_num) % point_num;
      const float limit = calc_min_radius_v3v3(
          positions[from_i] - center, positions[to_i] - center, direction);
      result = std::min(result, limit);
    }
    /* Right side. */
    for (const int line_i : IndexRange(spread)) {
      const int from_i = (point + line_i + 1 + point_num) % point_num;
      const int to_i = (point + line_i + 2 + point_num) % point_num;
      const float limit = calc_min_radius_v3v3(
          positions[from_i] - center, positions[to_i] - center, direction);
      result = std::min(result, limit);
    }
  }
  else {
    if (point == 0 || point >= point_num - 1) {
      return unlimited_radius;
    }
    /* Left side. */
    const int spread_left = std::min(spread, std::max(point - 2, 0));
    for (const int line_i : IndexRange(spread_left)) {
      const int from_i = std::max(point - line_i - 2, 0);
      const int to_i = std::max(point - line_i - 1, 0);
      const float limit = calc_min_radius_v3v3(
          positions[from_i] - center, positions[to_i] - center, direction);
      result = std::min(result, limit);
    }
    /* Right side. */
    const int spread_right = std::min(spread, std::max(point_num - point - 2, 0));
    for (const int line_i : IndexRange(spread_right)) {
      const int from_i = std::min(point + line_i + 1, point_num - 1);
      const int to_i = std::min(point + line_i + 2, point_num - 1);
      const float limit = calc_min_radius_v3v3(
          positions[from_i] - center, positions[to_i] - center, direction);
      result = std::min(result, limit);
    }
  }
  return result;
}

/**
 * Find a suitable center and radius to enclose the envelope around a point.
 */
static bool find_envelope(const Span<float3> positions,
                          const bool is_cyclic,
                          const int spread,
                          const int point,
                          float3 &r_center,
                          float &r_radius)
{
  /* Compute a plane normal for intersections. */
  const IndexRange points = positions.index_range();
  const float3 &pos = positions[point];
  const float3 &prev_pos =
      positions[points.contains(point - 1) ? point - 1 : (is_cyclic ? points.last() : point)];
  const float3 &next_pos =
      positions[points.contains(point + 1) ? point + 1 : (is_cyclic ? points.first() : point)];
  const float3 plane_normal = calculate_plane(pos, prev_pos, next_pos);
  if (math::is_zero(plane_normal)) {
    return false;
  }

  /* Find two intersections with maximal radii. */
  float max_distance1 = 0.0f;
  float max_distance2 = 0.0f;
  float3 intersect1 = pos;
  float3 intersect2 = pos;
  for (const int line_i : IndexRange(spread + 2)) {
    /* Raw indices, can be out of range. */
    const int from_spread_i = point - spread - 1 + line_i;
    const int to_spread_i = point + line_i;
    /* Clamp or wrap to valid indices. */
    const int from_i = is_cyclic ? (from_spread_i + points.size()) % points.size() :
                                   std::max(from_spread_i, int(points.first()));
    const int to_i = is_cyclic ? (to_spread_i + points.size()) % points.size() :
                                 std::min(to_spread_i, int(points.last()));
    const float3 &from_pos = positions[from_i];
    const float3 &to_pos = positions[to_i];
    const float3 line_delta = to_pos - from_pos;

    const std::optional<float3> line_intersect = find_plane_intersection(
        pos, plane_normal, from_pos, to_pos);
    if (!line_intersect) {
      continue;
    }
    const float3 line_direction = line_intersect.value() - pos;
    const float line_distance = math::length(line_direction);

    /* Diameter of a sphere centered in the plane, touching both #pos and the intersection line. */
    const float cos_angle = math::abs(math::dot(plane_normal, line_delta)) /
                            math::length(line_delta);
    const float diameter = line_distance * 2.0f * cos_angle / (1 + cos_angle);

    if (line_i == 0) {
      max_distance1 = diameter;
      intersect1 = line_intersect.value();
      continue;
    }
    /* Use as vector 1 or 2 based on primary direction. */
    if (math::dot(intersect1 - pos, line_direction) >= 0.0f) {
      if (diameter > max_distance1) {
        intersect1 = line_intersect.value();
        max_distance1 = diameter;
      }
    }
    else {
      if (diameter > max_distance2) {
        intersect2 = line_intersect.value();
        max_distance2 = diameter;
      }
    }
  }

  r_radius = 0.5f * (max_distance1 + max_distance2);
  if (r_radius < FLT_EPSILON) {
    return false;
  }

  const float3 new_center = 0.5f * (intersect1 + intersect2);
  /* Apply radius limiting to not cross existing lines. */
  const float3 dir = math::normalize(new_center - pos);
  r_radius = std::min(r_radius, calc_radius_limit(positions, is_cyclic, spread, point, dir));

  r_center = math::interpolate(
      pos, new_center, 2.0f * r_radius / math::distance(intersect1, intersect2));

  return true;
}

static void deform_drawing_as_envelope(const GreasePencilEnvelopeModifierData &emd,
                                       bke::greasepencil::Drawing &drawing,
                                       const IndexMask &curves_mask)
{
  bke::CurvesGeometry &curves = drawing.strokes_for_write();
  const bke::AttributeAccessor attributes = curves.attributes();
  const MutableSpan<float3> positions = curves.positions_for_write();
  const MutableSpan<float> radii = drawing.radii_for_write();
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  const VArray<float> vgroup_weights = modifier::greasepencil::get_influence_vertex_weights(
      curves, emd.influence);
  const VArray<bool> cyclic_flags = *attributes.lookup_or_default(
      "cyclic", bke::AttrDomain::Curve, false);

  /* Cache to avoid affecting neighboring point results when updating positions. */
  const Array<float3> old_positions(positions.as_span());

  curves_mask.foreach_index(GrainSize(512), [&](const int64_t curve_i) {
    const IndexRange points = points_by_curve[curve_i];
    const bool cyclic = cyclic_flags[curve_i];
    const int point_num = points.size();
    const int spread = cyclic ? (math::abs(((emd.spread + point_num / 2) % point_num) -
                                           point_num / 2)) :
                                std::min(emd.spread, point_num - 1);

    for (const int64_t i : points.index_range()) {
      const int64_t point_i = points[i];
      const float weight = vgroup_weights[point_i];

      float3 envelope_center;
      float envelope_radius;
      if (!find_envelope(old_positions.as_span().slice(points),
                         cyclic,
                         spread,
                         i,
                         envelope_center,
                         envelope_radius))
      {
        continue;
      }

      const float target_radius = radii[point_i] * emd.thickness + envelope_radius;
      radii[point_i] = math::interpolate(radii[point_i], target_radius, weight);
      positions[point_i] = math::interpolate(old_positions[point_i], envelope_center, weight);
    }
  });

  drawing.tag_positions_changed();
  curves.tag_radii_changed();
}

struct EnvelopeInfo {
  /* Offset left and right from the source point. */
  int spread;
  /* Number of points to skip. */
  int skip;
  /* Number of points in each envelope stroke. */
  int points_per_curve;
  /* Material index assigned to new strokes. */
  int material_index;
  float thickness;
  float strength;
};

static EnvelopeInfo get_envelope_info(const GreasePencilEnvelopeModifierData &emd,
                                      const ModifierEvalContext &ctx)
{
  EnvelopeInfo info;
  info.spread = emd.spread;
  info.skip = emd.skip;
  switch (GreasePencilEnvelopeModifierMode(emd.mode)) {
    case MOD_GREASE_PENCIL_ENVELOPE_DEFORM:
      info.points_per_curve = 0;
      break;
    case MOD_GREASE_PENCIL_ENVELOPE_SEGMENTS:
      info.points_per_curve = 2;
      break;
    case MOD_GREASE_PENCIL_ENVELOPE_FILLS:
      info.points_per_curve = 2 * (2 + emd.skip);
      break;
  }
  info.material_index = std::min(emd.mat_nr, ctx.object->totcol - 1);
  info.thickness = emd.thickness;
  info.strength = emd.strength;
  return info;
}

static int curve_spread(const EnvelopeInfo &info, const int point_num, const bool is_cyclic_curve)
{
  /* Clamp spread in the cyclic case to half the curve size. */
  return is_cyclic_curve ? std::min(info.spread, point_num / 2) : info.spread;
}

static int curve_envelope_strokes_num(const EnvelopeInfo &info,
                                      const int point_num,
                                      const bool is_cyclic_curve)
{
  const int spread = curve_spread(info, point_num, is_cyclic_curve);
  /* Number of envelope strokes making up the envelope. */
  const int num_strokes = point_num + spread - 1;
  /* Skip strokes (only every n-th point generates strokes). */
  const int num_strokes_simplified = (num_strokes + info.skip) / (1 + info.skip);
  return num_strokes_simplified;
}

/**
 * Create a single stroke as part of the envelope.
 *
 * In the simplest cast creates a single edge.
 * Example for spread 4:
 *
 *  (p-5) (p-4) (p-3) (p-2) (p-1) ( p ) (p+1) (p+2) (p+3) (p+4) (p+5)
 *           └---------------------------┘
 *
 * If fills is true a closed curve is created that connects contiguous point ranges.
 * Example (skip=0):
 *
 *  (p-5) (p-4) (p-3) (p-2) (p-1) ( p ) (p+1) (p+2) (p+3) (p+4) (p+5)
 *         | └---┘ └---------------------┘ └---┘ |
 *         |                                     |
 *         └-------------------------------------┘
 *
 * If skip > 0 more points are included in the range.
 * Example (skip=2):
 *
 *  (p-5) (p-4) (p-3) (p-2) (p-1) ( p ) (p+1) (p+2) (p+3) (p+4) (p+5)
 *         | └---┘ └---┘ └---┘ └---------┘ └---┘ └---┘ └---┘ |
 *         |                                                 |
 *         └-------------------------------------------------┘
 */
static void create_envelope_stroke_for_point(const IndexRange src_curve_points,
                                             const bool src_curve_cyclic,
                                             const int point,
                                             const int spread,
                                             const int base_length,
                                             const MutableSpan<int> point_src_indices)
{
  const int point_num = src_curve_points.size();
  BLI_assert(point_src_indices.size() == base_length * 2);

  /* Clamp or wrap to ensure a valid index. */
  auto get_index = [=](const int index) -> int {
    return src_curve_cyclic ? (index + point_num) % point_num :
                              math::clamp(index, 0, point_num - 1);
  };

  for (const int i : IndexRange(base_length)) {
    const int reverse_i = base_length - 1 - i;
    const int point_left = get_index(point - spread + reverse_i);
    const int point_right = get_index(point + reverse_i);
    point_src_indices[i] = src_curve_points[point_left];
    point_src_indices[base_length + i] = src_curve_points[point_right];
  }
}

static void create_envelope_strokes_for_curve(const EnvelopeInfo &info,
                                              const int src_curve_index,
                                              const IndexRange src_curve_points,
                                              const bool src_curve_cyclic,
                                              const VArray<int> &src_material_indices,
                                              const IndexRange dst_points,
                                              const MutableSpan<int> curve_offsets,
                                              const MutableSpan<int> material_indices,
                                              const MutableSpan<int> curve_src_indices,
                                              const MutableSpan<int> point_src_indices)
{
  const int src_point_num = src_curve_points.size();
  const int spread = curve_spread(info, src_point_num, src_curve_cyclic);
  const int num_strokes = curve_envelope_strokes_num(info, src_point_num, src_curve_cyclic);
  const bool use_fills = info.points_per_curve > 2;
  /* Length of continuous point ranges that get connected. */
  const int base_length = use_fills ? 2 + info.skip : 1;

  BLI_assert(curve_offsets.size() == num_strokes);
  BLI_assert(material_indices.size() == num_strokes);
  BLI_assert(curve_src_indices.size() == num_strokes);
  BLI_assert(point_src_indices.size() == num_strokes * info.points_per_curve);

  curve_src_indices.fill(src_curve_index);

  /*
   * Index range here goes beyond the point range:
   * This adds points [i - spread, i + 1] as a curve.
   * The total range covers [-spread - 1, spread + 1].
   * Each span only gets added once since it repeats for neighboring points.
   */

  for (const int i : IndexRange(num_strokes)) {
    const IndexRange dst_envelope_points = {i * info.points_per_curve, info.points_per_curve};

    curve_offsets[i] = dst_points[dst_envelope_points.start()];
    material_indices[i] = info.material_index >= 0 ? info.material_index :
                                                     src_material_indices[src_curve_index];

    create_envelope_stroke_for_point(src_curve_points,
                                     src_curve_cyclic,
                                     i,
                                     spread,
                                     base_length,
                                     point_src_indices.slice(dst_envelope_points));
  }
}

static void create_envelope_strokes(const EnvelopeInfo &info,
                                    bke::greasepencil::Drawing &drawing,
                                    const IndexMask &curves_mask,
                                    const bool keep_original)
{
  const bke::CurvesGeometry &src_curves = drawing.strokes();
  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  const VArray<bool> src_cyclic = *src_attributes.lookup_or_default(
      "cyclic", bke::AttrDomain::Curve, false);
  const VArray<int> src_material_indices = *src_attributes.lookup_or_default(
      "material_index", bke::AttrDomain::Curve, 0);

  /* Count envelopes. */
  Array<int> envelope_curves_by_curve(src_curves.curve_num + 1);
  Array<int> envelope_points_by_curve(src_curves.curve_num + 1);
  curves_mask.foreach_index([&](const int64_t src_curve_i) {
    const IndexRange points = src_curves.points_by_curve()[src_curve_i];
    const int curve_num = curve_envelope_strokes_num(info, points.size(), src_cyclic[src_curve_i]);
    envelope_curves_by_curve[src_curve_i] = curve_num;
    envelope_points_by_curve[src_curve_i] = info.points_per_curve * curve_num;
  });
  /* Ranges by source curve for envelope curves and points. */
  const OffsetIndices envelope_curve_offsets = offset_indices::accumulate_counts_to_offsets(
      envelope_curves_by_curve, keep_original ? src_curves.curve_num : 0);
  const OffsetIndices envelope_point_offsets = offset_indices::accumulate_counts_to_offsets(
      envelope_points_by_curve, keep_original ? src_curves.point_num : 0);
  const int dst_curve_num = envelope_curve_offsets.total_size();
  const int dst_point_num = envelope_point_offsets.total_size();
  if (dst_curve_num == 0 || dst_point_num == 0) {
    return;
  }

  bke::CurvesGeometry dst_curves(dst_point_num, dst_curve_num);
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
  bke::SpanAttributeWriter<int> dst_material_indices =
      dst_attributes.lookup_or_add_for_write_span<int>("material_index", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<bool> dst_cyclic = dst_attributes.lookup_or_add_for_write_span<bool>(
      "cyclic", bke::AttrDomain::Curve);
  /* Map each destination curve and point to its source. */
  Array<int> src_curve_indices(dst_curve_num);
  Array<int> src_point_indices(dst_point_num);

  if (keep_original) {
    /* Add indices to original data. */
    dst_curves.offsets_for_write()
        .slice(src_curves.curves_range())
        .copy_from(src_curves.offsets().drop_back(1));

    array_utils::fill_index_range(
        src_curve_indices.as_mutable_span().slice(src_curves.curves_range()));
    array_utils::fill_index_range(
        src_point_indices.as_mutable_span().slice(src_curves.points_range()));

    array_utils::copy(src_material_indices,
                      dst_material_indices.span.slice(src_curves.curves_range()));
  }

  curves_mask.foreach_index([&](const int64_t i) {
    const bool src_curve_cyclic = src_cyclic[i];
    const IndexRange src_curve_points = src_curves.points_by_curve()[i];
    const IndexRange envelope_curves = envelope_curve_offsets[i];
    const IndexRange envelope_points = envelope_point_offsets[i];

    create_envelope_strokes_for_curve(info,
                                      i,
                                      src_curve_points,
                                      src_curve_cyclic,
                                      src_material_indices,
                                      envelope_points,
                                      dst_curves.offsets_for_write().slice(envelope_curves),
                                      dst_material_indices.span.slice(envelope_curves),
                                      src_curve_indices.as_mutable_span().slice(envelope_curves),
                                      src_point_indices.as_mutable_span().slice(envelope_points));
  });
  dst_curves.offsets_for_write().last() = dst_point_num;

  bke::gather_attributes(
      src_attributes, bke::AttrDomain::Point, {}, {}, src_point_indices, dst_attributes);
  bke::gather_attributes(src_attributes,
                         bke::AttrDomain::Curve,
                         {},
                         {"cyclic", "material_index"},
                         src_curve_indices,
                         dst_attributes);

  /* Apply thickness and strength factors. */
  {
    bke::SpanAttributeWriter<float> radius_writer =
        dst_attributes.lookup_or_add_for_write_span<float>(
            "radius",
            bke::AttrDomain::Point,
            bke::AttributeInitVArray(VArray<float>::ForSingle(0.01f, dst_point_num)));
    bke::SpanAttributeWriter<float> opacity_writer =
        dst_attributes.lookup_or_add_for_write_span<float>(
            "opacity",
            bke::AttrDomain::Point,
            bke::AttributeInitVArray(VArray<float>::ForSingle(1.0f, dst_point_num)));
    const IndexRange all_new_points = keep_original ?
                                          IndexRange(src_curves.point_num,
                                                     dst_point_num - src_curves.point_num) :
                                          IndexRange(dst_point_num);
    for (const int point_i : all_new_points) {
      radius_writer.span[point_i] *= info.thickness;
      opacity_writer.span[point_i] *= info.strength;
    }
    radius_writer.finish();
    opacity_writer.finish();
  }

  dst_cyclic.finish();
  dst_material_indices.finish();
  dst_curves.update_curve_types();

  drawing.strokes_for_write() = std::move(dst_curves);
  drawing.tag_topology_changed();
}

static void modify_drawing(const GreasePencilEnvelopeModifierData &emd,
                           const ModifierEvalContext &ctx,
                           bke::greasepencil::Drawing &drawing)
{
  const EnvelopeInfo info = get_envelope_info(emd, ctx);

  IndexMaskMemory mask_memory;
  const IndexMask curves_mask = modifier::greasepencil::get_filtered_stroke_mask(
      ctx.object, drawing.strokes(), emd.influence, mask_memory);

  const auto mode = GreasePencilEnvelopeModifierMode(emd.mode);
  switch (mode) {
    case MOD_GREASE_PENCIL_ENVELOPE_DEFORM:
      deform_drawing_as_envelope(emd, drawing, curves_mask);
      break;
    case MOD_GREASE_PENCIL_ENVELOPE_SEGMENTS:
      create_envelope_strokes(info, drawing, curves_mask, true);
      break;
    case MOD_GREASE_PENCIL_ENVELOPE_FILLS:
      create_envelope_strokes(info, drawing, curves_mask, false);
      break;
  }
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;

  auto *emd = reinterpret_cast<GreasePencilEnvelopeModifierData *>(md);

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();
  const int frame = grease_pencil.runtime->eval_frame;

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, emd->influence, mask_memory);

  const Vector<Drawing *> drawings = modifier::greasepencil::get_drawings_for_write(
      grease_pencil, layer_mask, frame);
  threading::parallel_for_each(drawings,
                               [&](Drawing *drawing) { modify_drawing(*emd, *ctx, *drawing); });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  const GreasePencilEnvelopeModifierMode mode = GreasePencilEnvelopeModifierMode(
      RNA_enum_get(ptr, "mode"));

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "spread", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "thickness", UI_ITEM_NONE, nullptr, ICON_NONE);

  switch (mode) {
    case MOD_GREASE_PENCIL_ENVELOPE_DEFORM:
      break;
    case MOD_GREASE_PENCIL_ENVELOPE_FILLS:
    case MOD_GREASE_PENCIL_ENVELOPE_SEGMENTS:
      uiItemR(layout, ptr, "strength", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, ptr, "mat_nr", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(layout, ptr, "skip", UI_ITEM_NONE, nullptr, ICON_NONE);
      break;
  }

  if (uiLayout *influence_panel = uiLayoutPanelProp(
          C, layout, ptr, "open_influence_panel", "Influence"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_material_filter_settings(C, influence_panel, ptr);
    modifier::greasepencil::draw_vertex_group_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilEnvelope, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *emd = reinterpret_cast<const GreasePencilEnvelopeModifierData *>(md);

  BLO_write_struct(writer, GreasePencilEnvelopeModifierData, emd);
  modifier::greasepencil::write_influence_data(writer, &emd->influence);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *emd = reinterpret_cast<GreasePencilEnvelopeModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &emd->influence);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilEnvelope = {
    /*idname*/ "GreasePencilEnvelope",
    /*name*/ N_("Envelope"),
    /*struct_name*/ "GreasePencilEnvelopeModifierData",
    /*struct_size*/ sizeof(GreasePencilEnvelopeModifierData),
    /*srna*/ &RNA_GreasePencilEnvelopeModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_ENVELOPE,

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
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
};
