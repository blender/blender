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

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"

#include "ED_lattice.h"
#include "ED_util.h"

#include "lattice_intern.h"

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
