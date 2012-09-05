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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_object_deform.h"  /* own include */
#include "BKE_modifier.h"

#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

/* --- functions for getting vgroup aligned maps --- */

/**
 * gets the status of "flag" for each bDeformGroup
 * in ob->defbase and returns an array containing them
 */
char *BKE_objdef_lock_flags_get(Object *ob, const int defbase_tot)
{
	char is_locked = FALSE;
	int i;
	//int defbase_tot = BLI_countlist(&ob->defbase);
	char *lock_flags = MEM_mallocN(defbase_tot * sizeof(char), "defflags");
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

char *BKE_objdef_validmap_get(Object *ob, const int defbase_tot)
{
	bDeformGroup *dg;
	ModifierData *md;
	char *vgroup_validmap;
	GHash *gh;
	int i, step1 = 1;
	//int defbase_tot = BLI_countlist(&ob->defbase);

	if (ob->defbase.first == NULL) {
		return NULL;
	}

	gh = BLI_ghash_str_new("BKE_objdef_validmap_get gh");

	/* add all names to a hash table */
	for (dg = ob->defbase.first; dg; dg = dg->next) {
		BLI_ghash_insert(gh, dg->name, NULL);
	}

	BLI_assert(BLI_ghash_size(gh) == defbase_tot);

	/* now loop through the armature modifiers and identify deform bones */
	for (md = ob->modifiers.first; md; md = !md->next && step1 ? (step1 = 0), modifiers_getVirtualModifierList(ob) : md->next) {
		if (!(md->mode & (eModifierMode_Realtime | eModifierMode_Virtual)))
			continue;

		if (md->type == eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData *) md;

			if (amd->object && amd->object->pose) {
				bPose *pose = amd->object->pose;
				bPoseChannel *chan;

				for (chan = pose->chanbase.first; chan; chan = chan->next) {
					if (chan->bone->flag & BONE_NO_DEFORM)
						continue;

					if (BLI_ghash_remove(gh, chan->name, NULL, NULL)) {
						BLI_ghash_insert(gh, chan->name, SET_INT_IN_POINTER(1));
					}
				}
			}
		}
	}

	vgroup_validmap = MEM_mallocN(defbase_tot, "wpaint valid map");

	/* add all names to a hash table */
	for (dg = ob->defbase.first, i = 0; dg; dg = dg->next, i++) {
		vgroup_validmap[i] = (BLI_ghash_lookup(gh, dg->name) != NULL);
	}

	BLI_assert(i == BLI_ghash_size(gh));

	BLI_ghash_free(gh, NULL, NULL);

	return vgroup_validmap;
}
