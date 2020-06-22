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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_ccg.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_bits.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BKE_DerivedMesh.h"
#include "BKE_ccg.h"
#include "BKE_mesh.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_eval.h"

#include "opensubdiv_topology_refiner_capi.h"

/* -------------------------------------------------------------------- */
/** \name Various forward declarations
 * \{ */

static void subdiv_ccg_average_all_boundaries_and_corners(SubdivCCG *subdiv_ccg, CCGKey *key);

static void subdiv_ccg_average_inner_face_grids(SubdivCCG *subdiv_ccg,
                                                CCGKey *key,
                                                SubdivCCGFace *face);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generally useful internal helpers
 * \{ */

/* Number of floats in per-vertex elements.  */
static int num_element_float_get(const SubdivCCG *subdiv_ccg)
{
  /* We always have 3 floats for coordinate. */
  int num_floats = 3;
  if (subdiv_ccg->has_normal) {
    num_floats += 3;
  }
  if (subdiv_ccg->has_mask) {
    num_floats += 1;
  }
  return num_floats;
}

/* Per-vertex element size in bytes. */
static int element_size_bytes_get(const SubdivCCG *subdiv_ccg)
{
  return sizeof(float) * num_element_float_get(subdiv_ccg);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal helpers for CCG creation
 * \{ */

static void subdiv_ccg_init_layers(SubdivCCG *subdiv_ccg, const SubdivToCCGSettings *settings)
{
  /* CCG always contains coordinates. Rest of layers are coming after them. */
  int layer_offset = sizeof(float) * 3;
  /* Mask. */
  if (settings->need_mask) {
    subdiv_ccg->has_mask = true;
    subdiv_ccg->mask_offset = layer_offset;
    layer_offset += sizeof(float);
  }
  else {
    subdiv_ccg->has_mask = false;
    subdiv_ccg->mask_offset = -1;
  }
  /* Normals.
   *
   * NOTE: Keep them at the end, matching old CCGDM. Doesn't really matter
   * here, but some other area might in theory depend memory layout. */
  if (settings->need_normal) {
    subdiv_ccg->has_normal = true;
    subdiv_ccg->normal_offset = layer_offset;
    layer_offset += sizeof(float) * 3;
  }
  else {
    subdiv_ccg->has_normal = false;
    subdiv_ccg->normal_offset = -1;
  }
}

/* TODO(sergey): Make it more accessible function. */
static int topology_refiner_count_face_corners(OpenSubdiv_TopologyRefiner *topology_refiner)
{
  const int num_faces = topology_refiner->getNumFaces(topology_refiner);
  int num_corners = 0;
  for (int face_index = 0; face_index < num_faces; face_index++) {
    num_corners += topology_refiner->getNumFaceVertices(topology_refiner, face_index);
  }
  return num_corners;
}

/* NOTE: Grid size and layer flags are to be filled in before calling this
 * function. */
static void subdiv_ccg_alloc_elements(SubdivCCG *subdiv_ccg, Subdiv *subdiv)
{
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  const int element_size = element_size_bytes_get(subdiv_ccg);
  /* Allocate memory for surface grids. */
  const int num_faces = topology_refiner->getNumFaces(topology_refiner);
  const int num_grids = topology_refiner_count_face_corners(topology_refiner);
  const int grid_size = BKE_subdiv_grid_size_from_level(subdiv_ccg->level);
  const int grid_area = grid_size * grid_size;
  subdiv_ccg->grid_element_size = element_size;
  subdiv_ccg->num_grids = num_grids;
  subdiv_ccg->grids = MEM_calloc_arrayN(num_grids, sizeof(CCGElem *), "subdiv ccg grids");
  subdiv_ccg->grids_storage = MEM_calloc_arrayN(
      num_grids, ((size_t)grid_area) * element_size, "subdiv ccg grids storage");
  const size_t grid_size_in_bytes = (size_t)grid_area * element_size;
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    const size_t grid_offset = grid_size_in_bytes * grid_index;
    subdiv_ccg->grids[grid_index] = (CCGElem *)&subdiv_ccg->grids_storage[grid_offset];
  }
  /* Grid material flags. */
  subdiv_ccg->grid_flag_mats = MEM_calloc_arrayN(
      num_grids, sizeof(DMFlagMat), "ccg grid material flags");
  /* Grid hidden flags. */
  subdiv_ccg->grid_hidden = MEM_calloc_arrayN(
      num_grids, sizeof(BLI_bitmap *), "ccg grid material flags");
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    subdiv_ccg->grid_hidden[grid_index] = BLI_BITMAP_NEW(grid_area, "ccg grid hidden");
  }
  /* TODO(sergey): Allocate memory for loose elements. */
  /* Allocate memory for faces. */
  subdiv_ccg->num_faces = num_faces;
  if (num_faces) {
    subdiv_ccg->faces = MEM_calloc_arrayN(num_faces, sizeof(SubdivCCGFace), "Subdiv CCG faces");
    subdiv_ccg->grid_faces = MEM_calloc_arrayN(
        num_grids, sizeof(SubdivCCGFace *), "Subdiv CCG grid faces");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grids evaluation
 * \{ */

typedef struct CCGEvalGridsData {
  SubdivCCG *subdiv_ccg;
  Subdiv *subdiv;
  int *face_ptex_offset;
  SubdivCCGMaskEvaluator *mask_evaluator;
  SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator;
} CCGEvalGridsData;

static void subdiv_ccg_eval_grid_element_limit(CCGEvalGridsData *data,
                                               const int ptex_face_index,
                                               const float u,
                                               const float v,
                                               unsigned char *element)
{
  Subdiv *subdiv = data->subdiv;
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  if (subdiv->displacement_evaluator != NULL) {
    BKE_subdiv_eval_final_point(subdiv, ptex_face_index, u, v, (float *)element);
  }
  else if (subdiv_ccg->has_normal) {
    BKE_subdiv_eval_limit_point_and_normal(subdiv,
                                           ptex_face_index,
                                           u,
                                           v,
                                           (float *)element,
                                           (float *)(element + subdiv_ccg->normal_offset));
  }
  else {
    BKE_subdiv_eval_limit_point(subdiv, ptex_face_index, u, v, (float *)element);
  }
}

static void subdiv_ccg_eval_grid_element_mask(CCGEvalGridsData *data,
                                              const int ptex_face_index,
                                              const float u,
                                              const float v,
                                              unsigned char *element)
{
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  if (!subdiv_ccg->has_mask) {
    return;
  }
  float *mask_value_ptr = (float *)(element + subdiv_ccg->mask_offset);
  if (data->mask_evaluator != NULL) {
    *mask_value_ptr = data->mask_evaluator->eval_mask(data->mask_evaluator, ptex_face_index, u, v);
  }
  else {
    *mask_value_ptr = 0.0f;
  }
}

static void subdiv_ccg_eval_grid_element(CCGEvalGridsData *data,
                                         const int ptex_face_index,
                                         const float u,
                                         const float v,
                                         unsigned char *element)
{
  subdiv_ccg_eval_grid_element_limit(data, ptex_face_index, u, v, element);
  subdiv_ccg_eval_grid_element_mask(data, ptex_face_index, u, v, element);
}

static void subdiv_ccg_eval_regular_grid(CCGEvalGridsData *data, const int face_index)
{
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  const int ptex_face_index = data->face_ptex_offset[face_index];
  const int grid_size = subdiv_ccg->grid_size;
  const float grid_size_1_inv = 1.0f / (float)(grid_size - 1);
  const int element_size = element_size_bytes_get(subdiv_ccg);
  SubdivCCGFace *faces = subdiv_ccg->faces;
  SubdivCCGFace **grid_faces = subdiv_ccg->grid_faces;
  const SubdivCCGFace *face = &faces[face_index];
  for (int corner = 0; corner < face->num_grids; corner++) {
    const int grid_index = face->start_grid_index + corner;
    unsigned char *grid = (unsigned char *)subdiv_ccg->grids[grid_index];
    for (int y = 0; y < grid_size; y++) {
      const float grid_v = (float)y * grid_size_1_inv;
      for (int x = 0; x < grid_size; x++) {
        const float grid_u = (float)x * grid_size_1_inv;
        float u, v;
        BKE_subdiv_rotate_grid_to_quad(corner, grid_u, grid_v, &u, &v);
        const size_t grid_element_index = (size_t)y * grid_size + x;
        const size_t grid_element_offset = grid_element_index * element_size;
        subdiv_ccg_eval_grid_element(data, ptex_face_index, u, v, &grid[grid_element_offset]);
      }
    }
    /* Assign grid's face. */
    grid_faces[grid_index] = &faces[face_index];
    /* Assign material flags. */
    subdiv_ccg->grid_flag_mats[grid_index] = data->material_flags_evaluator->eval_material_flags(
        data->material_flags_evaluator, face_index);
  }
}

static void subdiv_ccg_eval_special_grid(CCGEvalGridsData *data, const int face_index)
{
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  const int grid_size = subdiv_ccg->grid_size;
  const float grid_size_1_inv = 1.0f / (float)(grid_size - 1);
  const int element_size = element_size_bytes_get(subdiv_ccg);
  SubdivCCGFace *faces = subdiv_ccg->faces;
  SubdivCCGFace **grid_faces = subdiv_ccg->grid_faces;
  const SubdivCCGFace *face = &faces[face_index];
  for (int corner = 0; corner < face->num_grids; corner++) {
    const int grid_index = face->start_grid_index + corner;
    const int ptex_face_index = data->face_ptex_offset[face_index] + corner;
    unsigned char *grid = (unsigned char *)subdiv_ccg->grids[grid_index];
    for (int y = 0; y < grid_size; y++) {
      const float u = 1.0f - ((float)y * grid_size_1_inv);
      for (int x = 0; x < grid_size; x++) {
        const float v = 1.0f - ((float)x * grid_size_1_inv);
        const size_t grid_element_index = (size_t)y * grid_size + x;
        const size_t grid_element_offset = grid_element_index * element_size;
        subdiv_ccg_eval_grid_element(data, ptex_face_index, u, v, &grid[grid_element_offset]);
      }
    }
    /* Assign grid's face. */
    grid_faces[grid_index] = &faces[face_index];
    /* Assign material flags. */
    subdiv_ccg->grid_flag_mats[grid_index] = data->material_flags_evaluator->eval_material_flags(
        data->material_flags_evaluator, face_index);
  }
}

static void subdiv_ccg_eval_grids_task(void *__restrict userdata_v,
                                       const int face_index,
                                       const TaskParallelTLS *__restrict UNUSED(tls))
{
  CCGEvalGridsData *data = userdata_v;
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  SubdivCCGFace *face = &subdiv_ccg->faces[face_index];
  if (face->num_grids == 4) {
    subdiv_ccg_eval_regular_grid(data, face_index);
  }
  else {
    subdiv_ccg_eval_special_grid(data, face_index);
  }
}

static bool subdiv_ccg_evaluate_grids(SubdivCCG *subdiv_ccg,
                                      Subdiv *subdiv,
                                      SubdivCCGMaskEvaluator *mask_evaluator,
                                      SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator)
{
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  const int num_faces = topology_refiner->getNumFaces(topology_refiner);
  /* Initialize data passed to all the tasks. */
  CCGEvalGridsData data;
  data.subdiv_ccg = subdiv_ccg;
  data.subdiv = subdiv;
  data.face_ptex_offset = BKE_subdiv_face_ptex_offset_get(subdiv);
  data.mask_evaluator = mask_evaluator;
  data.material_flags_evaluator = material_flags_evaluator;
  /* Threaded grids evaluation. */
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  BLI_task_parallel_range(
      0, num_faces, &data, subdiv_ccg_eval_grids_task, &parallel_range_settings);
  /* If displacement is used, need to calculate normals after all final
   * coordinates are known. */
  if (subdiv->displacement_evaluator != NULL) {
    BKE_subdiv_ccg_recalc_normals(subdiv_ccg);
  }
  return true;
}

/* Initialize face descriptors, assuming memory for them was already
 * allocated. */
static void subdiv_ccg_init_faces(SubdivCCG *subdiv_ccg)
{
  Subdiv *subdiv = subdiv_ccg->subdiv;
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  const int num_faces = subdiv_ccg->num_faces;
  int corner_index = 0;
  for (int face_index = 0; face_index < num_faces; face_index++) {
    const int num_corners = topology_refiner->getNumFaceVertices(topology_refiner, face_index);
    subdiv_ccg->faces[face_index].num_grids = num_corners;
    subdiv_ccg->faces[face_index].start_grid_index = corner_index;
    corner_index += num_corners;
  }
}

/* TODO(sergey): Consider making it generic enough to be fit into BLI. */
typedef struct StaticOrHeapIntStorage {
  int static_storage[64];
  int static_storage_size;
  int *heap_storage;
  int heap_storage_size;
} StaticOrHeapIntStorage;

static void static_or_heap_storage_init(StaticOrHeapIntStorage *storage)
{
  storage->static_storage_size = sizeof(storage->static_storage) /
                                 sizeof(*storage->static_storage);
  storage->heap_storage = NULL;
  storage->heap_storage_size = 0;
}

static int *static_or_heap_storage_get(StaticOrHeapIntStorage *storage, int size)
{
  /* Requested size small enough to be fit into stack allocated memory. */
  if (size <= storage->static_storage_size) {
    return storage->static_storage;
  }
  /* Make sure heap ius big enough. */
  if (size > storage->heap_storage_size) {
    MEM_SAFE_FREE(storage->heap_storage);
    storage->heap_storage = MEM_malloc_arrayN(size, sizeof(int), "int storage");
    storage->heap_storage_size = size;
  }
  return storage->heap_storage;
}

static void static_or_heap_storage_free(StaticOrHeapIntStorage *storage)
{
  MEM_SAFE_FREE(storage->heap_storage);
}

static void subdiv_ccg_allocate_adjacent_edges(SubdivCCG *subdiv_ccg, const int num_edges)
{
  subdiv_ccg->num_adjacent_edges = num_edges;
  subdiv_ccg->adjacent_edges = MEM_calloc_arrayN(
      subdiv_ccg->num_adjacent_edges, sizeof(*subdiv_ccg->adjacent_edges), "ccg adjacent edges");
}

static SubdivCCGCoord subdiv_ccg_coord(int grid_index, int x, int y)
{
  SubdivCCGCoord coord = {.grid_index = grid_index, .x = x, .y = y};
  return coord;
}

static CCGElem *subdiv_ccg_coord_to_elem(const CCGKey *key,
                                         const SubdivCCG *subdiv_ccg,
                                         const SubdivCCGCoord *coord)
{
  return CCG_grid_elem(key, subdiv_ccg->grids[coord->grid_index], coord->x, coord->y);
}

/* Returns storage where boundary elements are to be stored. */
static SubdivCCGCoord *subdiv_ccg_adjacent_edge_add_face(SubdivCCG *subdiv_ccg,
                                                         SubdivCCGAdjacentEdge *adjacent_edge)
{
  const int grid_size = subdiv_ccg->grid_size * 2;
  const int adjacent_face_index = adjacent_edge->num_adjacent_faces;
  ++adjacent_edge->num_adjacent_faces;
  /* Allocate memory for the boundary elements. */
  adjacent_edge->boundary_coords = MEM_reallocN(adjacent_edge->boundary_coords,
                                                adjacent_edge->num_adjacent_faces *
                                                    sizeof(*adjacent_edge->boundary_coords));
  adjacent_edge->boundary_coords[adjacent_face_index] = MEM_malloc_arrayN(
      grid_size * 2, sizeof(SubdivCCGCoord), "ccg adjacent boundary");
  return adjacent_edge->boundary_coords[adjacent_face_index];
}

static void subdiv_ccg_init_faces_edge_neighborhood(SubdivCCG *subdiv_ccg)
{
  Subdiv *subdiv = subdiv_ccg->subdiv;
  SubdivCCGFace *faces = subdiv_ccg->faces;
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  const int num_edges = topology_refiner->getNumEdges(topology_refiner);
  const int grid_size = subdiv_ccg->grid_size;
  if (num_edges == 0) {
    /* Early output, nothing to do in this case. */
    return;
  }
  subdiv_ccg_allocate_adjacent_edges(subdiv_ccg, num_edges);
  /* Initialize storage. */
  StaticOrHeapIntStorage face_vertices_storage;
  StaticOrHeapIntStorage face_edges_storage;
  static_or_heap_storage_init(&face_vertices_storage);
  static_or_heap_storage_init(&face_edges_storage);
  /* Store adjacency for all faces. */
  const int num_faces = subdiv_ccg->num_faces;
  for (int face_index = 0; face_index < num_faces; face_index++) {
    SubdivCCGFace *face = &faces[face_index];
    const int num_face_grids = face->num_grids;
    const int num_face_edges = num_face_grids;
    int *face_vertices = static_or_heap_storage_get(&face_vertices_storage, num_face_edges);
    topology_refiner->getFaceVertices(topology_refiner, face_index, face_vertices);
    /* Note that order of edges is same as order of MLoops, which also
     * means it's the same as order of grids. */
    int *face_edges = static_or_heap_storage_get(&face_edges_storage, num_face_edges);
    topology_refiner->getFaceEdges(topology_refiner, face_index, face_edges);
    /* Store grids adjacency for this edge. */
    for (int corner = 0; corner < num_face_edges; corner++) {
      const int vertex_index = face_vertices[corner];
      const int edge_index = face_edges[corner];
      int edge_vertices[2];
      topology_refiner->getEdgeVertices(topology_refiner, edge_index, edge_vertices);
      const bool is_edge_flipped = (edge_vertices[0] != vertex_index);
      /* Grid which is adjacent to the current corner. */
      const int current_grid_index = face->start_grid_index + corner;
      /* Grid which is adjacent to the next corner. */
      const int next_grid_index = face->start_grid_index + (corner + 1) % num_face_grids;
      /* Add new face to the adjacent edge. */
      SubdivCCGAdjacentEdge *adjacent_edge = &subdiv_ccg->adjacent_edges[edge_index];
      SubdivCCGCoord *boundary_coords = subdiv_ccg_adjacent_edge_add_face(subdiv_ccg,
                                                                          adjacent_edge);
      /* Fill CCG elements along the edge. */
      int boundary_element_index = 0;
      if (is_edge_flipped) {
        for (int i = 0; i < grid_size; i++) {
          boundary_coords[boundary_element_index++] = subdiv_ccg_coord(
              next_grid_index, grid_size - i - 1, grid_size - 1);
        }
        for (int i = 0; i < grid_size; i++) {
          boundary_coords[boundary_element_index++] = subdiv_ccg_coord(
              current_grid_index, grid_size - 1, i);
        }
      }
      else {
        for (int i = 0; i < grid_size; i++) {
          boundary_coords[boundary_element_index++] = subdiv_ccg_coord(
              current_grid_index, grid_size - 1, grid_size - i - 1);
        }
        for (int i = 0; i < grid_size; i++) {
          boundary_coords[boundary_element_index++] = subdiv_ccg_coord(
              next_grid_index, i, grid_size - 1);
        }
      }
    }
  }
  /* Free possibly heap-allocated storage. */
  static_or_heap_storage_free(&face_vertices_storage);
  static_or_heap_storage_free(&face_edges_storage);
}

static void subdiv_ccg_allocate_adjacent_vertices(SubdivCCG *subdiv_ccg, const int num_vertices)
{
  subdiv_ccg->num_adjacent_vertices = num_vertices;
  subdiv_ccg->adjacent_vertices = MEM_calloc_arrayN(subdiv_ccg->num_adjacent_vertices,
                                                    sizeof(*subdiv_ccg->adjacent_vertices),
                                                    "ccg adjacent vertices");
}

/* Returns storage where corner elements are to be stored. This is a pointer
 * to the actual storage. */
static SubdivCCGCoord *subdiv_ccg_adjacent_vertex_add_face(
    SubdivCCGAdjacentVertex *adjacent_vertex)
{
  const int adjacent_face_index = adjacent_vertex->num_adjacent_faces;
  ++adjacent_vertex->num_adjacent_faces;
  /* Allocate memory for the boundary elements. */
  adjacent_vertex->corner_coords = MEM_reallocN(adjacent_vertex->corner_coords,
                                                adjacent_vertex->num_adjacent_faces *
                                                    sizeof(*adjacent_vertex->corner_coords));
  return &adjacent_vertex->corner_coords[adjacent_face_index];
}

static void subdiv_ccg_init_faces_vertex_neighborhood(SubdivCCG *subdiv_ccg)
{
  Subdiv *subdiv = subdiv_ccg->subdiv;
  SubdivCCGFace *faces = subdiv_ccg->faces;
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  const int num_vertices = topology_refiner->getNumVertices(topology_refiner);
  const int grid_size = subdiv_ccg->grid_size;
  if (num_vertices == 0) {
    /* Early output, nothing to do in this case. */
    return;
  }
  subdiv_ccg_allocate_adjacent_vertices(subdiv_ccg, num_vertices);
  /* Initialize storage. */
  StaticOrHeapIntStorage face_vertices_storage;
  static_or_heap_storage_init(&face_vertices_storage);
  /* Key to access elements. */
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  /* Store adjacency for all faces. */
  const int num_faces = subdiv_ccg->num_faces;
  for (int face_index = 0; face_index < num_faces; face_index++) {
    SubdivCCGFace *face = &faces[face_index];
    const int num_face_grids = face->num_grids;
    const int num_face_edges = num_face_grids;
    int *face_vertices = static_or_heap_storage_get(&face_vertices_storage, num_face_edges);
    topology_refiner->getFaceVertices(topology_refiner, face_index, face_vertices);
    for (int corner = 0; corner < num_face_edges; corner++) {
      const int vertex_index = face_vertices[corner];
      /* Grid which is adjacent to the current corner. */
      const int grid_index = face->start_grid_index + corner;
      /* Add new face to the adjacent edge. */
      SubdivCCGAdjacentVertex *adjacent_vertex = &subdiv_ccg->adjacent_vertices[vertex_index];
      SubdivCCGCoord *corner_coord = subdiv_ccg_adjacent_vertex_add_face(adjacent_vertex);
      *corner_coord = subdiv_ccg_coord(grid_index, grid_size - 1, grid_size - 1);
    }
  }
  /* Free possibly heap-allocated storage. */
  static_or_heap_storage_free(&face_vertices_storage);
}

static void subdiv_ccg_init_faces_neighborhood(SubdivCCG *subdiv_ccg)
{
  subdiv_ccg_init_faces_edge_neighborhood(subdiv_ccg);
  subdiv_ccg_init_faces_vertex_neighborhood(subdiv_ccg);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation / evaluation
 * \{ */

SubdivCCG *BKE_subdiv_to_ccg(Subdiv *subdiv,
                             const SubdivToCCGSettings *settings,
                             SubdivCCGMaskEvaluator *mask_evaluator,
                             SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator)
{
  BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
  SubdivCCG *subdiv_ccg = MEM_callocN(sizeof(SubdivCCG), "subdiv ccg");
  subdiv_ccg->subdiv = subdiv;
  subdiv_ccg->level = bitscan_forward_i(settings->resolution - 1);
  subdiv_ccg->grid_size = BKE_subdiv_grid_size_from_level(subdiv_ccg->level);
  subdiv_ccg_init_layers(subdiv_ccg, settings);
  subdiv_ccg_alloc_elements(subdiv_ccg, subdiv);
  subdiv_ccg_init_faces(subdiv_ccg);
  subdiv_ccg_init_faces_neighborhood(subdiv_ccg);
  if (!subdiv_ccg_evaluate_grids(subdiv_ccg, subdiv, mask_evaluator, material_flags_evaluator)) {
    BKE_subdiv_ccg_destroy(subdiv_ccg);
    BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
    return NULL;
  }
  BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
  return subdiv_ccg;
}

Mesh *BKE_subdiv_to_ccg_mesh(Subdiv *subdiv,
                             const SubdivToCCGSettings *settings,
                             const Mesh *coarse_mesh)
{
  /* Make sure evaluator is ready. */
  BKE_subdiv_stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
  if (!BKE_subdiv_eval_begin_from_mesh(subdiv, coarse_mesh, NULL)) {
    if (coarse_mesh->totpoly) {
      return NULL;
    }
  }
  BKE_subdiv_stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_CCG);
  SubdivCCGMaskEvaluator mask_evaluator;
  bool has_mask = BKE_subdiv_ccg_mask_init_from_paint(&mask_evaluator, coarse_mesh);
  SubdivCCGMaterialFlagsEvaluator material_flags_evaluator;
  BKE_subdiv_ccg_material_flags_init_from_mesh(&material_flags_evaluator, coarse_mesh);
  SubdivCCG *subdiv_ccg = BKE_subdiv_to_ccg(
      subdiv, settings, has_mask ? &mask_evaluator : NULL, &material_flags_evaluator);
  if (has_mask) {
    mask_evaluator.free(&mask_evaluator);
  }
  material_flags_evaluator.free(&material_flags_evaluator);
  if (subdiv_ccg == NULL) {
    return NULL;
  }
  Mesh *result = BKE_mesh_new_nomain_from_template(coarse_mesh, 0, 0, 0, 0, 0);
  result->runtime.subdiv_ccg = subdiv_ccg;
  return result;
}

void BKE_subdiv_ccg_destroy(SubdivCCG *subdiv_ccg)
{
  const int num_grids = subdiv_ccg->num_grids;
  MEM_SAFE_FREE(subdiv_ccg->grids);
  MEM_SAFE_FREE(subdiv_ccg->grids_storage);
  MEM_SAFE_FREE(subdiv_ccg->edges);
  MEM_SAFE_FREE(subdiv_ccg->vertices);
  MEM_SAFE_FREE(subdiv_ccg->grid_flag_mats);
  if (subdiv_ccg->grid_hidden != NULL) {
    for (int grid_index = 0; grid_index < num_grids; grid_index++) {
      MEM_SAFE_FREE(subdiv_ccg->grid_hidden[grid_index]);
    }
    MEM_SAFE_FREE(subdiv_ccg->grid_hidden);
  }
  if (subdiv_ccg->subdiv != NULL) {
    BKE_subdiv_free(subdiv_ccg->subdiv);
  }
  MEM_SAFE_FREE(subdiv_ccg->faces);
  MEM_SAFE_FREE(subdiv_ccg->grid_faces);
  /* Free map of adjacent edges. */
  for (int i = 0; i < subdiv_ccg->num_adjacent_edges; i++) {
    SubdivCCGAdjacentEdge *adjacent_edge = &subdiv_ccg->adjacent_edges[i];
    for (int face_index = 0; face_index < adjacent_edge->num_adjacent_faces; face_index++) {
      MEM_SAFE_FREE(adjacent_edge->boundary_coords[face_index]);
    }
    MEM_SAFE_FREE(adjacent_edge->boundary_coords);
  }
  MEM_SAFE_FREE(subdiv_ccg->adjacent_edges);
  /* Free map of adjacent vertices. */
  for (int i = 0; i < subdiv_ccg->num_adjacent_vertices; i++) {
    SubdivCCGAdjacentVertex *adjacent_vertex = &subdiv_ccg->adjacent_vertices[i];
    MEM_SAFE_FREE(adjacent_vertex->corner_coords);
  }
  MEM_SAFE_FREE(subdiv_ccg->adjacent_vertices);
  MEM_freeN(subdiv_ccg);
}

void BKE_subdiv_ccg_key(CCGKey *key, const SubdivCCG *subdiv_ccg, int level)
{
  key->level = level;
  key->elem_size = element_size_bytes_get(subdiv_ccg);
  key->grid_size = BKE_subdiv_grid_size_from_level(level);
  key->grid_area = key->grid_size * key->grid_size;
  key->grid_bytes = key->elem_size * key->grid_area;

  key->normal_offset = subdiv_ccg->normal_offset;
  key->mask_offset = subdiv_ccg->mask_offset;

  key->has_normals = subdiv_ccg->has_normal;
  key->has_mask = subdiv_ccg->has_mask;
}

void BKE_subdiv_ccg_key_top_level(CCGKey *key, const SubdivCCG *subdiv_ccg)
{
  BKE_subdiv_ccg_key(key, subdiv_ccg, subdiv_ccg->level);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normals
 * \{ */

typedef struct RecalcInnerNormalsData {
  SubdivCCG *subdiv_ccg;
  CCGKey *key;
} RecalcInnerNormalsData;

typedef struct RecalcInnerNormalsTLSData {
  float (*face_normals)[3];
} RecalcInnerNormalsTLSData;

/* Evaluate high-res face normals, for faces which corresponds to grid elements
 *
 *   {(x, y), {x + 1, y}, {x + 1, y + 1}, {x, y + 1}}
 *
 * The result is stored in normals storage from TLS. */
static void subdiv_ccg_recalc_inner_face_normals(SubdivCCG *subdiv_ccg,
                                                 CCGKey *key,
                                                 RecalcInnerNormalsTLSData *tls,
                                                 const int grid_index)
{
  const int grid_size = subdiv_ccg->grid_size;
  const int grid_size_1 = grid_size - 1;
  CCGElem *grid = subdiv_ccg->grids[grid_index];
  if (tls->face_normals == NULL) {
    tls->face_normals = MEM_malloc_arrayN(
        grid_size_1 * grid_size_1, 3 * sizeof(float), "CCG TLS normals");
  }
  for (int y = 0; y < grid_size - 1; y++) {
    for (int x = 0; x < grid_size - 1; x++) {
      CCGElem *grid_elements[4] = {
          CCG_grid_elem(key, grid, x, y + 1),
          CCG_grid_elem(key, grid, x + 1, y + 1),
          CCG_grid_elem(key, grid, x + 1, y),
          CCG_grid_elem(key, grid, x, y),
      };
      float *co[4] = {
          CCG_elem_co(key, grid_elements[0]),
          CCG_elem_co(key, grid_elements[1]),
          CCG_elem_co(key, grid_elements[2]),
          CCG_elem_co(key, grid_elements[3]),
      };
      const int face_index = y * grid_size_1 + x;
      float *face_normal = tls->face_normals[face_index];
      normal_quad_v3(face_normal, co[0], co[1], co[2], co[3]);
    }
  }
}

/* Average normals at every grid element, using adjacent faces normals. */
static void subdiv_ccg_average_inner_face_normals(SubdivCCG *subdiv_ccg,
                                                  CCGKey *key,
                                                  RecalcInnerNormalsTLSData *tls,
                                                  const int grid_index)
{
  const int grid_size = subdiv_ccg->grid_size;
  const int grid_size_1 = grid_size - 1;
  CCGElem *grid = subdiv_ccg->grids[grid_index];
  const float(*face_normals)[3] = tls->face_normals;
  for (int y = 0; y < grid_size; y++) {
    for (int x = 0; x < grid_size; x++) {
      float normal_acc[3] = {0.0f, 0.0f, 0.0f};
      int counter = 0;
      /* Accumulate normals of all adjacent faces. */
      if (x < grid_size_1 && y < grid_size_1) {
        add_v3_v3(normal_acc, face_normals[y * grid_size_1 + x]);
        counter++;
      }
      if (x >= 1) {
        if (y < grid_size_1) {
          add_v3_v3(normal_acc, face_normals[y * grid_size_1 + (x - 1)]);
          counter++;
        }
        if (y >= 1) {
          add_v3_v3(normal_acc, face_normals[(y - 1) * grid_size_1 + (x - 1)]);
          counter++;
        }
      }
      if (y >= 1 && x < grid_size_1) {
        add_v3_v3(normal_acc, face_normals[(y - 1) * grid_size_1 + x]);
        counter++;
      }
      /* Normalize and store. */
      mul_v3_v3fl(CCG_grid_elem_no(key, grid, x, y), normal_acc, 1.0f / (float)counter);
    }
  }
}

static void subdiv_ccg_recalc_inner_normal_task(void *__restrict userdata_v,
                                                const int grid_index,
                                                const TaskParallelTLS *__restrict tls_v)
{
  RecalcInnerNormalsData *data = userdata_v;
  RecalcInnerNormalsTLSData *tls = tls_v->userdata_chunk;
  subdiv_ccg_recalc_inner_face_normals(data->subdiv_ccg, data->key, tls, grid_index);
  subdiv_ccg_average_inner_face_normals(data->subdiv_ccg, data->key, tls, grid_index);
}

static void subdiv_ccg_recalc_inner_normal_free(const void *__restrict UNUSED(userdata),
                                                void *__restrict tls_v)
{
  RecalcInnerNormalsTLSData *tls = tls_v;
  MEM_SAFE_FREE(tls->face_normals);
}

/* Recalculate normals which corresponds to non-boundaries elements of grids. */
static void subdiv_ccg_recalc_inner_grid_normals(SubdivCCG *subdiv_ccg)
{
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  RecalcInnerNormalsData data = {
      .subdiv_ccg = subdiv_ccg,
      .key = &key,
  };
  RecalcInnerNormalsTLSData tls_data = {NULL};
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  parallel_range_settings.userdata_chunk = &tls_data;
  parallel_range_settings.userdata_chunk_size = sizeof(tls_data);
  parallel_range_settings.func_free = subdiv_ccg_recalc_inner_normal_free;
  BLI_task_parallel_range(0,
                          subdiv_ccg->num_grids,
                          &data,
                          subdiv_ccg_recalc_inner_normal_task,
                          &parallel_range_settings);
}

void BKE_subdiv_ccg_recalc_normals(SubdivCCG *subdiv_ccg)
{
  if (!subdiv_ccg->has_normal) {
    /* Grids don't have normals, can do early output. */
    return;
  }
  subdiv_ccg_recalc_inner_grid_normals(subdiv_ccg);
  BKE_subdiv_ccg_average_grids(subdiv_ccg);
}

typedef struct RecalcModifiedInnerNormalsData {
  SubdivCCG *subdiv_ccg;
  CCGKey *key;
  SubdivCCGFace **effected_ccg_faces;
} RecalcModifiedInnerNormalsData;

static void subdiv_ccg_recalc_modified_inner_normal_task(void *__restrict userdata_v,
                                                         const int face_index,
                                                         const TaskParallelTLS *__restrict tls_v)
{
  RecalcModifiedInnerNormalsData *data = userdata_v;
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  CCGKey *key = data->key;
  RecalcInnerNormalsTLSData *tls = tls_v->userdata_chunk;
  SubdivCCGFace **faces = data->effected_ccg_faces;
  SubdivCCGFace *face = faces[face_index];
  const int num_face_grids = face->num_grids;
  for (int i = 0; i < num_face_grids; i++) {
    const int grid_index = face->start_grid_index + i;
    subdiv_ccg_recalc_inner_face_normals(data->subdiv_ccg, data->key, tls, grid_index);
    subdiv_ccg_average_inner_face_normals(data->subdiv_ccg, data->key, tls, grid_index);
  }
  subdiv_ccg_average_inner_face_grids(subdiv_ccg, key, face);
}

static void subdiv_ccg_recalc_modified_inner_normal_free(const void *__restrict UNUSED(userdata),
                                                         void *__restrict tls_v)
{
  RecalcInnerNormalsTLSData *tls = tls_v;
  MEM_SAFE_FREE(tls->face_normals);
}

static void subdiv_ccg_recalc_modified_inner_grid_normals(SubdivCCG *subdiv_ccg,
                                                          struct CCGFace **effected_faces,
                                                          int num_effected_faces)
{
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  RecalcModifiedInnerNormalsData data = {
      .subdiv_ccg = subdiv_ccg,
      .key = &key,
      .effected_ccg_faces = (SubdivCCGFace **)effected_faces,
  };
  RecalcInnerNormalsTLSData tls_data = {NULL};
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  parallel_range_settings.userdata_chunk = &tls_data;
  parallel_range_settings.userdata_chunk_size = sizeof(tls_data);
  parallel_range_settings.func_free = subdiv_ccg_recalc_modified_inner_normal_free;
  BLI_task_parallel_range(0,
                          num_effected_faces,
                          &data,
                          subdiv_ccg_recalc_modified_inner_normal_task,
                          &parallel_range_settings);
}

void BKE_subdiv_ccg_update_normals(SubdivCCG *subdiv_ccg,
                                   struct CCGFace **effected_faces,
                                   int num_effected_faces)
{
  if (!subdiv_ccg->has_normal) {
    /* Grids don't have normals, can do early output. */
    return;
  }
  if (num_effected_faces == 0) {
    /* No faces changed, so nothing to do here. */
    return;
  }
  subdiv_ccg_recalc_modified_inner_grid_normals(subdiv_ccg, effected_faces, num_effected_faces);
  /* TODO(sergey): Only average elements which are adjacent to modified
   * faces. */
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  subdiv_ccg_average_all_boundaries_and_corners(subdiv_ccg, &key);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Boundary averaging/stitching
 * \{ */

typedef struct AverageInnerGridsData {
  SubdivCCG *subdiv_ccg;
  CCGKey *key;
} AverageInnerGridsData;

static void average_grid_element_value_v3(float a[3], float b[3])
{
  add_v3_v3(a, b);
  mul_v3_fl(a, 0.5f);
  copy_v3_v3(b, a);
}

static void average_grid_element(SubdivCCG *subdiv_ccg,
                                 CCGKey *key,
                                 CCGElem *grid_element_a,
                                 CCGElem *grid_element_b)
{
  average_grid_element_value_v3(CCG_elem_co(key, grid_element_a),
                                CCG_elem_co(key, grid_element_b));
  if (subdiv_ccg->has_normal) {
    average_grid_element_value_v3(CCG_elem_no(key, grid_element_a),
                                  CCG_elem_no(key, grid_element_b));
  }
  if (subdiv_ccg->has_mask) {
    float mask = (*CCG_elem_mask(key, grid_element_a) + *CCG_elem_mask(key, grid_element_b)) *
                 0.5f;
    *CCG_elem_mask(key, grid_element_a) = mask;
    *CCG_elem_mask(key, grid_element_b) = mask;
  }
}

/* Accumulator to hold data during averaging. */
typedef struct GridElementAccumulator {
  float co[3];
  float no[3];
  float mask;
} GridElementAccumulator;

static void element_accumulator_init(GridElementAccumulator *accumulator)
{
  zero_v3(accumulator->co);
  zero_v3(accumulator->no);
  accumulator->mask = 0.0f;
}

static void element_accumulator_add(GridElementAccumulator *accumulator,
                                    const SubdivCCG *subdiv_ccg,
                                    CCGKey *key,
                                    /*const*/ CCGElem *grid_element)
{
  add_v3_v3(accumulator->co, CCG_elem_co(key, grid_element));
  if (subdiv_ccg->has_normal) {
    add_v3_v3(accumulator->no, CCG_elem_no(key, grid_element));
  }
  if (subdiv_ccg->has_mask) {
    accumulator->mask += *CCG_elem_mask(key, grid_element);
  }
}

static void element_accumulator_mul_fl(GridElementAccumulator *accumulator, const float f)
{
  mul_v3_fl(accumulator->co, f);
  mul_v3_fl(accumulator->no, f);
  accumulator->mask *= f;
}

static void element_accumulator_copy(SubdivCCG *subdiv_ccg,
                                     CCGKey *key,
                                     CCGElem *destination,
                                     const GridElementAccumulator *accumulator)
{
  copy_v3_v3(CCG_elem_co(key, destination), accumulator->co);
  if (subdiv_ccg->has_normal) {
    copy_v3_v3(CCG_elem_no(key, destination), accumulator->no);
  }
  if (subdiv_ccg->has_mask) {
    *CCG_elem_mask(key, destination) = accumulator->mask;
  }
}

static void subdiv_ccg_average_inner_face_grids(SubdivCCG *subdiv_ccg,
                                                CCGKey *key,
                                                SubdivCCGFace *face)
{
  CCGElem **grids = subdiv_ccg->grids;
  const int num_face_grids = face->num_grids;
  const int grid_size = subdiv_ccg->grid_size;
  CCGElem *prev_grid = grids[face->start_grid_index + num_face_grids - 1];
  /* Average boundary between neighbor grid. */
  for (int corner = 0; corner < num_face_grids; corner++) {
    CCGElem *grid = grids[face->start_grid_index + corner];
    for (int i = 1; i < grid_size; i++) {
      CCGElem *prev_grid_element = CCG_grid_elem(key, prev_grid, i, 0);
      CCGElem *grid_element = CCG_grid_elem(key, grid, 0, i);
      average_grid_element(subdiv_ccg, key, prev_grid_element, grid_element);
    }
    prev_grid = grid;
  }
  /* Average all grids centers into a single accumulator, and share it.
   * Guarantees correct and smooth averaging in the center. */
  GridElementAccumulator center_accumulator;
  element_accumulator_init(&center_accumulator);
  for (int corner = 0; corner < num_face_grids; corner++) {
    CCGElem *grid = grids[face->start_grid_index + corner];
    CCGElem *grid_center_element = CCG_grid_elem(key, grid, 0, 0);
    element_accumulator_add(&center_accumulator, subdiv_ccg, key, grid_center_element);
  }
  element_accumulator_mul_fl(&center_accumulator, 1.0f / (float)num_face_grids);
  for (int corner = 0; corner < num_face_grids; corner++) {
    CCGElem *grid = grids[face->start_grid_index + corner];
    CCGElem *grid_center_element = CCG_grid_elem(key, grid, 0, 0);
    element_accumulator_copy(subdiv_ccg, key, grid_center_element, &center_accumulator);
  }
}

static void subdiv_ccg_average_inner_grids_task(void *__restrict userdata_v,
                                                const int face_index,
                                                const TaskParallelTLS *__restrict UNUSED(tls_v))
{
  AverageInnerGridsData *data = userdata_v;
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  CCGKey *key = data->key;
  SubdivCCGFace *faces = subdiv_ccg->faces;
  SubdivCCGFace *face = &faces[face_index];
  subdiv_ccg_average_inner_face_grids(subdiv_ccg, key, face);
}

typedef struct AverageGridsBoundariesData {
  SubdivCCG *subdiv_ccg;
  CCGKey *key;
} AverageGridsBoundariesData;

typedef struct AverageGridsBoundariesTLSData {
  GridElementAccumulator *accumulators;
} AverageGridsBoundariesTLSData;

static void subdiv_ccg_average_grids_boundary(SubdivCCG *subdiv_ccg,
                                              CCGKey *key,
                                              SubdivCCGAdjacentEdge *adjacent_edge,
                                              AverageGridsBoundariesTLSData *tls)
{
  const int num_adjacent_faces = adjacent_edge->num_adjacent_faces;
  const int grid_size2 = subdiv_ccg->grid_size * 2;
  if (num_adjacent_faces == 1) {
    /* Nothing to average with. */
    return;
  }
  if (tls->accumulators == NULL) {
    tls->accumulators = MEM_calloc_arrayN(
        sizeof(GridElementAccumulator), grid_size2, "average accumulators");
  }
  else {
    for (int i = 1; i < grid_size2 - 1; i++) {
      element_accumulator_init(&tls->accumulators[i]);
    }
  }
  for (int face_index = 0; face_index < num_adjacent_faces; face_index++) {
    for (int i = 1; i < grid_size2 - 1; i++) {
      CCGElem *grid_element = subdiv_ccg_coord_to_elem(
          key, subdiv_ccg, &adjacent_edge->boundary_coords[face_index][i]);
      element_accumulator_add(&tls->accumulators[i], subdiv_ccg, key, grid_element);
    }
  }
  for (int i = 1; i < grid_size2 - 1; i++) {
    element_accumulator_mul_fl(&tls->accumulators[i], 1.0f / (float)num_adjacent_faces);
  }
  /* Copy averaged value to all the other faces. */
  for (int face_index = 0; face_index < num_adjacent_faces; face_index++) {
    for (int i = 1; i < grid_size2 - 1; i++) {
      CCGElem *grid_element = subdiv_ccg_coord_to_elem(
          key, subdiv_ccg, &adjacent_edge->boundary_coords[face_index][i]);
      element_accumulator_copy(subdiv_ccg, key, grid_element, &tls->accumulators[i]);
    }
  }
}

static void subdiv_ccg_average_grids_boundaries_task(void *__restrict userdata_v,
                                                     const int adjacent_edge_index,
                                                     const TaskParallelTLS *__restrict tls_v)
{
  AverageGridsBoundariesData *data = userdata_v;
  AverageGridsBoundariesTLSData *tls = tls_v->userdata_chunk;
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  CCGKey *key = data->key;
  SubdivCCGAdjacentEdge *adjacent_edge = &subdiv_ccg->adjacent_edges[adjacent_edge_index];
  subdiv_ccg_average_grids_boundary(subdiv_ccg, key, adjacent_edge, tls);
}

static void subdiv_ccg_average_grids_boundaries_free(const void *__restrict UNUSED(userdata),
                                                     void *__restrict tls_v)
{
  AverageGridsBoundariesTLSData *tls = tls_v;
  MEM_SAFE_FREE(tls->accumulators);
}

typedef struct AverageGridsCornerData {
  SubdivCCG *subdiv_ccg;
  CCGKey *key;
} AverageGridsCornerData;

static void subdiv_ccg_average_grids_corners(SubdivCCG *subdiv_ccg,
                                             CCGKey *key,
                                             SubdivCCGAdjacentVertex *adjacent_vertex)
{
  const int num_adjacent_faces = adjacent_vertex->num_adjacent_faces;
  if (num_adjacent_faces == 1) {
    /* Nothing to average with. */
    return;
  }
  GridElementAccumulator accumulator;
  element_accumulator_init(&accumulator);
  for (int face_index = 0; face_index < num_adjacent_faces; face_index++) {
    CCGElem *grid_element = subdiv_ccg_coord_to_elem(
        key, subdiv_ccg, &adjacent_vertex->corner_coords[face_index]);
    element_accumulator_add(&accumulator, subdiv_ccg, key, grid_element);
  }
  element_accumulator_mul_fl(&accumulator, 1.0f / (float)num_adjacent_faces);
  /* Copy averaged value to all the other faces. */
  for (int face_index = 0; face_index < num_adjacent_faces; face_index++) {
    CCGElem *grid_element = subdiv_ccg_coord_to_elem(
        key, subdiv_ccg, &adjacent_vertex->corner_coords[face_index]);
    element_accumulator_copy(subdiv_ccg, key, grid_element, &accumulator);
  }
}

static void subdiv_ccg_average_grids_corners_task(void *__restrict userdata_v,
                                                  const int adjacent_vertex_index,
                                                  const TaskParallelTLS *__restrict UNUSED(tls_v))
{
  AverageGridsCornerData *data = userdata_v;
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  CCGKey *key = data->key;
  SubdivCCGAdjacentVertex *adjacent_vertex = &subdiv_ccg->adjacent_vertices[adjacent_vertex_index];
  subdiv_ccg_average_grids_corners(subdiv_ccg, key, adjacent_vertex);
}

static void subdiv_ccg_average_all_boundaries(SubdivCCG *subdiv_ccg, CCGKey *key)
{
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  AverageGridsBoundariesData boundaries_data = {
      .subdiv_ccg = subdiv_ccg,
      .key = key,
  };
  AverageGridsBoundariesTLSData tls_data = {NULL};
  parallel_range_settings.userdata_chunk = &tls_data;
  parallel_range_settings.userdata_chunk_size = sizeof(tls_data);
  parallel_range_settings.func_free = subdiv_ccg_average_grids_boundaries_free;
  BLI_task_parallel_range(0,
                          subdiv_ccg->num_adjacent_edges,
                          &boundaries_data,
                          subdiv_ccg_average_grids_boundaries_task,
                          &parallel_range_settings);
}

static void subdiv_ccg_average_all_corners(SubdivCCG *subdiv_ccg, CCGKey *key)
{
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  AverageGridsCornerData corner_data = {
      .subdiv_ccg = subdiv_ccg,
      .key = key,
  };
  BLI_task_parallel_range(0,
                          subdiv_ccg->num_adjacent_vertices,
                          &corner_data,
                          subdiv_ccg_average_grids_corners_task,
                          &parallel_range_settings);
}

static void subdiv_ccg_average_all_boundaries_and_corners(SubdivCCG *subdiv_ccg, CCGKey *key)
{
  subdiv_ccg_average_all_boundaries(subdiv_ccg, key);
  subdiv_ccg_average_all_corners(subdiv_ccg, key);
}

void BKE_subdiv_ccg_average_grids(SubdivCCG *subdiv_ccg)
{
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  /* Average inner boundaries of grids (within one face), across faces
   * from different face-corners. */
  AverageInnerGridsData inner_data = {
      .subdiv_ccg = subdiv_ccg,
      .key = &key,
  };
  BLI_task_parallel_range(0,
                          subdiv_ccg->num_faces,
                          &inner_data,
                          subdiv_ccg_average_inner_grids_task,
                          &parallel_range_settings);
  subdiv_ccg_average_all_boundaries_and_corners(subdiv_ccg, &key);
}

typedef struct StitchFacesInnerGridsData {
  SubdivCCG *subdiv_ccg;
  CCGKey *key;
  struct CCGFace **effected_ccg_faces;
} StitchFacesInnerGridsData;

static void subdiv_ccg_stitch_face_inner_grids_task(
    void *__restrict userdata_v,
    const int face_index,
    const TaskParallelTLS *__restrict UNUSED(tls_v))
{
  StitchFacesInnerGridsData *data = userdata_v;
  SubdivCCG *subdiv_ccg = data->subdiv_ccg;
  CCGKey *key = data->key;
  struct CCGFace **effected_ccg_faces = data->effected_ccg_faces;
  struct CCGFace *effected_ccg_face = effected_ccg_faces[face_index];
  SubdivCCGFace *face = (SubdivCCGFace *)effected_ccg_face;
  subdiv_ccg_average_inner_face_grids(subdiv_ccg, key, face);
}

void BKE_subdiv_ccg_average_stitch_faces(SubdivCCG *subdiv_ccg,
                                         struct CCGFace **effected_faces,
                                         int num_effected_faces)
{
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  StitchFacesInnerGridsData data = {
      .subdiv_ccg = subdiv_ccg,
      .key = &key,
      .effected_ccg_faces = effected_faces,
  };
  TaskParallelSettings parallel_range_settings;
  BLI_parallel_range_settings_defaults(&parallel_range_settings);
  BLI_task_parallel_range(0,
                          num_effected_faces,
                          &data,
                          subdiv_ccg_stitch_face_inner_grids_task,
                          &parallel_range_settings);
  /* TODO(sergey): Only average elements which are adjacent to modified
   * faces. */
  subdiv_ccg_average_all_boundaries_and_corners(subdiv_ccg, &key);
}

void BKE_subdiv_ccg_topology_counters(const SubdivCCG *subdiv_ccg,
                                      int *r_num_vertices,
                                      int *r_num_edges,
                                      int *r_num_faces,
                                      int *r_num_loops)
{
  const int num_grids = subdiv_ccg->num_grids;
  const int grid_size = subdiv_ccg->grid_size;
  const int grid_area = grid_size * grid_size;
  const int num_edges_per_grid = 2 * (grid_size * (grid_size - 1));
  *r_num_vertices = num_grids * grid_area;
  *r_num_edges = num_grids * num_edges_per_grid;
  *r_num_faces = num_grids * (grid_size - 1) * (grid_size - 1);
  *r_num_loops = *r_num_faces * 4;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Neighbors
 * \{ */

void BKE_subdiv_ccg_print_coord(const char *message, const SubdivCCGCoord *coord)
{
  printf("%s: grid index: %d, coord: (%d, %d)\n", message, coord->grid_index, coord->x, coord->y);
}

bool BKE_subdiv_ccg_check_coord_valid(const SubdivCCG *subdiv_ccg, const SubdivCCGCoord *coord)
{
  if (coord->grid_index < 0 || coord->grid_index >= subdiv_ccg->num_grids) {
    return false;
  }
  const int grid_size = subdiv_ccg->grid_size;
  if (coord->x < 0 || coord->x >= grid_size) {
    return false;
  }
  if (coord->y < 0 || coord->y >= grid_size) {
    return false;
  }
  return true;
}

BLI_INLINE void subdiv_ccg_neighbors_init(SubdivCCGNeighbors *neighbors,
                                          const int num_unique,
                                          const int num_duplicates)
{
  const int size = num_unique + num_duplicates;
  neighbors->size = size;
  neighbors->num_duplicates = num_duplicates;
  if (size < ARRAY_SIZE(neighbors->coords_fixed)) {
    neighbors->coords = neighbors->coords_fixed;
  }
  else {
    neighbors->coords = MEM_mallocN(sizeof(*neighbors->coords) * size,
                                    "SubdivCCGNeighbors.coords");
  }
}

/* Check whether given coordinate belongs to a grid corner. */
BLI_INLINE bool is_corner_grid_coord(const SubdivCCG *subdiv_ccg, const SubdivCCGCoord *coord)
{
  const int grid_size_1 = subdiv_ccg->grid_size - 1;
  return (coord->x == 0 && coord->y == 0) || (coord->x == 0 && coord->y == grid_size_1) ||
         (coord->x == grid_size_1 && coord->y == grid_size_1) ||
         (coord->x == grid_size_1 && coord->y == 0);
}

/* Check whether given coordinate belongs to a grid boundary. */
BLI_INLINE bool is_boundary_grid_coord(const SubdivCCG *subdiv_ccg, const SubdivCCGCoord *coord)
{
  const int grid_size_1 = subdiv_ccg->grid_size - 1;
  return coord->x == 0 || coord->y == 0 || coord->x == grid_size_1 || coord->y == grid_size_1;
}

/* Check whether coordinate is at the boundary between two grids of the same face. */
BLI_INLINE bool is_inner_edge_grid_coordinate(const SubdivCCG *subdiv_ccg,
                                              const SubdivCCGCoord *coord)
{
  const int grid_size_1 = subdiv_ccg->grid_size - 1;
  if (coord->x == 0) {
    return coord->y > 0 && coord->y < grid_size_1;
  }
  if (coord->y == 0) {
    return coord->x > 0 && coord->x < grid_size_1;
  }
  return false;
}

BLI_INLINE SubdivCCGCoord coord_at_prev_row(const SubdivCCG *UNUSED(subdiv_ccg),
                                            const SubdivCCGCoord *coord)
{
  BLI_assert(coord->y > 0);
  SubdivCCGCoord result = *coord;
  result.y -= 1;
  return result;
}
BLI_INLINE SubdivCCGCoord coord_at_next_row(const SubdivCCG *subdiv_ccg,
                                            const SubdivCCGCoord *coord)
{
  UNUSED_VARS_NDEBUG(subdiv_ccg);
  BLI_assert(coord->y < subdiv_ccg->grid_size - 1);
  SubdivCCGCoord result = *coord;
  result.y += 1;
  return result;
}

BLI_INLINE SubdivCCGCoord coord_at_prev_col(const SubdivCCG *UNUSED(subdiv_ccg),
                                            const SubdivCCGCoord *coord)
{
  BLI_assert(coord->x > 0);
  SubdivCCGCoord result = *coord;
  result.x -= 1;
  return result;
}
BLI_INLINE SubdivCCGCoord coord_at_next_col(const SubdivCCG *subdiv_ccg,
                                            const SubdivCCGCoord *coord)
{
  UNUSED_VARS_NDEBUG(subdiv_ccg);
  BLI_assert(coord->x < subdiv_ccg->grid_size - 1);
  SubdivCCGCoord result = *coord;
  result.x += 1;
  return result;
}

/* For the input coordinate which is at the boundary of the grid do one step inside.  */
static SubdivCCGCoord coord_step_inside_from_boundary(const SubdivCCG *subdiv_ccg,
                                                      const SubdivCCGCoord *coord)

{
  SubdivCCGCoord result = *coord;
  const int grid_size_1 = subdiv_ccg->grid_size - 1;
  if (result.x == grid_size_1) {
    --result.x;
  }
  else if (result.y == grid_size_1) {
    --result.y;
  }
  else if (result.x == 0) {
    ++result.x;
  }
  else if (result.y == 0) {
    ++result.y;
  }
  else {
    BLI_assert(!"non-boundary element given");
  }
  return result;
}

BLI_INLINE
int next_grid_index_from_coord(const SubdivCCG *subdiv_ccg, const SubdivCCGCoord *coord)
{
  SubdivCCGFace *face = subdiv_ccg->grid_faces[coord->grid_index];
  const int face_grid_index = coord->grid_index;
  int next_face_grid_index = face_grid_index + 1 - face->start_grid_index;
  if (next_face_grid_index == face->num_grids) {
    next_face_grid_index = 0;
  }
  return face->start_grid_index + next_face_grid_index;
}
BLI_INLINE int prev_grid_index_from_coord(const SubdivCCG *subdiv_ccg, const SubdivCCGCoord *coord)
{
  SubdivCCGFace *face = subdiv_ccg->grid_faces[coord->grid_index];
  const int face_grid_index = coord->grid_index;
  int prev_face_grid_index = face_grid_index - 1 - face->start_grid_index;
  if (prev_face_grid_index < 0) {
    prev_face_grid_index = face->num_grids - 1;
  }
  return face->start_grid_index + prev_face_grid_index;
}

/* Simple case of getting neighbors of a corner coordinate: the corner is a face center, so
 * can only iterate over grid of a single face, without looking into adjacency. */
static void neighbor_coords_corner_center_get(const SubdivCCG *subdiv_ccg,
                                              const SubdivCCGCoord *coord,
                                              const bool include_duplicates,
                                              SubdivCCGNeighbors *r_neighbors)
{
  SubdivCCGFace *face = subdiv_ccg->grid_faces[coord->grid_index];
  const int num_adjacent_grids = face->num_grids;

  subdiv_ccg_neighbors_init(
      r_neighbors, num_adjacent_grids, (include_duplicates) ? num_adjacent_grids - 1 : 0);

  int duplicate_face_grid_index = num_adjacent_grids;
  for (int face_grid_index = 0; face_grid_index < num_adjacent_grids; ++face_grid_index) {
    SubdivCCGCoord neighbor_coord;
    neighbor_coord.grid_index = face->start_grid_index + face_grid_index;
    neighbor_coord.x = 1;
    neighbor_coord.y = 0;
    r_neighbors->coords[face_grid_index] = neighbor_coord;

    if (include_duplicates && neighbor_coord.grid_index != coord->grid_index) {
      neighbor_coord.x = 0;
      r_neighbors->coords[duplicate_face_grid_index++] = neighbor_coord;
    }
  }
}

/* Get index within adjacent_vertices array for the given CCG coordinate. */
static int adjacent_vertex_index_from_coord(const SubdivCCG *subdiv_ccg,
                                            const SubdivCCGCoord *coord)
{
  Subdiv *subdiv = subdiv_ccg->subdiv;
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;

  const SubdivCCGFace *face = subdiv_ccg->grid_faces[coord->grid_index];
  const int face_index = face - subdiv_ccg->faces;
  const int face_grid_index = coord->grid_index - face->start_grid_index;
  const int num_face_grids = face->num_grids;
  const int num_face_vertices = num_face_grids;

  StaticOrHeapIntStorage face_vertices_storage;
  static_or_heap_storage_init(&face_vertices_storage);

  int *face_vertices = static_or_heap_storage_get(&face_vertices_storage, num_face_vertices);
  topology_refiner->getFaceVertices(topology_refiner, face_index, face_vertices);

  const int adjacent_vertex_index = face_vertices[face_grid_index];
  static_or_heap_storage_free(&face_vertices_storage);
  return adjacent_vertex_index;
}

/* The corner is adjacent to a coarse vertex. */
static void neighbor_coords_corner_vertex_get(const SubdivCCG *subdiv_ccg,
                                              const SubdivCCGCoord *coord,
                                              const bool include_duplicates,
                                              SubdivCCGNeighbors *r_neighbors)
{
  Subdiv *subdiv = subdiv_ccg->subdiv;
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;

  const int adjacent_vertex_index = adjacent_vertex_index_from_coord(subdiv_ccg, coord);
  BLI_assert(adjacent_vertex_index >= 0);
  BLI_assert(adjacent_vertex_index < subdiv_ccg->num_adjacent_vertices);
  const int num_vertex_edges = topology_refiner->getNumVertexEdges(topology_refiner,
                                                                   adjacent_vertex_index);

  SubdivCCGAdjacentVertex *adjacent_vertex = &subdiv_ccg->adjacent_vertices[adjacent_vertex_index];
  const int num_adjacent_faces = adjacent_vertex->num_adjacent_faces;

  subdiv_ccg_neighbors_init(
      r_neighbors, num_vertex_edges, (include_duplicates) ? num_adjacent_faces - 1 : 0);

  StaticOrHeapIntStorage vertex_edges_storage;
  static_or_heap_storage_init(&vertex_edges_storage);

  int *vertex_edges = static_or_heap_storage_get(&vertex_edges_storage, num_vertex_edges);
  topology_refiner->getVertexEdges(topology_refiner, adjacent_vertex_index, vertex_edges);

  for (int i = 0; i < num_vertex_edges; ++i) {
    const int edge_index = vertex_edges[i];

    /* Use very first grid of every edge. */
    const int edge_face_index = 0;

    /* Depending edge orientation we use first (zero-based) or previous-to-last point. */
    int edge_vertices_indices[2];
    topology_refiner->getEdgeVertices(topology_refiner, edge_index, edge_vertices_indices);
    int edge_point_index, duplicate_edge_point_index;
    if (edge_vertices_indices[0] == adjacent_vertex_index) {
      duplicate_edge_point_index = 0;
      edge_point_index = duplicate_edge_point_index + 1;
    }
    else {
      /* Edge "consists" of 2 grids, which makes it 2 * grid_size elements per edge.
       * The index of last edge element is 2 * grid_size - 1 (due to zero-based indices),
       * and we are interested in previous to last element. */
      duplicate_edge_point_index = subdiv_ccg->grid_size * 2 - 1;
      edge_point_index = duplicate_edge_point_index - 1;
    }

    SubdivCCGAdjacentEdge *adjacent_edge = &subdiv_ccg->adjacent_edges[edge_index];
    r_neighbors->coords[i] = adjacent_edge->boundary_coords[edge_face_index][edge_point_index];
  }

  if (include_duplicates) {
    /* Add duplicates of the current grid vertex in adjacent faces if requested. */
    for (int i = 0, duplicate_i = num_vertex_edges; i < num_adjacent_faces; i++) {
      SubdivCCGCoord neighbor_coord = adjacent_vertex->corner_coords[i];
      if (neighbor_coord.grid_index != coord->grid_index) {
        r_neighbors->coords[duplicate_i++] = neighbor_coord;
      }
    }
  }

  static_or_heap_storage_free(&vertex_edges_storage);
}

static int adjacent_edge_index_from_coord(const SubdivCCG *subdiv_ccg, const SubdivCCGCoord *coord)
{
  Subdiv *subdiv = subdiv_ccg->subdiv;
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  SubdivCCGFace *face = subdiv_ccg->grid_faces[coord->grid_index];

  const int face_grid_index = coord->grid_index - face->start_grid_index;
  const int face_index = face - subdiv_ccg->faces;
  const int num_face_edges = topology_refiner->getNumFaceEdges(topology_refiner, face_index);

  StaticOrHeapIntStorage face_edges_storage;
  static_or_heap_storage_init(&face_edges_storage);
  int *face_edges_indices = static_or_heap_storage_get(&face_edges_storage, num_face_edges);
  topology_refiner->getFaceEdges(topology_refiner, face_index, face_edges_indices);

  const int grid_size_1 = subdiv_ccg->grid_size - 1;
  int adjacent_edge_index = -1;
  if (coord->x == grid_size_1) {
    adjacent_edge_index = face_edges_indices[face_grid_index];
  }
  else {
    BLI_assert(coord->y == grid_size_1);
    adjacent_edge_index =
        face_edges_indices[face_grid_index == 0 ? face->num_grids - 1 : face_grid_index - 1];
  }

  static_or_heap_storage_free(&face_edges_storage);

  return adjacent_edge_index;
}

static int adjacent_edge_point_index_from_coord(const SubdivCCG *subdiv_ccg,
                                                const SubdivCCGCoord *coord,
                                                const int adjacent_edge_index)
{
  Subdiv *subdiv = subdiv_ccg->subdiv;
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;

  const int adjacent_vertex_index = adjacent_vertex_index_from_coord(subdiv_ccg, coord);
  int edge_vertices_indices[2];
  topology_refiner->getEdgeVertices(topology_refiner, adjacent_edge_index, edge_vertices_indices);

  /* Vertex index of an edge which is used to see whether edge points in the right direction.
   * Tricky part here is that depending whether input coordinate is are maximum X or Y coordinate
   * of the grid we need to use different edge direction.
   * Basically, the edge adjacent to a previous loop needs to point opposite direction. */
  int directional_edge_vertex_index = -1;

  const int grid_size_1 = subdiv_ccg->grid_size - 1;
  int adjacent_edge_point_index = -1;
  if (coord->x == grid_size_1) {
    adjacent_edge_point_index = subdiv_ccg->grid_size - coord->y - 1;
    directional_edge_vertex_index = edge_vertices_indices[0];
  }
  else {
    BLI_assert(coord->y == grid_size_1);
    adjacent_edge_point_index = subdiv_ccg->grid_size + coord->x;
    directional_edge_vertex_index = edge_vertices_indices[1];
  }

  /* Flip the index if the edde points opposite direction. */
  if (adjacent_vertex_index != directional_edge_vertex_index) {
    const int num_edge_points = subdiv_ccg->grid_size * 2;
    adjacent_edge_point_index = num_edge_points - adjacent_edge_point_index - 1;
  }

  return adjacent_edge_point_index;
}

/* Adjacent edge has two points in the middle which corresponds to grid  corners, but which are
 * the same point in the final geometry.
 * So need to use extra step when calculating next/previous points, so we don't go from a corner
 * of one grid to a corner of adjacent grid. */
static int next_adjacent_edge_point_index(const SubdivCCG *subdiv_ccg, const int point_index)
{
  if (point_index == subdiv_ccg->grid_size - 1) {
    return point_index + 2;
  }
  return point_index + 1;
}
static int prev_adjacent_edge_point_index(const SubdivCCG *subdiv_ccg, const int point_index)
{
  if (point_index == subdiv_ccg->grid_size) {
    return point_index - 2;
  }
  return point_index - 1;
}

/* Common implementation of neighbor calculation when input coordinate is at the edge between two
 * coarse faces, but is not at the coarse vertex. */
static void neighbor_coords_edge_get(const SubdivCCG *subdiv_ccg,
                                     const SubdivCCGCoord *coord,
                                     const bool include_duplicates,
                                     SubdivCCGNeighbors *r_neighbors)

{
  const int adjacent_edge_index = adjacent_edge_index_from_coord(subdiv_ccg, coord);
  BLI_assert(adjacent_edge_index >= 0);
  BLI_assert(adjacent_edge_index < subdiv_ccg->num_adjacent_edges);
  const SubdivCCGAdjacentEdge *adjacent_edge = &subdiv_ccg->adjacent_edges[adjacent_edge_index];

  /* 2 neighbor points along the edge, plus one inner point per every adjacent grid. */
  const int num_adjacent_faces = adjacent_edge->num_adjacent_faces;
  subdiv_ccg_neighbors_init(
      r_neighbors, num_adjacent_faces + 2, (include_duplicates) ? num_adjacent_faces - 1 : 0);

  const int point_index = adjacent_edge_point_index_from_coord(
      subdiv_ccg, coord, adjacent_edge_index);
  const int next_point_index = next_adjacent_edge_point_index(subdiv_ccg, point_index);
  const int prev_point_index = prev_adjacent_edge_point_index(subdiv_ccg, point_index);

  for (int i = 0, duplicate_i = num_adjacent_faces; i < num_adjacent_faces; ++i) {
    SubdivCCGCoord *boundary_coords = adjacent_edge->boundary_coords[i];
    /* One step into the grid from the edge for each adjacent face. */
    SubdivCCGCoord grid_coord = boundary_coords[point_index];
    r_neighbors->coords[i + 2] = coord_step_inside_from_boundary(subdiv_ccg, &grid_coord);

    if (grid_coord.grid_index == coord->grid_index) {
      /* Prev and next along the edge for the current grid. */
      r_neighbors->coords[0] = boundary_coords[prev_point_index];
      r_neighbors->coords[1] = boundary_coords[next_point_index];
    }
    else if (include_duplicates) {
      /* Same coordinate on neighboring grids if requested. */
      r_neighbors->coords[duplicate_i + 2] = grid_coord;
      duplicate_i++;
    }
  }
}

/* The corner is at the middle of edge between faces. */
static void neighbor_coords_corner_edge_get(const SubdivCCG *subdiv_ccg,
                                            const SubdivCCGCoord *coord,
                                            const bool include_duplicates,
                                            SubdivCCGNeighbors *r_neighbors)
{
  neighbor_coords_edge_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
}

/* Input coordinate is at one of 4 corners of its grid corners. */
static void neighbor_coords_corner_get(const SubdivCCG *subdiv_ccg,
                                       const SubdivCCGCoord *coord,
                                       const bool include_duplicates,
                                       SubdivCCGNeighbors *r_neighbors)
{
  if (coord->x == 0 && coord->y == 0) {
    neighbor_coords_corner_center_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
  else {
    const int grid_size_1 = subdiv_ccg->grid_size - 1;
    if (coord->x == grid_size_1 && coord->y == grid_size_1) {
      neighbor_coords_corner_vertex_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
    }
    else {
      neighbor_coords_corner_edge_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
    }
  }
}

/* Simple case of getting neighbors of a boundary coordinate: the input coordinate is at the
 * boundary between two grids of the same face and there is no need to check adjacency with
 * other faces. */
static void neighbor_coords_boundary_inner_get(const SubdivCCG *subdiv_ccg,
                                               const SubdivCCGCoord *coord,
                                               const bool include_duplicates,
                                               SubdivCCGNeighbors *r_neighbors)
{
  subdiv_ccg_neighbors_init(r_neighbors, 4, (include_duplicates) ? 1 : 0);

  if (coord->x == 0) {
    r_neighbors->coords[0] = coord_at_prev_row(subdiv_ccg, coord);
    r_neighbors->coords[1] = coord_at_next_row(subdiv_ccg, coord);
    r_neighbors->coords[2] = coord_at_next_col(subdiv_ccg, coord);

    r_neighbors->coords[3].grid_index = prev_grid_index_from_coord(subdiv_ccg, coord);
    r_neighbors->coords[3].x = coord->y;
    r_neighbors->coords[3].y = 1;

    if (include_duplicates) {
      r_neighbors->coords[4] = r_neighbors->coords[3];
      r_neighbors->coords[4].y = 0;
    }
  }
  else if (coord->y == 0) {
    r_neighbors->coords[0] = coord_at_prev_col(subdiv_ccg, coord);
    r_neighbors->coords[1] = coord_at_next_col(subdiv_ccg, coord);
    r_neighbors->coords[2] = coord_at_next_row(subdiv_ccg, coord);

    r_neighbors->coords[3].grid_index = next_grid_index_from_coord(subdiv_ccg, coord);
    r_neighbors->coords[3].x = 1;
    r_neighbors->coords[3].y = coord->x;

    if (include_duplicates) {
      r_neighbors->coords[4] = r_neighbors->coords[3];
      r_neighbors->coords[4].x = 0;
    }
  }
}

/* Input coordinate is on an edge between two faces. Need to check adjacency. */
static void neighbor_coords_boundary_outer_get(const SubdivCCG *subdiv_ccg,
                                               const SubdivCCGCoord *coord,
                                               const bool include_duplicates,
                                               SubdivCCGNeighbors *r_neighbors)
{
  neighbor_coords_edge_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
}

/* Input coordinate is at one of 4 boundaries of its grid.
 * It could either be an inner boundary (which connects face center to the face edge) or could be
 * a part of coarse face edge. */
static void neighbor_coords_boundary_get(const SubdivCCG *subdiv_ccg,
                                         const SubdivCCGCoord *coord,
                                         const bool include_duplicates,
                                         SubdivCCGNeighbors *r_neighbors)
{
  if (is_inner_edge_grid_coordinate(subdiv_ccg, coord)) {
    neighbor_coords_boundary_inner_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
  else {
    neighbor_coords_boundary_outer_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
}

/* Input coordinate is inside of its grid, all the neighbors belong to the same grid. */
static void neighbor_coords_inner_get(const SubdivCCG *subdiv_ccg,
                                      const SubdivCCGCoord *coord,
                                      SubdivCCGNeighbors *r_neighbors)
{
  subdiv_ccg_neighbors_init(r_neighbors, 4, 0);

  r_neighbors->coords[0] = coord_at_prev_row(subdiv_ccg, coord);
  r_neighbors->coords[1] = coord_at_next_row(subdiv_ccg, coord);
  r_neighbors->coords[2] = coord_at_prev_col(subdiv_ccg, coord);
  r_neighbors->coords[3] = coord_at_next_col(subdiv_ccg, coord);
}

void BKE_subdiv_ccg_neighbor_coords_get(const SubdivCCG *subdiv_ccg,
                                        const SubdivCCGCoord *coord,
                                        const bool include_duplicates,
                                        SubdivCCGNeighbors *r_neighbors)
{
  BLI_assert(coord->grid_index >= 0);
  BLI_assert(coord->grid_index < subdiv_ccg->num_grids);
  BLI_assert(coord->x >= 0);
  BLI_assert(coord->x < subdiv_ccg->grid_size);
  BLI_assert(coord->y >= 0);
  BLI_assert(coord->y < subdiv_ccg->grid_size);

  if (is_corner_grid_coord(subdiv_ccg, coord)) {
    neighbor_coords_corner_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
  else if (is_boundary_grid_coord(subdiv_ccg, coord)) {
    neighbor_coords_boundary_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
  else {
    neighbor_coords_inner_get(subdiv_ccg, coord, r_neighbors);
  }

#ifndef NDEBUG
  for (int i = 0; i < r_neighbors->size; i++) {
    BLI_assert(BKE_subdiv_ccg_check_coord_valid(subdiv_ccg, &r_neighbors->coords[i]));
  }
#endif
}

int BKE_subdiv_ccg_grid_to_face_index(const SubdivCCG *subdiv_ccg, const int grid_index)
{
  const SubdivCCGFace *face = subdiv_ccg->grid_faces[grid_index];
  const int face_index = face - subdiv_ccg->faces;
  return face_index;
}

/** \} */
