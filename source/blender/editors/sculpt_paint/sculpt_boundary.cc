/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_colortools.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include <cmath>
#include <cstdlib>

namespace blender::ed::sculpt_paint::boundary {

static bool check_counts(const int neighbor_count, const int boundary_vertex_count)
{
  /* Corners are ambiguous as it can't be decide which boundary should be active. The flood fill
   * should also stop at corners. */
  if (neighbor_count <= 2) {
    return false;
  }

  /* Non manifold geometry in the mesh boundary.
   * The deformation result will be unpredictable and not very useful. */
  if (boundary_vertex_count > 2) {
    return false;
  }

  return true;
}

/**
 * This function is used to check where the propagation should stop when calculating the boundary,
 * as well as to check if the initial vertex is valid.
 */
static bool is_vert_in_editable_boundary_mesh(const OffsetIndices<int> faces,
                                              const Span<int> corner_verts,
                                              const GroupedSpan<int> vert_to_face,
                                              const Span<bool> hide_vert,
                                              const Span<bool> hide_poly,
                                              const BitSpan boundary,
                                              const int initial_vert)
{
  if (!hide_vert.is_empty() && hide_vert[initial_vert]) {
    return false;
  }

  int neighbor_count = 0;
  int boundary_vertex_count = 0;

  Vector<int> neighbors;
  for (const int neighbor : vert_neighbors_get_mesh(
           initial_vert, faces, corner_verts, vert_to_face, hide_poly, neighbors))
  {
    if (hide_vert.is_empty() || !hide_vert[neighbor]) {
      neighbor_count++;
      if (boundary::vert_is_boundary(hide_poly, vert_to_face, boundary, neighbor)) {
        boundary_vertex_count++;
      }
    }
  }

  return check_counts(neighbor_count, boundary_vertex_count);
}

static bool is_vert_in_editable_boundary_grids(const OffsetIndices<int> faces,
                                               const Span<int> corner_verts,
                                               const SubdivCCG &subdiv_ccg,
                                               const Span<bool> hide_poly,
                                               const BitSpan boundary,
                                               const SubdivCCGCoord initial_vert)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  if (!grid_hidden.is_empty() && grid_hidden[initial_vert.grid_index][initial_vert.to_index(key)])
  {
    return false;
  }

  SubdivCCGNeighbors neighbors;
  BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, initial_vert, false, neighbors);

  int neighbor_count = 0;
  int boundary_vertex_count = 0;
  for (const SubdivCCGCoord neighbor : neighbors.coords) {
    if (grid_hidden.is_empty() || !grid_hidden[neighbor.grid_index][neighbor.to_index(key)]) {
      neighbor_count++;
      if (boundary::vert_is_boundary(
              subdiv_ccg, hide_poly, corner_verts, faces, boundary, neighbor))
      {
        boundary_vertex_count++;
      }
    }
  }

  return check_counts(neighbor_count, boundary_vertex_count);
}

static bool is_vert_in_editable_boundary_bmesh(BMVert &initial_vert)
{
  if (BM_elem_flag_test(&initial_vert, BM_ELEM_HIDDEN)) {
    return false;
  }

  int neighbor_count = 0;
  int boundary_vertex_count = 0;

  Vector<BMVert *, 64> neighbors;
  for (BMVert *neighbor : vert_neighbors_get_bmesh(initial_vert, neighbors)) {
    if (!BM_elem_flag_test(neighbor, BM_ELEM_HIDDEN)) {
      neighbor_count++;
      if (boundary::vert_is_boundary(neighbor)) {
        boundary_vertex_count++;
      }
    }
  }

  return check_counts(neighbor_count, boundary_vertex_count);
}

/* -------------------------------------------------------------------- */
/** \name Nearest Boundary Vert
 * \{ */
/**
 * From a vertex index anywhere in the mesh, returns the closest vertex in a mesh boundary inside
 * the given radius, if it exists.
 */
static std::optional<int> get_closest_boundary_vert_mesh(Object &object,
                                                         const GroupedSpan<int> vert_to_face,
                                                         const Span<float3> vert_positions,
                                                         const Span<bool> hide_vert,
                                                         const Span<bool> hide_poly,
                                                         const BitSpan boundary,
                                                         const int initial_vert,
                                                         const float radius)
{
  if (boundary::vert_is_boundary(hide_poly, vert_to_face, boundary, initial_vert)) {
    return initial_vert;
  }

  flood_fill::FillDataMesh flood_fill(vert_positions.size());
  flood_fill.add_initial(initial_vert);

  const float3 initial_vert_position = vert_positions[initial_vert];
  const float radius_sq = radius * radius;

  std::optional<int> boundary_initial_vert;
  int boundary_initial_vert_steps = std::numeric_limits<int>::max();
  Array<int> floodfill_steps(vert_positions.size(), 0);

  flood_fill.execute(object, vert_to_face, [&](int from_v, int to_v) {
    if (!hide_vert.is_empty() && hide_vert[from_v]) {
      return false;
    }

    floodfill_steps[to_v] = floodfill_steps[from_v] + 1;

    if (boundary::vert_is_boundary(hide_poly, vert_to_face, boundary, to_v)) {
      if (floodfill_steps[to_v] < boundary_initial_vert_steps) {
        boundary_initial_vert_steps = floodfill_steps[to_v];
        boundary_initial_vert = to_v;
      }
    }

    const float len_sq = math::distance_squared(initial_vert_position, vert_positions[to_v]);
    return len_sq < radius_sq;
  });

  return boundary_initial_vert;
}

static std::optional<SubdivCCGCoord> get_closest_boundary_vert_grids(
    Object &object,
    const OffsetIndices<int> faces,
    const Span<int> corner_verts,
    const SubdivCCG &subdiv_ccg,
    const Span<bool> hide_poly,
    const BitSpan boundary,
    const SubdivCCGCoord initial_vert,
    const float radius)
{
  if (boundary::vert_is_boundary(
          subdiv_ccg, hide_poly, corner_verts, faces, boundary, initial_vert))
  {
    return initial_vert;
  }

  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int num_grids = key.grid_area * grids.size();

  flood_fill::FillDataGrids flood_fill(num_grids);
  flood_fill.add_initial(initial_vert);

  const float3 initial_vert_position = CCG_grid_elem_co(
      key, grids[initial_vert.grid_index], initial_vert.x, initial_vert.y);
  const float radius_sq = radius * radius;

  int boundary_initial_vert_steps = std::numeric_limits<int>::max();
  Array<int> floodfill_steps(num_grids, 0);
  std::optional<SubdivCCGCoord> boundary_initial_vert;

  flood_fill.execute(
      object, subdiv_ccg, [&](SubdivCCGCoord from_v, SubdivCCGCoord to_v, bool is_duplicate) {
        const int to_v_index = to_v.to_index(key);
        const int from_v_index = from_v.to_index(key);

        if (!subdiv_ccg.grid_hidden.is_empty()) {
          return false;
        }

        if (is_duplicate) {
          floodfill_steps[to_v_index] = floodfill_steps[from_v_index];
        }
        else {
          floodfill_steps[to_v_index] = floodfill_steps[from_v_index] + 1;
        }

        if (boundary::vert_is_boundary(subdiv_ccg, hide_poly, corner_verts, faces, boundary, to_v))
        {
          if (floodfill_steps[to_v_index] < boundary_initial_vert_steps) {
            boundary_initial_vert_steps = floodfill_steps[to_v_index];
            boundary_initial_vert = to_v;
          }
        }

        const float len_sq = math::distance_squared(
            initial_vert_position, CCG_grid_elem_co(key, grids[to_v.grid_index], to_v.x, to_v.y));
        return len_sq < radius_sq;
      });

  return boundary_initial_vert;
}

static std::optional<BMVert *> get_closest_boundary_vert_bmesh(Object &object,
                                                               BMesh *bm,
                                                               BMVert &initial_vert,
                                                               const float radius)
{
  if (boundary::vert_is_boundary(&initial_vert)) {
    return &initial_vert;
  }

  const int num_verts = BM_mesh_elem_count(bm, BM_VERT);
  flood_fill::FillDataBMesh flood_fill(num_verts);
  flood_fill.add_initial(&initial_vert);

  const float3 initial_vert_position = initial_vert.co;
  const float radius_sq = radius * radius;

  int boundary_initial_vert_steps = std::numeric_limits<int>::max();
  Array<int> floodfill_steps(num_verts, 0);
  std::optional<BMVert *> boundary_initial_vert;

  flood_fill.execute(object, [&](BMVert *from_v, BMVert *to_v) {
    const int from_v_i = BM_elem_index_get(from_v);
    const int to_v_i = BM_elem_index_get(to_v);

    if (BM_elem_flag_test(to_v, BM_ELEM_HIDDEN)) {
      return false;
    }

    floodfill_steps[to_v_i] = floodfill_steps[from_v_i] + 1;

    if (boundary::vert_is_boundary(to_v)) {
      if (floodfill_steps[to_v_i] < boundary_initial_vert_steps) {
        boundary_initial_vert_steps = floodfill_steps[to_v_i];
        boundary_initial_vert = to_v;
      }
    }

    const float len_sq = math::distance_squared(initial_vert_position, float3(to_v->co));
    return len_sq < radius_sq;
  });

  return boundary_initial_vert;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Boundary Index Calculation
 * \{ */

/* Used to allocate the memory of the boundary index arrays. This was decided considered the most
 * common use cases for the brush deformers, taking into account how many vertices those
 * deformations usually need in the boundary. */
constexpr int BOUNDARY_INDICES_BLOCK_SIZE = 300;

static void add_index(SculptBoundary &boundary,
                      const int new_index,
                      const float distance,
                      Set<int, BOUNDARY_INDICES_BLOCK_SIZE> &included_verts)
{
  boundary.verts.append(new_index);

  boundary.distance.add(new_index, distance);
  included_verts.add(new_index);
};

/**
 * Determines the indices of a boundary.
 */
static void indices_init_mesh(Object &object,
                              const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              const GroupedSpan<int> vert_to_face,
                              const Span<bool> hide_vert,
                              const Span<bool> hide_poly,
                              const BitSpan boundary_verts,
                              const Span<float3> vert_positions,
                              const int initial_boundary_vert,
                              SculptBoundary &boundary)
{
  flood_fill::FillDataMesh flood_fill(vert_positions.size());

  Set<int, BOUNDARY_INDICES_BLOCK_SIZE> included_verts;
  add_index(boundary, initial_boundary_vert, 1.0f, included_verts);
  flood_fill.add_initial(initial_boundary_vert);

  flood_fill.execute(object, vert_to_face, [&](const int from_v, const int to_v) {
    const float3 from_v_co = vert_positions[from_v];
    const float3 to_v_co = vert_positions[to_v];

    if (!boundary::vert_is_boundary(hide_poly, vert_to_face, boundary_verts, to_v)) {
      return false;
    }
    const float edge_len = len_v3v3(from_v_co, to_v_co);
    const float distance_boundary_to_dst = boundary.distance.lookup_default(from_v, 0.0f) +
                                           edge_len;
    add_index(boundary, to_v, distance_boundary_to_dst, included_verts);
    boundary.edges.append({from_v_co, to_v_co});
    return is_vert_in_editable_boundary_mesh(
        faces, corner_verts, vert_to_face, hide_vert, hide_poly, boundary_verts, to_v);
  });
}

static void indices_init_grids(Object &object,
                               const OffsetIndices<int> faces,
                               const Span<int> corner_verts,
                               const SubdivCCG &subdiv_ccg,
                               const Span<bool> hide_poly,
                               const BitSpan boundary_verts,
                               const SubdivCCGCoord initial_vert,
                               SculptBoundary &boundary)
{
  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int num_grids = key.grid_area * grids.size();
  flood_fill::FillDataGrids flood_fill(num_grids);

  const int initial_boundary_index = initial_vert.to_index(key);
  Set<int, BOUNDARY_INDICES_BLOCK_SIZE> included_verts;
  add_index(boundary, initial_boundary_index, 0.0f, included_verts);
  flood_fill.add_initial(initial_vert);

  flood_fill.execute(
      object,
      subdiv_ccg,
      [&](const SubdivCCGCoord from_v, const SubdivCCGCoord to_v, const bool is_duplicate) {
        const int from_v_i = from_v.to_index(key);
        const int to_v_i = to_v.to_index(key);

        const float3 from_v_co = CCG_elem_offset_co(
            key,
            grids[from_v.grid_index],
            CCG_grid_xy_to_index(key.grid_size, from_v.x, from_v.y));
        const float3 to_v_co = CCG_elem_offset_co(
            key, grids[to_v.grid_index], CCG_grid_xy_to_index(key.grid_size, to_v.x, to_v.y));

        if (!boundary::vert_is_boundary(
                subdiv_ccg, hide_poly, corner_verts, faces, boundary_verts, to_v))
        {
          return false;
        }
        const float edge_len = len_v3v3(from_v_co, to_v_co);
        const float distance_boundary_to_dst = boundary.distance.lookup_default(from_v_i, 0.0f) +
                                               edge_len;
        add_index(boundary, to_v_i, distance_boundary_to_dst, included_verts);
        if (!is_duplicate) {
          boundary.edges.append({from_v_co, to_v_co});
        }
        return is_vert_in_editable_boundary_grids(
            faces, corner_verts, subdiv_ccg, hide_poly, boundary_verts, to_v);
      });
}

static void indices_init_bmesh(Object &object,
                               BMesh *bm,
                               BMVert &initial_boundary_vert,
                               SculptBoundary &boundary)
{
  const int num_verts = BM_mesh_elem_count(bm, BM_VERT);
  flood_fill::FillDataBMesh flood_fill(num_verts);

  const int initial_boundary_index = BM_elem_index_get(&initial_boundary_vert);
  Set<int, BOUNDARY_INDICES_BLOCK_SIZE> included_verts;
  add_index(boundary, initial_boundary_index, 0.0f, included_verts);
  flood_fill.add_initial(&initial_boundary_vert);

  flood_fill.execute(object, [&](BMVert *from_v, BMVert *to_v) {
    const int from_v_i = BM_elem_index_get(from_v);
    const int to_v_i = BM_elem_index_get(to_v);

    const float3 from_v_co = from_v->co;
    const float3 to_v_co = to_v->co;

    if (!boundary::vert_is_boundary(to_v)) {
      return false;
    }
    const float edge_len = len_v3v3(from_v_co, to_v_co);
    const float distance_boundary_to_dst = boundary.distance.lookup_default(from_v_i, 0.0f) +
                                           edge_len;
    add_index(boundary, to_v_i, distance_boundary_to_dst, included_verts);
    boundary.edges.append({from_v_co, to_v_co});
    return is_vert_in_editable_boundary_bmesh(*to_v);
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Data Calculation
 * \{ */

#define BOUNDARY_VERTEX_NONE -1
#define BOUNDARY_STEPS_NONE -1

/**
 * This function initializes all data needed to calculate falloffs and deformation from the
 * boundary into the mesh into a #SculptBoundaryEditInfo array. This includes how many steps are
 * needed to go from a boundary vertex to an interior vertex and which vertex of the boundary is
 * the closest one.
 */
static void edit_data_init_mesh(OffsetIndices<int> faces,
                                Span<int> corner_verts,
                                GroupedSpan<int> vert_to_face,
                                Span<float3> vert_positions,
                                Span<bool> hide_vert,
                                Span<bool> hide_poly,
                                const int initial_vert_i,
                                const float radius,
                                SculptBoundary &boundary)
{
  boundary.edit_info.original_vertex_i = Array<int>(vert_positions.size(), BOUNDARY_VERTEX_NONE);
  boundary.edit_info.propagation_steps_num = Array<int>(vert_positions.size(),
                                                        BOUNDARY_STEPS_NONE);
  boundary.edit_info.strength_factor = Array<float>(vert_positions.size(), 0.0f);

  std::queue<int> current_iteration;

  for (const int i : boundary.verts.index_range()) {
    const int vert = boundary.verts[i];
    const int index = boundary.verts[i];

    boundary.edit_info.original_vertex_i[index] = index;
    boundary.edit_info.propagation_steps_num[index] = 0;

    current_iteration.push(vert);
  }

  int propagation_steps_num = 0;
  float accum_distance = 0.0f;

  std::queue<int> next_iteration;

  while (true) {
    /* Stop adding steps to edit info. This happens when a steps is further away from the boundary
     * than the brush radius or when the entire mesh was already processed. */
    if (accum_distance > radius || current_iteration.empty()) {
      boundary.max_propagation_steps = propagation_steps_num;
      break;
    }

    while (!current_iteration.empty()) {
      const int from_v = current_iteration.front();
      current_iteration.pop();

      Vector<int> neighbors;
      for (const int neighbor : vert_neighbors_get_mesh(
               from_v, faces, corner_verts, vert_to_face, hide_poly, neighbors))
      {
        if ((!hide_vert.is_empty() && hide_vert[from_v]) ||
            boundary.edit_info.propagation_steps_num[neighbor] != BOUNDARY_STEPS_NONE)
        {
          continue;
        }

        boundary.edit_info.original_vertex_i[neighbor] =
            boundary.edit_info.original_vertex_i[from_v];

        boundary.edit_info.propagation_steps_num[neighbor] =
            boundary.edit_info.propagation_steps_num[from_v] + 1;

        next_iteration.push(neighbor);

        /* Check the distance using the vertex that was propagated from the initial vertex that
         * was used to initialize the boundary. */
        if (boundary.edit_info.original_vertex_i[from_v] == initial_vert_i) {
          boundary.pivot_position = vert_positions[neighbor];
          accum_distance += math::distance(vert_positions[from_v], boundary.pivot_position);
        }
      }
    }

    /* Copy the new vertices to the queue to be processed in the next iteration. */
    while (!next_iteration.empty()) {
      const int next_v = next_iteration.front();
      next_iteration.pop();
      current_iteration.push(next_v);
    }

    propagation_steps_num++;
  }
}

static void edit_data_init_grids(const SubdivCCG &subdiv_ccg,
                                 const int initial_vert_i,
                                 const float radius,
                                 SculptBoundary &boundary)
{
  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int num_grids = key.grid_area * grids.size();

  boundary.edit_info.original_vertex_i = Array<int>(num_grids, BOUNDARY_VERTEX_NONE);
  boundary.edit_info.propagation_steps_num = Array<int>(num_grids, BOUNDARY_STEPS_NONE);
  boundary.edit_info.strength_factor = Array<float>(num_grids, 0.0f);

  std::queue<SubdivCCGCoord> current_iteration;

  for (const int i : boundary.verts.index_range()) {
    const SubdivCCGCoord vert = SubdivCCGCoord::from_index(key, boundary.verts[i]);

    const int index = boundary.verts[i];

    boundary.edit_info.original_vertex_i[index] = index;
    boundary.edit_info.propagation_steps_num[index] = 0;

    SubdivCCGNeighbors neighbors;
    BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, vert, true, neighbors);
    for (SubdivCCGCoord neighbor : neighbors.duplicates()) {
      boundary.edit_info.original_vertex_i[neighbor.to_index(key)] = index;
    }

    current_iteration.push(vert);
  }

  int propagation_steps_num = 0;
  float accum_distance = 0.0f;

  std::queue<SubdivCCGCoord> next_iteration;

  while (true) {
    /* Stop adding steps to edit info. This happens when a steps is further away from the boundary
     * than the brush radius or when the entire mesh was already processed. */
    if (accum_distance > radius || current_iteration.empty()) {
      boundary.max_propagation_steps = propagation_steps_num;
      break;
    }

    while (!current_iteration.empty()) {
      const SubdivCCGCoord from_v = current_iteration.front();
      current_iteration.pop();

      const int from_v_i = from_v.to_index(key);

      SubdivCCGNeighbors neighbors;
      BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, from_v, true, neighbors);

      for (const SubdivCCGCoord neighbor : neighbors.duplicates()) {
        const int neighbor_idx = neighbor.to_index(key);
        const int index_in_grid = CCG_grid_xy_to_index(key.grid_size, neighbor.x, neighbor.y);

        const bool is_hidden = !subdiv_ccg.grid_hidden.is_empty() &&
                               subdiv_ccg.grid_hidden[neighbor.grid_index][index_in_grid];
        if (is_hidden ||
            boundary.edit_info.propagation_steps_num[neighbor_idx] != BOUNDARY_STEPS_NONE)
        {
          continue;
        }
        boundary.edit_info.original_vertex_i[neighbor_idx] =
            boundary.edit_info.original_vertex_i[from_v_i];

        boundary.edit_info.propagation_steps_num[neighbor_idx] =
            boundary.edit_info.propagation_steps_num[from_v_i];
      }

      for (const SubdivCCGCoord neighbor : neighbors.unique()) {
        const int neighbor_idx = neighbor.to_index(key);
        const int index_in_grid = CCG_grid_xy_to_index(key.grid_size, neighbor.x, neighbor.y);

        const bool is_hidden = !subdiv_ccg.grid_hidden.is_empty() &&
                               subdiv_ccg.grid_hidden[neighbor.grid_index][index_in_grid];
        if (is_hidden ||
            boundary.edit_info.propagation_steps_num[neighbor_idx] != BOUNDARY_STEPS_NONE)
        {
          continue;
        }
        boundary.edit_info.original_vertex_i[neighbor_idx] =
            boundary.edit_info.original_vertex_i[from_v_i];

        boundary.edit_info.propagation_steps_num[neighbor_idx] =
            boundary.edit_info.propagation_steps_num[from_v_i] + 1;

        next_iteration.push(neighbor);

        /* When copying the data to the neighbor for the next iteration, it has to be copied to
         * all its duplicates too. This is because it is not possible to know if the updated
         * neighbor or one if its uninitialized duplicates is going to come first in order to
         * copy the data in the from_v neighbor iterator. */

        SubdivCCGNeighbors neighbor_duplicates;
        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, neighbor, true, neighbor_duplicates);

        for (const SubdivCCGCoord coord : neighbor_duplicates.duplicates()) {
          const int neighbor_duplicate_index = coord.to_index(key);
          boundary.edit_info.original_vertex_i[neighbor_duplicate_index] =
              boundary.edit_info.original_vertex_i[from_v_i];
          boundary.edit_info.propagation_steps_num[neighbor_duplicate_index] =
              boundary.edit_info.propagation_steps_num[from_v_i] + 1;
        }

        /* Check the distance using the vertex that was propagated from the initial vertex that
         * was used to initialize the boundary. */
        if (boundary.edit_info.original_vertex_i[from_v_i] == initial_vert_i) {
          boundary.pivot_position = CCG_elem_offset_co(
              key, grids[neighbor.grid_index], index_in_grid);
          accum_distance += math::distance(
              CCG_grid_elem_co(key, grids[from_v.grid_index], from_v.x, from_v.y),
              boundary.pivot_position);
        }
      }
    }

    /* Copy the new vertices to the queue to be processed in the next iteration. */
    while (!next_iteration.empty()) {
      const SubdivCCGCoord next_v = next_iteration.front();
      next_iteration.pop();
      current_iteration.push(next_v);
    }

    propagation_steps_num++;
  }
}

static void edit_data_init_bmesh(BMesh *bm,
                                 const int initial_vert_i,
                                 const float radius,
                                 SculptBoundary &boundary)
{
  const int num_verts = BM_mesh_elem_count(bm, BM_VERT);

  boundary.edit_info.original_vertex_i = Array<int>(num_verts, BOUNDARY_VERTEX_NONE);
  boundary.edit_info.propagation_steps_num = Array<int>(num_verts, BOUNDARY_STEPS_NONE);
  boundary.edit_info.strength_factor = Array<float>(num_verts, 0.0f);

  std::queue<BMVert *> current_iteration;

  for (const int i : boundary.verts.index_range()) {
    const int index = boundary.verts[i];
    BMVert *vert = BM_vert_at_index(bm, index);

    boundary.edit_info.original_vertex_i[index] = index;
    boundary.edit_info.propagation_steps_num[index] = 0;

    /* This ensures that all duplicate vertices in the boundary have the same original_vertex
     * index, so the deformation for them will be the same. */
    current_iteration.push(vert);
  }

  int propagation_steps_num = 0;
  float accum_distance = 0.0f;

  std::queue<BMVert *> next_iteration;

  while (true) {
    /* Stop adding steps to edit info. This happens when a steps is further away from the boundary
     * than the brush radius or when the entire mesh was already processed. */
    if (accum_distance > radius || current_iteration.empty()) {
      boundary.max_propagation_steps = propagation_steps_num;
      break;
    }

    while (!current_iteration.empty()) {
      BMVert *from_v = current_iteration.front();
      current_iteration.pop();

      const int from_v_i = BM_elem_index_get(from_v);

      Vector<BMVert *, 64> neighbors;
      for (BMVert *neighbor : vert_neighbors_get_bmesh(*from_v, neighbors)) {
        const int neighbor_idx = BM_elem_index_get(neighbor);
        if (BM_elem_flag_test(neighbor, BM_ELEM_HIDDEN) ||
            boundary.edit_info.propagation_steps_num[neighbor_idx] != BOUNDARY_STEPS_NONE)
        {
          continue;
        }
        boundary.edit_info.original_vertex_i[neighbor_idx] =
            boundary.edit_info.original_vertex_i[from_v_i];

        boundary.edit_info.propagation_steps_num[neighbor_idx] =
            boundary.edit_info.propagation_steps_num[from_v_i] + 1;

        next_iteration.push(neighbor);

        /* Check the distance using the vertex that was propagated from the initial vertex that
         * was used to initialize the boundary. */
        if (boundary.edit_info.original_vertex_i[from_v_i] == initial_vert_i) {
          boundary.pivot_position = neighbor->co;
          accum_distance += math::distance(float3(from_v->co), boundary.pivot_position);
        }
      }
    }

    /* Copy the new vertices to the queue to be processed in the next iteration. */
    while (!next_iteration.empty()) {
      BMVert *next_v = next_iteration.front();
      next_iteration.pop();
      current_iteration.push(next_v);
    }

    propagation_steps_num++;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Specialized Initialization
 *
 * Methods for initializing specialized data inside SculptBoundary
 * \{ */

/* These functions initialize the required vectors for the desired deformation using the
 * SculptBoundaryEditInfo. They calculate the data using the vertices that have the
 * max_propagation_steps value and them this data is copied to the rest of the vertices using the
 * original vertex index. */
static void bend_data_init_mesh(const Span<float3> vert_positions,
                                const Span<float3> vert_normals,
                                SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const int num_elements = boundary.edit_info.strength_factor.size();

  boundary.bend.pivot_rotation_axis = Array<float3>(num_elements, float3(0));
  boundary.bend.pivot_positions = Array<float3>(num_elements, float3(0));

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != boundary.max_propagation_steps) {
      continue;
    }

    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];

    const float3 normal = vert_normals[i];
    const float3 dir = vert_positions[orig_vert_i] - vert_positions[i];
    boundary.bend.pivot_rotation_axis[orig_vert_i] = math::normalize(math::cross(dir, normal));
    boundary.bend.pivot_positions[orig_vert_i] = vert_positions[i];
  }

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] == BOUNDARY_STEPS_NONE) {
      continue;
    }
    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];

    boundary.bend.pivot_positions[i] = boundary.bend.pivot_positions[orig_vert_i];
    boundary.bend.pivot_rotation_axis[i] = boundary.bend.pivot_rotation_axis[orig_vert_i];
  }
}

static void bend_data_init_grids(const SubdivCCG &subdiv_ccg, SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const int num_elements = boundary.edit_info.strength_factor.size();

  const CCGKey &key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  Span<CCGElem *> grids = subdiv_ccg.grids;

  boundary.bend.pivot_rotation_axis = Array<float3>(num_elements, float3(0));
  boundary.bend.pivot_positions = Array<float3>(num_elements, float3(0));

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != boundary.max_propagation_steps) {
      continue;
    }

    const SubdivCCGCoord vert = SubdivCCGCoord::from_index(key, i);
    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];
    const SubdivCCGCoord orig_vert = SubdivCCGCoord::from_index(key, orig_vert_i);

    const float3 normal = CCG_grid_elem_no(key, grids[vert.grid_index], vert.x, vert.y);
    const float3 dir = CCG_grid_elem_co(
                           key, grids[orig_vert.grid_index], orig_vert.x, orig_vert.y) -
                       CCG_grid_elem_co(key, grids[vert.grid_index], vert.x, vert.y);
    boundary.bend.pivot_rotation_axis[orig_vert_i] = math::normalize(math::cross(dir, normal));
    boundary.bend.pivot_positions[orig_vert_i] = CCG_grid_elem_co(
        key, grids[vert.grid_index], vert.x, vert.y);
  }

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] == BOUNDARY_STEPS_NONE) {
      continue;
    }
    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];

    boundary.bend.pivot_positions[i] = boundary.bend.pivot_positions[orig_vert_i];
    boundary.bend.pivot_rotation_axis[i] = boundary.bend.pivot_rotation_axis[orig_vert_i];
  }
}

static void bend_data_init_bmesh(BMesh *bm, SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const int num_elements = boundary.edit_info.strength_factor.size();

  boundary.bend.pivot_rotation_axis = Array<float3>(num_elements, float3(0));
  boundary.bend.pivot_positions = Array<float3>(num_elements, float3(0));

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != boundary.max_propagation_steps) {
      continue;
    }

    BMVert *vert = BM_vert_at_index(bm, i);
    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];
    BMVert *orig_vert = BM_vert_at_index(bm, orig_vert_i);

    const float3 normal = vert->no;
    const float3 dir = float3(orig_vert->co) - float3(vert->co);
    boundary.bend.pivot_rotation_axis[orig_vert_i] = math::normalize(math::cross(dir, normal));
    boundary.bend.pivot_positions[boundary.edit_info.original_vertex_i[i]] = vert->co;
  }

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] == BOUNDARY_STEPS_NONE) {
      continue;
    }
    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];
    boundary.bend.pivot_positions[i] = boundary.bend.pivot_positions[orig_vert_i];
    boundary.bend.pivot_rotation_axis[i] = boundary.bend.pivot_rotation_axis[orig_vert_i];
  }
}

static void bend_data_init(const Object &object, SculptBoundary &boundary)
{
  /* TODO: This method is to assist in refactoring, it should be removed when the rest of this
   * brush is done. */
  SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *ss.pbvh;

  switch (pbvh.type()) {
    case (bke::pbvh::Type::Mesh): {
      Mesh &mesh = *static_cast<Mesh *>(object.data);

      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
      const Span<float3> vert_normals = mesh.vert_normals();

      bend_data_init_mesh(positions_eval, vert_normals, boundary);
      break;
    }
    case (bke::pbvh::Type::Grids):
      bend_data_init_grids(*ss.subdiv_ccg, boundary);
      break;

    case (bke::pbvh::Type::BMesh):
      bend_data_init_bmesh(ss.bm, boundary);
      break;
  }
}

static void slide_data_init_mesh(const Span<float3> vert_positions, SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const int num_elements = boundary.edit_info.strength_factor.size();
  boundary.slide.directions = Array<float3>(num_elements, float3(0));

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != boundary.max_propagation_steps) {
      continue;
    }
    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];
    boundary.slide.directions[orig_vert_i] = math::normalize(vert_positions[orig_vert_i] -
                                                             vert_positions[i]);
  }

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] == BOUNDARY_STEPS_NONE) {
      continue;
    }
    boundary.slide.directions[i] =
        boundary.slide.directions[boundary.edit_info.original_vertex_i[i]];
  }
}

static void slide_data_init_grids(const SubdivCCG &subdiv_ccg, SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const int num_elements = boundary.edit_info.strength_factor.size();
  const CCGKey &key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  Span<CCGElem *> grids = subdiv_ccg.grids;

  boundary.slide.directions = Array<float3>(num_elements, float3(0));

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != boundary.max_propagation_steps) {
      continue;
    }
    const SubdivCCGCoord vert = SubdivCCGCoord::from_index(key, i);
    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];
    const SubdivCCGCoord orig_vert = SubdivCCGCoord::from_index(key, orig_vert_i);

    boundary.slide.directions[orig_vert_i] = math::normalize(
        CCG_grid_elem_co(key, grids[orig_vert.grid_index], orig_vert.x, orig_vert.y) -
        CCG_grid_elem_co(key, grids[vert.grid_index], vert.x, vert.y));
  }

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] == BOUNDARY_STEPS_NONE) {
      continue;
    }
    boundary.slide.directions[i] =
        boundary.slide.directions[boundary.edit_info.original_vertex_i[i]];
  }
}

static void slide_data_init_bmesh(BMesh *bm, SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const int num_elements = boundary.edit_info.strength_factor.size();
  boundary.slide.directions = Array<float3>(num_elements, float3(0));

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != boundary.max_propagation_steps) {
      continue;
    }
    BMVert *vert = BM_vert_at_index(bm, i);
    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];
    BMVert *orig_vert = BM_vert_at_index(bm, orig_vert_i);
    boundary.slide.directions[orig_vert_i] = math::normalize(float3(orig_vert->co) -
                                                             float3(vert->co));
  }

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] == BOUNDARY_STEPS_NONE) {
      continue;
    }
    boundary.slide.directions[i] =
        boundary.slide.directions[boundary.edit_info.original_vertex_i[i]];
  }
}

static void slide_data_init(const Object &object, SculptBoundary &boundary)
{
  /* TODO: This method is to assist in refactoring, it should be removed when the rest of this
   * brush is done. */
  SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *ss.pbvh;

  switch (pbvh.type()) {
    case (bke::pbvh::Type::Mesh): {
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);

      slide_data_init_mesh(positions_eval, boundary);
      break;
    }
    case (bke::pbvh::Type::Grids):
      slide_data_init_grids(*ss.subdiv_ccg, boundary);
      break;
    case (bke::pbvh::Type::BMesh):
      slide_data_init_bmesh(ss.bm, boundary);
      break;
  }
}

static void populate_twist_data(const Span<float3> positions, SculptBoundary &boundary)
{
  boundary.twist.pivot_position = float3(0);
  for (const float3 &position : positions) {
    boundary.twist.pivot_position += position;
  }
  boundary.twist.pivot_position *= 1.0f / boundary.verts.size();
  boundary.twist.rotation_axis = math::normalize(boundary.pivot_position -
                                                 boundary.initial_vert_position);
}

static void twist_data_init_mesh(const Span<float3> vert_positions, SculptBoundary &boundary)
{
  Array<float3> positions(boundary.verts.size());
  array_utils::gather(vert_positions, boundary.verts.as_span(), positions.as_mutable_span());
  populate_twist_data(positions, boundary);
}

static void twist_data_init_grids(const SubdivCCG &subdiv_ccg, SculptBoundary &boundary)
{
  const CCGKey &key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<CCGElem *> grids = subdiv_ccg.grids;

  Array<float3> positions(boundary.verts.size());
  for (const int i : positions.index_range()) {
    const SubdivCCGCoord vert = SubdivCCGCoord::from_index(key, boundary.verts[i]);
    positions[i] = CCG_grid_elem_co(key, grids[vert.grid_index], vert.x, vert.y);
  }
  populate_twist_data(positions, boundary);
}

static void twist_data_init_bmesh(BMesh *bm, SculptBoundary &boundary)
{
  Array<float3> positions(boundary.verts.size());
  for (const int i : positions.index_range()) {
    BMVert *vert = BM_vert_at_index(bm, i);
    positions[i] = vert->co;
  }
  populate_twist_data(positions, boundary);
}

static void twist_data_init(const Object &object, SculptBoundary &boundary)
{
  /* TODO: This method is to assist in refactoring, it should be removed when the rest of this
   * brush is done. */
  SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *ss.pbvh;

  switch (pbvh.type()) {
    case (bke::pbvh::Type::Mesh): {
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);

      twist_data_init_mesh(positions_eval, boundary);
      break;
    }
    case (bke::pbvh::Type::Grids):
      twist_data_init_grids(*ss.subdiv_ccg, boundary);
      break;
    case (bke::pbvh::Type::BMesh):
      twist_data_init_bmesh(ss.bm, boundary);
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Actions
 *
 * Actual functions related to modifying vertices.
 * \{ */

static float displacement_from_grab_delta_get(SculptSession &ss, SculptBoundary &boundary)
{
  float plane[4];
  float pos[3];
  float normal[3];
  sub_v3_v3v3(normal, ss.cache->initial_location, boundary.pivot_position);
  normalize_v3(normal);
  plane_from_point_normal_v3(plane, ss.cache->initial_location, normal);
  add_v3_v3v3(pos, ss.cache->initial_location, ss.cache->grab_delta_symmetry);
  return dist_signed_to_plane_v3(pos, plane);
}

static void boundary_brush_bend_task(Object &ob, const Brush &brush, bke::pbvh::Node *node)
{
  SculptSession &ss = *ob.sculpt;
  const int symm_area = ss.cache->mirror_symmetry_pass;
  SculptBoundary &boundary = *ss.cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  const float strength = ss.cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);

  const float disp = strength * displacement_from_grab_delta_get(ss, boundary);
  float angle_factor = disp / ss.cache->radius;
  /* Angle Snapping when inverting the brush. */
  if (ss.cache->invert) {
    angle_factor = floorf(angle_factor * 10) / 10.0f;
  }
  const float angle = angle_factor * M_PI;
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (boundary.edit_info.propagation_steps_num[vd.index] == BOUNDARY_STEPS_NONE) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);
    SCULPT_orig_vert_data_update(orig_data, vd);
    if (!SCULPT_check_vertex_pivot_symmetry(orig_data.co, boundary.initial_vert_position, symm)) {
      continue;
    }

    const float mask = 1.0f - vd.mask;
    const float automask = auto_mask::factor_get(
        ss.cache->automasking.get(), ss, vd.vertex, &automask_data);
    float t_orig_co[3];
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, brush.deform_target, &vd);
    sub_v3_v3v3(t_orig_co, orig_data.co, boundary.bend.pivot_positions[vd.index]);
    rotate_v3_v3v3fl(target_co,
                     t_orig_co,
                     boundary.bend.pivot_rotation_axis[vd.index],
                     angle * boundary.edit_info.strength_factor[vd.index] * mask * automask);
    add_v3_v3(target_co, boundary.bend.pivot_positions[vd.index]);
  }
  BKE_pbvh_vertex_iter_end;
}

static void brush_slide_task(Object &ob, const Brush &brush, bke::pbvh::Node *node)
{
  SculptSession &ss = *ob.sculpt;
  const int symm_area = ss.cache->mirror_symmetry_pass;
  SculptBoundary &boundary = *ss.cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  const float strength = ss.cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);

  const float disp = displacement_from_grab_delta_get(ss, boundary);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (boundary.edit_info.propagation_steps_num[vd.index] == BOUNDARY_STEPS_NONE) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);
    SCULPT_orig_vert_data_update(orig_data, vd);
    if (!SCULPT_check_vertex_pivot_symmetry(orig_data.co, boundary.initial_vert_position, symm)) {
      continue;
    }

    const float mask = 1.0f - vd.mask;
    const float automask = auto_mask::factor_get(
        ss.cache->automasking.get(), ss, vd.vertex, &automask_data);
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, brush.deform_target, &vd);
    madd_v3_v3v3fl(target_co,
                   orig_data.co,
                   boundary.slide.directions[vd.index],
                   boundary.edit_info.strength_factor[vd.index] * disp * mask * automask *
                       strength);
  }
  BKE_pbvh_vertex_iter_end;
}

static void brush_inflate_task(Object &ob, const Brush &brush, bke::pbvh::Node *node)
{
  SculptSession &ss = *ob.sculpt;
  const int symm_area = ss.cache->mirror_symmetry_pass;
  SculptBoundary &boundary = *ss.cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  const float strength = ss.cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  const float disp = displacement_from_grab_delta_get(ss, boundary);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (boundary.edit_info.propagation_steps_num[vd.index] == BOUNDARY_STEPS_NONE) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);
    SCULPT_orig_vert_data_update(orig_data, vd);
    if (!SCULPT_check_vertex_pivot_symmetry(orig_data.co, boundary.initial_vert_position, symm)) {
      continue;
    }

    const float mask = 1.0f - vd.mask;
    const float automask = auto_mask::factor_get(
        ss.cache->automasking.get(), ss, vd.vertex, &automask_data);
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, brush.deform_target, &vd);
    madd_v3_v3v3fl(target_co,
                   orig_data.co,
                   orig_data.no,
                   boundary.edit_info.strength_factor[vd.index] * disp * mask * automask *
                       strength);
  }
  BKE_pbvh_vertex_iter_end;
}

static void brush_grab_task(Object &ob, const Brush &brush, bke::pbvh::Node *node)
{
  SculptSession &ss = *ob.sculpt;
  const int symm_area = ss.cache->mirror_symmetry_pass;
  SculptBoundary &boundary = *ss.cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  const float strength = ss.cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (boundary.edit_info.propagation_steps_num[vd.index] == BOUNDARY_STEPS_NONE) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);
    SCULPT_orig_vert_data_update(orig_data, vd);
    if (!SCULPT_check_vertex_pivot_symmetry(orig_data.co, boundary.initial_vert_position, symm)) {
      continue;
    }

    const float mask = 1.0f - vd.mask;
    const float automask = auto_mask::factor_get(
        ss.cache->automasking.get(), ss, vd.vertex, &automask_data);
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, brush.deform_target, &vd);
    madd_v3_v3v3fl(target_co,
                   orig_data.co,
                   ss.cache->grab_delta_symmetry,
                   boundary.edit_info.strength_factor[vd.index] * mask * automask * strength);
  }
  BKE_pbvh_vertex_iter_end;
}

static void brush_twist_task(Object &ob, const Brush &brush, bke::pbvh::Node *node)
{
  SculptSession &ss = *ob.sculpt;
  const int symm_area = ss.cache->mirror_symmetry_pass;
  SculptBoundary &boundary = *ss.cache->boundaries[symm_area];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  const float strength = ss.cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, ss.cache->automasking.get(), *node);

  const float disp = strength * displacement_from_grab_delta_get(ss, boundary);
  float angle_factor = disp / ss.cache->radius;
  /* Angle Snapping when inverting the brush. */
  if (ss.cache->invert) {
    angle_factor = floorf(angle_factor * 10) / 10.0f;
  }
  const float angle = angle_factor * M_PI;

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (boundary.edit_info.propagation_steps_num[vd.index] == BOUNDARY_STEPS_NONE) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);
    SCULPT_orig_vert_data_update(orig_data, vd);
    if (!SCULPT_check_vertex_pivot_symmetry(orig_data.co, boundary.initial_vert_position, symm)) {
      continue;
    }

    const float mask = 1.0f - vd.mask;
    const float automask = auto_mask::factor_get(
        ss.cache->automasking.get(), ss, vd.vertex, &automask_data);
    float t_orig_co[3];
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, brush.deform_target, &vd);
    sub_v3_v3v3(t_orig_co, orig_data.co, boundary.twist.pivot_position);
    rotate_v3_v3v3fl(target_co,
                     t_orig_co,
                     boundary.twist.rotation_axis,
                     angle * mask * automask * boundary.edit_info.strength_factor[vd.index]);
    add_v3_v3(target_co, boundary.twist.pivot_position);
  }
  BKE_pbvh_vertex_iter_end;
}

static void brush_smooth_task(Object &ob, const Brush &brush, bke::pbvh::Node *node)
{
  SculptSession &ss = *ob.sculpt;
  const int symmetry_pass = ss.cache->mirror_symmetry_pass;
  const SculptBoundary &boundary = *ss.cache->boundaries[symmetry_pass];
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  const float strength = ss.cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data = SCULPT_orig_vert_data_init(ob, *node, undo::Type::Position);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (boundary.edit_info.propagation_steps_num[vd.index] == BOUNDARY_STEPS_NONE) {
      continue;
    }

    SCULPT_orig_vert_data_update(orig_data, vd);
    if (!SCULPT_check_vertex_pivot_symmetry(orig_data.co, boundary.initial_vert_position, symm)) {
      continue;
    }

    float coord_accum[3] = {0.0f, 0.0f, 0.0f};
    int total_neighbors = 0;
    const int current_propagation_steps = boundary.edit_info.propagation_steps_num[vd.index];
    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      if (current_propagation_steps == boundary.edit_info.propagation_steps_num[ni.index]) {
        add_v3_v3(coord_accum, SCULPT_vertex_co_get(ss, ni.vertex));
        total_neighbors++;
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    if (total_neighbors == 0) {
      continue;
    }
    float disp[3];
    float avg[3];
    const float mask = 1.0f - vd.mask;
    mul_v3_v3fl(avg, coord_accum, 1.0f / total_neighbors);
    sub_v3_v3v3(disp, avg, vd.co);
    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, brush.deform_target, &vd);
    madd_v3_v3v3fl(
        target_co, vd.co, disp, boundary.edit_info.strength_factor[vd.index] * mask * strength);
  }
  BKE_pbvh_vertex_iter_end;
}

/* This functions assigns a falloff factor to each one of the SculptBoundaryEditInfo structs
 * based on the brush curve and its propagation steps. The falloff goes from the boundary into
 * the mesh.
 */
static void init_falloff(const Brush &brush, const float radius, SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const int num_elements = boundary.edit_info.strength_factor.size();

  BKE_curvemapping_init(brush.curve);

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != BOUNDARY_STEPS_NONE) {
      boundary.edit_info.strength_factor[i] = BKE_brush_curve_strength(
          &brush, boundary.edit_info.propagation_steps_num[i], boundary.max_propagation_steps);
    }

    if (boundary.edit_info.original_vertex_i[i] == boundary.initial_vert_i) {
      /* All vertices that are propagated from the original vertex won't be affected by the
       * boundary falloff, so there is no need to calculate anything else. */
      continue;
    }

    const bool use_boundary_distances = brush.boundary_falloff_type !=
                                        BRUSH_BOUNDARY_FALLOFF_CONSTANT;

    if (!use_boundary_distances) {
      /* There are falloff modes that do not require to modify the previously calculated falloff
       * based on boundary distances. */
      continue;
    }

    const float boundary_distance = boundary.distance.lookup_default(
        boundary.edit_info.original_vertex_i[i], 0.0f);
    float falloff_distance = 0.0f;
    float direction = 1.0f;

    switch (brush.boundary_falloff_type) {
      case BRUSH_BOUNDARY_FALLOFF_RADIUS:
        falloff_distance = boundary_distance;
        break;
      case BRUSH_BOUNDARY_FALLOFF_LOOP: {
        const int div = boundary_distance / radius;
        const float mod = fmodf(boundary_distance, radius);
        falloff_distance = div % 2 == 0 ? mod : radius - mod;
        break;
      }
      case BRUSH_BOUNDARY_FALLOFF_LOOP_INVERT: {
        const int div = boundary_distance / radius;
        const float mod = fmodf(boundary_distance, radius);
        falloff_distance = div % 2 == 0 ? mod : radius - mod;
        /* Inverts the falloff in the intervals 1 2 5 6 9 10 ... etc. */
        if (((div - 1) & 2) == 0) {
          direction = -1.0f;
        }
        break;
      }
      case BRUSH_BOUNDARY_FALLOFF_CONSTANT:
        /* For constant falloff distances are not allocated, so this should never happen. */
        BLI_assert(false);
    }

    boundary.edit_info.strength_factor[i] *= direction * BKE_brush_curve_strength(
                                                             &brush, falloff_distance, radius);
  }
}

void do_boundary_brush(const Sculpt &sd, Object &ob, Span<bke::pbvh::Node *> nodes)
{
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  const ePaintSymmetryFlags symm_area = ss.cache->mirror_symmetry_pass;
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {

    PBVHVertRef initial_vert;

    if (ss.cache->mirror_symmetry_pass == 0) {
      initial_vert = SCULPT_active_vertex_get(ss);
    }
    else {
      float location[3];
      flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), symm_area);
      initial_vert = nearest_vert_calc(ob, location, ss.cache->radius_squared, false);
    }

    ss.cache->boundaries[symm_area] = data_init(
        ob, &brush, initial_vert, ss.cache->initial_radius);

    if (ss.cache->boundaries[symm_area]) {
      switch (brush.boundary_deform_type) {
        case BRUSH_BOUNDARY_DEFORM_BEND:
          bend_data_init(ob, *ss.cache->boundaries[symm_area]);
          break;
        case BRUSH_BOUNDARY_DEFORM_EXPAND:
          slide_data_init(ob, *ss.cache->boundaries[symm_area]);
          break;
        case BRUSH_BOUNDARY_DEFORM_TWIST:
          twist_data_init(ob, *ss.cache->boundaries[symm_area]);
          break;
        case BRUSH_BOUNDARY_DEFORM_INFLATE:
        case BRUSH_BOUNDARY_DEFORM_GRAB:
          /* Do nothing. These deform modes don't need any extra data to be precomputed. */
          break;
      }

      init_falloff(brush, ss.cache->initial_radius, *ss.cache->boundaries[symm_area]);
    }
  }

  /* No active boundary under the cursor. */
  if (!ss.cache->boundaries[symm_area]) {
    return;
  }

  switch (brush.boundary_deform_type) {
    case BRUSH_BOUNDARY_DEFORM_BEND:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          boundary_brush_bend_task(ob, brush, nodes[i]);
        }
      });
      break;
    case BRUSH_BOUNDARY_DEFORM_EXPAND:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          brush_slide_task(ob, brush, nodes[i]);
        }
      });
      break;
    case BRUSH_BOUNDARY_DEFORM_INFLATE:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          brush_inflate_task(ob, brush, nodes[i]);
        }
      });
      break;
    case BRUSH_BOUNDARY_DEFORM_GRAB:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          brush_grab_task(ob, brush, nodes[i]);
        }
      });
      break;
    case BRUSH_BOUNDARY_DEFORM_TWIST:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          brush_twist_task(ob, brush, nodes[i]);
        }
      });
      break;
    case BRUSH_BOUNDARY_DEFORM_SMOOTH:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          brush_smooth_task(ob, brush, nodes[i]);
        }
      });
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

std::unique_ptr<SculptBoundary> data_init(Object &object,
                                          const Brush *brush,
                                          const PBVHVertRef initial_vert,
                                          const float radius)
{
  /* TODO: Temporary bridge method to help in refactoring, this method should be deprecated
   * entirely. */
  const SculptSession &ss = *object.sculpt;
  if (initial_vert.i == PBVH_REF_NONE) {
    return nullptr;
  }

  switch (ss.pbvh->type()) {
    case (bke::pbvh::Type::Mesh): {
      const int vert = initial_vert.i;
      return data_init_mesh(object, brush, vert, radius);
    }
    case (bke::pbvh::Type::Grids): {
      const CCGKey &key = BKE_subdiv_ccg_key_top_level(*ss.subdiv_ccg);
      const SubdivCCGCoord vert = SubdivCCGCoord::from_index(key, initial_vert.i);
      return data_init_grids(object, brush, vert, radius);
    }
    case (bke::pbvh::Type::BMesh): {
      BMVert *vert = reinterpret_cast<BMVert *>(initial_vert.i);
      return data_init_bmesh(object, brush, vert, radius);
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

std::unique_ptr<SculptBoundary> data_init_mesh(Object &object,
                                               const Brush *brush,
                                               const int initial_vert,
                                               const float radius)
{
  SculptSession &ss = *object.sculpt;

  boundary::ensure_boundary_info(object);

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

  const bke::pbvh::Tree &pbvh = *ss.pbvh;

  const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);

  const std::optional<int> boundary_initial_vert = get_closest_boundary_vert_mesh(
      object,
      ss.vert_to_face_map,
      positions_eval,
      hide_vert,
      hide_poly,
      ss.vertex_info.boundary,
      initial_vert,
      radius);

  if (!boundary_initial_vert) {
    return nullptr;
  }

  /* Starting from a vertex that is the limit of a boundary is ambiguous, so return nullptr instead
   * of forcing a random active boundary from a corner. */
  /* TODO: Investigate whether initial_vert should actually be boundary_initial_vert. If
   * initial_vert is correct, the above comment and the doc-string for the relevant function should
   * be fixed. */
  if (!is_vert_in_editable_boundary_mesh(faces,
                                         corner_verts,
                                         ss.vert_to_face_map,
                                         hide_vert,
                                         hide_poly,
                                         ss.vertex_info.boundary,
                                         initial_vert))
  {
    return nullptr;
  }

  std::unique_ptr<SculptBoundary> boundary = std::make_unique<SculptBoundary>();
  *boundary = {};

  const int boundary_initial_vert_index = *boundary_initial_vert;
  boundary->initial_vert_i = boundary_initial_vert_index;
  boundary->initial_vert_position = positions_eval[boundary_initial_vert_index];

  indices_init_mesh(object,
                    faces,
                    corner_verts,
                    ss.vert_to_face_map,
                    hide_vert,
                    hide_poly,
                    ss.vertex_info.boundary,
                    positions_eval,
                    *boundary_initial_vert,
                    *boundary);

  const float boundary_radius = brush ? radius * (1.0f + brush->boundary_offset) : radius;
  edit_data_init_mesh(faces,
                      corner_verts,
                      ss.vert_to_face_map,
                      positions_eval,
                      hide_vert,
                      hide_poly,
                      boundary_initial_vert_index,
                      boundary_radius,
                      *boundary);

  return boundary;
}

std::unique_ptr<SculptBoundary> data_init_grids(Object &object,
                                                const Brush *brush,
                                                const SubdivCCGCoord initial_vert,
                                                const float radius)
{
  SculptSession &ss = *object.sculpt;

  boundary::ensure_boundary_info(object);

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const CCGKey &key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const std::optional<SubdivCCGCoord> boundary_initial_vert = get_closest_boundary_vert_grids(
      object,
      faces,
      corner_verts,
      subdiv_ccg,
      hide_poly,
      ss.vertex_info.boundary,
      initial_vert,
      radius);

  if (!boundary_initial_vert) {
    return nullptr;
  }

  /* Starting from a vertex that is the limit of a boundary is ambiguous, so return nullptr instead
   * of forcing a random active boundary from a corner. */
  if (!is_vert_in_editable_boundary_grids(
          faces, corner_verts, subdiv_ccg, hide_poly, ss.vertex_info.boundary, initial_vert))
  {
    return nullptr;
  }

  std::unique_ptr<SculptBoundary> boundary = std::make_unique<SculptBoundary>();
  *boundary = {};

  SubdivCCGCoord boundary_vert = *boundary_initial_vert;
  const int boundary_initial_vert_index = boundary_vert.to_index(key);
  boundary->initial_vert_i = boundary_initial_vert_index;
  boundary->initial_vert_position = CCG_grid_elem_co(
      key, grids[boundary_vert.grid_index], boundary_vert.x, boundary_vert.y);

  indices_init_grids(object,
                     faces,
                     corner_verts,
                     subdiv_ccg,
                     hide_poly,
                     ss.vertex_info.boundary,
                     boundary_vert,
                     *boundary);

  const float boundary_radius = brush ? radius * (1.0f + brush->boundary_offset) : radius;
  edit_data_init_grids(subdiv_ccg, boundary_initial_vert_index, boundary_radius, *boundary);

  return boundary;
}

std::unique_ptr<SculptBoundary> data_init_bmesh(Object &object,
                                                const Brush *brush,
                                                BMVert *initial_vert,
                                                const float radius)
{
  SculptSession &ss = *object.sculpt;

  SCULPT_vertex_random_access_ensure(ss);
  boundary::ensure_boundary_info(object);

  const std::optional<BMVert *> boundary_initial_vert = get_closest_boundary_vert_bmesh(
      object, ss.bm, *initial_vert, radius);

  if (!boundary_initial_vert) {
    return nullptr;
  }

  /* Starting from a vertex that is the limit of a boundary is ambiguous, so return nullptr instead
   * of forcing a random active boundary from a corner. */
  if (!is_vert_in_editable_boundary_bmesh(*initial_vert)) {
    return nullptr;
  }

  std::unique_ptr<SculptBoundary> boundary = std::make_unique<SculptBoundary>();
  *boundary = {};

  const int boundary_initial_vert_index = BM_elem_index_get(*boundary_initial_vert);
  boundary->initial_vert_i = boundary_initial_vert_index;
  boundary->initial_vert_position = (*boundary_initial_vert)->co;

  indices_init_bmesh(object, ss.bm, **boundary_initial_vert, *boundary);

  const float boundary_radius = brush ? radius * (1.0f + brush->boundary_offset) : radius;
  edit_data_init_bmesh(ss.bm, boundary_initial_vert_index, boundary_radius, *boundary);

  return boundary;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Boundary Drawing
 *
 * Helper methods to draw boundary information.
 * \{ */

std::unique_ptr<SculptBoundaryPreview> preview_data_init(Object &object,
                                                         const Brush *brush,
                                                         const PBVHVertRef initial_vert,
                                                         const float radius)
{
  std::unique_ptr<SculptBoundary> boundary = data_init(object, brush, initial_vert, radius);
  if (boundary == nullptr) {
    return nullptr;
  }
  std::unique_ptr<SculptBoundaryPreview> preview = std::make_unique<SculptBoundaryPreview>();
  preview->edges = boundary->edges;
  preview->pivot_position = boundary->pivot_position;
  preview->initial_vert_position = boundary->initial_vert_position;

  return preview;
}

void edges_preview_draw(const uint gpuattr,
                        SculptSession &ss,
                        const float outline_col[3],
                        const float outline_alpha)
{
  if (!ss.boundary_preview) {
    return;
  }
  immUniformColor3fvAlpha(outline_col, outline_alpha);
  GPU_line_width(2.0f);
  immBegin(GPU_PRIM_LINES, ss.boundary_preview->edges.size() * 2);
  for (const int i : ss.boundary_preview->edges.index_range()) {
    immVertex3fv(gpuattr, ss.boundary_preview->edges[i].first);
    immVertex3fv(gpuattr, ss.boundary_preview->edges[i].second);
  }
  immEnd();
}

void pivot_line_preview_draw(const uint gpuattr, SculptSession &ss)
{
  if (!ss.boundary_preview) {
    return;
  }
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  GPU_line_width(2.0f);
  immBegin(GPU_PRIM_LINES, 2);
  immVertex3fv(gpuattr, ss.boundary_preview->pivot_position);
  immVertex3fv(gpuattr, ss.boundary_preview->initial_vert_position);
  immEnd();
}

/** \} */

}  // namespace blender::ed::sculpt_paint::boundary
