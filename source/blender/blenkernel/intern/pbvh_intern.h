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

#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "DNA_customdata_types.h"
#include "DNA_material_types.h"

#include "bmesh.h"

/** \file
 * \ingroup bli
 */
struct MDynTopoVert;

/* Axis-aligned bounding box */
typedef struct {
  float bmin[3], bmax[3];
} BB;

/* Axis-aligned bounding box with centroid */
typedef struct {
  float bmin[3], bmax[3], bcentroid[3];
} BBC;

/* NOTE: this structure is getting large, might want to split it into
 * union'd structs */
struct PBVHNode {
  /* Opaque handle for drawing code */
  struct GPU_PBVH_Buffers *draw_buffers;
  struct GPU_PBVH_Buffers **mat_draw_buffers;  // currently only used by pbvh_bmesh
  int tot_mat_draw_buffers;

  int id;

  /* Voxel bounds */
  BB vb;
  BB orig_vb;

  /* For internal nodes, the offset of the children in the PBVH
   * 'nodes' array. */
  int children_offset;
  int subtree_tottri;

  /* Pointer into the PBVH prim_indices array and the number of
   * primitives used by this leaf node.
   *
   * Used for leaf nodes in both mesh- and multires-based PBVHs.
   */
  int *prim_indices;
  unsigned int totprim;

  /* Array of indices into the mesh's MVert array. Contains the
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
  const int *vert_indices;
  unsigned int uniq_verts, face_verts;

  /* An array mapping face corners into the vert_indices
   * array. The array is sized to match 'totprim', and each of
   * the face's corners gets an index into the vert_indices
   * array, in the same order as the corners in the original
   * MLoopTri.
   *
   * Used for leaf nodes in a mesh-based PBVH (not multires.)
   */
  const int (*face_vert_indices)[3];

  /* Indicates whether this node is a leaf or not; also used for
   * marking various updates that need to be applied. */
  PBVHNodeFlags flag : 32;

  /* Used for raycasting: how close bb is to the ray point. */
  float tmin;

  /* Scalar displacements for sculpt mode's layer brush. */
  float *layer_disp;

  int proxy_count;
  PBVHProxyNode *proxies;

  /* Dyntopo */
  TableGSet *bm_faces;
  TableGSet *bm_unique_verts;
  TableGSet *bm_other_verts;

  PBVHTriBuf *tribuf;       // all triangles
  PBVHTriBuf *tri_buffers;  // tribuffers, one per material used
  int tot_tri_buffers;

  int updategen;

  /* Used to store the brush color during a stroke and composite it over the original color */
  PBVHColorBufferNode color_buffer;
#ifdef PROXY_ADVANCED
  ProxyVertArray proxyverts;
#endif
};

typedef enum {
  PBVH_DYNTOPO_SMOOTH_SHADING = 1,
  PBVH_FAST_DRAW = 2  // hides facesets/masks and forces smooth to save GPU bandwidth
} PBVHFlags;

typedef struct PBVHBMeshLog PBVHBMeshLog;
struct DMFlagMat;

struct PBVH {
  PBVHType type;
  PBVHFlags flags;

  int idgen;

  bool dyntopo_stop;

  PBVHNode *nodes;
  int node_mem_count, totnode;

  int *prim_indices;
  int totprim;
  int totvert;

  int leaf_limit;
  int depth_limit;

  /* Mesh data */
  const struct Mesh *mesh;
  MVert *verts;
  const MPoly *mpoly;
  const MLoop *mloop;
  const MLoopTri *looptri;
  CustomData *vdata;
  CustomData *ldata;
  CustomData *pdata;

  int face_sets_color_seed;
  int face_sets_color_default;
  int *face_sets;

  /* Grid Data */
  CCGKey gridkey;
  CCGElem **grids;
  void **gridfaces;
  const struct DMFlagMat *grid_flag_mats;
  int totgrid;
  BLI_bitmap **grid_hidden;

  /* Only used during BVH build and update,
   * don't need to remain valid after */
  BLI_bitmap *vert_bitmap;

#ifdef PERFCNTRS
  int perf_modified;
#endif

  /* flag are verts/faces deformed */
  bool deformed;
  bool show_mask;
  bool show_face_sets;
  bool respect_hide;

  /* Dynamic topology */
  BMesh *bm;
  float bm_max_edge_len;
  float bm_min_edge_len;
  float bm_detail_range;

  int cd_dyn_vert;
  int cd_vert_node_offset;
  int cd_face_node_offset;
  int cd_vert_mask_offset;
  int cd_vcol_offset;
  int cd_faceset_offset;
  int cd_face_area;

  float planes[6][4];
  int num_planes;

  int symmetry;
  int boundary_symmetry;

  struct BMLog *bm_log;
  struct SubdivCCG *subdiv_ccg;

  bool flat_vcol_shading;
  bool need_full_render;  // used by pbvh drawing for PBVH_BMESH

  int balance_counter;
  int stroke_id;  // used to keep origdata up to date in PBVH_BMESH
  struct MDynTopoVert *mdyntopo_verts;
};

/* pbvh.c */
void BB_reset(BB *bb);
void BB_expand(BB *bb, const float co[3]);
void BB_expand_with_bb(BB *bb, BB *bb2);
void BBC_update_centroid(BBC *bbc);
int BB_widest_axis(const BB *bb);
void BB_intersect(BB *r_out, BB *a, BB *b);
float BB_volume(const BB *bb);

void pbvh_grow_nodes(PBVH *bvh, int totnode);
bool ray_face_intersection_quad(const float ray_start[3],
                                struct IsectRayPrecalc *isect_precalc,
                                const float t0[3],
                                const float t1[3],
                                const float t2[3],
                                const float t3[3],
                                float *depth);
bool ray_face_intersection_tri(const float ray_start[3],
                               struct IsectRayPrecalc *isect_precalc,
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

/* pbvh_bmesh.c */
bool pbvh_bmesh_node_raycast(PBVH *pbvh,
                             PBVHNode *node,
                             const float ray_start[3],
                             const float ray_normal[3],
                             struct IsectRayPrecalc *isect_precalc,
                             int *hit_count,
                             float *depth,
                             float *back_depth,
                             bool use_original,
                             SculptVertRef *r_active_vertex_index,
                             SculptFaceRef *r_active_face_index,
                             float *r_face_normal,
                             int stroke_id);

bool pbvh_bmesh_node_nearest_to_ray(PBVH *pbvh,
                                    PBVHNode *node,
                                    const float ray_start[3],
                                    const float ray_normal[3],
                                    float *depth,
                                    float *dist_sq,
                                    bool use_original,
                                    int stroke_id);

void pbvh_bmesh_normals_update(struct BMesh *bm, PBVHNode **nodes, int totnode);

void pbvh_free_all_draw_buffers(PBVHNode *node);
void pbvh_update_free_all_draw_buffers(PBVH *pbvh, PBVHNode *node);

BLI_INLINE int pbvh_bmesh_node_index_from_vert(PBVH *pbvh, const BMVert *key)
{
  const int node_index = BM_ELEM_CD_GET_INT((const BMElem *)key, pbvh->cd_vert_node_offset);
  BLI_assert(node_index != DYNTOPO_NODE_NONE);
  BLI_assert(node_index < pbvh->totnode);
  return node_index;
}

BLI_INLINE int pbvh_bmesh_node_index_from_face(PBVH *pbvh, const BMFace *key)
{
  const int node_index = BM_ELEM_CD_GET_INT((const BMElem *)key, pbvh->cd_face_node_offset);
  BLI_assert(node_index != DYNTOPO_NODE_NONE);
  BLI_assert(node_index < pbvh->totnode);
  return node_index;
}

BLI_INLINE PBVHNode *pbvh_bmesh_node_from_vert(PBVH *pbvh, const BMVert *key)
{
  int ni = pbvh_bmesh_node_index_from_vert(pbvh, key);

  return ni >= 0 ? pbvh->nodes + ni : NULL;
  // return &pbvh->nodes[pbvh_bmesh_node_index_from_vert(pbvh, key)];
}

BLI_INLINE PBVHNode *pbvh_bmesh_node_from_face(PBVH *pbvh, const BMFace *key)
{
  int ni = pbvh_bmesh_node_index_from_face(pbvh, key);

  return ni >= 0 ? pbvh->nodes + ni : NULL;
  // return &pbvh->nodes[pbvh_bmesh_node_index_from_face(pbvh, key)];
}
bool pbvh_bmesh_node_limit_ensure(PBVH *pbvh, int node_index);
void pbvh_bmesh_check_nodes(PBVH *pbvh);
void bke_pbvh_insert_face_finalize(PBVH *pbvh, BMFace *f, const int ni);
void bke_pbvh_insert_face(PBVH *pbvh, struct BMFace *f);
void bke_pbvh_update_vert_boundary(int cd_dyn_vert,
                                   int cd_faceset_offset,
                                   int cd_vert_node_offset,
                                   int cd_face_node_offset,
                                   BMVert *v,
                                   int bound_symmetry);

BLI_INLINE bool pbvh_check_vert_boundary(PBVH *pbvh, struct BMVert *v)
{
  MDynTopoVert *mv = (MDynTopoVert *)BM_ELEM_CD_GET_VOID_P(v, pbvh->cd_dyn_vert);

  if (mv->flag & DYNVERT_NEED_BOUNDARY) {
    bke_pbvh_update_vert_boundary(pbvh->cd_dyn_vert,
                                  pbvh->cd_faceset_offset,
                                  pbvh->cd_vert_node_offset,
                                  pbvh->cd_face_node_offset,
                                  v,
                                  pbvh->boundary_symmetry);
    return true;
  }

  return false;
}

void pbvh_bmesh_check_other_verts(PBVHNode *node);

//#define DEFRAGMENT_MEMORY
