/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_bitmap.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_multires.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_eval.h"
#include "BKE_subdiv_foreach.h"
#include "BKE_subdiv_mesh.h"

#include "opensubdiv_converter_capi.h"
#include "opensubdiv_evaluator_capi.h"
#include "opensubdiv_topology_refiner_capi.h"

#include "atomic_ops.h"
#include "subdiv_converter.h"

/* -------------------------------------------------------------------- */
/** \name Local Structs
 * \{ */

/* Surface refers to a simplified and lower-memory footprint representation of the limit surface.
 *
 * Used to store pre-calculated information which is expensive or impossible to evaluate when
 * travesing the final limit surface.  */

typedef struct SurfacePoint {
  float P[3];
  float tangent_matrix[3][3];
} SurfacePoint;

typedef struct SurfaceGrid {
  SurfacePoint *points;
} SurfaceGrid;

/* Geometry elements which are used to simplify creation of topology refiner at the sculpt level.
 * Contains a limited subset of information needed to construct topology refiner. */

typedef struct Vertex {
  /* All grid coordinates which the vertex corresponding to.
   * For a vertices which are created from inner points of grids there is always one coordinate. */
  int num_grid_coords;
  GridCoord *grid_coords;

  bool is_infinite_sharp;
} Vertex;

typedef struct Corner {
  const Vertex *vertex;
  int grid_index;
} Corner;

typedef struct Face {
  int start_corner_index;
  int num_corners;
} Face;

typedef struct Edge {
  int v1;
  int v2;

  float sharpness;
} Edge;

/* Storage of data which is linearly interpolated from the reshape level to the top level. */

typedef struct LinearGridElement {
  float mask;
} LinearGridElement;

typedef struct LinearGrid {
  LinearGridElement *elements;
} LinearGrid;

typedef struct LinearGrids {
  int num_grids;
  int level;

  /* Cached size for the grid, for faster lookup. */
  int grid_size;

  /* Indexed by grid index. */
  LinearGrid *grids;

  /* Elements for all grids are allocated in a single array, for the allocation performance. */
  LinearGridElement *elements_storage;
} LinearGrids;

/* Context which holds all information eeded during propagation and smoothing. */

typedef struct MultiresReshapeSmoothContext {
  const MultiresReshapeContext *reshape_context;

  /* Geometry at a reshape multires level. */
  struct {
    int num_vertices;
    Vertex *vertices;

    /* Maximum number of edges which might be stored in the edges array.
     * Is calculated based on the number of edges in the base mesh and the subdivision level. */
    int max_edges;

    /* Sparse storage of edges. Will only include edges which have non-zero sharpness.
     *
     * NOTE: Different type from others to be able to easier use atomic ops. */
    size_t num_edges;
    Edge *edges;

    int num_corners;
    Corner *corners;

    int num_faces;
    Face *faces;
  } geometry;

  /* Grids of data which is linearly interpolated between grid elements at the reshape level.
   * The data is actually stored as a delta, which is then to be added to the higher levels. */
  LinearGrids linear_delta_grids;

  /* Index i of this map indicates that base edge i is adjacent to at least one face. */
  BLI_bitmap *non_loose_base_edge_map;

  /* Subdivision surface created for geometry at a reshape level. */
  Subdiv *reshape_subdiv;

  /* Limit surface of the base mesh with original sculpt level details on it, subdivided up to the
   * top level.
   * Is used as a base point to calculate how much displacement has been made in the sculpt mode.
   *
   * NOTE: Referring to sculpt as it is the main user of this functionality and it is clear to
   * understand what it actually means in a concrete example. This is a generic code which is also
   * used by Subdivide operation, but the idea is exactly the same as propagation in the sculpt
   * mode. */
  SurfaceGrid *base_surface_grids;

  /* Defines how displacement is interpolated on the higher levels (for example, whether
   * displacement is smoothed in Catmull-Clark mode or interpolated linearly preserving sharp edges
   * of the current sculpt level).
   *
   * NOTE: Uses same enumerator type as Subdivide operator, since the values are the same and
   * decoupling type just adds extra headache to convert one enumerator to another. */
  eMultiresSubdivideModeType smoothing_type;
} MultiresReshapeSmoothContext;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linear grids manipulation
 * \{ */

static void linear_grids_init(LinearGrids *linear_grids)
{
  linear_grids->num_grids = 0;
  linear_grids->level = 0;

  linear_grids->grids = NULL;
  linear_grids->elements_storage = NULL;
}

static void linear_grids_allocate(LinearGrids *linear_grids, int num_grids, int level)
{
  const size_t grid_size = BKE_subdiv_grid_size_from_level(level);
  const size_t grid_area = grid_size * grid_size;
  const size_t num_grid_elements = num_grids * grid_area;

  linear_grids->num_grids = num_grids;
  linear_grids->level = level;
  linear_grids->grid_size = grid_size;

  linear_grids->grids = MEM_malloc_arrayN(num_grids, sizeof(LinearGrid), "linear grids");
  linear_grids->elements_storage = MEM_calloc_arrayN(
      num_grid_elements, sizeof(LinearGridElement), "linear elements storage");

  for (int i = 0; i < num_grids; ++i) {
    const size_t element_offset = grid_area * i;
    linear_grids->grids[i].elements = &linear_grids->elements_storage[element_offset];
  }
}

static LinearGridElement *linear_grid_element_get(const LinearGrids *linear_grids,
                                                  const GridCoord *grid_coord)
{
  BLI_assert(grid_coord->grid_index >= 0);
  BLI_assert(grid_coord->grid_index < linear_grids->num_grids);

  const int grid_size = linear_grids->grid_size;

  const int grid_x = lround(grid_coord->u * (grid_size - 1));
  const int grid_y = lround(grid_coord->v * (grid_size - 1));
  const int grid_element_index = grid_y * grid_size + grid_x;

  LinearGrid *grid = &linear_grids->grids[grid_coord->grid_index];
  return &grid->elements[grid_element_index];
}

static void linear_grids_free(LinearGrids *linear_grids)
{
  MEM_SAFE_FREE(linear_grids->grids);
  MEM_SAFE_FREE(linear_grids->elements_storage);
}

static void linear_grid_element_init(LinearGridElement *linear_grid_element)
{
  linear_grid_element->mask = 0.0f;
}

/* result = a - b. */
static void linear_grid_element_sub(LinearGridElement *result,
                                    const LinearGridElement *a,
                                    const LinearGridElement *b)
{
  result->mask = a->mask - b->mask;
}

static void linear_grid_element_interpolate(LinearGridElement *result,
                                            const LinearGridElement elements[4],
                                            const float weights[4])
{
  result->mask = elements[0].mask * weights[0] + elements[1].mask * weights[1] +
                 elements[2].mask * weights[2] + elements[3].mask * weights[3];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

static void base_surface_grids_allocate(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const int num_grids = reshape_context->num_grids;
  const int grid_size = reshape_context->top.grid_size;
  const int grid_area = grid_size * grid_size;

  SurfaceGrid *surface_grid = MEM_malloc_arrayN(num_grids, sizeof(SurfaceGrid), "delta grids");

  for (int grid_index = 0; grid_index < num_grids; ++grid_index) {
    surface_grid[grid_index].points = MEM_calloc_arrayN(
        sizeof(SurfacePoint), grid_area, "delta grid dispalcement");
  }

  reshape_smooth_context->base_surface_grids = surface_grid;
}

static void base_surface_grids_free(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  if (reshape_smooth_context->base_surface_grids == NULL) {
    return;
  }

  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const int num_grids = reshape_context->num_grids;
  for (int grid_index = 0; grid_index < num_grids; ++grid_index) {
    MEM_freeN(reshape_smooth_context->base_surface_grids[grid_index].points);
  }
  MEM_freeN(reshape_smooth_context->base_surface_grids);
}

static SurfacePoint *base_surface_grids_read(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const GridCoord *grid_coord)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const int grid_index = grid_coord->grid_index;
  const int grid_size = reshape_context->top.grid_size;
  const int grid_x = lround(grid_coord->u * (grid_size - 1));
  const int grid_y = lround(grid_coord->v * (grid_size - 1));
  const int grid_element_index = grid_y * grid_size + grid_x;

  SurfaceGrid *surface_grid = &reshape_smooth_context->base_surface_grids[grid_index];
  return &surface_grid->points[grid_element_index];
}

static void base_surface_grids_write(const MultiresReshapeSmoothContext *reshape_smooth_context,
                                     const GridCoord *grid_coord,
                                     float P[3],
                                     float tangent_matrix[3][3])
{
  SurfacePoint *point = base_surface_grids_read(reshape_smooth_context, grid_coord);
  copy_v3_v3(point->P, P);
  copy_m3_m3(point->tangent_matrix, tangent_matrix);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation of subdivision surface at a reshape level
 * \{ */

typedef void (*ForeachTopLevelGridCoordCallback)(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const PTexCoord *ptex_coord,
    const GridCoord *grid_coord,
    void *userdata_v);

typedef struct ForeachTopLevelGridCoordTaskData {
  const MultiresReshapeSmoothContext *reshape_smooth_context;

  int inner_grid_size;
  float inner_grid_size_1_inv;

  ForeachTopLevelGridCoordCallback callback;
  void *callback_userdata_v;
} ForeachHighLevelCoordTaskData;

/* Find grid index which given face was created for. */
static int get_face_grid_index(const MultiresReshapeSmoothContext *reshape_smooth_context,
                               const Face *face)
{
  const Corner *first_corner = &reshape_smooth_context->geometry.corners[face->start_corner_index];
  const int grid_index = first_corner->grid_index;

#ifndef NDEBUG
  for (int face_corner = 0; face_corner < face->num_corners; ++face_corner) {
    const int corner_index = face->start_corner_index + face_corner;
    const Corner *corner = &reshape_smooth_context->geometry.corners[corner_index];
    BLI_assert(corner->grid_index == grid_index);
  }
#endif

  return grid_index;
}

static GridCoord *vertex_grid_coord_with_grid_index(const Vertex *vertex, const int grid_index)
{
  for (int i = 0; i < vertex->num_grid_coords; ++i) {
    if (vertex->grid_coords[i].grid_index == grid_index) {
      return &vertex->grid_coords[i];
    }
  }
  return NULL;
}

/* Get grid coordinates which correspond to corners of the given face.
 * All the grid coordinates will be from the same grid index. */
static void grid_coords_from_face_vertices(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const Face *face,
    const GridCoord *grid_coords[])
{
  BLI_assert(face->num_corners == 4);

  const int grid_index = get_face_grid_index(reshape_smooth_context, face);
  BLI_assert(grid_index != -1);

  for (int i = 0; i < face->num_corners; ++i) {
    const int corner_index = face->start_corner_index + i;
    const Corner *corner = &reshape_smooth_context->geometry.corners[corner_index];
    grid_coords[i] = vertex_grid_coord_with_grid_index(corner->vertex, grid_index);
    BLI_assert(grid_coords[i] != NULL);
  }
}

static float lerp(float t, float a, float b)
{
  return (a + t * (b - a));
}

static void interpolate_grid_coord(GridCoord *result,
                                   const GridCoord *face_grid_coords[4],
                                   const float u,
                                   const float v)
{
  /*
   * v
   * ^
   * | (3) -------- (2)
   * |  |            |
   * |  |            |
   * |  |            |
   * |  |            |
   * | (0) -------- (1)
   * *--------------------------> u
   */

  const float u01 = lerp(u, face_grid_coords[0]->u, face_grid_coords[1]->u);
  const float u32 = lerp(u, face_grid_coords[3]->u, face_grid_coords[2]->u);

  const float v03 = lerp(v, face_grid_coords[0]->v, face_grid_coords[3]->v);
  const float v12 = lerp(v, face_grid_coords[1]->v, face_grid_coords[2]->v);

  result->grid_index = face_grid_coords[0]->grid_index;
  result->u = lerp(v, u01, u32);
  result->v = lerp(u, v03, v12);
}

static void foreach_toplevel_grid_coord_task(void *__restrict userdata_v,
                                             const int face_index,
                                             const TaskParallelTLS *__restrict UNUSED(tls))
{
  ForeachHighLevelCoordTaskData *data = userdata_v;

  const MultiresReshapeSmoothContext *reshape_smooth_context = data->reshape_smooth_context;
  const int inner_grid_size = data->inner_grid_size;
  const float inner_grid_size_1_inv = data->inner_grid_size_1_inv;

  const Face *face = &reshape_smooth_context->geometry.faces[face_index];
  const GridCoord *face_grid_coords[4];
  grid_coords_from_face_vertices(reshape_smooth_context, face, face_grid_coords);

  for (int y = 0; y < inner_grid_size; ++y) {
    const float ptex_v = (float)y * inner_grid_size_1_inv;
    for (int x = 0; x < inner_grid_size; ++x) {
      const float ptex_u = (float)x * inner_grid_size_1_inv;

      PTexCoord ptex_coord;
      ptex_coord.ptex_face_index = face_index;
      ptex_coord.u = ptex_u;
      ptex_coord.v = ptex_v;

      GridCoord grid_coord;
      interpolate_grid_coord(&grid_coord, face_grid_coords, ptex_u, ptex_v);

      data->callback(reshape_smooth_context, &ptex_coord, &grid_coord, data->callback_userdata_v);
    }
  }
}

static void foreach_toplevel_grid_coord(const MultiresReshapeSmoothContext *reshape_smooth_context,
                                        ForeachTopLevelGridCoordCallback callback,
                                        void *callback_userdata_v)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const int level_difference = (reshape_context->top.level - reshape_context->reshape.level);

  ForeachHighLevelCoordTaskData data;
  data.reshape_smooth_context = reshape_smooth_context;
  data.inner_grid_size = (1 << level_difference) + 1;
  data.inner_grid_size_1_inv = 1.0f / (float)(data.inner_grid_size - 1);
  data.callback = callback;
  data.callback_userdata_v = callback_userdata_v;

  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  parallel_range_settings.min_iter_per_thread = 1;

  const int num_faces = reshape_smooth_context->geometry.num_faces;
  BLI_task_parallel_range(
      0, num_faces, &data, foreach_toplevel_grid_coord_task, &parallel_range_settings);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generation of a topology information for OpenSubdiv converter
 *
 * Calculates vertices, their coordinates in the original grids, and connections of them so then
 * it's easy to create OpenSubdiv's topology refiner.
 * \{ */

static int get_reshape_level_resolution(const MultiresReshapeContext *reshape_context)
{
  return (1 << reshape_context->reshape.level) + 1;
}

/* Get crease which will be used for communication to OpenSubdiv topology.
 * Note that simple subdivision treats all base edges as infinitely sharp. */
static char get_effective_edge_crease_char(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const MEdge *base_edge)
{
  if (ELEM(reshape_smooth_context->smoothing_type,
           MULTIRES_SUBDIVIDE_LINEAR,
           MULTIRES_SUBDIVIDE_SIMPLE)) {
    return 255;
  }
  return base_edge->crease;
}

static void context_init(MultiresReshapeSmoothContext *reshape_smooth_context,
                         const MultiresReshapeContext *reshape_context,
                         const eMultiresSubdivideModeType mode)
{
  reshape_smooth_context->reshape_context = reshape_context;

  reshape_smooth_context->geometry.num_vertices = 0;
  reshape_smooth_context->geometry.vertices = NULL;

  reshape_smooth_context->geometry.max_edges = 0;
  reshape_smooth_context->geometry.num_edges = 0;
  reshape_smooth_context->geometry.edges = NULL;

  reshape_smooth_context->geometry.num_corners = 0;
  reshape_smooth_context->geometry.corners = NULL;

  reshape_smooth_context->geometry.num_faces = 0;
  reshape_smooth_context->geometry.faces = NULL;

  linear_grids_init(&reshape_smooth_context->linear_delta_grids);

  reshape_smooth_context->non_loose_base_edge_map = NULL;
  reshape_smooth_context->reshape_subdiv = NULL;
  reshape_smooth_context->base_surface_grids = NULL;

  reshape_smooth_context->smoothing_type = mode;
}

static void context_free_geometry(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  if (reshape_smooth_context->geometry.vertices != NULL) {
    for (int i = 0; i < reshape_smooth_context->geometry.num_vertices; ++i) {
      MEM_SAFE_FREE(reshape_smooth_context->geometry.vertices[i].grid_coords);
    }
  }
  MEM_SAFE_FREE(reshape_smooth_context->geometry.vertices);
  MEM_SAFE_FREE(reshape_smooth_context->geometry.corners);
  MEM_SAFE_FREE(reshape_smooth_context->geometry.faces);
  MEM_SAFE_FREE(reshape_smooth_context->geometry.edges);

  linear_grids_free(&reshape_smooth_context->linear_delta_grids);
}

static void context_free_subdiv(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  if (reshape_smooth_context->reshape_subdiv == NULL) {
    return;
  }
  BKE_subdiv_free(reshape_smooth_context->reshape_subdiv);
}

static void context_free(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  MEM_freeN(reshape_smooth_context->non_loose_base_edge_map);

  context_free_geometry(reshape_smooth_context);
  context_free_subdiv(reshape_smooth_context);
  base_surface_grids_free(reshape_smooth_context);
}

static bool foreach_topology_info(const SubdivForeachContext *foreach_context,
                                  const int num_vertices,
                                  const int num_edges,
                                  const int num_loops,
                                  const int num_polygons)
{
  MultiresReshapeSmoothContext *reshape_smooth_context = foreach_context->user_data;
  const int max_edges = reshape_smooth_context->smoothing_type == MULTIRES_SUBDIVIDE_LINEAR ?
                            num_edges :
                            reshape_smooth_context->geometry.max_edges;

  /* NOTE: Calloc so the counters are re-set to 0 "for free". */
  reshape_smooth_context->geometry.num_vertices = num_vertices;
  reshape_smooth_context->geometry.vertices = MEM_calloc_arrayN(
      sizeof(Vertex), num_vertices, "smooth vertices");

  reshape_smooth_context->geometry.max_edges = max_edges;
  reshape_smooth_context->geometry.edges = MEM_malloc_arrayN(
      sizeof(Edge), max_edges, "smooth edges");

  reshape_smooth_context->geometry.num_corners = num_loops;
  reshape_smooth_context->geometry.corners = MEM_malloc_arrayN(
      sizeof(Corner), num_loops, "smooth corners");

  reshape_smooth_context->geometry.num_faces = num_polygons;
  reshape_smooth_context->geometry.faces = MEM_malloc_arrayN(
      sizeof(Face), num_polygons, "smooth faces");

  return true;
}

static void foreach_single_vertex(const SubdivForeachContext *foreach_context,
                                  const GridCoord *grid_coord,
                                  const int subdiv_vertex_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = foreach_context->user_data;

  BLI_assert(subdiv_vertex_index < reshape_smooth_context->geometry.num_vertices);

  Vertex *vertex = &reshape_smooth_context->geometry.vertices[subdiv_vertex_index];

  vertex->grid_coords = MEM_reallocN(vertex->grid_coords,
                                     sizeof(Vertex) * (vertex->num_grid_coords + 1));
  vertex->grid_coords[vertex->num_grid_coords] = *grid_coord;
  ++vertex->num_grid_coords;
}

/* TODO(sergey): De-duplicate with similar function in multires_reshape_vertcos.c */
static void foreach_vertex(const SubdivForeachContext *foreach_context,
                           const PTexCoord *ptex_coord,
                           const int subdiv_vertex_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = foreach_context->user_data;
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const GridCoord grid_coord = multires_reshape_ptex_coord_to_grid(reshape_context, ptex_coord);
  const int face_index = multires_reshape_grid_to_face_index(reshape_context,
                                                             grid_coord.grid_index);

  const Mesh *base_mesh = reshape_context->base_mesh;
  const MPoly *base_poly = &base_mesh->mpoly[face_index];
  const int num_corners = base_poly->totloop;
  const int start_grid_index = reshape_context->face_start_grid_index[face_index];
  const int corner = grid_coord.grid_index - start_grid_index;

  if (grid_coord.u == 0.0f && grid_coord.v == 0.0f) {
    for (int current_corner = 0; current_corner < num_corners; ++current_corner) {
      GridCoord corner_grid_coord = grid_coord;
      corner_grid_coord.grid_index = start_grid_index + current_corner;
      foreach_single_vertex(foreach_context, &corner_grid_coord, subdiv_vertex_index);
    }
    return;
  }

  foreach_single_vertex(foreach_context, &grid_coord, subdiv_vertex_index);

  if (grid_coord.u == 0.0f) {
    GridCoord prev_grid_coord;
    prev_grid_coord.grid_index = start_grid_index + ((corner + num_corners - 1) % num_corners);
    prev_grid_coord.u = grid_coord.v;
    prev_grid_coord.v = 0.0f;

    foreach_single_vertex(foreach_context, &prev_grid_coord, subdiv_vertex_index);
  }

  if (grid_coord.v == 0.0f) {
    GridCoord next_grid_coord;
    next_grid_coord.grid_index = start_grid_index + ((corner + 1) % num_corners);
    next_grid_coord.u = 0.0f;
    next_grid_coord.v = grid_coord.u;

    foreach_single_vertex(foreach_context, &next_grid_coord, subdiv_vertex_index);
  }
}

static void foreach_vertex_inner(const struct SubdivForeachContext *foreach_context,
                                 void *UNUSED(tls),
                                 const int ptex_face_index,
                                 const float ptex_face_u,
                                 const float ptex_face_v,
                                 const int UNUSED(coarse_poly_index),
                                 const int UNUSED(coarse_corner),
                                 const int subdiv_vertex_index)
{
  const PTexCoord ptex_coord = {
      .ptex_face_index = ptex_face_index,
      .u = ptex_face_u,
      .v = ptex_face_v,
  };
  foreach_vertex(foreach_context, &ptex_coord, subdiv_vertex_index);
}

static void foreach_vertex_every_corner(const struct SubdivForeachContext *foreach_context,
                                        void *UNUSED(tls_v),
                                        const int ptex_face_index,
                                        const float ptex_face_u,
                                        const float ptex_face_v,
                                        const int UNUSED(coarse_vertex_index),
                                        const int UNUSED(coarse_face_index),
                                        const int UNUSED(coarse_face_corner),
                                        const int subdiv_vertex_index)
{
  const PTexCoord ptex_coord = {
      .ptex_face_index = ptex_face_index,
      .u = ptex_face_u,
      .v = ptex_face_v,
  };
  foreach_vertex(foreach_context, &ptex_coord, subdiv_vertex_index);
}

static void foreach_vertex_every_edge(const struct SubdivForeachContext *foreach_context,
                                      void *UNUSED(tls_v),
                                      const int ptex_face_index,
                                      const float ptex_face_u,
                                      const float ptex_face_v,
                                      const int UNUSED(coarse_edge_index),
                                      const int UNUSED(coarse_face_index),
                                      const int UNUSED(coarse_face_corner),
                                      const int subdiv_vertex_index)
{
  const PTexCoord ptex_coord = {
      .ptex_face_index = ptex_face_index,
      .u = ptex_face_u,
      .v = ptex_face_v,
  };
  foreach_vertex(foreach_context, &ptex_coord, subdiv_vertex_index);
}

static void foreach_loop(const struct SubdivForeachContext *foreach_context,
                         void *UNUSED(tls),
                         const int UNUSED(ptex_face_index),
                         const float UNUSED(ptex_face_u),
                         const float UNUSED(ptex_face_v),
                         const int UNUSED(coarse_loop_index),
                         const int coarse_poly_index,
                         const int coarse_corner,
                         const int subdiv_loop_index,
                         const int subdiv_vertex_index,
                         const int UNUSED(subdiv_edge_index))
{
  MultiresReshapeSmoothContext *reshape_smooth_context = foreach_context->user_data;
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  BLI_assert(subdiv_loop_index < reshape_smooth_context->geometry.num_corners);

  Corner *corner = &reshape_smooth_context->geometry.corners[subdiv_loop_index];
  corner->vertex = &reshape_smooth_context->geometry.vertices[subdiv_vertex_index];

  const int first_grid_index = reshape_context->face_start_grid_index[coarse_poly_index];
  corner->grid_index = first_grid_index + coarse_corner;
}

static void foreach_poly(const SubdivForeachContext *foreach_context,
                         void *UNUSED(tls),
                         const int UNUSED(coarse_poly_index),
                         const int subdiv_poly_index,
                         const int start_loop_index,
                         const int num_loops)
{
  MultiresReshapeSmoothContext *reshape_smooth_context = foreach_context->user_data;

  BLI_assert(subdiv_poly_index < reshape_smooth_context->geometry.num_faces);

  Face *face = &reshape_smooth_context->geometry.faces[subdiv_poly_index];
  face->start_corner_index = start_loop_index;
  face->num_corners = num_loops;
}

static void foreach_vertex_of_loose_edge(const struct SubdivForeachContext *foreach_context,
                                         void *UNUSED(tls),
                                         const int UNUSED(coarse_edge_index),
                                         const float UNUSED(u),
                                         const int vertex_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = foreach_context->user_data;
  Vertex *vertex = &reshape_smooth_context->geometry.vertices[vertex_index];

  if (vertex->num_grid_coords != 0) {
    vertex->is_infinite_sharp = true;
  }
}

static void store_edge(MultiresReshapeSmoothContext *reshape_smooth_context,
                       const int subdiv_v1,
                       const int subdiv_v2,
                       const char crease)
{
  /* This is a bit overhead to use atomics in such a simple function called from many threads,
   * but this allows to save quite measurable amount of memory. */
  const int edge_index = atomic_fetch_and_add_z(&reshape_smooth_context->geometry.num_edges, 1);
  BLI_assert(edge_index < reshape_smooth_context->geometry.max_edges);

  Edge *edge = &reshape_smooth_context->geometry.edges[edge_index];
  edge->v1 = subdiv_v1;
  edge->v2 = subdiv_v2;
  edge->sharpness = BKE_subdiv_edge_crease_to_sharpness_char(crease);
}

static void foreach_edge(const struct SubdivForeachContext *foreach_context,
                         void *UNUSED(tls),
                         const int coarse_edge_index,
                         const int UNUSED(subdiv_edge_index),
                         const int subdiv_v1,
                         const int subdiv_v2)
{
  MultiresReshapeSmoothContext *reshape_smooth_context = foreach_context->user_data;
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  if (reshape_smooth_context->smoothing_type == MULTIRES_SUBDIVIDE_LINEAR) {
    store_edge(reshape_smooth_context, subdiv_v1, subdiv_v2, (char)255);
    return;
  }

  /* Ignore all inner face edges as they have sharpness of zero when using Catmull-Clark mode. In
   * simple mode, all edges have maximum sharpness, so they can't be skipped. */
  if (coarse_edge_index == ORIGINDEX_NONE &&
      reshape_smooth_context->smoothing_type != MULTIRES_SUBDIVIDE_SIMPLE) {
    return;
  }
  /* Ignore all loose edges as well, as they are not communicated to the OpenSubdiv. */
  if (!BLI_BITMAP_TEST_BOOL(reshape_smooth_context->non_loose_base_edge_map, coarse_edge_index)) {
    return;
  }
  /* Edges without crease are to be ignored as well. */
  const Mesh *base_mesh = reshape_context->base_mesh;
  const MEdge *base_edge = &base_mesh->medge[coarse_edge_index];
  const char crease = get_effective_edge_crease_char(reshape_smooth_context, base_edge);
  if (crease == 0) {
    return;
  }
  store_edge(reshape_smooth_context, subdiv_v1, subdiv_v2, crease);
}

static void geometry_init_loose_information(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const Mesh *base_mesh = reshape_context->base_mesh;
  const MPoly *base_mpoly = base_mesh->mpoly;
  const MLoop *base_mloop = base_mesh->mloop;
  const MEdge *base_edge = base_mesh->medge;

  reshape_smooth_context->non_loose_base_edge_map = BLI_BITMAP_NEW(base_mesh->totedge,
                                                                   "non_loose_base_edge_map");

  int num_used_edges = 0;
  for (int poly_index = 0; poly_index < base_mesh->totpoly; ++poly_index) {
    const MPoly *base_poly = &base_mpoly[poly_index];
    for (int corner = 0; corner < base_poly->totloop; corner++) {
      const MLoop *loop = &base_mloop[base_poly->loopstart + corner];
      if (!BLI_BITMAP_TEST_BOOL(reshape_smooth_context->non_loose_base_edge_map, loop->e)) {
        BLI_BITMAP_ENABLE(reshape_smooth_context->non_loose_base_edge_map, loop->e);

        const char crease = get_effective_edge_crease_char(reshape_smooth_context,
                                                           &base_edge[loop->e]);
        if (crease != 0) {
          ++num_used_edges;
        }
      }
    }
  }

  const int resolution = get_reshape_level_resolution(reshape_context);
  const int num_subdiv_vertices_per_base_edge = resolution - 2;
  reshape_smooth_context->geometry.max_edges = num_used_edges *
                                               (num_subdiv_vertices_per_base_edge + 1);
}

static void geometry_create(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  SubdivForeachContext foreach_context = {
      .topology_info = foreach_topology_info,
      .vertex_inner = foreach_vertex_inner,
      .vertex_every_corner = foreach_vertex_every_corner,
      .vertex_every_edge = foreach_vertex_every_edge,
      .loop = foreach_loop,
      .poly = foreach_poly,
      .vertex_of_loose_edge = foreach_vertex_of_loose_edge,
      .edge = foreach_edge,
      .user_data = reshape_smooth_context,
  };

  geometry_init_loose_information(reshape_smooth_context);

  SubdivToMeshSettings mesh_settings;
  mesh_settings.resolution = get_reshape_level_resolution(reshape_context);
  mesh_settings.use_optimal_display = false;

  /* TODO(sergey): Tell the foreach() to ignore loose vertices. */
  BKE_subdiv_foreach_subdiv_geometry(
      reshape_context->subdiv, &foreach_context, &mesh_settings, reshape_context->base_mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generation of OpenSubdiv evaluator for topology created form reshape level
 * \{ */

static OpenSubdiv_SchemeType get_scheme_type(const OpenSubdiv_Converter *UNUSED(converter))
{
  return OSD_SCHEME_CATMARK;
}

static OpenSubdiv_VtxBoundaryInterpolation get_vtx_boundary_interpolation(
    const struct OpenSubdiv_Converter *converter)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const SubdivSettings *settings = &reshape_context->subdiv->settings;

  return BKE_subdiv_converter_vtx_boundary_interpolation_from_settings(settings);
}

static OpenSubdiv_FVarLinearInterpolation get_fvar_linear_interpolation(
    const OpenSubdiv_Converter *converter)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const SubdivSettings *settings = &reshape_context->subdiv->settings;

  return BKE_subdiv_converter_fvar_linear_from_settings(settings);
}

static bool specifies_full_topology(const OpenSubdiv_Converter *UNUSED(converter))
{
  return false;
}

static int get_num_faces(const OpenSubdiv_Converter *converter)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;

  return reshape_smooth_context->geometry.num_faces;
}

static int get_num_vertices(const OpenSubdiv_Converter *converter)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;

  return reshape_smooth_context->geometry.num_vertices;
}

static int get_num_face_vertices(const OpenSubdiv_Converter *converter, int face_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;
  BLI_assert(face_index < reshape_smooth_context->geometry.num_faces);

  const Face *face = &reshape_smooth_context->geometry.faces[face_index];
  return face->num_corners;
}

static void get_face_vertices(const OpenSubdiv_Converter *converter,
                              int face_index,
                              int *face_vertices)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;
  BLI_assert(face_index < reshape_smooth_context->geometry.num_faces);

  const Face *face = &reshape_smooth_context->geometry.faces[face_index];

  for (int i = 0; i < face->num_corners; ++i) {
    const int corner_index = face->start_corner_index + i;
    const Corner *corner = &reshape_smooth_context->geometry.corners[corner_index];
    face_vertices[i] = corner->vertex - reshape_smooth_context->geometry.vertices;
  }
}

static int get_num_edges(const struct OpenSubdiv_Converter *converter)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;
  return reshape_smooth_context->geometry.num_edges;
}

static void get_edge_vertices(const OpenSubdiv_Converter *converter,
                              const int edge_index,
                              int edge_vertices[2])
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;
  BLI_assert(edge_index < reshape_smooth_context->geometry.num_edges);

  const Edge *edge = &reshape_smooth_context->geometry.edges[edge_index];
  edge_vertices[0] = edge->v1;
  edge_vertices[1] = edge->v2;
}

static float get_edge_sharpness(const OpenSubdiv_Converter *converter, const int edge_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;
  BLI_assert(edge_index < reshape_smooth_context->geometry.num_edges);

  const Edge *edge = &reshape_smooth_context->geometry.edges[edge_index];
  return edge->sharpness;
}

static bool is_infinite_sharp_vertex(const OpenSubdiv_Converter *converter, int vertex_index)
{
  const MultiresReshapeSmoothContext *reshape_smooth_context = converter->user_data;

  BLI_assert(vertex_index < reshape_smooth_context->geometry.num_vertices);

  const Vertex *vertex = &reshape_smooth_context->geometry.vertices[vertex_index];
  return vertex->is_infinite_sharp;
}

static void converter_init(const MultiresReshapeSmoothContext *reshape_smooth_context,
                           OpenSubdiv_Converter *converter)
{
  converter->getSchemeType = get_scheme_type;
  converter->getVtxBoundaryInterpolation = get_vtx_boundary_interpolation;
  converter->getFVarLinearInterpolation = get_fvar_linear_interpolation;
  converter->specifiesFullTopology = specifies_full_topology;

  converter->getNumFaces = get_num_faces;
  converter->getNumEdges = get_num_edges;
  converter->getNumVertices = get_num_vertices;

  converter->getNumFaceVertices = get_num_face_vertices;
  converter->getFaceVertices = get_face_vertices;
  converter->getFaceEdges = NULL;

  converter->getEdgeVertices = get_edge_vertices;
  converter->getNumEdgeFaces = NULL;
  converter->getEdgeFaces = NULL;
  converter->getEdgeSharpness = get_edge_sharpness;

  converter->getNumVertexEdges = NULL;
  converter->getVertexEdges = NULL;
  converter->getNumVertexFaces = NULL;
  converter->getVertexFaces = NULL;
  converter->isInfiniteSharpVertex = is_infinite_sharp_vertex;
  converter->getVertexSharpness = NULL;

  converter->getNumUVLayers = NULL;
  converter->precalcUVLayer = NULL;
  converter->finishUVLayer = NULL;
  converter->getNumUVCoordinates = NULL;
  converter->getFaceCornerUVIndex = NULL;

  converter->freeUserData = NULL;

  converter->user_data = (void *)reshape_smooth_context;
}

/* Create subdiv descriptor created for topology at a reshape level,  */
static void reshape_subdiv_create(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const SubdivSettings *settings = &reshape_context->subdiv->settings;

  OpenSubdiv_Converter converter;
  converter_init(reshape_smooth_context, &converter);

  Subdiv *reshape_subdiv = BKE_subdiv_new_from_converter(settings, &converter);
  BKE_subdiv_eval_begin(reshape_subdiv);

  reshape_smooth_context->reshape_subdiv = reshape_subdiv;

  BKE_subdiv_converter_free(&converter);
}

/* Callback to provide coarse position for subdivision surface topology at a reshape level. */
typedef void(ReshapeSubdivCoarsePositionCb)(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const Vertex *vertex,
    float r_P[3]);

/* Refine subdivision surface topology at a reshape level for new coarse vertices positions.  */
static void reshape_subdiv_refine(const MultiresReshapeSmoothContext *reshape_smooth_context,
                                  ReshapeSubdivCoarsePositionCb coarse_position_cb)
{
  Subdiv *reshape_subdiv = reshape_smooth_context->reshape_subdiv;

  /* TODO(sergey): For non-trivial coarse_position_cb we should multi-thread this loop. */

  const int num_vertices = reshape_smooth_context->geometry.num_vertices;
  for (int i = 0; i < num_vertices; ++i) {
    const Vertex *vertex = &reshape_smooth_context->geometry.vertices[i];
    float P[3];
    coarse_position_cb(reshape_smooth_context, vertex, P);
    reshape_subdiv->evaluator->setCoarsePositions(reshape_subdiv->evaluator, P, i, 1);
  }
  reshape_subdiv->evaluator->refine(reshape_subdiv->evaluator);
}

BLI_INLINE const GridCoord *reshape_subdiv_refine_vertex_grid_coord(const Vertex *vertex)
{
  if (vertex->num_grid_coords == 0) {
    /* This is a loose vertex, the coordinate is not important. */
    /* TODO(sergey): Once the subdiv_foreach() supports properly ignoring loose elements this
     * should become an assert instead. */
    return NULL;
  }
  /* NOTE: All grid coordinates will point to the same object position, so can be simple and use
   * first grid coordinate. */
  return &vertex->grid_coords[0];
}

/* Version of reshape_subdiv_refine() which uses coarse position from original grids. */
static void reshape_subdiv_refine_orig_P(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const Vertex *vertex, float r_P[3])
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const GridCoord *grid_coord = reshape_subdiv_refine_vertex_grid_coord(vertex);

  /* Check whether this is a loose vertex. */
  if (grid_coord == NULL) {
    zero_v3(r_P);
    return;
  }

  float limit_P[3];
  float tangent_matrix[3][3];
  multires_reshape_evaluate_limit_at_grid(reshape_context, grid_coord, limit_P, tangent_matrix);

  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  float D[3];
  mul_v3_m3v3(D, tangent_matrix, orig_grid_element.displacement);

  add_v3_v3v3(r_P, limit_P, D);
}
static void reshape_subdiv_refine_orig(const MultiresReshapeSmoothContext *reshape_smooth_context)
{
  reshape_subdiv_refine(reshape_smooth_context, reshape_subdiv_refine_orig_P);
}

/* Version of reshape_subdiv_refine() which uses coarse position from final grids. */
static void reshape_subdiv_refine_final_P(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const Vertex *vertex, float r_P[3])
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const GridCoord *grid_coord = reshape_subdiv_refine_vertex_grid_coord(vertex);

  /* Check whether this is a loose vertex. */
  if (grid_coord == NULL) {
    zero_v3(r_P);
    return;
  }

  const ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(
      reshape_context, grid_coord);

  /* NOTE: At this point in reshape/propagate pipeline grid displacement is actually storing object
   * vertices coordinates. */
  copy_v3_v3(r_P, grid_element.displacement);
}
static void reshape_subdiv_refine_final(const MultiresReshapeSmoothContext *reshape_smooth_context)
{
  reshape_subdiv_refine(reshape_smooth_context, reshape_subdiv_refine_final_P);
}

static void reshape_subdiv_evaluate_limit_at_grid(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const PTexCoord *ptex_coord,
    const GridCoord *grid_coord,
    float limit_P[3],
    float r_tangent_matrix[3][3])
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  float dPdu[3], dPdv[3];
  BKE_subdiv_eval_limit_point_and_derivatives(reshape_smooth_context->reshape_subdiv,
                                              ptex_coord->ptex_face_index,
                                              ptex_coord->u,
                                              ptex_coord->v,
                                              limit_P,
                                              dPdu,
                                              dPdv);

  const int corner = multires_reshape_grid_to_corner(reshape_context, grid_coord->grid_index);
  BKE_multires_construct_tangent_matrix(r_tangent_matrix, dPdu, dPdv, corner);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Linearly interpolated data
 * \{ */

static LinearGridElement linear_grid_element_orig_get(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const GridCoord *grid_coord)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  LinearGridElement linear_grid_element;
  linear_grid_element_init(&linear_grid_element);

  linear_grid_element.mask = orig_grid_element.mask;

  return linear_grid_element;
}

static LinearGridElement linear_grid_element_final_get(
    const MultiresReshapeSmoothContext *reshape_smooth_context, const GridCoord *grid_coord)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const ReshapeGridElement final_grid_element = multires_reshape_grid_element_for_grid_coord(
      reshape_context, grid_coord);

  LinearGridElement linear_grid_element;
  linear_grid_element_init(&linear_grid_element);

  if (final_grid_element.mask != NULL) {
    linear_grid_element.mask = *final_grid_element.mask;
  }

  return linear_grid_element;
}

/* Interpolate difference of the linear data.
 *
 * Will access final data and original data at the grid elements at the reshape level,
 * calculate difference between final and original, and linearly interpolate to get value at the
 * top level. */
static void linear_grid_element_delta_interpolate(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const GridCoord *grid_coord,
    LinearGridElement *result)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  const int reshape_level = reshape_context->reshape.level;
  const int reshape_level_grid_size = BKE_subdiv_grid_size_from_level(reshape_level);
  const int reshape_level_grid_size_1 = reshape_level_grid_size - 1;
  const float reshape_level_grid_size_1_inv = 1.0f / (float)(reshape_level_grid_size_1);

  const float x_f = grid_coord->u * reshape_level_grid_size_1;
  const float y_f = grid_coord->v * reshape_level_grid_size_1;

  const int x_i = x_f;
  const int y_i = y_f;
  const int x_n_i = (x_i == reshape_level_grid_size - 1) ? (x_i) : (x_i + 1);
  const int y_n_i = (y_i == reshape_level_grid_size - 1) ? (y_i) : (y_i + 1);

  const int corners_int_coords[4][2] = {{x_i, y_i}, {x_n_i, y_i}, {x_n_i, y_n_i}, {x_i, y_n_i}};

  LinearGridElement corner_elements[4];
  for (int i = 0; i < 4; ++i) {
    GridCoord corner_grid_coord;
    corner_grid_coord.grid_index = grid_coord->grid_index;
    corner_grid_coord.u = corners_int_coords[i][0] * reshape_level_grid_size_1_inv;
    corner_grid_coord.v = corners_int_coords[i][1] * reshape_level_grid_size_1_inv;

    const LinearGridElement orig_element = linear_grid_element_orig_get(reshape_smooth_context,
                                                                        &corner_grid_coord);
    const LinearGridElement final_element = linear_grid_element_final_get(reshape_smooth_context,
                                                                          &corner_grid_coord);
    linear_grid_element_sub(&corner_elements[i], &final_element, &orig_element);
  }

  const float u = x_f - x_i;
  const float v = y_f - y_i;
  const float weights[4] = {(1.0f - u) * (1.0f - v), u * (1.0f - v), u * v, (1.0f - u) * v};

  linear_grid_element_interpolate(result, corner_elements, weights);
}

static void evaluate_linear_delta_grids_callback(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const PTexCoord *UNUSED(ptex_coord),
    const GridCoord *grid_coord,
    void *UNUSED(userdata_v))
{
  LinearGridElement *linear_delta_element = linear_grid_element_get(
      &reshape_smooth_context->linear_delta_grids, grid_coord);

  linear_grid_element_delta_interpolate(reshape_smooth_context, grid_coord, linear_delta_element);
}

static void evaluate_linear_delta_grids(MultiresReshapeSmoothContext *reshape_smooth_context)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  const int num_grids = reshape_context->num_grids;
  const int top_level = reshape_context->top.level;

  linear_grids_allocate(&reshape_smooth_context->linear_delta_grids, num_grids, top_level);

  foreach_toplevel_grid_coord(reshape_smooth_context, evaluate_linear_delta_grids_callback, NULL);
}

static void propagate_linear_data_delta(const MultiresReshapeSmoothContext *reshape_smooth_context,
                                        ReshapeGridElement *final_grid_element,
                                        const GridCoord *grid_coord)
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  LinearGridElement *linear_delta_element = linear_grid_element_get(
      &reshape_smooth_context->linear_delta_grids, grid_coord);

  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  if (final_grid_element->mask != NULL) {
    *final_grid_element->mask = clamp_f(
        orig_grid_element.mask + linear_delta_element->mask, 0.0f, 1.0f);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation of base surface
 * \{ */

static void evaluate_base_surface_grids_callback(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const PTexCoord *ptex_coord,
    const GridCoord *grid_coord,
    void *UNUSED(userdata_v))
{
  float limit_P[3];
  float tangent_matrix[3][3];
  reshape_subdiv_evaluate_limit_at_grid(
      reshape_smooth_context, ptex_coord, grid_coord, limit_P, tangent_matrix);

  base_surface_grids_write(reshape_smooth_context, grid_coord, limit_P, tangent_matrix);
}

static void evaluate_base_surface_grids(const MultiresReshapeSmoothContext *reshape_smooth_context)
{
  foreach_toplevel_grid_coord(reshape_smooth_context, evaluate_base_surface_grids_callback, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation of new surface
 * \{ */

/* Evaluate final position of the original (pre-sculpt-edit) point position at a given grid
 * coordinate. */
static void evaluate_final_original_point(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const GridCoord *grid_coord,
    float r_orig_final_P[3])
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  /* Element of an original MDISPS grid) */
  const ReshapeConstGridElement orig_grid_element =
      multires_reshape_orig_grid_element_for_grid_coord(reshape_context, grid_coord);

  /* Limit surface of the base mesh. */
  float base_mesh_limit_P[3];
  float base_mesh_tangent_matrix[3][3];
  multires_reshape_evaluate_limit_at_grid(
      reshape_context, grid_coord, base_mesh_limit_P, base_mesh_tangent_matrix);

  /* Convert original displacement from tangent space to object space. */
  float orig_displacement[3];
  mul_v3_m3v3(orig_displacement, base_mesh_tangent_matrix, orig_grid_element.displacement);

  /* Final point = limit surface + displacement. */
  add_v3_v3v3(r_orig_final_P, base_mesh_limit_P, orig_displacement);
}

static void evaluate_higher_grid_positions_with_details_callback(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const PTexCoord *ptex_coord,
    const GridCoord *grid_coord,
    void *UNUSED(userdata_v))
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;

  /* Position of the original veretx at top level. */
  float orig_final_P[3];
  evaluate_final_original_point(reshape_smooth_context, grid_coord, orig_final_P);

  /* Original surface point on sculpt level (sculpt level before edits in sculpt mode). */
  const SurfacePoint *orig_sculpt_point = base_surface_grids_read(reshape_smooth_context,
                                                                  grid_coord);

  /* Difference between original top level and original sculpt level in object space. */
  float original_detail_delta[3];
  sub_v3_v3v3(original_detail_delta, orig_final_P, orig_sculpt_point->P);

  /* Difference between original top level and original sculpt level in tangent space of original
   * sculpt level. */
  float original_detail_delta_tangent[3];
  float original_sculpt_tangent_matrix_inv[3][3];
  invert_m3_m3(original_sculpt_tangent_matrix_inv, orig_sculpt_point->tangent_matrix);
  mul_v3_m3v3(
      original_detail_delta_tangent, original_sculpt_tangent_matrix_inv, original_detail_delta);

  /* Limit surface of smoothed (subdivided) edited sculpt level. */
  float smooth_limit_P[3];
  float smooth_tangent_matrix[3][3];
  reshape_subdiv_evaluate_limit_at_grid(
      reshape_smooth_context, ptex_coord, grid_coord, smooth_limit_P, smooth_tangent_matrix);

  /* Add original detail to the smoothed surface. */
  float smooth_delta[3];
  mul_v3_m3v3(smooth_delta, smooth_tangent_matrix, original_detail_delta_tangent);

  /* Grid element of the result.
   *
   * NOTE: Displacement is storing object space coordinate. */
  ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(reshape_context,
                                                                                 grid_coord);

  add_v3_v3v3(grid_element.displacement, smooth_limit_P, smooth_delta);

  /* Propagate non-coordinate data. */
  propagate_linear_data_delta(reshape_smooth_context, &grid_element, grid_coord);
}

static void evaluate_higher_grid_positions_with_details(
    const MultiresReshapeSmoothContext *reshape_smooth_context)
{
  foreach_toplevel_grid_coord(
      reshape_smooth_context, evaluate_higher_grid_positions_with_details_callback, NULL);
}

static void evaluate_higher_grid_positions_callback(
    const MultiresReshapeSmoothContext *reshape_smooth_context,
    const PTexCoord *ptex_coord,
    const GridCoord *grid_coord,
    void *UNUSED(userdata_v))
{
  const MultiresReshapeContext *reshape_context = reshape_smooth_context->reshape_context;
  Subdiv *reshape_subdiv = reshape_smooth_context->reshape_subdiv;

  ReshapeGridElement grid_element = multires_reshape_grid_element_for_grid_coord(reshape_context,
                                                                                 grid_coord);

  /* Surface. */
  float P[3];
  BKE_subdiv_eval_limit_point(
      reshape_subdiv, ptex_coord->ptex_face_index, ptex_coord->u, ptex_coord->v, P);

  copy_v3_v3(grid_element.displacement, P);

  /* Propagate non-coordinate data. */
  propagate_linear_data_delta(reshape_smooth_context, &grid_element, grid_coord);
}

static void evaluate_higher_grid_positions(
    const MultiresReshapeSmoothContext *reshape_smooth_context)
{
  foreach_toplevel_grid_coord(
      reshape_smooth_context, evaluate_higher_grid_positions_callback, NULL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Entry point
 * \{ */

void multires_reshape_smooth_object_grids_with_details(
    const MultiresReshapeContext *reshape_context)
{
  const int level_difference = (reshape_context->top.level - reshape_context->reshape.level);
  if (level_difference == 0) {
    /* Early output. */
    return;
  }

  MultiresReshapeSmoothContext reshape_smooth_context;
  if (reshape_context->subdiv->settings.is_simple) {
    context_init(&reshape_smooth_context, reshape_context, MULTIRES_SUBDIVIDE_SIMPLE);
  }
  else {
    context_init(&reshape_smooth_context, reshape_context, MULTIRES_SUBDIVIDE_CATMULL_CLARK);
  }

  geometry_create(&reshape_smooth_context);
  evaluate_linear_delta_grids(&reshape_smooth_context);

  reshape_subdiv_create(&reshape_smooth_context);

  base_surface_grids_allocate(&reshape_smooth_context);
  reshape_subdiv_refine_orig(&reshape_smooth_context);
  evaluate_base_surface_grids(&reshape_smooth_context);

  reshape_subdiv_refine_final(&reshape_smooth_context);
  evaluate_higher_grid_positions_with_details(&reshape_smooth_context);

  context_free(&reshape_smooth_context);
}

void multires_reshape_smooth_object_grids(const MultiresReshapeContext *reshape_context,
                                          const eMultiresSubdivideModeType mode)
{
  const int level_difference = (reshape_context->top.level - reshape_context->reshape.level);
  if (level_difference == 0) {
    /* Early output. */
    return;
  }

  MultiresReshapeSmoothContext reshape_smooth_context;
  context_init(&reshape_smooth_context, reshape_context, mode);

  geometry_create(&reshape_smooth_context);
  evaluate_linear_delta_grids(&reshape_smooth_context);

  reshape_subdiv_create(&reshape_smooth_context);

  reshape_subdiv_refine_final(&reshape_smooth_context);
  evaluate_higher_grid_positions(&reshape_smooth_context);

  context_free(&reshape_smooth_context);
}

/** \} */
