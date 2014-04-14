/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_clip.h
 *  \ingroup editors
 */

#ifndef __ED_CLIP_H__
#define __ED_CLIP_H__

struct ARegion;
struct bContext;
struct bScreen;
struct ImBuf;
struct Main;
struct Mask;
struct MovieClip;
struct SpaceClip;
struct Scene;

/*  ** clip_editor.c ** */

/* common poll functions */
int ED_space_clip_poll(struct bContext *C);

int ED_space_clip_view_clip_poll(struct bContext *C);

int ED_space_clip_tracking_poll(struct bContext *C);
int ED_space_clip_maskedit_poll(struct bContext *C);
int ED_space_clip_maskedit_mask_poll(struct bContext *C);

void ED_space_clip_get_size(struct SpaceClip *sc, int *width, int *height);
void ED_space_clip_get_size_fl(struct SpaceClip *sc, float size[2]);
void ED_space_clip_get_zoom(struct SpaceClip *sc, struct ARegion *ar, float *zoomx, float *zoomy);
void ED_space_clip_get_aspect(struct SpaceClip *sc, float *aspx, float *aspy);
void ED_space_clip_get_aspect_dimension_aware(struct SpaceClip *sc, float *aspx, float *aspy);

int ED_space_clip_get_clip_frame_number(struct SpaceClip *sc);

struct ImBuf *ED_space_clip_get_buffer(struct SpaceClip *sc);
struct ImBuf *ED_space_clip_get_stable_buffer(struct SpaceClip *sc, float loc[2], float *scale, float *angle);

bool ED_space_clip_color_sample(struct Scene *scene, struct SpaceClip *sc, struct ARegion *ar, int mval[2], float r_col[3]);

void ED_clip_update_frame(const struct Main *mainp, int cfra);
bool ED_clip_view_selection(const struct bContext *C, struct ARegion *ar, bool fit);

void ED_clip_point_undistorted_pos(struct SpaceClip *sc, const float co[2], float r_co[2]);
void ED_clip_point_stable_pos(struct SpaceClip *sc, struct ARegion *ar, float x, float y, float *xr, float *yr);
void ED_clip_point_stable_pos__reverse(struct SpaceClip *sc, struct ARegion *ar, const float co[2], float r_co[2]);
void ED_clip_mouse_pos(struct SpaceClip *sc, struct ARegion *ar, const int mval[2], float co[2]);

bool ED_space_clip_check_show_trackedit(struct SpaceClip *sc);
bool ED_space_clip_check_show_maskedit(struct SpaceClip *sc);

struct MovieClip *ED_space_clip_get_clip(struct SpaceClip *sc);
void ED_space_clip_set_clip(struct bContext *C, struct bScreen *screen, struct SpaceClip *sc, struct MovieClip *clip);

struct Mask *ED_space_clip_get_mask(struct SpaceClip *sc);
void ED_space_clip_set_mask(struct bContext *C, struct SpaceClip *sc, struct Mask *mask);

/* ** clip_ops.c ** */
void ED_operatormacros_clip(void);

#endif /* __ED_CLIP_H__ */
