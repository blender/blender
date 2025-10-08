/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#pragma once

#include "BKE_customdata.hh"

struct BMVert;
struct BMEdge;
struct BMFace;
struct BMLoop;
struct Object;
struct Scene;
struct SpaceImage;
struct ToolSettings;
struct wmOperatorType;
struct View2D;

/* find nearest */

struct UvNearestHit {
  /** Only for `*_multi(..)` versions of functions. */
  Object *ob;
  /** Always set if we have a hit. */
  BMFace *efa;
  BMLoop *l;
  /**
   * Needs to be set before calling nearest functions.
   *
   * \note When #uv_nearest_hit_init_dist_px or #uv_nearest_hit_init_max are used,
   * this value is pixels squared.
   */
  float dist_sq;

  /** Scale the UVs to account for aspect ratio from the image view. */
  float scale[2];
};

UvNearestHit uv_nearest_hit_init_dist_px(const View2D *v2d, float dist_px);
UvNearestHit uv_nearest_hit_init_max(const View2D *v2d);
UvNearestHit uv_nearest_hit_init_max_default();

bool uv_find_nearest_vert(
    Scene *scene, Object *obedit, const float co[2], float penalty_dist, UvNearestHit *hit);
bool uv_find_nearest_vert_multi(Scene *scene,
                                blender::Span<Object *> objects,
                                const float co[2],
                                float penalty_dist,
                                UvNearestHit *hit);

bool uv_find_nearest_edge(
    Scene *scene, Object *obedit, const float co[2], float penalty, UvNearestHit *hit);
bool uv_find_nearest_edge_multi(Scene *scene,
                                blender::Span<Object *> objects,
                                const float co[2],
                                float penalty,
                                UvNearestHit *hit);

/**
 * \param only_in_face: when true, only hit faces which `co` is inside.
 * This gives users a result they might expect, especially when zoomed in.
 *
 * \note Concave faces can cause odd behavior, although in practice this isn't often an issue.
 * The center can be outside the face, in this case the distance to the center
 * could cause the face to be considered too far away.
 * If this becomes an issue we could track the distance to the faces closest edge.
 */
bool uv_find_nearest_face_ex(
    Scene *scene, Object *obedit, const float co[2], UvNearestHit *hit, bool only_in_face);
bool uv_find_nearest_face(Scene *scene, Object *obedit, const float co[2], UvNearestHit *hit);
bool uv_find_nearest_face_multi_ex(Scene *scene,
                                   blender::Span<Object *> objects,
                                   const float co[2],
                                   UvNearestHit *hit,
                                   bool only_in_face);
bool uv_find_nearest_face_multi(Scene *scene,
                                blender::Span<Object *> objects,
                                const float co[2],
                                UvNearestHit *hit);

BMLoop *uv_find_nearest_loop_from_vert(Scene *scene, Object *obedit, BMVert *v, const float co[2]);
BMLoop *uv_find_nearest_loop_from_edge(Scene *scene, Object *obedit, BMEdge *e, const float co[2]);

bool uvedit_vert_is_edge_select_any_other(const ToolSettings *ts,
                                          const BMesh *bm,
                                          const BMLoop *l,
                                          const BMUVOffsets &offsets);
bool uvedit_vert_is_face_select_any_other(const ToolSettings *ts,
                                          const BMesh *bm,
                                          const BMLoop *l,
                                          const BMUVOffsets &offsets);
bool uvedit_edge_is_face_select_any_other(const ToolSettings *ts,
                                          const BMesh *bm,
                                          const BMLoop *l,
                                          const BMUVOffsets &offsets);

bool uvedit_vert_is_all_other_faces_selected(const ToolSettings *ts,
                                             const BMesh *bm,
                                             const BMLoop *l,
                                             const BMUVOffsets &offsets);

[[nodiscard]] bool uvedit_vert_select_get_no_sync(const ToolSettings *ts,
                                                  const BMesh *bm,
                                                  const BMLoop *l);
[[nodiscard]] bool uvedit_edge_select_get_no_sync(const ToolSettings *ts,
                                                  const BMesh *bm,
                                                  const BMLoop *l);
[[nodiscard]] bool uvedit_face_select_get_no_sync(const ToolSettings *ts,
                                                  const BMesh *bm,
                                                  const BMFace *f);

void uvedit_vert_select_set_no_sync(const ToolSettings *ts,
                                    const BMesh *bm,
                                    BMLoop *l,
                                    bool select);
void uvedit_edge_select_set_no_sync(const ToolSettings *ts,
                                    const BMesh *bm,
                                    BMLoop *l,
                                    bool select);
void uvedit_face_select_set_no_sync(const ToolSettings *ts,
                                    const BMesh *bm,
                                    BMFace *f,
                                    bool select);

/* utility tool functions */

void uvedit_live_unwrap_update(SpaceImage *sima, Scene *scene, Object *obedit);

/* operators */

void UV_OT_average_islands_scale(wmOperatorType *ot);
void UV_OT_cube_project(wmOperatorType *ot);
void UV_OT_cylinder_project(wmOperatorType *ot);
void UV_OT_project_from_view(wmOperatorType *ot);
void UV_OT_minimize_stretch(wmOperatorType *ot);
void UV_OT_pack_islands(wmOperatorType *ot);
void UV_OT_reset(wmOperatorType *ot);
void UV_OT_sphere_project(wmOperatorType *ot);
void UV_OT_unwrap(wmOperatorType *ot);
void UV_OT_rip(wmOperatorType *ot);
void UV_OT_stitch(wmOperatorType *ot);
void UV_OT_smart_project(wmOperatorType *ot);
void UV_OT_copy_mirrored_faces(wmOperatorType *ot);

/* uvedit_copy_paste.cc */
void UV_OT_copy(wmOperatorType *ot);
void UV_OT_paste(wmOperatorType *ot);

/* `uvedit_path.cc` */

void UV_OT_shortest_path_pick(wmOperatorType *ot);
void UV_OT_shortest_path_select(wmOperatorType *ot);

/* `uvedit_select.cc` */

void uvedit_select_prepare_custom_data(const Scene *scene, BMesh *bm);
void uvedit_select_prepare_sync_select(const Scene *scene, BMesh *bm);

void uvedit_select_prepare_UNUSED(const Scene *scene, BMesh *bm);

bool uvedit_select_is_any_selected(const Scene *scene, BMesh *bm);
bool uvedit_select_is_any_selected_multi(const Scene *scene, blender::Span<Object *> objects);
/**
 * \warning This returns first selected UV,
 * not ideal in many cases since there could be multiple.
 */
const float *uvedit_first_selected_uv_from_vertex(Scene *scene,
                                                  const BMesh *bm,
                                                  BMVert *eve,
                                                  const BMUVOffsets &offsets);

void UV_OT_select_all(wmOperatorType *ot);
void UV_OT_select(wmOperatorType *ot);
void UV_OT_select_loop(wmOperatorType *ot);
void UV_OT_select_edge_ring(wmOperatorType *ot);
void UV_OT_select_linked(wmOperatorType *ot);
void UV_OT_select_linked_pick(wmOperatorType *ot);
void UV_OT_select_split(wmOperatorType *ot);
void UV_OT_select_pinned(wmOperatorType *ot);
void UV_OT_select_box(wmOperatorType *ot);
void UV_OT_select_lasso(wmOperatorType *ot);
void UV_OT_select_circle(wmOperatorType *ot);
void UV_OT_select_more(wmOperatorType *ot);
void UV_OT_select_less(wmOperatorType *ot);
void UV_OT_select_overlap(wmOperatorType *ot);
void UV_OT_select_similar(wmOperatorType *ot);
void UV_OT_custom_region_set(wmOperatorType *ot);

/* Used only when UV sync select is disabled. */
void UV_OT_select_mode(wmOperatorType *ot);
