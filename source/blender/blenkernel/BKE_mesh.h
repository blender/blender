/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): (mar-2001 nzc)
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_MESH_H__
#define __BKE_MESH_H__

/** \file BKE_mesh.h
 *  \ingroup bke
 */

struct ID;
struct BoundBox;
struct EdgeHash;
struct ListBase;
struct LinkNode;
struct BLI_Stack;
struct MemArena;
struct BMesh;
struct Main;
struct Mesh;
struct MPoly;
struct MLoop;
struct MFace;
struct MEdge;
struct MVert;
struct MDeformVert;
struct Object;
struct CustomData;
struct DerivedMesh;
struct Scene;
struct MLoopUV;
struct ReportList;

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

struct BMesh *BKE_mesh_to_bmesh(struct Mesh *me, struct Object *ob);

int poly_find_loop_from_vert(const struct MPoly *poly,
                             const struct MLoop *loopstart,
                             unsigned vert);

int poly_get_adj_loops_from_vert(unsigned r_adj[3], const struct MPoly *poly,
                                 const struct MLoop *mloop, unsigned vert);

int BKE_mesh_edge_other_vert(const struct MEdge *e, int v);

void BKE_mesh_unlink(struct Mesh *me);
void BKE_mesh_free(struct Mesh *me, int unlink);
struct Mesh *BKE_mesh_add(struct Main *bmain, const char *name);
struct Mesh *BKE_mesh_copy_ex(struct Main *bmain, struct Mesh *me);
struct Mesh *BKE_mesh_copy(struct Mesh *me);
void BKE_mesh_update_customdata_pointers(struct Mesh *me, const bool do_ensure_tess_cd);
void BKE_mesh_ensure_skin_customdata(struct Mesh *me);

void BKE_mesh_make_local(struct Mesh *me);
void BKE_mesh_boundbox_calc(struct Mesh *me, float r_loc[3], float r_size[3]);
void BKE_mesh_texspace_calc(struct Mesh *me);
float (*BKE_mesh_orco_verts_get(struct Object *ob))[3];
void   BKE_mesh_orco_verts_transform(struct Mesh *me, float (*orco)[3], int totvert, int invert);
int test_index_face(struct MFace *mface, struct CustomData *mfdata, int mfindex, int nr);
struct Mesh *BKE_mesh_from_object(struct Object *ob);
void BKE_mesh_assign_object(struct Object *ob, struct Mesh *me);
void BKE_mesh_from_metaball(struct ListBase *lb, struct Mesh *me);
int  BKE_mesh_nurbs_to_mdata(struct Object *ob, struct MVert **allvert, int *totvert,
                             struct MEdge **alledge, int *totedge, struct MLoop **allloop, struct MPoly **allpoly,
                             int *totloop, int *totpoly);
int BKE_mesh_nurbs_displist_to_mdata(struct Object *ob, struct ListBase *dispbase, struct MVert **allvert, int *_totvert,
                                     struct MEdge **alledge, int *_totedge, struct MLoop **allloop, struct MPoly **allpoly,
                                     struct MLoopUV **alluv, int *_totloop, int *_totpoly);
void BKE_mesh_from_nurbs_displist(struct Object *ob, struct ListBase *dispbase, const bool use_orco_uv);
void BKE_mesh_from_nurbs(struct Object *ob);
void BKE_mesh_to_curve_nurblist(struct DerivedMesh *dm, struct ListBase *nurblist, const int edge_users_test);
void BKE_mesh_to_curve(struct Scene *scene, struct Object *ob);
void BKE_mesh_material_index_remove(struct Mesh *me, short index);
void BKE_mesh_material_index_clear(struct Mesh *me);
void BKE_mesh_smooth_flag_set(struct Object *meshOb, int enableSmooth);

const char *BKE_mesh_cmp(struct Mesh *me1, struct Mesh *me2, float thresh);

struct BoundBox *BKE_mesh_boundbox_get(struct Object *ob);
void BKE_mesh_texspace_get(struct Mesh *me, float r_loc[3], float r_rot[3], float r_size[3]);
void BKE_mesh_texspace_copy_from_object(struct Mesh *me, struct Object *ob);

bool BKE_mesh_uv_cdlayer_rename_index(struct Mesh *me, const int poly_index, const int loop_index, const int face_index,
                                      const char *new_name, const bool do_tessface);
bool BKE_mesh_uv_cdlayer_rename(struct Mesh *me, const char *old_name, const char *new_name, bool do_tessface);

float (*BKE_mesh_vertexCos_get(const struct Mesh *me, int *r_numVerts))[3];

void BKE_mesh_calc_normals_split(struct Mesh *mesh);
void BKE_mesh_split_faces(struct Mesh *mesh);

struct Mesh *BKE_mesh_new_from_object(struct Main *bmain, struct Scene *sce, struct Object *ob,
                                      int apply_modifiers, int settings, int calc_tessface, int calc_undeformed);

/* vertex level transformations & checks (no derived mesh) */

bool BKE_mesh_minmax(struct Mesh *me, float r_min[3], float r_max[3]);
void BKE_mesh_transform(struct Mesh *me, float mat[4][4], bool do_keys);
void BKE_mesh_translate(struct Mesh *me, const float offset[3], const bool do_keys);

void BKE_mesh_ensure_navmesh(struct Mesh *me);

void BKE_mesh_tessface_calc(struct Mesh *mesh);
void BKE_mesh_tessface_ensure(struct Mesh *mesh);
void BKE_mesh_tessface_clear(struct Mesh *mesh);

void BKE_mesh_do_versions_cd_flag_init(struct Mesh *mesh);


void BKE_mesh_mselect_clear(struct Mesh *me);
void BKE_mesh_mselect_validate(struct Mesh *me);
int  BKE_mesh_mselect_find(struct Mesh *me, int index, int type);
int  BKE_mesh_mselect_active_get(struct Mesh *me, int type);
void BKE_mesh_mselect_active_set(struct Mesh *me, int index, int type);



/* *** mesh_evaluate.c *** */

void BKE_mesh_calc_normals_mapping(
        struct MVert *mverts, int numVerts,
        struct MLoop *mloop, struct MPoly *mpolys, int numLoops, int numPolys, float (*r_polyNors)[3],
        struct MFace *mfaces, int numFaces, const int *origIndexFace, float (*r_faceNors)[3]);
void BKE_mesh_calc_normals_mapping_ex(
        struct MVert *mverts, int numVerts,
        struct MLoop *mloop, struct MPoly *mpolys, int numLoops, int numPolys, float (*r_polyNors)[3],
        struct MFace *mfaces, int numFaces, const int *origIndexFace, float (*r_faceNors)[3],
        const bool only_face_normals);
void BKE_mesh_calc_normals_poly(
        struct MVert *mverts, int numVerts,
        struct MLoop *mloop, struct MPoly *mpolys,
        int numLoops, int numPolys, float (*r_polyNors)[3],
        const bool only_face_normals);
void BKE_mesh_calc_normals(struct Mesh *me);
void BKE_mesh_calc_normals_tessface(
        struct MVert *mverts, int numVerts,
        struct MFace *mfaces, int numFaces,
        float (*r_faceNors)[3]);
void BKE_mesh_loop_tangents_ex(
        struct MVert *mverts, const int numVerts, struct MLoop *mloops, float (*r_looptangent)[4], float (*loopnors)[3],
        struct MLoopUV *loopuv, const int numLoops, struct MPoly *mpolys, const int numPolys,
        struct ReportList *reports);
void BKE_mesh_loop_tangents(
        struct Mesh *mesh, const char *uvmap, float (*r_looptangents)[4], struct ReportList *reports);

/**
 * References a contiguous loop-fan with normal offset vars.
 */
typedef struct MLoopNorSpace {
	float vec_lnor[3];      /* Automatically computed loop normal. */
	float vec_ref[3];       /* Reference vector, orthogonal to vec_lnor. */
	float vec_ortho[3];     /* Third vector, orthogonal to vec_lnor and vec_ref. */
	float ref_alpha;        /* Reference angle, around vec_ortho, in ]0, pi] range (0.0 marks that space as invalid). */
	float ref_beta;         /* Reference angle, around vec_lnor, in ]0, 2pi] range (0.0 marks that space as invalid). */
	struct LinkNode *loops; /* All indices (uint_in_ptr) of loops using this lnor space (i.e. smooth fan of loops). */
} MLoopNorSpace;
/**
 * Collection of #MLoopNorSpace basic storage & pre-allocation.
 */
typedef struct MLoopNorSpaceArray {
	MLoopNorSpace **lspacearr;    /* MLoop aligned array */
	struct LinkNode *loops_pool;  /* Allocated once, avoids to call BLI_linklist_prepend_arena() for each loop! */
	struct MemArena *mem;
} MLoopNorSpaceArray;
void BKE_lnor_spacearr_init(MLoopNorSpaceArray *lnors_spacearr, const int numLoops);
void BKE_lnor_spacearr_clear(MLoopNorSpaceArray *lnors_spacearr);
void BKE_lnor_spacearr_free(MLoopNorSpaceArray *lnors_spacearr);
MLoopNorSpace *BKE_lnor_space_create(MLoopNorSpaceArray *lnors_spacearr);
void BKE_lnor_space_define(
        MLoopNorSpace *lnor_space, const float lnor[3], float vec_ref[3], float vec_other[3],
        struct BLI_Stack *edge_vectors);
void BKE_lnor_space_add_loop(
        MLoopNorSpaceArray *lnors_spacearr, MLoopNorSpace *lnor_space, const int ml_index, const bool add_to_list);
void BKE_lnor_space_custom_data_to_normal(MLoopNorSpace *lnor_space, const short clnor_data[2], float r_custom_lnor[3]);
void BKE_lnor_space_custom_normal_to_data(MLoopNorSpace *lnor_space, const float custom_lnor[3], short r_clnor_data[2]);

bool BKE_mesh_has_custom_loop_normals(struct Mesh *me);

void BKE_mesh_normals_loop_split(struct MVert *mverts, const int numVerts, struct MEdge *medges, const int numEdges,
        struct MLoop *mloops, float (*r_loopnors)[3], const int numLoops,
        struct MPoly *mpolys, const float (*polynors)[3], const int numPolys,
        const bool use_split_normals, float split_angle,
        MLoopNorSpaceArray *r_lnors_spacearr, short (*clnors_data)[2], int *r_loop_to_poly);

void BKE_mesh_normals_loop_custom_set(
        struct MVert *mverts, const int numVerts, struct MEdge *medges, const int numEdges,
        struct MLoop *mloops, float (*custom_loopnors)[3], const int numLoops,
        struct MPoly *mpolys, const float (*polynors)[3], const int numPolys,
        short (*r_clnors_data)[2]);
void BKE_mesh_normals_loop_custom_from_vertices_set(
        struct MVert *mverts, float (*custom_vertnors)[3], const int numVerts,
        struct MEdge *medges, const int numEdges, struct MLoop *mloops, const int numLoops,
        struct MPoly *mpolys, const float (*polynors)[3], const int numPolys,
        short (*r_clnors_data)[2]);

void BKE_mesh_calc_poly_normal(
        struct MPoly *mpoly, struct MLoop *loopstart,
        struct MVert *mvarray, float no[3]);
void BKE_mesh_calc_poly_normal_coords(
        struct MPoly *mpoly, struct MLoop *loopstart,
        const float (*vertex_coords)[3], float no[3]);
void BKE_mesh_calc_poly_center(
        struct MPoly *mpoly, struct MLoop *loopstart,
        struct MVert *mvarray, float cent[3]);
float BKE_mesh_calc_poly_area(
        struct MPoly *mpoly, struct MLoop *loopstart,
        struct MVert *mvarray);
void BKE_mesh_calc_poly_angles(
        struct MPoly *mpoly, struct MLoop *loopstart,
        struct MVert *mvarray, float angles[]);

void BKE_mesh_poly_edgehash_insert(
        struct EdgeHash *ehash,
        const struct MPoly *mp, const struct MLoop *mloop);
void BKE_mesh_poly_edgebitmap_insert(
        unsigned int *edge_bitmap,
        const struct MPoly *mp, const struct MLoop *mloop);


bool BKE_mesh_center_median(struct Mesh *me, float cent[3]);
bool BKE_mesh_center_bounds(struct Mesh *me, float cent[3]);
bool BKE_mesh_center_centroid(struct Mesh *me, float cent[3]);

void BKE_mesh_calc_volume(struct MVert *mverts, int numVerts,
                          struct MFace *mfaces, int numFaces,
                          float *r_vol, float *r_com);

/* tessface */
void BKE_mesh_loops_to_mface_corners(
        struct CustomData *fdata, struct CustomData *ldata,
        struct CustomData *pdata, unsigned int lindex[4], int findex,
        const int polyindex, const int mf_len,
        const int numTex, const int numCol,
        const bool hasPCol, const bool hasOrigSpace, const bool hasLNor);
void BKE_mesh_loops_to_tessdata(
        struct CustomData *fdata, struct CustomData *ldata, struct CustomData *pdata, struct MFace *mface,
        int *polyindices, unsigned int (*loopindices)[4], const int num_faces);
int BKE_mesh_recalc_tessellation(
        struct CustomData *fdata, struct CustomData *ldata, struct CustomData *pdata,
        struct MVert *mvert,
        int totface, int totloop, int totpoly,
        const bool do_face_normals);
int BKE_mesh_mpoly_to_mface(
        struct CustomData *fdata, struct CustomData *ldata,
        struct CustomData *pdata, int totface, int totloop, int totpoly);
void BKE_mesh_convert_mfaces_to_mpolys(struct Mesh *mesh);
void BKE_mesh_do_versions_convert_mfaces_to_mpolys(struct Mesh *mesh);
void BKE_mesh_convert_mfaces_to_mpolys_ex(
        struct ID *id,
        struct CustomData *fdata, struct CustomData *ldata, struct CustomData *pdata,
        int totedge_i, int totface_i, int totloop_i, int totpoly_i,
        struct MEdge *medge, struct MFace *mface,
        int *r_totloop, int *r_totpoly,
        struct MLoop **r_mloop, struct MPoly **r_mpoly);

/* flush flags */
void BKE_mesh_flush_hidden_from_verts_ex(
        const struct MVert *mvert,
        const struct MLoop *mloop,
        struct MEdge *medge, const int totedge,
        struct MPoly *mpoly, const int totpoly);
void BKE_mesh_flush_hidden_from_verts(struct Mesh *me);
void BKE_mesh_flush_hidden_from_polys_ex(
        struct MVert *mvert,
        const struct MLoop *mloop,
        struct MEdge *medge, const int totedge,
        const struct MPoly *mpoly, const int totpoly);
void BKE_mesh_flush_hidden_from_polys(struct Mesh *me);
void BKE_mesh_flush_select_from_polys_ex(
        struct MVert *mvert,       const int totvert,
        const struct MLoop *mloop,
        struct MEdge *medge,       const int totedge,
        const struct MPoly *mpoly, const int totpoly);
void BKE_mesh_flush_select_from_polys(struct Mesh *me);
void BKE_mesh_flush_select_from_verts_ex(
        const struct MVert *mvert, const int totvert,
        const struct MLoop *mloop,
        struct MEdge *medge,       const int totedge,
        struct MPoly *mpoly,       const int totpoly);
void BKE_mesh_flush_select_from_verts(struct Mesh *me);

/* spatial evaluation */
void BKE_mesh_calc_relative_deform(
        const struct MPoly *mpoly, const int totpoly,
        const struct MLoop *mloop, const int totvert,

        const float (*vert_cos_src)[3],
        const float (*vert_cos_dst)[3],

        const float (*vert_cos_org)[3],
              float (*vert_cos_new)[3]);



/* *** mesh_validate.c *** */

int BKE_mesh_validate(struct Mesh *me, const int do_verbose, const int cddata_check_mask);
void BKE_mesh_cd_validate(struct Mesh *me);
int BKE_mesh_validate_material_indices(struct Mesh *me);

bool BKE_mesh_validate_arrays(
        struct Mesh *me,
        struct MVert *mverts, unsigned int totvert,
        struct MEdge *medges, unsigned int totedge,
        struct MFace *mfaces, unsigned int totface,
        struct MLoop *mloops, unsigned int totloop,
        struct MPoly *mpolys, unsigned int totpoly,
        struct MDeformVert *dverts, /* assume totvert length */
        const bool do_verbose, const bool do_fixes,
        bool *r_change);

bool BKE_mesh_validate_all_customdata(
        struct CustomData *vdata, struct CustomData *edata,
        struct CustomData *ldata, struct CustomData *pdata,
        const bool check_meshmask,
        const bool do_verbose, const bool do_fixes,
        bool *r_change);

void BKE_mesh_strip_loose_faces(struct Mesh *me);
void BKE_mesh_strip_loose_polysloops(struct Mesh *me);
void BKE_mesh_strip_loose_edges(struct Mesh *me);

void BKE_mesh_calc_edges_legacy(struct Mesh *me, const bool use_old);
void BKE_mesh_calc_edges(struct Mesh *mesh, bool update, const bool select);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_MESH_H__ */
