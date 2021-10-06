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
 */

#pragma once

/** \file
 * \ingroup bke
 * \brief A BVH for high poly meshes.
 */

#include "BLI_bitmap.h"
#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"

/* For embedding CCGKey in iterator. */
#include "BKE_ccg.h"
#include <stdint.h>

//#define DEFRAGMENT_MEMORY

#ifdef __cplusplus
extern "C" {
#endif

// experimental feature to detect quad diagonals and mark (but not dissolve) them
//#define SCULPT_DIAGONAL_EDGE_MARKS

/*
   These structs represent logical verts/edges/faces.
   for PBVH_GRIDS and PBVH_FACES they store integer
   offsets, PBVH_BMESH stores pointers.

   The idea is to enforce stronger type checking by encapsulating
   intptr_t's in structs.*/
typedef struct SculptElemRef {
  intptr_t i;
} SculptElemRef;

typedef struct SculptVertRef {
  intptr_t i;
} SculptVertRef;

typedef struct SculptEdgeRef {
  intptr_t i;
} SculptEdgeRef;

typedef struct SculptFaceRef {
  intptr_t i;
} SculptFaceRef;

#define SCULPT_REF_NONE ((intptr_t)-1)

#if 0
typedef struct SculptLoopRef {
  intptr_t i;
} SculptLoopRef;
#endif

BLI_INLINE SculptVertRef BKE_pbvh_make_vref(intptr_t i)
{
  SculptVertRef ret = {i};
  return ret;
}

BLI_INLINE SculptEdgeRef BKE_pbvh_make_eref(intptr_t i)
{
  SculptEdgeRef ret = {i};
  return ret;
}

BLI_INLINE SculptFaceRef BKE_pbvh_make_fref(intptr_t i)
{
  SculptFaceRef ret = {i};
  return ret;
}

#ifdef DEFRAGMENT_MEMORY
#  include "BLI_smallhash.h"
#endif

typedef struct PBVHTri {
  int v[3];       // references into PBVHTriBuf->verts
  intptr_t l[3];  // loops
  int eflag;      // bitmask of which edges in the tri are real edges in the mesh

  float no[3];
  SculptFaceRef f;
} PBVHTri;

typedef struct PBVHTriBuf {
  PBVHTri *tris;
  SculptVertRef *verts;
  int *edges;
  int totvert, totedge, tottri;
  int verts_size, edges_size, tris_size;

  SmallHash vertmap;  // maps vertex ptrs to indices within verts

  // private field
  intptr_t *loops;
  int totloop, mat_nr;
  float min[3], max[3];
} PBVHTriBuf;

struct BMLog;
struct BMesh;
struct BMVert;
struct BMEdge;
struct BMFace;
struct CCGElem;
struct MeshElemMap;
struct CCGKey;
struct CustomData;
struct TableGSet;
struct DMFlagMat;
struct GPU_PBVH_Buffers;
struct IsectRayPrecalc;
struct MLoop;
struct MLoopTri;
struct MDynTopoVert;
struct MPoly;
struct MVert;
struct Mesh;
struct PBVH;
struct MEdge;
struct PBVHNode;
struct SubdivCCG;
struct TaskParallelSettings;

typedef struct PBVH PBVH;
typedef struct PBVHNode PBVHNode;

//#define PROXY_ADVANCED

// experimental performance test of "data-based programming" approach
#ifdef PROXY_ADVANCED
typedef struct ProxyKey {
  int node;
  int pindex;
} ProxyKey;

#  define MAX_PROXY_NEIGHBORS 12

typedef struct ProxyVertArray {
  float **ownerco;
  short **ownerno;
  float (*co)[3];
  float (*fno)[3];
  short (*no)[3];
  float *mask, **ownermask;
  SculptVertRef *index;
  float **ownercolor, (*color)[4];

  ProxyKey (*neighbors)[MAX_PROXY_NEIGHBORS];

  int size;
  int datamask;
  bool neighbors_dirty;

  GHash *indexmap;
} ProxyVertArray;

typedef enum {
  PV_OWNERCO = 1,
  PV_OWNERNO = 2,
  PV_CO = 4,
  PV_NO = 8,
  PV_MASK = 16,
  PV_OWNERMASK = 32,
  PV_INDEX = 64,
  PV_OWNERCOLOR = 128,
  PV_COLOR = 256,
  PV_NEIGHBORS = 512
} ProxyVertField;

typedef struct ProxyVertUpdateRec {
  float *co, *no, *mask, *color;
  SculptVertRef index, newindex;
} ProxyVertUpdateRec;

#  define PBVH_PROXY_DEFAULT CO | INDEX | MASK

struct SculptSession;

void BKE_pbvh_ensure_proxyarrays(
    struct SculptSession *ss, PBVH *pbvh, PBVHNode **nodes, int totnode, int mask);
void BKE_pbvh_load_proxyarrays(PBVH *pbvh, PBVHNode **nodes, int totnode, int mask);

void BKE_pbvh_ensure_proxyarray(
    struct SculptSession *ss,
    struct PBVH *pbvh,
    struct PBVHNode *node,
    int mask,
    struct GHash
        *vert_node_map,  // vert_node_map maps vertex SculptVertRefs to PBVHNode indices; optional
    bool check_indexmap,
    bool force_update);
void BKE_pbvh_gather_proxyarray(PBVH *pbvh, PBVHNode **nodes, int totnode);

void BKE_pbvh_free_proxyarray(struct PBVH *pbvh, struct PBVHNode *node);
void BKE_pbvh_update_proxyvert(struct PBVH *pbvh, struct PBVHNode *node, ProxyVertUpdateRec *rec);
ProxyVertArray *BKE_pbvh_get_proxyarrays(struct PBVH *pbvh, struct PBVHNode *node);

#endif

typedef struct {
  float (*co)[3];
} PBVHProxyNode;

typedef struct {
  float (*color)[4];
  int size;
} PBVHColorBufferNode;

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
  PBVH_Delete = 1 << 15,
  PBVH_UpdateCurvatureDir = 1 << 16,
  PBVH_UpdateTris = 1 << 17,
  PBVH_RebuildNodeVerts = 1 << 18,

  /* tri areas are not guaranteed to be up to date, tools should
     update all nodes on first step of brush*/
  PBVH_UpdateTriAreas = 1 << 19,
  PBVH_UpdateOtherVerts = 1 << 20
} PBVHNodeFlags;

typedef struct PBVHFrustumPlanes {
  float (*planes)[4];
  int num_planes;
} PBVHFrustumPlanes;

void BKE_pbvh_set_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes);
void BKE_pbvh_get_frustum_planes(PBVH *pbvh, PBVHFrustumPlanes *planes);

/* Callbacks */

/* returns 1 if the search should continue from this node, 0 otherwise */
typedef bool (*BKE_pbvh_SearchCallback)(PBVHNode *node, void *data);

typedef void (*BKE_pbvh_HitCallback)(PBVHNode *node, void *data);
typedef void (*BKE_pbvh_HitOccludedCallback)(PBVHNode *node, void *data, float *tmin);

typedef void (*BKE_pbvh_SearchNearestCallback)(PBVHNode *node, void *data, float *tmin);

void BKE_pbvh_get_nodes(PBVH *pbvh, int flag, PBVHNode ***r_array, int *r_totnode);
PBVHNode *BKE_pbvh_get_node(PBVH *pbvh, int node);

/* Building */

PBVH *BKE_pbvh_new(void);
void BKE_pbvh_build_mesh(PBVH *pbvh,
                         const struct Mesh *mesh,
                         const struct MPoly *mpoly,
                         const struct MLoop *mloop,
                         struct MVert *verts,
                         struct MDynTopoVert *mdyntopo_verts,
                         int totvert,
                         struct CustomData *vdata,
                         struct CustomData *ldata,
                         struct CustomData *pdata,
                         const struct MLoopTri *looptri,
                         int looptri_num,
                         bool fast_draw);
void BKE_pbvh_build_grids(PBVH *pbvh,
                          struct CCGElem **grids,
                          int totgrid,
                          struct CCGKey *key,
                          void **gridfaces,
                          struct DMFlagMat *flagmats,
                          unsigned int **grid_hidden,
                          bool fast_draw);
void BKE_pbvh_build_bmesh(PBVH *pbvh,
                          struct BMesh *bm,
                          bool smooth_shading,
                          struct BMLog *log,
                          const int cd_vert_node_offset,
                          const int cd_face_node_offset,
                          const int cd_dyn_vert,
                          const int cd_face_areas,
                          bool fast_draw);
void BKE_pbvh_update_offsets(PBVH *pbvh,
                             const int cd_vert_node_offset,
                             const int cd_face_node_offset,
                             const int cd_dyn_vert,
                             const int cd_face_areas);
void BKE_pbvh_free(PBVH *pbvh);

void BKE_pbvh_set_bm_log(PBVH *pbvh, struct BMLog *log);

/** update original data, only data whose r_** parameters are passed in will be updated*/
void BKE_pbvh_bmesh_update_origvert(
    PBVH *pbvh, struct BMVert *v, float **r_co, float **r_no, float **r_color, bool log_undo);
void BKE_pbvh_update_origcolor_bmesh(PBVH *pbvh, PBVHNode *node);
void BKE_pbvh_update_origco_bmesh(PBVH *pbvh, PBVHNode *node);

/**
checks if original data needs to be updated for v, and if so updates it.  Stroke_id
is provided by the sculpt code and is used to detect updates.  The reason we do it
inside the verts and not in the nodes is to allow splitting of the pbvh during the stroke.
*/
bool BKE_pbvh_bmesh_check_origdata(PBVH *pbvh, struct BMVert *v, int stroke_id);

/** used so pbvh can differentiate between different strokes,
    see BKE_pbvh_bmesh_check_origdata */
void BKE_pbvh_set_stroke_id(PBVH *pbvh, int stroke_id);

/* Hierarchical Search in the BVH, two methods:
 * - for each hit calling a callback
 * - gather nodes in an array (easy to multithread) */

void BKE_pbvh_search_callback(PBVH *pbvh,
                              BKE_pbvh_SearchCallback scb,
                              void *search_data,
                              BKE_pbvh_HitCallback hcb,
                              void *hit_data);

void BKE_pbvh_search_gather(
    PBVH *pbvh, BKE_pbvh_SearchCallback scb, void *search_data, PBVHNode ***array, int *tot);

/* Raycast
 * the hit callback is called for all leaf nodes intersecting the ray;
 * it's up to the callback to find the primitive within the leaves that is
 * hit first */

void BKE_pbvh_raycast(PBVH *pbvh,
                      BKE_pbvh_HitOccludedCallback cb,
                      void *data,
                      const float ray_start[3],
                      const float ray_normal[3],
                      bool original,
                      int stroke_id);

bool BKE_pbvh_node_raycast(PBVH *pbvh,
                           PBVHNode *node,
                           float (*origco)[3],
                           bool use_origco,
                           const float ray_start[3],
                           const float ray_normal[3],
                           struct IsectRayPrecalc *isect_precalc,
                           int *hit_count,
                           float *depth,
                           float *back_depth,
                           SculptVertRef *active_vertex_index,
                           SculptFaceRef *active_face_grid_index,
                           float *face_normal,
                           int stroke_id);

bool BKE_pbvh_bmesh_node_raycast_detail(PBVH *pbvh,
                                        PBVHNode *node,
                                        const float ray_start[3],
                                        struct IsectRayPrecalc *isect_precalc,
                                        float *depth,
                                        float *r_edge_length);

/* for orthographic cameras, project the far away ray segment points to the root node so
 * we can have better precision. */
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
                                       float *dist_sq,
                                       int stroke_id);

/* Drawing */

void BKE_pbvh_draw_cb(PBVH *pbvh,
                      bool update_only_visible,
                      PBVHFrustumPlanes *update_frustum,
                      PBVHFrustumPlanes *draw_frustum,
                      void (*draw_fn)(void *user_data, struct GPU_PBVH_Buffers *buffers),
                      void *user_data);

void BKE_pbvh_draw_debug_cb(
    PBVH *pbvh,
    void (*draw_fn)(void *user_data, const float bmin[3], const float bmax[3], PBVHNodeFlags flag),
    void *user_data);

/* PBVH Access */
typedef enum {
  PBVH_FACES,
  PBVH_GRIDS,
  PBVH_BMESH,
} PBVHType;

PBVHType BKE_pbvh_type(const PBVH *pbvh);
bool BKE_pbvh_has_faces(const PBVH *pbvh);

/* Get the PBVH root's bounding box */
void BKE_pbvh_bounding_box(const PBVH *pbvh, float min[3], float max[3]);

/* multires hidden data, only valid for type == PBVH_GRIDS */
unsigned int **BKE_pbvh_grid_hidden(const PBVH *pbvh);

int BKE_pbvh_count_grid_quads(BLI_bitmap **grid_hidden,
                              const int *grid_indices,
                              int totgrid,
                              int gridsize);

void BKE_pbvh_sync_face_sets_to_grids(PBVH *pbvh);

/* multires level, only valid for type == PBVH_GRIDS */
const struct CCGKey *BKE_pbvh_get_grid_key(const PBVH *pbvh);

struct CCGElem **BKE_pbvh_get_grids(const PBVH *pbvh);
BLI_bitmap **BKE_pbvh_get_grid_visibility(const PBVH *pbvh);
int BKE_pbvh_get_grid_num_vertices(const PBVH *pbvh);
int BKE_pbvh_get_grid_num_faces(const PBVH *pbvh);

/* Only valid for type == PBVH_BMESH */
struct BMesh *BKE_pbvh_get_bmesh(PBVH *pbvh);
void BKE_pbvh_bmesh_detail_size_set(PBVH *pbvh, float detail_size, float detail_range);

typedef enum {
  PBVH_Subdivide = 1 << 0,
  PBVH_Collapse = 1 << 1,
  PBVH_Cleanup = 1 << 2,  // dissolve verts surrounded by either 3 or 4 triangles then triangulate
  PBVH_LocalSubdivide = 1 << 3,
  PBVH_LocalCollapse = 1 << 4
} PBVHTopologyUpdateMode;

typedef float (*DyntopoMaskCB)(SculptVertRef vertex, void *userdata);

bool BKE_pbvh_bmesh_update_topology(
    PBVH *pbvh,
    PBVHTopologyUpdateMode mode,
    const float center[3],
    const float view_normal[3],
    float radius,
    const bool use_frontface,
    const bool use_projected,
    int symaxis,
    bool updatePBVH,
    DyntopoMaskCB mask_cb,
    void *mask_cb_data,
    int custom_max_steps,  // if 0, will use defaul hueristics for max steps
    bool disable_surface_relax);

bool BKE_pbvh_bmesh_update_topology_nodes(PBVH *pbvh,
                                          bool (*searchcb)(PBVHNode *node, void *data),
                                          void (*undopush)(PBVHNode *node, void *data),
                                          void *searchdata,
                                          PBVHTopologyUpdateMode mode,
                                          const float center[3],
                                          const float view_normal[3],
                                          float radius,
                                          const bool use_frontface,
                                          const bool use_projected,
                                          int sym_axis,
                                          bool updatePBVH,
                                          DyntopoMaskCB mask_cb,
                                          void *mask_cb_data,
                                          bool disable_surface_relax);
/* Node Access */

void BKE_pbvh_check_tri_areas(PBVH *pbvh, PBVHNode *node);

// updates boundaries and valences for whole mesh
void BKE_pbvh_bmesh_on_mesh_change(PBVH *pbvh);
bool BKE_pbvh_bmesh_check_valence(PBVH *pbvh, SculptVertRef vertex);
void BKE_pbvh_bmesh_update_valence(int cd_dyn_vert, SculptVertRef vertex);
void BKE_pbvh_bmesh_update_all_valence(PBVH *pbvh);
void BKE_pbvh_bmesh_flag_all_disk_sort(PBVH *pbvh);
bool BKE_pbvh_bmesh_mark_update_valence(PBVH *pbvh, SculptVertRef vertex);

void BKE_pbvh_node_mark_original_update(PBVHNode *node);
void BKE_pbvh_node_mark_update_tri_area(PBVHNode *node);
void BKE_pbvh_update_all_tri_areas(PBVH *pbvh);
void BKE_pbvh_node_mark_update(PBVHNode *node);
void BKE_pbvh_node_mark_update_mask(PBVHNode *node);
void BKE_pbvh_node_mark_update_color(PBVHNode *node);
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

void BKE_pbvh_node_get_grids(PBVH *pbvh,
                             PBVHNode *node,
                             int **grid_indices,
                             int *totgrid,
                             int *maxgrid,
                             int *gridsize,
                             struct CCGElem ***r_griddata);
void BKE_pbvh_node_num_verts(PBVH *pbvh, PBVHNode *node, int *r_uniquevert, int *r_totvert);
void BKE_pbvh_node_get_verts(PBVH *pbvh,
                             PBVHNode *node,
                             const int **r_vert_indices,
                             struct MVert **r_verts);

void BKE_pbvh_node_get_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);
void BKE_pbvh_node_get_original_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);

float BKE_pbvh_node_get_tmin(PBVHNode *node);

/* test if AABB is at least partially inside the PBVHFrustumPlanes volume */
bool BKE_pbvh_node_frustum_contain_AABB(PBVHNode *node, void *frustum);
/* test if AABB is at least partially outside the PBVHFrustumPlanes volume */
bool BKE_pbvh_node_frustum_exclude_AABB(PBVHNode *node, void *frustum);

struct TableGSet *BKE_pbvh_bmesh_node_unique_verts(PBVHNode *node);
struct TableGSet *BKE_pbvh_bmesh_node_other_verts(PBVHNode *node);
struct TableGSet *BKE_pbvh_bmesh_node_faces(PBVHNode *node);

void BKE_pbvh_bmesh_regen_node_verts(PBVH *pbvh);
void BKE_pbvh_bmesh_mark_node_regen(PBVH *pbvh, PBVHNode *node);

// now generated PBVHTris
void BKE_pbvh_bmesh_after_stroke(PBVH *pbvh, bool force_balance);

/* Update Bounding Box/Redraw and clear flags */

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
                           unsigned int **grid_hidden);
void BKE_pbvh_subdiv_cgg_set(PBVH *pbvh, struct SubdivCCG *subdiv_ccg);
void BKE_pbvh_face_sets_set(PBVH *pbvh, int *face_sets);

void BKE_pbvh_face_sets_color_set(PBVH *pbvh, int seed, int color_default);

void BKE_pbvh_respect_hide_set(PBVH *pbvh, bool respect_hide);

/* vertex deformer */
float (*BKE_pbvh_vert_coords_alloc(struct PBVH *pbvh))[3];
void BKE_pbvh_vert_coords_apply(struct PBVH *pbvh, const float (*vertCos)[3], const int totvert);
bool BKE_pbvh_is_deformed(struct PBVH *pbvh);

/* Vertex Iterator */

/* this iterator has quite a lot of code, but it's designed to:
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
  SculptVertRef vertex;
  bool respect_hide;

  /* grid */
  struct CCGKey key;
  struct CCGElem **grids;
  struct CCGElem *grid;
  BLI_bitmap **grid_hidden, *gh;
  int *grid_indices;
  int totgrid;
  int gridsize;

  /* mesh */
  struct MVert *mverts;
  int totvert;
  const int *vert_indices;
  struct MPropCol *vcol;
  float *vmask;

  /* bmesh */
  int bi;
  struct TableGSet *bm_cur_set;
  struct TableGSet *bm_unique_verts, *bm_other_verts;

  struct CustomData *bm_vdata;
  int cd_dyn_vert;
  int cd_vert_mask_offset;
  int cd_vcol_offset;

  /* result: these are all computed in the macro, but we assume
   * that compiler optimization's will skip the ones we don't use */
  struct MVert *mvert;
  struct BMVert *bm_vert;
  float *co;
  short *no;
  float *fno;
  float *mask;
  float *col;
  bool visible;
} PBVHVertexIter;

#define BKE_PBVH_DYNVERT(cd_dyn_vert, v) ((MDynTopoVert *)BM_ELEM_CD_GET_VOID_P(v, cd_dyn_vert))

void pbvh_vertex_iter_init(PBVH *pbvh, PBVHNode *node, PBVHVertexIter *vi, int mode);

#define BKE_pbvh_vertex_iter_begin(pbvh, node, vi, mode) \
  pbvh_vertex_iter_init(pbvh, node, &vi, mode); \
\
  for (vi.i = 0, vi.g = 0; vi.g < vi.totgrid; vi.g++) { \
    if (vi.grids) { \
      vi.width = vi.gridsize; \
      vi.height = vi.gridsize; \
      vi.vertex.i = vi.index = vi.grid_indices[vi.g] * vi.key.grid_area - 1; \
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
        else if (vi.mverts) { \
          vi.mvert = &vi.mverts[vi.vert_indices[vi.gx]]; \
          if (vi.respect_hide) { \
            vi.visible = !(vi.mvert->flag & ME_HIDE); \
            if (mode == PBVH_ITER_UNIQUE && !vi.visible) { \
              continue; \
            } \
          } \
          else { \
            BLI_assert(vi.visible); \
          } \
          vi.co = vi.mvert->co; \
          vi.no = vi.mvert->no; \
          vi.index = vi.vertex.i = vi.vert_indices[vi.i]; \
          if (vi.vmask) { \
            vi.mask = &vi.vmask[vi.index]; \
          } \
          if (vi.vcol) { \
            vi.col = vi.vcol[vi.index].color; \
          } \
        } \
        else { \
          BMVert *bv = NULL; \
          while (!bv) { \
            if (!vi.bm_cur_set->elems || vi.bi >= vi.bm_cur_set->cur) { \
              if (vi.bm_cur_set != vi.bm_other_verts && mode != PBVH_ITER_UNIQUE) { \
                vi.bm_cur_set = vi.bm_other_verts; \
                vi.bi = 0; \
                if (!vi.bm_cur_set->elems || vi.bi >= vi.bm_other_verts->cur) { \
                  break; \
                } \
              } \
              else { \
                break; \
              } \
            } \
            else { \
              bv = vi.bm_cur_set->elems[vi.bi++]; \
            } \
          } \
          if (!bv) { \
            continue; \
          } \
          vi.bm_vert = bv; \
          if (vi.cd_vcol_offset >= 0) { \
            MPropCol *vcol = BM_ELEM_CD_GET_VOID_P(bv, vi.cd_vcol_offset); \
            vi.col = vcol->color; \
          } \
          vi.vertex.i = (intptr_t)bv; \
          vi.index = BM_elem_index_get(vi.bm_vert); \
          vi.visible = !BM_elem_flag_test_bool(vi.bm_vert, BM_ELEM_HIDDEN); \
          if (mode == PBVH_ITER_UNIQUE && !vi.visible) { \
            continue; \
          } \
          vi.co = vi.bm_vert->co; \
          vi.fno = vi.bm_vert->no; \
          vi.mask = (float *)BM_ELEM_CD_GET_VOID_P(vi.bm_vert, vi.cd_vert_mask_offset); \
        }

#define BKE_pbvh_vertex_iter_end \
  } \
  } \
  } \
  ((void)0)

#define BKE_pbvh_vertex_index_to_table(pbvh, v) \
  (BKE_pbvh_type(pbvh) == PBVH_BMESH && v.i != -1 ? BM_elem_index_get((BMVert *)(v.i)) : (v.i))
SculptVertRef BKE_pbvh_table_index_to_vertex(PBVH *pbvh, int idx);

#define BKE_pbvh_edge_index_to_table(pbvh, v) \
  (BKE_pbvh_type(pbvh) == PBVH_BMESH && v.i != -1 ? BM_elem_index_get((BMEdge *)(v.i)) : (v.i))
SculptEdgeRef BKE_pbvh_table_index_to_edge(PBVH *pbvh, int idx);

#define BKE_pbvh_face_index_to_table(pbvh, v) \
  (BKE_pbvh_type(pbvh) == PBVH_BMESH && v.i != -1 ? BM_elem_index_get((BMFace *)(v.i)) : (v.i))
SculptFaceRef BKE_pbvh_table_index_to_face(PBVH *pbvh, int idx);

void BKE_pbvh_node_get_proxies(PBVHNode *node, PBVHProxyNode **proxies, int *proxy_count);
void BKE_pbvh_node_free_proxies(PBVHNode *node);
PBVHProxyNode *BKE_pbvh_node_add_proxy(PBVH *pbvh, PBVHNode *node);
void BKE_pbvh_gather_proxies(PBVH *pbvh, PBVHNode ***r_array, int *r_tot);

bool BKE_pbvh_node_vert_update_check_any(PBVH *pbvh, PBVHNode *node);

// void BKE_pbvh_node_BB_reset(PBVHNode *node);
// void BKE_pbvh_node_BB_expand(PBVHNode *node, float co[3]);

bool BKE_pbvh_draw_mask(const PBVH *pbvh);
void pbvh_show_mask_set(PBVH *pbvh, bool show_mask);

bool BKE_pbvh_draw_face_sets(PBVH *pbvh);
void pbvh_show_face_sets_set(PBVH *pbvh, bool show_face_sets);

/* Parallelization */
void BKE_pbvh_parallel_range_settings(struct TaskParallelSettings *settings,
                                      bool use_threading,
                                      int totnode);

struct MVert *BKE_pbvh_get_verts(const PBVH *pbvh);

PBVHColorBufferNode *BKE_pbvh_node_color_buffer_get(PBVHNode *node);
void BKE_pbvh_node_color_buffer_free(PBVH *pbvh);

int BKE_pbvh_get_node_index(PBVH *pbvh, PBVHNode *node);
int BKE_pbvh_get_node_id(PBVH *pbvh, PBVHNode *node);
void BKE_pbvh_set_flat_vcol_shading(PBVH *pbvh, bool value);

#define DYNTOPO_CD_INTERP

void SCULPT_update_flat_vcol_shading(struct Object *ob, struct Scene *scene);

void BKE_pbvh_curvature_update_set(PBVHNode *node, bool state);
bool BKE_pbvh_curvature_update_get(PBVHNode *node);

int BKE_pbvh_get_totnodes(PBVH *pbvh);

bool BKE_pbvh_bmesh_check_tris(PBVH *pbvh, PBVHNode *node);
PBVHTriBuf *BKE_pbvh_bmesh_get_tris(PBVH *pbvh, PBVHNode *node);
void BKE_pbvh_bmesh_free_tris(PBVH *pbvh, PBVHNode *node);

/*recalculates boundary flags for *all* vertices.  used by
  symmetrize.*/
void BKE_pbvh_recalc_bmesh_boundary(PBVH *pbvh);

/* saves all bmesh references to internal indices, to be restored later */
void BKE_pbvh_bmesh_save_indices(PBVH *pbvh);

/* restore bmesh references from previously indices saved by BKE_pbvh_bmesh_save_indices */
void BKE_pbvh_bmesh_from_saved_indices(PBVH *pbvh);

/* wraps calls to BM_mesh_toolflags_set in BKE_pbvh_bmesh_save_indices and
 * BKE_pbvh_bmesh_from_saved_indices */
void BKE_pbvh_bmesh_set_toolflags(PBVH *pbvh, bool use_toolflags);

void BKE_pbvh_bmesh_remove_face(PBVH *pbvh, struct BMFace *f, bool log_face);
void BKE_pbvh_bmesh_remove_edge(PBVH *pbvh, struct BMEdge *e, bool log_vert);
void BKE_pbvh_bmesh_remove_vertex(PBVH *pbvh, struct BMVert *v, bool log_vert);

void BKE_pbvh_bmesh_add_face(PBVH *pbvh, struct BMFace *f, bool log_face, bool force_tree_walk);

// note that e_tri and f_example are allowed to be NULL
struct BMFace *BKE_pbvh_face_create_bmesh(PBVH *pbvh,
                                          struct BMVert *v_tri[3],
                                          struct BMEdge *e_tri[3],
                                          const struct BMFace *f_example);

// if node is NULL, one will be foudn in the pbvh, which potentially can be slow
struct BMVert *BKE_pbvh_vert_create_bmesh(
    PBVH *pbvh, float co[3], float no[3], PBVHNode *node, struct BMVert *v_example);
PBVHNode *BKE_pbvh_node_from_face_bmesh(PBVH *pbvh, struct BMFace *f);
PBVHNode *BKE_pbvh_node_from_index(PBVH *pbvh, int node_i);

struct BMesh *BKE_pbvh_reorder_bmesh(PBVH *pbvh);
void BKE_pbvh_update_vert_boundary(int cd_dyn_vert,
                                   int cd_faceset_offset,
                                   int cd_vert_node_offset,
                                   int cd_face_node_offset,
                                   int cd_vcol,
                                   struct BMVert *v,
                                   int symmetry);

#define DYNTOPO_DYNAMIC_TESS

PBVHNode *BKE_pbvh_get_node_leaf_safe(PBVH *pbvh, int i);

void BKE_pbvh_get_vert_face_areas(PBVH *pbvh, SculptVertRef vertex, float *r_areas, int valence);
void BKE_pbvh_set_symmetry(PBVH *pbvh, int symmetry, int boundary_symmetry);

#if 0
typedef enum {
  SCULPT_TEXTURE_UV = 1 << 0,  // per-uv
  // SCULPT_TEXTURE_PTEX?
} SculptTextureType;

typedef int TexLayerRef;

/*
Texture points are texels projected into 3d.
*/
typedef intptr_t TexPointRef;

void *BKE_pbvh_get_tex_settings(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm);
void *BKE_pbvh_get_tex_data(PBVH *pbvh, PBVHNode *node, TexPointRef vdm);

typedef struct SculptTextureDef {
  SculptTextureType type;
  int settings_size;

  void (*build_begin)(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm);

  /*vdms can cache data per node, which is freed to maintain memory limit.
    they store cache in the same structure they return in buildNodeData.*/
  void (*freeCachedData)(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm);
  void (*ensuredCachedData)(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm);

  /*builds all data that isn't cached.*/
  void *(*buildNodeData)(PBVH *pbvh, PBVHNode *node);
  bool (*validate)(PBVH *pbvh, TexLayerRef vdm);

  void (*getPointsFromNode)(PBVH *pbvh,
                            PBVHNode *node,
                            TexLayerRef vdm,
                            TexPointRef **r_ids,
                            float ***r_cos,
                            float ***r_nos,
                            int *r_totpoint);
  void (*releaseNodePoints)(
      PBVH *pbvh, PBVHNode *node, TexLayerRef vdm, TexPointRef *ids, float **cos, float **nos);

#  if 0
  int (*getTrisFromNode)(PBVH *pbvh,
                         PBVHNode *node,
                         TexLayerRef vdm,
                         TexPointRef *((*r_tris)[3]),
                         TexPointRef **r_ids,
                         int tottri,
                         int totid);
  void (*getTriInterpWeightsFromNode)(PBVH *pbvh,
                                      PBVHNode *node,
                                      TexLayerRef vdm,
                                      float *((*r_tris)[3]),
                                      SculptLoopRef ***r_src_loops,
                                      int tottri,
                                      int totloop);
  int (*getTriCount)(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm);
#  endif

  void (*getPointNeighbors)(PBVH *pbvh,
                            PBVHNode *node,
                            TexLayerRef vdm,
                            TexPointRef id,
                            TexPointRef **r_neighbor_ids,
                            int *r_totneighbor,
                            int maxneighbors,
                            TexPointRef **r_duplicates_id,
                            int r_totduplicate,
                            int maxduplicates);
  void (*getPointValence)(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm, TexPointRef id);
  void (*freeNodeData)(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm, void *settings);

  void (*getPointsFromIds)(
      PBVH *pbvh, PBVHNode *node, TexLayerRef vdm, TexPointRef *ids, int totid);

  /*displacement texture stuff*/
  // can be tangent, object space displacement, whatever
  void (*worldToDelta)(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm, TexPointRef *ids, int totid);
  void (*deltaToWorld)(PBVH *pbvh, PBVHNode *node, TexLayerRef vdm, TexPointRef *ids, int totid);
} SculptDisplacementDef;

typedef struct SculptLayerEntry {
  char name[64];
  int type;
  void *settings;
  float factor;
  struct SculptLayerEntry *parent;
} SculptLayerEntry;

#endif

int BKE_pbvh_do_fset_symmetry(int fset, const int symflag, const float *co);
bool BKE_pbvh_check_vert_boundary(PBVH *pbvh, struct BMVert *v);

void BKE_pbvh_update_vert_boundary_faces(int *face_sets,
                                         struct MVert *mvert,
                                         struct MEdge *medge,
                                         struct MLoop *mloop,
                                         struct MPoly *mpoly,
                                         struct MDynTopoVert *mdyntopo_verts,
                                         struct MeshElemMap *pmap,
                                         SculptVertRef vertex);
void BKE_pbvh_update_vert_boundary_grids(PBVH *pbvh,
                                         struct SubdivCCG *subdiv_ccg,
                                         SculptVertRef vertex);

void BKE_pbvh_set_mdyntopo_verts(PBVH *pbvh, struct MDynTopoVert *mdyntopoverts);

#if 0
#  include "DNA_meshdata_types.h"
ATTR_NO_OPT static void MV_ADD_FLAG(MDynTopoVert *mv, int flag)
{
  if (flag & DYNVERT_NEED_BOUNDARY) {
    flag |= flag;
  }

  mv->flag |= flag;
}
#else
#  define MV_ADD_FLAG(mv, flag1) (mv)->flag |= (flag1)
#endif
#ifdef __cplusplus
}
#endif
