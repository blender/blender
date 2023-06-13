/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 */

#ifdef __cplusplus
extern "C" {
#endif

struct FreestyleConfig;
struct FreestyleLineStyle;
struct Material;
struct Render;

struct FreestyleGlobals {
  struct Scene *scene;

  /* camera information */
  float viewpoint[3];
  float mv[4][4];
  float proj[4][4];
  int viewport[4];
};

extern struct FreestyleGlobals g_freestyle;

/* Rendering */
void FRS_init(void);
void FRS_set_context(struct bContext *C);
int FRS_is_freestyle_enabled(struct ViewLayer *view_layer);
void FRS_init_stroke_renderer(struct Render *re);
void FRS_begin_stroke_rendering(struct Render *re);
void FRS_do_stroke_rendering(struct Render *re, struct ViewLayer *view_layer);
void FRS_end_stroke_rendering(struct Render *re);
void FRS_free_view_map_cache(void);
void FRS_composite_result(struct Render *re,
                          struct ViewLayer *view_layer,
                          struct Render *freestyle_render);
void FRS_exit(void);

/* FreestyleConfig.linesets */
void FRS_copy_active_lineset(struct FreestyleConfig *config);
void FRS_paste_active_lineset(struct FreestyleConfig *config);
void FRS_delete_active_lineset(struct FreestyleConfig *config);
/**
 * Reinsert the active lineset at an offset \a direction from current position.
 * \return if position of active lineset has changed.
 */
bool FRS_move_active_lineset(struct FreestyleConfig *config, int direction);

/* Testing */
struct Material *FRS_create_stroke_material(struct Main *bmain,
                                            struct FreestyleLineStyle *linestyle);

#ifdef __cplusplus
}
#endif
