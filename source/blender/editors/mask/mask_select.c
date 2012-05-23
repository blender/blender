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
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_mask.h"

#include "DNA_mask_types.h"
#include "DNA_object_types.h"  /* SELECT */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_mask.h"
#include "ED_clip.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mask_intern.h"  /* own include */

int ED_mask_spline_select_check(MaskSplinePoint *points, int tot_point)
{
	int i;

	for (i = 0; i < tot_point; i++) {
		MaskSplinePoint *point = &points[i];

		if (MASKPOINT_ISSEL(point))
			return TRUE;
	}

	return FALSE;
}

int ED_mask_select_check(Mask *mask)
{
	MaskObject *maskobj;

	for (maskobj = mask->maskobjs.first; maskobj; maskobj = maskobj->next) {
		MaskSpline *spline;

		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			if (ED_mask_spline_select_check(spline->points, spline->tot_point)) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

void ED_mask_point_select(MaskSplinePoint *point, int action)
{
	int i;

	switch (action) {
		case SEL_SELECT:
			MASKPOINT_SEL(point);
			break;
		case SEL_DESELECT:
			MASKPOINT_DESEL(point);
			break;
		case SEL_INVERT:
			MASKPOINT_INVSEL(point);
			break;
	}

	for (i = 0; i < point->tot_uw; i++) {
		switch (action) {
			case SEL_SELECT:
				point->uw[i].flag |= SELECT;
				break;
			case SEL_DESELECT:
				point->uw[i].flag &= ~SELECT;
				break;
			case SEL_INVERT:
				point->uw[i].flag ^= SELECT;
				break;
		}
	}
}


void ED_mask_select_toggle_all(Mask *mask, int action)
{
	MaskObject *maskobj;

	if (action == SEL_TOGGLE) {
		if (ED_mask_select_check(mask))
			action = SEL_DESELECT;
		else
			action = SEL_SELECT;
	}

	for (maskobj = mask->maskobjs.first; maskobj; maskobj = maskobj->next) {
		MaskSpline *spline;

		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			int i;

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				ED_mask_point_select(point, action);
			}
		}
	}
}

void ED_mask_select_flush_all(Mask *mask)
{
	MaskObject *maskobj;

	for (maskobj = mask->maskobjs.first; maskobj; maskobj = maskobj->next) {
		MaskSpline *spline;

		for (spline = maskobj->splines.first; spline; spline = spline->next) {
			int i;

			spline->flag &= ~SELECT;

			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *cur_point = &spline->points[i];

				if (MASKPOINT_ISSEL(cur_point)) {
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
	MaskObject *maskobj;
	MaskSpline *spline;
	MaskSplinePoint *point = NULL;
	float co[2];
	int extend = RNA_boolean_get(op->ptr, "extend");
	int is_handle = 0;
	const float threshold = 19;

	RNA_float_get_array(op->ptr, "location", co);

	point = ED_mask_point_find_nearest(C, mask, co, threshold, &maskobj, &spline, &is_handle, NULL);

	if (point) {
		if (!extend)
			ED_mask_select_toggle_all(mask, SEL_DESELECT);

		if (is_handle) {
			MASKPOINT_HANDLE_SEL(point);
		}
		else {
			ED_mask_point_select(point, SEL_SELECT);
		}

		maskobj->act_spline = spline;
		maskobj->act_point = point;

		ED_mask_select_flush_all(mask);

		WM_event_add_notifier(C, NC_MASK | ND_SELECT, mask);

		return OPERATOR_FINISHED;
	}
	else {
		MaskSplinePointUW *uw;

		if (ED_mask_feather_find_nearest(C, mask, co, threshold, &maskobj, &spline, &point, &uw, NULL)) {
			if (!extend)
				ED_mask_select_toggle_all(mask, SEL_DESELECT);

			if (uw)
				uw->flag |= SELECT;

			maskobj->act_spline = spline;
			maskobj->act_point = point;

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
	RNA_def_boolean(ot->srna, "extend", 0,
	                "Extend", "Extend selection rather than clearing the existing selection");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MIN, FLT_MAX,
	                     "Location", "Location of vertex in normalized space", -1.0f, 1.0f);
}
