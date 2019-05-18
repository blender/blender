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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_MESH_H__
#define __ED_MESH_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct BMBVHTree;
struct BMEdge;
struct BMEditMesh;
struct BMElem;
struct BMFace;
struct BMLoop;
struct BMVert;
struct BMesh;
struct Base;
struct Depsgraph;
struct ID;
struct MDeformVert;
struct Mesh;
struct Object;
struct ReportList;
struct Scene;
struct ToolSettings;
struct UndoType;
struct UvMapVert;
struct UvMapVert;
struct UvVertMap;
struct UvVertMap;
struct View3D;
struct ViewContext;
struct bContext;
struct bDeformGroup;
struct rcti;
struct wmKeyConfig;
struct wmOperator;

/* editmesh_utils.c */
void EDBM_verts_mirror_cache_begin_ex(struct BMEditMesh *em,
                                      const int axis,
                                      const bool use_self,
                                      const bool use_select,
                                      const bool use_topology,
                                      float maxdist,
                                      int *r_index);
void EDBM_verts_mirror_cache_begin(struct BMEditMesh *em,
                                   const int axis,
                                   const bool use_self,
                                   const bool use_select,
                                   const bool use_toplogy);
void EDBM_verts_mirror_apply(struct BMEditMesh *em, const int sel_from, const int sel_to);
struct BMVert *EDBM_verts_mirror_get(struct BMEditMesh *em, struct BMVert *v);
struct BMEdge *EDBM_verts_mirror_get_edge(struct BMEditMesh *em, struct BMEdge *e);
struct BMFace *EDBM_verts_mirror_get_face(struct BMEditMesh *em, struct BMFace *f);
void EDBM_verts_mirror_cache_clear(struct BMEditMesh *em, struct BMVert *v);
void EDBM_verts_mirror_cache_end(struct BMEditMesh *em);

void EDBM_mesh_normals_update(struct BMEditMesh *em);
void EDBM_mesh_clear(struct BMEditMesh *em);

void EDBM_selectmode_to_scene(struct bContext *C);
void EDBM_mesh_make(struct Object *ob, const int select_mode, const bool add_key_index);
void EDBM_mesh_free(struct BMEditMesh *em);
void EDBM_mesh_load(struct Main *bmain, struct Object *ob);

/* flushes based on the current select mode.  if in vertex select mode,
 * verts select/deselect edges and faces, if in edge select mode,
 * edges select/deselect faces and vertices, and in face select mode faces select/deselect
 * edges and vertices.*/
void EDBM_select_more(struct BMEditMesh *em, const bool use_face_step);
void EDBM_select_less(struct BMEditMesh *em, const bool use_face_step);

void EDBM_selectmode_flush_ex(struct BMEditMesh *em, const short selectmode);
void EDBM_selectmode_flush(struct BMEditMesh *em);

void EDBM_deselect_flush(struct BMEditMesh *em);
void EDBM_select_flush(struct BMEditMesh *em);

bool EDBM_vert_color_check(struct BMEditMesh *em);

bool EDBM_mesh_hide(struct BMEditMesh *em, bool swap);
bool EDBM_mesh_reveal(struct BMEditMesh *em, bool select);

void EDBM_update_generic(struct BMEditMesh *em, const bool do_tessface, const bool is_destructive);

struct UvElementMap *BM_uv_element_map_create(struct BMesh *bm,
                                              const bool selected,
                                              const bool use_winding,
                                              const bool do_islands);
void BM_uv_element_map_free(struct UvElementMap *vmap);
struct UvElement *BM_uv_element_get(struct UvElementMap *map,
                                    struct BMFace *efa,
                                    struct BMLoop *l);

bool EDBM_uv_check(struct BMEditMesh *em);
struct BMFace *EDBM_uv_active_face_get(struct BMEditMesh *em,
                                       const bool sloppy,
                                       const bool selected);

void BM_uv_vert_map_free(struct UvVertMap *vmap);
struct UvMapVert *BM_uv_vert_map_at_index(struct UvVertMap *vmap, unsigned int v);
struct UvVertMap *BM_uv_vert_map_create(struct BMesh *bm,
                                        const float limit[2],
                                        const bool use_select,
                                        const bool use_winding);

void EDBM_flag_enable_all(struct BMEditMesh *em, const char hflag);
void EDBM_flag_disable_all(struct BMEditMesh *em, const char hflag);

bool BMBVH_EdgeVisible(struct BMBVHTree *tree,
                       struct BMEdge *e,
                       struct Depsgraph *depsgraph,
                       struct ARegion *ar,
                       struct View3D *v3d,
                       struct Object *obedit);

/* editmesh_undo.c */
void ED_mesh_undosys_type(struct UndoType *ut);

/* editmesh_select.c */
struct EDBMSelectID_Context;
struct EDBMSelectID_Context *EDBM_select_id_context_create(struct ViewContext *vc,
                                                           struct Base **bases,
                                                           const uint bases_len,
                                                           short select_mode);
void EDBM_select_id_context_destroy(struct EDBMSelectID_Context *sel_id_ctx);
struct BMElem *EDBM_select_id_bm_elem_get(struct EDBMSelectID_Context *sel_id_ctx,
                                          const uint sel_id,
                                          uint *r_base_index);

uint EDBM_select_id_context_offset_for_object_elem(const struct EDBMSelectID_Context *sel_id_ctx,
                                                   int base_index,
                                                   char htype);

uint EDBM_select_id_context_elem_len(const struct EDBMSelectID_Context *sel_id_ctx);

void EDBM_select_mirrored(
    struct BMEditMesh *em, const int axis, const bool extend, int *r_totmirr, int *r_totfail);
void EDBM_automerge(struct Scene *scene, struct Object *ob, bool update, const char hflag);

bool EDBM_backbuf_border_init(
    struct ViewContext *vc, short xmin, short ymin, short xmax, short ymax);
bool EDBM_backbuf_check(unsigned int index);
void EDBM_backbuf_free(void);

bool EDBM_backbuf_border_mask_init(struct ViewContext *vc,
                                   const int mcords[][2],
                                   short tot,
                                   short xmin,
                                   short ymin,
                                   short xmax,
                                   short ymax);
bool EDBM_backbuf_circle_init(struct ViewContext *vc, short xs, short ys, short rads);

struct BMVert *EDBM_vert_find_nearest_ex(struct ViewContext *vc,
                                         float *r_dist,
                                         const bool use_select_bias,
                                         bool use_cycle,
                                         struct Base **bases,
                                         uint bases_len,
                                         uint *r_base_index);
struct BMVert *EDBM_vert_find_nearest(struct ViewContext *vc, float *r_dist);

struct BMEdge *EDBM_edge_find_nearest_ex(struct ViewContext *vc,
                                         float *r_dist,
                                         float *r_dist_center,
                                         const bool use_select_bias,
                                         bool use_cycle,
                                         struct BMEdge **r_eed_zbuf,
                                         struct Base **bases,
                                         uint bases_len,
                                         uint *r_base_index);
struct BMEdge *EDBM_edge_find_nearest(struct ViewContext *vc, float *r_dist);

struct BMFace *EDBM_face_find_nearest_ex(struct ViewContext *vc,
                                         float *r_dist,
                                         float *r_dist_center,
                                         const bool use_select_bias,
                                         bool use_cycle,
                                         struct BMFace **r_efa_zbuf,
                                         struct Base **bases,
                                         uint bases_len,
                                         uint *r_base_index);
struct BMFace *EDBM_face_find_nearest(struct ViewContext *vc, float *r_dist);

bool EDBM_unified_findnearest(struct ViewContext *vc,
                              struct Base **bases,
                              const uint bases_len,
                              int *r_base_index,
                              struct BMVert **r_eve,
                              struct BMEdge **r_eed,
                              struct BMFace **r_efa);

bool EDBM_unified_findnearest_from_raycast(struct ViewContext *vc,
                                           struct Base **bases,
                                           const uint bases_len,
                                           bool use_boundary,
                                           int *r_base_index,
                                           struct BMVert **r_eve,
                                           struct BMEdge **r_eed,
                                           struct BMFace **r_efa);

bool EDBM_select_pick(
    struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);

void EDBM_selectmode_set(struct BMEditMesh *em);
void EDBM_selectmode_convert(struct BMEditMesh *em,
                             const short selectmode_old,
                             const short selectmode_new);

/* user access this */
bool EDBM_selectmode_toggle(struct bContext *C,
                            const short selectmode_new,
                            const int action,
                            const bool use_extend,
                            const bool use_expand);

bool EDBM_selectmode_disable(struct Scene *scene,
                             struct BMEditMesh *em,
                             const short selectmode_disable,
                             const short selectmode_fallback);

bool EDBM_deselect_by_material(struct BMEditMesh *em, const short index, const bool select);

void EDBM_select_toggle_all(struct BMEditMesh *em);

void EDBM_select_swap(struct BMEditMesh *em); /* exported for UV */
bool EDBM_select_interior_faces(struct BMEditMesh *em);
void em_setup_viewcontext(struct bContext *C, struct ViewContext *vc); /* rename? */

bool EDBM_mesh_deselect_all_multi_ex(struct Base **bases, const uint bases_len);
bool EDBM_mesh_deselect_all_multi(struct bContext *C);

/* Only use for modes that don't support multi-edit-modes (painting). */
extern unsigned int bm_vertoffs, bm_solidoffs, bm_wireoffs;

/* editmesh_preselect_edgering.c */
struct EditMesh_PreSelEdgeRing;
struct EditMesh_PreSelEdgeRing *EDBM_preselect_edgering_create(void);
void EDBM_preselect_edgering_destroy(struct EditMesh_PreSelEdgeRing *psel);
void EDBM_preselect_edgering_clear(struct EditMesh_PreSelEdgeRing *psel);
void EDBM_preselect_edgering_draw(struct EditMesh_PreSelEdgeRing *psel, const float matrix[4][4]);
void EDBM_preselect_edgering_update_from_edge(struct EditMesh_PreSelEdgeRing *psel,
                                              struct BMesh *bm,
                                              struct BMEdge *eed_start,
                                              int previewlines,
                                              const float (*coords)[3]);

/* editmesh_preselect_elem.c */
struct EditMesh_PreSelElem;
struct EditMesh_PreSelElem *EDBM_preselect_elem_create(void);
void EDBM_preselect_elem_destroy(struct EditMesh_PreSelElem *psel);
void EDBM_preselect_elem_clear(struct EditMesh_PreSelElem *psel);
void EDBM_preselect_elem_draw(struct EditMesh_PreSelElem *psel, const float matrix[4][4]);
void EDBM_preselect_elem_update_from_single(struct EditMesh_PreSelElem *psel,
                                            struct BMesh *bm,
                                            struct BMElem *ele,
                                            const float (*coords)[3]);

/* mesh_ops.c */
void ED_operatortypes_mesh(void);
void ED_operatormacros_mesh(void);
void ED_keymap_mesh(struct wmKeyConfig *keyconf);

/* editmesh_tools.c (could be moved) */
void EDBM_project_snap_verts(struct bContext *C, struct ARegion *ar, struct BMEditMesh *em);

/* editface.c */
void paintface_flush_flags(struct bContext *C, struct Object *ob, short flag);
bool paintface_mouse_select(struct bContext *C,
                            struct Object *ob,
                            const int mval[2],
                            bool extend,
                            bool deselect,
                            bool toggle);
bool do_paintface_box_select(struct ViewContext *vc, const struct rcti *rect, int sel_op);
bool paintface_deselect_all_visible(struct bContext *C,
                                    struct Object *ob,
                                    int action,
                                    bool flush_flags);
void paintface_select_linked(struct bContext *C,
                             struct Object *ob,
                             const int mval[2],
                             const bool select);
bool paintface_minmax(struct Object *ob, float r_min[3], float r_max[3]);

void paintface_hide(struct bContext *C, struct Object *ob, const bool unselected);
void paintface_reveal(struct bContext *C, struct Object *ob, const bool select);

bool paintvert_deselect_all_visible(struct Object *ob, int action, bool flush_flags);
void paintvert_select_ungrouped(struct Object *ob, bool extend, bool flush_flags);
void paintvert_flush_flags(struct Object *ob);
void paintvert_tag_select_update(struct bContext *C, struct Object *ob);

/* mirrtopo */
typedef struct MirrTopoStore_t {
  intptr_t *index_lookup;
  int prev_vert_tot;
  int prev_edge_tot;
  bool prev_is_editmode;
} MirrTopoStore_t;

bool ED_mesh_mirrtopo_recalc_check(struct Mesh *me,
                                   struct Mesh *me_eval,
                                   MirrTopoStore_t *mesh_topo_store);
void ED_mesh_mirrtopo_init(struct Mesh *me,
                           struct Mesh *me_eval,
                           MirrTopoStore_t *mesh_topo_store,
                           const bool skip_em_vert_array_init);
void ED_mesh_mirrtopo_free(MirrTopoStore_t *mesh_topo_store);

/* object_vgroup.c */
#define WEIGHT_REPLACE 1
#define WEIGHT_ADD 2
#define WEIGHT_SUBTRACT 3

bool ED_vgroup_sync_from_pose(struct Object *ob);
void ED_vgroup_select_by_name(struct Object *ob, const char *name);
void ED_vgroup_data_clamp_range(struct ID *id, const int total);
bool ED_vgroup_array_copy(struct Object *ob, struct Object *ob_from);
bool ED_vgroup_parray_alloc(struct ID *id,
                            struct MDeformVert ***dvert_arr,
                            int *dvert_tot,
                            const bool use_vert_sel);
void ED_vgroup_parray_mirror_sync(struct Object *ob,
                                  struct MDeformVert **dvert_array,
                                  const int dvert_tot,
                                  const bool *vgroup_validmap,
                                  const int vgroup_tot);
void ED_vgroup_parray_mirror_assign(struct Object *ob,
                                    struct MDeformVert **dvert_array,
                                    const int dvert_tot);
void ED_vgroup_parray_remove_zero(struct MDeformVert **dvert_array,
                                  const int dvert_tot,
                                  const bool *vgroup_validmap,
                                  const int vgroup_tot,
                                  const float epsilon,
                                  const bool keep_single);
void ED_vgroup_parray_to_weight_array(const struct MDeformVert **dvert_array,
                                      const int dvert_tot,
                                      float *dvert_weights,
                                      const int def_nr);
void ED_vgroup_parray_from_weight_array(struct MDeformVert **dvert_array,
                                        const int dvert_tot,
                                        const float *dvert_weights,
                                        const int def_nr,
                                        const bool remove_zero);
void ED_vgroup_mirror(struct Object *ob,
                      const bool mirror_weights,
                      const bool flip_vgroups,
                      const bool all_vgroups,
                      const bool use_topology,
                      int *r_totmirr,
                      int *r_totfail);

void ED_vgroup_vert_add(
    struct Object *ob, struct bDeformGroup *dg, int vertnum, float weight, int assignmode);
void ED_vgroup_vert_remove(struct Object *ob, struct bDeformGroup *dg, int vertnum);
float ED_vgroup_vert_weight(struct Object *ob, struct bDeformGroup *dg, int vertnum);
void ED_vgroup_vert_active_mirror(struct Object *ob, int def_nr);

/* mesh_data.c */
#if 0
void ED_mesh_geometry_add(
    struct Mesh *mesh, struct ReportList *reports, int verts, int edges, int faces);
#endif
void ED_mesh_polys_add(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_tessfaces_add(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_edges_add(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_loops_add(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_vertices_add(struct Mesh *mesh, struct ReportList *reports, int count);

void ED_mesh_faces_remove(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_edges_remove(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_vertices_remove(struct Mesh *mesh, struct ReportList *reports, int count);

void ED_mesh_calc_tessface(struct Mesh *mesh, bool free_mpoly);
void ED_mesh_update(struct Mesh *mesh,
                    struct bContext *C,
                    bool calc_edges,
                    bool calc_edges_loose,
                    bool calc_tessface);

void ED_mesh_uv_texture_ensure(struct Mesh *me, const char *name);
int ED_mesh_uv_texture_add(struct Mesh *me,
                           const char *name,
                           const bool active_set,
                           const bool do_init);
bool ED_mesh_uv_texture_remove_index(struct Mesh *me, const int n);
bool ED_mesh_uv_texture_remove_active(struct Mesh *me);
bool ED_mesh_uv_texture_remove_named(struct Mesh *me, const char *name);
void ED_mesh_uv_loop_reset(struct bContext *C, struct Mesh *me);
void ED_mesh_uv_loop_reset_ex(struct Mesh *me, const int layernum);
bool ED_mesh_color_ensure(struct Mesh *me, const char *name);
int ED_mesh_color_add(struct Mesh *me,
                      const char *name,
                      const bool active_set,
                      const bool do_init);
bool ED_mesh_color_remove_index(struct Mesh *me, const int n);
bool ED_mesh_color_remove_active(struct Mesh *me);
bool ED_mesh_color_remove_named(struct Mesh *me, const char *name);

void ED_mesh_report_mirror(struct wmOperator *op, int totmirr, int totfail);
void ED_mesh_report_mirror_ex(struct wmOperator *op, int totmirr, int totfail, char selectmode);

/* mesh backup */
typedef struct BMBackup {
  struct BMesh *bmcopy;
} BMBackup;

/* save a copy of the bmesh for restoring later */
struct BMBackup EDBM_redo_state_store(struct BMEditMesh *em);
/* restore a bmesh from backup */
void EDBM_redo_state_restore(struct BMBackup, struct BMEditMesh *em, int recalctess);
/* delete the backup, optionally flushing it to an editmesh */
void EDBM_redo_state_free(struct BMBackup *, struct BMEditMesh *em, int recalctess);

/* *** meshtools.c *** */
int join_mesh_exec(struct bContext *C, struct wmOperator *op);
int join_mesh_shapes_exec(struct bContext *C, struct wmOperator *op);

/* mirror lookup api */
int ED_mesh_mirror_spatial_table(
    struct Object *ob, struct BMEditMesh *em, struct Mesh *me_eval, const float co[3], char mode);
int ED_mesh_mirror_topo_table(struct Object *ob, struct Mesh *me_eval, char mode);

/* retrieves mirrored cache vert, or NULL if there isn't one.
 * note: calling this without ensuring the mirror cache state
 * is bad.*/
int mesh_get_x_mirror_vert(struct Object *ob,
                           struct Mesh *me_eval,
                           int index,
                           const bool use_topology);
struct BMVert *editbmesh_get_x_mirror_vert(struct Object *ob,
                                           struct BMEditMesh *em,
                                           struct BMVert *eve,
                                           const float co[3],
                                           int index,
                                           const bool use_topology);
int *mesh_get_x_mirror_faces(struct Object *ob, struct BMEditMesh *em, struct Mesh *me_eval);

int ED_mesh_mirror_get_vert(struct Object *ob, int index);

bool ED_mesh_pick_vert(struct bContext *C,
                       struct Object *ob,
                       const int mval[2],
                       uint dist_px,
                       bool use_zbuf,
                       uint *r_index);
bool ED_mesh_pick_face(
    struct bContext *C, struct Object *ob, const int mval[2], uint dist_px, uint *r_index);
bool ED_mesh_pick_face_vert(
    struct bContext *C, struct Object *ob, const int mval[2], uint dist_px, uint *r_index);

struct MDeformVert *ED_mesh_active_dvert_get_em(struct Object *ob, struct BMVert **r_eve);
struct MDeformVert *ED_mesh_active_dvert_get_ob(struct Object *ob, int *r_index);
struct MDeformVert *ED_mesh_active_dvert_get_only(struct Object *ob);

void EDBM_mesh_stats_multi(struct Object **objects,
                           const uint objects_len,
                           int totelem[3],
                           int totelem_sel[3]);
void EDBM_mesh_elem_index_ensure_multi(struct Object **objects,
                                       const uint objects_len,
                                       const char htype);

#define ED_MESH_PICK_DEFAULT_VERT_DIST 25
#define ED_MESH_PICK_DEFAULT_FACE_DIST 1

#define USE_LOOPSLIDE_HACK

#ifdef __cplusplus
}
#endif

#endif /* __ED_MESH_H__ */
