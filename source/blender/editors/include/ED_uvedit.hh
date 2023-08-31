/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_customdata.h"

struct ARegion;
struct ARegionType;
struct BMEditMesh;
struct BMFace;
struct BMLoop;
struct BMesh;
struct Image;
struct ImageUser;
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

/* `uvedit_ops.cc` */

void ED_operatortypes_uvedit();
void ED_operatormacros_uvedit();
void ED_keymap_uvedit(wmKeyConfig *keyconf);

/**
 * Be careful when using this, it bypasses all synchronization options.
 */
void ED_uvedit_select_all(BMesh *bm);

bool ED_uvedit_minmax_multi(
    const Scene *scene, Object **objects_edit, uint objects_len, float r_min[2], float r_max[2]);
bool ED_uvedit_center_multi(
    const Scene *scene, Object **objects_edit, uint objects_len, float r_cent[2], char mode);

bool ED_uvedit_center_from_pivot_ex(SpaceImage *sima,
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

/* Visibility and selection tests. */

bool uvedit_face_visible_test_ex(const ToolSettings *ts, BMFace *efa);
bool uvedit_face_select_test_ex(const ToolSettings *ts, BMFace *efa, BMUVOffsets offsets);

bool uvedit_edge_select_test_ex(const ToolSettings *ts, BMLoop *l, BMUVOffsets offsets);
bool uvedit_uv_select_test_ex(const ToolSettings *ts, BMLoop *l, BMUVOffsets offsets);

bool uvedit_face_visible_test(const Scene *scene, BMFace *efa);
bool uvedit_face_select_test(const Scene *scene, BMFace *efa, BMUVOffsets offsets);
bool uvedit_edge_select_test(const Scene *scene, BMLoop *l, BMUVOffsets offsets);
bool uvedit_uv_select_test(const Scene *scene, BMLoop *l, BMUVOffsets offsets);

/* Individual UV element selection functions. */

/**
 * \brief Select UV Face
 *
 * Changes selection state of a single UV Face.
 */
void uvedit_face_select_set(
    const Scene *scene, BMesh *bm, BMFace *efa, bool select, bool do_history, BMUVOffsets offsets);
/**
 * \brief Select UV Edge
 *
 * Changes selection state of a single UV Edge.
 */
void uvedit_edge_select_set(
    const Scene *scene, BMesh *bm, BMLoop *l, bool select, bool do_history, BMUVOffsets offsets);
/**
 * \brief Select UV Vertex
 *
 * Changes selection state of a single UV vertex.
 */
void uvedit_uv_select_set(
    const Scene *scene, BMesh *bm, BMLoop *l, bool select, bool do_history, BMUVOffsets offsets);

/* Low level functions for (de)selecting individual UV elements. Ensure UV face visibility before
 * use. */

void uvedit_face_select_enable(
    const Scene *scene, BMesh *bm, BMFace *efa, bool do_history, BMUVOffsets offsets);
void uvedit_face_select_disable(const Scene *scene, BMesh *bm, BMFace *efa, BMUVOffsets offsets);

void uvedit_edge_select_enable(
    const Scene *scene, BMesh *bm, BMLoop *l, bool do_history, BMUVOffsets offsets);
void uvedit_edge_select_disable(const Scene *scene, BMesh *bm, BMLoop *l, BMUVOffsets offsets);

void uvedit_uv_select_enable(
    const Scene *scene, BMesh *bm, BMLoop *l, bool do_history, BMUVOffsets offsets);
void uvedit_uv_select_disable(const Scene *scene, BMesh *bm, BMLoop *l, BMUVOffsets offsets);

/* Sticky mode UV element selection functions. */

void uvedit_face_select_set_with_sticky(const Scene *scene,
                                        BMEditMesh *em,
                                        BMFace *efa,
                                        bool select,
                                        bool do_history,
                                        BMUVOffsets offsets);
void uvedit_edge_select_set_with_sticky(const Scene *scene,
                                        BMEditMesh *em,
                                        BMLoop *l,
                                        bool select,
                                        bool do_history,
                                        BMUVOffsets offsets);

void uvedit_uv_select_set_with_sticky(const Scene *scene,
                                      BMEditMesh *em,
                                      BMLoop *l,
                                      bool select,
                                      bool do_history,
                                      BMUVOffsets offsets);

/* Low level functions for sticky element selection (sticky mode independent). Type of sticky
 * selection is specified explicitly (using sticky_flag, except for face selection). */

void uvedit_face_select_shared_vert(const Scene *scene,
                                    BMEditMesh *em,
                                    BMFace *efa,
                                    const bool select,
                                    const bool do_history,
                                    BMUVOffsets offsets);
/**
 * Selects UV edges and shared vertices according to sticky_flag.
 *
 * \param sticky_flag:
 * - #SI_STICKY_LOC: selects all UV edges that share the same mesh vertices and UV coordinates.
 * - #SI_STICKY_VERTEX: selects all UV edges sharing the same mesh vertices.
 */
void uvedit_edge_select_shared_vert(const Scene *scene,
                                    BMEditMesh *em,
                                    BMLoop *l,
                                    const bool select,
                                    const int sticky_flag,
                                    const bool do_history,
                                    BMUVOffsets offsets);
/**
 * Selects shared UVs based on #sticky_flag.
 *
 * \param sticky_flag: Type of sticky selection:
 * - #SI_STICKY_LOC: selects all UVs sharing same mesh vertex and UV coordinates.
 * - #SI_STICKY_VERTEX: selects all UVs sharing same mesh vertex.
 */
void uvedit_uv_select_shared_vert(const Scene *scene,
                                  BMEditMesh *em,
                                  BMLoop *l,
                                  const bool select,
                                  const int sticky_flag,
                                  const bool do_history,
                                  BMUVOffsets offsets);

/**
 * Sets required UV edge flags as specified by the `sticky_flag`.
 */
void uvedit_edge_select_set_noflush(
    const Scene *scene, BMLoop *l, const bool select, const int sticky_flag, BMUVOffsets offsets);

/**
 * \brief UV Select Mode set
 *
 * Updates selection state for UVs based on the select mode and sticky mode. Similar to
 * #EDBM_selectmode_set.
 */
void ED_uvedit_selectmode_clean(const Scene *scene, Object *obedit);
void ED_uvedit_selectmode_clean_multi(bContext *C);

/**
 * \brief UV Select Mode Flush
 *
 * Flushes selections upwards as dictated by the UV select mode.
 */
void ED_uvedit_selectmode_flush(const Scene *scene, BMEditMesh *em);

/**
 * Mode independent UV de-selection flush.
 */
void uvedit_deselect_flush(const Scene *scene, BMEditMesh *em);
/**
 * Mode independent UV selection flush.
 */
void uvedit_select_flush(const Scene *scene, BMEditMesh *em);

bool ED_uvedit_nearest_uv_multi(const View2D *v2d,
                                const Scene *scene,
                                Object **objects,
                                uint objects_len,
                                const float mval_fl[2],
                                const bool ignore_selected,
                                float *dist_sq,
                                float r_uv[2]);

BMFace **ED_uvedit_selected_faces(const Scene *scene, BMesh *bm, int len_max, int *r_faces_len);
BMLoop **ED_uvedit_selected_edges(const Scene *scene, BMesh *bm, int len_max, int *r_edges_len);
BMLoop **ED_uvedit_selected_verts(const Scene *scene, BMesh *bm, int len_max, int *r_verts_len);

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

void ED_uvedit_active_vert_loop_set(BMesh *bm, BMLoop *l);
BMLoop *ED_uvedit_active_vert_loop_get(BMesh *bm);

void ED_uvedit_active_edge_loop_set(BMesh *bm, BMLoop *l);
BMLoop *ED_uvedit_active_edge_loop_get(BMesh *bm);

/**
 * Intentionally don't return #UV_SELECT_ISLAND as it's not an element type.
 * In this case return #UV_SELECT_VERTEX as a fallback.
 */
char ED_uvedit_select_mode_get(const Scene *scene);
void ED_uvedit_select_sync_flush(const ToolSettings *ts, BMEditMesh *em, bool select);

/* `uvedit_unwrap_ops.cc` */

void ED_uvedit_live_unwrap_begin(Scene *scene, Object *obedit);
void ED_uvedit_live_unwrap_re_solve();
void ED_uvedit_live_unwrap_end(short cancel);

void ED_uvedit_live_unwrap(const Scene *scene, Object **objects, int objects_len);
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
                            BMUVOffsets offsets);

/**
 * Returns true if UV coordinates lie on a valid tile in UDIM grid or tiled image.
 */
bool uv_coords_isect_udim(const Image *image, const int udim_grid[2], const float coords[2]);
