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

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"

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
			free_dverts(editlt->dvert, editlt->pntsu * editlt->pntsv * editlt->pntsw);

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

	actkey = ob_get_keyblock(obedit);
	if (actkey)
		key_to_latt(actkey, lt);

	lt->editlatt = MEM_callocN(sizeof(EditLatt), "editlatt");
	lt->editlatt->latt = MEM_dupallocN(lt);
	lt->editlatt->latt->def = MEM_dupallocN(lt->def);

	if (lt->dvert) {
		int tot = lt->pntsu * lt->pntsv * lt->pntsw;
		lt->editlatt->latt->dvert = MEM_mallocN(sizeof(MDeformVert) * tot, "Lattice MDeformVert");
		copy_dverts(lt->editlatt->latt->dvert, lt->dvert, tot);
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
	}

	if (lt->dvert) {
		free_dverts(lt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);
		lt->dvert = NULL;
	}

	if (editlt->dvert) {
		tot = lt->pntsu * lt->pntsv * lt->pntsw;

		lt->dvert = MEM_mallocN(sizeof(MDeformVert) * tot, "Lattice MDeformVert");
		copy_dverts(lt->dvert, editlt->dvert, tot);
	}
}

/************************** Operators *************************/

void ED_setflagsLatt(Object *obedit, int flag)
{
	Lattice *lt = obedit->data;
	BPoint *bp;
	int a;
	
	bp = lt->editlatt->latt->def;
	
	a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
	
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
				if (bp->f1) {
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

			while (a--) {
				if (bp->hide == 0) {
					bp->f1 ^= 1;
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

/****************************** Mouse Selection *************************/

static void findnearestLattvert__doClosest(void *userData, BPoint *bp, int x, int y)
{
	struct { BPoint *bp; short dist, select; int mval[2]; } *data = userData;
	float temp = abs(data->mval[0] - x) + abs(data->mval[1] - y);
	
	if ((bp->f1 & SELECT) == data->select)
		temp += 5;

	if (temp < data->dist) {
		data->dist = temp;

		data->bp = bp;
	}
}

static BPoint *findnearestLattvert(ViewContext *vc, const int mval[2], int sel)
{
	/* sel==1: selected gets a disadvantage */
	/* in nurb and bezt or bp the nearest is written */
	/* return 0 1 2: handlepunt */
	struct { BPoint *bp; short dist, select; int mval[2]; } data = {NULL};

	data.dist = 100;
	data.select = sel;
	data.mval[0] = mval[0];
	data.mval[1] = mval[1];

	ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
	lattice_foreachScreenVert(vc, findnearestLattvert__doClosest, &data);

	return data.bp;
}

int mouse_lattice(bContext *C, const int mval[2], int extend, int deselect, int toggle)
{
	ViewContext vc;
	BPoint *bp = NULL;

	view3d_set_viewcontext(C, &vc);
	bp = findnearestLattvert(&vc, mval, 1);

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

		WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);

		return 1;
	}

	return 0;
}

/******************************** Undo *************************/

typedef struct UndoLattice {
	BPoint *def;
	int pntsu, pntsv, pntsw;
} UndoLattice;

static void undoLatt_to_editLatt(void *data, void *edata, void *UNUSED(obdata))
{
	UndoLattice *ult = (UndoLattice *)data;
	EditLatt *editlatt = (EditLatt *)edata;
	int a = editlatt->latt->pntsu * editlatt->latt->pntsv * editlatt->latt->pntsw;

	memcpy(editlatt->latt->def, ult->def, a * sizeof(BPoint));
}

static void *editLatt_to_undoLatt(void *edata, void *UNUSED(obdata))
{
	UndoLattice *ult = MEM_callocN(sizeof(UndoLattice), "UndoLattice");
	EditLatt *editlatt = (EditLatt *)edata;
	
	ult->def = MEM_dupallocN(editlatt->latt->def);
	ult->pntsu = editlatt->latt->pntsu;
	ult->pntsv = editlatt->latt->pntsv;
	ult->pntsw = editlatt->latt->pntsw;
	
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

