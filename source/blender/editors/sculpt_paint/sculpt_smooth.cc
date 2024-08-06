/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_base.hh"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"

#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "sculpt_intern.hh"

#include "bmesh.hh"

#include <cmath>
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
                                const Span<CCGElem *> elems,
                                const Span<SubdivCCGCoord> coords)
{
  const float factor = math::rcp(float(coords.size()));
  float3 result(0);
  for (const SubdivCCGCoord coord : coords) {
    result += CCG_grid_elem_co(key, elems[coord.grid_index], coord.x, coord.y) * factor;
  }
  return result;
}

void neighbor_position_average_grids(const SubdivCCG &subdiv_ccg,
                                     const Span<int> grids,
                                     const MutableSpan<float3> new_positions)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<CCGElem *> elems = subdiv_ccg.grids;

  BLI_assert(grids.size() * key.grid_area == new_positions.size());

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

        new_positions[node_vert_index] = average_positions(key, elems, neighbors.coords);
      }
    }
  }
}

void neighbor_position_average_interior_grids(const OffsetIndices<int> faces,
                                              const Span<int> corner_verts,
                                              const BitSpan boundary_verts,
                                              const SubdivCCG &subdiv_ccg,
                                              const Span<int> grids,
                                              const MutableSpan<float3> new_positions)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<CCGElem *> elems = subdiv_ccg.grids;

  BLI_assert(grids.size() * key.grid_area == new_positions.size());

  for (const int i : grids.index_range()) {
    const int grid = grids[i];
    CCGElem *elem = elems[grid];
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
          new_positions[node_vert_index] = CCG_elem_offset_co(key, elem, offset);
        }
        else {
          new_positions[node_vert_index] = average_positions(key, elems, neighbors.coords);
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

  BLI_assert(grids.size() * key.grid_area == src.size());

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
    neighbor_data.clear();
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
    neighbor_data.clear();
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
    neighbor_data.clear();
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

/* Generic functions for laplacian smoothing. These functions do not take boundary vertices into
 * account. */

float3 neighbor_coords_average(SculptSession &ss, PBVHVertRef vertex)
{
  float3 avg(0);
  int total = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    avg += SCULPT_vertex_co_get(ss, ni.vertex);
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (total > 0) {
    return avg / total;
  }
  return SCULPT_vertex_co_get(ss, vertex);
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

void surface_smooth_laplacian_step(SculptSession &ss,
                                   float *disp,
                                   const float co[3],
                                   MutableSpan<float3> laplacian_disp,
                                   const PBVHVertRef vertex,
                                   const float origco[3],
                                   const float alpha)
{
  float weigthed_o[3], weigthed_q[3], d[3];
  int v_index = BKE_pbvh_vertex_to_index(*ss.pbvh, vertex);

  const float3 laplacian_smooth_co = neighbor_coords_average(ss, vertex);

  mul_v3_v3fl(weigthed_o, origco, alpha);
  mul_v3_v3fl(weigthed_q, co, 1.0f - alpha);
  add_v3_v3v3(d, weigthed_o, weigthed_q);
  sub_v3_v3v3(laplacian_disp[v_index], laplacian_smooth_co, d);

  sub_v3_v3v3(disp, laplacian_smooth_co, co);
}

void surface_smooth_displace_step(SculptSession &ss,
                                  float *co,
                                  MutableSpan<float3> laplacian_disp,
                                  const PBVHVertRef vertex,
                                  const float beta,
                                  const float fade)
{
  float b_avg[3] = {0.0f, 0.0f, 0.0f};
  float b_current_vertex[3];
  int total = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    add_v3_v3(b_avg, laplacian_disp[ni.index]);
    total++;
  }

  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  if (total > 0) {
    int v_index = BKE_pbvh_vertex_to_index(*ss.pbvh, vertex);

    mul_v3_v3fl(b_current_vertex, b_avg, (1.0f - beta) / total);
    madd_v3_v3fl(b_current_vertex, laplacian_disp[v_index], beta);
    mul_v3_fl(b_current_vertex, clamp_f(fade, 0.0f, 1.0f));
    sub_v3_v3(co, b_current_vertex);
  }
}

}  // namespace blender::ed::sculpt_paint::smooth
