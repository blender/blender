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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): John Roper
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/curve/editcurve_select.c
 *  \ingroup edcurve
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_rand.h"


#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_types.h"
#include "ED_view3d.h"
#include "ED_curve.h"

#include "curve_intern.h"


#include "RNA_access.h"
#include "RNA_define.h"


/* returns 1 in case (de)selection was successful */
bool select_beztriple(BezTriple *bezt, bool selstatus, short flag, eVisible_Types hidden)
{
	if ((bezt->hide == 0) || (hidden == HIDDEN)) {
		if (selstatus == SELECT) { /* selects */
			bezt->f1 |= flag;
			bezt->f2 |= flag;
			bezt->f3 |= flag;
			return true;
		}
		else { /* deselects */
			bezt->f1 &= ~flag;
			bezt->f2 &= ~flag;
			bezt->f3 &= ~flag;
			return true;
		}
	}

	return false;
}

/* returns 1 in case (de)selection was successful */
bool select_bpoint(BPoint *bp, bool selstatus, short flag, bool hidden)
{
	if ((bp->hide == 0) || (hidden == 1)) {
		if (selstatus == SELECT) {
			bp->f1 |= flag;
			return true;
		}
		else {
			bp->f1 &= ~flag;
			return true;
		}
	}

	return false;
}

static bool swap_selection_beztriple(BezTriple *bezt)
{
	if (bezt->f2 & SELECT)
		return select_beztriple(bezt, DESELECT, SELECT, VISIBLE);
	else
		return select_beztriple(bezt, SELECT, SELECT, VISIBLE);
}

static bool swap_selection_bpoint(BPoint *bp)
{
	if (bp->f1 & SELECT)
		return select_bpoint(bp, DESELECT, SELECT, VISIBLE);
	else
		return select_bpoint(bp, SELECT, SELECT, VISIBLE);
}


void ED_curve_select_all(EditNurb *editnurb)
{
	Nurb *nu;
	int a;
	for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
		if (nu->bezt) {
			BezTriple *bezt;
			for (bezt = nu->bezt, a = 0; a < nu->pntsu; a++, bezt++) {
				if (bezt->hide == 0) {
					bezt->f1 |= SELECT;
					bezt->f2 |= SELECT;
					bezt->f3 |= SELECT;
				}
			}
		}
		else if (nu->bp) {
			BPoint *bp;
			for (bp = nu->bp, a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
				if (bp->hide == 0)
					bp->f1 |= SELECT;
			}
		}
	}
}

void ED_curve_select_swap(EditNurb *editnurb, bool hide_handles)
{
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;

	for (nu = editnurb->nurbs.first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			while (a--) {
				if (bezt->hide == 0) {
					bezt->f2 ^= SELECT; /* always do the center point */
					if (!hide_handles) {
						bezt->f1 ^= SELECT;
						bezt->f3 ^= SELECT;
					}
				}
				bezt++;
			}
		}
		else {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;
			while (a--) {
				swap_selection_bpoint(bp);
				bp++;
			}
		}
	}
}

/**
 * \param next: -1/1 for prev/next
 * \param cont: when true select continuously
 * \param selstatus: inverts behavior
 */
static void select_adjacent_cp(
        ListBase *editnurb, short next,
        const bool cont, const bool selstatus)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	bool lastsel = false;

	if (next == 0) return;

	for (nu = editnurb->first; nu; nu = nu->next) {
		lastsel = false;
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;
			bezt = nu->bezt;
			if (next < 0) bezt = &nu->bezt[a - 1];
			while (a--) {
				if (a - abs(next) < 0) break;
				if ((lastsel == 0) && (bezt->hide == 0) && ((bezt->f2 & SELECT) || (selstatus == DESELECT))) {
					bezt += next;
					if (!(bezt->f2 & SELECT) || (selstatus == DESELECT)) {
						short sel = select_beztriple(bezt, selstatus, SELECT, VISIBLE);
						if ((sel == 1) && (cont == 0)) lastsel = true;
					}
				}
				else {
					bezt += next;
					lastsel = false;
				}
				/* move around in zigzag way so that we go through each */
				bezt -= (next - next / abs(next));
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			if (next < 0) bp = &nu->bp[a - 1];
			while (a--) {
				if (a - abs(next) < 0) break;
				if ((lastsel == 0) && (bp->hide == 0) && ((bp->f1 & SELECT) || (selstatus == DESELECT))) {
					bp += next;
					if (!(bp->f1 & SELECT) || (selstatus == DESELECT)) {
						short sel = select_bpoint(bp, selstatus, SELECT, VISIBLE);
						if ((sel == 1) && (cont == 0)) lastsel = true;
					}
				}
				else {
					bp += next;
					lastsel = false;
				}
				/* move around in zigzag way so that we go through each */
				bp -= (next - next / abs(next));
			}
		}
	}
}

/**************** select start/end operators **************/

/* (de)selects first or last of visible part of each Nurb depending on selFirst
 * selFirst: defines the end of which to select
 * doswap: defines if selection state of each first/last control point is swapped
 * selstatus: selection status in case doswap is false
 */
static void selectend_nurb(Object *obedit, eEndPoint_Types selfirst, bool doswap, bool selstatus)
{
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	Curve *cu;
	int a;

	if (obedit == NULL) return;

	cu = (Curve *)obedit->data;
	cu->actvert = CU_ACT_NONE;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;

			/* which point? */
			if (selfirst == LAST) { /* select last */
				bezt = &nu->bezt[a - 1];
			}
			else { /* select first */
				bezt = nu->bezt;
			}

			while (a--) {
				bool sel;
				if (doswap) sel = swap_selection_beztriple(bezt);
				else sel = select_beztriple(bezt, selstatus, SELECT, VISIBLE);

				if (sel == true) break;
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;

			/* which point? */
			if (selfirst == LAST) { /* select last */
				bp = &nu->bp[a - 1];
			}
			else { /* select first */
				bp = nu->bp;
			}

			while (a--) {
				if (bp->hide == 0) {
					bool sel;
					if (doswap) sel = swap_selection_bpoint(bp);
					else sel = select_bpoint(bp, selstatus, SELECT, VISIBLE);

					if (sel == true) break;
				}
			}
		}
	}
}

static int de_select_first_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);

	selectend_nurb(obedit, FIRST, true, DESELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	BKE_curve_nurb_vert_active_validate(obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_de_select_first(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select First";
	ot->idname = "CURVE_OT_de_select_first";
	ot->description = "(De)select first of visible part of each NURBS";

	/* api cfirstbacks */
	ot->exec = de_select_first_exec;
	ot->poll = ED_operator_editcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int de_select_last_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);

	selectend_nurb(obedit, LAST, true, DESELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	BKE_curve_nurb_vert_active_validate(obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_de_select_last(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select Last";
	ot->idname = "CURVE_OT_de_select_last";
	ot->description = "(De)select last of visible part of each NURBS";

	/* api clastbacks */
	ot->exec = de_select_last_exec;
	ot->poll = ED_operator_editcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************* de select all operator ***************/

static bool nurb_has_selected_cps(ListBase *editnurb)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			a = nu->pntsu;
			bezt = nu->bezt;
			while (a--) {
				if (bezt->hide == 0) {
					if ((bezt->f1 & SELECT) ||
					    (bezt->f2 & SELECT) ||
					    (bezt->f3 & SELECT))
					{
						return 1;
					}
				}
				bezt++;
			}
		}
		else {
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			while (a--) {
				if ((bp->hide == 0) && (bp->f1 & SELECT)) return 1;
				bp++;
			}
		}
	}

	return 0;
}

static int de_select_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	int action = RNA_enum_get(op->ptr, "action");

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		if (nurb_has_selected_cps(editnurb))
			action = SEL_DESELECT;
	}

	switch (action) {
		case SEL_SELECT:
			ED_curve_select_all(cu->editnurb);
			break;
		case SEL_DESELECT:
			ED_curve_deselect_all(cu->editnurb);
			break;
		case SEL_INVERT:
			ED_curve_select_swap(cu->editnurb, (cu->drawflag & CU_HIDE_HANDLES) != 0);
			break;
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	BKE_curve_nurb_vert_active_validate(cu);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->idname = "CURVE_OT_select_all";
	ot->description = "(De)select all control points";

	/* api callbacks */
	ot->exec = de_select_all_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_select_all(ot);
}



/***************** select linked operator ******************/

static int select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = (Curve *)obedit->data;
	EditNurb *editnurb = cu->editnurb;
	ListBase *nurbs = &editnurb->nurbs;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for (nu = nurbs->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			while (a--) {
				if ((bezt->f1 & SELECT) || (bezt->f2 & SELECT) || (bezt->f3 & SELECT)) {
					a = nu->pntsu;
					bezt = nu->bezt;
					while (a--) {
						select_beztriple(bezt, SELECT, SELECT, VISIBLE);
						bezt++;
					}
					break;
				}
				bezt++;
			}
		}
		else {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;
			while (a--) {
				if (bp->f1 & SELECT) {
					a = nu->pntsu * nu->pntsv;
					bp = nu->bp;
					while (a--) {
						select_bpoint(bp, SELECT, SELECT, VISIBLE);
						bp++;
					}
					break;
				}
				bp++;
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

static int select_linked_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	return select_linked_exec(C, op);
}

void CURVE_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked All";
	ot->idname = "CURVE_OT_select_linked";
	ot->description = "Select all control points linked to active one";

	/* api callbacks */
	ot->exec = select_linked_exec;
	ot->invoke = select_linked_invoke;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
}


/***************** select linked pick operator ******************/

static int select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	ViewContext vc;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	const bool select = !RNA_boolean_get(op->ptr, "deselect");

	view3d_operator_needs_opengl(C);
	view3d_set_viewcontext(C, &vc);

	if (!ED_curve_pick_vert(&vc, 1, event->mval, &nu, &bezt, &bp, NULL)) {
		return OPERATOR_CANCELLED;
	}

	if (bezt) {
		a = nu->pntsu;
		bezt = nu->bezt;
		while (a--) {
			select_beztriple(bezt, select, SELECT, VISIBLE);
			bezt++;
		}
	}
	else if (bp) {
		a = nu->pntsu * nu->pntsv;
		bp = nu->bp;
		while (a--) {
			select_bpoint(bp, select, SELECT, VISIBLE);
			bp++;
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	if (!select) {
		BKE_curve_nurb_vert_active_validate(obedit->data);
	}

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Linked";
	ot->idname = "CURVE_OT_select_linked_pick";
	ot->description = "Select all control points linked to already selected ones";

	/* api callbacks */
	ot->invoke = select_linked_pick_invoke;
	ot->poll = ED_operator_editsurfcurve_region_view3d;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect linked control points rather than selecting them");
}

/***************** select row operator **********************/

static int select_row_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	static BPoint *last = NULL;
	static int direction = 0;
	Nurb *nu = NULL;
	BPoint *bp = NULL;
	int u = 0, v = 0, a, b;

	if (!BKE_curve_nurb_vert_active_get(cu, &nu, (void *)&bp))
		return OPERATOR_CANCELLED;

	if (last == bp) {
		direction = 1 - direction;
		BKE_nurbList_flag_set(editnurb, 0);
	}
	last = bp;

	u = cu->actvert % nu->pntsu;
	v = cu->actvert / nu->pntsu;
	bp = nu->bp;
	for (a = 0; a < nu->pntsv; a++) {
		for (b = 0; b < nu->pntsu; b++, bp++) {
			if (direction) {
				if (a == v) select_bpoint(bp, SELECT, SELECT, VISIBLE);
			}
			else {
				if (b == u) select_bpoint(bp, SELECT, SELECT, VISIBLE);
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_row(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Control Point Row";
	ot->idname = "CURVE_OT_select_row";
	ot->description = "Select a row of control points including active one";

	/* api callbacks */
	ot->exec = select_row_exec;
	ot->poll = ED_operator_editsurf;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** select next operator **********************/

static int select_next_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);

	select_adjacent_cp(editnurb, 1, 0, SELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_next(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Next";
	ot->idname = "CURVE_OT_select_next";
	ot->description = "Select control points following already selected ones along the curves";

	/* api callbacks */
	ot->exec = select_next_exec;
	ot->poll = ED_operator_editcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** select previous operator **********************/

static int select_previous_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);

	select_adjacent_cp(editnurb, -1, 0, SELECT);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_previous(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Previous";
	ot->idname = "CURVE_OT_select_previous";
	ot->description = "Select control points preceding already selected ones along the curves";

	/* api callbacks */
	ot->exec = select_previous_exec;
	ot->poll = ED_operator_editcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/***************** select more operator **********************/

static int select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp, *tempbp;
	int a;
	short sel = 0;

	/* note that NURBS surface is a special case because we mimic */
	/* the behavior of "select more" of mesh tools.	      */
	/* The algorithm is designed to work in planar cases so it    */
	/* may not be optimal always (example: end of NURBS sphere)   */
	if (obedit->type == OB_SURF) {
		for (nu = editnurb->first; nu; nu = nu->next) {
			BLI_bitmap *selbpoints;
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			selbpoints = BLI_BITMAP_NEW(a, "selectlist");
			while (a > 0) {
				if ((!BLI_BITMAP_TEST(selbpoints, a)) && (bp->hide == 0) && (bp->f1 & SELECT)) {
					/* upper control point */
					if (a % nu->pntsu != 0) {
						tempbp = bp - 1;
						if (!(tempbp->f1 & SELECT)) select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
					}

					/* left control point. select only if it is not selected already */
					if (a - nu->pntsu > 0) {
						sel = 0;
						tempbp = bp + nu->pntsu;
						if (!(tempbp->f1 & SELECT)) sel = select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
						/* make sure selected bpoint is discarded */
						if (sel == 1) BLI_BITMAP_ENABLE(selbpoints, a - nu->pntsu);
					}

					/* right control point */
					if (a + nu->pntsu < nu->pntsu * nu->pntsv) {
						tempbp = bp - nu->pntsu;
						if (!(tempbp->f1 & SELECT)) select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
					}

					/* lower control point. skip next bp in case selection was made */
					if (a % nu->pntsu != 1) {
						sel = 0;
						tempbp = bp + 1;
						if (!(tempbp->f1 & SELECT)) sel = select_bpoint(tempbp, SELECT, SELECT, VISIBLE);
						if (sel) {
							bp++;
							a--;
						}
					}
				}

				bp++;
				a--;
			}

			MEM_freeN(selbpoints);
		}
	}
	else {
		select_adjacent_cp(editnurb, 1, 0, SELECT);
		select_adjacent_cp(editnurb, -1, 0, SELECT);
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "CURVE_OT_select_more";
	ot->description = "Select control points directly linked to already selected ones";

	/* api callbacks */
	ot->exec = select_more_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** select less operator *****************/

/* basic method: deselect if control point doesn't have all neighbors selected */
static int select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int a;
	int sel = 0;
	short lastsel = false;

	if (obedit->type == OB_SURF) {
		for (nu = editnurb->first; nu; nu = nu->next) {
			BLI_bitmap *selbpoints;
			a = nu->pntsu * nu->pntsv;
			bp = nu->bp;
			selbpoints = BLI_BITMAP_NEW(a, "selectlist");
			while (a--) {
				if ((bp->hide == 0) && (bp->f1 & SELECT)) {
					sel = 0;

					/* check if neighbors have been selected */
					/* edges of surface are an exception */
					if ((a + 1) % nu->pntsu == 0) {
						sel++;
					}
					else {
						bp--;
						if (BLI_BITMAP_TEST(selbpoints, a + 1) || ((bp->hide == 0) && (bp->f1 & SELECT))) sel++;
						bp++;
					}

					if ((a + 1) % nu->pntsu == 1) {
						sel++;
					}
					else {
						bp++;
						if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
						bp--;
					}

					if (a + 1 > nu->pntsu * nu->pntsv - nu->pntsu) {
						sel++;
					}
					else {
						bp -= nu->pntsu;
						if (BLI_BITMAP_TEST(selbpoints, a + nu->pntsu) || ((bp->hide == 0) && (bp->f1 & SELECT))) sel++;
						bp += nu->pntsu;
					}

					if (a < nu->pntsu) {
						sel++;
					}
					else {
						bp += nu->pntsu;
						if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
						bp -= nu->pntsu;
					}

					if (sel != 4) {
						select_bpoint(bp, DESELECT, SELECT, VISIBLE);
						BLI_BITMAP_ENABLE(selbpoints, a);
					}
				}
				else {
					lastsel = false;
				}

				bp++;
			}

			MEM_freeN(selbpoints);
		}
	}
	else {
		for (nu = editnurb->first; nu; nu = nu->next) {
			lastsel = false;
			/* check what type of curve/nurb it is */
			if (nu->type == CU_BEZIER) {
				a = nu->pntsu;
				bezt = nu->bezt;
				while (a--) {
					if ((bezt->hide == 0) && (bezt->f2 & SELECT)) {
						sel = (lastsel == 1);

						/* check if neighbors have been selected */
						/* first and last are exceptions */
						if (a == nu->pntsu - 1) {
							sel++;
						}
						else {
							bezt--;
							if ((bezt->hide == 0) && (bezt->f2 & SELECT)) sel++;
							bezt++;
						}

						if (a == 0) {
							sel++;
						}
						else {
							bezt++;
							if ((bezt->hide == 0) && (bezt->f2 & SELECT)) sel++;
							bezt--;
						}

						if (sel != 2) {
							select_beztriple(bezt, DESELECT, SELECT, VISIBLE);
							lastsel = true;
						}
						else {
							lastsel = false;
						}
					}
					else {
						lastsel = false;
					}

					bezt++;
				}
			}
			else {
				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a--) {
					if ((lastsel == 0) && (bp->hide == 0) && (bp->f1 & SELECT)) {
						if (lastsel != 0) sel = 1;
						else sel = 0;

						/* first and last are exceptions */
						if (a == nu->pntsu * nu->pntsv - 1) {
							sel++;
						}
						else {
							bp--;
							if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
							bp++;
						}

						if (a == 0) {
							sel++;
						}
						else {
							bp++;
							if ((bp->hide == 0) && (bp->f1 & SELECT)) sel++;
							bp--;
						}

						if (sel != 2) {
							select_bpoint(bp, DESELECT, SELECT, VISIBLE);
							lastsel = true;
						}
						else {
							lastsel = false;
						}
					}
					else {
						lastsel = false;
					}

					bp++;
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	BKE_curve_nurb_vert_active_validate(obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "CURVE_OT_select_less";
	ot->description = "Reduce current selection by deselecting boundary elements";

	/* api callbacks */
	ot->exec = select_less_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** select random *********************/

static void curve_select_random(ListBase *editnurb, float randfac, bool select)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt = nu->bezt;
			a = nu->pntsu;
			while (a--) {
				if (!bezt->hide) {
					if (BLI_frand() < randfac) {
						select_beztriple(bezt, select, SELECT, VISIBLE);
					}
				}
				bezt++;
			}
		}
		else {
			bp = nu->bp;
			a = nu->pntsu * nu->pntsv;

			while (a--) {
				if (!bp->hide) {
					if (BLI_frand() < randfac) {
						select_bpoint(bp, select, SELECT, VISIBLE);
					}
				}
				bp++;
			}
		}
	}
}

static int curve_select_random_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb = object_editcurve_get(obedit);
	const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
	const float randfac = RNA_float_get(op->ptr, "percent") / 100.0f;

	curve_select_random(editnurb, randfac, select);
	BKE_curve_nurb_vert_active_validate(obedit->data);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Random";
	ot->idname = "CURVE_OT_select_random";
	ot->description = "Randomly select some control points";

	/* api callbacks */
	ot->exec = curve_select_random_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_percentage(ot->srna, "percent", 50.f, 0.0f, 100.0f,
	                         "Percent", "Percentage of elements to select randomly", 0.0f, 100.0f);
	WM_operator_properties_select_action_simple(ot, SEL_SELECT);
}

/********************* every nth number of point *******************/

static void select_nth_bezt(Nurb *nu, BezTriple *bezt, int nth, int skip, int offset)
{
	int a, start;

	start = bezt - nu->bezt;
	a = nu->pntsu;
	bezt = &nu->bezt[a - 1];

	while (a--) {
		const int depth = abs(start - a);
		if ((offset + depth) % (skip + nth) >= skip) {
			select_beztriple(bezt, DESELECT, SELECT, HIDDEN);
		}

		bezt--;
	}
}

static void select_nth_bp(Nurb *nu, BPoint *bp, int nth, int skip, int offset)
{
	int a, startrow, startpnt;
	int row, pnt;

	startrow = (bp - nu->bp) / nu->pntsu;
	startpnt = (bp - nu->bp) % nu->pntsu;

	a = nu->pntsu * nu->pntsv;
	bp = &nu->bp[a - 1];
	row = nu->pntsv - 1;
	pnt = nu->pntsu - 1;

	while (a--) {
		const int depth = abs(pnt - startpnt) + abs(row - startrow);
		if ((offset + depth) % (skip + nth) >= skip) {
			select_bpoint(bp, DESELECT, SELECT, HIDDEN);
		}

		pnt--;
		if (pnt < 0) {
			pnt = nu->pntsu - 1;
			row--;
		}

		bp--;
	}
}

bool ED_curve_select_nth(Curve *cu, int nth, int skip, int offset)
{
	Nurb *nu = NULL;
	void *vert = NULL;

	if (!BKE_curve_nurb_vert_active_get(cu, &nu, &vert))
		return false;

	if (nu->bezt) {
		select_nth_bezt(nu, vert, nth, skip, offset);
	}
	else {
		select_nth_bp(nu, vert, nth, skip, offset);
	}

	return true;
}

static int select_nth_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	const int nth = RNA_int_get(op->ptr, "nth") - 1;
	const int skip = RNA_int_get(op->ptr, "skip");
	int offset = RNA_int_get(op->ptr, "offset");

	/* so input of offset zero ends up being (nth - 1) */
	offset = mod_i(offset, nth + skip);

	if (!ED_curve_select_nth(obedit->data, nth, skip, offset)) {
		if (obedit->type == OB_SURF) {
			BKE_report(op->reports, RPT_ERROR, "Surface has not got active point");
		}
		else {
			BKE_report(op->reports, RPT_ERROR, "Curve has not got active point");
		}

		return OPERATOR_CANCELLED;
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void CURVE_OT_select_nth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Checker Deselect";
	ot->description = "Deselect every other vertex";
	ot->idname = "CURVE_OT_select_nth";

	/* api callbacks */
	ot->exec = select_nth_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "nth", 2, 2, INT_MAX, "Nth Selection", "", 2, 100);
	RNA_def_int(ot->srna, "skip", 1, 1, INT_MAX, "Skip", "", 1, 100);
	RNA_def_int(ot->srna, "offset", 0, INT_MIN, INT_MAX, "Offset", "", -100, 100);
}


/* -------------------------------------------------------------------- */
/* Select Similar */

/** \name Select Similar
 * \{ */

enum {
	SIM_CMP_EQ = 0,
	SIM_CMP_GT,
	SIM_CMP_LT,
};

static EnumPropertyItem curve_prop_similar_compare_types[] = {
	{SIM_CMP_EQ, "EQUAL", 0, "Equal", ""},
	{SIM_CMP_GT, "GREATER", 0, "Greater", ""},
	{SIM_CMP_LT, "LESS", 0, "Less", ""},

	{0, NULL, 0, NULL, NULL}
};

enum {
	SIMCURHAND_TYPE = 0,
	SIMCURHAND_RADIUS,
	SIMCURHAND_WEIGHT,
	SIMCURHAND_DIRECTION,
};

static EnumPropertyItem curve_prop_similar_types[] = {
	{SIMCURHAND_TYPE, "TYPE", 0, "Type", ""},
	{SIMCURHAND_RADIUS, "RADIUS", 0, "Radius", ""},
	{SIMCURHAND_WEIGHT, "WEIGHT", 0, "Weight", ""},
	{SIMCURHAND_DIRECTION, "DIRECTION", 0, "Direction", ""},
	{0, NULL, 0, NULL, NULL}
};

static int curve_select_similar_cmp_fl(const float delta, const float thresh, const int compare)
{
	switch (compare) {
		case SIM_CMP_EQ:
			return (fabsf(delta) <= thresh);
		case SIM_CMP_GT:
			return ((delta + thresh) >= 0.0f);
		case SIM_CMP_LT:
			return ((delta - thresh) <= 0.0f);
		default:
			BLI_assert(0);
			return 0;
	}
}


static void curve_select_similar_direction__bezt(Nurb *nu, const float dir_ref[3], float angle_cos)
{
	BezTriple *bezt;
	int i;

	for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
		if (!bezt->hide) {
			float dir[3];
			BKE_nurb_bezt_calc_normal(nu, bezt, dir);
			if (fabsf(dot_v3v3(dir_ref, dir)) >= angle_cos) {
				select_beztriple(bezt, SELECT, SELECT, VISIBLE);
			}
		}
	}
}

static void curve_select_similar_direction__bp(Nurb *nu, const float dir_ref[3], float angle_cos)
{
	BPoint *bp;
	int i;

	for (i = nu->pntsu, bp = nu->bp; i--; bp++) {
		if (!bp->hide) {
			float dir[3];
			BKE_nurb_bpoint_calc_normal(nu, bp, dir);
			if (fabsf(dot_v3v3(dir_ref, dir)) >= angle_cos) {
				select_bpoint(bp, SELECT, SELECT, VISIBLE);
			}
		}
	}
}

static bool curve_select_similar_direction(ListBase *editnurb, Curve *cu, float thresh)
{
	Nurb *nu, *act_nu;
	void *act_vert;
	float dir[3];
	float angle_cos;

	if (!BKE_curve_nurb_vert_active_get(cu, &act_nu, &act_vert)) {
		return false;
	}

	if (act_nu->type == CU_BEZIER) {
		BKE_nurb_bezt_calc_normal(act_nu, act_vert, dir);
	}
	else {
		BKE_nurb_bpoint_calc_normal(act_nu, act_vert, dir);
	}

	/* map 0-1 to radians, 'cos' for comparison */
	angle_cos = cosf(thresh * M_PI_2);

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			curve_select_similar_direction__bezt(nu, dir, angle_cos);
		}
		else {
			curve_select_similar_direction__bp(nu, dir, angle_cos);
		}
	}

	return true;
}

static void curve_select_similar_radius__bezt(Nurb *nu, float radius_ref, int compare, float thresh)
{
	BezTriple *bezt;
	int i;

	for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
		if (!bezt->hide) {
			if (curve_select_similar_cmp_fl(bezt->radius - radius_ref, thresh, compare)) {
				select_beztriple(bezt, SELECT, SELECT, VISIBLE);
			}
		}
	}
}

static void curve_select_similar_radius__bp(Nurb *nu, float radius_ref, int compare, float thresh)
{
	BPoint *bp;
	int i;

	for (i = nu->pntsu * nu->pntsv, bp = nu->bp; i--; bp++) {
		if (!bp->hide) {
			if (curve_select_similar_cmp_fl(bp->radius - radius_ref, thresh, compare)) {
				select_bpoint(bp, SELECT, SELECT, VISIBLE);
			}
		}
	}
}

static bool curve_select_similar_radius(ListBase *editnurb, Curve *cu, float compare, float thresh)
{
	Nurb *nu, *act_nu;
	void *act_vert;
	float radius_ref;

	if (!BKE_curve_nurb_vert_active_get(cu, &act_nu, &act_vert)) {
		return false;
	}

	if (act_nu->type == CU_BEZIER) {
		radius_ref = ((BezTriple *)act_vert)->radius;
	}
	else {
		radius_ref = ((BPoint *)act_vert)->radius;
	}

	/* make relative */
	thresh *= radius_ref;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			curve_select_similar_radius__bezt(nu, radius_ref, compare, thresh);
		}
		else {
			curve_select_similar_radius__bp(nu, radius_ref, compare, thresh);
		}
	}

	return true;
}

static void curve_select_similar_weight__bezt(Nurb *nu, float weight_ref, int compare, float thresh)
{
	BezTriple *bezt;
	int i;

	for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
		if (!bezt->hide) {
			if (curve_select_similar_cmp_fl(bezt->weight - weight_ref, thresh, compare)) {
				select_beztriple(bezt, SELECT, SELECT, VISIBLE);
			}
		}
	}
}

static void curve_select_similar_weight__bp(Nurb *nu, float weight_ref, int compare, float thresh)
{
	BPoint *bp;
	int i;

	for (i = nu->pntsu * nu->pntsv, bp = nu->bp; i--; bp++) {
		if (!bp->hide) {
			if (curve_select_similar_cmp_fl(bp->weight - weight_ref, thresh, compare)) {
				select_bpoint(bp, SELECT, SELECT, VISIBLE);
			}
		}
	}
}

static bool curve_select_similar_weight(ListBase *editnurb, Curve *cu, float compare, float thresh)
{
	Nurb *nu, *act_nu;
	void *act_vert;
	float weight_ref;

	if (!BKE_curve_nurb_vert_active_get(cu, &act_nu, &act_vert))
		return false;

	if (act_nu->type == CU_BEZIER) {
		weight_ref = ((BezTriple *)act_vert)->weight;
	}
	else {
		weight_ref = ((BPoint *)act_vert)->weight;
	}

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == CU_BEZIER) {
			curve_select_similar_weight__bezt(nu, weight_ref, compare, thresh);
		}
		else {
			curve_select_similar_weight__bp(nu, weight_ref, compare, thresh);
		}
	}

	return true;
}

static void curve_select_all__bezt(Nurb *nu)
{
	BezTriple *bezt;
	int i;

	for (i = nu->pntsu, bezt = nu->bezt; i--; bezt++) {
		if (!bezt->hide) {
			select_beztriple(bezt, SELECT, SELECT, VISIBLE);
		}
	}
}

static void curve_select_all__bp(Nurb *nu)
{
	BPoint *bp;
	int i;

	for (i = nu->pntsu * nu->pntsv, bp = nu->bp; i--; bp++) {
		if (!bp->hide) {
			select_bpoint(bp, SELECT, SELECT, VISIBLE);
		}
	}
}

static bool curve_select_similar_type(ListBase *editnurb, Curve *cu)
{
	Nurb *nu, *act_nu;
	int type_ref;

	/* Get active nurb type */
	act_nu = BKE_curve_nurb_active_get(cu);

	if (!act_nu)
		return false;

	/* Get the active nurb type */
	type_ref = act_nu->type;

	for (nu = editnurb->first; nu; nu = nu->next) {
		if (nu->type == type_ref) {
			if (type_ref == CU_BEZIER) {
				curve_select_all__bezt(nu);
			}
			else {
				curve_select_all__bp(nu);
			}
		}
	}

	return true;
}

static int curve_select_similar_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Curve *cu = obedit->data;
	ListBase *editnurb = object_editcurve_get(obedit);
	bool changed = false;

	/* Get props */
	const int type = RNA_enum_get(op->ptr, "type");
	const float thresh = RNA_float_get(op->ptr, "threshold");
	const int compare = RNA_enum_get(op->ptr, "compare");

	switch (type) {
		case SIMCURHAND_TYPE:
			changed = curve_select_similar_type(editnurb, cu);
			break;
		case SIMCURHAND_RADIUS:
			changed = curve_select_similar_radius(editnurb, cu, compare, thresh);
			break;
		case SIMCURHAND_WEIGHT:
			changed = curve_select_similar_weight(editnurb, cu, compare, thresh);
			break;
		case SIMCURHAND_DIRECTION:
			changed = curve_select_similar_direction(editnurb, cu, thresh);
			break;
	}

	if (changed) {
		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void CURVE_OT_select_similar(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Similar";
	ot->idname = "CURVE_OT_select_similar";
	ot->description = "Select similar curve points by property type";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = curve_select_similar_exec;
	ot->poll = ED_operator_editsurfcurve;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", curve_prop_similar_types, SIMCURHAND_WEIGHT, "Type", "");
	RNA_def_enum(ot->srna, "compare", curve_prop_similar_compare_types, SIM_CMP_EQ, "Compare", "");
	RNA_def_float(ot->srna, "threshold", 0.1, 0.0, FLT_MAX, "Threshold", "", 0.0, 100.0);
}

/** \} */
