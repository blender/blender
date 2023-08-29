/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_bitmap.h"
#include "BLI_offset_indices.hh"
#include "BLI_sys_types.h"

#include "BKE_DerivedMesh.h"

struct CCGElem;
struct CCGFace;
struct CCGKey;
struct DMFlagMat;
struct Mesh;
struct Subdiv;

/* --------------------------------------------------------------------
 * Masks.
 */

/* Functor which evaluates mask value at a given (u, v) of given ptex face. */
struct SubdivCCGMaskEvaluator {
  float (*eval_mask)(SubdivCCGMaskEvaluator *mask_evaluator,
                     int ptex_face_index,
                     float u,
                     float v);

  /* Free the data, not the evaluator itself. */
  void (*free)(SubdivCCGMaskEvaluator *mask_evaluator);

  void *user_data;
};

/* Return true if mesh has mask and evaluator can be used. */
bool BKE_subdiv_ccg_mask_init_from_paint(SubdivCCGMaskEvaluator *mask_evaluator, const Mesh *mesh);

/* --------------------------------------------------------------------
 * Materials.
 */

/* Functor which evaluates material and flags of a given coarse face. */
struct SubdivCCGMaterialFlagsEvaluator {
  DMFlagMat (*eval_material_flags)(SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator,
                                   int coarse_face_index);

  /* Free the data, not the evaluator itself. */
  void (*free)(SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator);

  void *user_data;
};

void BKE_subdiv_ccg_material_flags_init_from_mesh(
    SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator, const Mesh *mesh);

/* --------------------------------------------------------------------
 * SubdivCCG.
 */

struct SubdivToCCGSettings {
  /* Resolution at which regular ptex (created for quad face) are being
   * evaluated. This defines how many vertices final mesh will have: every
   * regular ptex has resolution^2 vertices. Special (irregular, or ptex
   * created for a corner of non-quad face) will have resolution of
   * `resolution - 1`. */
  int resolution;
  /* Denotes which extra layers to be added to CCG elements. */
  bool need_normal;
  bool need_mask;
};

struct SubdivCCGCoord {
  /* Index of the grid within SubdivCCG::grids array. */
  int grid_index;

  /* Coordinate within the grid. */
  short x, y;
};

/* This is actually a coarse face, which consists of multiple CCG grids. */
struct SubdivCCGFace {
  /* Total number of grids in this face.
   *
   * This 1:1 corresponds to a number of corners (or loops) from a coarse
   * face. */
  int num_grids;
  /* Index of first grid from this face in SubdivCCG->grids array. */
  int start_grid_index;
};

/* Definition of an edge which is adjacent to at least one of the faces. */
struct SubdivCCGAdjacentEdge {
  int num_adjacent_faces;
  /* Indexed by adjacent face index, then by point index on the edge.
   * points to a coordinate into the grids. */
  SubdivCCGCoord **boundary_coords;
};

/* Definition of a vertex which is adjacent to at least one of the faces. */
struct SubdivCCGAdjacentVertex {
  int num_adjacent_faces;
  /* Indexed by adjacent face index, points to a coordinate in the grids. */
  SubdivCCGCoord *corner_coords;
};

/* Representation of subdivision surface which uses CCG grids. */
struct SubdivCCG {
  /* This is a subdivision surface this CCG was created for.
   *
   * TODO(sergey): Make sure the whole descriptor is valid, including all the
   * displacement attached to the surface. */
  Subdiv *subdiv;
  /* A level at which geometry was subdivided. This is what defines grid
   * resolution. It is NOT the topology refinement level. */
  int level;
  /* Resolution of grid. All grids have matching resolution, and resolution
   * is same as ptex created for non-quad faces. */
  int grid_size;
  /* Size of a single element of a grid (including coordinate and all the other layers).
   * Measured in bytes. */
  int grid_element_size;
  /* Grids represent limit surface, with displacement applied. Grids are
   * corresponding to face-corners of coarse mesh, each grid has
   * grid_size^2 elements.
   */
  /* Indexed by a grid index, points to a grid data which is stored in
   * grids_storage. */
  CCGElem **grids;
  /* Flat array of all grids' data. */
  unsigned char *grids_storage;
  int num_grids;
  /* Loose edges, each array element contains grid_size elements
   * corresponding to vertices created by subdividing coarse edges. */
  CCGElem **edges;
  int num_edges;
  /* Loose vertices. Every element corresponds to a loose vertex from a coarse
   * mesh, every coarse loose vertex corresponds to a single subdivided
   * element. */
  CCGElem *vertices;
  int num_vertices;
  /* Denotes which layers present in the elements.
   *
   * Grids always has coordinates, followed by extra layers which are set to
   * truth here.
   */
  bool has_normal;
  bool has_mask;
  /* Offsets of corresponding data layers in the elements. */
  int normal_offset;
  int mask_offset;

  /* Faces from which grids are emitted. */
  int num_faces;
  SubdivCCGFace *faces;
  /* Indexed by grid index, points to corresponding face from `faces`. */
  SubdivCCGFace **grid_faces;

  /* Edges which are adjacent to faces.
   * Used for faster grid stitching, in the cost of extra memory.
   */
  int num_adjacent_edges;
  SubdivCCGAdjacentEdge *adjacent_edges;

  /* Vertices which are adjacent to faces
   * Used for faster grid stitching, in the cost of extra memory.
   */
  int num_adjacent_vertices;
  SubdivCCGAdjacentVertex *adjacent_vertices;

  DMFlagMat *grid_flag_mats;
  BLI_bitmap **grid_hidden;

  /* TODO(sergey): Consider adding some accessors to a "decoded" geometry,
   * to make integration with draw manager and such easy.
   */

  /* TODO(sergey): Consider adding CD layers here, so we can draw final mesh
   * from grids, and have UVs and such work.
   */

  /* Integration with sculpting. */
  /* TODO(sergey): Is this really best way to go? Kind of annoying to have
   * such use-related flags in a more or less generic structure. */
  struct {
    /* Corresponds to MULTIRES_COORDS_MODIFIED. */
    bool coords;
    /* Corresponds to MULTIRES_HIDDEN_MODIFIED. */
    bool hidden;
  } dirty;

  /* Cached values, are not supposed to be accessed directly. */
  struct {
    /* Indexed by face, indicates index of the first grid which corresponds to the face. */
    int *start_face_grid_index;
  } cache_;
};

/* Create CCG representation of subdivision surface.
 *
 * NOTE: CCG stores dense vertices in a grid-like storage. There is no edges or
 * faces information's for the high-poly surface.
 *
 * NOTE: Subdiv is expected to be refined and ready for evaluation.
 * NOTE: CCG becomes an owner of subdiv.
 *
 * TODO(sergey): Allow some user-counter or more explicit control over who owns
 * the Subdiv. The goal should be to allow viewport GL Mesh and CCG to share
 * same Subsurf without conflicts. */
SubdivCCG *BKE_subdiv_to_ccg(Subdiv *subdiv,
                             const SubdivToCCGSettings *settings,
                             SubdivCCGMaskEvaluator *mask_evaluator,
                             SubdivCCGMaterialFlagsEvaluator *material_flags_evaluator);

/* Destroy CCG representation of subdivision surface. */
void BKE_subdiv_ccg_destroy(SubdivCCG *subdiv_ccg);

/* Helper function, creates Mesh structure which is properly setup to use
 * grids.
 */
Mesh *BKE_subdiv_to_ccg_mesh(Subdiv *subdiv,
                             const SubdivToCCGSettings *settings,
                             const Mesh *coarse_mesh);

/* Create a key for accessing grid elements at a given level. */
void BKE_subdiv_ccg_key(CCGKey *key, const SubdivCCG *subdiv_ccg, int level);
void BKE_subdiv_ccg_key_top_level(CCGKey *key, const SubdivCCG *subdiv_ccg);

/* Recalculate all normals based on grid element coordinates. */
void BKE_subdiv_ccg_recalc_normals(SubdivCCG *subdiv_ccg);

/* Update normals of affected faces. */
void BKE_subdiv_ccg_update_normals(SubdivCCG *subdiv_ccg,
                                   CCGFace **effected_faces,
                                   int num_effected_faces);

/* Average grid coordinates and normals along the grid boundaries. */
void BKE_subdiv_ccg_average_grids(SubdivCCG *subdiv_ccg);

/* Similar to above, but only updates given faces. */
void BKE_subdiv_ccg_average_stitch_faces(SubdivCCG *subdiv_ccg,
                                         CCGFace **effected_faces,
                                         int num_effected_faces);

/* Get geometry counters at the current subdivision level. */
void BKE_subdiv_ccg_topology_counters(const SubdivCCG *subdiv_ccg,
                                      int *r_num_vertices,
                                      int *r_num_edges,
                                      int *r_num_faces,
                                      int *r_num_loops);

struct SubdivCCGNeighbors {
  SubdivCCGCoord *coords;
  int size;
  int num_duplicates;

  SubdivCCGCoord coords_fixed[256];
};

void BKE_subdiv_ccg_print_coord(const char *message, const SubdivCCGCoord *coord);
bool BKE_subdiv_ccg_check_coord_valid(const SubdivCCG *subdiv_ccg, const SubdivCCGCoord *coord);

/* CCG element neighbors.
 *
 * Neighbors are considered:
 *
 * - For an inner elements of a grid other elements which are sharing same row or column (4
 *   neighbor elements in total).
 *
 * - For the corner element a single neighboring element on every adjacent edge, single from
 *   every grid.
 *
 * - For the boundary element two neighbor elements on the boundary (from same grid) and one
 *   element inside of every neighboring grid. */

/* Get actual neighbors of the given coordinate.
 *
 * SubdivCCGNeighbors.neighbors must be freed if it is not equal to
 * SubdivCCGNeighbors.fixed_neighbors.
 *
 * If include_duplicates is true, vertices in other grids that match
 * the current vertex are added at the end of the coords array. */
void BKE_subdiv_ccg_neighbor_coords_get(const SubdivCCG *subdiv_ccg,
                                        const SubdivCCGCoord *coord,
                                        bool include_duplicates,
                                        SubdivCCGNeighbors *r_neighbors);

int BKE_subdiv_ccg_grid_to_face_index(const SubdivCCG *subdiv_ccg, int grid_index);
void BKE_subdiv_ccg_eval_limit_point(const SubdivCCG *subdiv_ccg,
                                     const SubdivCCGCoord *coord,
                                     float r_point[3]);

enum SubdivCCGAdjacencyType {
  SUBDIV_CCG_ADJACENT_NONE,
  SUBDIV_CCG_ADJACENT_VERTEX,
  SUBDIV_CCG_ADJACENT_EDGE,
};

/* Returns if a grid coordinates is adjacent to a coarse mesh edge, vertex or nothing. If it is
 * adjacent to an edge, r_v1 and r_v2 will be set to the two vertices of that edge. If it is
 * adjacent to a vertex, r_v1 and r_v2 will be the index of that vertex. */
SubdivCCGAdjacencyType BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
    const SubdivCCG *subdiv_ccg,
    const SubdivCCGCoord *coord,
    blender::Span<int> corner_verts,
    blender::OffsetIndices<int> faces,
    int *r_v1,
    int *r_v2);

/* Get array which is indexed by face index and contains index of a first grid of the face.
 *
 * The "ensure" version allocates the mapping if it's not known yet and stores it in the subdiv_ccg
 * descriptor. This function is NOT safe for threading.
 *
 * The "get" version simply returns cached array. */
const int *BKE_subdiv_ccg_start_face_grid_index_ensure(SubdivCCG *subdiv_ccg);
const int *BKE_subdiv_ccg_start_face_grid_index_get(const SubdivCCG *subdiv_ccg);

void BKE_subdiv_ccg_grid_hidden_ensure(SubdivCCG *subdiv_ccg, int grid_index);
void BKE_subdiv_ccg_grid_hidden_free(SubdivCCG *subdiv_ccg, int grid_index);
