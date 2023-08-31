/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_meshdata_types.h"

/** \file
 * \ingroup bke
 */

struct PBVHGPUFormat;
struct MLoopTri;

/* Axis-aligned bounding box */
struct BB {
  float bmin[3], bmax[3];
};

/* Axis-aligned bounding box with centroid */
struct BBC {
  float bmin[3], bmax[3], bcentroid[3];
};

/* NOTE: this structure is getting large, might want to split it into
 * union'd structs */
struct PBVHNode {
  /* Opaque handle for drawing code */
  PBVHBatches *draw_batches = nullptr;

  /* Voxel bounds */
  BB vb = {};
  BB orig_vb = {};

  /* For internal nodes, the offset of the children in the PBVH
   * 'nodes' array. */
  int children_offset = 0;

  /* List of primitives for this node. Semantics depends on
   * PBVH type:
   *
   * - PBVH_FACES: Indices into the PBVH.looptri array.
   * - PBVH_GRIDS: Multires grid indices.
   * - PBVH_BMESH: Unused.  See PBVHNode.bm_faces.
   *
   * NOTE: This is a pointer inside of PBVH.prim_indices; it
   * is not allocated separately per node.
   */
  blender::Span<int> prim_indices;

  /* Array of indices into the mesh's vertex array. Contains the
   * indices of all vertices used by faces that are within this
   * node's bounding box.
   *
   * Note that a vertex might be used by a multiple faces, and
   * these faces might be in different leaf nodes. Such a vertex
   * will appear in the vert_indices array of each of those leaf
   * nodes.
   *
   * In order to support cases where you want access to multiple
   * nodes' vertices without duplication, the vert_indices array
   * is ordered such that the first part of the array, up to
   * index 'uniq_verts', contains "unique" vertex indices. These
   * vertices might not be truly unique to this node, but if
   * they appear in another node's vert_indices array, they will
   * be above that node's 'uniq_verts' value.
   *
   * Used for leaf nodes in a mesh-based PBVH (not multires.)
   */
  blender::Array<int, 0> vert_indices;
  int uniq_verts = 0;
  int face_verts = 0;

  /* Array of indices into the Mesh's corner array.
   * PBVH_FACES only.
   */
  blender::Array<int, 0> loop_indices;

  /* An array mapping face corners into the vert_indices
   * array. The array is sized to match 'totprim', and each of
   * the face's corners gets an index into the vert_indices
   * array, in the same order as the corners in the original
   * MLoopTri.
   *
   * Used for leaf nodes in a mesh-based PBVH (not multires.)
   */
  blender::Array<blender::int3, 0> face_vert_indices;

  /* Indicates whether this node is a leaf or not; also used for
   * marking various updates that need to be applied. */
  PBVHNodeFlags flag = PBVHNodeFlags(0);

  /* Used for ray-casting: how close the bounding-box is to the ray point. */
  float tmin = 0.0f;

  /* Scalar displacements for sculpt mode's layer brush. */
  float *layer_disp = nullptr;

  int proxy_count = 0;
  PBVHProxyNode *proxies = nullptr;

  /* Dyntopo */

  /* GSet of pointers to the BMFaces used by this node.
   * NOTE: PBVH_BMESH only. Faces are always triangles
   * (dynamic topology forcibly triangulates the mesh).
   */
  GSet *bm_faces = nullptr;
  GSet *bm_unique_verts = nullptr;
  GSet *bm_other_verts = nullptr;

  /* Deprecated. Stores original coordinates of triangles. */
  float (*bm_orco)[3] = nullptr;
  int (*bm_ortri)[3] = nullptr;
  BMVert **bm_orvert = nullptr;
  int bm_tot_ortri = 0;

  /* Used to store the brush color during a stroke and composite it over the original color */
  PBVHColorBufferNode color_buffer;
  PBVHPixelsNode pixels;

  /* Used to flash colors of updated node bounding boxes in
   * debug draw mode (when G.debug_value / bpy.app.debug_value is 889).
   */
  int debug_draw_gen = 0;
};

typedef struct PBVHBMeshLog PBVHBMeshLog;

struct PBVH {
  PBVHPublic header;

  blender::Vector<PBVHNode> nodes;

  /* Memory backing for PBVHNode.prim_indices. */
  blender::Array<int> prim_indices;
  int totprim;
  int totvert;
  int faces_num; /* Do not use directly, use BKE_pbvh_num_faces. */

  int leaf_limit;
  int pixel_leaf_limit;
  int depth_limit;

  /* Mesh data */
  Mesh *mesh;

  blender::Span<blender::float3> vert_normals;
  blender::Span<blender::float3> face_normals;
  bool *hide_vert;
  blender::MutableSpan<blender::float3> vert_positions;
  /** Local vertex positions owned by the PVBH when not sculpting base mesh positions directly. */
  blender::Array<blender::float3> vert_positions_deformed;
  blender::OffsetIndices<int> faces;
  bool *hide_poly;
  /** Only valid for polygon meshes. */
  blender::Span<int> corner_verts;
  /* Owned by the #PBVH, because after deformations they have to be recomputed. */
  blender::Array<MLoopTri> looptri;
  blender::Span<int> looptri_faces;
  CustomData *vert_data;
  CustomData *loop_data;
  CustomData *face_data;

  int face_sets_color_seed;
  int face_sets_color_default;
  int *face_sets;

  /* Grid Data */
  CCGKey gridkey;
  CCGElem **grids;
  blender::Span<int> grid_to_face_map;
  const DMFlagMat *grid_flag_mats;
  int totgrid;
  BLI_bitmap **grid_hidden;

  /* Used during BVH build and later to mark that a vertex needs to update
   * (its normal must be recalculated). */
  blender::Array<bool> vert_bitmap;

#ifdef PERFCNTRS
  int perf_modified;
#endif

  /* flag are verts/faces deformed */
  bool deformed;

  /* Dynamic topology */
  float bm_max_edge_len;
  float bm_min_edge_len;
  int cd_vert_node_offset;
  int cd_face_node_offset;

  float planes[6][4];
  int num_planes;

  BMLog *bm_log;
  SubdivCCG *subdiv_ccg;

  blender::GroupedSpan<int> pmap;

  CustomDataLayer *color_layer;
  eAttrDomain color_domain;

  bool is_drawing;

  /* Used by DynTopo to invalidate the draw cache. */
  bool draw_cache_invalid;

  PBVHGPUFormat *vbo_id;

  PBVHPixels pixels;
};

/* pbvh.cc */

void BB_reset(BB *bb);
/**
 * Expand the bounding box to include a new coordinate.
 */
void BB_expand(BB *bb, const float co[3]);
/**
 * Expand the bounding box to include another bounding box.
 */
void BB_expand_with_bb(BB *bb, const BB *bb2);
void BBC_update_centroid(BBC *bbc);
/**
 * Return 0, 1, or 2 to indicate the widest axis of the bounding box.
 */
int BB_widest_axis(const BB *bb);
void pbvh_grow_nodes(PBVH *bvh, int totnode);
bool ray_face_intersection_quad(const float ray_start[3],
                                IsectRayPrecalc *isect_precalc,
                                const float t0[3],
                                const float t1[3],
                                const float t2[3],
                                const float t3[3],
                                float *depth);
bool ray_face_intersection_tri(const float ray_start[3],
                               IsectRayPrecalc *isect_precalc,
                               const float t0[3],
                               const float t1[3],
                               const float t2[3],
                               float *depth);

bool ray_face_nearest_quad(const float ray_start[3],
                           const float ray_normal[3],
                           const float t0[3],
                           const float t1[3],
                           const float t2[3],
                           const float t3[3],
                           float *r_depth,
                           float *r_dist_sq);
bool ray_face_nearest_tri(const float ray_start[3],
                          const float ray_normal[3],
                          const float t0[3],
                          const float t1[3],
                          const float t2[3],
                          float *r_depth,
                          float *r_dist_sq);

/* pbvh_bmesh.cc */

bool pbvh_bmesh_node_raycast(PBVHNode *node,
                             const float ray_start[3],
                             const float ray_normal[3],
                             IsectRayPrecalc *isect_precalc,
                             float *dist,
                             bool use_original,
                             PBVHVertRef *r_active_vertex,
                             float *r_face_normal);
bool pbvh_bmesh_node_nearest_to_ray(PBVHNode *node,
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    float *depth,
                                    float *dist_sq,
                                    bool use_original);

void pbvh_bmesh_normals_update(blender::Span<PBVHNode *> nodes);

/* pbvh_pixels.hh */

void pbvh_node_pixels_free(PBVHNode *node);
void pbvh_pixels_free(PBVH *pbvh);
void pbvh_free_draw_buffers(PBVH *pbvh, PBVHNode *node);
