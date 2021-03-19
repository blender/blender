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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

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

/* common poll functions */
bool ED_space_clip_poll(struct bContext *C);

bool ED_space_clip_view_clip_poll(struct bContext *C);

bool ED_space_clip_tracking_poll(struct bContext *C);
bool ED_space_clip_maskedit_poll(struct bContext *C);
bool ED_space_clip_maskedit_mask_poll(struct bContext *C);

void ED_space_clip_get_size(struct SpaceClip *sc, int *width, int *height);
void ED_space_clip_get_size_fl(struct SpaceClip *sc, float size[2]);
void ED_space_clip_get_zoom(struct SpaceClip *sc,
                            struct ARegion *region,
                            float *zoomx,
                            float *zoomy);
void ED_space_clip_get_aspect(struct SpaceClip *sc, float *aspx, float *aspy);
void ED_space_clip_get_aspect_dimension_aware(struct SpaceClip *sc, float *aspx, float *aspy);

int ED_space_clip_get_clip_frame_number(struct SpaceClip *sc);

struct ImBuf *ED_space_clip_get_buffer(struct SpaceClip *sc);
struct ImBuf *ED_space_clip_get_stable_buffer(struct SpaceClip *sc,
                                              float loc[2],
                                              float *scale,
                                              float *angle);

bool ED_space_clip_get_position(struct SpaceClip *sc,
                                struct ARegion *ar,
                                int mval[2],
                                float fpos[2]);
bool ED_space_clip_color_sample(struct SpaceClip *sc,
                                struct ARegion *region,
                                int mval[2],
                                float r_col[3]);

void ED_clip_update_frame(const struct Main *mainp, int cfra);
bool ED_clip_view_selection(const struct bContext *C, struct ARegion *region, bool fit);

void ED_clip_select_all(struct SpaceClip *sc, int action, bool *r_has_selection);
bool ED_clip_can_select(struct bContext *C);

void ED_clip_point_undistorted_pos(struct SpaceClip *sc, const float co[2], float r_co[2]);
void ED_clip_point_stable_pos(
    struct SpaceClip *sc, struct ARegion *region, float x, float y, float *xr, float *yr);
void ED_clip_point_stable_pos__reverse(struct SpaceClip *sc,
                                       struct ARegion *region,
                                       const float co[2],
                                       float r_co[2]);
void ED_clip_mouse_pos(struct SpaceClip *sc,
                       struct ARegion *region,
                       const int mval[2],
                       float co[2]);

bool ED_space_clip_check_show_trackedit(struct SpaceClip *sc);
bool ED_space_clip_check_show_maskedit(struct SpaceClip *sc);

struct MovieClip *ED_space_clip_get_clip(struct SpaceClip *sc);
void ED_space_clip_set_clip(struct bContext *C,
                            struct bScreen *screen,
                            struct SpaceClip *sc,
                            struct MovieClip *clip);

struct Mask *ED_space_clip_get_mask(struct SpaceClip *sc);
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
