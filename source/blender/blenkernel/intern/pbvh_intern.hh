/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BKE_paint.hh" /* for SCULPT_BOUNDARY_NEEDS_UPDATE */
#include "BKE_pbvh_api.hh"

#include "../../bmesh/intern/bmesh_idmap.h"
#include "bmesh.h"

#define PBVH_STACK_FIXED_DEPTH 100

#include "DNA_meshdata_types.h"

/** \file
 * \ingroup bke
 */
struct CustomData;
struct PBVHTriBuf;

struct PBVHGPUFormat;
struct MLoopTri;
struct BMIdMap;

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
  int subtree_tottri = 0;

  int depth = 0;

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

  /* GSet of pointers to the BMFaces used by this node.
   * NOTE: PBVH_BMESH only.
   */
  blender::bke::dyntopo::DyntopoSet<BMVert> *bm_unique_verts = nullptr, *bm_other_verts = nullptr;
  blender::bke::dyntopo::DyntopoSet<BMFace> *bm_faces = nullptr;

  PBVHTriBuf *tribuf = nullptr;  // all triangles
  blender::Vector<PBVHTriBuf> *tri_buffers = nullptr;

  int updategen = 0;

#ifdef PROXY_ADVANCED
  ProxyVertArray proxyverts;
#endif
  PBVHPixelsNode pixels;

  /* Used to flash colors of updated node bounding boxes in
   * debug draw mode (when G.debug_value / bpy.app.debug_value is 889).
   */
  int debug_draw_gen = 0;
  int id = 0;
};

typedef enum { PBVH_IGNORE_UVS = 1 } PBVHFlags;
ENUM_OPERATORS(PBVHFlags, PBVH_IGNORE_UVS);

typedef struct PBVHBMeshLog PBVHBMeshLog;
struct DMFlagMat;

struct PBVH {
  PBVHPublic header;
  PBVHFlags flags;
  eAttrCorrectMode distort_correction_mode;

  int idgen;

  bool dyntopo_stop;

  blender::Vector<PBVHNode> nodes;

  /* Memory backing for PBVHNode.prim_indices. */
  blender::Array<int> prim_indices;
  int totprim;
  int totvert;
  int totloop;
  blender::OffsetIndices<int> faces;
  int faces_num; /* Do not use directly, use BKE_pbvh_num_faces. */

  int leaf_limit;
  int pixel_leaf_limit;
  int depth_limit;

  /* Mesh data */
  Mesh *mesh;

  /* NOTE: Normals are not `const` because they can be updated for drawing by sculpt code. */
  blender::MutableSpan<blender::float3> vert_normals;
  blender::MutableSpan<blender::float3> face_normals;
  bool *hide_vert;
  blender::MutableSpan<blender::float3> vert_positions;
  /** Local vertex positions owned by the PVBH when not sculpting base mesh positions directly. */
  blender::Array<blender::float3> vert_positions_deformed;
  blender::Span<int> looptri_faces;
  blender::Span<blender::int2> edges;
  bool *hide_poly;
  /** Only valid for polygon meshes. */
  blender::Span<int> corner_verts;
  blender::Span<int> corner_edges;
  const bool *sharp_edges;
  const bool *seam_edges;

  CustomData *vert_data;
  CustomData *loop_data;
  CustomData *face_data;

  /* Owned by the #PBVH, because after deformations they have to be recomputed. */
  blender::Array<MLoopTri> looptri;
  blender::Span<blender::float3> origco, origno;

  /* Sculpt flags*/
  uint8_t *sculpt_flags;

  /* Cached vertex valences */
  int *valence;

  int face_sets_color_seed;
  int face_sets_color_default;
  int *face_sets;
  float *face_areas; /* float2 vector, double buffered to avoid thread contention */
  int face_area_i;

  /* Grid Data */
  CCGKey gridkey;
  CCGElem **grids;
  void **gridfaces;
  const struct DMFlagMat *grid_flag_mats;
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
  float bm_detail_range;
  struct BMIdMap *bm_idmap;

  int cd_flag;
  int cd_valence;
  int cd_origco, cd_origno;
  int cd_vert_node_offset;
  int cd_face_node_offset;
  int cd_vert_mask_offset;
  int cd_faceset_offset;
  int cd_face_area;
  int cd_vcol_offset;

  int totuv;

  float planes[6][4];
  int num_planes;

  int symmetry;

  BMLog *bm_log;
  struct SubdivCCG *subdiv_ccg;

  bool need_full_render; /* Set by pbvh drawing for PBVH_BMESH. */

  int balance_counter;
  int stroke_id;

  bool invalid;
  blender::GroupedSpan<int> pmap;
  blender::GroupedSpan<int> vemap;

  CustomDataLayer *color_layer;
  eAttrDomain color_domain;

  bool is_drawing;

  /* Used by DynTopo to invalidate the draw cache. */
  bool draw_cache_invalid;

  int *boundary_flags, *edge_boundary_flags;
  int cd_boundary_flag, cd_edge_boundary;
  int cd_curvature_dir;

  PBVHGPUFormat *vbo_id;

  PBVHPixels pixels;
  bool show_orig;
  float sharp_angle_limit;

  BLI_bitmap *vert_boundary_map;
};

/* pbvh.cc */

void BB_reset(BB *bb);
void BB_zero(BB *bb);

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
void BB_intersect(BB *r_out, BB *a, BB *b);
float BB_volume(const BB *bb);

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

void pbvh_update_BB_redraw(PBVH *bvh, PBVHNode **nodes, int totnode, int flag);

bool ray_face_intersection_depth_tri(const float ray_start[3],
                                     struct IsectRayPrecalc *isect_precalc,
                                     const float t0[3],
                                     const float t1[3],
                                     const float t2[3],
                                     float *r_depth,
                                     float *r_back_depth,
                                     int *hit_count);
/* pbvh_bmesh.cc */

/* pbvh_bmesh.c */
bool pbvh_bmesh_node_raycast(SculptSession *ss,
                             PBVH *pbvh,
                             PBVHNode *node,
                             const float ray_start[3],
                             const float ray_normal[3],
                             struct IsectRayPrecalc *isect_precalc,
                             int *hit_count,
                             float *depth,
                             float *back_depth,
                             bool use_original,
                             PBVHVertRef *r_active_vertex_index,
                             PBVHFaceRef *r_active_face_index,
                             float *r_face_normal,
                             int stroke_id);

bool pbvh_bmesh_node_nearest_to_ray(SculptSession *ss,
                                    PBVH *pbvh,
                                    PBVHNode *node,
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    float *depth,
                                    float *dist_sq,
                                    bool use_original,
                                    int stroke_id);

void pbvh_update_free_all_draw_buffers(PBVH *pbvh, PBVHNode *node);

BLI_INLINE int pbvh_bmesh_node_index_from_vert(PBVH *pbvh, const BMVert *key)
{
  const int node_index = BM_ELEM_CD_GET_INT((const BMElem *)key, pbvh->cd_vert_node_offset);
  return node_index;
}

BLI_INLINE PBVHNode *pbvh_bmesh_node_from_vert(PBVH *pbvh, const BMVert *key)
{
  int ni = pbvh_bmesh_node_index_from_vert(pbvh, key);

  return ni >= 0 ? &pbvh->nodes[ni] : nullptr;
}

BLI_INLINE PBVHNode *pbvh_bmesh_node_from_face(PBVH *pbvh, const BMFace *key)
{
  int ni = BM_ELEM_CD_GET_INT(key, pbvh->cd_face_node_offset);
  return ni >= 0 && ni < pbvh->nodes.size() ? &pbvh->nodes[ni] : nullptr;
}

bool pbvh_bmesh_node_limit_ensure(PBVH *pbvh, int node_index);

//#define PBVH_BMESH_DEBUG

#ifdef PBVH_BMESH_DEBUG
void pbvh_bmesh_check_nodes(PBVH *pbvh);
void pbvh_bmesh_check_nodes_simple(PBVH *pbvh);
#else
#  define pbvh_bmesh_check_nodes(pbvh)
#  define pbvh_bmesh_check_nodes_simple(pbvh)
#endif

void bke_pbvh_insert_face_finalize(PBVH *pbvh, BMFace *f, const int ni);
void bke_pbvh_insert_face(PBVH *pbvh, struct BMFace *f);

inline bool pbvh_check_vert_boundary_bmesh(PBVH *pbvh, struct BMVert *v)
{
  int flag = BM_ELEM_CD_GET_INT(v, pbvh->cd_boundary_flag);

  if (flag & (SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_UV)) {
    blender::bke::pbvh::update_vert_boundary_bmesh(pbvh->cd_faceset_offset,
                                                   pbvh->cd_vert_node_offset,
                                                   pbvh->cd_face_node_offset,
                                                   pbvh->cd_vcol_offset,
                                                   pbvh->cd_boundary_flag,
                                                   pbvh->cd_flag,
                                                   pbvh->cd_valence,
                                                   v,
                                                   &pbvh->header.bm->ldata,
                                                   pbvh->sharp_angle_limit);
    return true;
  }
  else if (flag & SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE) {
    blender::bke::pbvh::update_sharp_vertex_bmesh(
        v, pbvh->cd_boundary_flag, pbvh->sharp_angle_limit);
  }

  return false;
}

inline bool pbvh_check_edge_boundary_bmesh(PBVH *pbvh, struct BMEdge *e)
{
  int flag = BM_ELEM_CD_GET_INT(e, pbvh->cd_edge_boundary);

  if (flag & (SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_UV |
              SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE))
  {
    blender::bke::pbvh::update_edge_boundary_bmesh(e,
                                                   pbvh->cd_faceset_offset,
                                                   pbvh->cd_edge_boundary,
                                                   pbvh->cd_flag,
                                                   pbvh->cd_valence,
                                                   &pbvh->header.bm->ldata,
                                                   pbvh->sharp_angle_limit);
    return true;
  }

  return false;
}

void pbvh_bmesh_check_other_verts(PBVHNode *node);
void pbvh_bmesh_normals_update(PBVH *pbvh, blender::Span<PBVHNode *> nodes);

/* pbvh_pixels.hh */

void pbvh_node_pixels_free(PBVHNode *node);
void pbvh_pixels_free(PBVH *pbvh);
void pbvh_free_draw_buffers(PBVH *pbvh, PBVHNode *node);

BLI_INLINE bool pbvh_boundary_needs_update_bmesh(PBVH *pbvh, BMVert *v)
{
  int *flags = (int *)BM_ELEM_CD_GET_VOID_P(v, pbvh->cd_boundary_flag);

  return *flags & SCULPT_BOUNDARY_NEEDS_UPDATE;
}

template<typename T = BMVert> inline void pbvh_boundary_update_bmesh(PBVH *pbvh, T *elem)
{
  int *flags;

  if constexpr (std::is_same_v<T, BMVert>) {
    flags = (int *)BM_ELEM_CD_GET_VOID_P(elem, pbvh->cd_boundary_flag);
  }
  else {
    flags = (int *)BM_ELEM_CD_GET_VOID_P(elem, pbvh->cd_edge_boundary);
  }

  *flags |= SCULPT_BOUNDARY_NEEDS_UPDATE;
}
