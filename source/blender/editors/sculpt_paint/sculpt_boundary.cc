/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "sculpt_boundary.hh"

#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_rotation_legacy.hh"
#include "BLI_math_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_colortools.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"

#include "mesh_brush_common.hh"
#include "paint_intern.hh"
#include "sculpt_automask.hh"
#include "sculpt_cloth.hh"
#include "sculpt_flood_fill.hh"
#include "sculpt_intern.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "bmesh.hh"

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
           faces, corner_verts, vert_to_face, hide_poly, initial_vert, neighbors))
  {
    if (hide_vert.is_empty() || !hide_vert[neighbor]) {
      neighbor_count++;
      if (boundary::vert_is_boundary(vert_to_face, hide_poly, boundary, neighbor)) {
        boundary_vertex_count++;
      }
    }
  }

  return check_counts(neighbor_count, boundary_vertex_count);
}

static bool is_vert_in_editable_boundary_grids(const OffsetIndices<int> faces,
                                               const Span<int> corner_verts,
                                               const SubdivCCG &subdiv_ccg,
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
      if (boundary::vert_is_boundary(faces, corner_verts, boundary, subdiv_ccg, neighbor)) {
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

  BMeshNeighborVerts neighbors;
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
  if (boundary::vert_is_boundary(vert_to_face, hide_poly, boundary, initial_vert)) {
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

    if (boundary::vert_is_boundary(vert_to_face, hide_poly, boundary, to_v)) {
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
    const BitSpan boundary,
    const SubdivCCGCoord initial_vert,
    const float radius)
{
  if (boundary::vert_is_boundary(faces, corner_verts, boundary, subdiv_ccg, initial_vert)) {
    return initial_vert;
  }

  const Span<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  flood_fill::FillDataGrids flood_fill(positions.size());
  flood_fill.add_initial(initial_vert);

  const float3 initial_vert_position = positions[initial_vert.to_index(key)];
  const float radius_sq = radius * radius;

  int boundary_initial_vert_steps = std::numeric_limits<int>::max();
  Array<int> floodfill_steps(positions.size(), 0);
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

        if (boundary::vert_is_boundary(faces, corner_verts, boundary, subdiv_ccg, to_v)) {
          if (floodfill_steps[to_v_index] < boundary_initial_vert_steps) {
            boundary_initial_vert_steps = floodfill_steps[to_v_index];
            boundary_initial_vert = to_v;
          }
        }

        const float len_sq = math::distance_squared(initial_vert_position,
                                                    positions[to_v.to_index(key)]);
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
  add_index(boundary, initial_boundary_vert, 0.0f, included_verts);
  flood_fill.add_initial(initial_boundary_vert);

  flood_fill.execute(object, vert_to_face, [&](const int from_v, const int to_v) {
    const float3 from_v_co = vert_positions[from_v];
    const float3 to_v_co = vert_positions[to_v];

    if (!boundary::vert_is_boundary(vert_to_face, hide_poly, boundary_verts, to_v)) {
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
                               const BitSpan boundary_verts,
                               const SubdivCCGCoord initial_vert,
                               SculptBoundary &boundary)
{
  const Span<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  flood_fill::FillDataGrids flood_fill(positions.size());

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

        const float3 &from_v_co = positions[from_v_i];
        const float3 &to_v_co = positions[to_v_i];

        if (!boundary::vert_is_boundary(faces, corner_verts, boundary_verts, subdiv_ccg, to_v)) {
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
            faces, corner_verts, subdiv_ccg, boundary_verts, to_v);
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
               faces, corner_verts, vert_to_face, hide_poly, from_v, neighbors))
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
  const Span<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  boundary.edit_info.original_vertex_i = Array<int>(positions.size(), BOUNDARY_VERTEX_NONE);
  boundary.edit_info.propagation_steps_num = Array<int>(positions.size(), BOUNDARY_STEPS_NONE);
  boundary.edit_info.strength_factor = Array<float>(positions.size(), 0.0f);

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
          boundary.pivot_position = positions[neighbor_idx];
          accum_distance += math::distance(positions[from_v_i], boundary.pivot_position);
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

      BMeshNeighborVerts neighbors;
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

  const Span<float3> positions = subdiv_ccg.positions;
  const Span<float3> normals = subdiv_ccg.normals;

  boundary.bend.pivot_rotation_axis = Array<float3>(num_elements, float3(0));
  boundary.bend.pivot_positions = Array<float3>(num_elements, float3(0));

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != boundary.max_propagation_steps) {
      continue;
    }

    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];

    const float3 normal = normals[i];
    const float3 dir = positions[orig_vert_i] - positions[i];
    boundary.bend.pivot_rotation_axis[orig_vert_i] = math::normalize(math::cross(dir, normal));
    boundary.bend.pivot_positions[orig_vert_i] = positions[i];
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
  const Span<float3> positions = subdiv_ccg.positions;

  boundary.slide.directions = Array<float3>(num_elements, float3(0));

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != boundary.max_propagation_steps) {
      continue;
    }
    const int orig_vert_i = boundary.edit_info.original_vertex_i[i];

    boundary.slide.directions[orig_vert_i] = math::normalize(positions[orig_vert_i] -
                                                             positions[i]);
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
  const Span<float3> vert_positions = subdiv_ccg.positions;
  Array<float3> positions(boundary.verts.size());
  array_utils::gather(vert_positions, boundary.verts.as_span(), positions.as_mutable_span());
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common helpers
 * \{ */

BLI_NOINLINE static void filter_uninitialized_verts(const Span<int> propagation_steps,
                                                    const MutableSpan<float> factors)
{
  BLI_assert(propagation_steps.size() == factors.size());

  for (const int i : factors.index_range()) {
    if (propagation_steps[i] == BOUNDARY_STEPS_NONE) {
      factors[i] = 0.0f;
    }
  }
}

struct LocalDataMesh {
  Vector<float3> positions;

  Vector<float> factors;
  Vector<int> propagation_steps;

  /* TODO: std::variant? */
  /* Bend */
  Vector<float3> pivot_positions;
  Vector<float3> pivot_axes;

  /* Slide */
  Vector<float3> slide_directions;

  /* Smooth */
  Vector<int> neighbor_offsets;
  Vector<int> neighbor_data;
  Vector<float3> average_positions;

  Vector<float3> new_positions;
  Vector<float3> translations;
};

struct LocalDataGrids {
  Vector<float3> positions;

  Vector<float> factors;
  Vector<int> propagation_steps;

  /* TODO: std::variant? */
  /* Bend */
  Vector<float3> pivot_positions;
  Vector<float3> pivot_axes;

  /* Slide */
  Vector<float3> slide_directions;

  /* Smooth */
  Vector<int> neighbor_offsets;
  Vector<int> neighbor_data;
  Vector<float3> average_positions;

  Vector<float3> new_positions;
  Vector<float3> translations;
};

struct LocalDataBMesh {
  Vector<float3> positions;

  Vector<float> factors;
  Vector<int> propagation_steps;

  /* TODO: std::variant? */
  /* Bend */
  Vector<float3> pivot_positions;
  Vector<float3> pivot_axes;

  /* Slide */
  Vector<float3> slide_directions;

  /* Smooth */
  Vector<int> neighbor_offsets;
  Vector<BMVert *> neighbor_data;
  Vector<float3> average_positions;

  Vector<float3> new_positions;
  Vector<float3> translations;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bend Deformation
 * \{ */

BLI_NOINLINE static void calc_bend_position(const Span<float3> positions,
                                            const Span<float3> pivot_positions,
                                            const Span<float3> pivot_axes,
                                            const Span<float> factors,
                                            const MutableSpan<float3> new_positions)
{
  BLI_assert(positions.size() == pivot_positions.size());
  BLI_assert(positions.size() == pivot_axes.size());
  BLI_assert(positions.size() == factors.size());
  BLI_assert(positions.size() == new_positions.size());

  for (const int i : positions.index_range()) {
    float3 from_pivot_to_pos = positions[i] - pivot_positions[i];
    float3 rotated;
    rotate_v3_v3v3fl(rotated, from_pivot_to_pos, pivot_axes[i], factors[i]);
    new_positions[i] = rotated + pivot_positions[i];
  }
}

static void calc_bend_mesh(const Depsgraph &depsgraph,
                           const Sculpt &sd,
                           Object &object,
                           const Span<int> vert_propagation_steps,
                           const Span<float> vert_factors,
                           const Span<float3> vert_pivot_positions,
                           const Span<float3> vert_pivot_axes,
                           const bke::pbvh::MeshNode &node,
                           LocalDataMesh &tls,
                           const float3 symmetry_pivot,
                           const float strength,
                           const eBrushDeformTarget deform_target,
                           const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();
  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_mesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_mesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  const Span<float3> pivot_positions = gather_data_mesh(
      vert_pivot_positions, verts, tls.pivot_positions);
  const Span<float3> pivot_axes = gather_data_mesh(vert_pivot_axes, verts, tls.pivot_axes);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_bend_position(orig_data.positions, pivot_positions, pivot_axes, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, verts, position_data.eval, translations);
      clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
      position_data.deform(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_mesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_bend_grids(const Depsgraph &depsgraph,
                            const Sculpt &sd,
                            Object &object,
                            SubdivCCG &subdiv_ccg,
                            const Span<int> vert_propagation_steps,
                            const Span<float> vert_factors,
                            const Span<float3> vert_pivot_positions,
                            const Span<float3> vert_pivot_axes,
                            const bke::pbvh::GridsNode &node,
                            LocalDataGrids &tls,
                            const float3 symmetry_pivot,
                            const float strength,
                            const eBrushDeformTarget deform_target)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_grids(
      subdiv_ccg, vert_factors, grids, tls.factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  const Span<int> propagation_steps = gather_data_grids(
      subdiv_ccg, vert_propagation_steps, grids, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  const Span<float3> pivot_positions = gather_data_grids(
      subdiv_ccg, vert_pivot_positions, grids, tls.pivot_positions);
  const Span<float3> pivot_axes = gather_data_grids(
      subdiv_ccg, vert_pivot_axes, grids, tls.pivot_axes);

  tls.new_positions.resize(grid_verts_num);
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_bend_position(orig_data.positions, pivot_positions, pivot_axes, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(grid_verts_num);
      const MutableSpan<float3> translations = tls.translations;
      const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_data.positions, translations);
      apply_translations(translations, grids, subdiv_ccg);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_grids(subdiv_ccg,
                         new_positions.as_span(),
                         grids,
                         cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_bend_bmesh(const Depsgraph &depsgraph,
                            const Sculpt &sd,
                            Object &object,
                            const Span<int> vert_propagation_steps,
                            const Span<float> vert_factors,
                            const Span<float3> vert_pivot_positions,
                            const Span<float3> vert_pivot_axes,
                            bke::pbvh::BMeshNode &node,
                            LocalDataBMesh &tls,
                            const float3 symmetry_pivot,
                            const float strength,
                            const eBrushDeformTarget deform_target)

{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_bmesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_bmesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  const Span<float3> pivot_positions = gather_data_bmesh(
      vert_pivot_positions, verts, tls.pivot_positions);
  const Span<float3> pivot_axes = gather_data_bmesh(vert_pivot_axes, verts, tls.pivot_axes);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_bend_position(orig_positions, pivot_positions, pivot_axes, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      const MutableSpan<float3> positions = gather_bmesh_positions(verts, tls.positions);
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_positions, translations);
      apply_translations(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_bmesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void do_bend_brush(const Depsgraph &depsgraph,
                          const Sculpt &sd,
                          Object &object,
                          const IndexMask &node_mask,
                          const SculptBoundary &boundary,
                          const float strength,
                          const eBrushDeformTarget deform_target)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const PositionDeformData position_data(depsgraph, object);

      threading::EnumerableThreadSpecific<LocalDataMesh> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataMesh &tls = all_tls.local();
        calc_bend_mesh(depsgraph,
                       sd,
                       object,
                       boundary.edit_info.propagation_steps_num,
                       boundary.edit_info.strength_factor,
                       boundary.bend.pivot_positions,
                       boundary.bend.pivot_rotation_axis,
                       nodes[i],
                       tls,
                       boundary.initial_vert_position,
                       strength,
                       deform_target,
                       position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      threading::EnumerableThreadSpecific<LocalDataGrids> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataGrids &tls = all_tls.local();
        calc_bend_grids(depsgraph,
                        sd,
                        object,
                        subdiv_ccg,
                        boundary.edit_info.propagation_steps_num,
                        boundary.edit_info.strength_factor,
                        boundary.bend.pivot_positions,
                        boundary.bend.pivot_rotation_axis,
                        nodes[i],
                        tls,
                        boundary.initial_vert_position,
                        strength,
                        deform_target);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      threading::EnumerableThreadSpecific<LocalDataBMesh> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataBMesh &tls = all_tls.local();
        calc_bend_bmesh(depsgraph,
                        sd,
                        object,
                        boundary.edit_info.propagation_steps_num,
                        boundary.edit_info.strength_factor,
                        boundary.bend.pivot_positions,
                        boundary.bend.pivot_rotation_axis,
                        nodes[i],
                        tls,
                        boundary.initial_vert_position,
                        strength,
                        deform_target);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Slide Deformation
 * \{ */

BLI_NOINLINE static void calc_slide_position(const Span<float3> positions,
                                             const Span<float3> directions,
                                             const Span<float> factors,
                                             const MutableSpan<float3> new_positions)
{
  BLI_assert(positions.size() == directions.size());
  BLI_assert(positions.size() == factors.size());
  BLI_assert(positions.size() == new_positions.size());

  for (const int i : positions.index_range()) {
    new_positions[i] = positions[i] + (directions[i] * factors[i]);
  }
}

static void calc_slide_mesh(const Depsgraph &depsgraph,
                            const Sculpt &sd,
                            Object &object,
                            const Span<int> vert_propagation_steps,
                            const Span<float> vert_factors,
                            const Span<float3> vert_slide_directions,
                            const bke::pbvh::MeshNode &node,
                            LocalDataMesh &tls,
                            const float3 symmetry_pivot,
                            const float strength,
                            const eBrushDeformTarget deform_target,
                            const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();
  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_mesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_mesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;

  const Span<float3> slide_directions = gather_data_mesh(
      vert_slide_directions, verts, tls.slide_directions);

  calc_slide_position(orig_data.positions, slide_directions, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, verts, position_data.eval, translations);
      clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
      position_data.deform(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_mesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_slide_grids(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             SubdivCCG &subdiv_ccg,
                             const Span<int> vert_propagation_steps,
                             const Span<float> vert_factors,
                             const Span<float3> vert_slide_directions,
                             const bke::pbvh::GridsNode &node,
                             LocalDataGrids &tls,
                             const float3 symmetry_pivot,
                             const float strength,
                             const eBrushDeformTarget deform_target)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_grids(
      subdiv_ccg, vert_factors, grids, tls.factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  const Span<int> propagation_steps = gather_data_grids(
      subdiv_ccg, vert_propagation_steps, grids, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(grid_verts_num);
  const MutableSpan<float3> new_positions = tls.new_positions;

  const Span<float3> slide_directions = gather_data_grids(
      subdiv_ccg, vert_slide_directions, grids, tls.pivot_positions);

  calc_slide_position(orig_data.positions, slide_directions, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(grid_verts_num);
      const MutableSpan<float3> translations = tls.translations;
      const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_data.positions, translations);
      apply_translations(translations, grids, subdiv_ccg);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_grids(subdiv_ccg,
                         new_positions.as_span(),
                         grids,
                         cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_slide_bmesh(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const Span<int> vert_propagation_steps,
                             const Span<float> vert_factors,
                             const Span<float3> vert_slide_directions,
                             bke::pbvh::BMeshNode &node,
                             LocalDataBMesh &tls,
                             const float3 symmetry_pivot,
                             const float strength,
                             const eBrushDeformTarget deform_target)

{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_bmesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_bmesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  const Span<float3> slide_directions = gather_data_bmesh(
      vert_slide_directions, verts, tls.pivot_positions);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_slide_position(orig_positions, slide_directions, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      const MutableSpan<float3> positions = gather_bmesh_positions(verts, tls.positions);
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_positions, translations);
      apply_translations(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_bmesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void do_slide_brush(const Depsgraph &depsgraph,
                           const Sculpt &sd,
                           Object &object,
                           const IndexMask &node_mask,
                           const SculptBoundary &boundary,
                           const float strength,
                           const eBrushDeformTarget deform_target)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const PositionDeformData position_data(depsgraph, object);

      threading::EnumerableThreadSpecific<LocalDataMesh> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataMesh &tls = all_tls.local();
        calc_slide_mesh(depsgraph,
                        sd,
                        object,
                        boundary.edit_info.propagation_steps_num,
                        boundary.edit_info.strength_factor,
                        boundary.slide.directions,
                        nodes[i],
                        tls,
                        boundary.initial_vert_position,
                        strength,
                        deform_target,
                        position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      threading::EnumerableThreadSpecific<LocalDataGrids> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataGrids &tls = all_tls.local();
        calc_slide_grids(depsgraph,
                         sd,
                         object,
                         subdiv_ccg,
                         boundary.edit_info.propagation_steps_num,
                         boundary.edit_info.strength_factor,
                         boundary.slide.directions,
                         nodes[i],
                         tls,
                         boundary.initial_vert_position,
                         strength,
                         deform_target);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      threading::EnumerableThreadSpecific<LocalDataBMesh> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataBMesh &tls = all_tls.local();
        calc_slide_bmesh(depsgraph,
                         sd,
                         object,
                         boundary.edit_info.propagation_steps_num,
                         boundary.edit_info.strength_factor,
                         boundary.slide.directions,
                         nodes[i],
                         tls,
                         boundary.initial_vert_position,
                         strength,
                         deform_target);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inflate Deformation
 * \{ */

BLI_NOINLINE static void calc_inflate_position(const Span<float3> positions,
                                               const Span<float3> normals,
                                               const Span<float> factors,
                                               const MutableSpan<float3> new_positions)
{
  BLI_assert(positions.size() == normals.size());
  BLI_assert(positions.size() == factors.size());
  BLI_assert(positions.size() == new_positions.size());

  for (const int i : positions.index_range()) {
    new_positions[i] = positions[i] + (normals[i] * factors[i]);
  }
}

static void calc_inflate_mesh(const Depsgraph &depsgraph,
                              const Sculpt &sd,
                              Object &object,
                              const Span<int> vert_propagation_steps,
                              const Span<float> vert_factors,
                              const bke::pbvh::MeshNode &node,
                              LocalDataMesh &tls,
                              const float3 symmetry_pivot,
                              const float strength,
                              const eBrushDeformTarget deform_target,
                              const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();
  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_mesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_mesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_inflate_position(orig_data.positions, orig_data.normals, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, verts, position_data.eval, translations);
      clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
      position_data.deform(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_mesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_inflate_grids(const Depsgraph &depsgraph,
                               const Sculpt &sd,
                               Object &object,
                               SubdivCCG &subdiv_ccg,
                               const Span<int> vert_propagation_steps,
                               const Span<float> vert_factors,
                               const bke::pbvh::GridsNode &node,
                               LocalDataGrids &tls,
                               const float3 symmetry_pivot,
                               const float strength,
                               const eBrushDeformTarget deform_target)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_grids(
      subdiv_ccg, vert_factors, grids, tls.factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  const Span<int> propagation_steps = gather_data_grids(
      subdiv_ccg, vert_propagation_steps, grids, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(grid_verts_num);
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_inflate_position(orig_data.positions, orig_data.normals, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(grid_verts_num);
      const MutableSpan<float3> translations = tls.translations;
      const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_data.positions, translations);
      apply_translations(translations, grids, subdiv_ccg);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_grids(subdiv_ccg,
                         new_positions.as_span(),
                         grids,
                         cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_inflate_bmesh(const Depsgraph &depsgraph,
                               const Sculpt &sd,
                               Object &object,
                               const Span<int> vert_propagation_steps,
                               const Span<float> vert_factors,
                               bke::pbvh::BMeshNode &node,
                               LocalDataBMesh &tls,
                               const float3 symmetry_pivot,
                               const float strength,
                               const eBrushDeformTarget deform_target)

{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_bmesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_bmesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_inflate_position(orig_positions, orig_normals, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      const MutableSpan<float3> positions = gather_bmesh_positions(verts, tls.positions);
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_positions, translations);
      apply_translations(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_bmesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void do_inflate_brush(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const IndexMask &node_mask,
                             const SculptBoundary &boundary,
                             const float strength,
                             const eBrushDeformTarget deform_target)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const PositionDeformData position_data(depsgraph, object);

      threading::EnumerableThreadSpecific<LocalDataMesh> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataMesh &tls = all_tls.local();
        calc_inflate_mesh(depsgraph,
                          sd,
                          object,
                          boundary.edit_info.propagation_steps_num,
                          boundary.edit_info.strength_factor,
                          nodes[i],
                          tls,
                          boundary.initial_vert_position,
                          strength,
                          deform_target,
                          position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      threading::EnumerableThreadSpecific<LocalDataGrids> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataGrids &tls = all_tls.local();
        calc_inflate_grids(depsgraph,
                           sd,
                           object,
                           subdiv_ccg,
                           boundary.edit_info.propagation_steps_num,
                           boundary.edit_info.strength_factor,
                           nodes[i],
                           tls,
                           boundary.initial_vert_position,
                           strength,
                           deform_target);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      threading::EnumerableThreadSpecific<LocalDataBMesh> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataBMesh &tls = all_tls.local();
        calc_inflate_bmesh(depsgraph,
                           sd,
                           object,
                           boundary.edit_info.propagation_steps_num,
                           boundary.edit_info.strength_factor,
                           nodes[i],
                           tls,
                           boundary.initial_vert_position,
                           strength,
                           deform_target);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grab Deformation
 * \{ */

BLI_NOINLINE static void calc_grab_position(const Span<float3> positions,
                                            const float3 grab_delta,
                                            const Span<float> factors,
                                            const MutableSpan<float3> new_positions)
{
  BLI_assert(positions.size() == factors.size());
  BLI_assert(positions.size() == new_positions.size());

  for (const int i : positions.index_range()) {
    new_positions[i] = positions[i] + (grab_delta * factors[i]);
  }
}

static void calc_grab_mesh(const Depsgraph &depsgraph,
                           const Sculpt &sd,
                           Object &object,
                           const Span<int> vert_propagation_steps,
                           const Span<float> vert_factors,
                           const bke::pbvh::MeshNode &node,
                           LocalDataMesh &tls,
                           const float3 grab_delta_symmetry,
                           const float3 symmetry_pivot,
                           const float strength,
                           const eBrushDeformTarget deform_target,
                           const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();
  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_mesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_mesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_grab_position(orig_data.positions, grab_delta_symmetry, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, verts, position_data.eval, translations);
      clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
      position_data.deform(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_mesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_grab_grids(const Depsgraph &depsgraph,
                            const Sculpt &sd,
                            Object &object,
                            SubdivCCG &subdiv_ccg,
                            const Span<int> vert_propagation_steps,
                            const Span<float> vert_factors,
                            const bke::pbvh::GridsNode &node,
                            LocalDataGrids &tls,
                            const float3 grab_delta_symmetry,
                            const float3 symmetry_pivot,
                            const float strength,
                            const eBrushDeformTarget deform_target)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_grids(
      subdiv_ccg, vert_factors, grids, tls.factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  const Span<int> propagation_steps = gather_data_grids(
      subdiv_ccg, vert_propagation_steps, grids, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(grid_verts_num);
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_grab_position(orig_data.positions, grab_delta_symmetry, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(grid_verts_num);
      const MutableSpan<float3> translations = tls.translations;
      const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_data.positions, translations);
      apply_translations(translations, grids, subdiv_ccg);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_grids(subdiv_ccg,
                         new_positions.as_span(),
                         grids,
                         cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_grab_bmesh(const Depsgraph &depsgraph,
                            const Sculpt &sd,
                            Object &object,
                            const Span<int> vert_propagation_steps,
                            const Span<float> vert_factors,
                            bke::pbvh::BMeshNode &node,
                            LocalDataBMesh &tls,
                            const float3 grab_delta_symmetry,
                            const float3 symmetry_pivot,
                            const float strength,
                            const eBrushDeformTarget deform_target)

{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_bmesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_bmesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_grab_position(orig_positions, grab_delta_symmetry, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      const MutableSpan<float3> positions = gather_bmesh_positions(verts, tls.positions);
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_positions, translations);
      apply_translations(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_bmesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void do_grab_brush(const Depsgraph &depsgraph,
                          const Sculpt &sd,
                          Object &object,
                          const IndexMask &node_mask,
                          const SculptBoundary &boundary,
                          const float strength,
                          const eBrushDeformTarget deform_target)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const PositionDeformData position_data(depsgraph, object);

      threading::EnumerableThreadSpecific<LocalDataMesh> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataMesh &tls = all_tls.local();
        calc_grab_mesh(depsgraph,
                       sd,
                       object,
                       boundary.edit_info.propagation_steps_num,
                       boundary.edit_info.strength_factor,
                       nodes[i],
                       tls,
                       ss.cache->grab_delta_symm,
                       boundary.initial_vert_position,
                       strength,
                       deform_target,
                       position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      threading::EnumerableThreadSpecific<LocalDataGrids> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataGrids &tls = all_tls.local();
        calc_grab_grids(depsgraph,
                        sd,
                        object,
                        subdiv_ccg,
                        boundary.edit_info.propagation_steps_num,
                        boundary.edit_info.strength_factor,
                        nodes[i],
                        tls,
                        ss.cache->grab_delta_symm,
                        boundary.initial_vert_position,
                        strength,
                        deform_target);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      threading::EnumerableThreadSpecific<LocalDataBMesh> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataBMesh &tls = all_tls.local();
        calc_grab_bmesh(depsgraph,
                        sd,
                        object,
                        boundary.edit_info.propagation_steps_num,
                        boundary.edit_info.strength_factor,
                        nodes[i],
                        tls,
                        ss.cache->grab_delta_symm,
                        boundary.initial_vert_position,
                        strength,
                        deform_target);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Twist Deformation
 * \{ */

BLI_NOINLINE static void calc_twist_position(const Span<float3> positions,
                                             const float3 pivot_point,
                                             const float3 pivot_axis,
                                             const Span<float> factors,
                                             const MutableSpan<float3> new_positions)
{
  BLI_assert(positions.size() == factors.size());
  BLI_assert(positions.size() == new_positions.size());

  for (const int i : positions.index_range()) {
    new_positions[i] = math::rotate_around_axis(positions[i], pivot_point, pivot_axis, factors[i]);
  }
}

static void calc_twist_mesh(const Depsgraph &depsgraph,
                            const Sculpt &sd,
                            Object &object,
                            const Span<int> vert_propagation_steps,
                            const Span<float> vert_factors,
                            const bke::pbvh::MeshNode &node,
                            LocalDataMesh &tls,
                            const float3 twist_pivot_position,
                            const float3 twist_axis,
                            const float3 symmetry_pivot,
                            const float strength,
                            const eBrushDeformTarget deform_target,
                            const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();
  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_mesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_mesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_twist_position(
      orig_data.positions, twist_pivot_position, twist_axis, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, verts, position_data.eval, translations);
      clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
      position_data.deform(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_mesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_twist_grids(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             SubdivCCG &subdiv_ccg,
                             const Span<int> vert_propagation_steps,
                             const Span<float> vert_factors,
                             const float3 twist_pivot_position,
                             const float3 twist_axis,
                             const bke::pbvh::GridsNode &node,
                             LocalDataGrids &tls,
                             const float3 symmetry_pivot,
                             const float strength,
                             const eBrushDeformTarget deform_target)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_grids(
      subdiv_ccg, vert_factors, grids, tls.factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  const Span<int> propagation_steps = gather_data_grids(
      subdiv_ccg, vert_propagation_steps, grids, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(grid_verts_num);
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_twist_position(
      orig_data.positions, twist_pivot_position, twist_axis, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(grid_verts_num);
      const MutableSpan<float3> translations = tls.translations;
      const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_data.positions, translations);
      apply_translations(translations, grids, subdiv_ccg);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_grids(subdiv_ccg,
                         new_positions.as_span(),
                         grids,
                         cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_twist_bmesh(const Depsgraph &depsgraph,
                             const Sculpt &sd,
                             Object &object,
                             const Span<int> vert_propagation_steps,
                             const Span<float> vert_factors,
                             const float3 twist_pivot_position,
                             const float3 twist_axis,
                             bke::pbvh::BMeshNode &node,
                             LocalDataBMesh &tls,
                             const float3 symmetry_pivot,
                             const float strength,
                             const eBrushDeformTarget deform_target)

{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_bmesh(vert_factors, verts, tls.factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  const Span<int> propagation_steps = gather_data_bmesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_twist_position(orig_positions, twist_pivot_position, twist_axis, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      const MutableSpan<float3> positions = gather_bmesh_positions(verts, tls.positions);
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_positions, translations);
      apply_translations(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_bmesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void do_twist_brush(const Depsgraph &depsgraph,
                           const Sculpt &sd,
                           Object &object,
                           const IndexMask &node_mask,
                           const SculptBoundary &boundary,
                           const float strength,
                           const eBrushDeformTarget deform_target)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const PositionDeformData position_data(depsgraph, object);

      threading::EnumerableThreadSpecific<LocalDataMesh> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataMesh &tls = all_tls.local();
        calc_twist_mesh(depsgraph,
                        sd,
                        object,
                        boundary.edit_info.propagation_steps_num,
                        boundary.edit_info.strength_factor,
                        nodes[i],
                        tls,
                        boundary.twist.pivot_position,
                        boundary.twist.rotation_axis,
                        boundary.initial_vert_position,
                        strength,
                        deform_target,
                        position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      threading::EnumerableThreadSpecific<LocalDataGrids> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataGrids &tls = all_tls.local();
        calc_twist_grids(depsgraph,
                         sd,
                         object,
                         subdiv_ccg,
                         boundary.edit_info.propagation_steps_num,
                         boundary.edit_info.strength_factor,
                         boundary.twist.pivot_position,
                         boundary.twist.rotation_axis,
                         nodes[i],
                         tls,
                         boundary.initial_vert_position,
                         strength,
                         deform_target);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      threading::EnumerableThreadSpecific<LocalDataBMesh> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataBMesh &tls = all_tls.local();
        calc_twist_bmesh(depsgraph,
                         sd,
                         object,
                         boundary.edit_info.propagation_steps_num,
                         boundary.edit_info.strength_factor,
                         boundary.twist.pivot_position,
                         boundary.twist.rotation_axis,
                         nodes[i],
                         tls,
                         boundary.initial_vert_position,
                         strength,
                         deform_target);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smooth Deformation
 * \{ */

BLI_NOINLINE static void calc_smooth_position(const Span<float3> positions,
                                              const Span<float3> average_position,
                                              const Span<float> factors,
                                              const MutableSpan<float3> new_positions)
{
  BLI_assert(positions.size() == average_position.size());
  BLI_assert(positions.size() == factors.size());
  BLI_assert(positions.size() == new_positions.size());

  for (const int i : positions.index_range()) {
    const float3 to_smooth = average_position[i] - positions[i];
    new_positions[i] = positions[i] + (to_smooth * factors[i]);
  }
}

BLI_NOINLINE static void calc_average_position(const Span<float3> vert_positions,
                                               const Span<int> vert_propagation_steps,
                                               const GroupedSpan<int> neighbors,
                                               const Span<int> propagation_steps,
                                               const MutableSpan<float> factors,
                                               const MutableSpan<float3> average_positions)
{
  BLI_assert(vert_positions.size() == vert_propagation_steps.size());
  BLI_assert(factors.size() == neighbors.size());
  BLI_assert(factors.size() == propagation_steps.size());
  BLI_assert(factors.size() == average_positions.size());

  for (const int i : neighbors.index_range()) {
    average_positions[i] = float3(0.0f);
    int valid_neighbors = 0;
    for (const int neighbor : neighbors[i]) {
      if (propagation_steps[i] == vert_propagation_steps[neighbor]) {
        average_positions[i] += vert_positions[neighbor];
        valid_neighbors++;
      }
    }
    average_positions[i] *= math::safe_rcp(float(valid_neighbors));
    if (valid_neighbors == 0) {
      factors[i] = 0.0f;
    }
  }
}

BLI_NOINLINE static void calc_average_position(const Span<int> vert_propagation_steps,
                                               const GroupedSpan<BMVert *> neighbors,
                                               const Span<int> propagation_steps,
                                               const MutableSpan<float> factors,
                                               const MutableSpan<float3> average_positions)
{
  BLI_assert(neighbors.size() == propagation_steps.size());
  BLI_assert(neighbors.size() == factors.size());
  BLI_assert(neighbors.size() == average_positions.size());

  for (const int i : neighbors.index_range()) {
    average_positions[i] = float3(0.0f);
    int valid_neighbors = 0;
    for (BMVert *neighbor : neighbors[i]) {
      const int neighbor_idx = BM_elem_index_get(neighbor);
      if (propagation_steps[i] == vert_propagation_steps[neighbor_idx]) {
        average_positions[i] += neighbor->co;
        valid_neighbors++;
      }
    }
    average_positions[i] *= math::safe_rcp(float(valid_neighbors));
    if (valid_neighbors == 0) {
      factors[i] = 0.0f;
    }
  }
}

static void calc_smooth_mesh(const Sculpt &sd,
                             Object &object,
                             const OffsetIndices<int> faces,
                             const Span<int> corner_verts,
                             const GroupedSpan<int> vert_to_face,
                             const Span<bool> hide_poly,
                             const Span<int> vert_propagation_steps,
                             const Span<float> vert_factors,
                             const bke::pbvh::MeshNode &node,
                             LocalDataMesh &tls,
                             const float3 symmetry_pivot,
                             const float strength,
                             const eBrushDeformTarget deform_target,
                             const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();
  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_mesh(vert_factors, verts, tls.factors);

  const Span<int> propagation_steps = gather_data_mesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                         corner_verts,
                                                         vert_to_face,
                                                         hide_poly,
                                                         verts,
                                                         tls.neighbor_offsets,
                                                         tls.neighbor_data);
  tls.average_positions.resize(verts.size());

  const Span<float3> positions = gather_data_mesh(position_data.eval, verts, tls.positions);
  const MutableSpan<float3> average_positions = tls.average_positions;
  calc_average_position(position_data.eval,
                        vert_propagation_steps,
                        neighbors,
                        propagation_steps,
                        factors,
                        average_positions);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_smooth_position(positions, average_positions, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, verts, position_data.eval, translations);
      clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
      position_data.deform(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_mesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_smooth_grids(const Sculpt &sd,
                              Object &object,
                              SubdivCCG &subdiv_ccg,
                              const Span<int> vert_propagation_steps,
                              const Span<float> vert_factors,
                              const bke::pbvh::GridsNode &node,
                              LocalDataGrids &tls,
                              const float3 symmetry_pivot,
                              const float strength,
                              const eBrushDeformTarget deform_target)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_grids(
      subdiv_ccg, vert_factors, grids, tls.factors);

  const Span<int> propagation_steps = gather_data_grids(
      subdiv_ccg, vert_propagation_steps, grids, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_data.positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  const GroupedSpan<int> neighbors = calc_vert_neighbors(
      subdiv_ccg, grids, tls.neighbor_offsets, tls.neighbor_data);

  tls.average_positions.resize(grid_verts_num);
  const MutableSpan<float3> average_positions = tls.average_positions;
  calc_average_position(subdiv_ccg.positions,
                        vert_propagation_steps,
                        neighbors,
                        propagation_steps,
                        factors,
                        average_positions);

  const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.new_positions.resize(grid_verts_num);
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_smooth_position(positions, average_positions, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(grid_verts_num);
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_data.positions, translations);
      apply_translations(translations, grids, subdiv_ccg);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_grids(subdiv_ccg,
                         new_positions.as_span(),
                         grids,
                         cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_smooth_bmesh(const Sculpt &sd,
                              Object &object,
                              const Span<int> vert_propagation_steps,
                              const Span<float> vert_factors,
                              bke::pbvh::BMeshNode &node,
                              LocalDataBMesh &tls,
                              const float3 symmetry_pivot,
                              const float strength,
                              const eBrushDeformTarget deform_target)

{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);

  const MutableSpan<float> factors = gather_data_bmesh(vert_factors, verts, tls.factors);

  const Span<int> propagation_steps = gather_data_bmesh(
      vert_propagation_steps, verts, tls.propagation_steps);

  filter_uninitialized_verts(propagation_steps, factors);
  filter_verts_outside_symmetry_area(orig_positions, symmetry_pivot, symm, factors);

  scale_factors(factors, strength);

  const GroupedSpan<BMVert *> neighbors = calc_vert_neighbors(
      verts, tls.neighbor_offsets, tls.neighbor_data);

  tls.average_positions.resize(verts.size());
  const MutableSpan<float3> average_positions = tls.average_positions;
  calc_average_position(
      vert_propagation_steps, neighbors, propagation_steps, factors, average_positions);
  const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);

  tls.new_positions.resize(verts.size());
  const MutableSpan<float3> new_positions = tls.new_positions;
  calc_smooth_position(positions, average_positions, factors, new_positions);

  switch (eBrushDeformTarget(deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY: {
      tls.translations.resize(verts.size());
      const MutableSpan<float3> translations = tls.translations;
      translations_from_new_positions(new_positions, positions, translations);

      clip_and_lock_translations(sd, ss, orig_positions, translations);
      apply_translations(translations, verts);
      break;
    }
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      scatter_data_bmesh(
          new_positions.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void do_smooth_brush(const Depsgraph &depsgraph,
                            const Sculpt &sd,
                            Object &object,
                            const IndexMask &node_mask,
                            const SculptBoundary &boundary,
                            const float strength,
                            const eBrushDeformTarget deform_target)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const PositionDeformData position_data(depsgraph, object);
      const OffsetIndices<int> faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
      const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

      threading::EnumerableThreadSpecific<LocalDataMesh> all_tls;
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataMesh &tls = all_tls.local();
        calc_smooth_mesh(sd,
                         object,
                         faces,
                         corner_verts,
                         vert_to_face_map,
                         hide_poly,
                         boundary.edit_info.propagation_steps_num,
                         boundary.edit_info.strength_factor,
                         nodes[i],
                         tls,
                         boundary.initial_vert_position,
                         strength,
                         deform_target,
                         position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      threading::EnumerableThreadSpecific<LocalDataGrids> all_tls;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataGrids &tls = all_tls.local();
        calc_smooth_grids(sd,
                          object,
                          subdiv_ccg,
                          boundary.edit_info.propagation_steps_num,
                          boundary.edit_info.strength_factor,
                          nodes[i],
                          tls,
                          boundary.initial_vert_position,
                          strength,
                          deform_target);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      threading::EnumerableThreadSpecific<LocalDataBMesh> all_tls;
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalDataBMesh &tls = all_tls.local();
        calc_smooth_bmesh(sd,
                          object,
                          boundary.edit_info.propagation_steps_num,
                          boundary.edit_info.strength_factor,
                          nodes[i],
                          tls,
                          boundary.initial_vert_position,
                          strength,
                          deform_target);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

/* -------------------------------------------------------------------- */
/** \name Brush Initialization
 * \{ */

static float displacement_from_grab_delta_get(const SculptSession &ss,
                                              const SculptBoundary &boundary)
{
  float4 plane;
  const float3 normal = math::normalize(ss.cache->initial_location_symm - boundary.pivot_position);
  plane_from_point_normal_v3(plane, ss.cache->initial_location_symm, normal);

  const float3 pos = ss.cache->initial_location_symm + ss.cache->grab_delta_symm;
  return dist_signed_to_plane_v3(pos, plane);
}

static std::pair<float, float> calc_boundary_falloff(const SculptBoundary &boundary,
                                                     const Brush &brush,
                                                     const float radius,
                                                     const int index)
{
  const float boundary_distance = boundary.distance.lookup_default(index, 0.0f);
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
      BLI_assert_unreachable();
      break;
  }

  return {falloff_distance, direction};
}

/**
 * These functions assign a falloff factor to each valid edit_info entry based on the brush curve,
 * its propagation steps, and mask values. The falloff goes from the boundary into the mesh.
 */
static void init_falloff_mesh(const Span<float> mask,
                              const Brush &brush,
                              const float radius,
                              SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const int num_elements = boundary.edit_info.strength_factor.size();
  BKE_curvemapping_init(brush.curve_distance_falloff);

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != BOUNDARY_STEPS_NONE) {
      const float mask_factor = mask.is_empty() ? 1.0f : 1.0f - mask[i];
      boundary.edit_info.strength_factor[i] = mask_factor *
                                              BKE_brush_curve_strength(
                                                  &brush,
                                                  boundary.edit_info.propagation_steps_num[i],
                                                  boundary.max_propagation_steps);
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

    auto [falloff_distance, direction] = calc_boundary_falloff(
        boundary, brush, radius, boundary.edit_info.original_vertex_i[i]);
    boundary.edit_info.strength_factor[i] *= direction * BKE_brush_curve_strength(
                                                             &brush, falloff_distance, radius);
  }
}

static void init_falloff_grids(const SubdivCCG &subdiv_ccg,
                               const Brush &brush,
                               const float radius,
                               SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  BKE_curvemapping_init(brush.curve_distance_falloff);

  for (const int grid : IndexRange(subdiv_ccg.grids_num)) {
    for (const int index : bke::ccg::grid_range(key, grid)) {
      if (boundary.edit_info.propagation_steps_num[index] != BOUNDARY_STEPS_NONE) {
        const float mask_factor = subdiv_ccg.masks.is_empty() ? 1.0f :
                                                                1.0f - subdiv_ccg.masks[index];
        boundary.edit_info.strength_factor[index] =
            mask_factor * BKE_brush_curve_strength(&brush,
                                                   boundary.edit_info.propagation_steps_num[index],
                                                   boundary.max_propagation_steps);
      }

      if (boundary.edit_info.original_vertex_i[index] == boundary.initial_vert_i) {
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

      auto [falloff_distance, direction] = calc_boundary_falloff(
          boundary, brush, radius, boundary.edit_info.original_vertex_i[index]);
      boundary.edit_info.strength_factor[index] *= direction *
                                                   BKE_brush_curve_strength(
                                                       &brush, falloff_distance, radius);
    }
  }
}

static void init_falloff_bmesh(BMesh *bm,
                               const Brush &brush,
                               const float radius,
                               SculptBoundary &boundary)
{
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.propagation_steps_num.size());
  BLI_assert(boundary.edit_info.original_vertex_i.size() ==
             boundary.edit_info.strength_factor.size());

  const int num_elements = boundary.edit_info.strength_factor.size();

  BKE_curvemapping_init(brush.curve_distance_falloff);

  for (const int i : IndexRange(num_elements)) {
    if (boundary.edit_info.propagation_steps_num[i] != BOUNDARY_STEPS_NONE) {
      BMVert *vert = BM_vert_at_index(bm, i);
      const int mask_offset = CustomData_get_offset_named(
          &bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
      const float mask_factor = mask_offset == -1 ? 1.0f :
                                                    1.0f - BM_ELEM_CD_GET_FLOAT(vert, mask_offset);

      boundary.edit_info.strength_factor[i] = mask_factor *
                                              BKE_brush_curve_strength(
                                                  &brush,
                                                  boundary.edit_info.propagation_steps_num[i],
                                                  boundary.max_propagation_steps);
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

    auto [falloff_distance, direction] = calc_boundary_falloff(
        boundary, brush, radius, boundary.edit_info.original_vertex_i[i]);
    boundary.edit_info.strength_factor[i] *= direction * BKE_brush_curve_strength(
                                                             &brush, falloff_distance, radius);
  }
}

static void init_boundary_mesh(const Depsgraph &depsgraph,
                               Object &object,
                               const Brush &brush,
                               const ePaintSymmetryFlags symm_area)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const bke::AttributeAccessor attributes = mesh.attributes();
  VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
  VArraySpan<float> mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point);

  const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);

  ActiveVert initial_vert_ref = ss.active_vert();
  if (std::holds_alternative<std::monostate>(initial_vert_ref)) {
    return;
  }

  std::optional<int> initial_vert;
  if (ss.cache->mirror_symmetry_pass == 0) {
    initial_vert = std::get<int>(initial_vert_ref);
  }
  else {
    float3 location = symmetry_flip(positions_eval[std::get<int>(initial_vert_ref)], symm_area);
    initial_vert = nearest_vert_calc_mesh(
        pbvh, positions_eval, hide_vert, location, ss.cache->radius_squared, false);
  }

  if (!initial_vert) {
    return;
  }

  ss.cache->boundaries[symm_area] = boundary::data_init_mesh(
      depsgraph, object, &brush, *initial_vert, ss.cache->initial_radius);

  if (ss.cache->boundaries[symm_area]) {
    switch (brush.boundary_deform_type) {
      case BRUSH_BOUNDARY_DEFORM_BEND:
        bend_data_init_mesh(positions_eval, vert_normals, *ss.cache->boundaries[symm_area]);
        break;
      case BRUSH_BOUNDARY_DEFORM_EXPAND:
        slide_data_init_mesh(positions_eval, *ss.cache->boundaries[symm_area]);
        break;
      case BRUSH_BOUNDARY_DEFORM_TWIST:
        twist_data_init_mesh(positions_eval, *ss.cache->boundaries[symm_area]);
        break;
      case BRUSH_BOUNDARY_DEFORM_INFLATE:
      case BRUSH_BOUNDARY_DEFORM_GRAB:
      case BRUSH_BOUNDARY_DEFORM_SMOOTH:
        /* Do nothing. These deform modes don't need any extra data to be precomputed. */
        break;
    }

    init_falloff_mesh(mask, brush, ss.cache->initial_radius, *ss.cache->boundaries[symm_area]);
  }
}

static void init_boundary_grids(Object &object,
                                const Brush &brush,
                                const ePaintSymmetryFlags symm_area)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey &key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float3> positions = subdiv_ccg.positions;

  ActiveVert initial_vert_ref = ss.active_vert();
  if (std::holds_alternative<std::monostate>(initial_vert_ref)) {
    return;
  }

  std::optional<SubdivCCGCoord> initial_vert;
  if (ss.cache->mirror_symmetry_pass == 0) {
    initial_vert = SubdivCCGCoord::from_index(key, std::get<int>(initial_vert_ref));
  }
  else {
    const int active_vert = std::get<int>(initial_vert_ref);
    float3 location = symmetry_flip(positions[active_vert], symm_area);
    initial_vert = nearest_vert_calc_grids(
        pbvh, subdiv_ccg, location, ss.cache->radius_squared, false);
  }

  if (!initial_vert) {
    return;
  }

  ss.cache->boundaries[symm_area] = boundary::data_init_grids(
      object, &brush, *initial_vert, ss.cache->initial_radius);

  if (ss.cache->boundaries[symm_area]) {
    switch (brush.boundary_deform_type) {
      case BRUSH_BOUNDARY_DEFORM_BEND:
        bend_data_init_grids(subdiv_ccg, *ss.cache->boundaries[symm_area]);
        break;
      case BRUSH_BOUNDARY_DEFORM_EXPAND:
        slide_data_init_grids(subdiv_ccg, *ss.cache->boundaries[symm_area]);
        break;
      case BRUSH_BOUNDARY_DEFORM_TWIST:
        twist_data_init_grids(subdiv_ccg, *ss.cache->boundaries[symm_area]);
        break;
      case BRUSH_BOUNDARY_DEFORM_INFLATE:
      case BRUSH_BOUNDARY_DEFORM_GRAB:
      case BRUSH_BOUNDARY_DEFORM_SMOOTH:
        /* Do nothing. These deform modes don't need any extra data to be precomputed. */
        break;
    }

    init_falloff_grids(
        subdiv_ccg, brush, ss.cache->initial_radius, *ss.cache->boundaries[symm_area]);
  }
}

static void init_boundary_bmesh(Object &object,
                                const Brush &brush,
                                const ePaintSymmetryFlags symm_area)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  BMesh *bm = ss.bm;

  ActiveVert initial_vert_ref = ss.active_vert();
  if (std::holds_alternative<std::monostate>(initial_vert_ref)) {
    return;
  }

  std::optional<BMVert *> initial_vert;
  if (ss.cache->mirror_symmetry_pass == 0) {
    initial_vert = std::get<BMVert *>(initial_vert_ref);
  }
  else {
    BMVert *active_vert = std::get<BMVert *>(initial_vert_ref);
    float3 location = symmetry_flip(active_vert->co, symm_area);
    initial_vert = nearest_vert_calc_bmesh(pbvh, location, ss.cache->radius_squared, false);
  }

  if (!initial_vert) {
    return;
  }

  ss.cache->boundaries[symm_area] = boundary::data_init_bmesh(
      object, &brush, *initial_vert, ss.cache->initial_radius);

  if (ss.cache->boundaries[symm_area]) {
    switch (brush.boundary_deform_type) {
      case BRUSH_BOUNDARY_DEFORM_BEND:
        bend_data_init_bmesh(bm, *ss.cache->boundaries[symm_area]);
        break;
      case BRUSH_BOUNDARY_DEFORM_EXPAND:
        slide_data_init_bmesh(bm, *ss.cache->boundaries[symm_area]);
        break;
      case BRUSH_BOUNDARY_DEFORM_TWIST:
        twist_data_init_bmesh(bm, *ss.cache->boundaries[symm_area]);
        break;
      case BRUSH_BOUNDARY_DEFORM_INFLATE:
      case BRUSH_BOUNDARY_DEFORM_GRAB:
      case BRUSH_BOUNDARY_DEFORM_SMOOTH:
        /* Do nothing. These deform modes don't need any extra data to be precomputed. */
        break;
    }

    init_falloff_bmesh(bm, brush, ss.cache->initial_radius, *ss.cache->boundaries[symm_area]);
  }
}

static float get_mesh_strength(const SculptSession &ss, const Brush &brush)
{
  const int symm_area = ss.cache->mirror_symmetry_pass;
  SculptBoundary &boundary = *ss.cache->boundaries[symm_area];

  const float strength = ss.cache->bstrength;

  switch (brush.boundary_deform_type) {
    case BRUSH_BOUNDARY_DEFORM_BEND: {
      const float disp = strength * displacement_from_grab_delta_get(ss, boundary);
      float angle_factor = disp / ss.cache->radius;
      /* Angle Snapping when inverting the brush. */
      if (ss.cache->invert) {
        angle_factor = floorf(angle_factor * 10) / 10.0f;
      }
      return angle_factor * M_PI;
    }
    case BRUSH_BOUNDARY_DEFORM_EXPAND: {
      return strength * displacement_from_grab_delta_get(ss, boundary);
    }
    case BRUSH_BOUNDARY_DEFORM_INFLATE: {
      return strength * displacement_from_grab_delta_get(ss, boundary);
    }
    case BRUSH_BOUNDARY_DEFORM_GRAB:
      return strength;
    case BRUSH_BOUNDARY_DEFORM_TWIST: {
      const float disp = strength * displacement_from_grab_delta_get(ss, boundary);
      float angle_factor = disp / ss.cache->radius;
      /* Angle Snapping when inverting the brush. */
      if (ss.cache->invert) {
        angle_factor = floorf(angle_factor * 10) / 10.0f;
      }
      return angle_factor * M_PI;
    }
    case BRUSH_BOUNDARY_DEFORM_SMOOTH:
      return strength;
  }

  BLI_assert_unreachable();
  return 0.0f;
}

void do_boundary_brush(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &ob,
                       const IndexMask &node_mask)
{
  SculptSession &ss = *ob.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  const ePaintSymmetryFlags symm_area = ss.cache->mirror_symmetry_pass;
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    switch (pbvh.type()) {
      case bke::pbvh::Type::Mesh:
        init_boundary_mesh(depsgraph, ob, brush, symm_area);
        break;
      case bke::pbvh::Type::Grids:
        init_boundary_grids(ob, brush, symm_area);
        break;
      case bke::pbvh::Type::BMesh:
        init_boundary_bmesh(ob, brush, symm_area);
        break;
    }
  }

  /* No active boundary under the cursor. */
  if (!ss.cache->boundaries[symm_area]) {
    return;
  }

  const float strength = get_mesh_strength(ss, brush);

  switch (brush.boundary_deform_type) {
    case BRUSH_BOUNDARY_DEFORM_BEND:
      do_bend_brush(depsgraph,
                    sd,
                    ob,
                    node_mask,
                    *ss.cache->boundaries[symm_area],
                    strength,
                    eBrushDeformTarget(brush.deform_target));
      break;
    case BRUSH_BOUNDARY_DEFORM_EXPAND:
      do_slide_brush(depsgraph,
                     sd,
                     ob,
                     node_mask,
                     *ss.cache->boundaries[symm_area],
                     strength,
                     eBrushDeformTarget(brush.deform_target));
      break;
    case BRUSH_BOUNDARY_DEFORM_INFLATE:
      do_inflate_brush(depsgraph,
                       sd,
                       ob,
                       node_mask,
                       *ss.cache->boundaries[symm_area],
                       strength,
                       eBrushDeformTarget(brush.deform_target));
      break;
    case BRUSH_BOUNDARY_DEFORM_GRAB:
      do_grab_brush(depsgraph,
                    sd,
                    ob,
                    node_mask,
                    *ss.cache->boundaries[symm_area],
                    strength,
                    eBrushDeformTarget(brush.deform_target));
      break;
    case BRUSH_BOUNDARY_DEFORM_TWIST:
      do_twist_brush(depsgraph,
                     sd,
                     ob,
                     node_mask,
                     *ss.cache->boundaries[symm_area],
                     strength,
                     eBrushDeformTarget(brush.deform_target));
      break;
    case BRUSH_BOUNDARY_DEFORM_SMOOTH:
      do_smooth_brush(depsgraph,
                      sd,
                      ob,
                      node_mask,
                      *ss.cache->boundaries[symm_area],
                      strength,
                      eBrushDeformTarget(brush.deform_target));
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

std::unique_ptr<SculptBoundary> data_init(const Depsgraph &depsgraph,
                                          Object &object,
                                          const Brush *brush,
                                          const int initial_vert,
                                          const float radius)
{
  /* TODO: Temporary bridge method to help in refactoring, this method should be deprecated
   * entirely. */
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  switch (pbvh.type()) {
    case (bke::pbvh::Type::Mesh): {
      return data_init_mesh(depsgraph, object, brush, initial_vert, radius);
    }
    case (bke::pbvh::Type::Grids): {
      const CCGKey &key = BKE_subdiv_ccg_key_top_level(*ss.subdiv_ccg);
      const SubdivCCGCoord vert = SubdivCCGCoord::from_index(key, initial_vert);
      return data_init_grids(object, brush, vert, radius);
    }
    case (bke::pbvh::Type::BMesh): {
      BMVert *vert = BM_vert_at_index(ss.bm, initial_vert);
      return data_init_bmesh(object, brush, vert, radius);
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

std::unique_ptr<SculptBoundary> data_init_mesh(const Depsgraph &depsgraph,
                                               Object &object,
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
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

  const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);

  const std::optional<int> boundary_initial_vert = get_closest_boundary_vert_mesh(
      object,
      vert_to_face_map,
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
                                         vert_to_face_map,
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
                    vert_to_face_map,
                    hide_vert,
                    hide_poly,
                    ss.vertex_info.boundary,
                    positions_eval,
                    *boundary_initial_vert,
                    *boundary);

  const float boundary_radius = brush ? radius * (1.0f + brush->boundary_offset) : radius;
  edit_data_init_mesh(faces,
                      corner_verts,
                      vert_to_face_map,
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
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const Span<float3> positions = subdiv_ccg.positions;
  const CCGKey &key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const std::optional<SubdivCCGCoord> boundary_initial_vert = get_closest_boundary_vert_grids(
      object, faces, corner_verts, subdiv_ccg, ss.vertex_info.boundary, initial_vert, radius);

  if (!boundary_initial_vert) {
    return nullptr;
  }

  /* Starting from a vertex that is the limit of a boundary is ambiguous, so return nullptr instead
   * of forcing a random active boundary from a corner. */
  if (!is_vert_in_editable_boundary_grids(
          faces, corner_verts, subdiv_ccg, ss.vertex_info.boundary, initial_vert))
  {
    return nullptr;
  }

  std::unique_ptr<SculptBoundary> boundary = std::make_unique<SculptBoundary>();
  *boundary = {};

  SubdivCCGCoord boundary_vert = *boundary_initial_vert;
  const int boundary_initial_vert_index = boundary_vert.to_index(key);
  boundary->initial_vert_i = boundary_initial_vert_index;
  boundary->initial_vert_position = positions[boundary_initial_vert_index];

  indices_init_grids(
      object, faces, corner_verts, subdiv_ccg, ss.vertex_info.boundary, boundary_vert, *boundary);

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

  vert_random_access_ensure(object);
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

std::unique_ptr<SculptBoundaryPreview> preview_data_init(const Depsgraph &depsgraph,
                                                         Object &object,
                                                         const Brush *brush,
                                                         const float radius)
{
  const SculptSession &ss = *object.sculpt;
  ActiveVert initial_vert = ss.active_vert();

  if (std::holds_alternative<std::monostate>(initial_vert)) {
    return nullptr;
  }

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  std::unique_ptr<SculptBoundary> boundary = nullptr;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh:
      boundary = data_init_mesh(depsgraph, object, brush, std::get<int>(initial_vert), radius);
      break;
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      boundary = data_init_grids(
          object, brush, SubdivCCGCoord::from_index(key, std::get<int>(initial_vert)), radius);
      break;
    }
    case bke::pbvh::Type::BMesh:
      boundary = data_init_bmesh(object, brush, std::get<BMVert *>(initial_vert), radius);
      break;
  }

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
