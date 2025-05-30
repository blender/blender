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
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"
#include "editors/sculpt_paint/sculpt_smooth.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::brushes {
inline namespace enhance_details_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float3> new_positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<int> neighbor_offsets;
  Vector<int> neighbor_data;
  Vector<float3> translations;
};

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const MeshAttributeData &attribute_data,
                       const Span<float3> vert_normals,
                       const Span<float3> all_translations,
                       const float strength,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       LocalData &tls,
                       const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;

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

  scale_factors(tls.factors, strength);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  gather_data_mesh(all_translations, verts, translations);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const Span<float3> all_translations,
                       const float strength,
                       const bke::pbvh::GridsNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  calc_factors_common_grids(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  scale_factors(tls.factors, strength);

  const MutableSpan<float3> translations = gather_data_grids(
      subdiv_ccg, all_translations, grids, tls.translations);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const Span<float3> all_translations,
                       const float strength,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  calc_factors_common_bmesh(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  scale_factors(tls.factors, strength);

  const MutableSpan translations = gather_data_bmesh(all_translations, verts, tls.translations);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

static void calc_translations_faces(const Span<float3> vert_positions,
                                    const OffsetIndices<int> faces,
                                    const Span<int> corner_verts,
                                    const GroupedSpan<int> vert_to_face_map,
                                    const Span<bool> hide_poly,
                                    const bke::pbvh::MeshNode &node,
                                    LocalData &tls,
                                    const MutableSpan<float3> all_translations)
{
  const Span<int> verts = node.verts();

  const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                         corner_verts,
                                                         vert_to_face_map,
                                                         hide_poly,
                                                         verts,
                                                         tls.neighbor_offsets,
                                                         tls.neighbor_data);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  smooth::neighbor_data_average_mesh_check_loose(vert_positions, verts, neighbors, new_positions);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_new_positions(new_positions, verts, vert_positions, translations);
  scatter_data_mesh(translations.as_span(), verts, all_translations);
}

static void calc_translations_grids(const SubdivCCG &subdiv_ccg,
                                    const bke::pbvh::GridsNode &node,
                                    LocalData &tls,
                                    const MutableSpan<float3> all_translations)
{
  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.new_positions.resize(positions.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  smooth::average_data_grids(subdiv_ccg, subdiv_ccg.positions.as_span(), grids, new_positions);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_new_positions(new_positions, positions, translations);
  scatter_data_grids(subdiv_ccg, translations.as_span(), grids, all_translations);
}

static void calc_translations_bmesh(const bke::pbvh::BMeshNode &node,
                                    LocalData &tls,
                                    const MutableSpan<float3> all_translations)
{
  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(
      const_cast<bke::pbvh::BMeshNode *>(&node));
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  smooth::neighbor_position_average_bmesh(verts, new_positions);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_new_positions(new_positions, positions, translations);
  scatter_data_bmesh(translations.as_span(), verts, all_translations);
}

}  // namespace enhance_details_cc

void do_enhance_details_brush(const Depsgraph &depsgraph,
                              const Sculpt &sd,
                              Object &object,
                              const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  if (ss.cache->detail_directions.is_empty()) {
    ss.cache->detail_directions.reinitialize(SCULPT_vertex_count_get(object));
    IndexMaskMemory memory;
    const IndexMask effective_nodes = bke::pbvh::search_nodes(
        pbvh, memory, [&](const bke::pbvh::Node &node) {
          return !node_fully_masked_or_hidden(node);
        });
    calc_smooth_translations(depsgraph, object, effective_nodes, ss.cache->detail_directions);
  }

  const float strength = std::clamp(ss.cache->bstrength, -1.0f, 1.0f);
  MutableSpan<float3> translations = ss.cache->detail_directions;

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
                   attribute_data,
                   vert_normals,
                   translations,
                   strength,
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
        calc_grids(depsgraph, sd, object, brush, translations, strength, nodes[i], tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_bmesh(depsgraph, sd, object, brush, translations, strength, nodes[i], tls);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint::brushes
namespace blender::ed::sculpt_paint {
void calc_smooth_translations(const Depsgraph &depsgraph,
                              const Object &object,
                              const IndexMask &node_mask,
                              const MutableSpan<float3> translations)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  threading::EnumerableThreadSpecific<brushes::LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        brushes::LocalData &tls = all_tls.local();
        calc_translations_faces(positions_eval,
                                faces,
                                corner_verts,
                                vert_to_face_map,
                                attribute_data.hide_poly,
                                nodes[i],
                                tls,
                                translations);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        brushes::LocalData &tls = all_tls.local();
        calc_translations_grids(subdiv_ccg, nodes[i], tls, translations);
      });
      break;
    }
    case bke::pbvh::Type::BMesh:
      vert_random_access_ensure(const_cast<Object &>(object));
      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        brushes::LocalData &tls = all_tls.local();
        calc_translations_bmesh(nodes[i], tls, translations);
      });
      break;
  }
}

}  // namespace blender::ed::sculpt_paint
