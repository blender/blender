/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_base.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

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

static float3 translation_to_plane(const float3 &current_position,
                                   const float3 &normal,
                                   const float3 &smoothed_position)
{
  float4 plane;
  plane_from_point_normal_v3(plane, current_position, normal);

  float3 smooth_closest_plane;
  closest_to_plane_v3(smooth_closest_plane, plane, smoothed_position);

  return smooth_closest_plane - current_position;
}

static bool get_normal_boundary(const float3 &current_position,
                                const Span<float3> vert_positions,
                                const Span<int> neighbors,
                                float3 &r_new_normal)
{
  /* If we are not dealing with a corner vertex, skip this step.*/
  if (neighbors.size() != 2) {
    return false;
  }

  float3 normal(0);
  for (const int vert : neighbors) {
    const float3 to_neighbor = vert_positions[vert] - current_position;
    normal += math::normalize(to_neighbor);
  }

  r_new_normal = math::normalize(normal);

  return true;
}

static float3 average_positions(const Span<float3> vert_positions, const Span<int> neighbors)
{
  const float factor = math::rcp(float(neighbors.size()));
  float3 result(0);
  for (const int vert : neighbors) {
    result += vert_positions[vert] * factor;
  }
  return result;
}

static bool get_normal_boundary(const CCGKey &key,
                                const Span<CCGElem *> elems,
                                const float3 &current_position,
                                const Span<SubdivCCGCoord> neighbors,
                                float3 &r_new_normal)
{
  /* If we are not dealing with a corner vertex, skip this step.*/
  if (neighbors.size() != 2) {
    return false;
  }

  float3 normal(0);
  for (const SubdivCCGCoord &coord : neighbors) {
    const float3 to_neighbor = CCG_grid_elem_co(key, elems[coord.grid_index], coord.x, coord.y) -
                               current_position;
    normal += math::normalize(to_neighbor);
  }

  r_new_normal = math::normalize(normal);

  return true;
}

static float3 average_positions(const CCGKey &key,
                                const Span<CCGElem *> elems,
                                const Span<float3> positions,
                                const Span<SubdivCCGCoord> neighbors,
                                const int current_grid,
                                const int current_grid_start)
{
  const float factor = math::rcp(float(neighbors.size()));
  float3 result(0);
  for (const SubdivCCGCoord &coord : neighbors) {
    if (current_grid == coord.grid_index) {
      const int offset = CCG_grid_xy_to_index(key.grid_size, coord.x, coord.y);
      result += positions[current_grid_start + offset] * factor;
    }
    else {
      result += CCG_grid_elem_co(key, elems[coord.grid_index], coord.x, coord.y) * factor;
    }
  }
  return result;
}

static bool get_normal_boundary(const float3 &current_position,
                                const Span<BMVert *> neighbors,
                                float3 &r_new_normal)
{
  /* If we are not dealing with a corner vertex, skip this step.*/
  if (neighbors.size() != 2) {
    return false;
  }

  float3 normal(0);
  for (BMVert *vert : neighbors) {
    const float3 neighbor_pos = vert->co;
    const float3 to_neighbor = neighbor_pos - current_position;
    normal += math::normalize(to_neighbor);
  }

  r_new_normal = math::normalize(normal);

  return true;
}

static float3 average_positions(const Span<const BMVert *> verts)
{
  const float factor = math::rcp(float(verts.size()));
  float3 result(0);
  for (const BMVert *vert : verts) {
    result += float3(vert->co) * factor;
  }
  return result;
}

BLI_NOINLINE static void calc_relaxed_translations_faces(const Span<float3> vert_positions,
                                                         const Span<float3> vert_normals,
                                                         const OffsetIndices<int> faces,
                                                         const Span<int> corner_verts,
                                                         const GroupedSpan<int> vert_to_face_map,
                                                         const BitSpan boundary_verts,
                                                         const int *face_sets,
                                                         const Span<bool> hide_poly,
                                                         const bool filter_boundary_face_sets,
                                                         const Span<int> verts,
                                                         const Span<float> factors,
                                                         MeshLocalData &tls,
                                                         const MutableSpan<float3> translations)
{
  BLI_assert(verts.size() == factors.size());
  BLI_assert(verts.size() == translations.size());

  tls.vert_neighbors.resize(verts.size());
  const MutableSpan<Vector<int>> neighbors = tls.vert_neighbors;
  calc_vert_neighbors_interior(
      faces, corner_verts, vert_to_face_map, boundary_verts, hide_poly, verts, neighbors);

  for (const int i : verts.index_range()) {
    if (factors[i] == 0.0f) {
      translations[i] = float3(0);
      continue;
    }

    /* Don't modify corner vertices */
    if (neighbors[i].size() <= 2) {
      translations[i] = float3(0);
      continue;
    }

    const bool is_boundary = boundary_verts[verts[i]];
    if (is_boundary) {
      neighbors[i].remove_if([&](const int vert) { return !boundary_verts[vert]; });
    }

    if (filter_boundary_face_sets) {
      neighbors[i].remove_if([&](const int vert) {
        return face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, vert);
      });
    }

    if (neighbors[i].is_empty()) {
      translations[i] = float3(0);
      continue;
    }

    const float3 smoothed_position = average_positions(vert_positions, neighbors[i]);

    /* Normal Calculation */
    float3 normal;
    if (is_boundary) {
      bool has_boundary_normal = get_normal_boundary(
          vert_positions[verts[i]], vert_positions, neighbors[i], normal);

      if (!has_boundary_normal) {
        normal = vert_normals[verts[i]];
      }
    }
    else {
      normal = vert_normals[verts[i]];
    }

    if (math::is_zero(normal)) {
      translations[i] = float3(0);
      continue;
    }

    const float3 translation = translation_to_plane(
        vert_positions[verts[i]], normal, smoothed_position);

    translations[i] = translation * factors[i];
  }
}

static void apply_positions_faces(const Sculpt &sd,
                                  const Span<float3> positions_eval,
                                  const Span<int> verts,
                                  Object &object,
                                  const MutableSpan<float3> translations,
                                  const MutableSpan<float3> positions_orig)
{
  write_translations(sd, object, positions_eval, verts, translations, positions_orig);
}

BLI_NOINLINE static void calc_relaxed_translations_grids(const SubdivCCG &subdiv_ccg,
                                                         const OffsetIndices<int> faces,
                                                         const Span<int> corner_verts,
                                                         const int *face_sets,
                                                         const GroupedSpan<int> vert_to_face_map,
                                                         const BitSpan boundary_verts,
                                                         const Span<int> grids,
                                                         const bool filter_boundary_face_sets,
                                                         GridLocalData &tls,
                                                         const Span<float> factors,
                                                         const Span<float3> positions,
                                                         const MutableSpan<float3> translations)
{
  const Span<CCGElem *> elems = subdiv_ccg.grids;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const int grid_verts_num = grids.size() * key.grid_area;
  BLI_assert(grid_verts_num == translations.size());
  BLI_assert(grid_verts_num == factors.size());

  tls.vert_neighbors.resize(grid_verts_num);
  const MutableSpan<Vector<SubdivCCGCoord>> neighbors = tls.vert_neighbors;
  calc_vert_neighbors_interior(faces, corner_verts, boundary_verts, subdiv_ccg, grids, neighbors);

  for (const int i : grids.index_range()) {
    CCGElem *elem = elems[grids[i]];
    const int node_start = i * key.grid_area;
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert = node_start + offset;
        if (factors[node_vert] == 0.0f) {
          translations[node_vert] = float3(0);
          continue;
        }

        /* Don't modify corner vertices */
        if (neighbors[node_vert].size() <= 2) {
          translations[node_vert] = float3(0);
          continue;
        }

        SubdivCCGCoord coord{};
        coord.grid_index = grids[i];
        coord.x = x;
        coord.y = y;

        const bool is_boundary = BKE_subdiv_ccg_coord_is_mesh_boundary(
            faces, corner_verts, boundary_verts, subdiv_ccg, coord);

        if (is_boundary) {
          neighbors[node_vert].remove_if([&](const SubdivCCGCoord neighbor) {
            return !BKE_subdiv_ccg_coord_is_mesh_boundary(
                faces, corner_verts, boundary_verts, subdiv_ccg, neighbor);
          });
        }

        if (filter_boundary_face_sets) {
          neighbors[node_vert].remove_if([&](const SubdivCCGCoord neighbor) {
            return face_set::vert_has_unique_face_set(
                vert_to_face_map, corner_verts, faces, face_sets, subdiv_ccg, neighbor);
          });
        }

        if (neighbors[i].is_empty()) {
          translations[node_vert] = float3(0);
          continue;
        }

        const float3 smoothed_position = average_positions(
            key, elems, positions, neighbors[node_vert], grids[i], node_start);

        /* Normal Calculation */
        float3 normal;
        if (is_boundary) {
          bool has_boundary_normal = get_normal_boundary(
              key, elems, positions[node_vert], neighbors[node_vert], normal);

          if (!has_boundary_normal) {
            normal = CCG_elem_offset_no(key, elem, offset);
          }
        }
        else {
          normal = CCG_elem_offset_no(key, elem, offset);
        }

        if (math::is_zero(normal)) {
          translations[node_vert] = float3(0);
          continue;
        }

        const float3 translation = translation_to_plane(
            positions[node_vert], normal, smoothed_position);

        translations[node_vert] = translation * factors[node_vert];
      }
    }
  }
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

BLI_NOINLINE static void calc_relaxed_translations_bmesh(const Set<BMVert *, 0> &verts,
                                                         const Span<float3> positions,
                                                         const bool filter_boundary_face_sets,
                                                         BMeshLocalData &tls,
                                                         const Span<float> factors,
                                                         const MutableSpan<float3> translations)
{
  BLI_assert(verts.size() == factors.size());
  BLI_assert(verts.size() == translations.size());

  tls.vert_neighbors.resize(verts.size());
  const MutableSpan<Vector<BMVert *>> neighbors = tls.vert_neighbors;
  calc_vert_neighbors_interior(verts, neighbors);

  int i = 0;
  for (const BMVert *vert : verts) {
    if (factors[i] == 0.0f) {
      translations[i] = float3(0);
      i++;
      continue;
    }

    /* Don't modify corner vertices */
    if (neighbors[i].size() <= 2) {
      translations[i] = float3(0);
      i++;
      continue;
    }

    const bool is_boundary = BM_vert_is_boundary(vert);
    if (is_boundary) {
      neighbors[i].remove_if([&](const BMVert *vert) { return !BM_vert_is_boundary(vert); });
    }

    if (filter_boundary_face_sets) {
      neighbors[i].remove_if(
          [&](const BMVert *vert) { return face_set::vert_has_unique_face_set(vert); });
    }

    if (neighbors[i].is_empty()) {
      translations[i] = float3(0);
      i++;
      continue;
    }

    const float3 smoothed_position = average_positions(neighbors[i]);

    /* Normal Calculation */
    float3 normal;
    if (is_boundary) {
      bool has_boundary_normal = get_normal_boundary(positions[i], neighbors[i], normal);

      if (!has_boundary_normal) {
        normal = vert->no;
      }
    }
    else {
      normal = vert->no;
    }

    if (math::is_zero(normal)) {
      translations[i] = float3(0);
      i++;
      continue;
    }

    const float3 translation = translation_to_plane(positions[i], normal, smoothed_position);

    translations[i] = translation * factors[i];
    i++;
  }
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

BLI_NOINLINE static void filter_factors_on_face_sets_mesh(const GroupedSpan<int> vert_to_face_map,
                                                          const int *face_sets,
                                                          const bool relax_face_sets,
                                                          const Span<int> verts,
                                                          const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());

  for (const int i : verts.index_range()) {
    if (relax_face_sets ==
        face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, verts[i]))
    {
      factors[i] = 0.0f;
    }
  }
}
BLI_NOINLINE static void filter_factors_on_face_sets_grids(const GroupedSpan<int> vert_to_face_map,
                                                           const Span<int> corner_verts,
                                                           const OffsetIndices<int> faces,
                                                           const SubdivCCG &subdiv_ccg,
                                                           const int *face_sets,
                                                           const bool relax_face_sets,
                                                           const Span<int> grids,
                                                           const MutableSpan<float> factors)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  BLI_assert(grids.size() * key.grid_area == factors.size());

  for (const int i : grids.index_range()) {
    const int node_start = i * key.grid_area;
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert = node_start + offset;
        if (factors[node_vert] == 0.0f) {
          continue;
        }

        SubdivCCGCoord coord{};
        coord.grid_index = grids[i];
        coord.x = x;
        coord.y = y;
        if (relax_face_sets ==
            face_set::vert_has_unique_face_set(
                vert_to_face_map, corner_verts, faces, face_sets, subdiv_ccg, coord))
        {
          factors[node_vert] = 0.0f;
        }
      }
    }
  }
}
BLI_NOINLINE static void filter_factors_on_face_sets_bmesh(const bool relax_face_sets,
                                                           const Set<BMVert *, 0> verts,
                                                           const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());

  int i = 0;
  for (const BMVert *vert : verts) {
    if (relax_face_sets == face_set::vert_has_unique_face_set(vert)) {
      factors[i] = 0.0f;
    }
    i++;
  }
}

BLI_NOINLINE static void calc_factors_faces(const Brush &brush,
                                            const Span<float3> positions_eval,
                                            const Span<float3> vert_normals,
                                            const float strength,
                                            const bool relax_face_sets,
                                            const Object &object,
                                            const bke::pbvh::Node &node,
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

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

  filter_factors_on_face_sets_mesh(
      ss.vert_to_face_map, ss.face_sets, relax_face_sets, verts, factors);
}

static void do_relax_face_sets_brush_mesh(const Sculpt &sd,
                                          const Brush &brush,
                                          Object &object,
                                          const Span<bke::pbvh::Node *> nodes,
                                          const float strength,
                                          const bool relax_face_sets)
{
  const SculptSession &ss = *object.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const bke::pbvh::Tree &pbvh = *ss.pbvh;

  const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
  const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);
  MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(nodes, node_offset_data);

  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<MeshLocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    MeshLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_factors_faces(brush,
                         positions_eval,
                         vert_normals,
                         strength,
                         relax_face_sets,
                         object,
                         *nodes[i],
                         tls,
                         factors.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    MeshLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_relaxed_translations_faces(positions_eval,
                                      vert_normals,
                                      faces,
                                      corner_verts,
                                      ss.vert_to_face_map,
                                      ss.vertex_info.boundary,
                                      ss.face_sets,
                                      hide_poly,
                                      relax_face_sets,
                                      bke::pbvh::node_unique_verts(*nodes[i]),
                                      factors.as_span().slice(node_vert_offsets[i]),
                                      tls,
                                      translations.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      apply_positions_faces(sd,
                            positions_eval,
                            bke::pbvh::node_unique_verts(*nodes[i]),
                            object,
                            translations.as_mutable_span().slice(node_vert_offsets[i]),
                            positions_orig);
    }
  });
}

BLI_NOINLINE static void calc_factors_grids(const Brush &brush,
                                            const Span<int> corner_verts,
                                            const OffsetIndices<int> faces,
                                            const bke::pbvh::Node &node,
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

  if (cache.automasking) {
    auto_mask::calc_grids_factors(object, *cache.automasking, node, grids, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions, factors);

  filter_factors_on_face_sets_grids(ss.vert_to_face_map,
                                    corner_verts,
                                    faces,
                                    subdiv_ccg,
                                    ss.face_sets,
                                    relax_face_sets,
                                    grids,
                                    factors);
}

static void do_relax_face_sets_brush_grids(const Sculpt &sd,
                                           const Brush &brush,
                                           Object &object,
                                           const Span<bke::pbvh::Node *> nodes,
                                           const float strength,
                                           const bool relax_face_sets)
{
  const SculptSession &ss = *object.sculpt;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      nodes, key, node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<GridLocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    GridLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_factors_grids(brush,
                         corner_verts,
                         faces,
                         *nodes[i],
                         strength,
                         relax_face_sets,
                         object,
                         tls,
                         current_positions.as_mutable_span().slice(node_vert_offsets[i]),
                         factors.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    GridLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_relaxed_translations_grids(subdiv_ccg,
                                      faces,
                                      corner_verts,
                                      ss.face_sets,
                                      ss.vert_to_face_map,
                                      ss.vertex_info.boundary,
                                      bke::pbvh::node_grid_indices(*nodes[i]),
                                      relax_face_sets,
                                      tls,
                                      factors.as_span().slice(node_vert_offsets[i]),
                                      current_positions.as_span().slice(node_vert_offsets[i]),
                                      translations.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      apply_positions_grids(sd,
                            bke::pbvh::node_grid_indices(*nodes[i]),
                            object,
                            current_positions.as_mutable_span().slice(node_vert_offsets[i]),
                            translations.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });
}

static void calc_factors_bmesh(Object &object,
                               const Brush &brush,
                               bke::pbvh::Node &node,
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

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions, factors);
  filter_factors_on_face_sets_bmesh(relax_face_sets, verts, factors);
}

static void do_relax_face_sets_brush_bmesh(const Sculpt &sd,
                                           const Brush &brush,
                                           Object &object,
                                           const Span<bke::pbvh::Node *> nodes,
                                           const float strength,
                                           const bool relax_face_sets)
{
  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets_bmesh(nodes,
                                                                              node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<BMeshLocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    BMeshLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_factors_bmesh(object,
                         brush,
                         *nodes[i],
                         strength,
                         relax_face_sets,
                         tls,
                         current_positions.as_mutable_span().slice(node_vert_offsets[i]),
                         factors.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    BMeshLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_relaxed_translations_bmesh(
          BKE_pbvh_bmesh_node_unique_verts(nodes[i]),
          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
          relax_face_sets,
          tls,
          factors.as_span().slice(node_vert_offsets[i]),
          translations.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      apply_positions_bmesh(sd,
                            BKE_pbvh_bmesh_node_unique_verts(nodes[i]),
                            object,
                            translations.as_mutable_span().slice(node_vert_offsets[i]),
                            current_positions.as_span().slice(node_vert_offsets[i]));
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Topology Relax
 * \{ */
BLI_NOINLINE static void calc_topology_relax_factors_faces(const Brush &brush,
                                                           const float strength,
                                                           const Object &object,
                                                           const bke::pbvh::Node &node,
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

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, orig_data.positions, factors);
}

static void do_topology_relax_brush_mesh(const Sculpt &sd,
                                         const Brush &brush,
                                         Object &object,
                                         const Span<bke::pbvh::Node *> nodes,
                                         const float strength)
{
  const SculptSession &ss = *object.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const bke::pbvh::Tree &pbvh = *ss.pbvh;

  const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
  const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);
  MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(nodes, node_offset_data);

  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<MeshLocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    MeshLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_topology_relax_factors_faces(brush,
                                        strength,
                                        object,
                                        *nodes[i],
                                        tls,
                                        factors.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    MeshLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_relaxed_translations_faces(positions_eval,
                                      vert_normals,
                                      faces,
                                      corner_verts,
                                      ss.vert_to_face_map,
                                      ss.vertex_info.boundary,
                                      ss.face_sets,
                                      hide_poly,
                                      false,
                                      bke::pbvh::node_unique_verts(*nodes[i]),
                                      factors.as_span().slice(node_vert_offsets[i]),
                                      tls,
                                      translations.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      apply_positions_faces(sd,
                            positions_eval,
                            bke::pbvh::node_unique_verts(*nodes[i]),
                            object,
                            translations.as_mutable_span().slice(node_vert_offsets[i]),
                            positions_orig);
    }
  });
}

BLI_NOINLINE static void calc_topology_relax_factors_grids(const Brush &brush,
                                                           const float strength,
                                                           const Object &object,
                                                           const bke::pbvh::Node &node,
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

  if (cache.automasking) {
    auto_mask::calc_grids_factors(object, *cache.automasking, node, grids, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, orig_data.positions, factors);
}

static void do_topology_relax_brush_grids(const Sculpt &sd,
                                          const Brush &brush,
                                          Object &object,
                                          const Span<bke::pbvh::Node *> nodes,
                                          const float strength)
{
  const SculptSession &ss = *object.sculpt;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets(
      nodes, key, node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<GridLocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    GridLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_topology_relax_factors_grids(
          brush,
          strength,
          object,
          *nodes[i],
          tls,
          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
          factors.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    GridLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_relaxed_translations_grids(subdiv_ccg,
                                      faces,
                                      corner_verts,
                                      ss.face_sets,
                                      ss.vert_to_face_map,
                                      ss.vertex_info.boundary,
                                      bke::pbvh::node_grid_indices(*nodes[i]),
                                      false,
                                      tls,
                                      factors.as_span().slice(node_vert_offsets[i]),
                                      current_positions.as_span().slice(node_vert_offsets[i]),
                                      translations.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      apply_positions_grids(sd,
                            bke::pbvh::node_grid_indices(*nodes[i]),
                            object,
                            current_positions.as_mutable_span().slice(node_vert_offsets[i]),
                            translations.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });
}

static void calc_topology_relax_factors_bmesh(Object &object,
                                              const Brush &brush,
                                              bke::pbvh::Node &node,
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

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, orig_positions, factors);
}

static void do_topology_relax_brush_bmesh(const Sculpt &sd,
                                          const Brush &brush,
                                          Object &object,
                                          const Span<bke::pbvh::Node *> nodes,
                                          const float strength)
{
  Array<int> node_offset_data;
  const OffsetIndices<int> node_vert_offsets = create_node_vert_offsets_bmesh(nodes,
                                                                              node_offset_data);

  Array<float3> current_positions(node_vert_offsets.total_size());
  Array<float3> translations(node_vert_offsets.total_size());
  Array<float> factors(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<BMeshLocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    BMeshLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_topology_relax_factors_bmesh(
          object,
          brush,
          *nodes[i],
          strength,
          tls,
          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
          factors.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    BMeshLocalData &tls = all_tls.local();
    for (const int i : range) {
      calc_relaxed_translations_bmesh(
          BKE_pbvh_bmesh_node_unique_verts(nodes[i]),
          current_positions.as_mutable_span().slice(node_vert_offsets[i]),
          false,
          tls,
          factors.as_span().slice(node_vert_offsets[i]),
          translations.as_mutable_span().slice(node_vert_offsets[i]));
    }
  });

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      apply_positions_bmesh(sd,
                            BKE_pbvh_bmesh_node_unique_verts(nodes[i]),
                            object,
                            translations.as_mutable_span().slice(node_vert_offsets[i]),
                            current_positions.as_span().slice(node_vert_offsets[i]));
    }
  });
}
/** \} */

}  // namespace relax_cc

void do_relax_face_sets_brush(const Sculpt &sd, Object &object, Span<bke::pbvh::Node *> nodes)
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
            sd, brush, object, nodes, strength * strength, relax_face_sets);
        break;
      case bke::pbvh::Type::Grids:
        do_relax_face_sets_brush_grids(
            sd, brush, object, nodes, strength * strength, relax_face_sets);
        break;
      case bke::pbvh::Type::BMesh:
        do_relax_face_sets_brush_bmesh(
            sd, brush, object, nodes, strength * strength, relax_face_sets);
        break;
    }
  }
}

void do_topology_relax_brush(const Sculpt &sd, Object &object, Span<bke::pbvh::Node *> nodes)
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
        do_topology_relax_brush_mesh(sd, brush, object, nodes, strength);
        break;
      case bke::pbvh::Type::Grids:
        do_topology_relax_brush_grids(sd, brush, object, nodes, strength);
        break;
      case bke::pbvh::Type::BMesh:
        do_topology_relax_brush_bmesh(sd, brush, object, nodes, strength);
        break;
    }
  }
}
}  // namespace blender::ed::sculpt_paint
