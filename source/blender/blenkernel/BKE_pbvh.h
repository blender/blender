/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief A BVH for high poly meshes.
 */

#include "BLI_bitmap.h"
#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#ifdef __cplusplus
#  include "BLI_offset_indices.hh"
#  include "BLI_vector.hh"
#endif

#include "bmesh.h"

/* For embedding CCGKey in iterator. */
#include "BKE_attribute.h"
#include "BKE_ccg.h"
#include "DNA_customdata_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BMLog;
struct BMesh;
struct CCGElem;
struct CCGKey;
struct CustomData;
struct DMFlagMat;
struct IsectRayPrecalc;
struct MLoopTri;
struct Mesh;
struct PBVH;
struct PBVHBatches;
struct PBVHNode;
struct PBVH_GPU_Args;
struct SculptSession;
struct SubdivCCG;
struct TaskParallelSettings;
struct Image;
struct ImageUser;

typedef struct PBVH PBVH;
typedef struct PBVHNode PBVHNode;

typedef enum {
  PBVH_FACES,
  PBVH_GRIDS,
  PBVH_BMESH,
} PBVHType;

/* Public members of PBVH, used for inlined functions. */
struct PBVHPublic {
  PBVHType type;
  BMesh *bm;
};

/*
 * These structs represent logical verts/edges/faces.
 * for PBVH_GRIDS and PBVH_FACES they store integer
 * offsets, PBVH_BMESH stores pointers.
 *
 * The idea is to enforce stronger type checking by encapsulating
 * intptr_t's in structs.
 */

/* A generic PBVH vertex.
 *
 * NOTE: in PBVH_GRIDS we consider the final grid points
 * to be vertices.  This is not true of edges or faces which are pulled from
 * the base mesh.
 */

#ifdef __cplusplus
/* A few C++ methods to play nice with sets and maps. */
#  define PBVH_REF_CXX_METHODS(Class) \
    bool operator==(const Class b) const \
    { \
      return i == b.i; \
    } \
    uint64_t hash() const \
    { \
      return i; \
    }
#else
#  define PBVH_REF_CXX_METHODS(cls)
#endif

typedef struct PBVHVertRef {
  intptr_t i;

  PBVH_REF_CXX_METHODS(PBVHVertRef)
} PBVHVertRef;

/* NOTE: edges in PBVH_GRIDS are always pulled from the base mesh. */
typedef struct PBVHEdgeRef {
  intptr_t i;

  PBVH_REF_CXX_METHODS(PBVHVertRef)
} PBVHEdgeRef;

/* NOTE: faces in PBVH_GRIDS are always puled from the base mesh. */
typedef struct PBVHFaceRef {
  intptr_t i;

  PBVH_REF_CXX_METHODS(PBVHVertRef)
} PBVHFaceRef;

#define PBVH_REF_NONE -1LL

typedef struct {
  float (*co)[3];
} PBVHProxyNode;

typedef struct {
  float (*color)[4];
} PBVHColorBufferNode;

typedef struct PBVHPixels {
  /**
   * Storage for texture painting on PBVH level.
   *
   * Contains #blender::bke::pbvh::pixels::PBVHData
   */
  void *data;
} PBVHPixels;

typedef struct PBVHPixelsNode {
  /**
   * Contains triangle/pixel data used during texture painting.
   *
   * Contains #blender::bke::pbvh::pixels::NodeData.
   */
  void *node_data;
} PBVHPixelsNode;

typedef struct PBVHAttrReq {
  char name[MAX_CUSTOMDATA_LAYER_NAME];
  eAttrDomain domain;
  eCustomDataType type;
} PBVHAttrReq;

typedef enum {
  PBVH_Leaf = 1 << 0,

  PBVH_UpdateNormals = 1 << 1,
  PBVH_UpdateBB = 1 << 2,
  PBVH_UpdateOriginalBB = 1 << 3,
  PBVH_UpdateDrawBuffers = 1 << 4,
  PBVH_UpdateRedraw = 1 << 5,
  PBVH_UpdateMask = 1 << 6,
  PBVH_UpdateVisibility = 1 << 8,

  PBVH_RebuildDrawBuffers = 1 << 9,
  PBVH_FullyHidden = 1 << 10,
  PBVH_FullyMasked = 1 << 11,
  PBVH_FullyUnmasked = 1 << 12,

  PBVH_UpdateTopology = 1 << 13,
  PBVH_UpdateColor = 1 << 14,
  PBVH_RebuildPixels = 1 << 15,
  PBVH_TexLeaf = 1 << 16,
  PBVH_TopologyUpdated = 1 << 17, /* Used internally by pbvh_bmesh.c */

} PBVHNodeFlags;
ENUM_OPERATORS(PBVHNodeFlags, PBVH_TopologyUpdated);

typedef struct PBVHFrustumPlanes {
  float (*planes)[4];
  int num_planes;
} PBVHFrustumPlanes;

BLI_INLINE PBVHType BKE_pbvh_type(const PBVH *pbvh)
{
  return ((const struct PBVHPublic *)pbvh)->type;
}

BLI_INLINE BMesh *BKE_pbvh_get_bmesh(PBVH *pbvh)
{
  return ((struct PBVHPublic *)pbvh)->bm;
}

void BKE_pbvh_set_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes);
void BKE_pbvh_get_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes);

BLI_INLINE PBVHVertRef BKE_pbvh_make_vref(intptr_t i)
{
  PBVHVertRef ret = {i};
  return ret;
}

BLI_INLINE PBVHEdgeRef BKE_pbvh_make_eref(intptr_t i)
{
  PBVHEdgeRef ret = {i};
  return ret;
}

BLI_INLINE PBVHFaceRef BKE_pbvh_make_fref(intptr_t i)
{
  PBVHFaceRef ret = {i};
  return ret;
}

BLI_INLINE int BKE_pbvh_vertex_to_index(PBVH *pbvh, PBVHVertRef v)
{
  return (BKE_pbvh_type(pbvh) == PBVH_BMESH && v.i != PBVH_REF_NONE ?
              BM_elem_index_get((BMVert *)(v.i)) :
              (v.i));
}

BLI_INLINE PBVHVertRef BKE_pbvh_index_to_vertex(PBVH *pbvh, int index)
{
  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      return BKE_pbvh_make_vref(index);
    case PBVH_BMESH:
      return BKE_pbvh_make_vref((intptr_t)BKE_pbvh_get_bmesh(pbvh)->vtable[index]);
  }

  return BKE_pbvh_make_vref(PBVH_REF_NONE);
}

BLI_INLINE int BKE_pbvh_edge_to_index(PBVH *pbvh, PBVHEdgeRef e)
{
  return (BKE_pbvh_type(pbvh) == PBVH_BMESH && e.i != PBVH_REF_NONE ?
              BM_elem_index_get((BMEdge *)(e.i)) :
              (e.i));
}

BLI_INLINE PBVHEdgeRef BKE_pbvh_index_to_edge(PBVH *pbvh, int index)
{
  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      return BKE_pbvh_make_eref(index);
    case PBVH_BMESH:
      return BKE_pbvh_make_eref((intptr_t)BKE_pbvh_get_bmesh(pbvh)->etable[index]);
  }

  return BKE_pbvh_make_eref(PBVH_REF_NONE);
}

BLI_INLINE int BKE_pbvh_face_to_index(PBVH *pbvh, PBVHFaceRef f)
{
  return (BKE_pbvh_type(pbvh) == PBVH_BMESH && f.i != PBVH_REF_NONE ?
              BM_elem_index_get((BMFace *)(f.i)) :
              (f.i));
}

BLI_INLINE PBVHFaceRef BKE_pbvh_index_to_face(PBVH *pbvh, int index)
{
  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      return BKE_pbvh_make_fref(index);
    case PBVH_BMESH:
      return BKE_pbvh_make_fref((intptr_t)BKE_pbvh_get_bmesh(pbvh)->ftable[index]);
  }

  return BKE_pbvh_make_fref(PBVH_REF_NONE);
}

/* Callbacks */

/**
 * Returns true if the search should continue from this node, false otherwise.
 */
typedef bool (*BKE_pbvh_SearchCallback)(PBVHNode *node, void *data);

typedef void (*BKE_pbvh_HitCallback)(PBVHNode *node, void *data);
typedef void (*BKE_pbvh_HitOccludedCallback)(PBVHNode *node, void *data, float *tmin);

typedef void (*BKE_pbvh_SearchNearestCallback)(PBVHNode *node, void *data, float *tmin);

/* Building */

PBVH *BKE_pbvh_new(PBVHType type);

/**
 * Do a full rebuild with on Mesh data structure.
 */
void BKE_pbvh_build_mesh(PBVH *pbvh, struct Mesh *mesh);
void BKE_pbvh_update_mesh_pointers(PBVH *pbvh, struct Mesh *mesh);
/**
 * Do a full rebuild with on Grids data structure.
 */
void BKE_pbvh_build_grids(PBVH *pbvh,
                          struct CCGElem **grids,
                          int totgrid,
                          struct CCGKey *key,
                          void **gridfaces,
                          struct DMFlagMat *flagmats,
                          unsigned int **grid_hidden,
                          struct Mesh *me,
                          struct SubdivCCG *subdiv_ccg);
/**
 * Build a PBVH from a BMesh.
 */
void BKE_pbvh_build_bmesh(PBVH *pbvh,
                          struct BMesh *bm,
                          bool smooth_shading,
                          struct BMLog *log,
                          int cd_vert_node_offset,
                          int cd_face_node_offset);

void BKE_pbvh_update_bmesh_offsets(PBVH *pbvh, int cd_vert_node_offset, int cd_face_node_offset);

void BKE_pbvh_build_pixels(PBVH *pbvh,
                           struct Mesh *mesh,
                           struct Image *image,
                           struct ImageUser *image_user);
void BKE_pbvh_free(PBVH *pbvh);

/* Hierarchical Search in the BVH, two methods:
 * - For each hit calling a callback.
 * - Gather nodes in an array (easy to multi-thread) see blender::bke::pbvh::search_gather.
 */

void BKE_pbvh_search_callback(PBVH *pbvh,
                              BKE_pbvh_SearchCallback scb,
                              void *search_data,
                              BKE_pbvh_HitCallback hcb,
                              void *hit_data);

/* Ray-cast
 * the hit callback is called for all leaf nodes intersecting the ray;
 * it's up to the callback to find the primitive within the leaves that is
 * hit first */

void BKE_pbvh_raycast(PBVH *pbvh,
                      BKE_pbvh_HitOccludedCallback cb,
                      void *data,
                      const float ray_start[3],
                      const float ray_normal[3],
                      bool original);

bool BKE_pbvh_node_raycast(PBVH *pbvh,
                           PBVHNode *node,
                           float (*origco)[3],
                           bool use_origco,
                           const float ray_start[3],
                           const float ray_normal[3],
                           struct IsectRayPrecalc *isect_precalc,
                           float *depth,
                           PBVHVertRef *active_vertex,
                           int *active_face_grid_index,
                           float *face_normal);

bool BKE_pbvh_bmesh_node_raycast_detail(PBVHNode *node,
                                        const float ray_start[3],
                                        struct IsectRayPrecalc *isect_precalc,
                                        float *depth,
                                        float *r_edge_length);

/**
 * For orthographic cameras, project the far away ray segment points to the root node so
 * we can have better precision.
 */
void BKE_pbvh_raycast_project_ray_root(
    PBVH *pbvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3]);

void BKE_pbvh_find_nearest_to_ray(PBVH *pbvh,
                                  BKE_pbvh_HitOccludedCallback cb,
                                  void *data,
                                  const float ray_start[3],
                                  const float ray_normal[3],
                                  bool original);

bool BKE_pbvh_node_find_nearest_to_ray(PBVH *pbvh,
                                       PBVHNode *node,
                                       float (*origco)[3],
                                       bool use_origco,
                                       const float ray_start[3],
                                       const float ray_normal[3],
                                       float *depth,
                                       float *dist_sq);

/* Drawing */

void BKE_pbvh_draw_cb(PBVH *pbvh,
                      bool update_only_visible,
                      PBVHFrustumPlanes *update_frustum,
                      PBVHFrustumPlanes *draw_frustum,
                      void (*draw_fn)(void *user_data,
                                      struct PBVHBatches *batches,
                                      struct PBVH_GPU_Args *args),
                      void *user_data,
                      bool full_render,
                      PBVHAttrReq *attrs,
                      int attrs_num);

void BKE_pbvh_draw_debug_cb(PBVH *pbvh,
                            void (*draw_fn)(PBVHNode *node,
                                            void *user_data,
                                            const float bmin[3],
                                            const float bmax[3],
                                            PBVHNodeFlags flag),
                            void *user_data);

/* PBVH Access */

bool BKE_pbvh_has_faces(const PBVH *pbvh);

/**
 * Get the PBVH root's bounding box.
 */
void BKE_pbvh_bounding_box(const PBVH *pbvh, float min[3], float max[3]);

/**
 * Multi-res hidden data, only valid for type == PBVH_GRIDS.
 */
unsigned int **BKE_pbvh_grid_hidden(const PBVH *pbvh);

void BKE_pbvh_sync_visibility_from_verts(PBVH *pbvh, struct Mesh *me);

/**
 * Returns the number of visible quads in the nodes' grids.
 */
int BKE_pbvh_count_grid_quads(BLI_bitmap **grid_hidden,
                              const int *grid_indices,
                              int totgrid,
                              int gridsize,
                              int display_gridsize);

/**
 * Multi-res level, only valid for type == #PBVH_GRIDS.
 */
const struct CCGKey *BKE_pbvh_get_grid_key(const PBVH *pbvh);

struct CCGElem **BKE_pbvh_get_grids(const PBVH *pbvh);
BLI_bitmap **BKE_pbvh_get_grid_visibility(const PBVH *pbvh);
int BKE_pbvh_get_grid_num_verts(const PBVH *pbvh);
int BKE_pbvh_get_grid_num_faces(const PBVH *pbvh);

/**
 * Only valid for type == #PBVH_BMESH.
 */
void BKE_pbvh_bmesh_detail_size_set(PBVH *pbvh, float detail_size);

typedef enum {
  PBVH_Subdivide = 1,
  PBVH_Collapse = 2,
} PBVHTopologyUpdateMode;
ENUM_OPERATORS(PBVHTopologyUpdateMode, PBVH_Collapse);

/**
 * Collapse short edges, subdivide long edges.
 */
bool BKE_pbvh_bmesh_update_topology(PBVH *pbvh,
                                    PBVHTopologyUpdateMode mode,
                                    const float center[3],
                                    const float view_normal[3],
                                    float radius,
                                    bool use_frontface,
                                    bool use_projected);

/* Node Access */

void BKE_pbvh_node_mark_update(PBVHNode *node);
void BKE_pbvh_node_mark_update_mask(PBVHNode *node);
void BKE_pbvh_node_mark_update_color(PBVHNode *node);
void BKE_pbvh_node_mark_update_face_sets(PBVHNode *node);
void BKE_pbvh_node_mark_update_visibility(PBVHNode *node);
void BKE_pbvh_node_mark_rebuild_draw(PBVHNode *node);
void BKE_pbvh_node_mark_redraw(PBVHNode *node);
void BKE_pbvh_node_mark_normals_update(PBVHNode *node);
void BKE_pbvh_node_mark_topology_update(PBVHNode *node);
void BKE_pbvh_node_fully_hidden_set(PBVHNode *node, int fully_hidden);
bool BKE_pbvh_node_fully_hidden_get(PBVHNode *node);
void BKE_pbvh_node_fully_masked_set(PBVHNode *node, int fully_masked);
bool BKE_pbvh_node_fully_masked_get(PBVHNode *node);
void BKE_pbvh_node_fully_unmasked_set(PBVHNode *node, int fully_masked);
bool BKE_pbvh_node_fully_unmasked_get(PBVHNode *node);

void BKE_pbvh_mark_rebuild_pixels(PBVH *pbvh);
void BKE_pbvh_vert_tag_update_normal(PBVH *pbvh, PBVHVertRef vertex);

void BKE_pbvh_node_get_grids(PBVH *pbvh,
                             PBVHNode *node,
                             int **grid_indices,
                             int *totgrid,
                             int *maxgrid,
                             int *gridsize,
                             struct CCGElem ***r_griddata);
void BKE_pbvh_node_num_verts(PBVH *pbvh, PBVHNode *node, int *r_uniquevert, int *r_totvert);
const int *BKE_pbvh_node_get_vert_indices(PBVHNode *node);
void BKE_pbvh_node_get_loops(PBVH *pbvh,
                             PBVHNode *node,
                             const int **r_loop_indices,
                             const int **r_corner_verts);

/* Get number of faces in the mesh; for PBVH_GRIDS the
 * number of base mesh faces is returned.
 */
int BKE_pbvh_num_faces(const PBVH *pbvh);

void BKE_pbvh_node_get_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);
void BKE_pbvh_node_get_original_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);

float BKE_pbvh_node_get_tmin(PBVHNode *node);

/**
 * Test if AABB is at least partially inside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_contain_AABB(PBVHNode *node, void *frustum);
/**
 * Test if AABB is at least partially outside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_exclude_AABB(PBVHNode *node, void *frustum);

struct GSet *BKE_pbvh_bmesh_node_unique_verts(PBVHNode *node);
struct GSet *BKE_pbvh_bmesh_node_other_verts(PBVHNode *node);
struct GSet *BKE_pbvh_bmesh_node_faces(PBVHNode *node);
/**
 * In order to perform operations on the original node coordinates
 * (currently just ray-cast), store the node's triangles and vertices.
 *
 * Skips triangles that are hidden.
 */
void BKE_pbvh_bmesh_node_save_orig(struct BMesh *bm,
                                   struct BMLog *log,
                                   PBVHNode *node,
                                   bool use_original);
void BKE_pbvh_bmesh_after_stroke(PBVH *pbvh);

/* Update Bounding Box/Redraw and clear flags. */

void BKE_pbvh_update_bounds(PBVH *pbvh, int flags);
void BKE_pbvh_update_vertex_data(PBVH *pbvh, int flags);
void BKE_pbvh_update_visibility(PBVH *pbvh);
void BKE_pbvh_update_normals(PBVH *pbvh, struct SubdivCCG *subdiv_ccg);
void BKE_pbvh_redraw_BB(PBVH *pbvh, float bb_min[3], float bb_max[3]);
void BKE_pbvh_get_grid_updates(PBVH *pbvh, bool clear, void ***r_gridfaces, int *r_totface);
void BKE_pbvh_grids_update(PBVH *pbvh,
                           struct CCGElem **grids,
                           void **gridfaces,
                           struct DMFlagMat *flagmats,
                           unsigned int **grid_hidden,
                           struct CCGKey *key);
void BKE_pbvh_subdiv_cgg_set(PBVH *pbvh, struct SubdivCCG *subdiv_ccg);
void BKE_pbvh_face_sets_set(PBVH *pbvh, int *face_sets);

/**
 * If an operation causes the hide status stored in the mesh to change, this must be called
 * to update the references to those attributes, since they are only added when necessary.
 */
void BKE_pbvh_update_hide_attributes_from_mesh(PBVH *pbvh);

void BKE_pbvh_face_sets_color_set(PBVH *pbvh, int seed, int color_default);

/* Vertex Deformer. */

float (*BKE_pbvh_vert_coords_alloc(struct PBVH *pbvh))[3];
void BKE_pbvh_vert_coords_apply(struct PBVH *pbvh, const float (*vertCos)[3], int totvert);
bool BKE_pbvh_is_deformed(struct PBVH *pbvh);

/* Vertex Iterator. */

/* This iterator has quite a lot of code, but it's designed to:
 * - allow the compiler to eliminate dead code and variables
 * - spend most of the time in the relatively simple inner loop */

/* NOTE: PBVH_ITER_ALL does not skip hidden vertices,
 * PBVH_ITER_UNIQUE does */
#define PBVH_ITER_ALL 0
#define PBVH_ITER_UNIQUE 1

typedef struct PBVHVertexIter {
  /* iteration */
  int g;
  int width;
  int height;
  int gx;
  int gy;
  int i;
  int index;
  PBVHVertRef vertex;

  /* grid */
  struct CCGKey key;
  struct CCGElem **grids;
  struct CCGElem *grid;
  BLI_bitmap **grid_hidden, *gh;
  int *grid_indices;
  int totgrid;
  int gridsize;

  /* mesh */
  float (*vert_positions)[3];
  float (*vert_normals)[3];
  const bool *hide_vert;
  int totvert;
  const int *vert_indices;
  float *vmask;
  bool is_mesh;

  /* bmesh */
  struct GSetIterator bm_unique_verts;
  struct GSetIterator bm_other_verts;
  struct CustomData *bm_vdata;
  int cd_vert_mask_offset;

  /* result: these are all computed in the macro, but we assume
   * that compiler optimization's will skip the ones we don't use */
  struct BMVert *bm_vert;
  float *co;
  float *no;
  float *fno;
  float *mask;
  bool visible;
} PBVHVertexIter;

void pbvh_vertex_iter_init(PBVH *pbvh, PBVHNode *node, PBVHVertexIter *vi, int mode);

#define BKE_pbvh_vertex_iter_begin(pbvh, node, vi, mode) \
  pbvh_vertex_iter_init(pbvh, node, &vi, mode); \
\
  for (vi.i = 0, vi.g = 0; vi.g < vi.totgrid; vi.g++) { \
    if (vi.grids) { \
      vi.width = vi.gridsize; \
      vi.height = vi.gridsize; \
      vi.index = vi.vertex.i = vi.grid_indices[vi.g] * vi.key.grid_area - 1; \
      vi.grid = vi.grids[vi.grid_indices[vi.g]]; \
      if (mode == PBVH_ITER_UNIQUE) { \
        vi.gh = vi.grid_hidden[vi.grid_indices[vi.g]]; \
      } \
    } \
    else { \
      vi.width = vi.totvert; \
      vi.height = 1; \
    } \
\
    for (vi.gy = 0; vi.gy < vi.height; vi.gy++) { \
      for (vi.gx = 0; vi.gx < vi.width; vi.gx++, vi.i++) { \
        if (vi.grid) { \
          vi.co = CCG_elem_co(&vi.key, vi.grid); \
          vi.fno = CCG_elem_no(&vi.key, vi.grid); \
          vi.mask = vi.key.has_mask ? CCG_elem_mask(&vi.key, vi.grid) : NULL; \
          vi.grid = CCG_elem_next(&vi.key, vi.grid); \
          vi.index++; \
          vi.vertex.i++; \
          vi.visible = true; \
          if (vi.gh) { \
            if (BLI_BITMAP_TEST(vi.gh, vi.gy * vi.gridsize + vi.gx)) { \
              continue; \
            } \
          } \
        } \
        else if (vi.vert_positions) { \
          vi.visible = !(vi.hide_vert && vi.hide_vert[vi.vert_indices[vi.gx]]); \
          if (mode == PBVH_ITER_UNIQUE && !vi.visible) { \
            continue; \
          } \
          vi.co = vi.vert_positions[vi.vert_indices[vi.gx]]; \
          vi.no = vi.vert_normals[vi.vert_indices[vi.gx]]; \
          vi.index = vi.vertex.i = vi.vert_indices[vi.i]; \
          if (vi.vmask) { \
            vi.mask = &vi.vmask[vi.index]; \
          } \
        } \
        else { \
          if (!BLI_gsetIterator_done(&vi.bm_unique_verts)) { \
            vi.bm_vert = (BMVert *)BLI_gsetIterator_getKey(&vi.bm_unique_verts); \
            BLI_gsetIterator_step(&vi.bm_unique_verts); \
          } \
          else { \
            vi.bm_vert = (BMVert *)BLI_gsetIterator_getKey(&vi.bm_other_verts); \
            BLI_gsetIterator_step(&vi.bm_other_verts); \
          } \
          vi.visible = !BM_elem_flag_test_bool(vi.bm_vert, BM_ELEM_HIDDEN); \
          if (mode == PBVH_ITER_UNIQUE && !vi.visible) { \
            continue; \
          } \
          vi.co = vi.bm_vert->co; \
          vi.fno = vi.bm_vert->no; \
          vi.vertex = BKE_pbvh_make_vref((intptr_t)vi.bm_vert); \
          vi.index = BM_elem_index_get(vi.bm_vert); \
          vi.mask = (float *)BM_ELEM_CD_GET_VOID_P(vi.bm_vert, vi.cd_vert_mask_offset); \
        }

#define BKE_pbvh_vertex_iter_end \
  } \
  } \
  } \
  ((void)0)

#define PBVH_FACE_ITER_VERTS_RESERVED 8

typedef struct PBVHFaceIter {
  PBVHFaceRef face;
  int index;
  bool *hide;
  int *face_set;
  int i;

  PBVHVertRef *verts;
  int verts_num;

  PBVHVertRef verts_reserved_[PBVH_FACE_ITER_VERTS_RESERVED];
  const PBVHNode *node_;
  PBVHType pbvh_type_;
  int verts_size_;
  GSetIterator bm_faces_iter_;
  int cd_hide_poly_, cd_face_set_;
  bool *hide_poly_;
  int *face_sets_;
  const int *poly_offsets_;
  const int *looptri_polys_;
  const int *corner_verts_;
  int prim_index_;
  const struct SubdivCCG *subdiv_ccg_;
  const struct BMesh *bm;
  CCGKey subdiv_key_;

  int last_poly_index_;
} PBVHFaceIter;

void BKE_pbvh_face_iter_init(PBVH *pbvh, PBVHNode *node, PBVHFaceIter *fd);
void BKE_pbvh_face_iter_step(PBVHFaceIter *fd);
bool BKE_pbvh_face_iter_done(PBVHFaceIter *fd);
void BKE_pbvh_face_iter_finish(PBVHFaceIter *fd);

/**
 * Iterate over faces inside a #PBVHNode. These are either base mesh faces
 * (for PBVH_FACES and PBVH_GRIDS) or BMesh faces (for PBVH_BMESH).
 */
#define BKE_pbvh_face_iter_begin(pbvh, node, fd) \
  BKE_pbvh_face_iter_init(pbvh, node, &fd); \
  for (; !BKE_pbvh_face_iter_done(&fd); BKE_pbvh_face_iter_step(&fd)) {

#define BKE_pbvh_face_iter_end(fd) \
  } \
  BKE_pbvh_face_iter_finish(&fd)

void BKE_pbvh_node_get_proxies(PBVHNode *node, PBVHProxyNode **proxies, int *proxy_count);
void BKE_pbvh_node_free_proxies(PBVHNode *node);
PBVHProxyNode *BKE_pbvh_node_add_proxy(PBVH *pbvh, PBVHNode *node);
void BKE_pbvh_node_get_bm_orco_data(PBVHNode *node,
                                    int (**r_orco_tris)[3],
                                    int *r_orco_tris_num,
                                    float (**r_orco_coords)[3],
                                    struct BMVert ***r_orco_verts);

/**
 * \note doing a full search on all vertices here seems expensive,
 * however this is important to avoid having to recalculate bound-box & sync the buffers to the
 * GPU (which is far more expensive!) See: #47232.
 */
bool BKE_pbvh_node_has_vert_with_normal_update_tag(PBVH *pbvh, PBVHNode *node);

// void BKE_pbvh_node_BB_reset(PBVHNode *node);
// void BKE_pbvh_node_BB_expand(PBVHNode *node, float co[3]);

bool pbvh_has_mask(const PBVH *pbvh);

bool pbvh_has_face_sets(PBVH *pbvh);

/* Parallelization. */

void BKE_pbvh_parallel_range_settings(struct TaskParallelSettings *settings,
                                      bool use_threading,
                                      int totnode);

float (*BKE_pbvh_get_vert_positions(const PBVH *pbvh))[3];
const float (*BKE_pbvh_get_vert_normals(const PBVH *pbvh))[3];
const bool *BKE_pbvh_get_vert_hide(const PBVH *pbvh);
bool *BKE_pbvh_get_vert_hide_for_write(PBVH *pbvh);

const bool *BKE_pbvh_get_poly_hide(const PBVH *pbvh);

PBVHColorBufferNode *BKE_pbvh_node_color_buffer_get(PBVHNode *node);
void BKE_pbvh_node_color_buffer_free(PBVH *pbvh);
bool BKE_pbvh_get_color_layer(const struct Mesh *me,
                              CustomDataLayer **r_layer,
                              eAttrDomain *r_attr);

/* Swaps colors at each element in indices (of domain pbvh->vcol_domain)
 * with values in colors. */
void BKE_pbvh_swap_colors(PBVH *pbvh,
                          const int *indices,
                          const int indices_num,
                          float (*colors)[4]);

/* Stores colors from the elements in indices (of domain pbvh->vcol_domain)
 * into colors. */
void BKE_pbvh_store_colors(PBVH *pbvh,
                           const int *indices,
                           const int indices_num,
                           float (*colors)[4]);

/* Like BKE_pbvh_store_colors but handles loop->vert conversion */
void BKE_pbvh_store_colors_vertex(PBVH *pbvh,
                                  const int *indices,
                                  const int indices_num,
                                  float (*colors)[4]);

bool BKE_pbvh_is_drawing(const PBVH *pbvh);
void BKE_pbvh_is_drawing_set(PBVH *pbvh, bool val);

/* Do not call in PBVH_GRIDS mode */
void BKE_pbvh_node_num_loops(PBVH *pbvh, PBVHNode *node, int *r_totloop);

void BKE_pbvh_update_active_vcol(PBVH *pbvh, const struct Mesh *mesh);

void BKE_pbvh_vertex_color_set(PBVH *pbvh, PBVHVertRef vertex, const float color[4]);
void BKE_pbvh_vertex_color_get(const PBVH *pbvh, PBVHVertRef vertex, float r_color[4]);

void BKE_pbvh_ensure_node_loops(PBVH *pbvh);
bool BKE_pbvh_draw_cache_invalid(const PBVH *pbvh);
int BKE_pbvh_debug_draw_gen_get(PBVHNode *node);

#ifdef __cplusplus
void BKE_pbvh_pmap_set(PBVH *pbvh, blender::GroupedSpan<int> pmap);
}

namespace blender::bke::pbvh {
Vector<PBVHNode *> search_gather(PBVH *pbvh,
                                 BKE_pbvh_SearchCallback scb,
                                 void *search_data,
                                 PBVHNodeFlags leaf_flag = PBVH_Leaf);
Vector<PBVHNode *> gather_proxies(PBVH *pbvh);

}  // namespace blender::bke::pbvh
#endif
