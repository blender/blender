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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
#ifndef __BKE_MESH_H__
#define __BKE_MESH_H__

/** \file
 * \ingroup bke
 */

/* defines BLI_INLINE */
#include "BLI_compiler_compat.h"

struct BLI_Stack;
struct BMEditMesh;
struct BMesh;
struct BMeshCreateParams;
struct BMeshFromMeshParams;
struct BMeshToMeshParams;
struct BoundBox;
struct CustomData;
struct CustomData_MeshMasks;
struct Depsgraph;
struct EdgeHash;
struct ID;
struct KeyBlock;
struct LinkNode;
struct ListBase;
struct MDeformVert;
struct MDisps;
struct MEdge;
struct MFace;
struct MLoop;
struct MLoopTri;
struct MLoopUV;
struct MPoly;
struct MVert;
struct Main;
struct MemArena;
struct Mesh;
struct ModifierData;
struct Object;
struct Scene;

#ifdef __cplusplus
extern "C" {
#endif

/* setting zero so we can catch bugs in OpenMP/BMesh */
#ifdef DEBUG
#  define BKE_MESH_OMP_LIMIT 0
#else
#  define BKE_MESH_OMP_LIMIT 10000
#endif

/* *** mesh.c *** */

struct BMesh *BKE_mesh_to_bmesh_ex(const struct Mesh *me,
                                   const struct BMeshCreateParams *create_params,
                                   const struct BMeshFromMeshParams *convert_params);
struct BMesh *BKE_mesh_to_bmesh(struct Mesh *me,
                                struct Object *ob,
                                const bool add_key_index,
                                const struct BMeshCreateParams *params);

struct Mesh *BKE_mesh_from_bmesh_nomain(struct BMesh *bm,
                                        const struct BMeshToMeshParams *params,
                                        const struct Mesh *me_settings);
struct Mesh *BKE_mesh_from_bmesh_for_eval_nomain(struct BMesh *bm,
                                                 const struct CustomData_MeshMasks *cd_mask_extra,
                                                 const struct Mesh *me_settings);

int poly_find_loop_from_vert(const struct MPoly *poly, const struct MLoop *loopstart, uint vert);
int poly_get_adj_loops_from_vert(const struct MPoly *poly,
                                 const struct MLoop *mloop,
                                 unsigned int vert,
                                 unsigned int r_adj[2]);

int BKE_mesh_edge_other_vert(const struct MEdge *e, int v);
void BKE_mesh_looptri_get_real_edges(const struct Mesh *mesh,
                                     const struct MLoopTri *looptri,
                                     int r_edges[3]);

void BKE_mesh_free(struct Mesh *me);
void BKE_mesh_clear_geometry(struct Mesh *me);
struct Mesh *BKE_mesh_add(struct Main *bmain, const char *name);
struct Mesh *BKE_mesh_copy(struct Main *bmain, const struct Mesh *me);
void BKE_mesh_copy_settings(struct Mesh *me_dst, const struct Mesh *me_src);
void BKE_mesh_update_customdata_pointers(struct Mesh *me, const bool do_ensure_tess_cd);
void BKE_mesh_ensure_skin_customdata(struct Mesh *me);

struct Mesh *BKE_mesh_new_nomain(
    int verts_len, int edges_len, int tessface_len, int loops_len, int polys_len);
struct Mesh *BKE_mesh_new_nomain_from_template(const struct Mesh *me_src,
                                               int verts_len,
                                               int edges_len,
                                               int tessface_len,
                                               int loops_len,
                                               int polys_len);
struct Mesh *BKE_mesh_new_nomain_from_template_ex(const struct Mesh *me_src,
                                                  int verts_len,
                                                  int edges_len,
                                                  int tessface_len,
                                                  int loops_len,
                                                  int polys_len,
                                                  struct CustomData_MeshMasks mask);

void BKE_mesh_eval_delete(struct Mesh *me_eval);

/* Performs copy for use during evaluation,
 * optional referencing original arrays to reduce memory. */
struct Mesh *BKE_mesh_copy_for_eval(struct Mesh *source, bool reference);

/* These functions construct a new Mesh,
 * contrary to BKE_mesh_from_nurbs which modifies ob itself. */
struct Mesh *BKE_mesh_new_nomain_from_curve(struct Object *ob);
struct Mesh *BKE_mesh_new_nomain_from_curve_displist(struct Object *ob, struct ListBase *dispbase);

bool BKE_mesh_ensure_facemap_customdata(struct Mesh *me);
bool BKE_mesh_clear_facemap_customdata(struct Mesh *me);

float (*BKE_mesh_orco_verts_get(struct Object *ob))[3];
void BKE_mesh_orco_verts_transform(struct Mesh *me, float (*orco)[3], int totvert, int invert);
int test_index_face(struct MFace *mface, struct CustomData *mfdata, int mfindex, int nr);
struct Mesh *BKE_mesh_from_object(struct Object *ob);
void BKE_mesh_assign_object(struct Main *bmain, struct Object *ob, struct Mesh *me);
void BKE_mesh_from_metaball(struct ListBase *lb, struct Mesh *me);
int BKE_mesh_nurbs_to_mdata(struct Object *ob,
                            struct MVert **r_allvert,
                            int *r_totvert,
                            struct MEdge **r_alledge,
                            int *r_totedge,
                            struct MLoop **r_allloop,
                            struct MPoly **r_allpoly,
                            int *r_totloop,
                            int *r_totpoly);
int BKE_mesh_nurbs_displist_to_mdata(struct Object *ob,
                                     const struct ListBase *dispbase,
                                     struct MVert **r_allvert,
                                     int *r_totvert,
                                     struct MEdge **r_alledge,
                                     int *r_totedge,
                                     struct MLoop **r_allloop,
                                     struct MPoly **r_allpoly,
                                     struct MLoopUV **r_alluv,
                                     int *r_totloop,
                                     int *r_totpoly);
void BKE_mesh_from_nurbs_displist(struct Main *bmain,
                                  struct Object *ob,
                                  struct ListBase *dispbase,
                                  const char *obdata_name,
                                  bool temporary);
void BKE_mesh_from_nurbs(struct Main *bmain, struct Object *ob);
void BKE_mesh_to_curve_nurblist(const struct Mesh *me,
                                struct ListBase *nurblist,
                                const int edge_users_test);
void BKE_mesh_to_curve(struct Main *bmain,
                       struct Depsgraph *depsgraph,
                       struct Scene *scene,
                       struct Object *ob);
void BKE_mesh_material_index_remove(struct Mesh *me, short index);
bool BKE_mesh_material_index_used(struct Mesh *me, short index);
void BKE_mesh_material_index_clear(struct Mesh *me);
void BKE_mesh_material_remap(struct Mesh *me, const unsigned int *remap, unsigned int remap_len);
void BKE_mesh_smooth_flag_set(struct Mesh *me, const bool use_smooth);

const char *BKE_mesh_cmp(struct Mesh *me1, struct Mesh *me2, float thresh);

struct BoundBox *BKE_mesh_boundbox_get(struct Object *ob);

void BKE_mesh_texspace_calc(struct Mesh *me);
void BKE_mesh_texspace_ensure(struct Mesh *me);
void BKE_mesh_texspace_get(struct Mesh *me, float r_loc[3], float r_size[3]);
void BKE_mesh_texspace_get_reference(struct Mesh *me,
                                     short **r_texflag,
                                     float **r_loc,
                                     float **r_size);
void BKE_mesh_texspace_copy_from_object(struct Mesh *me, struct Object *ob);

void BKE_mesh_split_faces(struct Mesh *mesh, bool free_loop_normals);

/* Create new mesh from the given object at its current state.
 * The owner of this mesh is unknown, it is up to the caller to decide.
 *
 * If preserve_all_data_layers is truth then the modifier stack is re-evaluated to ensure it
 * preserves all possible custom data layers.
 *
 * NOTE: Dependency graph argument is required when preserve_all_data_layers is truth, and is
 * ignored otherwise. */
struct Mesh *BKE_mesh_new_from_object(struct Depsgraph *depsgraph,
                                      struct Object *object,
                                      bool preserve_all_data_layers);

/* This is a version of BKE_mesh_new_from_object() which stores mesh in the given main database.
 * However, that function enforces object type to be a geometry one, and ensures a mesh is always
 * generated, be it empty. */
struct Mesh *BKE_mesh_new_from_object_to_bmain(struct Main *bmain,
                                               struct Depsgraph *depsgraph,
                                               struct Object *object,
                                               bool preserve_all_data_layers);

struct Mesh *BKE_mesh_create_derived_for_modifier(struct Depsgraph *depsgraph,
                                                  struct Scene *scene,
                                                  struct Object *ob_eval,
                                                  struct ModifierData *md_eval,
                                                  int build_shapekey_layers);

/* Copies a nomain-Mesh into an existing Mesh. */
void BKE_mesh_nomain_to_mesh(struct Mesh *mesh_src,
                             struct Mesh *mesh_dst,
                             struct Object *ob,
                             const struct CustomData_MeshMasks *mask,
                             bool take_ownership);
void BKE_mesh_nomain_to_meshkey(struct Mesh *mesh_src, struct Mesh *mesh_dst, struct KeyBlock *kb);

/* vertex level transformations & checks (no derived mesh) */

bool BKE_mesh_minmax(const struct Mesh *me, float r_min[3], float r_max[3]);
void BKE_mesh_transform(struct Mesh *me, float mat[4][4], bool do_keys);
void BKE_mesh_translate(struct Mesh *me, const float offset[3], const bool do_keys);

void BKE_mesh_ensure_navmesh(struct Mesh *me);

void BKE_mesh_tessface_calc(struct Mesh *mesh);
void BKE_mesh_tessface_ensure(struct Mesh *mesh);
void BKE_mesh_tessface_clear(struct Mesh *mesh);

void BKE_mesh_do_versions_cd_flag_init(struct Mesh *mesh);

void BKE_mesh_mselect_clear(struct Mesh *me);
void BKE_mesh_mselect_validate(struct Mesh *me);
int BKE_mesh_mselect_find(struct Mesh *me, int index, int type);
int BKE_mesh_mselect_active_get(struct Mesh *me, int type);
void BKE_mesh_mselect_active_set(struct Mesh *me, int index, int type);

void BKE_mesh_count_selected_items(const struct Mesh *mesh, int r_count[3]);

float (*BKE_mesh_vert_coords_alloc(const struct Mesh *mesh, int *r_vert_len))[3];
void BKE_mesh_vert_coords_get(const struct Mesh *mesh, float (*vert_coords)[3]);

void BKE_mesh_vert_coords_apply_with_mat4(struct Mesh *mesh,
                                          const float (*vert_coords)[3],
                                          const float mat[4][4]);
void BKE_mesh_vert_coords_apply(struct Mesh *mesh, const float (*vert_coords)[3]);
void BKE_mesh_vert_normals_apply(struct Mesh *mesh, const short (*vertNormals)[3]);

/* *** mesh_evaluate.c *** */

void BKE_mesh_calc_normals_mapping_simple(struct Mesh *me);
void BKE_mesh_calc_normals_mapping(struct MVert *mverts,
                                   int numVerts,
                                   const struct MLoop *mloop,
                                   const struct MPoly *mpolys,
                                   int numLoops,
                                   int numPolys,
                                   float (*r_polyNors)[3],
                                   const struct MFace *mfaces,
                                   int numFaces,
                                   const int *origIndexFace,
                                   float (*r_faceNors)[3]);
void BKE_mesh_calc_normals_mapping_ex(struct MVert *mverts,
                                      int numVerts,
                                      const struct MLoop *mloop,
                                      const struct MPoly *mpolys,
                                      int numLoops,
                                      int numPolys,
                                      float (*r_polyNors)[3],
                                      const struct MFace *mfaces,
                                      int numFaces,
                                      const int *origIndexFace,
                                      float (*r_faceNors)[3],
                                      const bool only_face_normals);
void BKE_mesh_calc_normals_poly(struct MVert *mverts,
                                float (*r_vertnors)[3],
                                int numVerts,
                                const struct MLoop *mloop,
                                const struct MPoly *mpolys,
                                int numLoops,
                                int numPolys,
                                float (*r_polyNors)[3],
                                const bool only_face_normals);
void BKE_mesh_calc_normals(struct Mesh *me);
void BKE_mesh_ensure_normals(struct Mesh *me);
void BKE_mesh_ensure_normals_for_display(struct Mesh *mesh);
void BKE_mesh_calc_normals_looptri(struct MVert *mverts,
                                   int numVerts,
                                   const struct MLoop *mloop,
                                   const struct MLoopTri *looptri,
                                   int looptri_num,
                                   float (*r_tri_nors)[3]);
void BKE_mesh_loop_manifold_fan_around_vert_next(const struct MLoop *mloops,
                                                 const struct MPoly *mpolys,
                                                 const int *loop_to_poly,
                                                 const int *e2lfan_curr,
                                                 const uint mv_pivot_index,
                                                 const struct MLoop **r_mlfan_curr,
                                                 int *r_mlfan_curr_index,
                                                 int *r_mlfan_vert_index,
                                                 int *r_mpfan_curr_index);

void BKE_edges_sharp_from_angle_set(const struct MVert *mverts,
                                    const int numVerts,
                                    struct MEdge *medges,
                                    const int numEdges,
                                    struct MLoop *mloops,
                                    const int numLoops,
                                    struct MPoly *mpolys,
                                    const float (*polynors)[3],
                                    const int numPolys,
                                    const float split_angle);

/**
 * References a contiguous loop-fan with normal offset vars.
 */
typedef struct MLoopNorSpace {
  /** Automatically computed loop normal. */
  float vec_lnor[3];
  /** Reference vector, orthogonal to vec_lnor. */
  float vec_ref[3];
  /** Third vector, orthogonal to vec_lnor and vec_ref. */
  float vec_ortho[3];
  /** Reference angle, around vec_ortho, in ]0, pi] range (0.0 marks that space as invalid). */
  float ref_alpha;
  /** Reference angle, around vec_lnor, in ]0, 2pi] range (0.0 marks that space as invalid). */
  float ref_beta;
  /** All loops using this lnor space (i.e. smooth fan of loops),
   * as (depending on owning MLoopNorSpaceArrary.data_type):
   *     - Indices (uint_in_ptr), or
   *     - BMLoop pointers. */
  struct LinkNode *loops;
  char flags;

  /** To be used for extended processing related to loop normal spaces (aka smooth fans). */
  void *user_data;
} MLoopNorSpace;
/**
 * MLoopNorSpace.flags
 */
enum {
  MLNOR_SPACE_IS_SINGLE = 1 << 0,
};

/**
 * Collection of #MLoopNorSpace basic storage & pre-allocation.
 */
typedef struct MLoopNorSpaceArray {
  MLoopNorSpace **lspacearr; /* MLoop aligned array */
  struct LinkNode
      *loops_pool; /* Allocated once, avoids to call BLI_linklist_prepend_arena() for each loop! */
  char data_type;  /* Whether we store loop indices, or pointers to BMLoop. */
  int num_spaces;  /* Number of clnors spaces defined in this array. */
  struct MemArena *mem;
} MLoopNorSpaceArray;
/**
 * MLoopNorSpaceArray.data_type
 */
enum {
  MLNOR_SPACEARR_LOOP_INDEX = 0,
  MLNOR_SPACEARR_BMLOOP_PTR = 1,
};

/* Low-level custom normals functions. */
void BKE_lnor_spacearr_init(MLoopNorSpaceArray *lnors_spacearr,
                            const int numLoops,
                            const char data_type);
void BKE_lnor_spacearr_clear(MLoopNorSpaceArray *lnors_spacearr);
void BKE_lnor_spacearr_free(MLoopNorSpaceArray *lnors_spacearr);
MLoopNorSpace *BKE_lnor_space_create(MLoopNorSpaceArray *lnors_spacearr);
void BKE_lnor_space_define(MLoopNorSpace *lnor_space,
                           const float lnor[3],
                           float vec_ref[3],
                           float vec_other[3],
                           struct BLI_Stack *edge_vectors);
void BKE_lnor_space_add_loop(MLoopNorSpaceArray *lnors_spacearr,
                             MLoopNorSpace *lnor_space,
                             const int ml_index,
                             void *bm_loop,
                             const bool is_single);
void BKE_lnor_space_custom_data_to_normal(MLoopNorSpace *lnor_space,
                                          const short clnor_data[2],
                                          float r_custom_lnor[3]);
void BKE_lnor_space_custom_normal_to_data(MLoopNorSpace *lnor_space,
                                          const float custom_lnor[3],
                                          short r_clnor_data[2]);

/* Medium-level custom normals functions. */
void BKE_mesh_normals_loop_split(const struct MVert *mverts,
                                 const int numVerts,
                                 struct MEdge *medges,
                                 const int numEdges,
                                 struct MLoop *mloops,
                                 float (*r_loopnors)[3],
                                 const int numLoops,
                                 struct MPoly *mpolys,
                                 const float (*polynors)[3],
                                 const int numPolys,
                                 const bool use_split_normals,
                                 const float split_angle,
                                 MLoopNorSpaceArray *r_lnors_spacearr,
                                 short (*clnors_data)[2],
                                 int *r_loop_to_poly);

void BKE_mesh_normals_loop_custom_set(const struct MVert *mverts,
                                      const int numVerts,
                                      struct MEdge *medges,
                                      const int numEdges,
                                      struct MLoop *mloops,
                                      float (*r_custom_loopnors)[3],
                                      const int numLoops,
                                      struct MPoly *mpolys,
                                      const float (*polynors)[3],
                                      const int numPolys,
                                      short (*r_clnors_data)[2]);
void BKE_mesh_normals_loop_custom_from_vertices_set(const struct MVert *mverts,
                                                    float (*r_custom_vertnors)[3],
                                                    const int numVerts,
                                                    struct MEdge *medges,
                                                    const int numEdges,
                                                    struct MLoop *mloops,
                                                    const int numLoops,
                                                    struct MPoly *mpolys,
                                                    const float (*polynors)[3],
                                                    const int numPolys,
                                                    short (*r_clnors_data)[2]);

void BKE_mesh_normals_loop_to_vertex(const int numVerts,
                                     const struct MLoop *mloops,
                                     const int numLoops,
                                     const float (*clnors)[3],
                                     float (*r_vert_clnors)[3]);

/* High-level custom normals functions. */
bool BKE_mesh_has_custom_loop_normals(struct Mesh *me);

void BKE_mesh_calc_normals_split(struct Mesh *mesh);
void BKE_mesh_calc_normals_split_ex(struct Mesh *mesh,
                                    struct MLoopNorSpaceArray *r_lnors_spacearr);

void BKE_mesh_set_custom_normals(struct Mesh *mesh, float (*r_custom_loopnors)[3]);
void BKE_mesh_set_custom_normals_from_vertices(struct Mesh *mesh, float (*r_custom_vertnors)[3]);

void BKE_mesh_calc_poly_normal(const struct MPoly *mpoly,
                               const struct MLoop *loopstart,
                               const struct MVert *mvarray,
                               float r_no[3]);
void BKE_mesh_calc_poly_normal_coords(const struct MPoly *mpoly,
                                      const struct MLoop *loopstart,
                                      const float (*vertex_coords)[3],
                                      float r_no[3]);
void BKE_mesh_calc_poly_center(const struct MPoly *mpoly,
                               const struct MLoop *loopstart,
                               const struct MVert *mvarray,
                               float r_cent[3]);
float BKE_mesh_calc_poly_area(const struct MPoly *mpoly,
                              const struct MLoop *loopstart,
                              const struct MVert *mvarray);
float BKE_mesh_calc_area(const struct Mesh *me);
float BKE_mesh_calc_poly_uv_area(const struct MPoly *mpoly, const struct MLoopUV *uv_array);
void BKE_mesh_calc_poly_angles(const struct MPoly *mpoly,
                               const struct MLoop *loopstart,
                               const struct MVert *mvarray,
                               float angles[]);

void BKE_mesh_poly_edgehash_insert(struct EdgeHash *ehash,
                                   const struct MPoly *mp,
                                   const struct MLoop *mloop);
void BKE_mesh_poly_edgebitmap_insert(unsigned int *edge_bitmap,
                                     const struct MPoly *mp,
                                     const struct MLoop *mloop);

bool BKE_mesh_center_median(const struct Mesh *me, float r_cent[3]);
bool BKE_mesh_center_bounds(const struct Mesh *me, float r_cent[3]);
bool BKE_mesh_center_of_surface(const struct Mesh *me, float r_cent[3]);
bool BKE_mesh_center_of_volume(const struct Mesh *me, float r_cent[3]);

void BKE_mesh_calc_volume(const struct MVert *mverts,
                          const int mverts_num,
                          const struct MLoopTri *mlooptri,
                          const int looptri_num,
                          const struct MLoop *mloop,
                          float *r_volume,
                          float r_center[3]);

/* tessface */
void BKE_mesh_loops_to_mface_corners(struct CustomData *fdata,
                                     struct CustomData *ldata,
                                     struct CustomData *pdata,
                                     unsigned int lindex[4],
                                     int findex,
                                     const int polyindex,
                                     const int mf_len,
                                     const int numTex,
                                     const int numCol,
                                     const bool hasPCol,
                                     const bool hasOrigSpace,
                                     const bool hasLNor);
void BKE_mesh_loops_to_tessdata(struct CustomData *fdata,
                                struct CustomData *ldata,
                                struct MFace *mface,
                                int *polyindices,
                                unsigned int (*loopindices)[4],
                                const int num_faces);
void BKE_mesh_tangent_loops_to_tessdata(struct CustomData *fdata,
                                        struct CustomData *ldata,
                                        struct MFace *mface,
                                        int *polyindices,
                                        unsigned int (*loopindices)[4],
                                        const int num_faces,
                                        const char *layer_name);
int BKE_mesh_tessface_calc_ex(struct CustomData *fdata,
                              struct CustomData *ldata,
                              struct CustomData *pdata,
                              struct MVert *mvert,
                              int totface,
                              int totloop,
                              int totpoly,
                              const bool do_face_nor_copy);
void BKE_mesh_recalc_looptri(const struct MLoop *mloop,
                             const struct MPoly *mpoly,
                             const struct MVert *mvert,
                             int totloop,
                             int totpoly,
                             struct MLoopTri *mlooptri);
void BKE_mesh_convert_mfaces_to_mpolys(struct Mesh *mesh);
void BKE_mesh_do_versions_convert_mfaces_to_mpolys(struct Mesh *mesh);
void BKE_mesh_convert_mfaces_to_mpolys_ex(struct ID *id,
                                          struct CustomData *fdata,
                                          struct CustomData *ldata,
                                          struct CustomData *pdata,
                                          int totedge_i,
                                          int totface_i,
                                          int totloop_i,
                                          int totpoly_i,
                                          struct MEdge *medge,
                                          struct MFace *mface,
                                          int *r_totloop,
                                          int *r_totpoly,
                                          struct MLoop **r_mloop,
                                          struct MPoly **r_mpoly);

void BKE_mesh_mdisp_flip(struct MDisps *md, const bool use_loop_mdisp_flip);

void BKE_mesh_polygon_flip_ex(struct MPoly *mpoly,
                              struct MLoop *mloop,
                              struct CustomData *ldata,
                              float (*lnors)[3],
                              struct MDisps *mdisp,
                              const bool use_loop_mdisp_flip);
void BKE_mesh_polygon_flip(struct MPoly *mpoly, struct MLoop *mloop, struct CustomData *ldata);
void BKE_mesh_polygons_flip(struct MPoly *mpoly,
                            struct MLoop *mloop,
                            struct CustomData *ldata,
                            int totpoly);

/* merge verts  */
/* Enum for merge_mode of CDDM_merge_verts.
 * Refer to mesh.c for details. */
enum {
  MESH_MERGE_VERTS_DUMP_IF_MAPPED,
  MESH_MERGE_VERTS_DUMP_IF_EQUAL,
};
struct Mesh *BKE_mesh_merge_verts(struct Mesh *mesh,
                                  const int *vtargetmap,
                                  const int tot_vtargetmap,
                                  const int merge_mode);

/* flush flags */
void BKE_mesh_flush_hidden_from_verts_ex(const struct MVert *mvert,
                                         const struct MLoop *mloop,
                                         struct MEdge *medge,
                                         const int totedge,
                                         struct MPoly *mpoly,
                                         const int totpoly);
void BKE_mesh_flush_hidden_from_verts(struct Mesh *me);
void BKE_mesh_flush_hidden_from_polys_ex(struct MVert *mvert,
                                         const struct MLoop *mloop,
                                         struct MEdge *medge,
                                         const int totedge,
                                         const struct MPoly *mpoly,
                                         const int totpoly);
void BKE_mesh_flush_hidden_from_polys(struct Mesh *me);
void BKE_mesh_flush_select_from_polys_ex(struct MVert *mvert,
                                         const int totvert,
                                         const struct MLoop *mloop,
                                         struct MEdge *medge,
                                         const int totedge,
                                         const struct MPoly *mpoly,
                                         const int totpoly);
void BKE_mesh_flush_select_from_polys(struct Mesh *me);
void BKE_mesh_flush_select_from_verts_ex(const struct MVert *mvert,
                                         const int totvert,
                                         const struct MLoop *mloop,
                                         struct MEdge *medge,
                                         const int totedge,
                                         struct MPoly *mpoly,
                                         const int totpoly);
void BKE_mesh_flush_select_from_verts(struct Mesh *me);

/* spatial evaluation */
void BKE_mesh_calc_relative_deform(const struct MPoly *mpoly,
                                   const int totpoly,
                                   const struct MLoop *mloop,
                                   const int totvert,

                                   const float (*vert_cos_src)[3],
                                   const float (*vert_cos_dst)[3],

                                   const float (*vert_cos_org)[3],
                                   float (*vert_cos_new)[3]);

/* *** mesh_validate.c *** */

bool BKE_mesh_validate(struct Mesh *me, const bool do_verbose, const bool cddata_check_mask);
bool BKE_mesh_is_valid(struct Mesh *me);
bool BKE_mesh_validate_material_indices(struct Mesh *me);

bool BKE_mesh_validate_arrays(struct Mesh *me,
                              struct MVert *mverts,
                              unsigned int totvert,
                              struct MEdge *medges,
                              unsigned int totedge,
                              struct MFace *mfaces,
                              unsigned int totface,
                              struct MLoop *mloops,
                              unsigned int totloop,
                              struct MPoly *mpolys,
                              unsigned int totpoly,
                              struct MDeformVert *dverts, /* assume totvert length */
                              const bool do_verbose,
                              const bool do_fixes,
                              bool *r_change);

bool BKE_mesh_validate_all_customdata(struct CustomData *vdata,
                                      const uint totvert,
                                      struct CustomData *edata,
                                      const uint totedge,
                                      struct CustomData *ldata,
                                      const uint totloop,
                                      struct CustomData *pdata,
                                      const uint totpoly,
                                      const bool check_meshmask,
                                      const bool do_verbose,
                                      const bool do_fixes,
                                      bool *r_change);

void BKE_mesh_strip_loose_faces(struct Mesh *me);
void BKE_mesh_strip_loose_polysloops(struct Mesh *me);
void BKE_mesh_strip_loose_edges(struct Mesh *me);

void BKE_mesh_calc_edges_legacy(struct Mesh *me, const bool use_old);
void BKE_mesh_calc_edges_loose(struct Mesh *mesh);
void BKE_mesh_calc_edges(struct Mesh *mesh, bool update, const bool select);
void BKE_mesh_calc_edges_tessface(struct Mesh *mesh);

/* In DerivedMesh.c */
void BKE_mesh_wrapper_deferred_finalize(struct Mesh *me_eval,
                                        const CustomData_MeshMasks *final_datamask);

/* **** Depsgraph evaluation **** */

void BKE_mesh_eval_geometry(struct Depsgraph *depsgraph, struct Mesh *mesh);

/* Draw Cache */
enum {
  BKE_MESH_BATCH_DIRTY_ALL = 0,
  BKE_MESH_BATCH_DIRTY_SELECT,
  BKE_MESH_BATCH_DIRTY_SELECT_PAINT,
  BKE_MESH_BATCH_DIRTY_SHADING,
  BKE_MESH_BATCH_DIRTY_UVEDIT_ALL,
  BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT,
};
void BKE_mesh_batch_cache_dirty_tag(struct Mesh *me, int mode);
void BKE_mesh_batch_cache_free(struct Mesh *me);

extern void (*BKE_mesh_batch_cache_dirty_tag_cb)(struct Mesh *me, int mode);
extern void (*BKE_mesh_batch_cache_free_cb)(struct Mesh *me);

/* Inlines */

/* Instead of -1 that function uses ORIGINDEX_NONE as defined in BKE_customdata.h,
 * but I don't want to force every user of BKE_mesh.h to also include that file.
 * ~~ Sybren */
BLI_INLINE int BKE_mesh_origindex_mface_mpoly(const int *index_mf_to_mpoly,
                                              const int *index_mp_to_orig,
                                              const int i)
{
  const int j = index_mf_to_mpoly[i];
  return (j != -1) ? (index_mp_to_orig ? index_mp_to_orig[j] : j) : -1;
}

#ifdef __cplusplus
}
#endif

#endif /* __BKE_MESH_H__ */
