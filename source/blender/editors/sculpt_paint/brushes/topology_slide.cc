/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

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

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace topology_slide_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<Vector<int>> vert_neighbors;
  Vector<float3> translations;
};

BLI_NOINLINE static void calc_translation_directions(const Brush &brush,
                                                     const StrokeCache &cache,
                                                     const Span<float3> positions,
                                                     const MutableSpan<float3> r_translations)
{

  switch (brush.slide_deform_type) {
    case BRUSH_SLIDE_DEFORM_DRAG:
      r_translations.fill(math::normalize(cache.location - cache.last_location));
      break;
    case BRUSH_SLIDE_DEFORM_PINCH:
      for (const int i : positions.index_range()) {
        r_translations[i] = math::normalize(cache.location - positions[i]);
      }
      break;
    case BRUSH_SLIDE_DEFORM_EXPAND:
      for (const int i : positions.index_range()) {
        r_translations[i] = math::normalize(positions[i] - cache.location);
      }
      break;
  }
}

static inline void add_neighbor_influence(const float3 &position,
                                          const float3 &dir,
                                          const float3 &neighbor_position,
                                          float3 &translation)
{
  const float3 neighbor_disp = neighbor_position - position;
  const float3 neighbor_dir = math::normalize(neighbor_disp);
  if (math::dot(dir, neighbor_dir) > 0.0f) {
    translation += neighbor_dir * math::dot(dir, neighbor_disp);
  }
}

BLI_NOINLINE static void calc_neighbor_influence(const Span<float3> vert_positions,
                                                 const Span<float3> positions,
                                                 const Span<Vector<int>> vert_neighbors,
                                                 const MutableSpan<float3> translations)
{
  for (const int i : positions.index_range()) {
    const float3 &position = positions[i];
    const float3 &dir = translations[i];

    float3 final_translation(0);
    for (const int neighbor : vert_neighbors[i]) {
      add_neighbor_influence(position, dir, vert_positions[neighbor], final_translation);
    }

    translations[i] = final_translation;
  }
}

BLI_NOINLINE static void calc_neighbor_influence(const SubdivCCG &subdiv_ccg,
                                                 const Span<float3> positions,
                                                 const Span<int> grids,
                                                 const MutableSpan<float3> translations)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<CCGElem *> elems = subdiv_ccg.grids;
  for (const int i : grids.index_range()) {
    const int node_start = i * key.grid_area;
    const int grid = grids[i];
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert_index = node_start + offset;

        const float3 &position = positions[node_vert_index];
        const float3 &dir = translations[node_vert_index];

        SubdivCCGCoord coord{};
        coord.grid_index = grid;
        coord.x = x;
        coord.y = y;

        SubdivCCGNeighbors neighbors;
        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);

        float3 final_translation(0);
        for (const SubdivCCGCoord neighbor : neighbors.coords) {
          add_neighbor_influence(
              position,
              dir,
              CCG_grid_elem_co(key, elems[neighbor.grid_index], neighbor.x, neighbor.y),
              final_translation);
        }

        translations[node_vert_index] = final_translation;
      }
    }
  }
}

BLI_NOINLINE static void calc_neighbor_influence(const Span<float3> positions,
                                                 const Set<BMVert *, 0> &verts,
                                                 const MutableSpan<float3> translations)
{
  Vector<BMVert *, 64> neighbors;
  int i = 0;
  for (BMVert *vert : verts) {
    const float3 &position = positions[i];
    const float3 &dir = translations[i];

    float3 final_translation(0);
    neighbors.clear();
    for (BMVert *neighbor : vert_neighbors_get_bmesh(*vert, neighbors)) {
      add_neighbor_influence(position, dir, neighbor->co, final_translation);
    }

    translations[i] = final_translation;
    i++;
  }
}

static void calc_faces(const Sculpt &sd,
                       const Brush &brush,
                       const Span<float3> positions_eval,
                       const OffsetIndices<int> faces,
                       const Span<int> corner_verts,
                       const GroupedSpan<int> vert_to_face_map,
                       const Span<bool> hide_poly,
                       const bke::pbvh::Node &node,
                       Object &object,
                       LocalData &tls,
                       const MutableSpan<float3> positions_orig)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);
  const Span<int> verts = bke::pbvh::node_unique_verts(node);
  const MutableSpan positions = gather_data_mesh(positions_eval, verts, tls.positions);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  filter_region_clip_factors(ss, orig_data.positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, orig_data.normals, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, orig_data.positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  calc_brush_texture_factors(ss, brush, orig_data.positions, factors);

  scale_factors(factors, cache.bstrength);

  tls.vert_neighbors.resize(verts.size());
  calc_vert_neighbors(faces, corner_verts, vert_to_face_map, hide_poly, verts, tls.vert_neighbors);
  const Span<Vector<int>> vert_neighbors = tls.vert_neighbors;

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translation_directions(brush, cache, positions, translations);
  calc_neighbor_influence(positions_eval, positions, vert_neighbors, translations);
  scale_translations(translations, factors);

  write_translations(sd, object, positions_eval, verts, translations, positions_orig);
}

static void calc_grids(const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const bke::pbvh::Node &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);
  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, orig_data.positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, orig_data.normals, grids, factors);
  }

  tls.distances.resize(positions.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, orig_data.positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_grids_factors(object, *cache.automasking, node, grids, factors);
  }

  calc_brush_texture_factors(ss, brush, orig_data.positions, factors);

  scale_factors(factors, cache.bstrength);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translation_directions(brush, cache, positions, translations);
  calc_neighbor_influence(subdiv_ccg, positions, grids, translations);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, orig_data.positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(
    const Sculpt &sd, Object &object, const Brush &brush, bke::pbvh::Node &node, LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, orig_positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, orig_normals, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, orig_positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  calc_brush_texture_factors(ss, brush, orig_positions, factors);

  scale_factors(factors, cache.bstrength);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translation_directions(brush, cache, positions, translations);
  calc_neighbor_influence(positions, verts, translations);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, orig_positions, translations);
  apply_translations(translations, verts);
}

}  // namespace topology_slide_cc

void do_topology_slide_brush(const Sculpt &sd, Object &object, Span<bke::pbvh::Node *> nodes)
{
  const SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    return;
  }

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (object.sculpt->pbvh->type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const bke::pbvh::Tree &pbvh = *ss.pbvh;
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
      MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_faces(sd,
                     brush,
                     positions_eval,
                     faces,
                     corner_verts,
                     ss.vert_to_face_map,
                     hide_poly,
                     *nodes[i],
                     object,
                     tls,
                     positions_orig);
          BKE_pbvh_node_mark_positions_update(nodes[i]);
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_grids(sd, object, brush, *nodes[i], tls);
        }
      });
      break;
    case bke::pbvh::Type::BMesh:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_bmesh(sd, object, brush, *nodes[i], tls);
        }
      });
      break;
  }
}

}  // namespace blender::ed::sculpt_paint
