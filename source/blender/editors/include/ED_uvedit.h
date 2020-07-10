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

#ifndef __ED_UVEDIT_H__
#define __ED_UVEDIT_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct BMEditMesh;
struct BMFace;
struct BMLoop;
struct BMesh;
struct Depsgraph;
struct Image;
struct ImageUser;
struct Main;
struct Object;
struct Scene;
struct SpaceImage;
struct ToolSettings;
struct ViewLayer;
struct bNode;
struct wmKeyConfig;

/* uvedit_ops.c */
void ED_operatortypes_uvedit(void);
void ED_operatormacros_uvedit(void);
void ED_keymap_uvedit(struct wmKeyConfig *keyconf);

bool ED_uvedit_minmax(const struct Scene *scene,
                      struct Object *obedit,
                      float min[2],
                      float max[2]);
void ED_uvedit_select_all(struct BMesh *bm);

bool ED_uvedit_minmax_multi(const struct Scene *scene,
                            struct Object **objects_edit,
                            uint objects_len,
                            float r_min[2],
                            float r_max[2]);
bool ED_uvedit_center_multi(const struct Scene *scene,
                            struct Object **objects_edit,
                            uint objects_len,
                            float r_cent[2],
                            char mode);

bool ED_uvedit_center_from_pivot_ex(struct SpaceImage *sima,
                                    struct Scene *scene,
                                    struct ViewLayer *view_layer,
                                    float r_center[2],
                                    char mode,
                                    bool *r_has_select);
bool ED_uvedit_center_from_pivot(struct SpaceImage *sima,
                                 struct Scene *scene,
                                 struct ViewLayer *view_layer,
                                 float r_center[2],
                                 char mode);

bool ED_object_get_active_image(struct Object *ob,
                                int mat_nr,
                                struct Image **r_ima,
                                struct ImageUser **r_iuser,
                                struct bNode **r_node,
                                struct bNodeTree **r_ntree);
void ED_object_assign_active_image(struct Main *bmain,
                                   struct Object *ob,
                                   int mat_nr,
                                   struct Image *ima);

bool ED_uvedit_test(struct Object *obedit);

/* visibility and selection */
bool uvedit_face_visible_test_ex(const struct ToolSettings *ts, struct BMFace *efa);
bool uvedit_face_select_test_ex(const struct ToolSettings *ts,
                                struct BMFace *efa,
                                const int cd_loop_uv_offset);
bool uvedit_edge_select_test_ex(const struct ToolSettings *ts,
                                struct BMLoop *l,
                                const int cd_loop_uv_offset);
bool uvedit_uv_select_test_ex(const struct ToolSettings *ts,
                              struct BMLoop *l,
                              const int cd_loop_uv_offset);

bool uvedit_face_visible_test(const struct Scene *scene, struct BMFace *efa);
bool uvedit_face_select_test(const struct Scene *scene,
                             struct BMFace *efa,
                             const int cd_loop_uv_offset);
bool uvedit_edge_select_test(const struct Scene *scene,
                             struct BMLoop *l,
                             const int cd_loop_uv_offset);
bool uvedit_uv_select_test(const struct Scene *scene,
                           struct BMLoop *l,
                           const int cd_loop_uv_offset);
/* uv face */
bool uvedit_face_select_set(const struct Scene *scene,
                            struct BMEditMesh *em,
                            struct BMFace *efa,
                            const bool select,
                            const bool do_history,
                            const int cd_loop_uv_offset);
bool uvedit_face_select_enable(const struct Scene *scene,
                               struct BMEditMesh *em,
                               struct BMFace *efa,
                               const bool do_history,
                               const int cd_loop_uv_offset);
bool uvedit_face_select_disable(const struct Scene *scene,
                                struct BMEditMesh *em,
                                struct BMFace *efa,
                                const int cd_loop_uv_offset);
/* uv edge */
void uvedit_edge_select_set(struct BMEditMesh *em,
                            const struct Scene *scene,
                            struct BMLoop *l,
                            const bool select,
                            const bool do_history,
                            const int cd_loop_uv_offset);
void uvedit_edge_select_enable(struct BMEditMesh *em,
                               const struct Scene *scene,
                               struct BMLoop *l,
                               const bool do_history,
                               const int cd_loop_uv_offset);
void uvedit_edge_select_disable(struct BMEditMesh *em,
                                const struct Scene *scene,
                                struct BMLoop *l,
                                const int cd_loop_uv_offset);
/* uv vert */
void uvedit_uv_select_set(struct BMEditMesh *em,
                          const struct Scene *scene,
                          struct BMLoop *l,
                          const bool select,
                          const bool do_history,
                          const int cd_loop_uv_offset);
void uvedit_uv_select_enable(struct BMEditMesh *em,
                             const struct Scene *scene,
                             struct BMLoop *l,
                             const bool do_history,
                             const int cd_loop_uv_offset);
void uvedit_uv_select_disable(struct BMEditMesh *em,
                              const struct Scene *scene,
                              struct BMLoop *l,
                              const int cd_loop_uv_offset);

bool ED_uvedit_nearest_uv(const struct Scene *scene,
                          struct Object *obedit,
                          const float co[2],
                          float *dist_sq,
                          float r_uv[2]);
bool ED_uvedit_nearest_uv_multi(const struct Scene *scene,
                                struct Object **objects,
                                const uint objects_len,
                                const float co[2],
                                float *dist_sq,
                                float r_uv[2]);

void ED_uvedit_get_aspect(struct Object *obedit, float *r_aspx, float *r_aspy);

void ED_uvedit_active_vert_loop_set(struct BMesh *bm, struct BMLoop *l);
struct BMLoop *ED_uvedit_active_vert_loop_get(struct BMesh *bm);

void ED_uvedit_active_edge_loop_set(struct BMesh *bm, struct BMLoop *l);
struct BMLoop *ED_uvedit_active_edge_loop_get(struct BMesh *bm);

/* uvedit_unwrap_ops.c */
void ED_uvedit_live_unwrap_begin(struct Scene *scene, struct Object *obedit);
void ED_uvedit_live_unwrap_re_solve(void);
void ED_uvedit_live_unwrap_end(short cancel);

void ED_uvedit_live_unwrap(const struct Scene *scene, struct Object **objects, int objects_len);
void ED_uvedit_add_simple_uvs(struct Main *bmain, const struct Scene *scene, struct Object *ob);

/* uvedit_draw.c */
void ED_image_draw_cursor(struct ARegion *region, const float cursor[2]);
void ED_uvedit_draw_main(struct SpaceImage *sima,
                         const struct Scene *scene,
                         struct ViewLayer *view_layer,
                         struct Object *obedit,
                         struct Object *obact,
                         struct Depsgraph *depsgraph);

/* uvedit_buttons.c */
void ED_uvedit_buttons_register(struct ARegionType *art);

#ifdef __cplusplus
}
#endif

#endif /* __ED_UVEDIT_H__ */
