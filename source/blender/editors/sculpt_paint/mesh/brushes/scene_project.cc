/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 *
 * The Scene Project brush projects vertices of the active object towards the surfaces of other
 * objects in the scene. Using raycasting along the specified direction, it determines the distance
 * to the nearest target surface for each affected vertex. The vertex is then displaced along the
 * raycasting direction, with the magnitude proportional to the determined distance and the brush's
 * overall influence (strength, falloff, etc.).
 *
 * Settings:
 *  - Projection Direction: The ray direction, which can be set to the view normal or the brush
 * plane normal.
 *  - Bidirectional: When enabled, projects vertices both along the projection direction and
 * its inverse, choosing the closest intersection.
 *  - Relative: Offsets the projection to maintain the average relative positions of the
 * vertices.
 *
 * Inverting the brush inverts the ray direction.
 */

#include <cfloat>

#include "editors/sculpt_paint/mesh/brushes/brushes.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_mesh.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh/mesh_brush_common.hh"
#include "editors/sculpt_paint/mesh/sculpt_automask.hh"
#include "editors/sculpt_paint/mesh/sculpt_intern.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::brushes {

inline namespace scene_project_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float3> ray_origins;
  Vector<float> hit_distances;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

static inline float absolute_min_distance(const float d1, const float d2)
{
  return math::abs(d1) < math::abs(d2) ? d1 : d2;
}

static inline void raycast(const float3 &ray_origin,
                           const float3 &ray_normal,
                           const bke::BVHTreeFromMesh &tree_data,
                           BVHTreeRayHit &hit)
{
  hit.dist = BVH_RAYCAST_DIST_MAX;
  BLI_bvhtree_ray_cast(tree_data.tree,
                       ray_origin,
                       ray_normal,
                       0.0f,
                       &hit,
                       tree_data.raycast_callback,
                       const_cast<bke::BVHTreeFromMesh *>(&tree_data));
}

/**
 * Casts rays from the vertices of the active object to the target object, using the given
 * normal as the ray direction. Updates `best_hit_distances` with the minimum absolute hit distance
 * found against this target object.
 *
 * It should be noted that:
 *
 * 1. The BVH of the target object, used for raycasting, expects the ray to be expressed in the
 *    coordinate system of the target object.
 * 2. The hit distance is best thought as a parametric distance, and does not depend on the
 *    coordinate system used.
 *
 * Explanation for the latter point:

 * Mathematically, if the ray hits the target object, then there exists a point `Q` on
 * the surface of the target object such that:
 *
 * `Q = P + dN`
 *
 * where `P` is the position of the vertex that casts the ray, `N` is
 * the normal, and `d` is a non-negative real number representing the distance.
 *
 * Suppose that M is a transformation matrix. Multiplying both sides by M, we get:
 *
 * `MQ = M(P + dN)`
 *
 * and by linearity:
 *
 * `MQ = MP + dMN`
 *
 * Therefore, `d` also represents the parametric distance in the new coordinate system.
 */
static void object_raycast(const ProjectBrushTarget &project_target,
                           const bool bidirectional,
                           const float3 &normal,
                           const Span<float3> positions,
                           const Span<float> factors,
                           const MutableSpan<float3> ray_origins,
                           const MutableSpan<float> best_hit_distances)
{
  /* Positions and normal are in the coordinate system of the active object. Convert them to the
   * coordinate system of the target. */
  const float3 ray_direction = math::transform_direction(project_target.active_to_target_matrix,
                                                         normal);

  math::transform_points(positions, project_target.active_to_target_matrix, ray_origins, false);

  threading::isolate_task([&]() {
    threading::parallel_for(positions.index_range(), 256, [&](IndexRange range) {
      BVHTreeRayHit hit;

      for (const int i : range) {
        if (factors[i] == 0.0f) {
          continue;
        }

        raycast(ray_origins[i], ray_direction, project_target.tree_data, hit);
        best_hit_distances[i] = absolute_min_distance(best_hit_distances[i], hit.dist);
      }

      if (bidirectional) {
        for (const int i : range) {
          if (factors[i] == 0.0f) {
            continue;
          }

          raycast(ray_origins[i], -ray_direction, project_target.tree_data, hit);
          best_hit_distances[i] = absolute_min_distance(best_hit_distances[i], -hit.dist);
        }
      }
    });
  });
}

/**
 * Casts rays from the active object's positions to find the closest hits with the target objects
 * in the scene, storing distances in `r_hit_distances`.
 */
static void scene_raycast(const Span<ProjectBrushTarget> project_targets,
                          const bool bidirectional,
                          const float minimum_distance,
                          const float3 &normal,
                          const Span<float3> positions,
                          const Span<float> factors,
                          const MutableSpan<float3> ray_origins,
                          const MutableSpan<float> r_hit_distances)
{
  r_hit_distances.fill(BVH_RAYCAST_DIST_MAX);

  for (const int i : project_targets.index_range()) {
    object_raycast(project_targets[i],
                   bidirectional,
                   normal,
                   positions,
                   factors,
                   ray_origins,
                   r_hit_distances);
  }

  /* Set hit distances to zero for vertices with no hits, preventing displacement. */
  for (const int i : r_hit_distances.index_range()) {
    if (math::abs(r_hit_distances[i]) == BVH_RAYCAST_DIST_MAX) {
      r_hit_distances[i] = 0.0f;
    }
    else {
      r_hit_distances[i] = r_hit_distances[i] - minimum_distance;
    }
  }
}

static void calc_translations(const float3 &normal,
                              const Span<float> factors,
                              const Span<float> hit_distances,
                              const MutableSpan<float3> r_translations)
{
  for (const int i : factors.index_range()) {
    r_translations[i] = normal * hit_distances[i] * factors[i];
  }
}

static float3 calc_normal(const Brush &brush, const StrokeCache &cache)
{
  switch (brush.project_ray_direction_type) {
    case BRUSH_PROJECT_RAY_DIRECTION_VIEW_NORMAL:
      return -cache.view_normal_symm;
    case BRUSH_PROJECT_RAY_DIRECTION_PLANE_NORMAL:
      return -cache.sculpt_normal_symm;
    default:
      BLI_assert_unreachable();
      return float3(0.0f);
  }
}

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const bool bidirectional,
                       const float3 &normal,
                       const MeshAttributeData &attribute_data,
                       const Span<float3> vert_normals,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       LocalData &tls,
                       const PositionDeformData &position_data)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  const Span<int> verts = node.verts();

  calc_factors_common_mesh_indexed(depsgraph,
                                   brush,
                                   object,
                                   attribute_data,
                                   position_data.eval,
                                   vert_normals,
                                   node,
                                   tls.factors,
                                   tls.distances);

  tls.positions.resize(verts.size());
  const MutableSpan<float3> positions = tls.positions;
  gather_data_mesh(position_data.eval, verts, positions);

  tls.ray_origins.resize(verts.size());
  tls.hit_distances.resize(verts.size());
  const MutableSpan<float> hit_distances = tls.hit_distances;

  scene_raycast(ss.cache->project_targets,
                bidirectional,
                brush.minimum_distance,
                normal,
                positions,
                tls.factors,
                tls.ray_origins,
                hit_distances);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(normal, tls.factors, hit_distances, translations);
  scale_translations(translations, ss.cache->bstrength);

  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const bool bidirectional,
                       const float3 &normal,
                       const bke::pbvh::GridsNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  calc_factors_common_grids(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  tls.ray_origins.resize(positions.size());
  tls.hit_distances.resize(positions.size());
  const MutableSpan<float> hit_distances = tls.hit_distances;
  scene_raycast(ss.cache->project_targets,
                bidirectional,
                brush.minimum_distance,
                normal,
                positions,
                tls.factors,
                tls.ray_origins,
                hit_distances);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(normal, tls.factors, hit_distances, translations);
  scale_translations(translations, ss.cache->bstrength);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const bool bidirectional,
                       const float3 &normal,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.runtime->sculpt_session;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  calc_factors_common_bmesh(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  tls.ray_origins.resize(positions.size());
  tls.hit_distances.resize(positions.size());
  const MutableSpan<float> hit_distances = tls.hit_distances;
  scene_raycast(ss.cache->project_targets,
                bidirectional,
                brush.minimum_distance,
                normal,
                positions,
                tls.factors,
                tls.ray_origins,
                hit_distances);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(normal, tls.factors, hit_distances, translations);
  scale_translations(translations, ss.cache->bstrength);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace scene_project_cc

void do_scene_project_brush(const Depsgraph &depsgraph,
                            const Sculpt &sd,
                            Object &object,
                            const IndexMask &node_mask)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const StrokeCache &cache = *object.runtime->sculpt_session->cache;

  const bool bidirectional = brush.flag2 & BRUSH_PROJECT_USE_BIDIRECTIONAL;
  const float3 normal = calc_normal(brush, cache);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *id_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

      node_mask.foreach_index(
          [&](const int i) {
            LocalData &tls = all_tls.local();
            calc_faces(depsgraph,
                       sd,
                       brush,
                       bidirectional,
                       normal,
                       attribute_data,
                       vert_normals,
                       nodes[i],
                       object,
                       tls,
                       position_data);
            bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
          },
          exec_mode::grain_size(1));
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(
          [&](const int i) {
            LocalData &tls = all_tls.local();
            calc_grids(depsgraph, sd, object, brush, bidirectional, normal, nodes[i], tls);
            bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
          },
          exec_mode::grain_size(1));
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(
          [&](const int i) {
            LocalData &tls = all_tls.local();
            calc_bmesh(depsgraph, sd, object, brush, bidirectional, normal, nodes[i], tls);
            bke::pbvh::update_node_bounds_bmesh(nodes[i]);
          },
          exec_mode::grain_size(1));
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint::brushes
