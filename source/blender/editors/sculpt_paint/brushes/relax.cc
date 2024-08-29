/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_boundary.hh"
#include "editors/sculpt_paint/sculpt_face_set.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"
#include "editors/sculpt_paint/sculpt_smooth.hh"

namespace blender::ed::sculpt_paint {

inline namespace relax_cc {

/* -------------------------------------------------------------------- */
/** \name Relax Vertex
 * \{ */

struct MeshLocalData {
  Vector<float> factors;
  Vector<float> distances;
  Vector<Vector<int>> vert_neighbors;
};

struct GridLocalData {
  Vector<float> factors;
  Vector<float> distances;
  Vector<Vector<SubdivCCGCoord>> vert_neighbors;
};

struct BMeshLocalData {
  Vector<float> factors;
  Vector<float> distances;
  Vector<Vector<BMVert *>> vert_neighbors;
};

static void apply_positions_faces(const Depsgraph &depsgraph,
                                  const Sculpt &sd,
                                  const Span<float3> positions_eval,
                                  const Span<int> verts,
                                  Object &object,
                                  const MutableSpan<float3> translations,
                                  const MutableSpan<float3> positions_orig)
{
  write_translations(depsgraph, sd, object, positions_eval, verts, translations, positions_orig);
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

  /* This operations needs a strength tweak as the relax deformation is too weak by default. */
  const float modified_strength = strength * 1.5f;
  return {modified_strength, modified_strength, strength, strength};
}

BLI_NOINLINE static void calc_factors_faces(const Depsgraph &depsgraph,
                                            const Brush &brush,
                                            const Span<float3> positions_eval,
                                            const Span<float3> vert_normals,
                                            const float strength,
                                            const bool relax_face_sets,
                                            const Object &object,
                                            const bke::pbvh::MeshNode &node,
                                            MeshLocalData &tls,
                                            const MutableSpan<float> factors)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);

  fill_factor_from_hide_and_mask(mesh, verts, factors);
  filter_region_clip_factors(ss, positions_eval, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, vert_normals, verts, factors);
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
      ss.vert_to_face_map, ss.face_sets, relax_face_sets, verts, factors);
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
  MutableSpan<bke::pbvh::MeshNode> nodes = ss.pbvh->nodes<bke::pbvh::MeshNode>();
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
  MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      nodes, node_mask, node_offset_data);

  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<MeshLocalData> all_tls;
  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    MeshLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      calc_factors_faces(depsgraph,
                         brush,
                         positions_eval,
                         vert_normals,
                         strength,
                         relax_face_sets,
                         object,
                         nodes[i],
                         tls,
                         factors.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    MeshLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      smooth::calc_relaxed_translations_faces(
          positions_eval,
          vert_normals,
          faces,
          corner_verts,
          ss.vert_to_face_map,
          ss.vertex_info.boundary,
          ss.face_sets,
          hide_poly,
          relax_face_sets,
          bke::pbvh::node_unique_verts(nodes[i]),
          factors.as_span().slice(node_vert_offsets[i]),
          tls.vert_neighbors,
          translations.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    apply_positions_faces(depsgraph,
                          sd,
                          positions_eval,
                          bke::pbvh::node_unique_verts(nodes[i]),
                          object,
                          translations.as_mutable_span().slice(node_vert_offsets[i]),
                          positions_orig);
  });
}

BLI_NOINLINE static void calc_factors_grids(const Depsgraph &depsgraph,
                                            const Brush &brush,
                                            const Span<int> corner_verts,
                                            const OffsetIndices<int> faces,
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
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const int grid_verts_num = grids.size() * key.grid_area;

  gather_grids_positions(key, subdiv_ccg.grids, grids, positions);

  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, subdiv_ccg, grids, factors);
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

  face_set::filter_verts_with_unique_face_sets_grids(ss.vert_to_face_map,
                                                     corner_verts,
                                                     faces,
                                                     subdiv_ccg,
                                                     ss.face_sets,
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
  MutableSpan<bke::pbvh::GridsNode> nodes = ss.pbvh->nodes<bke::pbvh::GridsNode>();
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      key, nodes, node_mask, node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<GridLocalData> all_tls;
  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    GridLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      calc_factors_grids(depsgraph,
                         brush,
                         corner_verts,
                         faces,
                         nodes[i],
                         strength,
                         relax_face_sets,
                         object,
                         tls,
                         current_positions.as_mutable_span().slice(node_vert_offsets[i]),
                         factors.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    GridLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      smooth::calc_relaxed_translations_grids(
          subdiv_ccg,
          faces,
          corner_verts,
          ss.face_sets,
          ss.vert_to_face_map,
          ss.vertex_info.boundary,
          bke::pbvh::node_grid_indices(nodes[i]),
          relax_face_sets,
          factors.as_span().slice(node_vert_offsets[i]),
          current_positions.as_span().slice(node_vert_offsets[i]),
          tls.vert_neighbors,
          translations.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    apply_positions_grids(sd,
                          bke::pbvh::node_grid_indices(nodes[i]),
                          object,
                          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
                          translations.as_mutable_span().slice(node_vert_offsets[i]));
  });
}

static void calc_factors_bmesh(const Depsgraph &depsgraph,
                               Object &object,
                               const Brush &brush,
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
    calc_front_face(cache.view_normal, verts, factors);
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
  face_set::filter_verts_with_unique_face_sets_bmesh(relax_face_sets, verts, factors);
}

static void do_relax_face_sets_brush_bmesh(const Depsgraph &depsgraph,
                                           const Sculpt &sd,
                                           const Brush &brush,
                                           Object &object,
                                           const IndexMask &node_mask,
                                           const float strength,
                                           const bool relax_face_sets)
{
  SculptSession &ss = *object.sculpt;
  MutableSpan<bke::pbvh::BMeshNode> nodes = ss.pbvh->nodes<bke::pbvh::BMeshNode>();
  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets_bmesh(
      nodes, node_mask, node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<BMeshLocalData> all_tls;
  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    BMeshLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      calc_factors_bmesh(depsgraph,
                         object,
                         brush,
                         nodes[i],
                         strength,
                         relax_face_sets,
                         tls,
                         current_positions.as_mutable_span().slice(node_vert_offsets[i]),
                         factors.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    BMeshLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      smooth::calc_relaxed_translations_bmesh(
          BKE_pbvh_bmesh_node_unique_verts(&nodes[i]),
          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
          relax_face_sets,
          factors.as_span().slice(node_vert_offsets[i]),
          tls.vert_neighbors,
          translations.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    apply_positions_bmesh(sd,
                          BKE_pbvh_bmesh_node_unique_verts(&nodes[i]),
                          object,
                          translations.as_mutable_span().slice(node_vert_offsets[i]),
                          current_positions.as_span().slice(node_vert_offsets[i]));
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Topology Relax
 * \{ */
BLI_NOINLINE static void calc_topology_relax_factors_faces(const Depsgraph &depsgraph,
                                                           const Brush &brush,
                                                           const float strength,
                                                           const Object &object,
                                                           const bke::pbvh::MeshNode &node,
                                                           MeshLocalData &tls,
                                                           const MutableSpan<float> factors)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);
  const Span<int> verts = bke::pbvh::node_unique_verts(node);

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
  MutableSpan<bke::pbvh::MeshNode> nodes = ss.pbvh->nodes<bke::pbvh::MeshNode>();
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
  MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      nodes, node_mask, node_offset_data);

  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<MeshLocalData> all_tls;
  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    MeshLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      calc_topology_relax_factors_faces(depsgraph,
                                        brush,
                                        strength,
                                        object,
                                        nodes[i],
                                        tls,
                                        factors.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    MeshLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      smooth::calc_relaxed_translations_faces(
          positions_eval,
          vert_normals,
          faces,
          corner_verts,
          ss.vert_to_face_map,
          ss.vertex_info.boundary,
          ss.face_sets,
          hide_poly,
          false,
          bke::pbvh::node_unique_verts(nodes[i]),
          factors.as_span().slice(node_vert_offsets[i]),
          tls.vert_neighbors,
          translations.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    apply_positions_faces(depsgraph,
                          sd,
                          positions_eval,
                          bke::pbvh::node_unique_verts(nodes[i]),
                          object,
                          translations.as_mutable_span().slice(node_vert_offsets[i]),
                          positions_orig);
  });
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

  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const int grid_verts_num = grids.size() * key.grid_area;

  gather_grids_positions(key, subdiv_ccg.grids, grids, positions);
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, orig_data.positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, orig_data.normals, factors);
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
  MutableSpan<bke::pbvh::GridsNode> nodes = ss.pbvh->nodes<bke::pbvh::GridsNode>();
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      key, nodes, node_mask, node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<GridLocalData> all_tls;
  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    GridLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      calc_topology_relax_factors_grids(
          depsgraph,
          brush,
          strength,
          object,
          nodes[i],
          tls,
          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
          factors.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    GridLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      smooth::calc_relaxed_translations_grids(
          subdiv_ccg,
          faces,
          corner_verts,
          ss.face_sets,
          ss.vert_to_face_map,
          ss.vertex_info.boundary,
          bke::pbvh::node_grid_indices(nodes[i]),
          false,
          factors.as_span().slice(node_vert_offsets[i]),
          current_positions.as_span().slice(node_vert_offsets[i]),
          tls.vert_neighbors,
          translations.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    apply_positions_grids(sd,
                          bke::pbvh::node_grid_indices(nodes[i]),
                          object,
                          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
                          translations.as_mutable_span().slice(node_vert_offsets[i]));
  });
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
    calc_front_face(cache.view_normal, orig_normals, factors);
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
  const SculptSession &ss = *object.sculpt;
  MutableSpan<bke::pbvh::BMeshNode> nodes = ss.pbvh->nodes<bke::pbvh::BMeshNode>();

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets_bmesh(
      nodes, node_mask, node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<BMeshLocalData> all_tls;
  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    BMeshLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      calc_topology_relax_factors_bmesh(
          depsgraph,
          object,
          brush,
          nodes[i],
          strength,
          tls,
          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
          factors.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
    BMeshLocalData &tls = all_tls.local();
    node_mask.slice(range).foreach_index([&](const int i) {
      smooth::calc_relaxed_translations_bmesh(
          BKE_pbvh_bmesh_node_unique_verts(&nodes[i]),
          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
          false,
          factors.as_span().slice(node_vert_offsets[i]),
          tls.vert_neighbors,
          translations.as_mutable_span().slice(node_vert_offsets[i]));
    });
  });

  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    apply_positions_bmesh(sd,
                          BKE_pbvh_bmesh_node_unique_verts(&nodes[i]),
                          object,
                          translations.as_mutable_span().slice(node_vert_offsets[i]),
                          current_positions.as_span().slice(node_vert_offsets[i]));
  });
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
    switch (ss.pbvh->type()) {
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
    switch (ss.pbvh->type()) {
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
}  // namespace blender::ed::sculpt_paint
