/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct ImBuf;
struct Main;
struct Mask;
struct MovieClip;
struct SpaceClip;
struct bContext;
struct bScreen;

/*  ** clip_editor.c ** */

/* Returns true when the following conditions are met:
 * - Current space is Space Clip.
 * - There is a movie clip opened in it. */
bool ED_space_clip_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space is Space Clip.
 * - It is set to Clip view.
 *
 * It is not required to have movie clip opened for editing. */
bool ED_space_clip_view_clip_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space is Space Clip.
 * - It is set to Tracking mode.
 *
 * It is not required to have movie clip opened for editing. */
bool ED_space_clip_tracking_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space is Space Clip.
 * - It is set to Mask mode.
 *
 * It is not required to have mask opened for editing. */
bool ED_space_clip_maskedit_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space is Space Clip.
 * - It is set to Mask mode.
 * - Mask has visible and editable splines.
 *
 * It is not required to have mask opened for editing. */
bool ED_space_clip_maskedit_visible_splines_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space is Space Clip.
 * - It is set to Mask mode.
 * - The space has mask opened. */
bool ED_space_clip_maskedit_mask_poll(struct bContext *C);

/* Returns true when the following conditions are met:
 * - Current space is Space Clip.
 * - It is set to Mask mode.
 * - The space has mask opened.
 * - Mask has visible and editable splines. */
bool ED_space_clip_maskedit_mask_visible_splines_poll(struct bContext *C);

void ED_space_clip_get_size(const struct SpaceClip *sc, int *width, int *height);
void ED_space_clip_get_size_fl(const struct SpaceClip *sc, float size[2]);
void ED_space_clip_get_zoom(const struct SpaceClip *sc,
                            const struct ARegion *region,
                            float *zoomx,
                            float *zoomy);
void ED_space_clip_get_aspect(const struct SpaceClip *sc, float *aspx, float *aspy);
void ED_space_clip_get_aspect_dimension_aware(const struct SpaceClip *sc,
                                              float *aspx,
                                              float *aspy);

/**
 * Return current frame number in clip space.
 */
int ED_space_clip_get_clip_frame_number(const struct SpaceClip *sc);

struct ImBuf *ED_space_clip_get_buffer(const struct SpaceClip *sc);
struct ImBuf *ED_space_clip_get_stable_buffer(const struct SpaceClip *sc,
                                              float loc[2],
                                              float *scale,
                                              float *angle);

bool ED_space_clip_get_position(const struct SpaceClip *sc,
                                const struct ARegion *region,
                                int mval[2],
                                float fpos[2]);
/**
 * Returns color in linear space, matching #ED_space_image_color_sample().
 */
bool ED_space_clip_color_sample(const struct SpaceClip *sc,
                                const struct ARegion *region,
                                const int mval[2],
                                float r_col[3]);

void ED_clip_update_frame(const struct Main *mainp, int cfra);
bool ED_clip_view_selection(const struct bContext *C, const struct ARegion *region, bool fit);

void ED_clip_select_all(const struct SpaceClip *sc, int action, bool *r_has_selection);
bool ED_clip_can_select(struct bContext *C);

void ED_clip_point_undistorted_pos(const struct SpaceClip *sc, const float co[2], float r_co[2]);
void ED_clip_point_stable_pos(const struct SpaceClip *sc,
                              const struct ARegion *region,
                              float x,
                              float y,
                              float *xr,
                              float *yr);
/**
 * \brief the reverse of #ED_clip_point_stable_pos(), gets the marker region coords.
 * better name here? view_to_track / track_to_view or so?
 */
void ED_clip_point_stable_pos__reverse(const struct SpaceClip *sc,
                                       const struct ARegion *region,
                                       const float co[2],
                                       float r_co[2]);
/**
 * Takes `event->mval`.
 */
void ED_clip_mouse_pos(const struct SpaceClip *sc,
                       const struct ARegion *region,
                       const int mval[2],
                       float co[2]);

bool ED_space_clip_check_show_trackedit(const struct SpaceClip *sc);
bool ED_space_clip_check_show_maskedit(const struct SpaceClip *sc);

struct MovieClip *ED_space_clip_get_clip(const struct SpaceClip *sc);
void ED_space_clip_set_clip(struct bContext *C,
                            struct bScreen *screen,
                            struct SpaceClip *sc,
                            struct MovieClip *clip);

struct Mask *ED_space_clip_get_mask(const struct SpaceClip *sc);
void ED_space_clip_set_mask(struct bContext *C, struct SpaceClip *sc, struct Mask *mask);

/* Locked state is used to preserve current clip editor viewport upon changes. Example usage:
 *
 *   ...
 *
 *   ClipViewLockState lock_state;
 *   ED_clip_view_lock_state_store(C, &lock_state);
 *
 *   <change selection>
 *
 *   ED_clip_view_lock_state_restore_no_jump(C, &lock_state);
 *
 * These function are to be used from space clip editor context only. Otherwise debug builds will
 * assert, release builds will crash. */

typedef struct ClipViewLockState {
  float offset_x, offset_y;
  float lock_offset_x, lock_offset_y;
  float zoom;
} ClipViewLockState;

void ED_clip_view_lock_state_store(const struct bContext *C, ClipViewLockState *state);
void ED_clip_view_lock_state_restore_no_jump(const struct bContext *C,
                                             const ClipViewLockState *state);

/* ** clip_ops.c ** */
void ED_operatormacros_clip(void);

#ifdef __cplusplus
}
#endif
