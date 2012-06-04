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
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mask/mask_select.c
 *  \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_rect.h"
#include "BLI_lasso.h"

#include "BKE_context.h"
#include "BKE_mask.h"

#include "DNA_mask_types.h"
#include "DNA_object_types.h"  /* SELECT */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h"
#include "ED_mask.h"  /* own include */

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h"  /* own include */

/* 'check' select */
int ED_mask_spline_select_check(MaskSpline *spline)
{
	int i;

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];

		if (MASKPOINT_ISSEL_ANY(point))
			return TRUE;
	}

	return FALSE;
}

int ED_mask_layer_select_check(MaskLayer *masklay)
{
	MaskSpline *spline;

	if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
		return FALSE;
	}

	for (spline = masklay->splines.first; spline; spline = spline->next) {
		if (ED_mask_spline_select_check(spline)) {
			return TRUE;
		}
	}

	return FALSE;
}

int ED_mask_select_check(Mask *mask)
{
	MaskLayer *masklay;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		if (ED_mask_layer_select_check(masklay)) {
			return TRUE;
		}
	}

	return FALSE;
}

/* 'sel' select  */
void ED_mask_spline_select_set(MaskSpline *spline, const short do_select)
{
	int i;

	if (do_select)
		spline->flag |= SELECT;
	else
		spline->flag &= ~SELECT;

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];

		BKE_mask_point_select_set(point, do_select);
	}
}

void ED_mask_layer_select_set(MaskLayer *masklay, const short do_select)
{
	MaskSpline *spline;

	if (masklay->restrictflag & MASK_RESTRICT_SELECT) {
		if (do_select == TRUE) {
			return;
		}
	}

	for (spline = masklay->splines.first; spline; spline = spline->next) {
		ED_mask_spline_select_set(spline, do_select);
	}
}

void ED_mask_select_toggle_all(Mask *mask, int action)
{
	MaskLayer *masklay;

	if (action == SEL_TOGGLE) {
		if (ED_mask_select_check(mask))
			action = SEL_DESELECT;
		else
			action = SEL_SELECT;
	}

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {

		if (masklay->restrictflag & MASK_RESTRICT_VIEW) {
			continue;
		}

		ED_mask_layer_select_set(masklay, (action == SEL_SELECT) ? TRUE : FALSE);
	}
}

void ED_mask_select_flush_all(Mask *mask)
{
	MaskLayer *masklay;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			int i;

			spline->flag &= ~SELECT;

			/* intentionally _dont_ do this in the masklay loop
			 * so we clear flags on all splines */
			if (masklay->restrictflag & MASK_RESTRICT_VIEW) {
				continue;
			}

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *cur_point = &spline->points[i];

				if (MASKPOINT_ISSEL_ANY(cur_point)) {
					spline->flag |= SELECT;
				}
				else {
					int j;

					for (j = 0; j < cur_point->tot_uw; j++) {
						if (cur_point->uw[j].flag & SELECT) {
							spline->flag |= SELECT;
							break;
						}
					}
				}
			}
		}
	}
}

/******************** toggle selection *********************/

static int select_all_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	int action = RNA_enum_get(op->ptr, "action");

	ED_mask_select_toggle_all(mask, action);
	ED_mask_select_flush_all(mask);

	WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

	return OPERATOR_FINISHED;
}

void MASK_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select or Deselect All";
	ot->description = "Change selection of all curve points";
	ot->idname = "MASK_OT_select_all";

	/* api callbacks */
	ot->exec = select_all_exec;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_select_all(ot);
}

/******************** select *********************/

static int select_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	MaskSpline *spline;
	MaskSplinePoint *point = NULL;
	float co[2];
	short extend = RNA_boolean_get(op->ptr, "extend");
	short deselect = RNA_boolean_get(op->ptr, "deselect");
	short toggle = RNA_boolean_get(op->ptr, "toggle");

	int is_handle = 0;
	const float threshold = 19;

	RNA_float_get_array(op->ptr, "location", co);

	point = ED_mask_point_find_nearest(C, mask, co, threshold, &masklay, &spline, &is_handle, NULL);

	if (extend == 0 && deselect == 0 && toggle == 0)
		ED_mask_select_toggle_all(mask, SEL_DESELECT);

	if (point) {

		if (is_handle) {
			if (extend) {
				masklay->act_spline = spline;
				masklay->act_point = point;

				BKE_mask_point_select_set_handle(point, TRUE);
			}
			else if (deselect) {
				BKE_mask_point_select_set_handle(point, FALSE);
			}
			else {
				masklay->act_spline = spline;
				masklay->act_point = point;

				if (!MASKPOINT_ISSEL_HANDLE(point)) {
					BKE_mask_point_select_set_handle(point, TRUE);
				}
				else if (toggle) {
					BKE_mask_point_select_set_handle(point, FALSE);
				}
			}
		}
		else {
			if (extend) {
				masklay->act_spline = spline;
				masklay->act_point = point;

				BKE_mask_point_select_set(point, TRUE);
			}
			else if (deselect) {
				BKE_mask_point_select_set(point, FALSE);
			}
			else {
				masklay->act_spline = spline;
				masklay->act_point = point;

				if (!MASKPOINT_ISSEL_ANY(point)) {
					BKE_mask_point_select_set(point, TRUE);
				}
				else if (toggle) {
					BKE_mask_point_select_set(point, FALSE);
				}
			}
		}

		masklay->act_spline = spline;
		masklay->act_point = point;

		ED_mask_select_flush_all(mask);

		WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

		return OPERATOR_FINISHED;
	}
	else {
		MaskSplinePointUW *uw;

		if (ED_mask_feather_find_nearest(C, mask, co, threshold, &masklay, &spline, &point, &uw, NULL)) {

			if (extend) {
				masklay->act_spline = spline;
				masklay->act_point = point;

				if (uw) uw->flag |= SELECT;
			}
			else if (deselect) {
				if (uw) uw->flag &= ~SELECT;
			}
			else {
				masklay->act_spline = spline;
				masklay->act_point = point;

				if (uw) {
					if (!(uw->flag & SELECT)) {
						uw->flag |= SELECT;
					}
					else if (toggle) {
						uw->flag &= ~SELECT;
					}
				}
			}

			ED_mask_select_flush_all(mask);

			WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

			return OPERATOR_FINISHED;
		}
	}

	return OPERATOR_PASS_THROUGH;
}

static int select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	float co[2];

	ED_mask_mouse_pos(C, event, co);

	RNA_float_set_array(op->ptr, "location", co);

	return select_exec(C, op);
}

void MASK_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->description = "Select spline points";
	ot->idname = "MASK_OT_select";

	/* api callbacks */
	ot->exec = select_exec;
	ot->invoke = select_invoke;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_mouse_select(ot);

	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MIN, FLT_MAX,
	                     "Location", "Location of vertex in normalized space", -1.0f, 1.0f);
}



/********************** border select operator *********************/

static int border_select_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	int i;

	rcti rect;
	rctf rectf;
	int change = FALSE, mode, extend;

	/* get rectangle from operator */
	rect.xmin = RNA_int_get(op->ptr, "xmin");
	rect.ymin = RNA_int_get(op->ptr, "ymin");
	rect.xmax = RNA_int_get(op->ptr, "xmax");
	rect.ymax = RNA_int_get(op->ptr, "ymax");

	ED_mask_point_pos(C, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	ED_mask_point_pos(C, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

	mode = RNA_int_get(op->ptr, "gesture_mode");
	extend = RNA_boolean_get(op->ptr, "extend");

	/* do actual selection */
	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];
				MaskSplinePoint *point_deform = &points_array[i];

				/* TODO: handles? */
				/* TODO: uw? */

				if (BLI_in_rctf(&rectf, point_deform->bezt.vec[1][0], point_deform->bezt.vec[1][1])) {
					BKE_mask_point_select_set(point, mode == GESTURE_MODAL_SELECT);
					BKE_mask_point_select_set_handle(point, mode == GESTURE_MODAL_SELECT);
				}
				else if (!extend) {
					BKE_mask_point_select_set(point, FALSE);
					BKE_mask_point_select_set_handle(point, FALSE);
				}

				change = TRUE;
			}
		}
	}

	if (change) {
		ED_mask_select_flush_all(mask);

		WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void MASK_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Border Select";
	ot->description = "Select markers using border selection";
	ot->idname = "MASK_OT_select_border";

	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = border_select_exec;
	ot->modal = WM_border_select_modal;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_gesture_border(ot, TRUE);
}

static int do_lasso_select_mask(bContext *C, int mcords[][2], short moves, short select)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	int i;

	rcti rect;
	int change = FALSE;

	/* get rectangle from operator */
	BLI_lasso_boundbox(&rect, mcords, moves);

	/* do actual selection */
	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];
				MaskSplinePoint *point_deform = &points_array[i];

				/* TODO: handles? */
				/* TODO: uw? */

				float screen_co[2];

				/* marker in screen coords */
				ED_mask_point_pos__reverse(C,
				                           point_deform->bezt.vec[1][0], point_deform->bezt.vec[1][1],
				                           &screen_co[0], &screen_co[1]);

				if (BLI_in_rcti(&rect, screen_co[0], screen_co[1]) &&
				    BLI_lasso_is_point_inside(mcords, moves, screen_co[0], screen_co[1], INT_MAX))
				{
					BKE_mask_point_select_set(point, select);
					BKE_mask_point_select_set_handle(point, select);
				}

				change = TRUE;
			}
		}
	}

	if (change) {
		ED_mask_select_flush_all(mask);

		WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);
	}

	return change;
}

static int clip_lasso_select_exec(bContext *C, wmOperator *op)
{
	int mcords_tot;
	int (*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

	if (mcords) {
		short select;

		select = !RNA_boolean_get(op->ptr, "deselect");
		do_lasso_select_mask(C, mcords, mcords_tot, select);

		MEM_freeN(mcords);

		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

void MASK_OT_select_lasso(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Lasso Select";
	ot->description = "Select markers using lasso selection";
	ot->idname = "MASK_OT_select_lasso";

	/* api callbacks */
	ot->invoke = WM_gesture_lasso_invoke;
	ot->modal = WM_gesture_lasso_modal;
	ot->exec = clip_lasso_select_exec;
	ot->poll = ED_maskediting_mask_poll;
	ot->cancel = WM_gesture_lasso_cancel;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_collection_runtime(ot->srna, "path", &RNA_OperatorMousePath, "Path", "");
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect rather than select items");
	RNA_def_boolean(ot->srna, "extend", 1, "Extend", "Extend selection instead of deselecting everything first");
}

/********************** circle select operator *********************/

static int mask_spline_point_inside_ellipse(BezTriple *bezt, float offset[2], float ellipse[2])
{
	/* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
	float x, y;

	x = (bezt->vec[1][0] - offset[0]) * ellipse[0];
	y = (bezt->vec[1][1] - offset[1]) * ellipse[1];

	return x * x + y * y < 1.0f;
}

static int circle_select_exec(bContext *C, wmOperator *op)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	int i;

	SpaceClip *sc = CTX_wm_space_clip(C);
	ARegion *ar = CTX_wm_region(C);
	int x, y, radius, width, height, mode, change = FALSE;
	float zoomx, zoomy, offset[2], ellipse[2];

	/* get operator properties */
	x = RNA_int_get(op->ptr, "x");
	y = RNA_int_get(op->ptr, "y");
	radius = RNA_int_get(op->ptr, "radius");

	mode = RNA_int_get(op->ptr, "gesture_mode");

	/* TODO - make generic! - this is SpaceClip only! */
	/* compute ellipse and position in unified coordinates */
	ED_space_clip_size(sc, &width, &height);
	ED_space_clip_zoom(sc, ar, &zoomx, &zoomy);
	width = height = MAX2(width, height);

	ellipse[0] = width * zoomx / radius;
	ellipse[1] = height * zoomy / radius;

	ED_mask_point_pos(C, x, y, &offset[0], &offset[1]);

	/* do actual selection */
	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];
				MaskSplinePoint *point_deform = &points_array[i];

				if (mask_spline_point_inside_ellipse(&point_deform->bezt, offset, ellipse)) {
					BKE_mask_point_select_set(point, mode == GESTURE_MODAL_SELECT);
					BKE_mask_point_select_set_handle(point, mode == GESTURE_MODAL_SELECT);

					change = TRUE;
				}
			}
		}
	}

	if (change) {
		ED_mask_select_flush_all(mask);

		WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void MASK_OT_select_circle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Circle Select";
	ot->description = "Select markers using circle selection";
	ot->idname = "MASK_OT_select_circle";

	/* api callbacks */
	ot->invoke = WM_gesture_circle_invoke;
	ot->modal = WM_gesture_circle_modal;
	ot->exec = circle_select_exec;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 0, INT_MIN, INT_MAX, "Radius", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Gesture Mode", "", INT_MIN, INT_MAX);
}

static int mask_select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;
	MaskSpline *spline;
	MaskSplinePoint *point = NULL;
	float co[2];
	int do_select = !RNA_boolean_get(op->ptr, "deselect");

	int is_handle = 0;
	const float threshold = 19;
	int change = FALSE;

	ED_mask_mouse_pos(C, event, co);

	point = ED_mask_point_find_nearest(C, mask, co, threshold, &masklay, &spline, &is_handle, NULL);

	if (point) {
		ED_mask_spline_select_set(spline, do_select);
		masklay->act_spline = spline;
		masklay->act_point = point;

		change = TRUE;
	}

	if (change) {
		ED_mask_select_flush_all(mask);

		WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void MASK_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->idname = "MASK_OT_select_linked_pick";
	ot->description = "(De)select all points linked to the curve under the mouse cursor";

	/* api callbacks */
	ot->invoke = mask_select_linked_pick_invoke;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "");
}

static int mask_select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;

	int change = FALSE;

	/* do actual selection */
	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			if (ED_mask_spline_select_check(spline)) {
				ED_mask_spline_select_set(spline, TRUE);
				change = TRUE;
			}
		}
	}

	if (change) {
		ED_mask_select_flush_all(mask);

		WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void MASK_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked All";
	ot->idname = "MASK_OT_select_linked";
	ot->description = "Select all vertices linked to the active mesh";

	/* api callbacks */
	ot->exec = mask_select_linked_exec;
	ot->poll = ED_maskediting_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
