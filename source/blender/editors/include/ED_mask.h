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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_mask.h
 *  \ingroup editors
 */

#ifndef __ED_MASK_H__
#define __ED_MASK_H__

struct wmKeyConfig;
struct MaskLayer;
struct MaskLayerShape;
struct wmEvent;

/* mask_edit.c */
void ED_mask_get_size(struct ScrArea *sa, int *width, int *height);
void ED_mask_zoom(struct ScrArea *sa, struct ARegion *ar, float *zoomx, float *zoomy);
void ED_mask_get_aspect(struct ScrArea *sa, struct ARegion *ar, float *aspx, float *aspy);

void ED_mask_pixelspace_factor(struct ScrArea *sa, struct ARegion *ar, float *scalex, float *scaley);
void ED_mask_mouse_pos(struct ScrArea *sa, struct ARegion *ar, struct wmEvent *event, float co[2]);

void ED_mask_point_pos(struct ScrArea *sa, struct ARegion *ar, float x, float y, float *xr, float *yr);
void ED_mask_point_pos__reverse(struct ScrArea *sa, struct ARegion *ar,
                                float x, float y, float *xr, float *yr);

void ED_operatortypes_mask(void);
void ED_keymap_mask(struct wmKeyConfig *keyconf);
void ED_operatormacros_mask(void);

/* mask_draw.c */
void ED_mask_draw(const bContext *C, const char draw_flag, const char draw_type);
void ED_mask_draw_region(struct Mask *mask, struct ARegion *ar,
                         const char draw_flag, const char draw_type,
                         int width, int height,
                         const short do_scale_applied, const short do_post_draw,
                         float stabmat[4][4],
                         const bContext *C);

/* mask_shapekey.c */
void ED_mask_layer_shape_auto_key(struct MaskLayer *masklay, const int frame);
int ED_mask_layer_shape_auto_key_all(struct Mask *mask, const int frame);
int ED_mask_layer_shape_auto_key_select(struct Mask *mask, const int frame);

/* ----------- Mask AnimEdit API ------------------ */
short ED_masklayer_frames_looper(struct MaskLayer *masklay, struct Scene *scene,
                                 short (*masklay_shape_cb)(struct MaskLayerShape *, struct Scene *));
void ED_masklayer_make_cfra_list(struct MaskLayer *masklay, ListBase *elems, short onlysel);

short ED_masklayer_frame_select_check(struct MaskLayer *masklay);
void  ED_masklayer_frame_select_set(struct MaskLayer *masklay, short mode);
void  ED_masklayer_frames_select_border(struct MaskLayer *masklay, float min, float max, short select_mode);
void  ED_mask_select_frames(struct MaskLayer *masklay, short select_mode);
void  ED_mask_select_frame(struct MaskLayer *masklay, int selx, short select_mode);

void ED_masklayer_frames_delete(struct MaskLayer *masklay);
void ED_masklayer_frames_duplicate(struct MaskLayer *masklay);

#if 0
void free_gpcopybuf(void);
void copy_gpdata(void);
void paste_gpdata(void);

 void snap_masklayer_frames(struct MaskLayer *masklay, short mode);
 void mirror_masklayer_frames(struct MaskLayer *masklay, short mode);
#endif

#endif /* __ED_MASK_H__ */
