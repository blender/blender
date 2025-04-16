/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/brushes.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::brushes {

inline namespace topology_slide_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<int> neighbor_offsets;
  Vector<int> neighbor_data;
  Vector<float3> translations;
};

BLI_NOINLINE static void calc_translation_directions(const Brush &brush,
                                                     const StrokeCache &cache,
                                                     const Span<float3> positions,
                                                     const MutableSpan<float3> r_translations)
{

  switch (brush.slide_deform_type) {
    case BRUSH_SLIDE_DEFORM_DRAG:
      r_translations.fill(math::normalize(cache.location_symm - cache.last_location_symm));
      break;
    case BRUSH_SLIDE_DEFORM_PINCH:
      for (const int i : positions.index_range()) {
        r_translations[i] = math::normalize(cache.location_symm - positions[i]);
      }
      break;
    case BRUSH_SLIDE_DEFORM_EXPAND:
      for (const int i : positions.index_range()) {
        r_translations[i] = math::normalize(positions[i] - cache.location_symm);
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
                                                 const GroupedSpan<int> vert_neighbors,
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
                                                 const Span<int> grids,
                                                 const MutableSpan<float3> translations)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float3> positions = subdiv_ccg.positions;
  for (const int i : grids.index_range()) {
    const int node_start = i * key.grid_area;
    const int grid = grids[i];
    const int start = grid * key.grid_area;
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int vert = start + offset;
        const int node_vert = node_start + offset;

        const float3 &position = positions[vert];
        const float3 &dir = translations[node_vert];

        SubdivCCGCoord coord{};
        coord.grid_index = grid;
        coord.x = x;
        coord.y = y;

        SubdivCCGNeighbors neighbors;
        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);

        float3 final_translation(0);
        for (const SubdivCCGCoord neighbor : neighbors.coords) {
          add_neighbor_influence(
              position, dir, positions[neighbor.to_index(key)], final_translation);
        }

        translations[node_vert] = final_translation;
      }
    }
  }
}

BLI_NOINLINE static void calc_neighbor_influence(const Span<float3> positions,
                                                 const Set<BMVert *, 0> &verts,
                                                 const MutableSpan<float3> translations)
{
  BMeshNeighborVerts neighbors;
  int i = 0;
  for (BMVert *vert : verts) {
    const float3 &position = positions[i];
    const float3 &dir = translations[i];

    float3 final_translation(0);
    for (BMVert *neighbor : vert_neighbors_get_bmesh(*vert, neighbors)) {
      add_neighbor_influence(position, dir, neighbor->co, final_translation);
    }

    translations[i] = final_translation;
    i++;
  }
}

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const OffsetIndices<int> faces,
                       const Span<int> corner_verts,
                       const GroupedSpan<int> vert_to_face_map,
                       const MeshAttributeData &attribute_data,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       LocalData &tls,
                       const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);
  const Span<int> verts = node.verts();
  const MutableSpan positions = gather_data_mesh(position_data.eval, verts, tls.positions);

  calc_factors_common_from_orig_data_mesh(depsgraph,
                                          brush,
                                          object,
                                          attribute_data,
                                          orig_data.positions,
                                          orig_data.normals,
                                          node,
                                          tls.factors,
                                          tls.distances);

  scale_factors(tls.factors, cache.bstrength);

  const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                         corner_verts,
                                                         vert_to_face_map,
                                                         attribute_data.hide_poly,
                                                         verts,
                                                         tls.neighbor_offsets,
                                                         tls.neighbor_data);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translation_directions(brush, cache, positions, translations);
  calc_neighbor_influence(position_data.eval, positions, neighbors, translations);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const bke::pbvh::GridsNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);
  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  calc_factors_common_from_orig_data_grids(depsgraph,
                                           brush,
                                           object,
                                           orig_data.positions,
                                           orig_data.normals,
                                           node,
                                           tls.factors,
                                           tls.distances);

  scale_factors(tls.factors, cache.bstrength);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translation_directions(brush, cache, positions, translations);
  calc_neighbor_influence(subdiv_ccg, grids, translations);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, orig_data.positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  calc_factors_common_from_orig_data_bmesh(
      depsgraph, brush, object, orig_positions, orig_normals, node, tls.factors, tls.distances);

  scale_factors(tls.factors, cache.bstrength);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translation_directions(brush, cache, positions, translations);
  calc_neighbor_influence(positions, verts, translations);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, orig_positions, translations);
  apply_translations(translations, verts);
}

}  // namespace topology_slide_cc

void do_topology_slide_brush(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const IndexMask &node_mask)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    return;
  }

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const PositionDeformData position_data(depsgraph, object);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const MeshAttributeData attribute_data(mesh);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_faces(depsgraph,
                   sd,
                   brush,
                   faces,
                   corner_verts,
                   vert_to_face_map,
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
        calc_grids(depsgraph, sd, object, brush, nodes[i], tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_bmesh(depsgraph, sd, object, brush, nodes[i], tls);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint::brushes
