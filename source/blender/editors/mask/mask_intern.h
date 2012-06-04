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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mask/mask_intern.h
 *  \ingroup spclip
 */

#ifndef __MASK_INTERN_H__
#define __MASK_INTERN_H__

struct bContext;
struct wmEvent;
struct wmOperatorType;

/* internal exports only */

/* mask_add.c */
void MASK_OT_add_vertex(struct wmOperatorType *ot);
void MASK_OT_add_feather_vertex(struct wmOperatorType *ot);

/* mask_ops.c */
void MASK_OT_new(struct wmOperatorType *ot);
void MASK_OT_layer_new(struct wmOperatorType *ot);
void MASK_OT_layer_remove(struct wmOperatorType *ot);
void MASK_OT_cyclic_toggle(struct wmOperatorType *ot);

void MASK_OT_slide_point(struct wmOperatorType *ot);

void MASK_OT_delete(struct wmOperatorType *ot);

void MASK_OT_hide_view_clear(struct wmOperatorType *ot);
void MASK_OT_hide_view_set(struct wmOperatorType *ot);
void MASK_OT_switch_direction(struct wmOperatorType *ot);

void MASK_OT_handle_type_set(struct wmOperatorType *ot);

int ED_mask_feather_find_nearest(
        struct bContext *C, struct Mask *mask, float normal_co[2], int threshold,
        struct MaskLayer **masklay_r, struct MaskSpline **spline_r, struct MaskSplinePoint **point_r,
        struct MaskSplinePointUW **uw_r, float *score);

struct MaskSplinePoint *ED_mask_point_find_nearest(
        struct bContext *C, struct Mask *mask, float normal_co[2], int threshold,
        struct MaskLayer **masklay_r, struct MaskSpline **spline_r, int *is_handle_r,
        float *score);

/* mask_relationships.c */
void MASK_OT_parent_set(struct wmOperatorType *ot);
void MASK_OT_parent_clear(struct wmOperatorType *ot);

/* mask_select.c */
void MASK_OT_select(struct wmOperatorType *ot);
void MASK_OT_select_all(struct wmOperatorType *ot);

void MASK_OT_select_border(struct wmOperatorType *ot);
void MASK_OT_select_lasso(struct wmOperatorType *ot);
void MASK_OT_select_circle(struct wmOperatorType *ot);
void MASK_OT_select_linked_pick(struct wmOperatorType *ot);
void MASK_OT_select_linked(struct wmOperatorType *ot);

int ED_mask_spline_select_check(struct MaskSpline *spline);
int ED_mask_layer_select_check(struct MaskLayer *masklay);
int ED_mask_select_check(struct Mask *mask);

void ED_mask_spline_select_set(struct MaskSpline *spline, const short do_select);
void ED_mask_layer_select_set(struct MaskLayer *masklay, const short do_select);
void ED_mask_select_toggle_all(struct Mask *mask, int action);
void ED_mask_select_flush_all(struct Mask *mask);

/* mask_editor.c */
int ED_maskedit_poll(struct bContext *C);
int ED_maskedit_mask_poll(struct bContext *C);

void ED_mask_size(struct bContext *C, int *width, int *height);
void ED_mask_aspect(struct bContext *C, float *aspx, float *aspy);

void ED_mask_pixelspace_factor(struct bContext *C, float *scalex, float *scaley);
void ED_mask_mouse_pos(struct bContext *C, struct wmEvent *event, float co[2]);

void ED_mask_point_pos(struct bContext *C, float x, float y, float *xr, float *yr);
void ED_mask_point_pos__reverse(struct bContext *C, float x, float y, float *xr, float *yr);

/* mask_shapekey.c */
void MASK_OT_shape_key_insert(struct wmOperatorType *ot);
void MASK_OT_shape_key_clear(struct wmOperatorType *ot);

#endif /* __MASK_INTERN_H__ */
