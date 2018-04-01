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
#include "BLI_array_utils.h"

#include "DNA_defs.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_object.h"
#include "ED_mball.h"
#include "ED_util.h"

#include "WM_types.h"
#include "WM_api.h"

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

typedef struct UndoMBall {
	ListBase editelems;
	int lastelem_index;
	size_t undo_size;
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

static void undomball_to_editmball(UndoMBall *umb, MetaBall *mb)
{
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

static void *editmball_from_undomball(UndoMBall *umb, MetaBall *mb)
{
	BLI_assert(BLI_array_is_zeroed(umb, 1));

	/* allocate memory for undo ListBase */
	umb->lastelem_index = -1;

	/* copy contents of current ListBase to the undo ListBase */
	int index = 0;
	for (MetaElem *ml_edit = mb->editelems->first; ml_edit; ml_edit = ml_edit->next, index += 1) {
		MetaElem *ml_undo = MEM_dupallocN(ml_edit);
		BLI_addtail(&umb->editelems, ml_undo);
		if (ml_edit == mb->lastelem) {
			umb->lastelem_index = index;
		}
		umb->undo_size += sizeof(MetaElem);
	}

	return umb;
}

/* free undo ListBase of MetaElems */
static void undomball_free_data(UndoMBall *umb)
{
	freeMetaElemlist(&umb->editelems);
}

static Object *editmball_object_from_context(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_MBALL) {
		MetaBall *mb = obedit->data;
		if (mb->editelems != NULL) {
			return obedit;
		}
	}
	return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct MBallUndoStep {
	UndoStep step;
	/* note: will split out into list for multi-object-editmode. */
	UndoRefID_Object obedit_ref;
	UndoMBall data;
} MBallUndoStep;

static bool mball_undosys_poll(bContext *C)
{
	return editmball_object_from_context(C) != NULL;
}

static bool mball_undosys_step_encode(struct bContext *C, UndoStep *us_p)
{
	MBallUndoStep *us = (MBallUndoStep *)us_p;
	us->obedit_ref.ptr = editmball_object_from_context(C);
	MetaBall *mb = us->obedit_ref.ptr->data;
	editmball_from_undomball(&us->data, mb);
	us->step.data_size = us->data.undo_size;
	return true;
}

static void mball_undosys_step_decode(struct bContext *C, UndoStep *us_p, int UNUSED(dir))
{
	ED_object_mode_set(C, OB_MODE_EDIT);

	MBallUndoStep *us = (MBallUndoStep *)us_p;
	Object *obedit = us->obedit_ref.ptr;
	MetaBall *mb = obedit->data;
	undomball_to_editmball(&us->data, mb);
	DEG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
}

static void mball_undosys_step_free(UndoStep *us_p)
{
	MBallUndoStep *us = (MBallUndoStep *)us_p;
	undomball_free_data(&us->data);
}

static void mball_undosys_foreach_ID_ref(
        UndoStep *us_p, UndoTypeForEachIDRefFn foreach_ID_ref_fn, void *user_data)
{
	MBallUndoStep *us = (MBallUndoStep *)us_p;
	foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->obedit_ref));
}

/* Export for ED_undo_sys. */
void ED_mball_undosys_type(UndoType *ut)
{
	ut->name = "Edit MBall";
	ut->poll = mball_undosys_poll;
	ut->step_encode = mball_undosys_step_encode;
	ut->step_decode = mball_undosys_step_decode;
	ut->step_free = mball_undosys_step_free;

	ut->step_foreach_ID_ref = mball_undosys_foreach_ID_ref;

	ut->mode = BKE_UNDOTYPE_MODE_STORE;
	ut->use_context = true;

	ut->step_size = sizeof(MBallUndoStep);

}

/** \} */
