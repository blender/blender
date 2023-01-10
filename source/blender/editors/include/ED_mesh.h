/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

struct ARegion;
struct BMBVHTree;
struct BMEdge;
struct BMEditMesh;
struct BMElem;
struct BMFace;
struct BMLoop;
struct BMVert;
struct BMesh;
struct BMeshNormalsUpdate_Params;
struct Base;
struct Depsgraph;
struct ID;
struct MDeformVert;
struct Mesh;
struct Object;
struct ReportList;
struct Scene;
struct SelectPick_Params;
struct UndoType;
struct UvMapVert;
struct UvVertMap;
struct View3D;
struct ViewContext;
struct bContext;
struct bDeformGroup;
struct wmKeyConfig;
struct wmOperator;

/* editmesh_utils.c */

/**
 * \param em: Edit-mesh used for generating mirror data.
 * \param use_self: Allow a vertex to point to itself (middle verts).
 * \param use_select: Restrict to selected verts.
 * \param respecthide: Skip hidden vertices.
 * \param use_topology: Use topology mirror.
 * \param maxdist: Distance for close point test.
 * \param r_index: Optional array to write into, as an alternative to a custom-data layer
 * (length of total verts).
 */
void EDBM_verts_mirror_cache_begin_ex(struct BMEditMesh *em,
                                      int axis,
                                      bool use_self,
                                      bool use_select,
                                      bool respecthide,
                                      bool use_topology,
                                      float maxdist,
                                      int *r_index);
void EDBM_verts_mirror_cache_begin(struct BMEditMesh *em,
                                   int axis,
                                   bool use_self,
                                   bool use_select,
                                   bool respecthide,
                                   bool use_topology);
void EDBM_verts_mirror_apply(struct BMEditMesh *em, int sel_from, int sel_to);
struct BMVert *EDBM_verts_mirror_get(struct BMEditMesh *em, struct BMVert *v);
struct BMEdge *EDBM_verts_mirror_get_edge(struct BMEditMesh *em, struct BMEdge *e);
struct BMFace *EDBM_verts_mirror_get_face(struct BMEditMesh *em, struct BMFace *f);
void EDBM_verts_mirror_cache_clear(struct BMEditMesh *em, struct BMVert *v);
void EDBM_verts_mirror_cache_end(struct BMEditMesh *em);

void EDBM_mesh_normals_update_ex(struct BMEditMesh *em,
                                 const struct BMeshNormalsUpdate_Params *params);
void EDBM_mesh_normals_update(struct BMEditMesh *em);
void EDBM_mesh_clear(struct BMEditMesh *em);

void EDBM_selectmode_to_scene(struct bContext *C);
void EDBM_mesh_make(struct Object *ob, int select_mode, bool add_key_index);
/**
 * Should only be called on the active edit-mesh, otherwise call #BKE_editmesh_free_data.
 */
void EDBM_mesh_free_data(struct BMEditMesh *em);
/**
 * \warning This can invalidate the #Mesh runtime cache of other objects (for linked duplicates).
 * Most callers should run #DEG_id_tag_update on `ob->data`, see: T46738, T46913.
 * This ensures #BKE_object_free_derived_caches runs on all objects that use this mesh.
 */
void EDBM_mesh_load_ex(struct Main *bmain, struct Object *ob, bool free_data);
void EDBM_mesh_load(struct Main *bmain, struct Object *ob);

/**
 * flushes based on the current select mode. If in vertex select mode,
 * verts select/deselect edges and faces, if in edge select mode,
 * edges select/deselect faces and vertices, and in face select mode faces select/deselect
 * edges and vertices.
 */
void EDBM_select_more(struct BMEditMesh *em, bool use_face_step);
void EDBM_select_less(struct BMEditMesh *em, bool use_face_step);

void EDBM_selectmode_flush_ex(struct BMEditMesh *em, short selectmode);
void EDBM_selectmode_flush(struct BMEditMesh *em);

void EDBM_deselect_flush(struct BMEditMesh *em);
void EDBM_select_flush(struct BMEditMesh *em);

bool EDBM_vert_color_check(struct BMEditMesh *em);

/**
 * Swap is 0 or 1, if 1 it hides not selected.
 */
bool EDBM_mesh_hide(struct BMEditMesh *em, bool swap);
bool EDBM_mesh_reveal(struct BMEditMesh *em, bool select);

struct EDBMUpdate_Params {
  uint calc_looptri : 1;
  uint calc_normals : 1;
  uint is_destructive : 1;
};

/**
 * So many tools call these that we better make it a generic function.
 */
void EDBM_update(struct Mesh *me, const struct EDBMUpdate_Params *params);
/**
 * Bad level call from Python API.
 */
void EDBM_update_extern(struct Mesh *me, bool do_tessellation, bool is_destructive);

/**
 * A specialized vert map used by stitch operator.
 */
struct UvElementMap *BM_uv_element_map_create(struct BMesh *bm,
                                              const struct Scene *scene,
                                              bool uv_selected,
                                              bool use_winding,
                                              bool use_seams,
                                              bool do_islands);
void BM_uv_element_map_free(struct UvElementMap *element_map);
struct UvElement *BM_uv_element_get(const struct UvElementMap *element_map,
                                    const struct BMFace *efa,
                                    const struct BMLoop *l);
struct UvElement *BM_uv_element_get_head(struct UvElementMap *element_map,
                                         struct UvElement *child);
int BM_uv_element_get_unique_index(struct UvElementMap *element_map, struct UvElement *child);

struct UvElement **BM_uv_element_map_ensure_head_table(struct UvElementMap *element_map);
int *BM_uv_element_map_ensure_unique_index(struct UvElementMap *element_map);

/**
 * Can we edit UV's for this mesh?
 */
bool EDBM_uv_check(struct BMEditMesh *em);
/**
 * last_sel, use em->act_face otherwise get the last selected face in the editselections
 * at the moment, last_sel is mainly useful for making sure the space image doesn't flicker.
 */
struct BMFace *EDBM_uv_active_face_get(struct BMEditMesh *em, bool sloppy, bool selected);

void BM_uv_vert_map_free(struct UvVertMap *vmap);
struct UvMapVert *BM_uv_vert_map_at_index(struct UvVertMap *vmap, unsigned int v);
/**
 * Return a new #UvVertMap from the edit-mesh.
 */
struct UvVertMap *BM_uv_vert_map_create(struct BMesh *bm, bool use_select, bool use_winding);

void EDBM_flag_enable_all(struct BMEditMesh *em, char hflag);
void EDBM_flag_disable_all(struct BMEditMesh *em, char hflag);

bool BMBVH_EdgeVisible(struct BMBVHTree *tree,
                       struct BMEdge *e,
                       struct Depsgraph *depsgraph,
                       struct ARegion *region,
                       struct View3D *v3d,
                       struct Object *obedit);

void EDBM_project_snap_verts(struct bContext *C,
                             struct Depsgraph *depsgraph,
                             struct ARegion *region,
                             struct Object *obedit,
                             struct BMEditMesh *em);

/* editmesh_automerge.c */

void EDBM_automerge(struct Object *obedit, bool update, char hflag, float dist);
void EDBM_automerge_and_split(struct Object *obedit,
                              bool split_edges,
                              bool split_faces,
                              bool update,
                              char hflag,
                              float dist);

/* editmesh_undo.cc */

/** Export for ED_undo_sys. */
void ED_mesh_undosys_type(struct UndoType *ut);

/* editmesh_select.cc */

void EDBM_select_mirrored(struct BMEditMesh *em,
                          const struct Mesh *me,
                          int axis,
                          bool extend,
                          int *r_totmirr,
                          int *r_totfail);

/**
 * Nearest vertex under the cursor.
 *
 * \param dist_px_manhattan_p: (in/out), minimal distance to the nearest and at the end,
 * actual distance.
 * \param use_select_bias:
 * - When true, selected vertices are given a 5 pixel bias
 *   to make them further than unselected vertices.
 * - When false, unselected vertices are given the bias.
 * \param use_cycle: Cycle over elements within #FIND_NEAR_CYCLE_THRESHOLD_MIN in order of index.
 */
struct BMVert *EDBM_vert_find_nearest_ex(struct ViewContext *vc,
                                         float *dist_px_manhattan_p,
                                         bool use_select_bias,
                                         bool use_cycle,
                                         struct Base **bases,
                                         uint bases_len,
                                         uint *r_base_index);
struct BMVert *EDBM_vert_find_nearest(struct ViewContext *vc, float *dist_px_manhattan_p);

struct BMEdge *EDBM_edge_find_nearest_ex(struct ViewContext *vc,
                                         float *dist_px_manhattan,
                                         float *r_dist_center,
                                         bool use_select_bias,
                                         bool use_cycle,
                                         struct BMEdge **r_eed_zbuf,
                                         struct Base **bases,
                                         uint bases_len,
                                         uint *r_base_index);
struct BMEdge *EDBM_edge_find_nearest(struct ViewContext *vc, float *dist_px_manhattan_p);

/**
 * \param use_zbuf_single_px: Special case, when using the back-buffer selection,
 * only use the pixel at `vc->mval` instead of using `dist_px_manhattan_p` to search over a larger
 * region. This is needed because historically selection worked this way for a long time, however
 * it's reasonable that some callers might want to expand the region too. So add an argument to do
 * this,
 */
struct BMFace *EDBM_face_find_nearest_ex(struct ViewContext *vc,
                                         float *dist_px_manhattan,
                                         float *r_dist_center,
                                         bool use_zbuf_single_px,
                                         bool use_select_bias,
                                         bool use_cycle,
                                         struct BMFace **r_efa_zbuf,
                                         struct Base **bases,
                                         uint bases_len,
                                         uint *r_base_index);
struct BMFace *EDBM_face_find_nearest(struct ViewContext *vc, float *dist_px_manhattan_p);

bool EDBM_unified_findnearest(struct ViewContext *vc,
                              struct Base **bases,
                              uint bases_len,
                              int *r_base_index,
                              struct BMVert **r_eve,
                              struct BMEdge **r_eed,
                              struct BMFace **r_efa);

bool EDBM_unified_findnearest_from_raycast(struct ViewContext *vc,
                                           struct Base **bases,
                                           uint bases_len,
                                           bool use_boundary_vertices,
                                           bool use_boundary_edges,
                                           int *r_base_index_vert,
                                           int *r_base_index_edge,
                                           int *r_base_index_face,
                                           struct BMVert **r_eve,
                                           struct BMEdge **r_eed,
                                           struct BMFace **r_efa);

bool EDBM_select_pick(struct bContext *C,
                      const int mval[2],
                      const struct SelectPick_Params *params);

/**
 * When switching select mode, makes sure selection is consistent for editing
 * also for paranoia checks to make sure edge or face mode works.
 */
void EDBM_selectmode_set(struct BMEditMesh *em);
/**
 * Expand & Contract the Selection
 * (used when changing modes and Ctrl key held)
 *
 * Flush the selection up:
 * - vert -> edge
 * - vert -> face
 * - edge -> face
 *
 * Flush the selection down:
 * - face -> edge
 * - face -> vert
 * - edge -> vert
 */
void EDBM_selectmode_convert(struct BMEditMesh *em, short selectmode_old, short selectmode_new);

/**
 * User access this.
 */
bool EDBM_selectmode_set_multi(struct bContext *C, short selectmode);
/**
 * User facing function, does notification.
 */
bool EDBM_selectmode_toggle_multi(
    struct bContext *C, short selectmode_new, int action, bool use_extend, bool use_expand);

/**
 * Use to disable a select-mode if its enabled, Using another mode as a fallback
 * if the disabled mode is the only mode set.
 *
 * \return true if the mode is changed.
 */
bool EDBM_selectmode_disable(struct Scene *scene,
                             struct BMEditMesh *em,
                             short selectmode_disable,
                             short selectmode_fallback);

bool EDBM_deselect_by_material(struct BMEditMesh *em, short index, bool select);

void EDBM_select_toggle_all(struct BMEditMesh *em);

void EDBM_select_swap(struct BMEditMesh *em); /* exported for UV */
bool EDBM_select_interior_faces(struct BMEditMesh *em);
void em_setup_viewcontext(struct bContext *C, struct ViewContext *vc); /* rename? */

bool EDBM_mesh_deselect_all_multi_ex(struct Base **bases, uint bases_len);
bool EDBM_mesh_deselect_all_multi(struct bContext *C);
bool EDBM_selectmode_disable_multi_ex(struct Scene *scene,
                                      struct Base **bases,
                                      uint bases_len,
                                      short selectmode_disable,
                                      short selectmode_fallback);
bool EDBM_selectmode_disable_multi(struct bContext *C,
                                   short selectmode_disable,
                                   short selectmode_fallback);

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
typedef enum eEditMesh_PreSelPreviewAction {
  PRESELECT_ACTION_TRANSFORM = 1,
  PRESELECT_ACTION_CREATE = 2,
  PRESELECT_ACTION_DELETE = 3,
} eEditMesh_PreSelPreviewAction;

struct EditMesh_PreSelElem *EDBM_preselect_elem_create(void);
void EDBM_preselect_elem_destroy(struct EditMesh_PreSelElem *psel);
void EDBM_preselect_elem_clear(struct EditMesh_PreSelElem *psel);
void EDBM_preselect_preview_clear(struct EditMesh_PreSelElem *psel);
void EDBM_preselect_elem_draw(struct EditMesh_PreSelElem *psel, const float matrix[4][4]);
void EDBM_preselect_elem_update_from_single(struct EditMesh_PreSelElem *psel,
                                            struct BMesh *bm,
                                            struct BMElem *ele,
                                            const float (*coords)[3]);

void EDBM_preselect_elem_update_preview(struct EditMesh_PreSelElem *psel,
                                        struct ViewContext *vc,
                                        struct BMesh *bm,
                                        struct BMElem *ele,
                                        const int mval[2]);
void EDBM_preselect_action_set(struct EditMesh_PreSelElem *psel,
                               eEditMesh_PreSelPreviewAction action);
eEditMesh_PreSelPreviewAction EDBM_preselect_action_get(struct EditMesh_PreSelElem *psel);

/* mesh_ops.c */

void ED_operatortypes_mesh(void);
void ED_operatormacros_mesh(void);
/**
 * Note mesh keymap also for other space?
 */
void ED_keymap_mesh(struct wmKeyConfig *keyconf);

/* editface.cc */

/**
 * Copy the face flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting faces (while painting).
 */
void paintface_flush_flags(struct bContext *C,
                           struct Object *ob,
                           bool flush_selection,
                           bool flush_hidden);
/**
 * \return True when pick finds an element or the selection changed.
 */
bool paintface_mouse_select(struct bContext *C,
                            const int mval[2],
                            const struct SelectPick_Params *params,
                            struct Object *ob);
bool paintface_deselect_all_visible(struct bContext *C,
                                    struct Object *ob,
                                    int action,
                                    bool flush_flags);
void paintface_select_linked(struct bContext *C,
                             struct Object *ob,
                             const int mval[2],
                             bool select);
bool paintface_minmax(struct Object *ob, float r_min[3], float r_max[3]);

void paintface_hide(struct bContext *C, struct Object *ob, bool unselected);
void paintface_reveal(struct bContext *C, struct Object *ob, bool select);

/**
 * \note if the caller passes false to flush_flags,
 * then they will need to run #paintvert_flush_flags(ob) themselves.
 */
bool paintvert_deselect_all_visible(struct Object *ob, int action, bool flush_flags);
void paintvert_select_ungrouped(struct Object *ob, bool extend, bool flush_flags);
/**
 * (similar to void `paintface_flush_flags(Object *ob)`)
 * copy the vertex flags, most importantly selection from the mesh to the final derived mesh,
 * use in object mode when selecting vertices (while painting).
 */
void paintvert_flush_flags(struct Object *ob);
void paintvert_tag_select_update(struct bContext *C, struct Object *ob);

void paintvert_hide(struct bContext *C, struct Object *ob, bool unselected);
void paintvert_reveal(struct bContext *C, struct Object *ob, bool select);

/* mirrtopo */
typedef struct MirrTopoStore_t {
  intptr_t *index_lookup;
  int prev_vert_tot;
  int prev_edge_tot;
  bool prev_is_editmode;
} MirrTopoStore_t;

bool ED_mesh_mirrtopo_recalc_check(struct BMEditMesh *em,
                                   struct Mesh *me,
                                   MirrTopoStore_t *mesh_topo_store);
void ED_mesh_mirrtopo_init(struct BMEditMesh *em,
                           struct Mesh *me,
                           MirrTopoStore_t *mesh_topo_store,
                           bool skip_em_vert_array_init);
void ED_mesh_mirrtopo_free(MirrTopoStore_t *mesh_topo_store);

/* object_vgroup.cc */

#define WEIGHT_REPLACE 1
#define WEIGHT_ADD 2
#define WEIGHT_SUBTRACT 3

bool ED_vgroup_sync_from_pose(struct Object *ob);
void ED_vgroup_select_by_name(struct Object *ob, const char *name);
/**
 * Removes out of range #MDeformWeights
 */
void ED_vgroup_data_clamp_range(struct ID *id, int total);
/**
 * Matching index only.
 */
bool ED_vgroup_array_copy(struct Object *ob, struct Object *ob_from);
bool ED_vgroup_parray_alloc(struct ID *id,
                            struct MDeformVert ***dvert_arr,
                            int *dvert_tot,
                            bool use_vert_sel);
/**
 * For use with tools that use ED_vgroup_parray_alloc with \a use_vert_sel == true.
 * This finds the unselected mirror deform verts and copies the weights to them from the selected.
 *
 * \note \a dvert_array has mirrored weights filled in,
 * in case cleanup operations are needed on both.
 */
void ED_vgroup_parray_mirror_sync(struct Object *ob,
                                  struct MDeformVert **dvert_array,
                                  int dvert_tot,
                                  const bool *vgroup_validmap,
                                  int vgroup_tot);
/**
 * Fill in the pointers for mirror verts (as if all mirror verts were selected too).
 *
 * similar to #ED_vgroup_parray_mirror_sync but only fill in mirror points.
 */
void ED_vgroup_parray_mirror_assign(struct Object *ob,
                                    struct MDeformVert **dvert_array,
                                    int dvert_tot);
void ED_vgroup_parray_remove_zero(struct MDeformVert **dvert_array,
                                  int dvert_tot,
                                  const bool *vgroup_validmap,
                                  int vgroup_tot,
                                  float epsilon,
                                  bool keep_single);
void ED_vgroup_parray_to_weight_array(const struct MDeformVert **dvert_array,
                                      int dvert_tot,
                                      float *dvert_weights,
                                      int def_nr);
void ED_vgroup_parray_from_weight_array(struct MDeformVert **dvert_array,
                                        int dvert_tot,
                                        const float *dvert_weights,
                                        int def_nr,
                                        bool remove_zero);
void ED_vgroup_mirror(struct Object *ob,
                      bool mirror_weights,
                      bool flip_vgroups,
                      bool all_vgroups,
                      bool use_topology,
                      int *r_totmirr,
                      int *r_totfail);

/**
 * Called while not in editmode.
 */
void ED_vgroup_vert_add(
    struct Object *ob, struct bDeformGroup *dg, int vertnum, float weight, int assignmode);
/**
 * Mesh object mode, lattice can be in edit-mode.
 */
void ED_vgroup_vert_remove(struct Object *ob, struct bDeformGroup *dg, int vertnum);
float ED_vgroup_vert_weight(struct Object *ob, struct bDeformGroup *dg, int vertnum);
/**
 * Use when adjusting the active vertex weight and apply to mirror vertices.
 */
void ED_vgroup_vert_active_mirror(struct Object *ob, int def_nr);

/* mesh_data.cc */

void ED_mesh_verts_add(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_edges_add(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_loops_add(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_polys_add(struct Mesh *mesh, struct ReportList *reports, int count);

void ED_mesh_verts_remove(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_edges_remove(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_loops_remove(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_polys_remove(struct Mesh *mesh, struct ReportList *reports, int count);

void ED_mesh_geometry_clear(struct Mesh *mesh);

void ED_mesh_update(struct Mesh *mesh, struct bContext *C, bool calc_edges, bool calc_edges_loose);

bool *ED_mesh_uv_map_vert_select_layer_ensure(struct Mesh *mesh, int uv_map_index);
bool *ED_mesh_uv_map_edge_select_layer_ensure(struct Mesh *mesh, int uv_map_index);
bool *ED_mesh_uv_map_pin_layer_ensure(struct Mesh *mesh, int uv_map_index);
const bool *ED_mesh_uv_map_vert_select_layer_get(const struct Mesh *mesh, int uv_map_index);
const bool *ED_mesh_uv_map_edge_select_layer_get(const struct Mesh *mesh, int uv_map_index);
const bool *ED_mesh_uv_map_pin_layer_get(const struct Mesh *mesh, int uv_map_index);

bool ED_mesh_edge_is_loose(const struct Mesh *mesh, int index);

void ED_mesh_uv_ensure(struct Mesh *me, const char *name);
int ED_mesh_uv_add(
    struct Mesh *me, const char *name, bool active_set, bool do_init, struct ReportList *reports);
bool ED_mesh_uv_remove_index(struct Mesh *me, int n);
bool ED_mesh_uv_remove_active(struct Mesh *me);
bool ED_mesh_uv_remove_named(struct Mesh *me, const char *name);

void ED_mesh_uv_loop_reset(struct bContext *C, struct Mesh *me);
/**
 * Without a #bContext, called when UV-editing.
 */
void ED_mesh_uv_loop_reset_ex(struct Mesh *me, int layernum);
bool ED_mesh_color_ensure(struct Mesh *me, const char *name);
int ED_mesh_color_add(
    struct Mesh *me, const char *name, bool active_set, bool do_init, struct ReportList *reports);
int ED_mesh_sculpt_color_add(struct Mesh *me,
                             const char *name,
                             bool do_init,
                             struct ReportList *reports);

void ED_mesh_report_mirror(struct wmOperator *op, int totmirr, int totfail);
void ED_mesh_report_mirror_ex(struct wmOperator *op, int totmirr, int totfail, char selectmode);

/**
 * Returns the pinned mesh, the mesh from the pinned object, or the mesh from the active object.
 */
struct Mesh *ED_mesh_context(struct bContext *C);

/**
 * Split all edges that would appear sharp based on face and edge sharpness tags and the
 * auto smooth angle.
 */
void ED_mesh_split_faces(struct Mesh *mesh);

/* mesh backup */
typedef struct BMBackup {
  struct BMesh *bmcopy;
} BMBackup;

/**
 * Save a copy of the #BMesh for restoring later.
 */
struct BMBackup EDBM_redo_state_store(struct BMEditMesh *em);
/**
 * Restore a BMesh from backup.
 */
void EDBM_redo_state_restore(struct BMBackup *backup, struct BMEditMesh *em, bool recalc_looptri)
    ATTR_NONNULL(1, 2);
/**
 * Delete the backup, flushing it to an edit-mesh.
 */
void EDBM_redo_state_restore_and_free(struct BMBackup *backup,
                                      struct BMEditMesh *em,
                                      bool recalc_looptri) ATTR_NONNULL(1, 2);
void EDBM_redo_state_free(struct BMBackup *backup) ATTR_NONNULL(1);

/* *** meshtools.cc *** */

int ED_mesh_join_objects_exec(struct bContext *C, struct wmOperator *op);
int ED_mesh_shapes_join_objects_exec(struct bContext *C, struct wmOperator *op);

/* mirror lookup api */

/* Spatial Mirror */
void ED_mesh_mirror_spatial_table_begin(struct Object *ob,
                                        struct BMEditMesh *em,
                                        struct Mesh *me_eval);
void ED_mesh_mirror_spatial_table_end(struct Object *ob);
int ED_mesh_mirror_spatial_table_lookup(struct Object *ob,
                                        struct BMEditMesh *em,
                                        struct Mesh *me_eval,
                                        const float co[3]);

/* Topology Mirror */

/**
 * Mode is 's' start, or 'e' end, or 'u' use if end, ob can be NULL.
 * \note This is supposed return -1 on error,
 * which callers are currently checking for, but is not used so far.
 */
void ED_mesh_mirror_topo_table_begin(struct Object *ob, struct Mesh *me_eval);
void ED_mesh_mirror_topo_table_end(struct Object *ob);

/**
 * Retrieves mirrored cache vert, or NULL if there isn't one.
 * \note calling this without ensuring the mirror cache state is bad.
 */
int mesh_get_x_mirror_vert(struct Object *ob, struct Mesh *me_eval, int index, bool use_topology);
struct BMVert *editbmesh_get_x_mirror_vert(struct Object *ob,
                                           struct BMEditMesh *em,
                                           struct BMVert *eve,
                                           const float co[3],
                                           int index,
                                           bool use_topology);
/**
 * This is a Mesh-based copy of #mesh_get_x_mirror_faces().
 */
int *mesh_get_x_mirror_faces(struct Object *ob, struct BMEditMesh *em, struct Mesh *me_eval);

/**
 * Wrapper for object-mode/edit-mode.
 *
 * call #BM_mesh_elem_table_ensure first for edit-mesh.
 */
int ED_mesh_mirror_get_vert(struct Object *ob, int index);

bool ED_mesh_pick_vert(struct bContext *C,
                       struct Object *ob,
                       const int mval[2],
                       uint dist_px,
                       bool use_zbuf,
                       uint *r_index);
/**
 * Face selection in object mode,
 * currently only weight-paint and vertex-paint use this.
 *
 * \return boolean true == Found
 */
bool ED_mesh_pick_face(
    struct bContext *C, struct Object *ob, const int mval[2], uint dist_px, uint *r_index);
/**
 * Use when the back buffer stores face index values. but we want a vert.
 * This gets the face then finds the closest vertex to mval.
 */
bool ED_mesh_pick_face_vert(
    struct bContext *C, struct Object *ob, const int mval[2], uint dist_px, uint *r_index);

struct MDeformVert *ED_mesh_active_dvert_get_em(struct Object *ob, struct BMVert **r_eve);
struct MDeformVert *ED_mesh_active_dvert_get_ob(struct Object *ob, int *r_index);
struct MDeformVert *ED_mesh_active_dvert_get_only(struct Object *ob);

void EDBM_mesh_stats_multi(struct Object **objects,
                           uint objects_len,
                           int totelem[3],
                           int totelem_sel[3]);
void EDBM_mesh_elem_index_ensure_multi(struct Object **objects, uint objects_len, char htype);

#define ED_MESH_PICK_DEFAULT_VERT_DIST 25
#define ED_MESH_PICK_DEFAULT_FACE_DIST 1

#define USE_LOOPSLIDE_HACK

#ifdef __cplusplus
}
#endif
