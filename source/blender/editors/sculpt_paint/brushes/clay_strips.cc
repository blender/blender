/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 *
 * The Clay Strips brush displaces vertices toward the brush plane.
 * The displacement occurs in the direction of the plane's normal.
 * Only vertices located below the plane (in brush-local space) are affected.
 *
 * The magnitude of the displacement is determined by the product of the following factors:
 * - A falloff factor based on the XY distance from the center of the plane
 * - A parabolic falloff factor based on the local Z distance from the plane
 * - Additional standard modifiers such as masking, brush strength, texture masking, etc.
 */

#include "editors/sculpt_paint/brushes/brushes.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::brushes {

inline namespace clay_strips_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float2> xy_positions;
  Vector<float> z_positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

/**
 * Transforms positions from object space positions to brush-local space. Splitting the XY and Z
 * components gives slightly better performance.
 */
static void calc_local_positions(const Span<float3> vert_positions,
                                 const Span<int> verts,
                                 const float4x4 &mat,
                                 const MutableSpan<float2> xy_positions,
                                 const MutableSpan<float> z_positions)
{
  BLI_assert(xy_positions.size() == verts.size());
  BLI_assert(z_positions.size() == verts.size());

  for (const int i : verts.index_range()) {
    const float3 position = math::transform_point(mat, vert_positions[verts[i]]);

    xy_positions[i] = position.xy();
    z_positions[i] = position.z;
  }
}

static void calc_local_positions(const Span<float3> positions,
                                 const float4x4 &mat,
                                 const MutableSpan<float2> xy_positions,
                                 const MutableSpan<float> z_positions)
{
  BLI_assert(xy_positions.size() == positions.size());
  BLI_assert(z_positions.size() == positions.size());

  for (const int i : positions.index_range()) {
    const float3 position = math::transform_point(mat, positions[i]);

    xy_positions[i] = position.xy();
    z_positions[i] = position.z;
  }
}

/**
 * Applies a parabolic factor of the form `z * (1 - z)` to each vertex.
 * Vertices outside of the interval (0, 1) are out of range and their factors are set to zero.
 * Note: The local coordinate system is constructed such that all relevant `z` values
 * are non-negative.
 */
static void apply_z_axis_factors(const Span<float> z_positions, const MutableSpan<float> factors)
{
  BLI_assert(factors.size() == z_positions.size());

  for (const int i : factors.index_range()) {
    const float local_z = z_positions[i];

    /* Note: if `local_z > 1`, then `1 - local_z < 0` and the product is negative. */
    factors[i] *= math::max(0.0f, local_z * (1.0f - local_z));
  }
}

/**
 * If plane trim is enabled, vertices with `z` values greater than the
 * specified `plane_trim` threshold are ignored (i.e., their factors are set to zero).
 */
static void apply_plane_trim_factors(const Brush &brush,
                                     const Span<float> z_positions,
                                     const MutableSpan<float> factors)
{
  BLI_assert(factors.size() == z_positions.size());

  const bool use_plane_trim = brush.flag & BRUSH_PLANE_TRIM;
  if (!use_plane_trim) {
    return;
  }

  for (const int i : factors.index_range()) {
    if (z_positions[i] > brush.plane_trim) {
      factors[i] = 0.0f;
    }
  }
}

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const float4x4 &mat,
                       const float3 &offset,
                       const Span<float3> vert_normals,
                       const MeshAttributeData &attribute_data,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       LocalData &tls,
                       const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, position_data.eval, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  tls.xy_positions.resize(verts.size());
  tls.z_positions.resize(verts.size());
  MutableSpan<float2> xy_positions = tls.xy_positions;
  MutableSpan<float> z_positions = tls.z_positions;

  calc_local_positions(position_data.eval, verts, mat, xy_positions, z_positions);
  apply_z_axis_factors(z_positions, factors);
  apply_plane_trim_factors(brush, z_positions, factors);

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_cube_distances<float2>(brush, xy_positions, distances);
  filter_distances_with_radius(1.0f, distances, factors);
  apply_hardness_to_distances(1.0f, cache.hardness, distances);
  BKE_brush_calc_curve_factors(eBrushCurvePreset(brush.curve_distance_falloff_preset),
                               brush.curve_distance_falloff,
                               distances,
                               1.0f,
                               factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, position_data.eval, verts, factors);

  tls.translations.resize(verts.size());
  translations_from_offset_and_factors(offset, factors, tls.translations);

  clip_and_lock_translations(sd, ss, position_data.eval, verts, tls.translations);
  position_data.deform(tls.translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float4x4 &mat,
                       const float3 &offset,
                       const bke::pbvh::GridsNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, subdiv_ccg, grids, factors);
  }

  tls.xy_positions.resize(positions.size());
  tls.z_positions.resize(positions.size());
  MutableSpan<float2> xy_positions = tls.xy_positions;
  MutableSpan<float> z_positions = tls.z_positions;

  calc_local_positions(positions, mat, xy_positions, z_positions);
  apply_z_axis_factors(z_positions, factors);
  apply_plane_trim_factors(brush, z_positions, factors);

  tls.distances.resize(positions.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_cube_distances<float2>(brush, xy_positions, distances);
  filter_distances_with_radius(1.0f, distances, factors);
  apply_hardness_to_distances(1.0f, cache.hardness, distances);
  BKE_brush_calc_curve_factors(eBrushCurvePreset(brush.curve_distance_falloff_preset),
                               brush.curve_distance_falloff,
                               distances,
                               1.0f,
                               factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.translations.resize(positions.size());
  translations_from_offset_and_factors(offset, factors, tls.translations);

  clip_and_lock_translations(sd, ss, positions, tls.translations);
  apply_translations(tls.translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float4x4 &mat,
                       const float3 &offset,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, verts, factors);
  }

  tls.xy_positions.resize(positions.size());
  tls.z_positions.resize(positions.size());
  MutableSpan<float2> xy_positions = tls.xy_positions;
  MutableSpan<float> z_positions = tls.z_positions;

  calc_local_positions(positions, mat, xy_positions, z_positions);
  apply_z_axis_factors(z_positions, factors);
  apply_plane_trim_factors(brush, z_positions, factors);

  tls.distances.resize(positions.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_cube_distances<float2>(brush, xy_positions, distances);
  filter_distances_with_radius(1.0f, distances, factors);
  apply_hardness_to_distances(1.0f, cache.hardness, distances);
  BKE_brush_calc_curve_factors(eBrushCurvePreset(brush.curve_distance_falloff_preset),
                               brush.curve_distance_falloff,
                               distances,
                               1.0f,
                               factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.translations.resize(positions.size());
  translations_from_offset_and_factors(offset, factors, tls.translations);

  clip_and_lock_translations(sd, ss, positions, tls.translations);
  apply_translations(tls.translations, verts);
}

}  // namespace clay_strips_cc

void do_clay_strips_brush(const Depsgraph &depsgraph,
                          const Sculpt &sd,
                          Object &object,
                          const IndexMask &node_mask,
                          const float3 &plane_normal,
                          const float3 &plane_center)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const bool flip = (ss.cache->bstrength < 0.0f);

  /* Note: This return has to happen *after* the call to calc_brush_plane for now, as
   * the method is not idempotent and sets variables inside the stroke cache. */
  if (math::is_zero(ss.cache->grab_delta_symm)) {
    return;
  }

  const float4x4 mat = clay_strips::calc_local_matrix(
      brush, *ss.cache, plane_normal, plane_center, flip);
  const float3 offset = plane_normal * ss.cache->bstrength * ss.cache->radius;

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      const PositionDeformData position_data(depsgraph, object);
      const MeshAttributeData attribute_data(mesh);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_faces(depsgraph,
                   sd,
                   brush,
                   mat,
                   offset,
                   vert_normals,
                   attribute_data,
                   nodes[i],
                   object,
                   tls,
                   position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_grids(depsgraph, sd, object, brush, mat, offset, nodes[i], tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_bmesh(depsgraph, sd, object, brush, mat, offset, nodes[i], tls);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

namespace clay_strips {

/**
 * Checks whether the node's bounding box overlaps with the region affected by the brush.
 * Clay Strips affects only vertices below the brush plane. The brush-local coordinate
 * system is oriented so that vertices below the plane have positive local z-coordinates.
 * Therefore, we only need to check if the node intersects the [-1,1] x [-1,1] x [0,1] volume in
 * local space.
 */
static bool node_in_box(const float4x4 &mat, const Bounds<float3> &bounds)
{
  const float3 brush_center = float3(0.0f, 0.0f, 0.5f);
  const float3 node_center = math::transform_point(mat, (bounds.max + bounds.min) * 0.5f);
  const float3 center_diff = brush_center - node_center;

  const float3 brush_half_lengths = float3(1.0f, 1.0f, 0.5f);
  const float3 node_half_lengths = (bounds.max - bounds.min) * 0.5f;

  const float3 &node_x_axis = mat.x_axis();
  const float3 &node_y_axis = mat.y_axis();
  const float3 &node_z_axis = mat.z_axis();

  /* Tests if `axis` separates the boxes. */
  auto axis_separates_boxes = [&](const float3 &axis) {
    const float radius1 = math::dot(math::abs(axis), brush_half_lengths);
    const float radius2 = math::abs(math::dot(axis, node_x_axis)) * node_half_lengths.x +
                          math::abs(math::dot(axis, node_y_axis)) * node_half_lengths.y +
                          math::abs(math::dot(axis, node_z_axis)) * node_half_lengths.z;

    const float projection = math::abs(math::dot(center_diff, axis));

    return projection > radius1 + radius2;
  };

  const std::array<float3, 3> brush_axes = {
      float3{1.0f, 0.0f, 0.0f}, float3{0.0f, 1.0f, 0.0f}, float3{0.0f, 0.0f, 1.0f}};
  const std::array<float3, 3> node_axes = {node_x_axis, node_y_axis, node_z_axis};

  /**
   * Intersection is tested using the Separating Axis Theorem.
   * Two boxes (not necessarily axis-aligned) intersect if and only if there does not exist an axis
   * that separates them. In particular, it is necessary and sufficient to:
   */

  /* 1. Test axes aligned with the region affected by the brush. */
  for (const float3 &axis : brush_axes) {
    if (axis_separates_boxes(axis)) {
      return false;
    }
  }

  /* 2. Test axes aligned with the node bounds. */
  for (const float3 &axis : node_axes) {
    if (axis_separates_boxes(axis)) {
      return false;
    }
  }

  /* 3. Test all their cross products. */
  for (const float3 &brush_axis : brush_axes) {
    for (const float3 &node_axis : node_axes) {
      if (axis_separates_boxes(math::cross(brush_axis, node_axis))) {
        return false;
      }
    }
  }

  /* None of the axes separates the boxes: they intersect. */
  return true;
}

float4x4 calc_local_matrix(const Brush &brush,
                           const StrokeCache &cache,
                           const float3 &plane_normal,
                           const float3 &plane_center,
                           const bool flip)
{
  float4x4 mat = float4x4::identity();
  mat.x_axis() = math::cross(plane_normal, cache.grab_delta_symm);
  mat.y_axis() = math::cross(plane_normal, mat.x_axis());
  mat.z_axis() = plane_normal;

  /* Flip the z-axis so that the vertices below the plane have positive z-coordinates. When the
   * brush is inverted, the affected z-coordinates are already positive. */
  if (!flip) {
    mat.z_axis() *= -1.0f;
  }

  mat.location() = plane_center;
  mat = math::normalize(mat);

  /* Scale brush local space matrix. */
  const float4x4 scale = math::from_scale<float4x4>(float3(cache.radius));
  float4x4 tmat = mat * scale;
  tmat.y_axis() *= brush.tip_scale_x;
  mat = math::invert(tmat);
  return mat;
}

CursorSampleResult calc_node_mask(const Depsgraph &depsgraph,
                                  Object &object,
                                  const Brush &brush,
                                  IndexMaskMemory &memory)
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const SculptSession &ss = *object.sculpt;

  const bool flip = (ss.cache->bstrength < 0.0f);
  const float displace = ss.cache->radius * brush_plane_offset_get(brush, ss) *
                         (flip ? -1.0f : 1.0f);

  /* TODO: Test to see if the sqrt2 extra factor can be removed */
  const float initial_radius_squared = math::square(ss.cache->radius * math::numbers::sqrt2);

  const bool use_original = !ss.cache->accum;
  const IndexMask initial_node_mask = gather_nodes(pbvh,
                                                   eBrushFalloffShape(brush.falloff_shape),
                                                   use_original,
                                                   ss.cache->location_symm,
                                                   initial_radius_squared,
                                                   ss.cache->view_normal_symm,
                                                   memory);

  float3 plane_center;
  float3 plane_normal;
  calc_brush_plane(depsgraph, brush, object, initial_node_mask, plane_normal, plane_center);
  plane_normal = tilt_apply_to_normal(plane_normal, *ss.cache, brush.tilt_strength_factor);
  plane_center += plane_normal * ss.cache->scale * displace;

  if (math::is_zero(ss.cache->grab_delta_symm) || math::is_zero(plane_normal)) {
    /* The brush local matrix is degenerate: return an empty index mask. */
    return {IndexMask(), plane_center, plane_normal};
  }

  const float4x4 mat = calc_local_matrix(brush, *ss.cache, plane_normal, plane_center, flip);

  const IndexMask plane_mask = bke::pbvh::search_nodes(
      pbvh, memory, [&](const bke::pbvh::Node &node) {
        if (node_fully_masked_or_hidden(node)) {
          return false;
        }
        return node_in_box(mat, node.bounds());
      });

  return {plane_mask, plane_center, plane_normal};
}
}  // namespace clay_strips

}  // namespace blender::ed::sculpt_paint::brushes
