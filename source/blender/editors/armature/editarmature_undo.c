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

#include "BKE_context.h"

#include "ED_armature.h"
#include "ED_util.h"

typedef struct UndoArmature {
	EditBone *act_edbone;
	ListBase lb;
} UndoArmature;

static void undoBones_to_editBones(void *uarmv, void *armv, void *UNUSED(data))
{
	UndoArmature *uarm = uarmv;
	bArmature *arm = armv;
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

static void *editBones_to_undoBones(void *armv, void *UNUSED(obdata))
{
	bArmature *arm = armv;
	UndoArmature *uarm;
	EditBone *ebone;

	uarm = MEM_callocN(sizeof(UndoArmature), "listbase undo");

	ED_armature_ebone_listbase_copy(&uarm->lb, arm->edbo);

	/* active bone */
	if (arm->act_edbone) {
		ebone = arm->act_edbone;
		uarm->act_edbone = ebone->temp.ebone;
	}

	ED_armature_ebone_listbase_temp_clear(&uarm->lb);

	return uarm;
}

static void free_undoBones(void *uarmv)
{
	UndoArmature *uarm = uarmv;

	ED_armature_ebone_listbase_free(&uarm->lb);

	MEM_freeN(uarm);
}

static void *get_armature_edit(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_ARMATURE) {
		return obedit->data;
	}
	return NULL;
}

/* and this is all the undo system needs to know */
void undo_push_armature(bContext *C, const char *name)
{
	// XXX solve getdata()
	undo_editmode_push(C, name, get_armature_edit, free_undoBones, undoBones_to_editBones, editBones_to_undoBones, NULL);
}
