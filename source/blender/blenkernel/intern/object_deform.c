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

/** \file blender/blenkernel/intern/object_deform.c
 *  \ingroup bke
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "BKE_action.h"
#include "BKE_object_deform.h"  /* own include */
#include "BKE_object.h"
#include "BKE_modifier.h"

#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

/* --- functions for getting vgroup aligned maps --- */

/**
 * gets the status of "flag" for each bDeformGroup
 * in ob->defbase and returns an array containing them
 */
bool *BKE_objdef_lock_flags_get(Object *ob, const int defbase_tot)
{
	bool is_locked = false;
	int i;
	//int defbase_tot = BLI_countlist(&ob->defbase);
	bool *lock_flags = MEM_mallocN(defbase_tot * sizeof(bool), "defflags");
	bDeformGroup *defgroup;

	for (i = 0, defgroup = ob->defbase.first; i < defbase_tot && defgroup; defgroup = defgroup->next, i++) {
		lock_flags[i] = ((defgroup->flag & DG_LOCK_WEIGHT) != 0);
		is_locked |= lock_flags[i];
	}
	if (is_locked) {
		return lock_flags;
	}

	MEM_freeN(lock_flags);
	return NULL;
}

bool *BKE_objdef_validmap_get(Object *ob, const int defbase_tot)
{
	bDeformGroup *dg;
	ModifierData *md;
	bool *vgroup_validmap;
	GHash *gh;
	int i, step1 = 1;
	//int defbase_tot = BLI_countlist(&ob->defbase);
	VirtualModifierData virtualModifierData;

	if (BLI_listbase_is_empty(&ob->defbase)) {
		return NULL;
	}

	gh = BLI_ghash_str_new_ex("BKE_objdef_validmap_get gh", defbase_tot);

	/* add all names to a hash table */
	for (dg = ob->defbase.first; dg; dg = dg->next) {
		BLI_ghash_insert(gh, dg->name, NULL);
	}

	BLI_assert(BLI_ghash_size(gh) == defbase_tot);

	/* now loop through the armature modifiers and identify deform bones */
	for (md = ob->modifiers.first; md; md = !md->next && step1 ? (step1 = 0), modifiers_getVirtualModifierList(ob, &virtualModifierData) : md->next) {
		if (!(md->mode & (eModifierMode_Realtime | eModifierMode_Virtual)))
			continue;

		if (md->type == eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData *) md;

			if (amd->object && amd->object->pose) {
				bPose *pose = amd->object->pose;
				bPoseChannel *chan;

				for (chan = pose->chanbase.first; chan; chan = chan->next) {
					void **val_p;
					if (chan->bone->flag & BONE_NO_DEFORM)
						continue;

					val_p = BLI_ghash_lookup_p(gh, chan->name);
					if (val_p) {
						*val_p = SET_INT_IN_POINTER(1);
					}
				}
			}
		}
	}

	vgroup_validmap = MEM_mallocN(sizeof(*vgroup_validmap) * defbase_tot, "wpaint valid map");

	/* add all names to a hash table */
	for (dg = ob->defbase.first, i = 0; dg; dg = dg->next, i++) {
		vgroup_validmap[i] = (BLI_ghash_lookup(gh, dg->name) != NULL);
	}

	BLI_assert(i == BLI_ghash_size(gh));

	BLI_ghash_free(gh, NULL, NULL);

	return vgroup_validmap;
}

/* Returns total selected vgroups,
 * wpi.defbase_sel is assumed malloc'd, all values are set */
bool *BKE_objdef_selected_get(Object *ob, int defbase_tot, int *r_dg_flags_sel_tot)
{
	bool *dg_selection = MEM_mallocN(defbase_tot * sizeof(bool), __func__);
	bDeformGroup *defgroup;
	unsigned int i;
	Object *armob = BKE_object_pose_armature_get(ob);
	(*r_dg_flags_sel_tot) = 0;

	if (armob) {
		bPose *pose = armob->pose;
		for (i = 0, defgroup = ob->defbase.first; i < defbase_tot && defgroup; defgroup = defgroup->next, i++) {
			bPoseChannel *pchan = BKE_pose_channel_find_name(pose, defgroup->name);
			if (pchan && (pchan->bone->flag & BONE_SELECTED)) {
				dg_selection[i] = true;
				(*r_dg_flags_sel_tot) += 1;
			}
			else {
				dg_selection[i] = false;
			}
		}
	}
	else {
		memset(dg_selection, false, sizeof(*dg_selection) * defbase_tot);
	}

	return dg_selection;
}
