/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/brushes.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_boundary.hh"
#include "editors/sculpt_paint/sculpt_face_set.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"
#include "editors/sculpt_paint/sculpt_smooth.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::brushes {

inline namespace relax_cc {

/* -------------------------------------------------------------------- */
/** \name Relax Vertex
 * \{ */

struct MeshLocalData {
  Vector<float> factors;
  Vector<float> distances;
};

struct GridLocalData {
  Vector<float> factors;
  Vector<float> distances;
};

struct BMeshLocalData {
  Vector<float> factors;
  Vector<float> distances;
};

static void apply_positions_faces(const Sculpt &sd,
                                  const Span<int> verts,
                                  Object &object,
                                  const MutableSpan<float3> translations,
                                  const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void apply_positions_grids(const Sculpt &sd,
                                  const Span<int> grids,
                                  Object &object,
                                  const Span<float3> positions,
                                  const MutableSpan<float3> translations)
{
  SculptSession &ss = *object.sculpt;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void apply_positions_bmesh(const Sculpt &sd,
                                  const Set<BMVert *, 0> verts,
                                  Object &object,
                                  const MutableSpan<float3> translations,
                                  const Span<float3> positions)

{
  SculptSession &ss = *object.sculpt;

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Relax Face Set
 * \{ */

static std::array<float, 4> iteration_strengths(const float strength, const int stroke_iteration)
{
  if (stroke_iteration % 3 == 0) {
    return {strength, strength, strength, strength};
  }

  /* This operation needs a strength tweak as the relax deformation is too weak by default.
   * We cap the strength at 1.0 to avoid ripping the mesh in cases where this modified value is
   * too strong. */
  const float modified_strength = std::min(strength * 1.5f, 1.0f);
  return {modified_strength, modified_strength, strength, strength};
}

BLI_NOINLINE static void calc_factors_faces(const Depsgraph &depsgraph,
                                            const Brush &brush,
                                            const Span<float3> positions_eval,
                                            const Span<float3> vert_normals,
                                            const GroupedSpan<int> vert_to_face_map,
                                            const MeshAttributeData &attribute_data,
                                            const float strength,
                                            const bool relax_face_sets,
                                            const Object &object,
                                            const bke::pbvh::MeshNode &node,
                                            MeshLocalData &tls,
                                            const MutableSpan<float> factors)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();

  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, positions_eval, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, positions_eval, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

  face_set::filter_verts_with_unique_face_sets_mesh(
      vert_to_face_map, attribute_data.face_sets, relax_face_sets, verts, factors);
}

static void do_relax_face_sets_brush_mesh(const Depsgraph &depsgraph,
                                          const Sculpt &sd,
                                          const Brush &brush,
                                          Object &object,
                                          const IndexMask &node_mask,
                                          const float strength,
                                          const bool relax_face_sets)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const MeshAttributeData attribute_data(mesh);

  const PositionDeformData position_data(depsgraph, object);
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      nodes, node_mask, node_offset_data);

  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<MeshLocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    MeshLocalData &tls = all_tls.local();
    calc_factors_faces(depsgraph,
                       brush,
                       position_data.eval,
                       vert_normals,
                       vert_to_face_map,
                       attribute_data,
                       strength,
                       relax_face_sets,
                       object,
                       nodes[i],
                       tls,
                       factors.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    smooth::calc_relaxed_translations_faces(
        position_data.eval,
        vert_normals,
        faces,
        corner_verts,
        vert_to_face_map,
        ss.vertex_info.boundary,
        attribute_data.face_sets,
        attribute_data.hide_poly,
        relax_face_sets,
        nodes[i].verts(),
        factors.as_span().slice(node_vert_offsets[pos]),
        translations.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    apply_positions_faces(sd,
                          nodes[i].verts(),
                          object,
                          translations.as_mutable_span().slice(node_vert_offsets[pos]),
                          position_data);
    bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
  });
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

BLI_NOINLINE static void calc_factors_grids(const Depsgraph &depsgraph,
                                            const Brush &brush,
                                            const OffsetIndices<int> faces,
                                            const Span<int> corner_verts,
                                            const GroupedSpan<int> vert_to_face_map,
                                            const Span<int> face_sets,
                                            const bke::pbvh::GridsNode &node,
                                            const float strength,
                                            const bool relax_face_sets,
                                            Object &object,
                                            GridLocalData &tls,
                                            const MutableSpan<float3> positions,
                                            const MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;

  gather_data_grids(subdiv_ccg, subdiv_ccg.positions.as_span(), grids, positions);

  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, subdiv_ccg, grids, factors);
  }

  tls.distances.resize(grid_verts_num);
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions, factors);

  face_set::filter_verts_with_unique_face_sets_grids(faces,
                                                     corner_verts,
                                                     vert_to_face_map,
                                                     face_sets,
                                                     subdiv_ccg,
                                                     relax_face_sets,
                                                     grids,
                                                     factors);
}

static void do_relax_face_sets_brush_grids(const Depsgraph &depsgraph,
                                           const Sculpt &sd,
                                           const Brush &brush,
                                           Object &object,
                                           const IndexMask &node_mask,
                                           const float strength,
                                           const bool relax_face_sets)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  MutableSpan<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      key, nodes, node_mask, node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<GridLocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    GridLocalData &tls = all_tls.local();
    calc_factors_grids(depsgraph,
                       brush,
                       faces,
                       corner_verts,
                       vert_to_face_map,
                       face_sets,
                       nodes[i],
                       strength,
                       relax_face_sets,
                       object,
                       tls,
                       current_positions.as_mutable_span().slice(node_vert_offsets[pos]),
                       factors.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    smooth::calc_relaxed_translations_grids(
        subdiv_ccg,
        faces,
        corner_verts,
        face_sets,
        vert_to_face_map,
        ss.vertex_info.boundary,
        nodes[i].grids(),
        relax_face_sets,
        factors.as_span().slice(node_vert_offsets[pos]),
        translations.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    apply_positions_grids(sd,
                          nodes[i].grids(),
                          object,
                          current_positions.as_mutable_span().slice(node_vert_offsets[pos]),
                          translations.as_mutable_span().slice(node_vert_offsets[pos]));
    bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
  });
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

static void calc_factors_bmesh(const Depsgraph &depsgraph,
                               Object &object,
                               const Brush &brush,
                               const int face_set_offset,
                               bke::pbvh::BMeshNode &node,
                               const float strength,
                               const bool relax_face_sets,
                               BMeshLocalData &tls,
                               MutableSpan<float3> positions,
                               MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  gather_bmesh_positions(verts, positions);

  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions, factors);
  face_set::filter_verts_with_unique_face_sets_bmesh(
      face_set_offset, relax_face_sets, verts, factors);
}

static void do_relax_face_sets_brush_bmesh(const Depsgraph &depsgraph,
                                           const Sculpt &sd,
                                           const Brush &brush,
                                           Object &object,
                                           const IndexMask &node_mask,
                                           const float strength,
                                           const bool relax_face_sets)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets_bmesh(
      nodes, node_mask, node_offset_data);

  const int face_set_offset = CustomData_get_offset_named(
      &object.sculpt->bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<BMeshLocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    BMeshLocalData &tls = all_tls.local();
    calc_factors_bmesh(depsgraph,
                       object,
                       brush,
                       face_set_offset,
                       nodes[i],
                       strength,
                       relax_face_sets,
                       tls,
                       current_positions.as_mutable_span().slice(node_vert_offsets[pos]),
                       factors.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    smooth::calc_relaxed_translations_bmesh(
        BKE_pbvh_bmesh_node_unique_verts(&nodes[i]),
        current_positions.as_mutable_span().slice(node_vert_offsets[pos]),
        face_set_offset,
        relax_face_sets,
        factors.as_span().slice(node_vert_offsets[pos]),
        translations.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    apply_positions_bmesh(sd,
                          BKE_pbvh_bmesh_node_unique_verts(&nodes[i]),
                          object,
                          translations.as_mutable_span().slice(node_vert_offsets[pos]),
                          current_positions.as_span().slice(node_vert_offsets[pos]));
    bke::pbvh::update_node_bounds_bmesh(nodes[i]);
  });
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Topology Relax
 * \{ */
BLI_NOINLINE static void calc_topology_relax_factors_faces(const Depsgraph &depsgraph,
                                                           const Brush &brush,
                                                           const float strength,
                                                           const Object &object,
                                                           const MeshAttributeData &attribute_data,
                                                           const bke::pbvh::MeshNode &node,
                                                           MeshLocalData &tls,
                                                           const MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);
  const Span<int> verts = node.verts();

  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, orig_data.positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, orig_data.normals, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, orig_data.positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, orig_data.positions, factors);
}

static void do_topology_relax_brush_mesh(const Depsgraph &depsgraph,
                                         const Sculpt &sd,
                                         const Brush &brush,
                                         Object &object,
                                         const IndexMask &node_mask,
                                         const float strength)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const MeshAttributeData attribute_data(mesh);
  const PositionDeformData position_data(depsgraph, object);
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      nodes, node_mask, node_offset_data);

  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<MeshLocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    MeshLocalData &tls = all_tls.local();
    calc_topology_relax_factors_faces(depsgraph,
                                      brush,
                                      strength,
                                      object,
                                      attribute_data,
                                      nodes[i],
                                      tls,
                                      factors.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    smooth::calc_relaxed_translations_faces(
        position_data.eval,
        vert_normals,
        faces,
        corner_verts,
        vert_to_face_map,
        ss.vertex_info.boundary,
        attribute_data.face_sets,
        attribute_data.hide_poly,
        false,
        nodes[i].verts(),
        factors.as_span().slice(node_vert_offsets[pos]),
        translations.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    apply_positions_faces(sd,
                          nodes[i].verts(),
                          object,
                          translations.as_mutable_span().slice(node_vert_offsets[pos]),
                          position_data);
    bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
  });
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

BLI_NOINLINE static void calc_topology_relax_factors_grids(const Depsgraph &depsgraph,
                                                           const Brush &brush,
                                                           const float strength,
                                                           const Object &object,
                                                           const bke::pbvh::GridsNode &node,
                                                           GridLocalData &tls,
                                                           const MutableSpan<float3> positions,
                                                           const MutableSpan<float> factors)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;

  gather_data_grids(subdiv_ccg, subdiv_ccg.positions.as_span(), grids, positions);
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, orig_data.positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, orig_data.normals, factors);
  }

  tls.distances.resize(grid_verts_num);
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, orig_data.positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, orig_data.positions, factors);
}

static void do_topology_relax_brush_grids(const Depsgraph &depsgraph,
                                          const Sculpt &sd,
                                          const Brush &brush,
                                          Object &object,
                                          const IndexMask &node_mask,
                                          const float strength)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
  MutableSpan<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      key, nodes, node_mask, node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<GridLocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    GridLocalData &tls = all_tls.local();
    calc_topology_relax_factors_grids(
        depsgraph,
        brush,
        strength,
        object,
        nodes[i],
        tls,
        current_positions.as_mutable_span().slice(node_vert_offsets[pos]),
        factors.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    smooth::calc_relaxed_translations_grids(
        subdiv_ccg,
        faces,
        corner_verts,
        face_sets,
        vert_to_face_map,
        ss.vertex_info.boundary,
        nodes[i].grids(),
        false,
        factors.as_span().slice(node_vert_offsets[pos]),
        translations.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    apply_positions_grids(sd,
                          nodes[i].grids(),
                          object,
                          current_positions.as_mutable_span().slice(node_vert_offsets[pos]),
                          translations.as_mutable_span().slice(node_vert_offsets[pos]));
    bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
  });
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

static void calc_topology_relax_factors_bmesh(const Depsgraph &depsgraph,
                                              Object &object,
                                              const Brush &brush,
                                              bke::pbvh::BMeshNode &node,
                                              const float strength,
                                              BMeshLocalData &tls,
                                              MutableSpan<float3> positions,
                                              MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  gather_bmesh_positions(verts, positions);

  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, orig_positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, orig_normals, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, orig_positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, orig_positions, factors);
}

static void do_topology_relax_brush_bmesh(const Depsgraph &depsgraph,
                                          const Sculpt &sd,
                                          const Brush &brush,
                                          Object &object,
                                          const IndexMask &node_mask,
                                          const float strength)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  const int face_set_offset = CustomData_get_offset_named(
      &object.sculpt->bm->pdata, CD_PROP_INT32, ".sculpt_face_set");

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets_bmesh(
      nodes, node_mask, node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<BMeshLocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    BMeshLocalData &tls = all_tls.local();
    calc_topology_relax_factors_bmesh(
        depsgraph,
        object,
        brush,
        nodes[i],
        strength,
        tls,
        current_positions.as_mutable_span().slice(node_vert_offsets[pos]),
        factors.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    smooth::calc_relaxed_translations_bmesh(
        BKE_pbvh_bmesh_node_unique_verts(&nodes[i]),
        current_positions.as_mutable_span().slice(node_vert_offsets[pos]),
        face_set_offset,
        false,
        factors.as_span().slice(node_vert_offsets[pos]),
        translations.as_mutable_span().slice(node_vert_offsets[pos]));
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
    apply_positions_bmesh(sd,
                          BKE_pbvh_bmesh_node_unique_verts(&nodes[i]),
                          object,
                          translations.as_mutable_span().slice(node_vert_offsets[pos]),
                          current_positions.as_span().slice(node_vert_offsets[pos]));
    bke::pbvh::update_node_bounds_bmesh(nodes[i]);
  });
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}
/** \} */

}  // namespace relax_cc

void do_relax_face_sets_brush(const Depsgraph &depsgraph,
                              const Sculpt &sd,
                              Object &object,
                              const IndexMask &node_mask)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  boundary::ensure_boundary_info(object);

  const SculptSession &ss = *object.sculpt;
  const std::array<float, 4> strengths = iteration_strengths(ss.cache->bstrength,
                                                             ss.cache->iteration_count);

  /* On every third step of the stroke, behave more similarly to the Topology Relax brush */
  const bool relax_face_sets = !(ss.cache->iteration_count % 3 == 0);

  for (const float strength : strengths) {
    switch (bke::object::pbvh_get(object)->type()) {
      case bke::pbvh::Type::Mesh:
        do_relax_face_sets_brush_mesh(
            depsgraph, sd, brush, object, node_mask, strength * strength, relax_face_sets);
        break;
      case bke::pbvh::Type::Grids:
        do_relax_face_sets_brush_grids(
            depsgraph, sd, brush, object, node_mask, strength * strength, relax_face_sets);
        break;
      case bke::pbvh::Type::BMesh:
        do_relax_face_sets_brush_bmesh(
            depsgraph, sd, brush, object, node_mask, strength * strength, relax_face_sets);
        break;
    }
  }
}

void do_topology_relax_brush(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const IndexMask &node_mask)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const SculptSession &ss = *object.sculpt;

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    return;
  }

  const float strength = ss.cache->bstrength;

  boundary::ensure_boundary_info(object);

  for (int i = 0; i < 4; i++) {
    switch (bke::object::pbvh_get(object)->type()) {
      case bke::pbvh::Type::Mesh:
        do_topology_relax_brush_mesh(depsgraph, sd, brush, object, node_mask, strength);
        break;
      case bke::pbvh::Type::Grids:
        do_topology_relax_brush_grids(depsgraph, sd, brush, object, node_mask, strength);
        break;
      case bke::pbvh::Type::BMesh:
        do_topology_relax_brush_bmesh(depsgraph, sd, brush, object, node_mask, strength);
        break;
    }
  }
}
}  // namespace blender::ed::sculpt_paint::brushes
