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
struct Mask;
struct wmEvent;
struct wmOperatorType;

/* internal exports only */

/* mask_add.c */
void MASK_OT_add_vertex(struct wmOperatorType *ot);
void MASK_OT_add_feather_vertex(struct wmOperatorType *ot);
void MASK_OT_primitive_circle_add(struct wmOperatorType *ot);
void MASK_OT_primitive_square_add(struct wmOperatorType *ot);

/* mask_ops.c */
struct Mask *ED_mask_new(struct bContext *C, const char *name);
struct MaskLayer *ED_mask_layer_ensure(struct bContext *C);

void MASK_OT_new(struct wmOperatorType *ot);
void MASK_OT_layer_new(struct wmOperatorType *ot);
void MASK_OT_layer_remove(struct wmOperatorType *ot);
void MASK_OT_cyclic_toggle(struct wmOperatorType *ot);

void MASK_OT_slide_point(struct wmOperatorType *ot);

void MASK_OT_delete(struct wmOperatorType *ot);

void MASK_OT_hide_view_clear(struct wmOperatorType *ot);
void MASK_OT_hide_view_set(struct wmOperatorType *ot);
void MASK_OT_feather_weight_clear(struct wmOperatorType *ot);
void MASK_OT_switch_direction(struct wmOperatorType *ot);
void MASK_OT_normals_make_consistent(struct wmOperatorType *ot);

void MASK_OT_handle_type_set(struct wmOperatorType *ot);

bool ED_mask_feather_find_nearest(
        const struct bContext *C, struct Mask *mask, const float normal_co[2], const float threshold,
        struct MaskLayer **masklay_r, struct MaskSpline **spline_r, struct MaskSplinePoint **point_r,
        struct MaskSplinePointUW **uw_r, float *score);

struct MaskSplinePoint *ED_mask_point_find_nearest(
        const struct bContext *C, struct Mask *mask, const float normal_co[2], const float threshold,
        struct MaskLayer **masklay_r, struct MaskSpline **spline_r, bool *is_handle_r,
        float *score);

void MASK_OT_layer_move(struct wmOperatorType *ot);

void MASK_OT_duplicate(struct wmOperatorType *ot);
void MASK_OT_copy_splines(struct wmOperatorType *ot);
void MASK_OT_paste_splines(struct wmOperatorType *ot);

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
void MASK_OT_select_more(struct wmOperatorType *ot);
void MASK_OT_select_less(struct wmOperatorType *ot);

bool ED_mask_spline_select_check(struct MaskSpline *spline);
bool ED_mask_layer_select_check(struct MaskLayer *masklay);
bool ED_mask_select_check(struct Mask *mask);

void ED_mask_spline_select_set(struct MaskSpline *spline, const short do_select);
void ED_mask_layer_select_set(struct MaskLayer *masklay, const short do_select);
void ED_mask_select_toggle_all(struct Mask *mask, int action);
void ED_mask_select_flush_all(struct Mask *mask);

/* mask_editor.c */
int ED_maskedit_poll(struct bContext *C);
int ED_maskedit_mask_poll(struct bContext *C);

/* mask_shapekey.c */
void MASK_OT_shape_key_insert(struct wmOperatorType *ot);
void MASK_OT_shape_key_clear(struct wmOperatorType *ot);
void MASK_OT_shape_key_feather_reset(struct wmOperatorType *ot);
void MASK_OT_shape_key_rekey(struct wmOperatorType *ot);

#endif /* __MASK_INTERN_H__ */
