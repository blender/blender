/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "sculpt_smooth.hh"

#include "MEM_guardedalloc.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_base.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"

#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "mesh_brush_common.hh"
#include "sculpt_automask.hh"
#include "sculpt_color.hh"
#include "sculpt_face_set.hh"
#include "sculpt_hide.hh"
#include "sculpt_intern.hh"

#include "bmesh.hh"

#include <cstdlib>

namespace blender::ed::sculpt_paint::smooth {

template<typename T> T calc_average(const Span<T> positions, const Span<int> indices)
{
  const float factor = math::rcp(float(indices.size()));
  T result{};
  for (const int i : indices) {
    result += positions[i] * factor;
  }
  return result;
}

template<typename T>
void neighbor_data_average_mesh_check_loose(const Span<T> src,
                                            const Span<int> verts,
                                            const Span<Vector<int>> vert_neighbors,
                                            const MutableSpan<T> dst)
{
  BLI_assert(verts.size() == dst.size());
  BLI_assert(vert_neighbors.size() == dst.size());

  for (const int i : vert_neighbors.index_range()) {
    const Span<int> neighbors = vert_neighbors[i];
    if (neighbors.is_empty()) {
      dst[i] = src[verts[i]];
    }
    else {
      dst[i] = calc_average(src, neighbors);
    }
  }
}

template void neighbor_data_average_mesh_check_loose<float>(Span<float>,
                                                            Span<int>,
                                                            Span<Vector<int>>,
                                                            MutableSpan<float>);
template void neighbor_data_average_mesh_check_loose<float3>(Span<float3>,
                                                             Span<int>,
                                                             Span<Vector<int>>,
                                                             MutableSpan<float3>);

template<typename T>
void neighbor_data_average_mesh(const Span<T> src,
                                const Span<Vector<int>> vert_neighbors,
                                const MutableSpan<T> dst)
{
  BLI_assert(vert_neighbors.size() == dst.size());

  for (const int i : vert_neighbors.index_range()) {
    BLI_assert(!vert_neighbors[i].is_empty());
    dst[i] = calc_average(src, vert_neighbors[i]);
  }
}

template void neighbor_data_average_mesh<float>(Span<float>,
                                                Span<Vector<int>>,
                                                MutableSpan<float>);
template void neighbor_data_average_mesh<float3>(Span<float3>,
                                                 Span<Vector<int>>,
                                                 MutableSpan<float3>);
template void neighbor_data_average_mesh<float4>(Span<float4>,
                                                 Span<Vector<int>>,
                                                 MutableSpan<float4>);

static float3 average_positions(const CCGKey &key,
                                const Span<float3> positions,
                                const Span<SubdivCCGCoord> coords)
{
  const float factor = math::rcp(float(coords.size()));
  float3 result(0);
  for (const SubdivCCGCoord coord : coords) {
    result += positions[coord.to_index(key)] * factor;
  }
  return result;
}

void neighbor_position_average_interior_grids(const OffsetIndices<int> faces,
                                              const Span<int> corner_verts,
                                              const BitSpan boundary_verts,
                                              const SubdivCCG &subdiv_ccg,
                                              const Span<int> grids,
                                              const MutableSpan<float3> new_positions)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float3> positions = subdiv_ccg.positions;

  BLI_assert(grids.size() * key.grid_area == new_positions.size());

  for (const int i : grids.index_range()) {
    const int node_verts_start = i * key.grid_area;
    const int grid = grids[i];
    const IndexRange grid_range = bke::ccg::grid_range(key, grid);

    /* TODO: This loop could be optimized in the future by skipping unnecessary logic for
     * non-boundary grid vertices. */
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert_index = node_verts_start + offset;
        const int vert = grid_range[offset];

        SubdivCCGCoord coord{};
        coord.grid_index = grid;
        coord.x = x;
        coord.y = y;

        SubdivCCGNeighbors neighbors;
        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);

        if (BKE_subdiv_ccg_coord_is_mesh_boundary(
                faces, corner_verts, boundary_verts, subdiv_ccg, coord))
        {
          if (neighbors.coords.size() == 2) {
            /* Do not include neighbors of corner vertices. */
            neighbors.coords.clear();
          }
          else {
            /* Only include other boundary vertices as neighbors of boundary vertices. */
            neighbors.coords.remove_if([&](const SubdivCCGCoord coord) {
              return !BKE_subdiv_ccg_coord_is_mesh_boundary(
                  faces, corner_verts, boundary_verts, subdiv_ccg, coord);
            });
          }
        }

        if (neighbors.coords.is_empty()) {
          new_positions[node_vert_index] = positions[vert];
        }
        else {
          new_positions[node_vert_index] = average_positions(key, positions, neighbors.coords);
        }
      }
    }
  }
}

template<typename T>
void average_data_grids(const SubdivCCG &subdiv_ccg,
                        const Span<T> src,
                        const Span<int> grids,
                        const MutableSpan<T> dst)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  BLI_assert(grids.size() * key.grid_area == dst.size());

  for (const int i : grids.index_range()) {
    const int grid = grids[i];
    const int node_verts_start = i * key.grid_area;

    /* TODO: This loop could be optimized in the future by skipping unnecessary logic for
     * non-boundary grid vertices. */
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert_index = node_verts_start + offset;

        SubdivCCGCoord coord{};
        coord.grid_index = grid;
        coord.x = x;
        coord.y = y;

        SubdivCCGNeighbors neighbors;
        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);

        T sum{};
        for (const SubdivCCGCoord neighbor : neighbors.coords) {
          const int index = neighbor.grid_index * key.grid_area +
                            CCG_grid_xy_to_index(key.grid_size, neighbor.x, neighbor.y);
          sum += src[index];
        }
        dst[node_vert_index] = sum / neighbors.coords.size();
      }
    }
  }
}

template<typename T>
void average_data_bmesh(const Span<T> src, const Set<BMVert *, 0> &verts, const MutableSpan<T> dst)
{
  Vector<BMVert *, 64> neighbor_data;

  int i = 0;
  for (BMVert *vert : verts) {
    T sum{};
    const Span<BMVert *> neighbors = vert_neighbors_get_bmesh(*vert, neighbor_data);
    for (const BMVert *neighbor : neighbors) {
      sum += src[BM_elem_index_get(neighbor)];
    }
    dst[i] = sum / neighbors.size();
    i++;
  }
}

template void average_data_grids<float>(const SubdivCCG &,
                                        Span<float>,
                                        Span<int>,
                                        MutableSpan<float>);
template void average_data_grids<float3>(const SubdivCCG &,
                                         Span<float3>,
                                         Span<int>,
                                         MutableSpan<float3>);
template void average_data_bmesh<float>(Span<float> src,
                                        const Set<BMVert *, 0> &,
                                        MutableSpan<float>);
template void average_data_bmesh<float3>(Span<float3> src,
                                         const Set<BMVert *, 0> &,
                                         MutableSpan<float3>);

static float3 average_positions(const Span<const BMVert *> verts)
{
  const float factor = math::rcp(float(verts.size()));
  float3 result(0);
  for (const BMVert *vert : verts) {
    result += float3(vert->co) * factor;
  }
  return result;
}

void neighbor_position_average_bmesh(const Set<BMVert *, 0> &verts,
                                     const MutableSpan<float3> new_positions)
{
  BLI_assert(verts.size() == new_positions.size());
  Vector<BMVert *, 64> neighbor_data;

  int i = 0;
  for (BMVert *vert : verts) {
    const Span<BMVert *> neighbors = vert_neighbors_get_bmesh(*vert, neighbor_data);
    new_positions[i] = average_positions(neighbors);
    i++;
  }
}

void neighbor_position_average_interior_bmesh(const Set<BMVert *, 0> &verts,
                                              const MutableSpan<float3> new_positions)
{
  BLI_assert(verts.size() == new_positions.size());
  Vector<BMVert *, 64> neighbor_data;

  int i = 0;
  for (BMVert *vert : verts) {
    const Span<BMVert *> neighbors = vert_neighbors_get_interior_bmesh(*vert, neighbor_data);
    if (neighbors.is_empty()) {
      new_positions[i] = float3(vert->co);
    }
    else {
      new_positions[i] = average_positions(neighbors);
    }
    i++;
  }
}

void bmesh_four_neighbor_average(float avg[3], const float3 &direction, const BMVert *v)
{
  float avg_co[3] = {0.0f, 0.0f, 0.0f};
  float tot_co = 0.0f;

  BMIter eiter;
  BMEdge *e;

  BM_ITER_ELEM (e, &eiter, const_cast<BMVert *>(v), BM_EDGES_OF_VERT) {
    if (BM_edge_is_boundary(e)) {
      copy_v3_v3(avg, v->co);
      return;
    }
    BMVert *v_other = (e->v1 == v) ? e->v2 : e->v1;
    float vec[3];
    sub_v3_v3v3(vec, v_other->co, v->co);
    madd_v3_v3fl(vec, v->no, -dot_v3v3(vec, v->no));
    normalize_v3(vec);

    /* fac is a measure of how orthogonal or parallel the edge is
     * relative to the direction. */
    float fac = dot_v3v3(vec, direction);
    fac = fac * fac - 0.5f;
    fac *= fac;
    madd_v3_v3fl(avg_co, v_other->co, fac);
    tot_co += fac;
  }

  /* In case vert has no Edge s. */
  if (tot_co > 0.0f) {
    mul_v3_v3fl(avg, avg_co, 1.0f / tot_co);

    /* Preserve volume. */
    float vec[3];
    sub_v3_v3(avg, v->co);
    mul_v3_v3fl(vec, v->no, dot_v3v3(avg, v->no));
    sub_v3_v3(avg, vec);
    add_v3_v3(avg, v->co);
  }
  else {
    zero_v3(avg);
  }
}

void neighbor_color_average(const OffsetIndices<int> faces,
                            const Span<int> corner_verts,
                            const GroupedSpan<int> vert_to_face_map,
                            const GSpan color_attribute,
                            const bke::AttrDomain color_domain,
                            const Span<Vector<int>> vert_neighbors,
                            const MutableSpan<float4> smooth_colors)
{
  BLI_assert(vert_neighbors.size() == smooth_colors.size());

  for (const int i : vert_neighbors.index_range()) {
    float4 sum(0);
    const Span<int> neighbors = vert_neighbors[i];
    for (const int vert : neighbors) {
      sum += color::color_vert_get(
          faces, corner_verts, vert_to_face_map, color_attribute, color_domain, vert);
    }
    smooth_colors[i] = sum / neighbors.size();
  }
}

/* HC Smooth Algorithm. */
/* From: Improved Laplacian Smoothing of Noisy Surface Meshes */

void surface_smooth_laplacian_step(const Span<float3> positions,
                                   const Span<float3> orig_positions,
                                   const Span<float3> average_positions,
                                   const float alpha,
                                   MutableSpan<float3> laplacian_disp,
                                   MutableSpan<float3> translations)
{
  BLI_assert(positions.size() == orig_positions.size());
  BLI_assert(positions.size() == average_positions.size());
  BLI_assert(positions.size() == laplacian_disp.size());
  BLI_assert(positions.size() == translations.size());

  for (const int i : average_positions.index_range()) {
    const float3 weighted_o = orig_positions[i] * alpha;
    const float3 weighted_q = positions[i] * (1.0f - alpha);
    const float3 d = weighted_o + weighted_q;
    laplacian_disp[i] = average_positions[i] - d;
    translations[i] = average_positions[i] - positions[i];
  }
}

void surface_smooth_displace_step(const Span<float3> laplacian_disp,
                                  const Span<float3> average_laplacian_disp,
                                  const float beta,
                                  const MutableSpan<float3> translations)
{
  BLI_assert(laplacian_disp.size() == average_laplacian_disp.size());
  BLI_assert(laplacian_disp.size() == translations.size());

  for (const int i : laplacian_disp.index_range()) {
    float3 b_current_vert = average_laplacian_disp[i] * (1.0f - beta);
    b_current_vert += laplacian_disp[i] * beta;
    translations[i] = -b_current_vert;
  }
}

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

static float3 calc_boundary_normal_corner(const float3 &current_position,
                                          const Span<float3> vert_positions,
                                          const Span<int> neighbors)
{
  float3 normal(0);
  for (const int vert : neighbors) {
    const float3 to_neighbor = vert_positions[vert] - current_position;
    normal += math::normalize(to_neighbor);
  }
  return math::normalize(normal);
}

static float3 calc_boundary_normal_corner(const CCGKey &key,
                                          const Span<float3> positions,
                                          const float3 &current_position,
                                          const Span<SubdivCCGCoord> neighbors)
{
  float3 normal(0);
  for (const SubdivCCGCoord &coord : neighbors) {
    const float3 to_neighbor = positions[coord.to_index(key)] - current_position;
    normal += math::normalize(to_neighbor);
  }
  return math::normalize(normal);
}

static float3 calc_boundary_normal_corner(const float3 &current_position,
                                          const Span<BMVert *> neighbors)
{
  float3 normal(0);
  for (BMVert *vert : neighbors) {
    const float3 neighbor_pos = vert->co;
    const float3 to_neighbor = neighbor_pos - current_position;
    normal += math::normalize(to_neighbor);
  }
  return math::normalize(normal);
}

void calc_relaxed_translations_faces(const Span<float3> vert_positions,
                                     const Span<float3> vert_normals,
                                     const OffsetIndices<int> faces,
                                     const Span<int> corner_verts,
                                     const GroupedSpan<int> vert_to_face_map,
                                     const BitSpan boundary_verts,
                                     const Span<int> face_sets,
                                     const Span<bool> hide_poly,
                                     const bool filter_boundary_face_sets,
                                     const Span<int> verts,
                                     const Span<float> factors,
                                     Vector<Vector<int>> &neighbors,
                                     const MutableSpan<float3> translations)
{
  BLI_assert(verts.size() == factors.size());
  BLI_assert(verts.size() == translations.size());

  neighbors.resize(verts.size());
  calc_vert_neighbors(faces, corner_verts, vert_to_face_map, hide_poly, verts, neighbors);

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

    const float3 smoothed_position = calc_average(vert_positions, neighbors[i]);

    /* Normal Calculation */
    float3 normal;
    if (is_boundary && neighbors[i].size() == 2) {
      normal = calc_boundary_normal_corner(vert_positions[verts[i]], vert_positions, neighbors[i]);
      if (math::is_zero(normal)) {
        translations[i] = float3(0);
        continue;
      }
    }
    else {
      normal = vert_normals[verts[i]];
    }

    const float3 translation = translation_to_plane(
        vert_positions[verts[i]], normal, smoothed_position);

    translations[i] = translation * factors[i];
  }
}

void calc_relaxed_translations_grids(const SubdivCCG &subdiv_ccg,
                                     const OffsetIndices<int> faces,
                                     const Span<int> corner_verts,
                                     const Span<int> face_sets,
                                     const GroupedSpan<int> vert_to_face_map,
                                     const BitSpan boundary_verts,
                                     const Span<int> grids,
                                     const bool filter_boundary_face_sets,
                                     const Span<float> factors,
                                     Vector<Vector<SubdivCCGCoord>> &neighbors,
                                     const MutableSpan<float3> translations)
{
  const Span<float3> positions = subdiv_ccg.positions;
  const Span<float3> normals = subdiv_ccg.normals;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const int grid_verts_num = grids.size() * key.grid_area;
  BLI_assert(grid_verts_num == translations.size());
  BLI_assert(grid_verts_num == factors.size());

  neighbors.resize(grid_verts_num);
  calc_vert_neighbors(subdiv_ccg, grids, neighbors);

  for (const int i : grids.index_range()) {
    const IndexRange grid_range = bke::ccg::grid_range(key, grids[i]);
    const int node_start = i * key.grid_area;
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert = node_start + offset;
        const int vert = grid_range[offset];
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
                faces, corner_verts, vert_to_face_map, face_sets, subdiv_ccg, neighbor);
          });
        }

        if (neighbors[i].is_empty()) {
          translations[node_vert] = float3(0);
          continue;
        }

        const float3 smoothed_position = average_positions(key, positions, neighbors[node_vert]);

        /* Normal Calculation */
        float3 normal;
        if (is_boundary && neighbors[i].size() == 2) {
          normal = calc_boundary_normal_corner(
              key, positions, positions[vert], neighbors[node_vert]);
          if (math::is_zero(normal)) {
            translations[node_vert] = float3(0);
            continue;
          }
        }
        else {
          normal = normals[vert];
        }

        const float3 translation = translation_to_plane(
            positions[vert], normal, smoothed_position);

        translations[node_vert] = translation * factors[node_vert];
      }
    }
  }
}

void calc_relaxed_translations_bmesh(const Set<BMVert *, 0> &verts,
                                     const Span<float3> positions,
                                     const int face_set_offset,
                                     const bool filter_boundary_face_sets,
                                     const Span<float> factors,
                                     Vector<Vector<BMVert *>> &neighbors,
                                     const MutableSpan<float3> translations)
{
  BLI_assert(verts.size() == factors.size());
  BLI_assert(verts.size() == translations.size());

  neighbors.resize(verts.size());
  calc_vert_neighbors(verts, neighbors);

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
      neighbors[i].remove_if([&](const BMVert *vert) {
        return face_set::vert_has_unique_face_set(face_set_offset, *vert);
      });
    }

    if (neighbors[i].is_empty()) {
      translations[i] = float3(0);
      i++;
      continue;
    }

    const float3 smoothed_position = average_positions(neighbors[i]);

    /* Normal Calculation */
    float3 normal;
    if (is_boundary && neighbors[i].size() == 2) {
      normal = calc_boundary_normal_corner(positions[i], neighbors[i]);
      if (math::is_zero(normal)) {
        translations[i] = float3(0);
        i++;
        continue;
      }
    }
    else {
      normal = vert->no;
    }

    const float3 translation = translation_to_plane(positions[i], normal, smoothed_position);

    translations[i] = translation * factors[i];
    i++;
  }
}

void blur_geometry_data_array(const Object &object,
                              const int iterations,
                              const MutableSpan<float> data)
{
  struct LocalData {
    Vector<int> vert_indices;
    Vector<Vector<int>> vert_neighbors;
    Vector<float> new_factors;
  };
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert",
                                                                  bke::AttrDomain::Point);
      const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly",
                                                                  bke::AttrDomain::Face);
      for ([[maybe_unused]] const int _ : IndexRange(iterations)) {
        node_mask.foreach_index(GrainSize(1), [&](const int i) {
          LocalData &tls = all_tls.local();
          const Span<int> verts = hide::node_visible_verts(nodes[i], hide_vert, tls.vert_indices);

          tls.vert_neighbors.resize(verts.size());
          const MutableSpan<Vector<int>> neighbors = tls.vert_neighbors;
          calc_vert_neighbors(faces, corner_verts, vert_to_face_map, hide_poly, verts, neighbors);

          tls.new_factors.resize(verts.size());
          const MutableSpan<float> new_factors = tls.new_factors;
          smooth::neighbor_data_average_mesh(data.as_span(), neighbors, new_factors);

          scatter_data_mesh(new_factors.as_span(), verts, data);
        });
      }
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
      for ([[maybe_unused]] const int _ : IndexRange(iterations)) {
        node_mask.foreach_index(GrainSize(1), [&](const int node_index) {
          LocalData &tls = all_tls.local();
          const Span<int> grids = nodes[node_index].grids();
          const int grid_verts_num = key.grid_area * grids.size();

          tls.new_factors.resize(grid_verts_num);
          const MutableSpan<float> new_factors = tls.new_factors;
          smooth::average_data_grids(subdiv_ccg, data.as_span(), grids, new_factors);

          if (grid_hidden.is_empty()) {
            scatter_data_grids(subdiv_ccg, new_factors.as_span(), grids, data);
          }
          else {
            for (const int i : grids.index_range()) {
              const int node_start = i * key.grid_area;
              BKE_subdiv_ccg_foreach_visible_grid_vert(
                  key, grid_hidden, grids[i], [&](const int offset) {
                    data[i] = new_factors[node_start + offset];
                  });
            }
          }
        });
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      for ([[maybe_unused]] const int _ : IndexRange(iterations)) {
        node_mask.foreach_index(GrainSize(1), [&](const int node_index) {
          LocalData &tls = all_tls.local();
          const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(
              const_cast<bke::pbvh::BMeshNode *>(&nodes[node_index]));

          tls.new_factors.resize(verts.size());
          const MutableSpan<float> new_factors = tls.new_factors;
          smooth::average_data_bmesh(data.as_span(), verts, new_factors);

          int i = 0;
          for (const BMVert *vert : verts) {
            if (BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
              i++;
              continue;
            }
            data[BM_elem_index_get(vert)] = new_factors[i];
            i++;
          }
        });
      }
      break;
    }
  }
}

}  // namespace blender::ed::sculpt_paint::smooth
