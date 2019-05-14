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

#ifndef __BKE_PBVH_H__
#define __BKE_PBVH_H__

/** \file
 * \ingroup bke
 * \brief A BVH for high poly meshes.
 */

#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

struct BMLog;
struct BMesh;
struct CCGDerivedMesh;
struct CCGElem;
struct CCGKey;
struct CustomData;
struct DMFlagMat;
struct GPUBatch;
struct GPU_PBVH_Buffers;
struct MLoop;
struct MLoopTri;
struct MPoly;
struct MVert;
struct PBVH;
struct PBVHNode;

typedef struct PBVH PBVH;
typedef struct PBVHNode PBVHNode;

typedef struct {
  float (*co)[3];
} PBVHProxyNode;

typedef enum {
  PBVH_Leaf = 1,

  PBVH_UpdateNormals = 2,
  PBVH_UpdateBB = 4,
  PBVH_UpdateOriginalBB = 8,
  PBVH_UpdateDrawBuffers = 16,
  PBVH_UpdateRedraw = 32,

  PBVH_RebuildDrawBuffers = 64,
  PBVH_FullyHidden = 128,

  PBVH_UpdateTopology = 256,
} PBVHNodeFlags;

/* Callbacks */

/* returns 1 if the search should continue from this node, 0 otherwise */
typedef bool (*BKE_pbvh_SearchCallback)(PBVHNode *node, void *data);

typedef void (*BKE_pbvh_HitCallback)(PBVHNode *node, void *data);
typedef void (*BKE_pbvh_HitOccludedCallback)(PBVHNode *node, void *data, float *tmin);

typedef void (*BKE_pbvh_SearchNearestCallback)(PBVHNode *node, void *data, float *tmin);

/* Building */

PBVH *BKE_pbvh_new(void);
void BKE_pbvh_build_mesh(PBVH *bvh,
                         const struct MPoly *mpoly,
                         const struct MLoop *mloop,
                         struct MVert *verts,
                         int totvert,
                         struct CustomData *vdata,
                         struct CustomData *ldata,
                         const struct MLoopTri *looptri,
                         int looptri_num);
void BKE_pbvh_build_grids(PBVH *bvh,
                          struct CCGElem **grid_elems,
                          int totgrid,
                          struct CCGKey *key,
                          void **gridfaces,
                          struct DMFlagMat *flagmats,
                          unsigned int **grid_hidden);
void BKE_pbvh_build_bmesh(PBVH *bvh,
                          struct BMesh *bm,
                          bool smooth_shading,
                          struct BMLog *log,
                          const int cd_vert_node_offset,
                          const int cd_face_node_offset);
void BKE_pbvh_free(PBVH *bvh);
void BKE_pbvh_free_layer_disp(PBVH *bvh);

/* Hierarchical Search in the BVH, two methods:
 * - for each hit calling a callback
 * - gather nodes in an array (easy to multithread) */

void BKE_pbvh_search_callback(PBVH *bvh,
                              BKE_pbvh_SearchCallback scb,
                              void *search_data,
                              BKE_pbvh_HitCallback hcb,
                              void *hit_data);

void BKE_pbvh_search_gather(
    PBVH *bvh, BKE_pbvh_SearchCallback scb, void *search_data, PBVHNode ***array, int *tot);

/* Raycast
 * the hit callback is called for all leaf nodes intersecting the ray;
 * it's up to the callback to find the primitive within the leaves that is
 * hit first */

void BKE_pbvh_raycast(PBVH *bvh,
                      BKE_pbvh_HitOccludedCallback cb,
                      void *data,
                      const float ray_start[3],
                      const float ray_normal[3],
                      bool original);

bool BKE_pbvh_node_raycast(PBVH *bvh,
                           PBVHNode *node,
                           float (*origco)[3],
                           bool use_origco,
                           const float ray_start[3],
                           const float ray_normal[3],
                           float *depth);

bool BKE_pbvh_bmesh_node_raycast_detail(PBVHNode *node,
                                        const float ray_start[3],
                                        const float ray_normal[3],
                                        float *depth,
                                        float *r_edge_length);

/* for orthographic cameras, project the far away ray segment points to the root node so
 * we can have better precision. */
void BKE_pbvh_raycast_project_ray_root(
    PBVH *bvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3]);

void BKE_pbvh_find_nearest_to_ray(PBVH *bvh,
                                  BKE_pbvh_HitOccludedCallback cb,
                                  void *data,
                                  const float ray_start[3],
                                  const float ray_normal[3],
                                  bool original);

bool BKE_pbvh_node_find_nearest_to_ray(PBVH *bvh,
                                       PBVHNode *node,
                                       float (*origco)[3],
                                       bool use_origco,
                                       const float ray_start[3],
                                       const float ray_normal[3],
                                       float *depth,
                                       float *dist_sq);

/* Drawing */

void BKE_pbvh_draw_cb(PBVH *bvh,
                      float (*planes)[4],
                      float (*fnors)[3],
                      bool show_vcol,
                      void (*draw_fn)(void *user_data, struct GPU_PBVH_Buffers *buffers),
                      void *user_data);

void BKE_pbvh_draw_debug_cb(
    PBVH *bvh,
    void (*draw_fn)(void *user_data, const float bmin[3], const float bmax[3], PBVHNodeFlags flag),
    void *user_data);

/* PBVH Access */
typedef enum {
  PBVH_FACES,
  PBVH_GRIDS,
  PBVH_BMESH,
} PBVHType;

PBVHType BKE_pbvh_type(const PBVH *bvh);
bool BKE_pbvh_has_faces(const PBVH *bvh);

/* Get the PBVH root's bounding box */
void BKE_pbvh_bounding_box(const PBVH *bvh, float min[3], float max[3]);

/* multires hidden data, only valid for type == PBVH_GRIDS */
unsigned int **BKE_pbvh_grid_hidden(const PBVH *bvh);

int BKE_pbvh_count_grid_quads(BLI_bitmap **grid_hidden,
                              int *grid_indices,
                              int totgrid,
                              int gridsize);

/* multires level, only valid for type == PBVH_GRIDS */
void BKE_pbvh_get_grid_key(const PBVH *pbvh, struct CCGKey *key);

struct CCGElem **BKE_pbvh_get_grids(const PBVH *pbvh, int *num_grids);

/* Only valid for type == PBVH_BMESH */
struct BMesh *BKE_pbvh_get_bmesh(PBVH *pbvh);
void BKE_pbvh_bmesh_detail_size_set(PBVH *pbvh, float detail_size);

typedef enum {
  PBVH_Subdivide = 1,
  PBVH_Collapse = 2,
} PBVHTopologyUpdateMode;
bool BKE_pbvh_bmesh_update_topology(PBVH *bvh,
                                    PBVHTopologyUpdateMode mode,
                                    const float center[3],
                                    const float view_normal[3],
                                    float radius,
                                    const bool use_frontface,
                                    const bool use_projected);

/* Node Access */

void BKE_pbvh_node_mark_update(PBVHNode *node);
void BKE_pbvh_node_mark_rebuild_draw(PBVHNode *node);
void BKE_pbvh_node_mark_redraw(PBVHNode *node);
void BKE_pbvh_node_mark_normals_update(PBVHNode *node);
void BKE_pbvh_node_mark_topology_update(PBVHNode *node);
void BKE_pbvh_node_fully_hidden_set(PBVHNode *node, int fully_hidden);

void BKE_pbvh_node_get_grids(PBVH *bvh,
                             PBVHNode *node,
                             int **grid_indices,
                             int *totgrid,
                             int *maxgrid,
                             int *gridsize,
                             struct CCGElem ***grid_elems);
void BKE_pbvh_node_num_verts(PBVH *bvh, PBVHNode *node, int *r_uniquevert, int *r_totvert);
void BKE_pbvh_node_get_verts(PBVH *bvh,
                             PBVHNode *node,
                             const int **r_vert_indices,
                             struct MVert **r_verts);

void BKE_pbvh_node_get_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);
void BKE_pbvh_node_get_original_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);

float BKE_pbvh_node_get_tmin(PBVHNode *node);

/* test if AABB is at least partially inside the planes' volume */
bool BKE_pbvh_node_planes_contain_AABB(PBVHNode *node, void *data);
/* test if AABB is at least partially outside the planes' volume */
bool BKE_pbvh_node_planes_exclude_AABB(PBVHNode *node, void *data);

struct GSet *BKE_pbvh_bmesh_node_unique_verts(PBVHNode *node);
struct GSet *BKE_pbvh_bmesh_node_other_verts(PBVHNode *node);
struct GSet *BKE_pbvh_bmesh_node_faces(PBVHNode *node);
void BKE_pbvh_bmesh_node_save_orig(PBVHNode *node);
void BKE_pbvh_bmesh_after_stroke(PBVH *bvh);

/* Update Normals/Bounding Box/Redraw and clear flags */

void BKE_pbvh_update(PBVH *bvh, int flags, float (*face_nors)[3]);
void BKE_pbvh_redraw_BB(PBVH *bvh, float bb_min[3], float bb_max[3]);
void BKE_pbvh_get_grid_updates(PBVH *bvh, bool clear, void ***r_gridfaces, int *r_totface);
void BKE_pbvh_grids_update(PBVH *bvh,
                           struct CCGElem **grid_elems,
                           void **gridfaces,
                           struct DMFlagMat *flagmats,
                           unsigned int **grid_hidden);

/* Layer displacement */

/* Get the node's displacement layer, creating it if necessary */
float *BKE_pbvh_node_layer_disp_get(PBVH *pbvh, PBVHNode *node);

/* If the node has a displacement layer, free it and set to null */
void BKE_pbvh_node_layer_disp_free(PBVHNode *node);

/* vertex deformer */
float (*BKE_pbvh_get_vertCos(struct PBVH *pbvh))[3];
void BKE_pbvh_apply_vertCos(struct PBVH *pbvh, float (*vertCos)[3], const int totvert);
bool BKE_pbvh_isDeformed(struct PBVH *pbvh);

/* Vertex Iterator */

/* this iterator has quite a lot of code, but it's designed to:
 * - allow the compiler to eliminate dead code and variables
 * - spend most of the time in the relatively simple inner loop */

/* note: PBVH_ITER_ALL does not skip hidden vertices,
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

  /* grid */
  struct CCGElem **grids;
  struct CCGElem *grid;
  struct CCGKey *key;
  BLI_bitmap **grid_hidden, *gh;
  int *grid_indices;
  int totgrid;
  int gridsize;

  /* mesh */
  struct MVert *mverts;
  int totvert;
  const int *vert_indices;
  float *vmask;

  /* bmesh */
  struct GSetIterator bm_unique_verts;
  struct GSetIterator bm_other_verts;
  struct CustomData *bm_vdata;
  int cd_vert_mask_offset;

  /* result: these are all computed in the macro, but we assume
   * that compiler optimization's will skip the ones we don't use */
  struct MVert *mvert;
  struct BMVert *bm_vert;
  float *co;
  short *no;
  float *fno;
  float *mask;
} PBVHVertexIter;

void pbvh_vertex_iter_init(PBVH *bvh, PBVHNode *node, PBVHVertexIter *vi, int mode);

#define BKE_pbvh_vertex_iter_begin(bvh, node, vi, mode) \
  pbvh_vertex_iter_init(bvh, node, &vi, mode); \
\
  for (vi.i = 0, vi.g = 0; vi.g < vi.totgrid; vi.g++) { \
    if (vi.grids) { \
      vi.width = vi.gridsize; \
      vi.height = vi.gridsize; \
      vi.grid = vi.grids[vi.grid_indices[vi.g]]; \
      if (mode == PBVH_ITER_UNIQUE) \
        vi.gh = vi.grid_hidden[vi.grid_indices[vi.g]]; \
    } \
    else { \
      vi.width = vi.totvert; \
      vi.height = 1; \
    } \
\
    for (vi.gy = 0; vi.gy < vi.height; vi.gy++) { \
      for (vi.gx = 0; vi.gx < vi.width; vi.gx++, vi.i++) { \
        if (vi.grid) { \
          vi.co = CCG_elem_co(vi.key, vi.grid); \
          vi.fno = CCG_elem_no(vi.key, vi.grid); \
          vi.mask = vi.key->has_mask ? CCG_elem_mask(vi.key, vi.grid) : NULL; \
          vi.grid = CCG_elem_next(vi.key, vi.grid); \
          if (vi.gh) { \
            if (BLI_BITMAP_TEST(vi.gh, vi.gy * vi.gridsize + vi.gx)) \
              continue; \
          } \
        } \
        else if (vi.mverts) { \
          vi.mvert = &vi.mverts[vi.vert_indices[vi.gx]]; \
          if (mode == PBVH_ITER_UNIQUE && vi.mvert->flag & ME_HIDE) \
            continue; \
          vi.co = vi.mvert->co; \
          vi.no = vi.mvert->no; \
          if (vi.vmask) \
            vi.mask = &vi.vmask[vi.vert_indices[vi.gx]]; \
        } \
        else { \
          if (!BLI_gsetIterator_done(&vi.bm_unique_verts)) { \
            vi.bm_vert = BLI_gsetIterator_getKey(&vi.bm_unique_verts); \
            BLI_gsetIterator_step(&vi.bm_unique_verts); \
          } \
          else { \
            vi.bm_vert = BLI_gsetIterator_getKey(&vi.bm_other_verts); \
            BLI_gsetIterator_step(&vi.bm_other_verts); \
          } \
          if (mode == PBVH_ITER_UNIQUE && BM_elem_flag_test(vi.bm_vert, BM_ELEM_HIDDEN)) \
            continue; \
          vi.co = vi.bm_vert->co; \
          vi.fno = vi.bm_vert->no; \
          vi.mask = BM_ELEM_CD_GET_VOID_P(vi.bm_vert, vi.cd_vert_mask_offset); \
        }

#define BKE_pbvh_vertex_iter_end \
  } \
  } \
  } \
  ((void)0)

void BKE_pbvh_node_get_proxies(PBVHNode *node, PBVHProxyNode **proxies, int *proxy_count);
void BKE_pbvh_node_free_proxies(PBVHNode *node);
PBVHProxyNode *BKE_pbvh_node_add_proxy(PBVH *bvh, PBVHNode *node);
void BKE_pbvh_gather_proxies(PBVH *pbvh, PBVHNode ***nodes, int *totnode);
void BKE_pbvh_node_get_bm_orco_data(PBVHNode *node,
                                    int (**r_orco_tris)[3],
                                    int *r_orco_tris_num,
                                    float (**r_orco_coords)[3]);

bool BKE_pbvh_node_vert_update_check_any(PBVH *bvh, PBVHNode *node);

// void BKE_pbvh_node_BB_reset(PBVHNode *node);
// void BKE_pbvh_node_BB_expand(PBVHNode *node, float co[3]);

bool pbvh_has_mask(PBVH *bvh);
void pbvh_show_diffuse_color_set(PBVH *bvh, bool show_diffuse_color);
void pbvh_show_mask_set(PBVH *bvh, bool show_mask);

#endif /* __BKE_PBVH_H__ */
