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
 * Contributor(s): Blender Foundation, 2002-2009 full recode.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/armature/editarmature_undo.c
 *  \ingroup edarmature
 */

#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_array_utils.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_undo_system.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_util.h"

#include "WM_types.h"
#include "WM_api.h"

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

typedef struct UndoArmature {
	EditBone *act_edbone;
	ListBase lb;
	size_t undo_size;
} UndoArmature;

static void undoarm_to_editarm(UndoArmature *uarm, bArmature *arm)
{
	EditBone *ebone;

	ED_armature_ebone_listbase_free(arm->edbo);
	ED_armature_ebone_listbase_copy(arm->edbo, &uarm->lb);

	/* active bone */
	if (uarm->act_edbone) {
		ebone = uarm->act_edbone;
		arm->act_edbone = ebone->temp.ebone;
	}
	else {
		arm->act_edbone = NULL;
	}

	ED_armature_ebone_listbase_temp_clear(arm->edbo);
}

static void *undoarm_from_editarm(UndoArmature *uarm, bArmature *arm)
{
	BLI_assert(BLI_array_is_zeroed(uarm, 1));

	/* TODO: include size of ID-properties. */
	uarm->undo_size = 0;

	ED_armature_ebone_listbase_copy(&uarm->lb, arm->edbo);

	/* active bone */
	if (arm->act_edbone) {
		EditBone *ebone = arm->act_edbone;
		uarm->act_edbone = ebone->temp.ebone;
	}

	ED_armature_ebone_listbase_temp_clear(&uarm->lb);

	for (EditBone *ebone = uarm->lb.first; ebone; ebone = ebone->next) {
		uarm->undo_size += sizeof(EditBone);
	}

	return uarm;
}

static void undoarm_free_data(UndoArmature *uarm)
{
	ED_armature_ebone_listbase_free(&uarm->lb);
}

static Object *editarm_object_from_context(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_ARMATURE) {
		bArmature *arm = obedit->data;
		if (arm->edbo != NULL) {
			return obedit;
		}
	}
	return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct ArmatureUndoStep {
	UndoStep step;
	/* note: will split out into list for multi-object-editmode. */
	UndoRefID_Object obedit_ref;
	UndoArmature data;
} ArmatureUndoStep;

static bool armature_undosys_poll(bContext *C)
{
	return editarm_object_from_context(C) != NULL;
}

static bool armature_undosys_step_encode(struct bContext *C, UndoStep *us_p)
{
	ArmatureUndoStep *us = (ArmatureUndoStep *)us_p;
	us->obedit_ref.ptr = editarm_object_from_context(C);
	bArmature *arm = us->obedit_ref.ptr->data;
	undoarm_from_editarm(&us->data, arm);
	us->step.data_size = us->data.undo_size;
	return true;
}

static void armature_undosys_step_decode(struct bContext *C, UndoStep *us_p, int UNUSED(dir))
{
	/* TODO(campbell): undo_system: use low-level API to set mode. */
	ED_object_mode_set(C, OB_MODE_EDIT);
	BLI_assert(armature_undosys_poll(C));

	ArmatureUndoStep *us = (ArmatureUndoStep *)us_p;
	Object *obedit = us->obedit_ref.ptr;
	bArmature *arm = obedit->data;
	undoarm_to_editarm(&us->data, arm);
	DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
}

static void armature_undosys_step_free(UndoStep *us_p)
{
	ArmatureUndoStep *us = (ArmatureUndoStep *)us_p;
	undoarm_free_data(&us->data);
}

static void armature_undosys_foreach_ID_ref(
        UndoStep *us_p, UndoTypeForEachIDRefFn foreach_ID_ref_fn, void *user_data)
{
	ArmatureUndoStep *us = (ArmatureUndoStep *)us_p;
	foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->obedit_ref));
}

/* Export for ED_undo_sys. */
void ED_armature_undosys_type(UndoType *ut)
{
	ut->name = "Edit Armature";
	ut->poll = armature_undosys_poll;
	ut->step_encode = armature_undosys_step_encode;
	ut->step_decode = armature_undosys_step_decode;
	ut->step_free = armature_undosys_step_free;

	ut->step_foreach_ID_ref = armature_undosys_foreach_ID_ref;

	ut->mode = BKE_UNDOTYPE_MODE_STORE;
	ut->use_context = true;

	ut->step_size = sizeof(ArmatureUndoStep);
}

/** \} */
