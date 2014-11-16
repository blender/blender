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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_lattice.c
 *  \ingroup edobj
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_rand.h"
#include "BLI_bitmap.h"

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_deform.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "ED_lattice.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/********************** Load/Make/Free ********************/

void free_editLatt(Object *ob)
{
	Lattice *lt = ob->data;
	
	if (lt->editlatt) {
		Lattice *editlt = lt->editlatt->latt;

		if (editlt->def)
			MEM_freeN(editlt->def);
		if (editlt->dvert)
			BKE_defvert_array_free(editlt->dvert, editlt->pntsu * editlt->pntsv * editlt->pntsw);

		MEM_freeN(editlt);
		MEM_freeN(lt->editlatt);

		lt->editlatt = NULL;
	}
}

void make_editLatt(Object *obedit)
{
	Lattice *lt = obedit->data;
	KeyBlock *actkey;

	free_editLatt(obedit);

	actkey = BKE_keyblock_from_object(obedit);
	if (actkey)
		BKE_keyblock_convert_to_lattice(actkey, lt);

	lt->editlatt = MEM_callocN(sizeof(EditLatt), "editlatt");
	lt->editlatt->latt = MEM_dupallocN(lt);
	lt->editlatt->latt->def = MEM_dupallocN(lt->def);

	if (lt->dvert) {
		int tot = lt->pntsu * lt->pntsv * lt->pntsw;
		lt->editlatt->latt->dvert = MEM_mallocN(sizeof(MDeformVert) * tot, "Lattice MDeformVert");
		BKE_defvert_array_copy(lt->editlatt->latt->dvert, lt->dvert, tot);
	}

	if (lt->key) lt->editlatt->shapenr = obedit->shapenr;
}

void load_editLatt(Object *obedit)
{
	Lattice *lt, *editlt;
	KeyBlock *actkey;
	BPoint *bp;
	float *fp;
	int tot;

	lt = obedit->data;
	editlt = lt->editlatt->latt;

	if (lt->editlatt->shapenr) {
		actkey = BLI_findlink(&lt->key->block, lt->editlatt->shapenr - 1);

		/* active key: vertices */
		tot = editlt->pntsu * editlt->pntsv * editlt->pntsw;
		
		if (actkey->data) MEM_freeN(actkey->data);
		
		fp = actkey->data = MEM_callocN(lt->key->elemsize * tot, "actkey->data");
		actkey->totelem = tot;

		bp = editlt->def;
		while (tot--) {
			copy_v3_v3(fp, bp->vec);
			fp += 3;
			bp++;
		}
	}
	else {
		MEM_freeN(lt->def);

		lt->def = MEM_dupallocN(editlt->def);

		lt->flag = editlt->flag;

		lt->pntsu = editlt->pntsu;
		lt->pntsv = editlt->pntsv;
		lt->pntsw = editlt->pntsw;
		
		lt->typeu = editlt->typeu;
		lt->typev = editlt->typev;
		lt->typew = editlt->typew;
		lt->actbp = editlt->actbp;
	}

	if (lt->dvert) {
		BKE_defvert_array_free(lt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);
		lt->dvert = NULL;
	}

	if (editlt->dvert) {
		tot = lt->pntsu * lt->pntsv * lt->pntsw;

		lt->dvert = MEM_mallocN(sizeof(MDeformVert) * tot, "Lattice MDeformVert");
		BKE_defvert_array_copy(lt->dvert, editlt->dvert, tot);
	}
}

static void bpoint_select_set(BPoint *bp, bool select)
{
	if (select) {
		if (!bp->hide) {
			bp->f1 |= SELECT;
		}
	}
	else {
		bp->f1 &= ~SELECT;
	}
}

/************************** Select Random Operator **********************/

static int lattice_select_random_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
	const float randfac = RNA_float_get(op->ptr, "percent") / 100.0f;
	const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);

	int tot;
	BPoint *bp;

	tot = lt->pntsu * lt->pntsv * lt->pntsw;
	bp = lt->def;
	while (tot--) {
		if (!bp->hide) {
			if (BLI_frand() < randfac) {
				bpoint_select_set(bp, select);
			}
		}
		bp++;
	}

	if (select == false) {
		lt->actbp = LT_ACTBP_NONE;
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void LATTICE_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Random";
	ot->description = "Randomly select UVW control points";
	ot->idname = "LATTICE_OT_select_random";

	/* api callbacks */
	ot->exec = lattice_select_random_exec;
	ot->poll = ED_operator_editlattice;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_float_percentage(ot->srna, "percent", 50.f, 0.0f, 100.0f,
	                         "Percent", "Percentage of elements to select randomly", 0.f, 100.0f);
	WM_operator_properties_select_action_simple(ot, SEL_SELECT);
}


/* -------------------------------------------------------------------- */
/* Select Mirror Operator */

static int lattice_select_mirror_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
	const bool extend = RNA_boolean_get(op->ptr, "extend");
	const int axis = RNA_enum_get(op->ptr, "axis");
	bool flip_uvw[3] = {false};
	int tot, i;
	BPoint *bp;
	BLI_bitmap *selpoints;

	tot = lt->pntsu * lt->pntsv * lt->pntsw;

	flip_uvw[axis] = true;

	if (!extend) {
		lt->actbp = LT_ACTBP_NONE;
	}

	/* store "original" selection */
	selpoints = BLI_BITMAP_NEW(tot, __func__);
	BKE_lattice_bitmap_from_flag(lt, selpoints, SELECT, false, false);

	/* actual (de)selection */
	for (i = 0; i < tot; i++) {
		const int i_flip = BKE_lattice_index_flip(lt, i, flip_uvw[0], flip_uvw[1], flip_uvw[2]);
		bp = &lt->def[i];
		if (!bp->hide) {
			if (BLI_BITMAP_TEST(selpoints, i_flip)) {
				bp->f1 |= SELECT;
			}
			else {
				if (!extend) {
					bp->f1 &= ~SELECT;
				}
			}
		}
	}


	MEM_freeN(selpoints);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void LATTICE_OT_select_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Mirror";
	ot->description = "Select mirrored lattice points";
	ot->idname = "LATTICE_OT_select_mirror";

	/* api callbacks */
	ot->exec = lattice_select_mirror_exec;
	ot->poll = ED_operator_editlattice;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "axis", object_axis_unsigned_items, 0, "Axis", "");

	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}


/************************** Select More/Less Operator *************************/

static bool lattice_test_bitmap_uvw(Lattice *lt, BLI_bitmap *selpoints, int u, int v, int w, const bool selected)
{
	if ((u < 0 || u >= lt->pntsu) ||
	    (v < 0 || v >= lt->pntsv) ||
	    (w < 0 || w >= lt->pntsw))
	{
		return false;
	}
	else {
		int i = BKE_lattice_index_from_uvw(lt, u, v, w);
		if (lt->def[i].hide == 0) {
			return (BLI_BITMAP_TEST(selpoints, i) != 0) == selected;
		}
		return false;
	}
}

static int lattice_select_more_less(bContext *C, const bool select)
{
	Object *obedit = CTX_data_edit_object(C);
	Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
	BPoint *bp;
	const int tot = lt->pntsu * lt->pntsv * lt->pntsw;
	int u, v, w;
	BLI_bitmap *selpoints;

	lt->actbp = LT_ACTBP_NONE;

	selpoints = BLI_BITMAP_NEW(tot, __func__);
	BKE_lattice_bitmap_from_flag(lt, selpoints, SELECT, false, false);

	bp = lt->def;
	for (w = 0; w < lt->pntsw; w++) {
		for (v = 0; v < lt->pntsv; v++) {
			for (u = 0; u < lt->pntsu; u++) {
				if ((bp->hide == 0) && (((bp->f1 & SELECT) == 0) == select)) {
					if (lattice_test_bitmap_uvw(lt, selpoints, u + 1, v, w, select) ||
					    lattice_test_bitmap_uvw(lt, selpoints, u - 1, v, w, select) ||
					    lattice_test_bitmap_uvw(lt, selpoints, u, v + 1, w, select) ||
					    lattice_test_bitmap_uvw(lt, selpoints, u, v - 1, w, select) ||
					    lattice_test_bitmap_uvw(lt, selpoints, u, v, w + 1, select) ||
					    lattice_test_bitmap_uvw(lt, selpoints, u, v, w - 1, select))
					{
						BKE_BIT_TEST_SET(bp->f1, select, SELECT);
					}
				}
				bp++;
			}
		}
	}

	MEM_freeN(selpoints);

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
	return OPERATOR_FINISHED;
}

static int lattice_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	return lattice_select_more_less(C, true);
}

static int lattice_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	return lattice_select_more_less(C, false);
}

void LATTICE_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->description = "Select vertex directly linked to already selected ones";
	ot->idname = "LATTICE_OT_select_more";

	/* api callbacks */
	ot->exec = lattice_select_more_exec;
	ot->poll = ED_operator_editlattice;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void LATTICE_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->description = "Deselect vertices at the boundary of each selection region";
	ot->idname = "LATTICE_OT_select_less";

	/* api callbacks */
	ot->exec = lattice_select_less_exec;
	ot->poll = ED_operator_editlattice;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Select All Operator *************************/

void ED_setflagsLatt(Object *obedit, int flag)
{
	Lattice *lt = obedit->data;
	BPoint *bp;
	int a;
	
	bp = lt->editlatt->latt->def;
	
	a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
	lt->editlatt->latt->actbp = LT_ACTBP_NONE;

	while (a--) {
		if (bp->hide == 0) {
			bp->f1 = flag;
		}
		bp++;
	}
}

static int lattice_select_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Lattice *lt = obedit->data;
	BPoint *bp;
	int a;
	int action = RNA_enum_get(op->ptr, "action");

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;

		bp = lt->editlatt->latt->def;
		a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;

		while (a--) {
			if (bp->hide == 0) {
				if (bp->f1 & SELECT) {
					action = SEL_DESELECT;
					break;
				}
			}
			bp++;
		}
	}

	switch (action) {
		case SEL_SELECT:
			ED_setflagsLatt(obedit, 1);
			break;
		case SEL_DESELECT:
			ED_setflagsLatt(obedit, 0);
			break;
		case SEL_INVERT:
			bp = lt->editlatt->latt->def;
			a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
			lt->editlatt->latt->actbp = LT_ACTBP_NONE;

			while (a--) {
				if (bp->hide == 0) {
					bp->f1 ^= SELECT;
				}
				bp++;
			}
			break;
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void LATTICE_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->description = "Change selection of all UVW control points";
	ot->idname = "LATTICE_OT_select_all";
	
	/* api callbacks */
	ot->exec = lattice_select_all_exec;
	ot->poll = ED_operator_editlattice;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/************************** Select Ungrouped Verts Operator *************************/

static int lattice_select_ungrouped_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
	MDeformVert *dv;
	BPoint *bp;
	int a, tot;

	if (BLI_listbase_is_empty(&obedit->defbase) || lt->dvert == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No weights/vertex groups on object");
		return OPERATOR_CANCELLED;
	}

	if (!RNA_boolean_get(op->ptr, "extend")) {
		ED_setflagsLatt(obedit, 0);
	}

	dv = lt->dvert;
	tot = lt->pntsu * lt->pntsv * lt->pntsw;

	for (a = 0, bp = lt->def; a < tot; a++, bp++, dv++) {
		if (bp->hide == 0) {
			if (dv->dw == NULL) {
				bp->f1 |= SELECT;
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void LATTICE_OT_select_ungrouped(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Ungrouped";
	ot->idname = "LATTICE_OT_select_ungrouped";
	ot->description = "Select vertices without a group";

	/* api callbacks */
	ot->exec = lattice_select_ungrouped_exec;
	ot->poll = ED_operator_editlattice;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/************************** Make Regular Operator *************************/

static int make_regular_poll(bContext *C)
{
	Object *ob;

	if (ED_operator_editlattice(C)) return 1;

	ob = CTX_data_active_object(C);
	return (ob && ob->type == OB_LATTICE);
}

static int make_regular_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_edit_object(C);
	Lattice *lt;
	
	if (ob) {
		lt = ob->data;
		BKE_lattice_resize(lt->editlatt->latt, lt->pntsu, lt->pntsv, lt->pntsw, NULL);
	}
	else {
		ob = CTX_data_active_object(C);
		lt = ob->data;
		BKE_lattice_resize(lt, lt->pntsu, lt->pntsv, lt->pntsw, NULL);
	}
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void LATTICE_OT_make_regular(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Regular";
	ot->description = "Set UVW control points a uniform distance apart";
	ot->idname = "LATTICE_OT_make_regular";
	
	/* api callbacks */
	ot->exec = make_regular_exec;
	ot->poll = make_regular_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************** Flip Verts Operator *************************/

/* flipping options */
typedef enum eLattice_FlipAxes {
	LATTICE_FLIP_U = 0,
	LATTICE_FLIP_V = 1,
	LATTICE_FLIP_W = 2
} eLattice_FlipAxes;

/* Flip midpoint value so that relative distances between midpoint and neighbor-pair is maintained
 * ! Assumes that uvw <=> xyz (i.e. axis-aligned index-axes with coordinate-axes)
 * - Helper for lattice_flip_exec()
 */
static void lattice_flip_point_value(Lattice *lt, int u, int v, int w, float mid, eLattice_FlipAxes axis)
{
	BPoint *bp;
	float diff;
	
	/* just the point in the middle (unpaired) */
	bp = &lt->def[BKE_lattice_index_from_uvw(lt, u, v, w)];
	
	/* flip over axis */
	diff = mid - bp->vec[axis];
	bp->vec[axis] = mid + diff;
}

/* Swap pairs of lattice points along a specified axis
 * - Helper for lattice_flip_exec()
 */
static void lattice_swap_point_pairs(Lattice *lt, int u, int v, int w, float mid, eLattice_FlipAxes axis)
{
	BPoint *bpA, *bpB;
	
	int numU = lt->pntsu;
	int numV = lt->pntsv;
	int numW = lt->pntsw;
	
	int u0 = u, u1 = u;
	int v0 = v, v1 = v;
	int w0 = w, w1 = w;
	
	/* get pair index by just overriding the relevant pair-value
	 * - "-1" else buffer overflow
	 */
	switch (axis) {
		case LATTICE_FLIP_U:
			u1 = numU - u - 1;
			break;
		case LATTICE_FLIP_V:
			v1 = numV - v - 1;
			break;
		case LATTICE_FLIP_W:
			w1 = numW - w - 1;
			break;
	}
	
	/* get points to operate on */
	bpA = &lt->def[BKE_lattice_index_from_uvw(lt, u0, v0, w0)];
	bpB = &lt->def[BKE_lattice_index_from_uvw(lt, u1, v1, w1)];
	
	/* Swap all coordinates, so that flipped coordinates belong to
	 * the indices on the correct side of the lattice.
	 *
	 *   Coords:  (-2 4) |0| (3 4)   --> (3 4) |0| (-2 4) 
	 *   Indices:  (0,L)     (1,R)   --> (0,L)     (1,R)
	 */
	swap_v3_v3(bpA->vec, bpB->vec);
	
	/* However, we need to mirror the coordinate values on the axis we're dealing with,
	 * otherwise we'd have effectively only rotated the points around. If we don't do this,
	 * we'd just be reimplementing the naive mirroring algorithm, which causes unwanted deforms
	 * such as flipped normals, etc.
	 *
	 *   Coords:  (3 4) |0| (-2 4)  --\   
	 *                                 \-> (-3 4) |0| (2 4)
	 *   Indices: (0,L)     (1,R)   -->     (0,L)     (1,R)
	 */
	lattice_flip_point_value(lt, u0, v0, w0, mid, axis);
	lattice_flip_point_value(lt, u1, v1, w1, mid, axis);
}
	
static int lattice_flip_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Lattice *lt;
	
	eLattice_FlipAxes axis = RNA_enum_get(op->ptr, "axis");
	int numU, numV, numW;
	int totP;
	
	float mid = 0.0f;
	short isOdd = 0;
	
	/* get lattice - we need the "edit lattice" from the lattice... confusing... */
	lt = (Lattice *)obedit->data;
	lt = lt->editlatt->latt;
	
	numU = lt->pntsu;
	numV = lt->pntsv;
	numW = lt->pntsw;
	totP = numU * numV * numW;
	
	/* First Pass: determine midpoint - used for flipping center verts if there are odd number of points on axis */
	switch (axis) {
		case LATTICE_FLIP_U:
			isOdd = numU & 1;
			break;
		case LATTICE_FLIP_V:
			isOdd = numV & 1;
			break;
		case LATTICE_FLIP_W:
			isOdd = numW & 1;
			break;
			
		default:
			printf("lattice_flip(): Unknown flipping axis (%d)\n", axis);
			return OPERATOR_CANCELLED;
	}
	
	if (isOdd) {
		BPoint *bp;
		float avgInv = 1.0f / (float)totP;
		int i;
		
		/* midpoint calculation - assuming that u/v/w are axis-aligned */
		for (i = 0, bp = lt->def; i < totP; i++, bp++) {
			mid += bp->vec[axis] * avgInv;
		}
	}
	
	/* Second Pass: swap pairs of vertices per axis, assuming they are all sorted */
	switch (axis) {
		case LATTICE_FLIP_U:
		{
			int u, v, w;
			
			/* v/w strips - front to back, top to bottom */
			for (w = 0; w < numW; w++) {
				for (v = 0; v < numV; v++) {
					/* swap coordinates of pairs of vertices on u */
					for (u = 0; u < (numU / 2); u++) {
						lattice_swap_point_pairs(lt, u, v, w, mid, axis);
					}
					
					/* flip u-coordinate of midpoint (i.e. unpaired point on u) */
					if (isOdd) {
						u = (numU / 2);
						lattice_flip_point_value(lt, u, v, w, mid, axis);
					}
				}
			}
			break;
		}
		case LATTICE_FLIP_V:
		{
			int u, v, w;
			
			/* u/w strips - front to back, left to right */
			for (w = 0; w < numW; w++) {
				for (u = 0; u < numU; u++) {
					/* swap coordinates of pairs of vertices on v */
					for (v = 0; v < (numV / 2); v++) {
						lattice_swap_point_pairs(lt, u, v, w, mid, axis);
					}
					
					/* flip v-coordinate of midpoint (i.e. unpaired point on v) */
					if (isOdd) {
						v = (numV / 2);
						lattice_flip_point_value(lt, u, v, w, mid, axis);
					}
				}
			}
			break;
		}
		case LATTICE_FLIP_W:
		{
			int u, v, w;
			
			for (v = 0; v < numV; v++) {
				for (u = 0; u < numU; u++) {
					/* swap coordinates of pairs of vertices on w */
					for (w = 0; w < (numW / 2); w++) {
						lattice_swap_point_pairs(lt, u, v, w, mid, axis);
					}
					
					/* flip w-coordinate of midpoint (i.e. unpaired point on w) */
					if (isOdd) {
						w = (numW / 2);
						lattice_flip_point_value(lt, u, v, w, mid, axis);
					}
				}
			}
			break;
		}
		default: /* shouldn't happen, but just in case */
			break;
	}
	
	/* updates */
	DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
	
	return OPERATOR_FINISHED;
}

void LATTICE_OT_flip(wmOperatorType *ot)
{
	static EnumPropertyItem flip_items[] = {
		{LATTICE_FLIP_U, "U", 0, "U (X) Axis", ""},
		{LATTICE_FLIP_V, "V", 0, "V (Y) Axis", ""},
		{LATTICE_FLIP_W, "W", 0, "W (Z) Axis", ""},
		{0, NULL, 0, NULL, NULL}};
	
	/* identifiers */
	ot->name = "Flip (Distortion Free)";
	ot->description = "Mirror all control points without inverting the lattice deform";
	ot->idname = "LATTICE_OT_flip";
	
	/* api callbacks */
	ot->poll = ED_operator_editlattice;
	ot->invoke = WM_menu_invoke;
	ot->exec = lattice_flip_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "axis", flip_items, LATTICE_FLIP_U, "Flip Axis", "Coordinates along this axis get flipped");
}

/****************************** Mouse Selection *************************/

static void findnearestLattvert__doClosest(void *userData, BPoint *bp, const float screen_co[2])
{
	struct { BPoint *bp; float dist; int select; float mval_fl[2]; } *data = userData;
	float dist_test = len_manhattan_v2v2(data->mval_fl, screen_co);
	
	if ((bp->f1 & SELECT) && data->select)
		dist_test += 5.0f;

	if (dist_test < data->dist) {
		data->dist = dist_test;

		data->bp = bp;
	}
}

static BPoint *findnearestLattvert(ViewContext *vc, const int mval[2], int sel)
{
	/* (sel == 1): selected gets a disadvantage */
	/* in nurb and bezt or bp the nearest is written */
	/* return 0 1 2: handlepunt */
	struct { BPoint *bp; float dist; int select; float mval_fl[2]; } data = {NULL};

	data.dist = ED_view3d_select_dist_px();
	data.select = sel;
	data.mval_fl[0] = mval[0];
	data.mval_fl[1] = mval[1];

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
	lattice_foreachScreenVert(vc, findnearestLattvert__doClosest, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

	return data.bp;
}

bool mouse_lattice(bContext *C, const int mval[2], bool extend, bool deselect, bool toggle)
{
	ViewContext vc;
	BPoint *bp = NULL;
	Lattice *lt;

	view3d_set_viewcontext(C, &vc);
	lt = ((Lattice *)vc.obedit->data)->editlatt->latt;
	bp = findnearestLattvert(&vc, mval, true);

	if (bp) {
		if (extend) {
			bp->f1 |= SELECT;
		}
		else if (deselect) {
			bp->f1 &= ~SELECT;
		}
		else if (toggle) {
			bp->f1 ^= SELECT;  /* swap */
		}
		else {
			ED_setflagsLatt(vc.obedit, 0);
			bp->f1 |= SELECT;
		}

		if (bp->f1 & SELECT) {
			lt->actbp = bp - lt->def;
		}
		else {
			lt->actbp = LT_ACTBP_NONE;
		}

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);

		return true;
	}

	return false;
}

/******************************** Undo *************************/

typedef struct UndoLattice {
	BPoint *def;
	int pntsu, pntsv, pntsw, actbp;
} UndoLattice;

static void undoLatt_to_editLatt(void *data, void *edata, void *UNUSED(obdata))
{
	UndoLattice *ult = (UndoLattice *)data;
	EditLatt *editlatt = (EditLatt *)edata;
	int a = editlatt->latt->pntsu * editlatt->latt->pntsv * editlatt->latt->pntsw;

	memcpy(editlatt->latt->def, ult->def, a * sizeof(BPoint));
	editlatt->latt->actbp = ult->actbp;
}

static void *editLatt_to_undoLatt(void *edata, void *UNUSED(obdata))
{
	UndoLattice *ult = MEM_callocN(sizeof(UndoLattice), "UndoLattice");
	EditLatt *editlatt = (EditLatt *)edata;
	
	ult->def = MEM_dupallocN(editlatt->latt->def);
	ult->pntsu = editlatt->latt->pntsu;
	ult->pntsv = editlatt->latt->pntsv;
	ult->pntsw = editlatt->latt->pntsw;
	ult->actbp = editlatt->latt->actbp;
	
	return ult;
}

static void free_undoLatt(void *data)
{
	UndoLattice *ult = (UndoLattice *)data;

	if (ult->def) MEM_freeN(ult->def);
	MEM_freeN(ult);
}

static int validate_undoLatt(void *data, void *edata)
{
	UndoLattice *ult = (UndoLattice *)data;
	EditLatt *editlatt = (EditLatt *)edata;

	return (ult->pntsu == editlatt->latt->pntsu &&
	        ult->pntsv == editlatt->latt->pntsv &&
	        ult->pntsw == editlatt->latt->pntsw);
}

static void *get_editlatt(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);

	if (obedit && obedit->type == OB_LATTICE) {
		Lattice *lt = obedit->data;
		return lt->editlatt;
	}

	return NULL;
}

/* and this is all the undo system needs to know */
void undo_push_lattice(bContext *C, const char *name)
{
	undo_editmode_push(C, name, get_editlatt, free_undoLatt, undoLatt_to_editLatt, editLatt_to_undoLatt, validate_undoLatt);
}

