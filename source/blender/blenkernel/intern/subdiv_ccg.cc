/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv_ccg.hh"

#include "MEM_guardedalloc.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_bits.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_set.hh"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "BKE_DerivedMesh.hh"
#include "BKE_ccg.h"
#include "BKE_mesh.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_eval.hh"

#include "opensubdiv_topology_refiner_capi.hh"

using blender::Array;
using blender::float3;
using blender::GrainSize;
using blender::IndexMask;
using blender::IndexMaskMemory;
using blender::IndexRange;
using blender::MutableSpan;
using blender::OffsetIndices;
using blender::Span;
using blender::Vector;
using blender::VectorSet;
using namespace blender::bke::subdiv;

/* -------------------------------------------------------------------- */
/** \name Various forward declarations
 * \{ */

#ifdef WITH_OPENSUBDIV

static void subdiv_ccg_average_inner_face_grids(SubdivCCG &subdiv_ccg,
                                                const CCGKey &key,
                                                const IndexRange face);

void subdiv_ccg_average_faces_boundaries_and_corners(SubdivCCG &subdiv_ccg,
                                                     const CCGKey &key,
                                                     const IndexMask &face_mask);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generally useful internal helpers
 * \{ */

/* Number of floats in per-vertex elements. */
static int num_element_float_get(const SubdivCCG &subdiv_ccg)
{
  /* We always have 3 floats for coordinate. */
  int num_floats = 3;
  if (subdiv_ccg.has_normal) {
    num_floats += 3;
  }
  if (subdiv_ccg.has_mask) {
    num_floats += 1;
  }
  return num_floats;
}

/* Per-vertex element size in bytes. */
static int element_size_bytes_get(const SubdivCCG &subdiv_ccg)
{
  return sizeof(float) * num_element_float_get(subdiv_ccg);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal helpers for CCG creation
 * \{ */

static void subdiv_ccg_init_layers(SubdivCCG &subdiv_ccg, const SubdivToCCGSettings &settings)
{
  /* CCG always contains coordinates. Rest of layers are coming after them. */
  int layer_offset = sizeof(float[3]);
  /* Mask. */
  if (settings.need_mask) {
    subdiv_ccg.has_mask = true;
    subdiv_ccg.mask_offset = layer_offset;
    layer_offset += sizeof(float);
  }
  else {
    subdiv_ccg.has_mask = false;
    subdiv_ccg.mask_offset = -1;
  }
  /* Normals.
   *
   * NOTE: Keep them at the end, matching old CCGDM. Doesn't really matter
   * here, but some other area might in theory depend memory layout. */
  if (settings.need_normal) {
    subdiv_ccg.has_normal = true;
    subdiv_ccg.normal_offset = layer_offset;
    layer_offset += sizeof(float[3]);
  }
  else {
    subdiv_ccg.has_normal = false;
    subdiv_ccg.normal_offset = -1;
  }
}

/* TODO(sergey): Make it more accessible function. */
static int topology_refiner_count_face_corners(const OpenSubdiv_TopologyRefiner *topology_refiner)
{
  const int num_faces = topology_refiner->getNumFaces();
  int num_corners = 0;
  for (int face_index = 0; face_index < num_faces; face_index++) {
    num_corners += topology_refiner->getNumFaceVertices(face_index);
  }
  return num_corners;
}

/* NOTE: Grid size and layer flags are to be filled in before calling this
 * function. */
static void subdiv_ccg_alloc_elements(SubdivCCG &subdiv_ccg, Subdiv &subdiv)
{
  const OpenSubdiv_TopologyRefiner *topology_refiner = subdiv.topology_refiner;
  const int64_t element_size = element_size_bytes_get(subdiv_ccg);
  /* Allocate memory for surface grids. */
  const int64_t num_grids = topology_refiner_count_face_corners(topology_refiner);
  const int64_t grid_size = grid_size_from_level(subdiv_ccg.level);
  const int64_t grid_area = grid_size * grid_size;
  subdiv_ccg.grid_element_size = element_size;
  subdiv_ccg.grids.reinitialize(num_grids);
  subdiv_ccg.grids_storage.reinitialize(num_grids * grid_area * element_size);
  const size_t grid_size_in_bytes = size_t(grid_area) * element_size;
  for (int grid_index = 0; grid_index < num_grids; grid_index++) {
    const size_t grid_offset = grid_size_in_bytes * grid_index;
    subdiv_ccg.grids[grid_index] = (CCGElem *)&subdiv_ccg.grids_storage[grid_offset];
  }
  /* TODO(sergey): Allocate memory for loose elements. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grids evaluation
 * \{ */

static void subdiv_ccg_eval_grid_element_limit(Subdiv &subdiv,
                                               SubdivCCG &subdiv_ccg,
                                               const int ptex_face_index,
                                               const float u,
                                               const float v,
                                               uchar *element)
{
  if (subdiv.displacement_evaluator != nullptr) {
    eval_final_point(&subdiv, ptex_face_index, u, v, (float *)element);
  }
  else if (subdiv_ccg.has_normal) {
    eval_limit_point_and_normal(&subdiv,
                                ptex_face_index,
                                u,
                                v,
                                (float *)element,
                                (float *)(element + subdiv_ccg.normal_offset));
  }
  else {
    eval_limit_point(&subdiv, ptex_face_index, u, v, (float *)element);
  }
}

static void subdiv_ccg_eval_grid_element_mask(SubdivCCG &subdiv_ccg,
                                              SubdivCCGMaskEvaluator *mask_evaluator,
                                              const int ptex_face_index,
                                              const float u,
                                              const float v,
                                              uchar *element)
{
  if (!subdiv_ccg.has_mask) {
    return;
  }
  float *mask_value_ptr = (float *)(element + subdiv_ccg.mask_offset);
  if (mask_evaluator != nullptr) {
    *mask_value_ptr = mask_evaluator->eval_mask(mask_evaluator, ptex_face_index, u, v);
  }
  else {
    *mask_value_ptr = 0.0f;
  }
}

static void subdiv_ccg_eval_grid_element(Subdiv &subdiv,
                                         SubdivCCG &subdiv_ccg,
                                         SubdivCCGMaskEvaluator *mask_evaluator,
                                         const int ptex_face_index,
                                         const float u,
                                         const float v,
                                         uchar *element)
{
  subdiv_ccg_eval_grid_element_limit(subdiv, subdiv_ccg, ptex_face_index, u, v, element);
  subdiv_ccg_eval_grid_element_mask(subdiv_ccg, mask_evaluator, ptex_face_index, u, v, element);
}

static void subdiv_ccg_eval_regular_grid(Subdiv &subdiv,
                                         SubdivCCG &subdiv_ccg,
                                         const Span<int> face_ptex_offset,
                                         SubdivCCGMaskEvaluator *mask_evaluator,
                                         const int face_index)
{
  const int ptex_face_index = face_ptex_offset[face_index];
  const int grid_size = subdiv_ccg.grid_size;
  const float grid_size_1_inv = 1.0f / (grid_size - 1);
  const int element_size = element_size_bytes_get(subdiv_ccg);
  const IndexRange face = subdiv_ccg.faces[face_index];
  for (int corner = 0; corner < face.size(); corner++) {
    const int grid_index = face.start() + corner;
    uchar *grid = (uchar *)subdiv_ccg.grids[grid_index];
    for (int y = 0; y < grid_size; y++) {
      const float grid_v = y * grid_size_1_inv;
      for (int x = 0; x < grid_size; x++) {
        const float grid_u = x * grid_size_1_inv;
        float u, v;
        rotate_grid_to_quad(corner, grid_u, grid_v, &u, &v);
        const size_t grid_element_index = size_t(y) * grid_size + x;
        const size_t grid_element_offset = grid_element_index * element_size;
        subdiv_ccg_eval_grid_element(
            subdiv, subdiv_ccg, mask_evaluator, ptex_face_index, u, v, &grid[grid_element_offset]);
      }
    }
  }
}

static void subdiv_ccg_eval_special_grid(Subdiv &subdiv,
                                         SubdivCCG &subdiv_ccg,
                                         const Span<int> face_ptex_offset,
                                         SubdivCCGMaskEvaluator *mask_evaluator,
                                         const int face_index)
{
  const int grid_size = subdiv_ccg.grid_size;
  const float grid_size_1_inv = 1.0f / (grid_size - 1);
  const int element_size = element_size_bytes_get(subdiv_ccg);
  const IndexRange face = subdiv_ccg.faces[face_index];
  for (int corner = 0; corner < face.size(); corner++) {
    const int grid_index = face.start() + corner;
    const int ptex_face_index = face_ptex_offset[face_index] + corner;
    uchar *grid = (uchar *)subdiv_ccg.grids[grid_index];
    for (int y = 0; y < grid_size; y++) {
      const float u = 1.0f - (y * grid_size_1_inv);
      for (int x = 0; x < grid_size; x++) {
        const float v = 1.0f - (x * grid_size_1_inv);
        const size_t grid_element_index = size_t(y) * grid_size + x;
        const size_t grid_element_offset = grid_element_index * element_size;
        subdiv_ccg_eval_grid_element(
            subdiv, subdiv_ccg, mask_evaluator, ptex_face_index, u, v, &grid[grid_element_offset]);
      }
    }
  }
}

static bool subdiv_ccg_evaluate_grids(SubdivCCG &subdiv_ccg,
                                      Subdiv &subdiv,
                                      SubdivCCGMaskEvaluator *mask_evaluator)
{
  using namespace blender;
  const OpenSubdiv_TopologyRefiner *topology_refiner = subdiv.topology_refiner;
  const int num_faces = topology_refiner->getNumFaces();
  const Span<int> face_ptex_offset(face_ptex_offset_get(&subdiv), subdiv_ccg.faces.size());
  threading::parallel_for(IndexRange(num_faces), 1024, [&](const IndexRange range) {
    for (const int face_index : range) {
      if (subdiv_ccg.faces[face_index].size() == 4) {
        subdiv_ccg_eval_regular_grid(
            subdiv, subdiv_ccg, face_ptex_offset, mask_evaluator, face_index);
      }
      else {
        subdiv_ccg_eval_special_grid(
            subdiv, subdiv_ccg, face_ptex_offset, mask_evaluator, face_index);
      }
    }
  });
  /* If displacement is used, need to calculate normals after all final
   * coordinates are known. */
  if (subdiv.displacement_evaluator != nullptr) {
    BKE_subdiv_ccg_recalc_normals(subdiv_ccg);
  }
  return true;
}

static void subdiv_ccg_allocate_adjacent_edges(SubdivCCG &subdiv_ccg, const int num_edges)
{
  subdiv_ccg.adjacent_edges = Array<SubdivCCGAdjacentEdge>(num_edges, SubdivCCGAdjacentEdge{});
}

static SubdivCCGCoord subdiv_ccg_coord(int grid_index, int x, int y)
{
  SubdivCCGCoord coord{};
  coord.grid_index = grid_index;
  coord.x = x;
  coord.y = y;
  return coord;
}

static CCGElem *subdiv_ccg_coord_to_elem(const CCGKey &key,
                                         const SubdivCCG &subdiv_ccg,
                                         const SubdivCCGCoord &coord)
{
  return CCG_grid_elem(&key, subdiv_ccg.grids[coord.grid_index], coord.x, coord.y);
}

/* Returns storage where boundary elements are to be stored. */
static SubdivCCGCoord *subdiv_ccg_adjacent_edge_add_face(SubdivCCG &subdiv_ccg,
                                                         SubdivCCGAdjacentEdge &adjacent_edge)
{
  const int grid_size = subdiv_ccg.grid_size * 2;
  const int adjacent_face_index = adjacent_edge.num_adjacent_faces;
  ++adjacent_edge.num_adjacent_faces;
  /* Allocate memory for the boundary elements. */
  adjacent_edge.boundary_coords = static_cast<SubdivCCGCoord **>(
      MEM_reallocN(adjacent_edge.boundary_coords,
                   adjacent_edge.num_adjacent_faces * sizeof(*adjacent_edge.boundary_coords)));
  adjacent_edge.boundary_coords[adjacent_face_index] = static_cast<SubdivCCGCoord *>(
      MEM_malloc_arrayN(grid_size * 2, sizeof(SubdivCCGCoord), "ccg adjacent boundary"));
  return adjacent_edge.boundary_coords[adjacent_face_index];
}

static void subdiv_ccg_init_faces_edge_neighborhood(SubdivCCG &subdiv_ccg)
{
  Subdiv *subdiv = subdiv_ccg.subdiv;
  const OffsetIndices<int> faces = subdiv_ccg.faces;
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  const int num_edges = topology_refiner->getNumEdges();
  const int grid_size = subdiv_ccg.grid_size;
  if (num_edges == 0) {
    /* Early output, nothing to do in this case. */
    return;
  }
  subdiv_ccg_allocate_adjacent_edges(subdiv_ccg, num_edges);

  Vector<int, 64> face_vertices;
  Vector<int, 64> face_edges;
  /* Store adjacency for all faces. */
  for (const int face_index : faces.index_range()) {
    const IndexRange face = faces[face_index];
    const int num_face_grids = face.size();
    face_vertices.reinitialize(num_face_grids);
    topology_refiner->getFaceVertices(face_index, face_vertices.data());
    /* Note that order of edges is same as order of MLoops, which also
     * means it's the same as order of grids. */
    face_edges.reinitialize(num_face_grids);
    topology_refiner->getFaceEdges(face_index, face_edges.data());
    /* Store grids adjacency for this edge. */
    for (int corner = 0; corner < num_face_grids; corner++) {
      const int vertex_index = face_vertices[corner];
      const int edge_index = face_edges[corner];
      int edge_vertices[2];
      topology_refiner->getEdgeVertices(edge_index, edge_vertices);
      const bool is_edge_flipped = (edge_vertices[0] != vertex_index);
      /* Grid which is adjacent to the current corner. */
      const int current_grid_index = face.start() + corner;
      /* Grid which is adjacent to the next corner. */
      const int next_grid_index = face.start() + (corner + 1) % num_face_grids;
      /* Add new face to the adjacent edge. */
      SubdivCCGAdjacentEdge &adjacent_edge = subdiv_ccg.adjacent_edges[edge_index];
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
}

static void subdiv_ccg_allocate_adjacent_vertices(SubdivCCG &subdiv_ccg, const int num_vertices)
{
  subdiv_ccg.adjacent_verts = Array<SubdivCCGAdjacentVertex>(num_vertices,
                                                             SubdivCCGAdjacentVertex{});
}

/* Returns storage where corner elements are to be stored. This is a pointer
 * to the actual storage. */
static SubdivCCGCoord *subdiv_ccg_adjacent_vertex_add_face(
    SubdivCCGAdjacentVertex &adjacent_vertex)
{
  const int adjacent_face_index = adjacent_vertex.num_adjacent_faces;
  ++adjacent_vertex.num_adjacent_faces;
  /* Allocate memory for the boundary elements. */
  adjacent_vertex.corner_coords = static_cast<SubdivCCGCoord *>(
      MEM_reallocN(adjacent_vertex.corner_coords,
                   adjacent_vertex.num_adjacent_faces * sizeof(*adjacent_vertex.corner_coords)));
  return &adjacent_vertex.corner_coords[adjacent_face_index];
}

static void subdiv_ccg_init_faces_vertex_neighborhood(SubdivCCG &subdiv_ccg)
{
  Subdiv *subdiv = subdiv_ccg.subdiv;
  const OffsetIndices<int> faces = subdiv_ccg.faces;
  OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
  const int num_vertices = topology_refiner->getNumVertices();
  const int grid_size = subdiv_ccg.grid_size;
  if (num_vertices == 0) {
    /* Early output, nothing to do in this case. */
    return;
  }
  subdiv_ccg_allocate_adjacent_vertices(subdiv_ccg, num_vertices);
  Vector<int, 64> face_vertices;
  /* Store adjacency for all faces. */
  for (const int face_index : faces.index_range()) {
    const IndexRange face = faces[face_index];
    const int num_face_grids = face.size();
    face_vertices.reinitialize(num_face_grids);
    topology_refiner->getFaceVertices(face_index, face_vertices.data());
    for (int corner = 0; corner < num_face_grids; corner++) {
      const int vertex_index = face_vertices[corner];
      /* Grid which is adjacent to the current corner. */
      const int grid_index = face.start() + corner;
      /* Add new face to the adjacent edge. */
      SubdivCCGAdjacentVertex &adjacent_vertex = subdiv_ccg.adjacent_verts[vertex_index];
      SubdivCCGCoord *corner_coord = subdiv_ccg_adjacent_vertex_add_face(adjacent_vertex);
      *corner_coord = subdiv_ccg_coord(grid_index, grid_size - 1, grid_size - 1);
    }
  }
}

static void subdiv_ccg_init_faces_neighborhood(SubdivCCG &subdiv_ccg)
{
  subdiv_ccg_init_faces_edge_neighborhood(subdiv_ccg);
  subdiv_ccg_init_faces_vertex_neighborhood(subdiv_ccg);
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation / evaluation
 * \{ */

std::unique_ptr<SubdivCCG> BKE_subdiv_to_ccg(Subdiv &subdiv,
                                             const SubdivToCCGSettings &settings,
                                             const Mesh &coarse_mesh,
                                             SubdivCCGMaskEvaluator *mask_evaluator)
{
#ifdef WITH_OPENSUBDIV
  stats_begin(&subdiv.stats, SUBDIV_STATS_SUBDIV_TO_CCG);
  std::unique_ptr<SubdivCCG> subdiv_ccg = std::make_unique<SubdivCCG>();
  subdiv_ccg->subdiv = &subdiv;
  subdiv_ccg->level = bitscan_forward_i(settings.resolution - 1);
  subdiv_ccg->grid_size = grid_size_from_level(subdiv_ccg->level);
  subdiv_ccg_init_layers(*subdiv_ccg, settings);
  subdiv_ccg->faces = coarse_mesh.faces();
  subdiv_ccg->grid_to_face_map = coarse_mesh.corner_to_face_map();
  subdiv_ccg_alloc_elements(*subdiv_ccg, subdiv);
  subdiv_ccg_init_faces_neighborhood(*subdiv_ccg);
  if (!subdiv_ccg_evaluate_grids(*subdiv_ccg, subdiv, mask_evaluator)) {
    stats_end(&subdiv.stats, SUBDIV_STATS_SUBDIV_TO_CCG);
    return nullptr;
  }
  stats_end(&subdiv.stats, SUBDIV_STATS_SUBDIV_TO_CCG);
  return subdiv_ccg;
#else
  UNUSED_VARS(subdiv, settings, coarse_mesh, mask_evaluator);
  return {};
#endif
}

Mesh *BKE_subdiv_to_ccg_mesh(Subdiv &subdiv,
                             const SubdivToCCGSettings &settings,
                             const Mesh &coarse_mesh)
{
  /* Make sure evaluator is ready. */
  stats_begin(&subdiv.stats, SUBDIV_STATS_SUBDIV_TO_CCG);
  if (!eval_begin_from_mesh(&subdiv, &coarse_mesh, nullptr, SUBDIV_EVALUATOR_TYPE_CPU, nullptr)) {
    if (coarse_mesh.faces_num) {
      return nullptr;
    }
  }
  stats_end(&subdiv.stats, SUBDIV_STATS_SUBDIV_TO_CCG);
  SubdivCCGMaskEvaluator mask_evaluator;
  bool has_mask = BKE_subdiv_ccg_mask_init_from_paint(&mask_evaluator, &coarse_mesh);
  std::unique_ptr<SubdivCCG> subdiv_ccg = BKE_subdiv_to_ccg(
      subdiv, settings, coarse_mesh, has_mask ? &mask_evaluator : nullptr);
  if (has_mask) {
    mask_evaluator.free(&mask_evaluator);
  }
  if (!subdiv_ccg) {
    return nullptr;
  }
  Mesh *result = BKE_mesh_new_nomain_from_template(&coarse_mesh, 0, 0, 0, 0);
  result->runtime->subdiv_ccg = std::move(subdiv_ccg);
  return result;
}

SubdivCCG::~SubdivCCG()
{
  if (this->subdiv != nullptr) {
    free(this->subdiv);
  }

  for (const int i : this->adjacent_edges.index_range()) {
    SubdivCCGAdjacentEdge *adjacent_edge = &this->adjacent_edges[i];
    for (int face_index = 0; face_index < adjacent_edge->num_adjacent_faces; face_index++) {
      MEM_SAFE_FREE(adjacent_edge->boundary_coords[face_index]);
    }
    MEM_SAFE_FREE(adjacent_edge->boundary_coords);
  }

  for (const int i : this->adjacent_verts.index_range()) {
    SubdivCCGAdjacentVertex *adjacent_vertex = &this->adjacent_verts[i];
    MEM_SAFE_FREE(adjacent_vertex->corner_coords);
  }
}

CCGKey BKE_subdiv_ccg_key(const SubdivCCG &subdiv_ccg, int level)
{
#ifdef WITH_OPENSUBDIV
  CCGKey key;
  key.level = level;
  key.elem_size = element_size_bytes_get(subdiv_ccg);
  key.grid_size = grid_size_from_level(level);
  key.grid_area = key.grid_size * key.grid_size;
  key.grid_bytes = key.elem_size * key.grid_area;

  key.normal_offset = subdiv_ccg.normal_offset;
  key.mask_offset = subdiv_ccg.mask_offset;

  key.has_normals = subdiv_ccg.has_normal;
  key.has_mask = subdiv_ccg.has_mask;
  return key;
#else
  UNUSED_VARS(subdiv_ccg, level);
  return {};
#endif
}

CCGKey BKE_subdiv_ccg_key_top_level(const SubdivCCG &subdiv_ccg)
{
  return BKE_subdiv_ccg_key(subdiv_ccg, subdiv_ccg.level);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normals
 * \{ */

#ifdef WITH_OPENSUBDIV

/* Evaluate high-res face normals, for faces which corresponds to grid elements
 *
 *   {(x, y), {x + 1, y}, {x + 1, y + 1}, {x, y + 1}}
 *
 * The result is stored in normals storage from TLS. */
static void subdiv_ccg_recalc_inner_face_normals(const SubdivCCG &subdiv_ccg,
                                                 const CCGKey &key,
                                                 MutableSpan<float3> face_normals,
                                                 const int corner)
{
  const int grid_size = subdiv_ccg.grid_size;
  const int grid_size_1 = grid_size - 1;
  CCGElem *grid = subdiv_ccg.grids[corner];
  for (int y = 0; y < grid_size - 1; y++) {
    for (int x = 0; x < grid_size - 1; x++) {
      CCGElem *grid_elements[4] = {
          CCG_grid_elem(&key, grid, x, y + 1),
          CCG_grid_elem(&key, grid, x + 1, y + 1),
          CCG_grid_elem(&key, grid, x + 1, y),
          CCG_grid_elem(&key, grid, x, y),
      };
      float *co[4] = {
          CCG_elem_co(&key, grid_elements[0]),
          CCG_elem_co(&key, grid_elements[1]),
          CCG_elem_co(&key, grid_elements[2]),
          CCG_elem_co(&key, grid_elements[3]),
      };
      const int face_index = y * grid_size_1 + x;
      float *face_normal = face_normals[face_index];
      normal_quad_v3(face_normal, co[0], co[1], co[2], co[3]);
    }
  }
}

/* Average normals at every grid element, using adjacent faces normals. */
static void subdiv_ccg_average_inner_face_normals(const SubdivCCG &subdiv_ccg,
                                                  const CCGKey &key,
                                                  const Span<float3> face_normals,
                                                  const int corner)
{
  const int grid_size = subdiv_ccg.grid_size;
  const int grid_size_1 = grid_size - 1;
  CCGElem *grid = subdiv_ccg.grids[corner];
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
      mul_v3_v3fl(CCG_grid_elem_no(&key, grid, x, y), normal_acc, 1.0f / counter);
    }
  }
}

/* Recalculate normals which corresponds to non-boundaries elements of grids. */
static void subdiv_ccg_recalc_inner_grid_normals(SubdivCCG &subdiv_ccg, const IndexMask &face_mask)
{
  using namespace blender;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const int grid_size_1 = subdiv_ccg.grid_size - 1;
  threading::EnumerableThreadSpecific<Array<float3>> face_normals_tls(
      [&]() { return Array<float3>(grid_size_1 * grid_size_1); });

  const OffsetIndices<int> faces = subdiv_ccg.faces;
  face_mask.foreach_segment(GrainSize(512), [&](const IndexMaskSegment segment) {
    MutableSpan<float3> face_normals = face_normals_tls.local();
    for (const int face_index : segment) {
      const IndexRange face = faces[face_index];
      for (const int grid_index : face) {
        subdiv_ccg_recalc_inner_face_normals(subdiv_ccg, key, face_normals, grid_index);
        subdiv_ccg_average_inner_face_normals(subdiv_ccg, key, face_normals, grid_index);
      }
      subdiv_ccg_average_inner_face_grids(subdiv_ccg, key, face);
    }
  });
}

#endif

void BKE_subdiv_ccg_recalc_normals(SubdivCCG &subdiv_ccg)
{
#ifdef WITH_OPENSUBDIV
  if (!subdiv_ccg.has_normal) {
    /* Grids don't have normals, can do early output. */
    return;
  }
  subdiv_ccg_recalc_inner_grid_normals(subdiv_ccg, subdiv_ccg.faces.index_range());
  BKE_subdiv_ccg_average_grids(subdiv_ccg);
#else
  UNUSED_VARS(subdiv_ccg);
#endif
}

void BKE_subdiv_ccg_update_normals(SubdivCCG &subdiv_ccg, const IndexMask &face_mask)
{
#ifdef WITH_OPENSUBDIV
  if (!subdiv_ccg.has_normal) {
    /* Grids don't have normals, can do early output. */
    return;
  }
  if (face_mask.is_empty()) {
    /* No faces changed, so nothing to do here. */
    return;
  }
  subdiv_ccg_recalc_inner_grid_normals(subdiv_ccg, face_mask);

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  subdiv_ccg_average_faces_boundaries_and_corners(subdiv_ccg, key, face_mask);
#else
  UNUSED_VARS(subdiv_ccg, face_mask);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Boundary averaging/stitching
 * \{ */

#ifdef WITH_OPENSUBDIV

static void average_grid_element_value_v3(float a[3], float b[3])
{
  add_v3_v3(a, b);
  mul_v3_fl(a, 0.5f);
  copy_v3_v3(b, a);
}

static void average_grid_element(const SubdivCCG &subdiv_ccg,
                                 const CCGKey &key,
                                 CCGElem *grid_element_a,
                                 CCGElem *grid_element_b)
{
  average_grid_element_value_v3(CCG_elem_co(&key, grid_element_a),
                                CCG_elem_co(&key, grid_element_b));
  if (subdiv_ccg.has_normal) {
    average_grid_element_value_v3(CCG_elem_no(&key, grid_element_a),
                                  CCG_elem_no(&key, grid_element_b));
  }
  if (subdiv_ccg.has_mask) {
    float mask = (*CCG_elem_mask(&key, grid_element_a) + *CCG_elem_mask(&key, grid_element_b)) *
                 0.5f;
    *CCG_elem_mask(&key, grid_element_a) = mask;
    *CCG_elem_mask(&key, grid_element_b) = mask;
  }
}

/* Accumulator to hold data during averaging. */
struct GridElementAccumulator {
  float co[3];
  float no[3];
  float mask;
};

static void element_accumulator_init(GridElementAccumulator &accumulator)
{
  zero_v3(accumulator.co);
  zero_v3(accumulator.no);
  accumulator.mask = 0.0f;
}

static void element_accumulator_add(GridElementAccumulator &accumulator,
                                    const SubdivCCG &subdiv_ccg,
                                    const CCGKey &key,
                                    /*const*/ CCGElem &grid_element)
{
  add_v3_v3(accumulator.co, CCG_elem_co(&key, &grid_element));
  if (subdiv_ccg.has_normal) {
    add_v3_v3(accumulator.no, CCG_elem_no(&key, &grid_element));
  }
  if (subdiv_ccg.has_mask) {
    accumulator.mask += *CCG_elem_mask(&key, &grid_element);
  }
}

static void element_accumulator_mul_fl(GridElementAccumulator &accumulator, const float f)
{
  mul_v3_fl(accumulator.co, f);
  mul_v3_fl(accumulator.no, f);
  accumulator.mask *= f;
}

static void element_accumulator_copy(const SubdivCCG &subdiv_ccg,
                                     const CCGKey &key,
                                     CCGElem &destination,
                                     const GridElementAccumulator &accumulator)
{
  copy_v3_v3(CCG_elem_co(&key, &destination), accumulator.co);
  if (subdiv_ccg.has_normal) {
    copy_v3_v3(CCG_elem_no(&key, &destination), accumulator.no);
  }
  if (subdiv_ccg.has_mask) {
    *CCG_elem_mask(&key, &destination) = accumulator.mask;
  }
}

static void subdiv_ccg_average_inner_face_grids(SubdivCCG &subdiv_ccg,
                                                const CCGKey &key,
                                                const IndexRange face)
{
  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const int num_face_grids = face.size();
  const int grid_size = subdiv_ccg.grid_size;
  CCGElem *prev_grid = grids[face.start() + num_face_grids - 1];
  /* Average boundary between neighbor grid. */
  for (int corner = 0; corner < num_face_grids; corner++) {
    CCGElem *grid = grids[face.start() + corner];
    for (int i = 1; i < grid_size; i++) {
      CCGElem *prev_grid_element = CCG_grid_elem(&key, prev_grid, i, 0);
      CCGElem *grid_element = CCG_grid_elem(&key, grid, 0, i);
      average_grid_element(subdiv_ccg, key, prev_grid_element, grid_element);
    }
    prev_grid = grid;
  }
  /* Average all grids centers into a single accumulator, and share it.
   * Guarantees correct and smooth averaging in the center. */
  GridElementAccumulator center_accumulator;
  element_accumulator_init(center_accumulator);
  for (int corner = 0; corner < num_face_grids; corner++) {
    CCGElem *grid = grids[face.start() + corner];
    CCGElem *grid_center_element = CCG_grid_elem(&key, grid, 0, 0);
    element_accumulator_add(center_accumulator, subdiv_ccg, key, *grid_center_element);
  }
  element_accumulator_mul_fl(center_accumulator, 1.0f / num_face_grids);
  for (int corner = 0; corner < num_face_grids; corner++) {
    CCGElem *grid = grids[face.start() + corner];
    CCGElem *grid_center_element = CCG_grid_elem(&key, grid, 0, 0);
    element_accumulator_copy(subdiv_ccg, key, *grid_center_element, center_accumulator);
  }
}

static void subdiv_ccg_average_grids_boundary(SubdivCCG &subdiv_ccg,
                                              const CCGKey &key,
                                              const SubdivCCGAdjacentEdge &adjacent_edge,
                                              MutableSpan<GridElementAccumulator> accumulators)
{
  const int num_adjacent_faces = adjacent_edge.num_adjacent_faces;
  const int grid_size2 = subdiv_ccg.grid_size * 2;
  if (num_adjacent_faces == 1) {
    /* Nothing to average with. */
    return;
  }
  for (int i = 1; i < grid_size2 - 1; i++) {
    element_accumulator_init(accumulators[i]);
  }
  for (int face_index = 0; face_index < num_adjacent_faces; face_index++) {
    for (int i = 1; i < grid_size2 - 1; i++) {
      CCGElem *grid_element = subdiv_ccg_coord_to_elem(
          key, subdiv_ccg, adjacent_edge.boundary_coords[face_index][i]);
      element_accumulator_add(accumulators[i], subdiv_ccg, key, *grid_element);
    }
  }
  for (int i = 1; i < grid_size2 - 1; i++) {
    element_accumulator_mul_fl(accumulators[i], 1.0f / num_adjacent_faces);
  }
  /* Copy averaged value to all the other faces. */
  for (int face_index = 0; face_index < num_adjacent_faces; face_index++) {
    for (int i = 1; i < grid_size2 - 1; i++) {
      CCGElem *grid_element = subdiv_ccg_coord_to_elem(
          key, subdiv_ccg, adjacent_edge.boundary_coords[face_index][i]);
      element_accumulator_copy(subdiv_ccg, key, *grid_element, accumulators[i]);
    }
  }
}

struct AverageGridsCornerData {
  SubdivCCG *subdiv_ccg;
  CCGKey *key;

  /* Optional lookup table. Maps task range index to index in `subdiv_ccg.adjacent_verts`. */
  const int *adjacent_vert_index_map;
};

static void subdiv_ccg_average_grids_corners(SubdivCCG &subdiv_ccg,
                                             const CCGKey &key,
                                             const SubdivCCGAdjacentVertex &adjacent_vertex)
{
  const int num_adjacent_faces = adjacent_vertex.num_adjacent_faces;
  if (num_adjacent_faces == 1) {
    /* Nothing to average with. */
    return;
  }
  GridElementAccumulator accumulator;
  element_accumulator_init(accumulator);
  for (int face_index = 0; face_index < num_adjacent_faces; face_index++) {
    CCGElem *grid_element = subdiv_ccg_coord_to_elem(
        key, subdiv_ccg, adjacent_vertex.corner_coords[face_index]);
    element_accumulator_add(accumulator, subdiv_ccg, key, *grid_element);
  }
  element_accumulator_mul_fl(accumulator, 1.0f / num_adjacent_faces);
  /* Copy averaged value to all the other faces. */
  for (int face_index = 0; face_index < num_adjacent_faces; face_index++) {
    CCGElem *grid_element = subdiv_ccg_coord_to_elem(
        key, subdiv_ccg, adjacent_vertex.corner_coords[face_index]);
    element_accumulator_copy(subdiv_ccg, key, *grid_element, accumulator);
  }
}

static void subdiv_ccg_average_boundaries(SubdivCCG &subdiv_ccg,
                                          const CCGKey &key,
                                          const IndexMask &adjacent_edge_mask)
{
  using namespace blender;
  threading::EnumerableThreadSpecific<Array<GridElementAccumulator>> all_accumulators(
      [&]() { return Array<GridElementAccumulator>(subdiv_ccg.grid_size * 2); });

  adjacent_edge_mask.foreach_segment(GrainSize(1024), [&](const IndexMaskSegment segment) {
    MutableSpan<GridElementAccumulator> accumulators = all_accumulators.local();
    for (const int i : segment) {
      const SubdivCCGAdjacentEdge &adjacent_edge = subdiv_ccg.adjacent_edges[i];
      subdiv_ccg_average_grids_boundary(subdiv_ccg, key, adjacent_edge, accumulators);
    }
  });
}

static void subdiv_ccg_average_corners(SubdivCCG &subdiv_ccg,
                                       const CCGKey &key,
                                       const IndexMask &adjacent_vert_mask)
{
  using namespace blender;
  adjacent_vert_mask.foreach_index(GrainSize(1024), [&](const int i) {
    const SubdivCCGAdjacentVertex &adjacent_vert = subdiv_ccg.adjacent_verts[i];
    subdiv_ccg_average_grids_corners(subdiv_ccg, key, adjacent_vert);
  });
}

#endif

void BKE_subdiv_ccg_average_grids(SubdivCCG &subdiv_ccg)
{
#ifdef WITH_OPENSUBDIV
  using namespace blender;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  /* Average inner boundaries of grids (within one face), across faces
   * from different face-corners. */
  BKE_subdiv_ccg_average_stitch_faces(subdiv_ccg, subdiv_ccg.faces.index_range());
  subdiv_ccg_average_boundaries(subdiv_ccg, key, subdiv_ccg.adjacent_edges.index_range());
  subdiv_ccg_average_corners(subdiv_ccg, key, subdiv_ccg.adjacent_verts.index_range());
#else
  UNUSED_VARS(subdiv_ccg);
#endif
}

#ifdef WITH_OPENSUBDIV

static void subdiv_ccg_affected_face_adjacency(SubdivCCG &subdiv_ccg,
                                               const IndexMask &face_mask,
                                               blender::Set<int> &adjacent_verts,
                                               blender::Set<int> &adjacent_edges)
{
  Subdiv *subdiv = subdiv_ccg.subdiv;
  const OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;

  Vector<int, 64> face_vertices;
  Vector<int, 64> face_edges;

  face_mask.foreach_index([&](const int face_index) {
    const int num_face_grids = subdiv_ccg.faces[face_index].size();
    face_vertices.reinitialize(num_face_grids);
    topology_refiner->getFaceVertices(face_index, face_vertices.data());
    adjacent_verts.add_multiple(face_vertices);

    face_edges.reinitialize(num_face_grids);
    topology_refiner->getFaceEdges(face_index, face_edges.data());
    adjacent_edges.add_multiple(face_edges);
  });
}

void subdiv_ccg_average_faces_boundaries_and_corners(SubdivCCG &subdiv_ccg,
                                                     const CCGKey &key,
                                                     const IndexMask &face_mask)
{
  blender::Set<int> adjacent_vert_set;
  blender::Set<int> adjacent_edge_set;
  subdiv_ccg_affected_face_adjacency(subdiv_ccg, face_mask, adjacent_vert_set, adjacent_edge_set);

  Vector<int> adjacent_verts(adjacent_vert_set.begin(), adjacent_vert_set.end());
  Vector<int> adjacent_edges(adjacent_edge_set.begin(), adjacent_edge_set.end());

  std::sort(adjacent_verts.begin(), adjacent_verts.end());
  std::sort(adjacent_edges.begin(), adjacent_edges.end());

  IndexMaskMemory memory;
  subdiv_ccg_average_boundaries(
      subdiv_ccg, key, IndexMask::from_indices(adjacent_edges.as_span(), memory));

  subdiv_ccg_average_corners(
      subdiv_ccg, key, IndexMask::from_indices(adjacent_verts.as_span(), memory));
}

#endif

void BKE_subdiv_ccg_average_stitch_faces(SubdivCCG &subdiv_ccg, const IndexMask &face_mask)
{
#ifdef WITH_OPENSUBDIV
  using namespace blender;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  face_mask.foreach_index(GrainSize(512), [&](const int face_index) {
    subdiv_ccg_average_inner_face_grids(subdiv_ccg, key, subdiv_ccg.faces[face_index]);
  });
  /* TODO(sergey): Only average elements which are adjacent to modified
   * faces. */
  subdiv_ccg_average_boundaries(subdiv_ccg, key, subdiv_ccg.adjacent_edges.index_range());
  subdiv_ccg_average_corners(subdiv_ccg, key, subdiv_ccg.adjacent_verts.index_range());
#else
  UNUSED_VARS(subdiv_ccg, face_mask);
#endif
}

void BKE_subdiv_ccg_topology_counters(const SubdivCCG &subdiv_ccg,
                                      int &r_num_vertices,
                                      int &r_num_edges,
                                      int &r_num_faces,
                                      int &r_num_loops)
{
  const int num_grids = subdiv_ccg.grids.size();
  const int grid_size = subdiv_ccg.grid_size;
  const int grid_area = grid_size * grid_size;
  const int num_edges_per_grid = 2 * (grid_size * (grid_size - 1));
  r_num_vertices = num_grids * grid_area;
  r_num_edges = num_grids * num_edges_per_grid;
  r_num_faces = num_grids * (grid_size - 1) * (grid_size - 1);
  r_num_loops = r_num_faces * 4;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Neighbors
 * \{ */

void BKE_subdiv_ccg_print_coord(const char *message, const SubdivCCGCoord &coord)
{
  printf("%s: grid index: %d, coord: (%d, %d)\n", message, coord.grid_index, coord.x, coord.y);
}

bool BKE_subdiv_ccg_check_coord_valid(const SubdivCCG &subdiv_ccg, const SubdivCCGCoord &coord)
{
  if (coord.grid_index < 0 || coord.grid_index >= subdiv_ccg.grids.size()) {
    return false;
  }
  const int grid_size = subdiv_ccg.grid_size;
  if (coord.x < 0 || coord.x >= grid_size) {
    return false;
  }
  if (coord.y < 0 || coord.y >= grid_size) {
    return false;
  }
  return true;
}

BLI_INLINE void subdiv_ccg_neighbors_init(SubdivCCGNeighbors &neighbors,
                                          const int num_unique,
                                          const int num_duplicates)
{
  const int size = num_unique + num_duplicates;
  neighbors.coords.reinitialize(size);
  neighbors.num_duplicates = num_duplicates;
}

/* Check whether given coordinate belongs to a grid corner. */
BLI_INLINE bool is_corner_grid_coord(const SubdivCCG &subdiv_ccg, const SubdivCCGCoord &coord)
{
  const int grid_size_1 = subdiv_ccg.grid_size - 1;
  return (coord.x == 0 && coord.y == 0) || (coord.x == 0 && coord.y == grid_size_1) ||
         (coord.x == grid_size_1 && coord.y == grid_size_1) ||
         (coord.x == grid_size_1 && coord.y == 0);
}

/* Check whether given coordinate belongs to a grid boundary. */
BLI_INLINE bool is_boundary_grid_coord(const SubdivCCG &subdiv_ccg, const SubdivCCGCoord &coord)
{
  const int grid_size_1 = subdiv_ccg.grid_size - 1;
  return coord.x == 0 || coord.y == 0 || coord.x == grid_size_1 || coord.y == grid_size_1;
}

/* Check whether coordinate is at the boundary between two grids of the same face. */
BLI_INLINE bool is_inner_edge_grid_coordinate(const SubdivCCG &subdiv_ccg,
                                              const SubdivCCGCoord &coord)
{
  const int grid_size_1 = subdiv_ccg.grid_size - 1;
  if (coord.x == 0) {
    return coord.y > 0 && coord.y < grid_size_1;
  }
  if (coord.y == 0) {
    return coord.x > 0 && coord.x < grid_size_1;
  }
  return false;
}

BLI_INLINE SubdivCCGCoord coord_at_prev_row(const SubdivCCG & /*subdiv_ccg*/,
                                            const SubdivCCGCoord &coord)
{
  BLI_assert(coord.y > 0);
  SubdivCCGCoord result = coord;
  result.y -= 1;
  return result;
}
BLI_INLINE SubdivCCGCoord coord_at_next_row(const SubdivCCG &subdiv_ccg,
                                            const SubdivCCGCoord &coord)
{
  UNUSED_VARS_NDEBUG(subdiv_ccg);
  BLI_assert(coord.y < subdiv_ccg.grid_size - 1);
  SubdivCCGCoord result = coord;
  result.y += 1;
  return result;
}

BLI_INLINE SubdivCCGCoord coord_at_prev_col(const SubdivCCG & /*subdiv_ccg*/,
                                            const SubdivCCGCoord &coord)
{
  BLI_assert(coord.x > 0);
  SubdivCCGCoord result = coord;
  result.x -= 1;
  return result;
}
BLI_INLINE SubdivCCGCoord coord_at_next_col(const SubdivCCG &subdiv_ccg,
                                            const SubdivCCGCoord &coord)
{
  UNUSED_VARS_NDEBUG(subdiv_ccg);
  BLI_assert(coord.x < subdiv_ccg.grid_size - 1);
  SubdivCCGCoord result = coord;
  result.x += 1;
  return result;
}

#ifdef WITH_OPENSUBDIV

/* For the input coordinate which is at the boundary of the grid do one step inside. */
static SubdivCCGCoord coord_step_inside_from_boundary(const SubdivCCG &subdiv_ccg,
                                                      const SubdivCCGCoord &coord)

{
  SubdivCCGCoord result = coord;
  const int grid_size_1 = subdiv_ccg.grid_size - 1;
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
    BLI_assert_msg(0, "non-boundary element given");
  }
  return result;
}

BLI_INLINE
int next_grid_index_from_coord(const SubdivCCG &subdiv_ccg, const SubdivCCGCoord &coord)
{
  const IndexRange face = subdiv_ccg.faces[subdiv_ccg.grid_to_face_map[coord.grid_index]];
  const int face_grid_index = coord.grid_index;
  int next_face_grid_index = face_grid_index + 1 - face.start();
  if (next_face_grid_index == face.size()) {
    next_face_grid_index = 0;
  }
  return face.start() + next_face_grid_index;
}
BLI_INLINE int prev_grid_index_from_coord(const SubdivCCG &subdiv_ccg, const SubdivCCGCoord &coord)
{
  const IndexRange face = subdiv_ccg.faces[subdiv_ccg.grid_to_face_map[coord.grid_index]];
  const int face_grid_index = coord.grid_index;
  int prev_face_grid_index = face_grid_index - 1 - face.start();
  if (prev_face_grid_index < 0) {
    prev_face_grid_index = face.size() - 1;
  }
  return face.start() + prev_face_grid_index;
}

/* Simple case of getting neighbors of a corner coordinate: the corner is a face center, so
 * can only iterate over grid of a single face, without looking into adjacency. */
static void neighbor_coords_corner_center_get(const SubdivCCG &subdiv_ccg,
                                              const SubdivCCGCoord &coord,
                                              const bool include_duplicates,
                                              SubdivCCGNeighbors &r_neighbors)
{
  const IndexRange face = subdiv_ccg.faces[subdiv_ccg.grid_to_face_map[coord.grid_index]];
  const int num_adjacent_grids = face.size();

  subdiv_ccg_neighbors_init(
      r_neighbors, num_adjacent_grids, (include_duplicates) ? num_adjacent_grids - 1 : 0);

  int duplicate_face_grid_index = num_adjacent_grids;
  for (int face_grid_index = 0; face_grid_index < num_adjacent_grids; ++face_grid_index) {
    SubdivCCGCoord neighbor_coord;
    neighbor_coord.grid_index = face.start() + face_grid_index;
    neighbor_coord.x = 1;
    neighbor_coord.y = 0;
    r_neighbors.coords[face_grid_index] = neighbor_coord;

    if (include_duplicates && neighbor_coord.grid_index != coord.grid_index) {
      neighbor_coord.x = 0;
      r_neighbors.coords[duplicate_face_grid_index++] = neighbor_coord;
    }
  }
}

/* Get index within adjacent_verts array for the given CCG coordinate. */
static int adjacent_vertex_index_from_coord(const SubdivCCG &subdiv_ccg,
                                            const SubdivCCGCoord &coord)
{
  Subdiv *subdiv = subdiv_ccg.subdiv;
  const OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;

  const int face_index = subdiv_ccg.grid_to_face_map[coord.grid_index];
  const IndexRange face = subdiv_ccg.faces[face_index];
  const int face_grid_index = coord.grid_index - face.start();
  const int num_face_grids = face.size();

  Array<int, 64> face_vertices(num_face_grids);
  topology_refiner->getFaceVertices(face_index, face_vertices.data());

  const int adjacent_vertex_index = face_vertices[face_grid_index];
  return adjacent_vertex_index;
}

/* The corner is adjacent to a coarse vertex. */
static void neighbor_coords_corner_vertex_get(const SubdivCCG &subdiv_ccg,
                                              const SubdivCCGCoord &coord,
                                              const bool include_duplicates,
                                              SubdivCCGNeighbors &r_neighbors)
{
  Subdiv *subdiv = subdiv_ccg.subdiv;
  const OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;

  const int adjacent_vertex_index = adjacent_vertex_index_from_coord(subdiv_ccg, coord);
  const int num_vertex_edges = topology_refiner->getNumVertexEdges(adjacent_vertex_index);

  const SubdivCCGAdjacentVertex &adjacent_vert = subdiv_ccg.adjacent_verts[adjacent_vertex_index];
  const int num_adjacent_faces = adjacent_vert.num_adjacent_faces;

  subdiv_ccg_neighbors_init(
      r_neighbors, num_vertex_edges, (include_duplicates) ? num_adjacent_faces - 1 : 0);

  Array<int, 64> vertex_edges(num_vertex_edges);
  topology_refiner->getVertexEdges(adjacent_vertex_index, vertex_edges.data());

  for (int i = 0; i < num_vertex_edges; ++i) {
    const int edge_index = vertex_edges[i];

    /* Use very first grid of every edge. */
    const int edge_face_index = 0;

    /* Depending edge orientation we use first (zero-based) or previous-to-last point. */
    int edge_vertices_indices[2];
    topology_refiner->getEdgeVertices(edge_index, edge_vertices_indices);
    int edge_point_index, duplicate_edge_point_index;
    if (edge_vertices_indices[0] == adjacent_vertex_index) {
      duplicate_edge_point_index = 0;
      edge_point_index = duplicate_edge_point_index + 1;
    }
    else {
      /* Edge "consists" of 2 grids, which makes it 2 * grid_size elements per edge.
       * The index of last edge element is 2 * grid_size - 1 (due to zero-based indices),
       * and we are interested in previous to last element. */
      duplicate_edge_point_index = subdiv_ccg.grid_size * 2 - 1;
      edge_point_index = duplicate_edge_point_index - 1;
    }

    const SubdivCCGAdjacentEdge &adjacent_edge = subdiv_ccg.adjacent_edges[edge_index];
    r_neighbors.coords[i] = adjacent_edge.boundary_coords[edge_face_index][edge_point_index];
  }

  if (include_duplicates) {
    /* Add duplicates of the current grid vertex in adjacent faces if requested. */
    for (int i = 0, duplicate_i = num_vertex_edges; i < num_adjacent_faces; i++) {
      SubdivCCGCoord neighbor_coord = adjacent_vert.corner_coords[i];
      if (neighbor_coord.grid_index != coord.grid_index) {
        r_neighbors.coords[duplicate_i++] = neighbor_coord;
      }
    }
  }
}

static int adjacent_edge_index_from_coord(const SubdivCCG &subdiv_ccg, const SubdivCCGCoord &coord)
{
  Subdiv *subdiv = subdiv_ccg.subdiv;
  const OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;

  const int face_index = subdiv_ccg.grid_to_face_map[coord.grid_index];
  const IndexRange face = subdiv_ccg.faces[face_index];
  const int face_grid_index = coord.grid_index - face.start();
  const int num_face_edges = topology_refiner->getNumFaceEdges(face_index);

  Array<int, 64> face_edges(num_face_edges);
  topology_refiner->getFaceEdges(face_index, face_edges.data());

  const int grid_size_1 = subdiv_ccg.grid_size - 1;
  int adjacent_edge_index = -1;
  if (coord.x == grid_size_1) {
    adjacent_edge_index = face_edges[face_grid_index];
  }
  else {
    BLI_assert(coord.y == grid_size_1);
    adjacent_edge_index = face_edges[face_grid_index == 0 ? face.size() - 1 : face_grid_index - 1];
  }

  return adjacent_edge_index;
}

static int adjacent_edge_point_index_from_coord(const SubdivCCG &subdiv_ccg,
                                                const SubdivCCGCoord &coord,
                                                const int adjacent_edge_index)
{
  Subdiv *subdiv = subdiv_ccg.subdiv;
  const OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;

  const int adjacent_vertex_index = adjacent_vertex_index_from_coord(subdiv_ccg, coord);
  int edge_vertices_indices[2];
  topology_refiner->getEdgeVertices(adjacent_edge_index, edge_vertices_indices);

  /* Vertex index of an edge which is used to see whether edge points in the right direction.
   * Tricky part here is that depending whether input coordinate is are maximum X or Y coordinate
   * of the grid we need to use different edge direction.
   * Basically, the edge adjacent to a previous loop needs to point opposite direction. */
  int directional_edge_vertex_index = -1;

  const int grid_size_1 = subdiv_ccg.grid_size - 1;
  int adjacent_edge_point_index = -1;
  if (coord.x == grid_size_1) {
    adjacent_edge_point_index = subdiv_ccg.grid_size - coord.y - 1;
    directional_edge_vertex_index = edge_vertices_indices[0];
  }
  else {
    BLI_assert(coord.y == grid_size_1);
    adjacent_edge_point_index = subdiv_ccg.grid_size + coord.x;
    directional_edge_vertex_index = edge_vertices_indices[1];
  }

  /* Flip the index if the edge points opposite direction. */
  if (adjacent_vertex_index != directional_edge_vertex_index) {
    const int num_edge_points = subdiv_ccg.grid_size * 2;
    adjacent_edge_point_index = num_edge_points - adjacent_edge_point_index - 1;
  }

  return adjacent_edge_point_index;
}

/* Adjacent edge has two points in the middle which corresponds to grid  corners, but which are
 * the same point in the final geometry.
 * So need to use extra step when calculating next/previous points, so we don't go from a corner
 * of one grid to a corner of adjacent grid. */
static int next_adjacent_edge_point_index(const SubdivCCG &subdiv_ccg, const int point_index)
{
  if (point_index == subdiv_ccg.grid_size - 1) {
    return point_index + 2;
  }
  return point_index + 1;
}
static int prev_adjacent_edge_point_index(const SubdivCCG &subdiv_ccg, const int point_index)
{
  if (point_index == subdiv_ccg.grid_size) {
    return point_index - 2;
  }
  return point_index - 1;
}

/* When the point index corresponds to a grid corner, returns the point index which corresponds to
 * the corner of the adjacent grid, as the adjacent edge has two separate points for each grid
 * corner at the middle of the edge. */
static int adjacent_grid_corner_point_index_on_edge(const SubdivCCG &subdiv_ccg,
                                                    const int point_index)
{
  if (point_index == subdiv_ccg.grid_size) {
    return point_index - 1;
  }
  return point_index + 1;
}

/* Common implementation of neighbor calculation when input coordinate is at the edge between two
 * coarse faces, but is not at the coarse vertex. */
static void neighbor_coords_edge_get(const SubdivCCG &subdiv_ccg,
                                     const SubdivCCGCoord &coord,
                                     const bool include_duplicates,
                                     SubdivCCGNeighbors &r_neighbors)

{
  const bool is_corner = is_corner_grid_coord(subdiv_ccg, coord);
  const int adjacent_edge_index = adjacent_edge_index_from_coord(subdiv_ccg, coord);
  const SubdivCCGAdjacentEdge *adjacent_edge = &subdiv_ccg.adjacent_edges[adjacent_edge_index];

  /* 2 neighbor points along the edge, plus one inner point per every adjacent grid. */
  const int num_adjacent_faces = adjacent_edge->num_adjacent_faces;
  int num_duplicates = 0;
  if (include_duplicates) {
    num_duplicates += num_adjacent_faces - 1;
    if (is_corner) {
      /* When the coord is a grid corner, add an extra duplicate per adjacent grid in all adjacent
       * faces to the edge. */
      num_duplicates += num_adjacent_faces;
    }
  }
  subdiv_ccg_neighbors_init(r_neighbors, num_adjacent_faces + 2, num_duplicates);

  const int point_index = adjacent_edge_point_index_from_coord(
      subdiv_ccg, coord, adjacent_edge_index);
  const int point_index_duplicate = adjacent_grid_corner_point_index_on_edge(subdiv_ccg,
                                                                             point_index);

  const int next_point_index = next_adjacent_edge_point_index(subdiv_ccg, point_index);
  const int prev_point_index = prev_adjacent_edge_point_index(subdiv_ccg, point_index);

  int duplicate_i = num_adjacent_faces;
  for (int i = 0; i < num_adjacent_faces; ++i) {
    const SubdivCCGCoord *boundary_coords = adjacent_edge->boundary_coords[i];
    /* One step into the grid from the edge for each adjacent face. */
    SubdivCCGCoord grid_coord = boundary_coords[point_index];
    r_neighbors.coords[i + 2] = coord_step_inside_from_boundary(subdiv_ccg, grid_coord);

    if (grid_coord.grid_index == coord.grid_index) {
      /* Previous and next along the edge for the current grid. */
      r_neighbors.coords[0] = boundary_coords[prev_point_index];
      r_neighbors.coords[1] = boundary_coords[next_point_index];
    }
    else if (include_duplicates) {
      /* Same coordinate on neighboring grids if requested. */
      r_neighbors.coords[duplicate_i + 2] = grid_coord;
      duplicate_i++;
    }

    /* When it is a corner, add the duplicate of the adjacent grid in the same face. */
    if (include_duplicates && is_corner) {
      SubdivCCGCoord duplicate_corner_grid_coord = boundary_coords[point_index_duplicate];
      r_neighbors.coords[duplicate_i + 2] = duplicate_corner_grid_coord;
      duplicate_i++;
    }
  }
  BLI_assert(duplicate_i - num_adjacent_faces == num_duplicates);
}

/* The corner is at the middle of edge between faces. */
static void neighbor_coords_corner_edge_get(const SubdivCCG &subdiv_ccg,
                                            const SubdivCCGCoord &coord,
                                            const bool include_duplicates,
                                            SubdivCCGNeighbors &r_neighbors)
{
  neighbor_coords_edge_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
}

/* Input coordinate is at one of 4 corners of its grid corners. */
static void neighbor_coords_corner_get(const SubdivCCG &subdiv_ccg,
                                       const SubdivCCGCoord &coord,
                                       const bool include_duplicates,
                                       SubdivCCGNeighbors &r_neighbors)
{
  if (coord.x == 0 && coord.y == 0) {
    neighbor_coords_corner_center_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
  else {
    const int grid_size_1 = subdiv_ccg.grid_size - 1;
    if (coord.x == grid_size_1 && coord.y == grid_size_1) {
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
static void neighbor_coords_boundary_inner_get(const SubdivCCG &subdiv_ccg,
                                               const SubdivCCGCoord &coord,
                                               const bool include_duplicates,
                                               SubdivCCGNeighbors &r_neighbors)
{
  subdiv_ccg_neighbors_init(r_neighbors, 4, (include_duplicates) ? 1 : 0);

  if (coord.x == 0) {
    r_neighbors.coords[0] = coord_at_prev_row(subdiv_ccg, coord);
    r_neighbors.coords[1] = coord_at_next_row(subdiv_ccg, coord);
    r_neighbors.coords[2] = coord_at_next_col(subdiv_ccg, coord);

    r_neighbors.coords[3].grid_index = prev_grid_index_from_coord(subdiv_ccg, coord);
    r_neighbors.coords[3].x = coord.y;
    r_neighbors.coords[3].y = 1;

    if (include_duplicates) {
      r_neighbors.coords[4] = r_neighbors.coords[3];
      r_neighbors.coords[4].y = 0;
    }
  }
  else if (coord.y == 0) {
    r_neighbors.coords[0] = coord_at_prev_col(subdiv_ccg, coord);
    r_neighbors.coords[1] = coord_at_next_col(subdiv_ccg, coord);
    r_neighbors.coords[2] = coord_at_next_row(subdiv_ccg, coord);

    r_neighbors.coords[3].grid_index = next_grid_index_from_coord(subdiv_ccg, coord);
    r_neighbors.coords[3].x = 1;
    r_neighbors.coords[3].y = coord.x;

    if (include_duplicates) {
      r_neighbors.coords[4] = r_neighbors.coords[3];
      r_neighbors.coords[4].x = 0;
    }
  }
}

/* Input coordinate is on an edge between two faces. Need to check adjacency. */
static void neighbor_coords_boundary_outer_get(const SubdivCCG &subdiv_ccg,
                                               const SubdivCCGCoord &coord,
                                               const bool include_duplicates,
                                               SubdivCCGNeighbors &r_neighbors)
{
  neighbor_coords_edge_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
}

/* Input coordinate is at one of 4 boundaries of its grid.
 * It could either be an inner boundary (which connects face center to the face edge) or could be
 * a part of coarse face edge. */
static void neighbor_coords_boundary_get(const SubdivCCG &subdiv_ccg,
                                         const SubdivCCGCoord &coord,
                                         const bool include_duplicates,
                                         SubdivCCGNeighbors &r_neighbors)
{
  if (is_inner_edge_grid_coordinate(subdiv_ccg, coord)) {
    neighbor_coords_boundary_inner_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
  else {
    neighbor_coords_boundary_outer_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
}

/* Input coordinate is inside of its grid, all the neighbors belong to the same grid. */
static void neighbor_coords_inner_get(const SubdivCCG &subdiv_ccg,
                                      const SubdivCCGCoord &coord,
                                      SubdivCCGNeighbors &r_neighbors)
{
  subdiv_ccg_neighbors_init(r_neighbors, 4, 0);

  r_neighbors.coords[0] = coord_at_prev_row(subdiv_ccg, coord);
  r_neighbors.coords[1] = coord_at_next_row(subdiv_ccg, coord);
  r_neighbors.coords[2] = coord_at_prev_col(subdiv_ccg, coord);
  r_neighbors.coords[3] = coord_at_next_col(subdiv_ccg, coord);
}

#endif

void BKE_subdiv_ccg_neighbor_coords_get(const SubdivCCG &subdiv_ccg,
                                        const SubdivCCGCoord &coord,
                                        const bool include_duplicates,
                                        SubdivCCGNeighbors &r_neighbors)
{
#ifdef WITH_OPENSUBDIV
  BLI_assert(coord.grid_index >= 0);
  BLI_assert(coord.grid_index < subdiv_ccg.grids.size());
  BLI_assert(coord.x >= 0);
  BLI_assert(coord.x < subdiv_ccg.grid_size);
  BLI_assert(coord.y >= 0);
  BLI_assert(coord.y < subdiv_ccg.grid_size);

  if (is_corner_grid_coord(subdiv_ccg, coord)) {
    neighbor_coords_corner_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
  else if (is_boundary_grid_coord(subdiv_ccg, coord)) {
    neighbor_coords_boundary_get(subdiv_ccg, coord, include_duplicates, r_neighbors);
  }
  else {
    neighbor_coords_inner_get(subdiv_ccg, coord, r_neighbors);
  }

#  ifndef NDEBUG
  for (const int i : r_neighbors.coords.index_range()) {
    BLI_assert(BKE_subdiv_ccg_check_coord_valid(subdiv_ccg, r_neighbors.coords[i]));
  }
#  endif
#else
  UNUSED_VARS(subdiv_ccg, coord, include_duplicates, r_neighbors);
#endif
}

const int *BKE_subdiv_ccg_start_face_grid_index_ensure(SubdivCCG &subdiv_ccg)
{
#ifdef WITH_OPENSUBDIV
  if (subdiv_ccg.cache_.start_face_grid_index.is_empty()) {
    const Subdiv *subdiv = subdiv_ccg.subdiv;
    OpenSubdiv_TopologyRefiner *topology_refiner = subdiv->topology_refiner;
    if (topology_refiner == nullptr) {
      return nullptr;
    }

    const int num_coarse_faces = topology_refiner->getNumFaces();

    subdiv_ccg.cache_.start_face_grid_index.reinitialize(num_coarse_faces);

    int start_grid_index = 0;
    for (int face_index = 0; face_index < num_coarse_faces; face_index++) {
      const int num_face_grids = topology_refiner->getNumFaceVertices(face_index);
      subdiv_ccg.cache_.start_face_grid_index[face_index] = start_grid_index;
      start_grid_index += num_face_grids;
    }
  }
#endif

  return subdiv_ccg.cache_.start_face_grid_index.data();
}

const int *BKE_subdiv_ccg_start_face_grid_index_get(const SubdivCCG &subdiv_ccg)
{
  return subdiv_ccg.cache_.start_face_grid_index.data();
}

static void adjacent_vertices_index_from_adjacent_edge(const SubdivCCG &subdiv_ccg,
                                                       const SubdivCCGCoord &coord,
                                                       const blender::Span<int> corner_verts,
                                                       const blender::OffsetIndices<int> faces,
                                                       int &r_v1,
                                                       int &r_v2)
{
  const int grid_size_1 = subdiv_ccg.grid_size - 1;
  const int face_index = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, coord.grid_index);
  const blender::IndexRange face = faces[face_index];
  r_v1 = corner_verts[coord.grid_index];

  const int corner = blender::bke::mesh::face_find_corner_from_vert(face, corner_verts, r_v1);
  if (coord.x == grid_size_1) {
    const int next = blender::bke::mesh::face_corner_next(face, corner);
    r_v2 = corner_verts[next];
  }
  if (coord.y == grid_size_1) {
    const int prev = blender::bke::mesh::face_corner_prev(face, corner);
    r_v2 = corner_verts[prev];
  }
}

SubdivCCGAdjacencyType BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
    const SubdivCCG &subdiv_ccg,
    const SubdivCCGCoord &coord,
    const blender::Span<int> corner_verts,
    const blender::OffsetIndices<int> faces,
    int &r_v1,
    int &r_v2)
{

  const int grid_size_1 = subdiv_ccg.grid_size - 1;
  if (is_corner_grid_coord(subdiv_ccg, coord)) {
    if (coord.x == 0 && coord.y == 0) {
      /* Grid corner in the center of a face. */
      return SUBDIV_CCG_ADJACENT_NONE;
    }
    if (coord.x == grid_size_1 && coord.y == grid_size_1) {
      /* Grid corner adjacent to a coarse mesh vertex. */
      r_v1 = r_v2 = corner_verts[coord.grid_index];
      return SUBDIV_CCG_ADJACENT_VERTEX;
    }
    /* Grid corner adjacent to the middle of a coarse mesh edge. */
    adjacent_vertices_index_from_adjacent_edge(subdiv_ccg, coord, corner_verts, faces, r_v1, r_v2);
    return SUBDIV_CCG_ADJACENT_EDGE;
  }

  if (is_boundary_grid_coord(subdiv_ccg, coord)) {
    if (!is_inner_edge_grid_coordinate(subdiv_ccg, coord)) {
      /* Grid boundary adjacent to a coarse mesh edge. */
      adjacent_vertices_index_from_adjacent_edge(
          subdiv_ccg, coord, corner_verts, faces, r_v1, r_v2);
      return SUBDIV_CCG_ADJACENT_EDGE;
    }
  }
  return SUBDIV_CCG_ADJACENT_NONE;
}

blender::BitGroupVector<> &BKE_subdiv_ccg_grid_hidden_ensure(SubdivCCG &subdiv_ccg)
{
  if (subdiv_ccg.grid_hidden.is_empty()) {
    const int grid_area = subdiv_ccg.grid_size * subdiv_ccg.grid_size;
    subdiv_ccg.grid_hidden = blender::BitGroupVector<>(subdiv_ccg.grids.size(), grid_area, false);
  }
  return subdiv_ccg.grid_hidden;
}

void BKE_subdiv_ccg_grid_hidden_free(SubdivCCG &subdiv_ccg)
{
  subdiv_ccg.grid_hidden = {};
}

static void subdiv_ccg_coord_to_ptex_coord(const SubdivCCG &subdiv_ccg,
                                           const SubdivCCGCoord &coord,
                                           int *r_ptex_face_index,
                                           float *r_u,
                                           float *r_v)
{
  Subdiv *subdiv = subdiv_ccg.subdiv;

  const float grid_size = subdiv_ccg.grid_size;
  const float grid_size_1_inv = 1.0f / (grid_size - 1);

  const float grid_u = coord.x * grid_size_1_inv;
  const float grid_v = coord.y * grid_size_1_inv;

  const int face_index = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, coord.grid_index);
  const OffsetIndices<int> faces = subdiv_ccg.faces;
  const IndexRange face = faces[face_index];
  const int *face_ptex_offset = face_ptex_offset_get(subdiv);
  *r_ptex_face_index = face_ptex_offset[face_index];

  const float corner = coord.grid_index - face.start();

  if (face.size() == 4) {
    rotate_grid_to_quad(corner, grid_u, grid_v, r_u, r_v);
  }
  else {
    *r_ptex_face_index += corner;
    *r_u = 1.0f - grid_v;
    *r_v = 1.0f - grid_u;
  }
}

void BKE_subdiv_ccg_eval_limit_point(const SubdivCCG &subdiv_ccg,
                                     const SubdivCCGCoord &coord,
                                     float r_point[3])
{
  Subdiv *subdiv = subdiv_ccg.subdiv;
  int ptex_face_index;
  float u, v;
  subdiv_ccg_coord_to_ptex_coord(subdiv_ccg, coord, &ptex_face_index, &u, &v);
  eval_limit_point(subdiv, ptex_face_index, u, v, r_point);
}

/** \} */
