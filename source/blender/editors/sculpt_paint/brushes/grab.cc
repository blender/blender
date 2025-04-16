/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <queue>

#include "editors/sculpt_paint/brushes/brushes.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
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
inline namespace grab_cc {

struct LocalData {
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

BLI_NOINLINE static void calc_silhouette_factors(const StrokeCache &cache,
                                                 const float3 &offset,
                                                 const Span<float3> normals,
                                                 const MutableSpan<float> factors)
{
  BLI_assert(normals.size() == factors.size());

  const float sign = math::sign(math::dot(cache.initial_normal_symm, cache.grab_delta_symm));
  const float3 test_dir = math::normalize(offset) * sign;
  for (const int i : factors.index_range()) {
    factors[i] *= std::max(math::dot(test_dir, normals[i]), 0.0f);
  }
}

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const float3 &offset,
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

  calc_factors_common_from_orig_data_mesh(depsgraph,
                                          brush,
                                          object,
                                          attribute_data,
                                          orig_data.positions,
                                          orig_data.normals,
                                          node,
                                          tls.factors,
                                          tls.distances);

  if (brush.flag2 & BRUSH_GRAB_SILHOUETTE) {
    calc_silhouette_factors(cache, offset, orig_data.normals, tls.factors);
  }

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_offset_and_factors(offset, tls.factors, translations);

  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float3 &offset,
                       bke::pbvh::GridsNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);
  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;

  calc_factors_common_from_orig_data_grids(depsgraph,
                                           brush,
                                           object,
                                           orig_data.positions,
                                           orig_data.normals,
                                           node,
                                           tls.factors,
                                           tls.distances);

  if (brush.flag2 & BRUSH_GRAB_SILHOUETTE) {
    calc_silhouette_factors(cache, offset, orig_data.normals, tls.factors);
  }

  tls.translations.resize(grid_verts_num);
  const MutableSpan<float3> translations = tls.translations;
  translations_from_offset_and_factors(offset, tls.factors, translations);

  clip_and_lock_translations(sd, ss, orig_data.positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float3 &offset,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  calc_factors_common_from_orig_data_bmesh(
      depsgraph, brush, object, orig_positions, orig_normals, node, tls.factors, tls.distances);

  if (brush.flag2 & BRUSH_GRAB_SILHOUETTE) {
    calc_silhouette_factors(cache, offset, orig_normals, tls.factors);
  }

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_offset_and_factors(offset, tls.factors, translations);

  clip_and_lock_translations(sd, ss, orig_positions, translations);
  apply_translations(translations, verts);
}

}  // namespace grab_cc

void do_grab_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &object,
                   const IndexMask &node_mask)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  float3 grab_delta = ss.cache->grab_delta_symm;

  if (ss.cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss.cache->normal_weight, grab_delta);
  }

  grab_delta *= ss.cache->bstrength;

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_faces(depsgraph,
                   sd,
                   brush,
                   grab_delta,
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
        calc_grids(depsgraph, sd, object, brush, grab_delta, nodes[i], tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_bmesh(depsgraph, sd, object, brush, grab_delta, nodes[i], tls);
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

void geometry_preview_lines_update(Depsgraph &depsgraph,
                                   Object &object,
                                   SculptSession &ss,
                                   float radius)
{
  ss.preview_verts = {};

  /* This function is called from the cursor drawing code, so the tree may not be built yet. */
  const bke::pbvh::Tree *pbvh = bke::object::pbvh_get(object);
  if (!pbvh) {
    return;
  }

  if (!ss.deform_modifiers_active) {
    return;
  }

  if (pbvh->type() != bke::pbvh::Type::Mesh) {
    return;
  }

  BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);

  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  /* Always grab active shape key if the sculpt happens on shapekey. */
  const Span<float3> positions = ss.shapekey_active ?
                                     bke::pbvh::vert_positions_eval(depsgraph, object) :
                                     mesh.vert_positions();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const int active_vert = std::get<int>(ss.active_vert());
  const float3 brush_co = positions[active_vert];
  const float radius_sq = radius * radius;

  Vector<int> preview_verts;
  Vector<int> neighbors;
  BitVector<> visited_verts(positions.size());
  std::queue<int> queue;
  queue.push(active_vert);
  while (!queue.empty()) {
    const int from_vert = queue.front();
    queue.pop();

    neighbors.clear();
    for (const int neighbor : vert_neighbors_get_mesh(
             faces, corner_verts, vert_to_face_map, hide_poly, from_vert, neighbors))
    {
      preview_verts.append(from_vert);
      preview_verts.append(neighbor);
      if (visited_verts[neighbor]) {
        continue;
      }
      visited_verts[neighbor].set();
      if (math::distance_squared(brush_co, positions[neighbor]) < radius_sq) {
        queue.push(neighbor);
      }
    }
  }

  ss.preview_verts = preview_verts.as_span();
}

}  // namespace blender::ed::sculpt_paint
