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

/** \file blender/editors/lattice/editlattice_undo.c
 *  \ingroup edlattice
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_array_utils.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_object.h"
#include "ED_lattice.h"
#include "ED_util.h"

#include "WM_types.h"
#include "WM_api.h"

#include "lattice_intern.h"

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

typedef struct UndoLattice {
	BPoint *def;
	int pntsu, pntsv, pntsw, actbp;
	size_t undo_size;
} UndoLattice;

static void undolatt_to_editlatt(UndoLattice *ult, EditLatt *editlatt)
{
	int len = editlatt->latt->pntsu * editlatt->latt->pntsv * editlatt->latt->pntsw;

	memcpy(editlatt->latt->def, ult->def, sizeof(BPoint) * len);
	editlatt->latt->actbp = ult->actbp;
}

static void *undolatt_from_editlatt(UndoLattice *ult, EditLatt *editlatt)
{
	BLI_assert(BLI_array_is_zeroed(ult, 1));

	ult->def = MEM_dupallocN(editlatt->latt->def);
	ult->pntsu = editlatt->latt->pntsu;
	ult->pntsv = editlatt->latt->pntsv;
	ult->pntsw = editlatt->latt->pntsw;
	ult->actbp = editlatt->latt->actbp;

	ult->undo_size += sizeof(*ult->def) * ult->pntsu * ult->pntsv * ult->pntsw;

	return ult;
}

static void undolatt_free_data(UndoLattice *ult)
{
	if (ult->def) {
		MEM_freeN(ult->def);
	}
}

#if 0
static int validate_undoLatt(void *data, void *edata)
{
	UndoLattice *ult = (UndoLattice *)data;
	EditLatt *editlatt = (EditLatt *)edata;

	return (ult->pntsu == editlatt->latt->pntsu &&
	        ult->pntsv == editlatt->latt->pntsv &&
	        ult->pntsw == editlatt->latt->pntsw);
}
#endif

static Object *editlatt_object_from_context(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_LATTICE) {
		Lattice *lt = obedit->data;
		if (lt->editlatt != NULL) {
			return obedit;
		}
	}

	return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct LatticeUndoStep {
	UndoStep step;
	/* note: will split out into list for multi-object-editmode. */
	UndoRefID_Object obedit_ref;
	UndoLattice data;
} LatticeUndoStep;

static bool lattice_undosys_poll(bContext *C)
{
	return editlatt_object_from_context(C) != NULL;
}

static bool lattice_undosys_step_encode(struct bContext *C, UndoStep *us_p)
{
	LatticeUndoStep *us = (LatticeUndoStep *)us_p;
	us->obedit_ref.ptr = editlatt_object_from_context(C);
	Lattice *lt = us->obedit_ref.ptr->data;
	undolatt_from_editlatt(&us->data, lt->editlatt);
	us->step.data_size = us->data.undo_size;
	return true;
}

static void lattice_undosys_step_decode(struct bContext *C, UndoStep *us_p, int UNUSED(dir))
{
	/* TODO(campbell): undo_system: use low-level API to set mode. */
	ED_object_mode_set(C, OB_MODE_EDIT);
	BLI_assert(lattice_undosys_poll(C));

	LatticeUndoStep *us = (LatticeUndoStep *)us_p;
	Object *obedit = us->obedit_ref.ptr;
	Lattice *lt = obedit->data;
	EditLatt *editlatt = lt->editlatt;
	undolatt_to_editlatt(&us->data, editlatt);
	DEG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
}

static void lattice_undosys_step_free(UndoStep *us_p)
{
	LatticeUndoStep *us = (LatticeUndoStep *)us_p;
	undolatt_free_data(&us->data);
}

static void lattice_undosys_foreach_ID_ref(
        UndoStep *us_p, UndoTypeForEachIDRefFn foreach_ID_ref_fn, void *user_data)
{
	LatticeUndoStep *us = (LatticeUndoStep *)us_p;
	foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->obedit_ref));
}

/* Export for ED_undo_sys. */
void ED_lattice_undosys_type(UndoType *ut)
{
	ut->name = "Edit Lattice";
	ut->poll = lattice_undosys_poll;
	ut->step_encode = lattice_undosys_step_encode;
	ut->step_decode = lattice_undosys_step_decode;
	ut->step_free = lattice_undosys_step_free;

	ut->step_foreach_ID_ref = lattice_undosys_foreach_ID_ref;

	ut->mode = BKE_UNDOTYPE_MODE_STORE;
	ut->use_context = true;

	ut->step_size = sizeof(LatticeUndoStep);
}

/** \} */
