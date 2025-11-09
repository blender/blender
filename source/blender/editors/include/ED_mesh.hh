/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_attribute.hh"

#include "BLI_compiler_attrs.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_virtual_array.hh"

#include "DNA_windowmanager_enums.h"

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
struct KeyBlock;
struct Main;
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
struct wmKeyConfig;
struct wmOperator;
struct UvElement;
struct UvElementMap;

/* `editmesh_utils.cc` */
class EditMeshSymmetryHelper {
 public:
  static std::optional<EditMeshSymmetryHelper> create_if_needed(Object *ob, uchar htype);

  bool any_mirror_vert_selected(BMVert *v, char hflag) const;
  bool any_mirror_edge_selected(BMEdge *e, char hflag) const;
  bool any_mirror_face_selected(BMFace *f, char hflag) const;

  void set_hflag_on_mirror_verts(BMVert *v, char hflag, bool value) const;
  void set_hflag_on_mirror_edges(BMEdge *e, char hflag, bool value) const;
  void set_hflag_on_mirror_faces(BMFace *f, char hflag, bool value) const;

  void apply_on_mirror_verts(BMVert *v, blender::FunctionRef<void(BMVert *)> op) const;
  void apply_on_mirror_edges(BMEdge *e, blender::FunctionRef<void(BMEdge *)> op) const;
  void apply_on_mirror_faces(BMFace *f, blender::FunctionRef<void(BMFace *)> op) const;

 private:
  EditMeshSymmetryHelper(Object *ob, uchar htype);

  BMEditMesh *em_;
  Mesh *mesh_;
  uchar htype_;
  bool use_topology_mirror_;

  blender::Map<BMVert *, blender::Vector<BMVert *>> vert_to_mirror_map_;
  blender::Map<BMEdge *, blender::Vector<BMEdge *>> edge_to_mirror_map_;
  blender::Map<BMFace *, blender::Vector<BMFace *>> face_to_mirror_map_;
};

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
void EDBM_verts_mirror_cache_begin_ex(BMEditMesh *em,
                                      int axis,
                                      bool use_self,
                                      bool use_select,
                                      bool respecthide,
                                      bool use_topology,
                                      float maxdist,
                                      int *r_index);
void EDBM_verts_mirror_cache_begin(
    BMEditMesh *em, int axis, bool use_self, bool use_select, bool respecthide, bool use_topology);
void EDBM_verts_mirror_apply(BMEditMesh *em, int sel_from, int sel_to);
BMVert *EDBM_verts_mirror_get(BMEditMesh *em, BMVert *v);
BMEdge *EDBM_verts_mirror_get_edge(BMEditMesh *em, BMEdge *e);
BMFace *EDBM_verts_mirror_get_face(BMEditMesh *em, BMFace *f);
void EDBM_verts_mirror_cache_clear(BMEditMesh *em, BMVert *v);
void EDBM_verts_mirror_cache_end(BMEditMesh *em);

void EDBM_mesh_normals_update_ex(BMEditMesh *em, const BMeshNormalsUpdate_Params *params);
void EDBM_mesh_normals_update(BMEditMesh *em);

void EDBM_selectmode_to_scene(bContext *C);
void EDBM_mesh_make(Object *ob, int select_mode, bool add_key_index);
/** Replaces the edit-mesh in the object with a new one based on the given mesh. */
void EDBM_mesh_make_from_mesh(Object *ob, Mesh *src_mesh, int select_mode, bool add_key_index);
/**
 * Should only be called on the active edit-mesh, otherwise call #BKE_editmesh_free_data.
 */
void EDBM_mesh_free_data(BMEditMesh *em);
/**
 * \warning This can invalidate the #Mesh runtime cache of other objects (for linked duplicates).
 * Most callers should run #DEG_id_tag_update on `ob->data`, see: #46738, #46913.
 * This ensures #BKE_object_free_derived_caches runs on all objects that use this mesh.
 */
void EDBM_mesh_load_ex(Main *bmain, Object *ob, bool free_data);
void EDBM_mesh_load(Main *bmain, Object *ob);

/**
 * flushes based on the current select mode. If in vertex select mode,
 * verts select/deselect edges and faces, if in edge select mode,
 * edges select/deselect faces and vertices, and in face select mode faces select/deselect
 * edges and vertices.
 */
void EDBM_select_more(BMEditMesh *em, bool use_face_step);
void EDBM_select_less(BMEditMesh *em, bool use_face_step);

void EDBM_selectmode_flush_ex(BMEditMesh *em, short selectmode);
void EDBM_selectmode_flush(BMEditMesh *em);

/**
 * Mode independent selection/de-selection flush from vertices.
 *
 * \param select: When true, flush the selection state to de-selected elements,
 * otherwise perform the opposite, flushing de-selection.
 */
void EDBM_select_flush_from_verts(BMEditMesh *em, bool select);

bool EDBM_vert_color_check(BMEditMesh *em);

/**
 * Swap is 0 or 1, if 1 it hides not selected.
 */
bool EDBM_mesh_hide(BMEditMesh *em, bool swap);
bool EDBM_mesh_reveal(BMEditMesh *em, bool select);

struct EDBMUpdate_Params {
  uint calc_looptris : 1;
  uint calc_normals : 1;
  uint is_destructive : 1;
};

/**
 * So many tools call these that we better make it a generic function.
 */
void EDBM_update(Mesh *mesh, const EDBMUpdate_Params *params);
/**
 * Bad level call from Python API.
 */
void EDBM_update_extern(Mesh *mesh, bool do_tessellation, bool is_destructive);

/**
 * A specialized vert map used by stitch operator.
 */
UvElementMap *BM_uv_element_map_create(BMesh *bm,
                                       const Scene *scene,
                                       bool uv_selected,
                                       bool use_winding,
                                       bool use_seams,
                                       bool do_islands);
void BM_uv_element_map_free(UvElementMap *element_map);

/**
 * Return the #UvElement associated with a given #BMLoop, or NULL if no association exists.
 *
 * \param element_map: The #UvElementMap to look in.
 * \param l: The loop to search for.
 * \return The #UvElement associated with #l, or NULL if not found. (e.g. the vertex is hidden.)
 */
UvElement *BM_uv_element_get(const UvElementMap *element_map, const BMLoop *l);
UvElement *BM_uv_element_get_head(UvElementMap *element_map, UvElement *child);
int BM_uv_element_get_unique_index(UvElementMap *element_map, UvElement *child);

UvElement **BM_uv_element_map_ensure_head_table(UvElementMap *element_map);
int *BM_uv_element_map_ensure_unique_index(UvElementMap *element_map);

/**
 * Can we edit UVs for this mesh?
 */
bool EDBM_uv_check(BMEditMesh *em);
/**
 * last_sel, use em->act_face otherwise get the last selected face in the edit-selections
 * at the moment, last_sel is mainly useful for making sure the space image doesn't flicker.
 */
BMFace *EDBM_uv_active_face_get(BMEditMesh *em, bool sloppy, bool selected);

void BM_uv_vert_map_free(UvVertMap *vmap);
UvMapVert *BM_uv_vert_map_at_index(UvVertMap *vmap, unsigned int v);
/**
 * Return a new #UvVertMap from the edit-mesh.
 */
UvVertMap *BM_uv_vert_map_create(BMesh *bm, bool use_select, bool respect_hide);

void EDBM_flag_enable_all(BMEditMesh *em, char hflag);
void EDBM_flag_disable_all(BMEditMesh *em, char hflag);

/** \copydoc #BM_uvselect_clear */
bool EDBM_uvselect_clear(BMEditMesh *em);

bool BMBVH_EdgeVisible(const BMBVHTree *tree,
                       const BMEdge *e,
                       const Depsgraph *depsgraph,
                       const ARegion *region,
                       const View3D *v3d,
                       const Object *obedit);

void EDBM_project_snap_verts(
    bContext *C, Depsgraph *depsgraph, ARegion *region, Object *obedit, BMEditMesh *em);

/* `editmesh_automerge.cc` */

/** \return true if a change is made. */
bool EDBM_automerge(Object *obedit, bool update, char hflag, float dist);
/** \return true if a change is made. */
bool EDBM_automerge_connected(Object *obedit, bool update, char hflag, float dist);

/** \return true if a change is made. */
bool EDBM_automerge_and_split(
    Object *obedit, bool split_edges, bool split_faces, bool update, char hflag, float dist);

/* `editmesh_undo.cc` */

/** Export for ED_undo_sys. */
void ED_mesh_undosys_type(UndoType *ut);

/* `editmesh_select.cc` */

void EDBM_select_mirrored(
    BMEditMesh *em, const Mesh *mesh, int axis, bool extend, int *r_totmirr, int *r_totfail);

#if 0 /* Unused but seems useful to keep. */
/**
 * Select mirrored elements on all enabled axis.
 * Does nothing if selection symmetry isn't enabled.
 *
 * \return true if the selection changed.
 */
bool EDBM_select_mirrored_extend_all(Object *obedit, BMEditMesh *em);
#endif

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
BMVert *EDBM_vert_find_nearest_ex(ViewContext *vc,
                                  float *dist_px_manhattan_p,
                                  bool use_select_bias,
                                  bool use_cycle,
                                  blender::Span<Base *> bases,
                                  uint *r_base_index);
BMVert *EDBM_vert_find_nearest(ViewContext *vc, float *dist_px_manhattan_p);

BMEdge *EDBM_edge_find_nearest_ex(ViewContext *vc,
                                  float *dist_px_manhattan,
                                  float *r_dist_center,
                                  bool use_select_bias,
                                  bool use_cycle,
                                  BMEdge **r_eed_zbuf,
                                  blender::Span<Base *> bases,
                                  uint *r_base_index);
BMEdge *EDBM_edge_find_nearest(ViewContext *vc, float *dist_px_manhattan_p);

/**
 * \param use_zbuf_single_px: Special case, when using the back-buffer selection,
 * only use the pixel at `vc->mval` instead of using `dist_px_manhattan_p` to search over a larger
 * region. This is needed because historically selection worked this way for a long time, however
 * it's reasonable that some callers might want to expand the region too. So add an argument to do
 * this,
 */
BMFace *EDBM_face_find_nearest_ex(ViewContext *vc,
                                  float *dist_px_manhattan,
                                  float *r_dist_center,
                                  bool use_zbuf_single_px,
                                  bool use_select_bias,
                                  bool use_cycle,
                                  BMFace **r_efa_zbuf,
                                  blender::Span<Base *> bases,
                                  uint *r_base_index);
BMFace *EDBM_face_find_nearest(ViewContext *vc, float *dist_px_manhattan_p);

bool EDBM_unified_findnearest(ViewContext *vc,
                              blender::Span<Base *> bases,
                              int *r_base_index,
                              BMVert **r_eve,
                              BMEdge **r_eed,
                              BMFace **r_efa);

bool EDBM_unified_findnearest_from_raycast(ViewContext *vc,
                                           blender::Span<Base *> bases,
                                           bool use_boundary_vertices,
                                           bool use_boundary_edges,
                                           int *r_base_index_vert,
                                           int *r_base_index_edge,
                                           int *r_base_index_face,
                                           BMVert **r_eve,
                                           BMEdge **r_eed,
                                           BMFace **r_efa);

bool EDBM_select_pick(bContext *C, const int mval[2], const SelectPick_Params &params);

/**
 * When switching select mode, makes sure selection is consistent for editing
 * also for paranoia checks to make sure edge or face mode works.
 */
void EDBM_selectmode_set(BMEditMesh *em, short selectmode);
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
void EDBM_selectmode_convert(BMEditMesh *em, short selectmode_old, short selectmode_new);

/**
 * Select-mode setting utility.
 * This operates on tool-settings and all objects passed in.
 */
bool EDBM_selectmode_set_multi_ex(Scene *scene,
                                  blender::Span<Object *> objects,
                                  const short selectmode);
/**
 * High level select-mode setting utility.
 * This operates on tool-settings and all edit-mode objects.
 */
bool EDBM_selectmode_set_multi(bContext *C, short selectmode);
/**
 * User facing function, handles notification.
 *
 * \param selectmode_toggle: The mode to adjust based on `action`, must not contain mixed flags.
 */
bool EDBM_selectmode_toggle_multi(
    bContext *C, short selectmode_toggle, int action, bool use_extend, bool use_expand);

/**
 * Use to disable a select-mode if its enabled, Using another mode as a fallback
 * if the disabled mode is the only mode set.
 *
 * \return true if the mode is changed.
 */
bool EDBM_selectmode_disable(Scene *scene,
                             BMEditMesh *em,
                             short selectmode_disable,
                             short selectmode_fallback);

bool EDBM_deselect_by_material(BMEditMesh *em, short index, bool select);

void EDBM_select_toggle_all(BMEditMesh *em);

void EDBM_select_swap(BMEditMesh *em); /* exported for UV */
bool EDBM_select_interior_faces(BMEditMesh *em);
ViewContext em_setup_viewcontext(bContext *C); /* rename? */

bool EDBM_mesh_deselect_all_multi_ex(blender::Span<Base *> bases);
bool EDBM_mesh_deselect_all_multi(bContext *C);
bool EDBM_selectmode_disable_multi_ex(Scene *scene,
                                      blender::Span<Base *> bases,
                                      short selectmode_disable,
                                      short selectmode_fallback);
bool EDBM_selectmode_disable_multi(bContext *C,
                                   short selectmode_disable,
                                   short selectmode_fallback);

/* `editmesh_preselect_edgering.cc` */

struct EditMesh_PreSelEdgeRing;
EditMesh_PreSelEdgeRing *EDBM_preselect_edgering_create();
void EDBM_preselect_edgering_destroy(EditMesh_PreSelEdgeRing *psel);
void EDBM_preselect_edgering_clear(EditMesh_PreSelEdgeRing *psel);
void EDBM_preselect_edgering_draw(EditMesh_PreSelEdgeRing *psel, const float matrix[4][4]);
void EDBM_preselect_edgering_update_from_edge(EditMesh_PreSelEdgeRing *psel,
                                              BMesh *bm,
                                              BMEdge *eed_start,
                                              int previewlines,
                                              blender::Span<blender::float3> vert_positions);

/* `editmesh_preselect_elem.cc` */

struct EditMesh_PreSelElem;
enum eEditMesh_PreSelPreviewAction {
  PRESELECT_ACTION_TRANSFORM = 1,
  PRESELECT_ACTION_CREATE = 2,
  PRESELECT_ACTION_DELETE = 3,
};

EditMesh_PreSelElem *EDBM_preselect_elem_create();
void EDBM_preselect_elem_destroy(EditMesh_PreSelElem *psel);
void EDBM_preselect_elem_clear(EditMesh_PreSelElem *psel);
void EDBM_preselect_preview_clear(EditMesh_PreSelElem *psel);
void EDBM_preselect_elem_draw(EditMesh_PreSelElem *psel, const float matrix[4][4]);
void EDBM_preselect_elem_update_from_single(EditMesh_PreSelElem *psel,
                                            BMesh *bm,
                                            BMElem *ele,
                                            blender::Span<blender::float3> vert_positions);

void EDBM_preselect_elem_update_preview(
    EditMesh_PreSelElem *psel, ViewContext *vc, BMesh *bm, BMElem *ele, const int mval[2]);
void EDBM_preselect_action_set(EditMesh_PreSelElem *psel, eEditMesh_PreSelPreviewAction action);
eEditMesh_PreSelPreviewAction EDBM_preselect_action_get(EditMesh_PreSelElem *psel);

/* `mesh_ops.cc` */

void ED_operatortypes_mesh();
void ED_operatormacros_mesh();
/**
 * Note mesh keymap also for other space?
 */
void ED_keymap_mesh(wmKeyConfig *keyconf);

/* `editface.cc` */

/**
 * Copy the face flags, most importantly selection from the mesh to the final evaluated mesh,
 * use in object mode when selecting faces (while painting).
 */
void paintface_flush_flags(bContext *C, Object *ob, bool flush_selection, bool flush_hidden);
/**
 * \return True when pick finds an element or the selection changed.
 */
bool paintface_mouse_select(bContext *C,
                            const int mval[2],
                            const SelectPick_Params &params,
                            Object *ob);
bool paintface_deselect_all_visible(bContext *C, Object *ob, int action, bool flush_flags);
void paintface_select_linked(bContext *C, Object *ob, const int mval[2], bool select);

void paintface_select_loop(bContext *C, Object *ob, const int mval[2], bool select);
/**
 * Grow the selection of faces.
 * \param face_step: If true will also select faces that only touch on the corner.
 */
void paintface_select_more(Mesh *mesh, bool face_step);
void paintface_select_less(Mesh *mesh, bool face_step);
bool paintface_minmax(Object *ob, float r_min[3], float r_max[3]);

void paintface_hide(bContext *C, Object *ob, bool unselected);
void paintface_reveal(bContext *C, Object *ob, bool select);

/**
 * \note if the caller passes false to flush_flags,
 * then they will need to run #paintvert_flush_flags(ob) themselves.
 */
bool paintvert_deselect_all_visible(Object *ob, int action, bool flush_flags);
void paintvert_select_ungrouped(Object *ob, bool extend, bool flush_flags);
/**
 * (similar to void `paintface_flush_flags(Object *ob)`)
 * copy the vertex flags, most importantly selection from the mesh to the final evaluated mesh,
 * use in object mode when selecting vertices (while painting).
 */
void paintvert_flush_flags(Object *ob);
void paintvert_tag_select_update(bContext *C, Object *ob);
/* Select vertices that are connected to already selected vertices. */
void paintvert_select_linked(bContext *C, Object *ob);
/* Select vertices that are linked to the vertex under the given region space coordinates. */
void paintvert_select_linked_pick(bContext *C,
                                  Object *ob,
                                  const int region_coordinates[2],
                                  bool select);
void paintvert_select_more(Mesh *mesh, bool face_step);
void paintvert_select_less(Mesh *mesh, bool face_step);
void paintvert_hide(bContext *C, Object *ob, bool unselected);
void paintvert_reveal(bContext *C, Object *ob, bool select);

/* mirrtopo */
struct MirrTopoStore_t {
  intptr_t *index_lookup;
  int prev_vert_tot;
  int prev_edge_tot;
  bool prev_is_editmode;
};

bool ED_mesh_mirrtopo_recalc_check(BMEditMesh *em, Mesh *mesh, MirrTopoStore_t *mesh_topo_store);
void ED_mesh_mirrtopo_init(BMEditMesh *em,
                           Mesh *mesh,
                           MirrTopoStore_t *mesh_topo_store,
                           bool skip_em_vert_array_init);
void ED_mesh_mirrtopo_free(MirrTopoStore_t *mesh_topo_store);

/* `mesh_data.cc` */

void ED_mesh_verts_add(Mesh *mesh, ReportList *reports, int count);
void ED_mesh_edges_add(Mesh *mesh, ReportList *reports, int count);
void ED_mesh_loops_add(Mesh *mesh, ReportList *reports, int count);
void ED_mesh_faces_add(Mesh *mesh, ReportList *reports, int count);

void ED_mesh_verts_remove(Mesh *mesh, ReportList *reports, int count);
void ED_mesh_edges_remove(Mesh *mesh, ReportList *reports, int count);
void ED_mesh_loops_remove(Mesh *mesh, ReportList *reports, int count);
void ED_mesh_faces_remove(Mesh *mesh, ReportList *reports, int count);

void ED_mesh_geometry_clear(Mesh *mesh);

blender::bke::AttributeWriter<bool> ED_mesh_uv_map_pin_layer_ensure(Mesh *mesh, int uv_index);
blender::VArray<bool> ED_mesh_uv_map_pin_layer_get(const Mesh *mesh, int uv_index);

void ED_mesh_uv_ensure(Mesh *mesh, const char *name);
int ED_mesh_uv_add(
    Mesh *mesh, const char *name, bool active_set, bool do_init, ReportList *reports);

void ED_mesh_uv_loop_reset(bContext *C, Mesh *mesh);
bool ED_mesh_color_ensure(Mesh *mesh, const char *name);
std::string ED_mesh_color_add(
    Mesh *mesh, const char *name, bool active_set, bool do_init, ReportList *reports);

void ED_mesh_report_mirror(ReportList &reports, int totmirr, int totfail);
void ED_mesh_report_mirror_ex(ReportList &reports, int totmirr, int totfail, char selectmode);

KeyBlock *ED_mesh_get_edit_shape_key(const Mesh *me);

/**
 * Returns the pinned mesh, the mesh from the pinned object, or the mesh from the active object.
 */
Mesh *ED_mesh_context(bContext *C);

/**
 * Split all edges that would appear sharp based on face and edge sharpness tags and the
 * auto smooth angle.
 */
void ED_mesh_split_faces(Mesh *mesh);

/* mesh backup */
struct BMBackup {
  BMesh *bmcopy;
};

/**
 * Save a copy of the #BMesh for restoring later.
 */
BMBackup EDBM_redo_state_store(BMEditMesh *em);
/**
 * Restore a BMesh from backup.
 */
void EDBM_redo_state_restore(BMBackup *backup, BMEditMesh *em, bool recalc_looptris)
    ATTR_NONNULL(1, 2);
/**
 * Delete the backup, flushing it to an edit-mesh.
 */
void EDBM_redo_state_restore_and_free(BMBackup *backup, BMEditMesh *em, bool recalc_looptris)
    ATTR_NONNULL(1, 2);
void EDBM_redo_state_free(BMBackup *backup) ATTR_NONNULL(1);

namespace blender::ed::mesh {

wmOperatorStatus join_objects_exec(bContext *C, wmOperator *op);

}

/* `meshtools.cc` */

wmOperatorStatus ED_mesh_shapes_join_objects_exec(bContext *C,
                                                  bool ensure_keys_exist,
                                                  bool mirror,
                                                  ReportList *reports);

/* Mirror lookup API. */

/* Spatial Mirror */
void ED_mesh_mirror_spatial_table_begin(Object *ob, BMEditMesh *em, Mesh *mesh_eval);
void ED_mesh_mirror_spatial_table_end(Object *ob);
int ED_mesh_mirror_spatial_table_lookup(Object *ob,
                                        BMEditMesh *em,
                                        Mesh *mesh_eval,
                                        const float co[3]);

/* Topology Mirror */

/**
 * Mode is 's' start, or 'e' end, or 'u' use if end, ob can be NULL.
 * \note This is supposed return -1 on error,
 * which callers are currently checking for, but is not used so far.
 */
void ED_mesh_mirror_topo_table_begin(Object *ob, Mesh *mesh_eval);
void ED_mesh_mirror_topo_table_end(Object *ob);

/**
 * Retrieves mirrored cache vert, or NULL if there isn't one.
 * \note calling this without ensuring the mirror cache state is bad.
 */
int mesh_get_x_mirror_vert(Object *ob, Mesh *mesh_eval, int index, bool use_topology);
BMVert *editbmesh_get_x_mirror_vert(
    Object *ob, BMEditMesh *em, BMVert *eve, const float co[3], int index, bool use_topology);
/**
 * This is a Mesh-based copy of #mesh_get_x_mirror_faces().
 */
int *mesh_get_x_mirror_faces(Object *ob, BMEditMesh *em, Mesh *mesh_eval);

/**
 * Wrapper for object-mode/edit-mode.
 *
 * call #BM_mesh_elem_table_ensure first for edit-mesh.
 */
int ED_mesh_mirror_get_vert(Object *ob, int index);

bool ED_mesh_pick_vert(
    bContext *C, Object *ob, const int mval[2], uint dist_px, bool use_zbuf, uint *r_index);
/**
 * Face selection in object mode,
 * currently only weight-paint and vertex-paint use this.
 *
 * \return boolean true == Found
 */
bool ED_mesh_pick_face(bContext *C, Object *ob, const int mval[2], uint dist_px, uint *r_index);
/**
 * Use when the back buffer stores face index values. but we want a vert.
 * This gets the face then finds the closest vertex to mval.
 */
bool ED_mesh_pick_face_vert(
    bContext *C, Object *ob, const int mval[2], uint dist_px, uint *r_index);
/**
 * Used for paint face loop selection which needs to get closest edge even though in face select
 * mode. Changes the select_buffer context to edge selection for this.
 */
bool ED_mesh_pick_edge(bContext *C, Object *ob, const int mval[2], uint dist_px, uint *r_index);

MDeformVert *ED_mesh_active_dvert_get_em(Object *ob, BMVert **r_eve);
MDeformVert *ED_mesh_active_dvert_get_ob(Object *ob, int *r_index);
MDeformVert *ED_mesh_active_dvert_get_only(Object *ob);

void EDBM_mesh_stats_multi(blender::Span<Object *> objects, int totelem[3], int totelem_sel[3]);
void EDBM_mesh_elem_index_ensure_multi(blender::Span<Object *> objects, char htype);

#define ED_MESH_PICK_DEFAULT_VERT_DIST 25
#define ED_MESH_PICK_DEFAULT_FACE_DIST 1

#define USE_LOOPSLIDE_HACK
