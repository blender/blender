/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_colortools.hh"
#include "BKE_key.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace smooth_cc {

static Vector<float> iteration_strengths(const float strength)
{
  constexpr int max_iterations = 4;

  BLI_assert_msg(strength >= 0.0f,
                 "The smooth brush expects a non-negative strength to behave properly");
  const float clamped_strength = std::min(strength, 1.0f);

  const int count = int(clamped_strength * max_iterations);
  const float last = max_iterations * (clamped_strength - float(count) / max_iterations);
  Vector<float> result;
  result.append_n_times(1.0f, count);
  result.append(last);
  return result;
}

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<Vector<int>> vert_neighbors;
  Vector<float3> new_positions;
  Vector<float3> translations;
};

BLI_NOINLINE static void apply_positions_faces(const Sculpt &sd,
                                               const Brush &brush,
                                               const Span<float3> positions_eval,
                                               const Span<float3> vert_normals,
                                               const PBVHNode &node,
                                               const float strength,
                                               Object &object,
                                               LocalData &tls,
                                               const Span<float3> new_positions,
                                               MutableSpan<float3> positions_orig)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);

  tls.factors.reinitialize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  filter_region_clip_factors(ss, positions_eval, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, vert_normals, verts, factors);
  }

  tls.distances.reinitialize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, positions_eval, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

  tls.translations.reinitialize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_new_positions(new_positions, verts, positions_eval, translations);
  scale_translations(translations, factors);

  write_translations(sd, object, positions_eval, verts, translations, positions_orig);
}

BLI_NOINLINE static void do_smooth_brush_mesh(const Sculpt &sd,
                                              const Brush &brush,
                                              Object &object,
                                              Span<PBVHNode *> nodes,
                                              const float brush_strength)
{
  const SculptSession &ss = *object.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const PBVH &pbvh = *ss.pbvh;

  const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
  const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);
  MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(nodes, node_offset_data);
  Array<float3> new_positions(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<LocalData> all_tls;

  /* Calculate the new positions into a separate array in a separate loop because multiple loops
   * are updated in parallel. Without this there would be non-threadsafe access to changing
   * positions in other PBVH nodes. */
  for (const float strength : iteration_strengths(brush_strength)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        const Span<int> verts = bke::pbvh::node_unique_verts(*nodes[i]);
        tls.vert_neighbors.reinitialize(verts.size());
        calc_vert_neighbors_interior(faces,
                                     corner_verts,
                                     ss.vert_to_face_map,
                                     ss.vertex_info.boundary,
                                     hide_poly,
                                     verts,
                                     tls.vert_neighbors);
        smooth::neighbor_position_average_mesh(
            positions_eval,
            verts,
            tls.vert_neighbors,
            new_positions.as_mutable_span().slice(node_vert_offsets[i]));
      }
    });

    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        apply_positions_faces(sd,
                              brush,
                              positions_eval,
                              vert_normals,
                              *nodes[i],
                              strength,
                              object,
                              tls,
                              new_positions.as_span().slice(node_vert_offsets[i]),
                              positions_orig);
      }
    });
  }
}

static void calc_grids(const Sculpt &sd,
                       const OffsetIndices<int> faces,
                       const Span<int> corner_verts,
                       const BitSpan boundary_verts,
                       Object &object,
                       const Brush &brush,
                       const float strength,
                       const PBVHNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.factors.reinitialize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, subdiv_ccg, grids, factors);
  }

  tls.distances.reinitialize(positions.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_grids_factors(object, *cache.automasking, node, grids, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.new_positions.reinitialize(positions.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  smooth::neighbor_position_average_interior_grids(
      faces, corner_verts, boundary_verts, subdiv_ccg, grids, new_positions);

  tls.translations.reinitialize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_new_positions(new_positions, positions, translations);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float strength,
                       PBVHNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  tls.factors.reinitialize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, verts, factors);
  }

  tls.distances.reinitialize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.new_positions.reinitialize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  smooth::neighbor_position_average_interior_bmesh(verts, new_positions);

  tls.translations.reinitialize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_new_positions(new_positions, positions, translations);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace smooth_cc

void do_smooth_brush(const Sculpt &sd,
                     Object &object,
                     const Span<PBVHNode *> nodes,
                     const float brush_strength)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  boundary::ensure_boundary_info(object);

  switch (BKE_pbvh_type(*object.sculpt->pbvh)) {
    case PBVH_FACES:
      do_smooth_brush_mesh(sd, brush, object, nodes, brush_strength);
      break;
    case PBVH_GRIDS: {
      const Mesh &base_mesh = *static_cast<const Mesh *>(object.data);
      const OffsetIndices faces = base_mesh.faces();
      const Span<int> corner_verts = base_mesh.corner_verts();

      threading::EnumerableThreadSpecific<LocalData> all_tls;
      for (const float strength : iteration_strengths(brush_strength)) {
        threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
          LocalData &tls = all_tls.local();
          for (const int i : range) {
            calc_grids(sd,
                       faces,
                       corner_verts,
                       ss.vertex_info.boundary,
                       object,
                       brush,
                       strength,
                       *nodes[i],
                       tls);
          }
        });
      }
      break;
    }
    case PBVH_BMESH: {
      BM_mesh_elem_index_ensure(ss.bm, BM_VERT);
      BM_mesh_elem_table_ensure(ss.bm, BM_VERT);
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      for (const float strength : iteration_strengths(brush_strength)) {
        threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
          LocalData &tls = all_tls.local();
          for (const int i : range) {
            calc_bmesh(sd, object, brush, strength, *nodes[i], tls);
          }
        });
      }
      break;
    }
  }
}

}  // namespace blender::ed::sculpt_paint
