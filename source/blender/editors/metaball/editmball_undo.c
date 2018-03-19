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

/** \file blender/editors/metaball/editmball_undo.c
 *  \ingroup edmeta
 */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "DNA_defs.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"

#include "ED_mball.h"
#include "ED_util.h"

typedef struct UndoMBall {
	ListBase editelems;
	int lastelem_index;
} UndoMBall;

/* free all MetaElems from ListBase */
static void freeMetaElemlist(ListBase *lb)
{
	MetaElem *ml;

	if (lb == NULL) {
		return;
	}

	while ((ml = BLI_pophead(lb))) {
		MEM_freeN(ml);
	}
}

static void undoMball_to_editMball(void *umb_v, void *mb_v, void *UNUSED(obdata))
{
	MetaBall *mb = mb_v;
	UndoMBall *umb = umb_v;

	freeMetaElemlist(mb->editelems);
	mb->lastelem = NULL;

	/* copy 'undo' MetaElems to 'edit' MetaElems */
	int index = 0;
	for (MetaElem *ml_undo = umb->editelems.first; ml_undo; ml_undo = ml_undo->next, index += 1) {
		MetaElem *ml_edit = MEM_dupallocN(ml_undo);
		BLI_addtail(mb->editelems, ml_edit);
		if (index == umb->lastelem_index) {
			mb->lastelem = ml_edit;
		}
	}
	
}

static void *editMball_to_undoMball(void *mb_v, void *UNUSED(obdata))
{
	MetaBall *mb = mb_v;
	UndoMBall *umb;

	/* allocate memory for undo ListBase */
	umb = MEM_callocN(sizeof(UndoMBall), __func__);
	umb->lastelem_index = -1;
	
	/* copy contents of current ListBase to the undo ListBase */
	int index = 0;
	for (MetaElem *ml_edit = mb->editelems->first; ml_edit; ml_edit = ml_edit->next, index += 1) {
		MetaElem *ml_undo = MEM_dupallocN(ml_edit);
		BLI_addtail(&umb->editelems, ml_undo);
		if (ml_edit == mb->lastelem) {
			umb->lastelem_index = index;
		}
	}
	
	return umb;
}

/* free undo ListBase of MetaElems */
static void free_undoMball(void *umb_v)
{
	UndoMBall *umb = umb_v;
	
	freeMetaElemlist(&umb->editelems);
	MEM_freeN(umb);
}

static MetaBall *metaball_get_obdata(Object *ob)
{
	if (ob && ob->type == OB_MBALL) {
		return ob->data;
	}
	return NULL;
}


static void *get_data(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	return metaball_get_obdata(obedit);
}

/* this is undo system for MetaBalls */
void undo_push_mball(bContext *C, const char *name)
{
	undo_editmode_push(C, name, get_data, free_undoMball, undoMball_to_editMball, editMball_to_undoMball, NULL);
}
