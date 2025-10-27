/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 *
 * The Plane brush translates the vertices towards the brush plane.
 * The vertices are displaced along the direction parallel to the normal of the plane.
 *
 * The z-distances of the vertices are affected by two parameters:
 *  - Height: Affects the z-distances of the vertices above the plane.
 *  - Depth: Affects the z-distances of the vertices below the plane.
 *
 * Invert Modes:
 *  - Invert Displacement: Reverses the default behavior, displacing
 *    vertices away from the plane.
 *  - Swap Height and Depth: Exchanges the roles of Height and Depth.
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
#include "BLI_math_matrix.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::brushes {

inline namespace plane_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float3> local_positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

static void calc_local_positions(const float4x4 &mat,
                                 const Span<int> verts,
                                 const Span<float3> positions,
                                 const MutableSpan<float3> local_positions)
{
  for (const int i : verts.index_range()) {
    local_positions[i] = math::transform_point(mat, positions[verts[i]]);
  }
}

/**
 * Computes the local distances. For vertices above the plane,
 * the z-distances are divided by `height`, effectively scaling the
 * z-distances so that a vertex of local coordinates
 * `(0, 0, height)` has a z-distance of 1.
 *
 * When `height` is 0, the local distances are set to 1. In object space, this is
 * equivalent to setting the distances equal to the radius, resulting in
 * a falloff strength of 0 (no displacement).
 *
 * The effect of `depth` on vertices below the plane is analogous.
 */
static void calc_local_distances(const float height,
                                 const float depth,
                                 const MutableSpan<float3> local_positions,
                                 const MutableSpan<float> distances)
{
  if (height != 0.0f) {
    const float height_rcp = math::rcp(height);

    for (const int i : local_positions.index_range()) {
      const float3 &position = local_positions[i];
      if (position.z >= 0.0f) {
        distances[i] = math::length(float3(position.x, position.y, position.z * height_rcp));
      }
    }
  }
  else {
    for (const int i : local_positions.index_range()) {
      if (local_positions[i].z >= 0.0f) {
        distances[i] = 1.0f;
      }
    }
  }

  if (depth != 0.0f) {
    const float depth_rcp = math::rcp(depth);

    for (const int i : local_positions.index_range()) {
      const float3 &position = local_positions[i];
      if (position.z < 0.0f) {
        distances[i] = math::length(float3(position.x, position.y, position.z * depth_rcp));
      }
    }
  }
  else {
    for (const int i : local_positions.index_range()) {
      if (local_positions[i].z < 0.0f) {
        distances[i] = 1.0f;
      }
    }
  }
}

/*
 * Scales factors by `height` (if local z > 0) or `depth` (if local z < 0).
 * This is necessary to "normalize" the strength contribute given by the local z distance.
 *
 * Note that if `height = 0`, the falloff strength of the vertices
 * above the plane is 0 (see #calc_local_distances), hence
 * the factor for such vertices is already 0.
 *
 * The same is true for vertices below the plane if `depth` = 0.
 */
static void scale_factors_by_height_and_depth(const float height,
                                              const float depth,
                                              const MutableSpan<float3> local_positions,
                                              const MutableSpan<float> factors)
{
  if (!ELEM(height, 1.0f, 0.0f)) {
    for (const int i : factors.index_range()) {
      if (local_positions[i].z > 0.0f) {
        factors[i] *= height;
      }
    }
  }

  if (!ELEM(depth, 0.0f, 1.0f)) {
    for (const int i : factors.index_range()) {
      if (local_positions[i].z < 0.0f) {
        factors[i] *= depth;
      }
    }
  }
}

/*
 * Computes the translation vectors for the Plane brush.
 *
 * The translation of a vertex with index `i`, position `P`, and plane projection `Q`
 * is determined by the vector `PQ` scaled by `factors[i]` and `strength`.
 *
 * In local (brush) space, `PQ` is given by:
 *    `PQ = Q - P = (x, y, 0) - (x, y, z) = (0, 0, -z)`
 *
 * Therefore, the translation in object space is:
 *    `T = A * (0, 0, -z) * factors[i] * strength`
 *
 * where `A` is the 3x3 local-to-object transformation matrix.
 *
 * Given how the local space is defined, `A * (0, 0, -z)` simplifies to:
 *    `-z * radius * plane_normal`
 *
 * Substituting this back, the translation becomes:
 *    `T = -z * radius * plane_normal * factors[i] * strength`
 * which is equal to:
 *     `(factors[i] * z) * (-plane_normal * radius * strength)`
 */
static void calc_translations(const float3 &plane_normal,
                              const float radius,
                              const float strength,
                              const MutableSpan<float3> local_positions,
                              const MutableSpan<float> factors,
                              const MutableSpan<float3> r_translations)
{
  for (const int i : local_positions.index_range()) {
    factors[i] *= local_positions[i].z;
  }

  const float3 &offset = -plane_normal * radius * strength;
  translations_from_offset_and_factors(offset, factors, r_translations);
}

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const float4x4 &mat,
                       const float3 &plane_normal,
                       const float strength,
                       const float height,
                       const float depth,
                       const MeshAttributeData &attribute_data,
                       const Span<float3> vert_normals,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       LocalData &tls,
                       const PositionDeformData &position_data)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, position_data.eval, verts, factors);

  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  tls.positions.resize(verts.size());
  const MutableSpan<float3> local_positions = tls.positions;
  calc_local_positions(mat, verts, position_data.eval, local_positions);

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_local_distances(height, depth, local_positions, distances);
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
  const MutableSpan<float3> translations = tls.translations;
  scale_factors_by_height_and_depth(height, depth, local_positions, factors);
  calc_translations(plane_normal, cache.radius, strength, local_positions, factors, translations);

  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float4x4 &mat,
                       const float3 &plane_normal,
                       const float strength,
                       const float height,
                       const float depth,
                       bke::pbvh::GridsNode &node,
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

  tls.local_positions.resize(positions.size());
  const MutableSpan<float3> local_positions = tls.local_positions;
  math::transform_points(positions, mat, local_positions, false);

  tls.distances.resize(positions.size());
  const MutableSpan<float> distances = tls.distances;
  calc_local_distances(height, depth, local_positions, distances);
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
  const MutableSpan<float3> translations = tls.translations;
  scale_factors_by_height_and_depth(height, depth, local_positions, factors);
  calc_translations(plane_normal, cache.radius, strength, local_positions, factors, translations);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float4x4 &mat,
                       const float3 &plane_normal,
                       const float strength,
                       const float height,
                       const float depth,
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

  tls.local_positions.resize(positions.size());
  const MutableSpan<float3> local_positions = tls.local_positions;
  math::transform_points(positions, mat, local_positions, false);

  tls.distances.resize(positions.size());
  const MutableSpan<float> distances = tls.distances;
  calc_local_distances(height, depth, local_positions, distances);
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
  const MutableSpan<float3> translations = tls.translations;
  scale_factors_by_height_and_depth(height, depth, local_positions, factors);
  calc_translations(plane_normal, cache.radius, strength, local_positions, factors, translations);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace plane_cc

void do_plane_brush(const Depsgraph &depsgraph,
                    const Sculpt &sd,
                    Object &object,
                    const IndexMask &node_mask,
                    const float3 &plane_normal,
                    const float3 &plane_center)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (math::is_zero(ss.cache->grab_delta_symm)) {
    return;
  }

  float3 normal = plane_normal;
  float3 center = plane_center;

  normal = tilt_apply_to_normal(normal, *ss.cache, brush.tilt_strength_factor);

  const bool flip = ss.cache->initial_direction_flipped;
  const float offset = brush_plane_offset_get(brush, ss);
  const float displace = ss.cache->radius * offset * (flip ? -1.0f : 1.0f);
  center += normal * ss.cache->scale * displace;

  float4x4 mat = float4x4::identity();
  mat.x_axis() = math::cross(normal, ss.cache->grab_delta_symm);
  mat.y_axis() = math::cross(normal, mat.x_axis());
  mat.z_axis() = normal;
  mat.location() = center;
  mat = math::normalize(mat);

  const float4x4 scale = math::from_scale<float4x4>(float3(ss.cache->radius));
  float4x4 tmat = mat * scale;

  mat = math::invert(tmat);

  float strength = ss.cache->bstrength;
  float height = brush.plane_height;
  float depth = brush.plane_depth;

  if (flip) {
    switch (brush.plane_inversion_mode) {
      case BRUSH_PLANE_INVERT_DISPLACEMENT: {
        strength *= -1.0f;
        break;
      }
      case BRUSH_PLANE_SWAP_HEIGHT_AND_DEPTH: {
        std::swap(height, depth);
        break;
      }
    }
  }

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_faces(depsgraph,
                   sd,
                   brush,
                   mat,
                   normal,
                   strength,
                   height,
                   depth,
                   attribute_data,
                   vert_normals,
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
        calc_grids(
            depsgraph, sd, object, brush, mat, normal, strength, height, depth, nodes[i], tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_bmesh(
            depsgraph, sd, object, brush, mat, normal, strength, height, depth, nodes[i], tls);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

namespace plane {
CursorSampleResult calc_node_mask(const Depsgraph &depsgraph,
                                  Object &ob,
                                  const Brush &brush,
                                  IndexMaskMemory &memory)
{
  const SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  const bool use_original = !ss.cache->accum;
  const IndexMask initial_node_mask = gather_nodes(pbvh,
                                                   eBrushFalloffShape(brush.falloff_shape),
                                                   use_original,
                                                   ss.cache->location_symm,
                                                   ss.cache->radius_squared,
                                                   ss.cache->view_normal_symm,
                                                   memory);

  float3 plane_center;
  float3 plane_normal;
  calc_brush_plane(depsgraph, brush, ob, initial_node_mask, plane_normal, plane_center);

  /* Recompute the node mask using the center of the brush plane as the center.
   *
   * The indices of the nodes in `cursor_node_mask` have been calculated based on the cursor
   * location. However, for the Plane brush, its effective center often deviates from the cursor
   * location. Calculating the affected nodes using the cursor location as the center can lead to
   * issues (see, for example, #123768). */
  const IndexMask plane_mask = bke::pbvh::search_nodes(
      pbvh, memory, [&](const bke::pbvh::Node &node) {
        if (node_fully_masked_or_hidden(node)) {
          return false;
        }
        return node_in_sphere(node, plane_center, ss.cache->radius_squared, use_original);
      });

  return {plane_mask, plane_center, plane_normal};
}
}  // namespace plane

}  // namespace blender::ed::sculpt_paint::brushes
