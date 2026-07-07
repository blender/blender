/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/mesh/brushes/brushes.hh"
#include "editors/sculpt_paint/mesh/mesh_brush_common.hh"
#include "editors/sculpt_paint/mesh/sculpt_automask.hh"

#include "DNA_brush_types.h"

#include "BKE_mesh.hh"
#include "BKE_object_types.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_base.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh/sculpt_face_set.hh"
#include "editors/sculpt_paint/mesh/sculpt_intern.hh"
#include "editors/sculpt_paint/mesh/sculpt_undo.hh"

#include "ED_sculpt.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::brushes {
inline namespace draw_face_sets_cc {

struct MeshLocalData {
  Vector<float3> positions;
  Vector<float3> normals;
  Vector<float> factors;
  Vector<float> distances;
};

static void calc_face_normals(const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              const Span<float3> vert_positions,
                              const Span<int> face_indices,
                              const MutableSpan<float3> normals)
{
  BLI_assert(face_indices.size() == normals.size());

  for (const int i : face_indices.index_range()) {
    normals[i] = bke::mesh::face_normal_calc(vert_positions,
                                             corner_verts.slice(faces[face_indices[i]]));
  }
}

BLI_NOINLINE static void apply_face_set(const int face_set_id,
                                        const Span<int> face_indices,
                                        const Span<float> factors,
                                        const MutableSpan<int> face_sets)
{
  BLI_assert(face_indices.size() == factors.size());

  for (const int i : face_indices.index_range()) {
    if (factors[i] > face_set::FACE_SET_MIN_FADE) {
      face_sets[face_indices[i]] = face_set_id;
    }
  }
}

static void calc_faces(const Depsgraph &depsgraph,
                       Object &object,
                       const Brush &brush,
                       const float strength,
                       const int face_set_id,
                       Span<float3> positions_eval,
                       const bke::pbvh::MeshNode &node,
                       const Span<int> face_indices,
                       MeshLocalData &tls,
                       const MutableSpan<int> face_sets)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  const StrokeCache &cache = *ss.cache;
  Mesh &mesh = *id_cast<Mesh *>(object.data);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  tls.positions.resize(face_indices.size());
  const MutableSpan<float3> face_centers = tls.positions;
  face_set::calc_face_centers(faces, corner_verts, positions_eval, face_indices, face_centers);

  tls.normals.resize(face_indices.size());
  const MutableSpan<float3> face_normals = tls.normals;
  calc_face_normals(faces, corner_verts, positions_eval, face_indices, face_normals);

  tls.factors.resize(face_indices.size());
  const MutableSpan<float> factors = tls.factors;

  face_set::fill_factor_from_hide_and_mask(mesh, face_indices, factors);

  filter_region_clip_factors(ss, face_centers, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, face_normals, factors);
  }

  tls.distances.resize(face_indices.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, face_centers, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    const OffsetIndices<int> faces = mesh.faces();
    const Span<int> corner_verts = mesh.corner_verts();
    auto_mask::calc_face_factors(
        depsgraph, object, faces, corner_verts, *cache.automasking, node, face_indices, factors);
  }

  calc_brush_texture_factors(ss, brush, face_centers, factors);
  scale_factors(factors, strength);

  apply_face_set(face_set_id, face_indices, factors, face_sets);
}

static void do_draw_face_sets_brush_mesh(const Depsgraph &depsgraph,
                                         Object &object,
                                         const Brush &brush,
                                         const IndexMask &node_mask)
{
  const SculptSession &ss = *object.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);

  undo::push_nodes(depsgraph, object, node_mask, undo::Type::FaceSet);

  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(
      *id_cast<Mesh *>(object.data));

  threading::EnumerableThreadSpecific<MeshLocalData> all_tls;
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    MeshLocalData &tls = all_tls.local();
    const Span<int> face_indices = nodes[i].faces();
    calc_faces(depsgraph,
               object,
               brush,
               ss.cache->bstrength,
               ss.cache->paint_face_set,
               positions_eval,
               nodes[i],
               face_indices,
               tls,
               face_sets.span);
  });
  pbvh.tag_face_sets_changed(node_mask);
  face_sets.finish();
}

struct GridLocalData {
  Vector<int> face_indices;
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
};

static void calc_grids(const Depsgraph &depsgraph,
                       Object &object,
                       const Brush &brush,
                       const float strength,
                       const int face_set_id,
                       const bke::pbvh::GridsNode &node,
                       GridLocalData &tls,
                       const MutableSpan<int> face_sets)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  ed::sculpt_paint::fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, subdiv_ccg, grids, factors);
  }

  tls.distances.resize(positions.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);
  scale_factors(factors, strength);

  tls.face_indices.resize(positions.size());
  MutableSpan<int> face_indices = tls.face_indices;

  face_set::calc_face_indices_grids(subdiv_ccg, grids, face_indices);
  apply_face_set(face_set_id, face_indices, factors, face_sets);
}

static void do_draw_face_sets_brush_grids(const Depsgraph &depsgraph,
                                          Object &object,
                                          const Brush &brush,
                                          const IndexMask &node_mask)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  undo::push_nodes(depsgraph, object, node_mask, undo::Type::FaceSet);

  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(
      *id_cast<Mesh *>(object.data));

  threading::EnumerableThreadSpecific<GridLocalData> all_tls;
  MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    GridLocalData &tls = all_tls.local();
    calc_grids(depsgraph,
               object,
               brush,
               ss.cache->bstrength,
               ss.cache->paint_face_set,
               nodes[i],
               tls,
               face_sets.span);
  });
  pbvh.tag_face_sets_changed(node_mask);
  face_sets.finish();
}
struct BMeshLocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
};

BLI_NOINLINE static void apply_face_set(const int face_set_id,
                                        const Set<BMFace *, 0> &faces,
                                        const MutableSpan<float> factors,
                                        const int cd_offset)
{
  int i = 0;
  for (BMFace *face : faces) {
    if (factors[i] > face_set::FACE_SET_MIN_FADE) {
      BM_ELEM_CD_SET_INT(face, cd_offset, face_set_id);
    }
    i++;
  }
}

static void calc_bmesh(Object &object,
                       const Brush &brush,
                       const float strength,
                       const int face_set_id,
                       bke::pbvh::BMeshNode &node,
                       BMeshLocalData &tls,
                       const int cd_offset)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  const StrokeCache &cache = *ss.cache;

  const Set<BMFace *, 0> &faces = BKE_pbvh_bmesh_node_faces(&node);
  tls.positions.resize(faces.size());
  const MutableSpan<float3> positions = tls.positions;
  face_set::calc_face_centers(faces, positions);

  tls.factors.resize(faces.size());
  const MutableSpan<float> factors = tls.factors;
  face_set::fill_factor_from_hide_and_mask(*ss.bm, faces, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, faces, factors);
  }

  tls.distances.resize(faces.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);
  scale_factors(factors, strength);

  apply_face_set(face_set_id, faces, factors, cd_offset);
}

static void do_draw_face_sets_brush_bmesh(const Depsgraph &depsgraph,
                                          Object &object,
                                          const Brush &brush,
                                          const IndexMask &node_mask)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  undo::push_nodes(depsgraph, object, node_mask, undo::Type::FaceSet);

  const int cd_offset = face_set::ensure_face_sets_bmesh(object);

  threading::EnumerableThreadSpecific<BMeshLocalData> all_tls;
  MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    BMeshLocalData &tls = all_tls.local();
    calc_bmesh(
        object, brush, ss.cache->bstrength, ss.cache->paint_face_set, nodes[i], tls, cd_offset);
  });
  pbvh.tag_face_sets_changed(node_mask);
}

}  // namespace draw_face_sets_cc

void do_draw_face_sets_brush(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const IndexMask &node_mask)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (object.runtime->sculpt_session->cache->paint_face_set == face_set_none_id) {
    if (object.runtime->sculpt_session->cache->invert) {
      /* When inverting the brush, pick the paint face mask ID from the mesh. */
      object.runtime->sculpt_session->cache->paint_face_set = face_set::active_face_set_get(
          object);
    }
    else {
      /* By default, create a new Face Sets. */
      object.runtime->sculpt_session->cache->paint_face_set = face_set::find_next_available_id(
          object);
    }
  }

  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      do_draw_face_sets_brush_mesh(depsgraph, object, brush, node_mask);
      break;
    case bke::pbvh::Type::Grids:
      do_draw_face_sets_brush_grids(depsgraph, object, brush, node_mask);
      break;
    case bke::pbvh::Type::BMesh:
      do_draw_face_sets_brush_bmesh(depsgraph, object, brush, node_mask);
      break;
  }
}
}  // namespace blender::ed::sculpt_paint::brushes
