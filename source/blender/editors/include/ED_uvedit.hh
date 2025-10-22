/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_vector_list.hh"

#include "BKE_customdata.hh"

struct ARegion;
struct ARegionType;
struct BMEdge;
struct BMEditMesh;
struct BMFace;
struct BMLoop;
struct BMVert;
struct BMesh;
struct Image;
struct ImageUser;
struct ListBase;
struct Main;
struct Object;
struct Scene;
struct SpaceImage;
struct ToolSettings;
struct View2D;
struct ViewLayer;
struct bContext;
struct bNode;
struct bNodeTree;
struct wmKeyConfig;
struct wmTimer;

/* `uvedit_ops.cc` */

void ED_operatortypes_uvedit();
void ED_operatormacros_uvedit();
void ED_keymap_uvedit(wmKeyConfig *keyconf);

/**
 * Be careful when using this, it bypasses all synchronization options.
 */
void ED_uvedit_select_all(const ToolSettings *ts, BMesh *bm);

void ED_uvedit_foreach_uv(const Scene *scene,
                          BMesh *bm,
                          const bool skip_invisible,
                          const bool selected,
                          blender::FunctionRef<void(float[2])> user_fn);
void ED_uvedit_foreach_uv_multi(const Scene *scene,
                                blender::Span<Object *> objects_edit,
                                const bool skip_invisible,
                                const bool skip_nonselected,
                                blender::FunctionRef<void(float[2])> user_fn);
bool ED_uvedit_minmax_multi(const Scene *scene,
                            blender::Span<Object *> objects_edit,
                            float r_min[2],
                            float r_max[2]);
bool ED_uvedit_center_multi(const Scene *scene,
                            blender::Span<Object *> objects_edit,
                            float r_cent[2],
                            char mode);

bool ED_uvedit_center_from_pivot_ex(const SpaceImage *sima,
                                    Scene *scene,
                                    ViewLayer *view_layer,
                                    float r_center[2],
                                    char mode,
                                    bool *r_has_select);

bool ED_object_get_active_image(Object *ob,
                                int mat_nr,
                                Image **r_ima,
                                ImageUser **r_iuser,
                                const bNode **r_node,
                                const bNodeTree **r_ntree);
void ED_object_assign_active_image(Main *bmain, Object *ob, int mat_nr, Image *ima);

bool ED_uvedit_test(Object *obedit);

/* `uvedit_select.cc` */

namespace blender::ed::uv {

/**
 * Abstract away the details of syncing selection from the mesh (viewport)
 * to a UV state which is "synchronized".
 *
 * Where practical (see note below) this is a preferred alternative to clearing the
 * UV selection state and re-initializing it from the mesh, because there may be UV's
 * selected on one UV island and not another, even though the vertices are shared.
 * Flushing and re-initializing will set both, losing the users selection.
 *
 * Note that what is considered practical is open to interpretation,
 * picking individual elements and basic selection actions should be supported.
 * Selection actions such as random or by vertex group... isn't so practical.
 */
class UVSyncSelectFromMesh : NonCopyable {
 private:
  char uv_sticky_;
  BMesh &bm_;

  blender::VectorList<BMVert *> bm_verts_select_;
  blender::VectorList<BMEdge *> bm_edges_select_;
  blender::VectorList<BMFace *> bm_faces_select_;

  blender::VectorList<BMVert *> bm_verts_deselect_;
  blender::VectorList<BMEdge *> bm_edges_deselect_;
  blender::VectorList<BMFace *> bm_faces_deselect_;

 public:
  UVSyncSelectFromMesh(BMesh &bm, char uv_sticky) : uv_sticky_(uv_sticky), bm_(bm) {}
  UVSyncSelectFromMesh(const UVSyncSelectFromMesh &) = delete;

  static std::unique_ptr<UVSyncSelectFromMesh> create_if_needed(const ToolSettings &ts, BMesh &bm);
  void apply();

  /* Select. */

  void vert_select_enable(BMVert *v);
  void edge_select_enable(BMEdge *f);
  void face_select_enable(BMFace *f);

  /* De-Select. */

  void vert_select_disable(BMVert *v);
  void edge_select_disable(BMEdge *f);
  void face_select_disable(BMFace *f);

  /* Select set. */

  void vert_select_set(BMVert *v, bool value);
  void edge_select_set(BMEdge *f, bool value);
  void face_select_set(BMFace *f, bool value);
};

}  // namespace blender::ed::uv

bool ED_uvedit_sync_uvselect_ignore(const ToolSettings *ts);
bool ED_uvedit_sync_uvselect_is_valid_or_ignore(const ToolSettings *ts, const BMesh *bm);
void ED_uvedit_sync_uvselect_ensure_if_needed(const ToolSettings *ts, BMesh *bm);

/* Visibility and selection tests. */

bool uvedit_face_visible_test_ex(const ToolSettings *ts, const BMFace *efa);
bool uvedit_face_select_test_ex(const ToolSettings *ts, const BMesh *bm, const BMFace *efa);

bool uvedit_edge_select_test_ex(const ToolSettings *ts,
                                const BMesh *bm,
                                const BMLoop *l,
                                const BMUVOffsets &offsets);
bool uvedit_uv_select_test_ex(const ToolSettings *ts,
                              const BMesh *bm,
                              const BMLoop *l,
                              const BMUVOffsets &offsets);

bool uvedit_face_visible_test(const Scene *scene, const BMFace *efa);
bool uvedit_face_select_test(const Scene *scene, const BMesh *bm, const BMFace *efa);
bool uvedit_edge_select_test(const Scene *scene,
                             const BMesh *bm,
                             const BMLoop *l,
                             const BMUVOffsets &offsets);
bool uvedit_uv_select_test(const Scene *scene,
                           const BMesh *bm,
                           const BMLoop *l,
                           const BMUVOffsets &offsets);

/* Low level loop selection, this ignores the selection modes. */

bool uvedit_loop_vert_select_get(const ToolSettings *ts, const BMesh *bm, const BMLoop *l);
bool uvedit_loop_edge_select_get(const ToolSettings *ts, const BMesh *bm, const BMLoop *l);
void uvedit_loop_vert_select_set(const ToolSettings *ts,
                                 const BMesh *bm,
                                 BMLoop *l,
                                 const bool select);
void uvedit_loop_edge_select_set(const ToolSettings *ts,
                                 const BMesh *bm,
                                 BMLoop *l,
                                 const bool select);

/* Individual UV element selection functions. */

/**
 * \brief Select UV Face
 *
 * Changes selection state of a single UV Face.
 */
void uvedit_face_select_set(const Scene *scene, BMesh *bm, BMFace *efa, bool select);
/**
 * \brief Select UV Edge
 *
 * Changes selection state of a single UV Edge.
 */
void uvedit_edge_select_set(const Scene *scene, BMesh *bm, BMLoop *l, bool select);
/**
 * \brief Select UV Vertex
 *
 * Changes selection state of a single UV vertex.
 */
void uvedit_uv_select_set(const Scene *scene, BMesh *bm, BMLoop *l, bool select);

/* Low level functions for (de)selecting individual UV elements. Ensure UV face visibility before
 * use. */

void uvedit_face_select_enable(const Scene *scene, BMesh *bm, BMFace *efa);
void uvedit_face_select_disable(const Scene *scene, BMesh *bm, BMFace *efa);

void uvedit_edge_select_enable(const Scene *scene, BMesh *bm, BMLoop *l);
void uvedit_edge_select_disable(const Scene *scene, BMesh *bm, BMLoop *l);

void uvedit_uv_select_enable(const Scene *scene, BMesh *bm, BMLoop *l);
void uvedit_uv_select_disable(const Scene *scene, BMesh *bm, BMLoop *l);

/* Sticky mode UV element selection functions. */

void uvedit_face_select_set_with_sticky(
    const Scene *scene, BMesh *bm, BMFace *efa, bool select, const BMUVOffsets &offsets);
void uvedit_edge_select_set_with_sticky(
    const Scene *scene, BMesh *bm, BMLoop *l, bool select, const BMUVOffsets &offsets);

void uvedit_uv_select_set_with_sticky(
    const Scene *scene, BMesh *bm, BMLoop *l, bool select, const BMUVOffsets &offsets);

/* Low level functions for sticky element selection (sticky mode independent). Type of sticky
 * selection is specified explicitly (using sticky_flag, except for face selection). */

void uvedit_face_select_shared_vert(
    const Scene *scene, BMesh *bm, BMFace *efa, const bool select, const BMUVOffsets &offsets);
/**
 * Selects UV edges and shared vertices according to sticky_flag.
 *
 * \param sticky_flag:
 * - #UV_STICKY_LOCATION: selects all UV edges that share the same mesh vertices and UV coords.
 * - #UV_STICKY_VERT: selects all UV edges sharing the same mesh vertices.
 */
void uvedit_edge_select_shared_vert(const Scene *scene,
                                    BMesh *bm,
                                    BMLoop *l,
                                    const bool select,
                                    const int sticky_flag,
                                    const BMUVOffsets &offsets);
/**
 * Selects shared UVs based on #sticky_flag.
 *
 * \param sticky_flag: Type of sticky selection:
 * - #UV_STICKY_LOCATION: selects all UVs sharing same mesh vertex and UV coords.
 * - #UV_STICKY_VERT: selects all UVs sharing same mesh vertex.
 */
void uvedit_uv_select_shared_vert(const Scene *scene,
                                  BMesh *bm,
                                  BMLoop *l,
                                  const bool select,
                                  const int sticky_flag,
                                  const BMUVOffsets &offsets);

/**
 * Sets required UV edge flags as specified by the `sticky_flag`.
 */
void uvedit_edge_select_set_noflush(const Scene *scene,
                                    BMesh *bm,
                                    BMLoop *l,
                                    const bool select,
                                    const int sticky_flag,
                                    const BMUVOffsets &offsets);

/**
 * \brief UV Select Mode set
 *
 * Updates selection state for UVs based on the select mode and sticky mode. Similar to
 * #EDBM_selectmode_set.
 */
void ED_uvedit_selectmode_clean(const Scene *scene, Object *obedit);
void ED_uvedit_selectmode_clean_multi(bContext *C);
void ED_uvedit_select_sync_multi(bContext *C);
void ED_uvedit_sticky_selectmode_update(bContext *C);

/**
 * \brief UV Select Mode Flush
 *
 * Flushes selections upwards as dictated by the UV select mode.
 */
void ED_uvedit_selectmode_flush(const Scene *scene, BMesh *bm);

/**
 * Mode independent UV selection/de-selection flush from vertices.
 *
 * \param select: When true, flush the selection state to de-selected elements,
 * otherwise perform the opposite, flushing de-selection.
 */
void uvedit_select_flush_from_verts(const Scene *scene, BMesh *bm, bool select);

bool ED_uvedit_nearest_uv_multi(const View2D *v2d,
                                const Scene *scene,
                                blender::Span<Object *> objects,
                                const float mval_fl[2],
                                const bool ignore_selected,
                                float *dist_sq,
                                float r_uv[2]);

BMFace **ED_uvedit_selected_faces(const Scene *scene, BMesh *bm, int len_max, int *r_faces_len);
BMLoop **ED_uvedit_selected_edges(const Scene *scene, BMesh *bm, int len_max, int *r_edges_len);
BMLoop **ED_uvedit_selected_verts(const Scene *scene, BMesh *bm, int len_max, int *r_verts_len);

void ED_uvedit_active_vert_loop_set(BMesh *bm, BMLoop *l);
BMLoop *ED_uvedit_active_vert_loop_get(const ToolSettings *ts, BMesh *bm);

void ED_uvedit_active_edge_loop_set(BMesh *bm, BMLoop *l);
BMLoop *ED_uvedit_active_edge_loop_get(const ToolSettings *ts, BMesh *bm);

/**
 * Intentionally don't return #UV_SELECT_ISLAND as it's not an element type.
 * In this case return #UV_SELECT_VERT as a fallback.
 */
char ED_uvedit_select_mode_get(const Scene *scene);
bool ED_uvedit_select_island_check(const ToolSettings *ts);
void ED_uvedit_select_sync_flush(const ToolSettings *ts, BMesh *bm, bool select);

/* `uvedit_unwrap_ops.cc` */

void ED_uvedit_deselect_all(const Scene *scene, Object *obedit, int action);

void ED_uvedit_get_aspect(Object *obedit, float *r_aspx, float *r_aspy);

/**
 * Return the X / Y aspect (wider aspects are over 1, taller are below 1).
 * Apply this aspect by multiplying with the Y axis (X aspect is always 1 & unchanged).
 */
float ED_uvedit_get_aspect_y(Object *obedit);

void ED_uvedit_get_aspect_from_material(Object *ob,
                                        const int material_index,
                                        float *r_aspx,
                                        float *r_aspy);

/** Return true if the timer is managed by live-unwrap. */
bool ED_uvedit_live_unwrap_timer_check(const wmTimer *timer);

/**
 * \param win_modal: Support interactive (modal) unwrapping that updates with a timer.
 */
void ED_uvedit_live_unwrap_begin(Scene *scene, Object *obedit, struct wmWindow *win_modal);
void ED_uvedit_live_unwrap_re_solve();
void ED_uvedit_live_unwrap_end(bool cancel);

void ED_uvedit_live_unwrap(const Scene *scene, blender::Span<Object *> objects);
void ED_uvedit_add_simple_uvs(Main *bmain, const Scene *scene, Object *ob);

/* `uvedit_draw.cc` */

void ED_image_draw_cursor(ARegion *region, const float cursor[2]);

/* `uvedit_buttons.cc` */

void ED_uvedit_buttons_register(ARegionType *art);

/* `uvedit_islands.cc` */

struct FaceIsland {
  FaceIsland *next;
  FaceIsland *prev;
  BMFace **faces;
  int faces_len;
  /**
   * \note While this is duplicate information,
   * it allows islands from multiple meshes to be stored in the same list.
   */
  BMUVOffsets offsets;
  float aspect_y;
};

/**
 * Calculate islands and add them to \a island_list returning the number of items added.
 */
int bm_mesh_calc_uv_islands(const Scene *scene,
                            BMesh *bm,
                            ListBase *island_list,
                            const bool only_selected_faces,
                            const bool only_selected_uvs,
                            const bool use_seams,
                            const float aspect_y,
                            const BMUVOffsets &offsets);

/**
 * Returns true if UV coordinates lie on a valid tile in UDIM grid or tiled image.
 */
bool uv_coords_isect_udim(const Image *image, const int udim_grid[2], const float coords[2]);
