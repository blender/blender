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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sculpt_paint/paint_curve.c
 *  \ingroup edsculpt
 */

#include <string.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_paint.h"

#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "ED_paint.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "paint_intern.h"

#define PAINT_CURVE_SELECT_THRESHOLD 40.0f
#define PAINT_CURVE_POINT_SELECT(pcp, i) (*(&pcp->bez.f1 + i) = SELECT)


int paint_curve_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	Paint *p;
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	SpaceImage *sima;

	if (rv3d && !(ob && ((ob->mode & OB_MODE_ALL_PAINT) != 0)))
		return false;

	sima = CTX_wm_space_image(C);

	if (sima && sima->mode != SI_MODE_PAINT)
		return false;

	p = BKE_paint_get_active_from_context(C);

	if (p && p->brush && (p->brush->flag & BRUSH_CURVE)) {
		return true;
	}

	return false;
}

/* Paint Curve Undo*/

typedef struct UndoCurve {
	struct UndoImageTile *next, *prev;

	PaintCurvePoint *points; /* points of curve */
	int tot_points;
	int active_point;

	char idname[MAX_ID_NAME];  /* name instead of pointer*/
} UndoCurve;

static void paintcurve_undo_restore(bContext *C, ListBase *lb)
{
	Paint *p = BKE_paint_get_active_from_context(C);
	UndoCurve *uc;
	PaintCurve *pc;

	if (p->brush) {
		pc = p->brush->paint_curve;
	}

	if (!pc)
		return;

	uc = (UndoCurve *)lb->first;

	if (strncmp(uc->idname, pc->id.name, BLI_strnlen(uc->idname, sizeof(uc->idname))) == 0) {
		SWAP(PaintCurvePoint *, pc->points, uc->points);
		SWAP(int, pc->tot_points, uc->tot_points);
		SWAP(int, pc->add_index, uc->active_point);
	}
}

static void paintcurve_undo_delete(ListBase *lb)
{
	UndoCurve *uc;
	uc = (UndoCurve *)lb->first;

	if (uc->points)
		MEM_freeN(uc->points);
	uc->points = NULL;
}


static void paintcurve_undo_begin(bContext *C, wmOperator *op, PaintCurve *pc)
{
	PaintMode mode = BKE_paintmode_get_active_from_context(C);
	ListBase *lb = NULL;
	int undo_stack_id;
	UndoCurve *uc;

	switch (mode) {
		case PAINT_TEXTURE_2D:
		case PAINT_TEXTURE_PROJECTIVE:
			undo_stack_id = UNDO_PAINT_IMAGE;
			break;

		case PAINT_SCULPT:
			undo_stack_id = UNDO_PAINT_MESH;
			break;

		default:
			/* do nothing, undo is handled by global */
			return;
	}


	ED_undo_paint_push_begin(undo_stack_id, op->type->name,
	                         paintcurve_undo_restore, paintcurve_undo_delete, NULL);
	lb = undo_paint_push_get_list(undo_stack_id);

	uc = MEM_callocN(sizeof(*uc), "Undo_curve");

	lb->first = uc;

	BLI_strncpy(uc->idname, pc->id.name, sizeof(uc->idname));
	uc->tot_points = pc->tot_points;
	uc->active_point = pc->add_index;
	uc->points = MEM_dupallocN(pc->points);

	undo_paint_push_count_alloc(undo_stack_id, sizeof(*uc) + sizeof(*pc->points) * pc->tot_points);

	ED_undo_paint_push_end(undo_stack_id);
}
#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)

/* returns 0, 1, or 2 in point according to handle 1, pivot or handle 2 */
static PaintCurvePoint *paintcurve_point_get_closest(PaintCurve *pc, const float pos[2], bool ignore_pivot, const float threshold, char *point)
{
	PaintCurvePoint *pcp, *closest = NULL;
	int i;
	float dist, closest_dist = FLT_MAX;

	for (i = 0, pcp = pc->points; i < pc->tot_points; i++, pcp++) {
		dist = len_manhattan_v2v2(pos, pcp->bez.vec[0]);
		if (dist < threshold) {
			if (dist < closest_dist) {
				closest = pcp;
				closest_dist = dist;
				if (point)
					*point = SEL_F1;
			}
		}
		if (!ignore_pivot) {
			dist = len_manhattan_v2v2(pos, pcp->bez.vec[1]);
			if (dist < threshold) {
				if (dist < closest_dist) {
					closest = pcp;
					closest_dist = dist;
					if (point)
						*point = SEL_F2;
				}
			}
		}
		dist = len_manhattan_v2v2(pos, pcp->bez.vec[2]);
		if (dist < threshold) {
			if (dist < closest_dist) {
				closest = pcp;
				closest_dist = dist;
				if (point)
					*point = SEL_F3;
			}
		}
	}

	return closest;
}

static int paintcurve_point_co_index(char sel)
{
	char i = 0;
	while (sel != 1) {
		sel >>= 1;
		i++;
	}
	return i;
}

/******************* Operators *********************************/

static int paintcurve_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	Paint *p = BKE_paint_get_active_from_context(C);
	Main *bmain = CTX_data_main(C);

	if (p && p->brush) {
		p->brush->paint_curve = BKE_paint_curve_add(bmain, "PaintCurve");
	}

	return OPERATOR_FINISHED;
}

void PAINTCURVE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Paint Curve";
	ot->description = "Add new paint curve";
	ot->idname = "PAINTCURVE_OT_new";

	/* api callbacks */
	ot->exec = paintcurve_new_exec;
	ot->poll = paint_curve_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void paintcurve_point_add(bContext *C,  wmOperator *op, const int loc[2])
{
	Paint *p = BKE_paint_get_active_from_context(C);
	Brush *br = p->brush;
	Main *bmain = CTX_data_main(C);
	PaintCurve *pc;
	PaintCurvePoint *pcp;
	wmWindow *window = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	float vec[3] = {loc[0], loc[1], 0.0};
	int add_index;
	int i;

	pc = br->paint_curve;
	if (!pc) {
		br->paint_curve = pc = BKE_paint_curve_add(bmain, "PaintCurve");
	}

	paintcurve_undo_begin(C, op, pc);

	pcp = MEM_mallocN((pc->tot_points + 1) * sizeof(PaintCurvePoint), "PaintCurvePoint");
	add_index = pc->add_index;

	if (pc->points) {
		if (add_index > 0)
			memcpy(pcp, pc->points, add_index * sizeof(PaintCurvePoint));
		if (add_index < pc->tot_points)
			memcpy(pcp + add_index + 1, pc->points + add_index, (pc->tot_points - add_index) * sizeof(PaintCurvePoint));

		MEM_freeN(pc->points);
	}
	pc->points = pcp;
	pc->tot_points++;

	/* initialize new point */
	memset(&pcp[add_index], 0, sizeof(PaintCurvePoint));
	copy_v3_v3(pcp[add_index].bez.vec[0], vec);
	copy_v3_v3(pcp[add_index].bez.vec[1], vec);
	copy_v3_v3(pcp[add_index].bez.vec[2], vec);

	/* last step, clear selection from all bezier handles expect the next */
	for (i = 0; i < pc->tot_points; i++) {
		pcp[i].bez.f1 = pcp[i].bez.f2 = pcp[i].bez.f3 = 0;
	}
	pcp[add_index].bez.f3 = SELECT;
	pcp[add_index].bez.h2 = HD_ALIGN;

	pc->add_index = add_index + 1;

	WM_paint_cursor_tag_redraw(window, ar);
}


static int paintcurve_add_point_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int loc[2] = {event->mval[0], event->mval[1]};
	paintcurve_point_add(C, op, loc);
	RNA_int_set_array(op->ptr, "location", loc);
	return OPERATOR_FINISHED;
}

static int paintcurve_add_point_exec(bContext *C, wmOperator *op)
{
	int loc[2];

	if (RNA_struct_property_is_set(op->ptr, "location")) {
		RNA_int_get_array(op->ptr, "location", loc);
		paintcurve_point_add(C, op, loc);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void PAINTCURVE_OT_add_point(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Paint Curve Point";
	ot->description = "Add new paint curve point";
	ot->idname = "PAINTCURVE_OT_add_point";

	/* api callbacks */
	ot->invoke = paintcurve_add_point_invoke;
	ot->exec = paintcurve_add_point_exec;
	ot->poll = paint_curve_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

	/* properties */
	RNA_def_int_vector(ot->srna, "location", 2, NULL, 0, SHRT_MAX,
	                   "Location", "Location of vertex in area space", 0, SHRT_MAX);
}

static int paintcurve_delete_point_exec(bContext *C, wmOperator *op)
{
	Paint *p = BKE_paint_get_active_from_context(C);
	Brush *br = p->brush;
	PaintCurve *pc;
	PaintCurvePoint *pcp;
	wmWindow *window = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	int i;
	int tot_del = 0;
	pc = br->paint_curve;

	if (!pc || pc->tot_points == 0) {
		return OPERATOR_CANCELLED;
	}

	paintcurve_undo_begin(C, op, pc);

#define DELETE_TAG 2

	for (i = 0, pcp = pc->points; i < pc->tot_points; i++, pcp++) {
		if ((pcp->bez.f1 & SELECT) || (pcp->bez.f2 & SELECT) || (pcp->bez.f3 & SELECT)) {
			pcp->bez.f2 |= DELETE_TAG;
			tot_del++;
		}
	}

	if (tot_del > 0) {
		int j = 0;
		int new_tot = pc->tot_points - tot_del;
		PaintCurvePoint *points_new = NULL;
		if (new_tot > 0)
			points_new = MEM_mallocN(new_tot * sizeof(PaintCurvePoint), "PaintCurvePoint");

		for (i = 0, pcp = pc->points; i < pc->tot_points; i++, pcp++) {
			if (!(pcp->bez.f2 & DELETE_TAG)) {
				points_new[j] = pc->points[i];

				if ((i + 1) == pc->add_index) {
					pc->add_index = j + 1;
				}
				j++;
			}
			else if ((i + 1) == pc->add_index) {
				/* prefer previous point */
				pc->add_index = j;
			}
		}
		MEM_freeN(pc->points);

		pc->points = points_new;
		pc->tot_points = new_tot;
	}

#undef DELETE_TAG

	WM_paint_cursor_tag_redraw(window, ar);

	return OPERATOR_FINISHED;
}


void PAINTCURVE_OT_delete_point(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add New Paint Curve Point";
	ot->description = "Add new paint curve point";
	ot->idname = "PAINTCURVE_OT_delete_point";

	/* api callbacks */
	ot->exec = paintcurve_delete_point_exec;
	ot->poll = paint_curve_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;
}


static bool paintcurve_point_select(bContext *C, wmOperator *op, const int loc[2], bool toggle, bool extend)
{
	wmWindow *window = CTX_wm_window(C);
	ARegion *ar = CTX_wm_region(C);
	Paint *p = BKE_paint_get_active_from_context(C);
	Brush *br = p->brush;
	PaintCurve *pc;
	PaintCurvePoint *pcp;
	int i;
	const float loc_fl[2] = {UNPACK2(loc)};

	pc = br->paint_curve;

	if (!pc)
		return false;

	paintcurve_undo_begin(C, op, pc);

	pcp = pc->points;

	if (toggle) {
		char select = 0;
		bool selected = false;

		for (i = 0; i < pc->tot_points; i++) {
			if (pcp[i].bez.f1 || pcp[i].bez.f2 || pcp[i].bez.f3) {
				selected = true;
				break;
			}
		}

		if (!selected) {
			select = SELECT;
		}

		for (i = 0; i < pc->tot_points; i++) {
			pc->points[i].bez.f1 = pc->points[i].bez.f2 = pc->points[i].bez.f3 = select;
		}
	}
	else {
		PaintCurvePoint *pcp;
		char selflag;

		pcp = paintcurve_point_get_closest(pc, loc_fl, false, PAINT_CURVE_SELECT_THRESHOLD, &selflag);

		if (pcp) {
			pc->add_index = (pcp - pc->points) + 1;

			if (selflag == SEL_F2) {
				if (extend)
					pcp->bez.f2 ^= SELECT;
				else
					pcp->bez.f2 |= SELECT;
			}
			else if (selflag == SEL_F1) {
				if (extend)
					pcp->bez.f1 ^= SELECT;
				else
					pcp->bez.f1 |= SELECT;
			}
			else if (selflag == SEL_F3) {
				if (extend)
					pcp->bez.f3 ^= SELECT;
				else
					pcp->bez.f3 |= SELECT;
			}
		}

		/* clear selection for unselected points if not extending and if a point has been selected */
		if (!extend && pcp) {
			for (i = 0; i < pc->tot_points; i++) {
				pc->points[i].bez.f1 = pc->points[i].bez.f2 = pc->points[i].bez.f3 = 0;

				if ((pc->points + i) == pcp) {
					char index = paintcurve_point_co_index(selflag);
					PAINT_CURVE_POINT_SELECT(pcp, index);
				}
			}
		}

		if (!pcp)
			return false;
	}

	WM_paint_cursor_tag_redraw(window, ar);

	return true;
}


static int paintcurve_select_point_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	int loc[2] = {UNPACK2(event->mval)};
	bool toggle = RNA_boolean_get(op->ptr, "toggle");
	bool extend = RNA_boolean_get(op->ptr, "extend");
	if (paintcurve_point_select(C, op, loc, toggle, extend)) {
		RNA_int_set_array(op->ptr, "location", loc);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static int paintcurve_select_point_exec(bContext *C, wmOperator *op)
{
	int loc[2];

	if (RNA_struct_property_is_set(op->ptr, "location")) {
		bool toggle = RNA_boolean_get(op->ptr, "toggle");
		bool extend = RNA_boolean_get(op->ptr, "extend");
		RNA_int_get_array(op->ptr, "location", loc);
		if (paintcurve_point_select(C, op, loc, toggle, extend))
			return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void PAINTCURVE_OT_select(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Select Paint Curve Point";
	ot->description = "Select a paint curve point";
	ot->idname = "PAINTCURVE_OT_select";

	/* api callbacks */
	ot->invoke = paintcurve_select_point_invoke;
	ot->exec = paintcurve_select_point_exec;
	ot->poll = paint_curve_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

	/* properties */
	RNA_def_int_vector(ot->srna, "location", 2, NULL, 0, SHRT_MAX,
	                   "Location", "Location of vertex in area space", 0, SHRT_MAX);
	prop = RNA_def_boolean(ot->srna, "toggle", false, "Toggle", "(De)select all");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

typedef struct PointSlideData {
	PaintCurvePoint *pcp;
	char select;
	int initial_loc[2];
	float point_initial_loc[3][2];
	int event;
	bool align;
} PointSlideData;

static int paintcurve_slide_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Paint *p = BKE_paint_get_active_from_context(C);
	const float loc_fl[2] = {UNPACK2(event->mval)};
	char select;
	int i;
	bool do_select = RNA_boolean_get(op->ptr, "select");
	bool align = RNA_boolean_get(op->ptr, "align");
	Brush *br = p->brush;
	PaintCurve *pc = br->paint_curve;
	PaintCurvePoint *pcp;

	if (!pc)
		return OPERATOR_PASS_THROUGH;

	if (do_select) {
	        pcp = paintcurve_point_get_closest(pc, loc_fl, align, PAINT_CURVE_SELECT_THRESHOLD, &select);
	}
	else {
		/* just find first selected point */
		for (i = 0; i < pc->tot_points; i++) {
			if (pc->points[i].bez.f1 || pc->points[i].bez.f2 || pc->points[i].bez.f3) {
				pcp = &pc->points[i];
				select = SEL_F3;
				break;
			}
		}
	}


	if (pcp) {
		ARegion *ar = CTX_wm_region(C);
		wmWindow *window = CTX_wm_window(C);
		PointSlideData *psd = MEM_mallocN(sizeof(PointSlideData), "PointSlideData");
		copy_v2_v2_int(psd->initial_loc, event->mval);
		psd->event = event->type;
		psd->pcp = pcp;
		psd->select = paintcurve_point_co_index(select);
		for (i = 0; i < 3; i++) {
			copy_v2_v2(psd->point_initial_loc[i], pcp->bez.vec[i]);
		}
		psd->align = align;
		op->customdata = psd;

		if (do_select)
			paintcurve_undo_begin(C, op, pc);

		/* first, clear all selection from points */
		for (i = 0; i < pc->tot_points; i++)
			pc->points[i].bez.f1 = pc->points[i].bez.f3 = pc->points[i].bez.f2 = 0;

		/* only select the active point */
		PAINT_CURVE_POINT_SELECT(pcp, psd->select);
		pc->add_index = (pcp - pc->points) + 1;

		WM_event_add_modal_handler(C, op);
		WM_paint_cursor_tag_redraw(window, ar);
		return OPERATOR_RUNNING_MODAL;
	}

	return OPERATOR_PASS_THROUGH;
}

static int paintcurve_slide_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	PointSlideData *psd = op->customdata;

	if (event->type == psd->event && event->val == KM_RELEASE) {
		MEM_freeN(psd);
		return OPERATOR_FINISHED;
	}

	switch (event->type) {
		case MOUSEMOVE:
		{
			ARegion *ar = CTX_wm_region(C);
			wmWindow *window = CTX_wm_window(C);
			float diff[2] = {event->mval[0] - psd->initial_loc[0],
			                 event->mval[1] - psd->initial_loc[1]};
			if (psd->select == 1) {
				int i;
				for (i = 0; i < 3; i++)
					add_v2_v2v2(psd->pcp->bez.vec[i], diff, psd->point_initial_loc[i]);
			}
			else {
				add_v2_v2(diff, psd->point_initial_loc[psd->select]);
				copy_v2_v2(psd->pcp->bez.vec[psd->select], diff);

				if (psd->align) {
					char opposite = (psd->select == 0) ? 2 : 0;
					sub_v2_v2v2(diff, psd->pcp->bez.vec[1], psd->pcp->bez.vec[psd->select]);
					add_v2_v2v2(psd->pcp->bez.vec[opposite], psd->pcp->bez.vec[1], diff);
				}
			}
			WM_paint_cursor_tag_redraw(window, ar);
			break;
		}
		default:
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}


void PAINTCURVE_OT_slide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Slide Paint Curve Point";
	ot->description = "Select and slide paint curve point";
	ot->idname = "PAINTCURVE_OT_slide";

	/* api callbacks */
	ot->invoke = paintcurve_slide_invoke;
	ot->modal = paintcurve_slide_modal;
	ot->poll = paint_curve_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "align", false, "Align Handles", "Aligns opposite point handle during transform");
	RNA_def_boolean(ot->srna, "select", true, "Select", "Attempt to select a point handle before transform");
}

static int paintcurve_draw_exec(bContext *C, wmOperator *UNUSED(op))
{
	PaintMode mode = BKE_paintmode_get_active_from_context(C);
	const char *name;

	switch (mode) {
		case PAINT_TEXTURE_2D:
		case PAINT_TEXTURE_PROJECTIVE:
			name = "PAINT_OT_image_paint";
			break;
		case PAINT_WEIGHT:
			name = "PAINT_OT_weight_paint";
			break;
		case PAINT_VERTEX:
			name = "PAINT_OT_vertex_paint";
			break;
		case PAINT_SCULPT:
			name = "SCULPT_OT_brush_stroke";
			break;
		default:
			return OPERATOR_PASS_THROUGH;
	}

	return WM_operator_name_call(C, name, WM_OP_INVOKE_DEFAULT, NULL);
}

void PAINTCURVE_OT_draw(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Draw Curve";
	ot->description = "Draw curve";
	ot->idname = "PAINTCURVE_OT_draw";

	/* api callbacks */
	ot->exec = paintcurve_draw_exec;
	ot->poll = paint_curve_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;
}

static int paintcurve_cursor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	op->customdata = SET_INT_IN_POINTER(event->type);
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int paintcurve_cursor_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (event->type == GET_INT_FROM_POINTER(op->customdata) && event->val == KM_RELEASE)
		return OPERATOR_FINISHED;

	if (event->type == MOUSEMOVE) {
		PaintMode mode = BKE_paintmode_get_active_from_context(C);

		switch (mode) {
			case PAINT_TEXTURE_2D:
			{
				ARegion *ar = CTX_wm_region(C);
				SpaceImage *sima = CTX_wm_space_image(C);
				float location[2];

				if (!sima)
					return OPERATOR_CANCELLED;

				UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
				copy_v2_v2(sima->cursor, location);
				WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, NULL);
				break;
			}
			default:
				ED_view3d_cursor3d_update(C, event->mval);
				break;
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

void PAINTCURVE_OT_cursor(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Place Cursor";
	ot->description = "Place cursor";
	ot->idname = "PAINTCURVE_OT_cursor";

	/* api callbacks */
	ot->invoke = paintcurve_cursor_invoke;
	ot->modal = paintcurve_cursor_modal;
	ot->poll = paint_curve_poll;

	/* flags */
	ot->flag = 0;
}
