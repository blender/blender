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

#include "BLT_translation.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string_utils.h"

#include "DNA_armature_types.h"
#include "DNA_cloth_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_object_deform.h"  /* own include */
#include "BKE_object.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_gpencil.h"

/** \name Misc helpers
 * \{ */

static Lattice *object_defgroup_lattice_get(ID *id)
{
	Lattice *lt = (Lattice *)id;
	BLI_assert(GS(id->name) == ID_LT);
	return (lt->editlatt) ? lt->editlatt->latt : lt;
}

/**
 * Update users of vgroups from this object, according to given map.
 *
 * Use it when you remove or reorder vgroups in the object.
 *
 * \param map an array mapping old indices to new indices.
 */
void BKE_object_defgroup_remap_update_users(Object *ob, int *map)
{
	ModifierData *md;
	ParticleSystem *psys;
	int a;

	/* these cases don't use names to refer to vertex groups, so when
	 * they get removed the numbers get out of sync, this corrects that */

	if (ob->soft) {
		ob->soft->vertgroup = map[ob->soft->vertgroup];
	}

	for (md = ob->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Explode) {
			ExplodeModifierData *emd = (ExplodeModifierData *)md;
			emd->vgroup = map[emd->vgroup];
		}
		else if (md->type == eModifierType_Cloth) {
			ClothModifierData *clmd = (ClothModifierData *)md;
			ClothSimSettings *clsim = clmd->sim_parms;

			if (clsim) {
				clsim->vgroup_mass = map[clsim->vgroup_mass];
				clsim->vgroup_bend = map[clsim->vgroup_bend];
				clsim->vgroup_struct = map[clsim->vgroup_struct];
			}
		}
	}

	for (psys = ob->particlesystem.first; psys; psys = psys->next) {
		for (a = 0; a < PSYS_TOT_VG; a++) {
			psys->vgroup[a] = map[psys->vgroup[a]];
		}
	}
}
/** \} */


/** \name Group creation
 * \{ */

/**
 * Add a vgroup of given name to object. *Does not* handle MDeformVert data at all!
 */
bDeformGroup *BKE_object_defgroup_add_name(Object *ob, const char *name)
{
	bDeformGroup *defgroup;

	if (!ob || !OB_TYPE_SUPPORT_VGROUP(ob->type))
		return NULL;

	defgroup = BKE_defgroup_new(ob, name);

	ob->actdef = BLI_listbase_count(&ob->defbase);

	return defgroup;
}

/**
 * Add a vgroup of default name to object. *Does not* handle MDeformVert data at all!
 */
bDeformGroup *BKE_object_defgroup_add(Object *ob)
{
	return BKE_object_defgroup_add_name(ob, DATA_("Group"));
}

/**
 * Create MDeformVert data for given ID. Work in Object mode only.
 */
MDeformVert *BKE_object_defgroup_data_create(ID *id)
{
	if (GS(id->name) == ID_ME) {
		Mesh *me = (Mesh *)id;
		me->dvert = CustomData_add_layer(&me->vdata, CD_MDEFORMVERT, CD_CALLOC, NULL, me->totvert);
		return me->dvert;
	}
	else if (GS(id->name) == ID_LT) {
		Lattice *lt = (Lattice *)id;
		lt->dvert = MEM_callocN(sizeof(MDeformVert) * lt->pntsu * lt->pntsv * lt->pntsw, "lattice deformVert");
		return lt->dvert;
	}

	return NULL;
}
/** \} */


/** \name Group clearing
 * \{ */

/**
 * Remove all verts (or only selected ones) from given vgroup. Work in Object and Edit modes.
 *
 * \param use_selection: Only operate on selection.
 * \return True if any vertex was removed, false otherwise.
 */
bool BKE_object_defgroup_clear(Object *ob, bDeformGroup *dg, const bool use_selection)
{
	MDeformVert *dv;
	const int def_nr = BLI_findindex(&ob->defbase, dg);
	bool changed = false;

	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (me->edit_btmesh) {
			BMEditMesh *em = me->edit_btmesh;
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

			if (cd_dvert_offset != -1) {
				BMVert *eve;
				BMIter iter;

				BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
					dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);

					if (dv && dv->dw && (!use_selection || BM_elem_flag_test(eve, BM_ELEM_SELECT))) {
						MDeformWeight *dw = defvert_find_index(dv, def_nr);
						defvert_remove_group(dv, dw); /* dw can be NULL */
						changed = true;
					}
				}
			}
		}
		else {
			if (me->dvert) {
				MVert *mv;
				int i;

				mv = me->mvert;
				dv = me->dvert;

				for (i = 0; i < me->totvert; i++, mv++, dv++) {
					if (dv->dw && (!use_selection || (mv->flag & SELECT))) {
						MDeformWeight *dw = defvert_find_index(dv, def_nr);
						defvert_remove_group(dv, dw);  /* dw can be NULL */
						changed = true;
					}
				}
			}
		}
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = object_defgroup_lattice_get((ID *)(ob->data));

		if (lt->dvert) {
			BPoint *bp;
			int i, tot = lt->pntsu * lt->pntsv * lt->pntsw;

			for (i = 0, bp = lt->def; i < tot; i++, bp++) {
				if (!use_selection || (bp->f1 & SELECT)) {
					MDeformWeight *dw;

					dv = &lt->dvert[i];

					dw = defvert_find_index(dv, def_nr);
					defvert_remove_group(dv, dw);  /* dw can be NULL */
					changed = true;
				}
			}
		}
	}

	return changed;
}

/**
 * Remove all verts (or only selected ones) from all vgroups. Work in Object and Edit modes.
 *
 * \param use_selection: Only operate on selection.
 * \return True if any vertex was removed, false otherwise.
 */
bool BKE_object_defgroup_clear_all(Object *ob, const bool use_selection)
{
	bDeformGroup *dg;
	bool changed = false;

	for (dg = ob->defbase.first; dg; dg = dg->next) {
		if (BKE_object_defgroup_clear(ob, dg, use_selection)) {
			changed = true;
		}
	}

	return changed;
}
/** \} */


/** \name Group removal
 * \{ */

static void object_defgroup_remove_update_users(Object *ob, const int idx)
{
	int i, defbase_tot = BLI_listbase_count(&ob->defbase) + 1;
	int *map = MEM_mallocN(sizeof(int) * defbase_tot, "vgroup del");

	map[idx] = map[0] = 0;
	for (i = 1; i < idx; i++) {
		map[i] = i;
	}
	for (i = idx + 1; i < defbase_tot; i++) {
		map[i] = i - 1;
	}

	BKE_object_defgroup_remap_update_users(ob, map);
	MEM_freeN(map);
}

static void object_defgroup_remove_common(Object *ob, bDeformGroup *dg, const int def_nr)
{
	object_defgroup_remove_update_users(ob, def_nr + 1);

	/* Remove the group */
	BLI_freelinkN(&ob->defbase, dg);

	/* Update the active deform index if necessary */
	if (ob->actdef > def_nr)
		ob->actdef--;

	/* remove all dverts */
	if (BLI_listbase_is_empty(&ob->defbase)) {
		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			CustomData_free_layer_active(&me->vdata, CD_MDEFORMVERT, me->totvert);
			me->dvert = NULL;
		}
		else if (ob->type == OB_LATTICE) {
			Lattice *lt = object_defgroup_lattice_get((ID *)(ob->data));
			if (lt->dvert) {
				MEM_freeN(lt->dvert);
				lt->dvert = NULL;
			}
		}
	}
	else if (ob->actdef < 1) {  /* Keep a valid active index if we still have some vgroups. */
		ob->actdef = 1;
	}
}

static void object_defgroup_remove_object_mode(Object *ob, bDeformGroup *dg)
{
	MDeformVert *dvert_array = NULL;
	int dvert_tot = 0;
	const int def_nr = BLI_findindex(&ob->defbase, dg);

	BLI_assert(def_nr != -1);

	BKE_object_defgroup_array_get(ob->data, &dvert_array, &dvert_tot);

	if (dvert_array) {
		int i, j;
		MDeformVert *dv;
		for (i = 0, dv = dvert_array; i < dvert_tot; i++, dv++) {
			MDeformWeight *dw;

			dw = defvert_find_index(dv, def_nr);
			defvert_remove_group(dv, dw); /* dw can be NULL */

			/* inline, make into a function if anything else needs to do this */
			for (j = 0; j < dv->totweight; j++) {
				if (dv->dw[j].def_nr > def_nr) {
					dv->dw[j].def_nr--;
				}
			}
			/* done */
		}
	}

	object_defgroup_remove_common(ob, dg, def_nr);
}

static void object_defgroup_remove_edit_mode(Object *ob, bDeformGroup *dg)
{
	int i;
	const int def_nr = BLI_findindex(&ob->defbase, dg);

	BLI_assert(def_nr != -1);

	/* Make sure that no verts are using this group - if none were removed, we can skip next per-vert update. */
	if (!BKE_object_defgroup_clear(ob, dg, false)) {
		/* Nothing to do. */
	}
	/* Else, make sure that any groups with higher indices are adjusted accordingly */
	else if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		BMEditMesh *em = me->edit_btmesh;
		const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

		BMIter iter;
		BMVert *eve;
		MDeformVert *dvert;

		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			dvert = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);

			if (dvert) {
				for (i = 0; i < dvert->totweight; i++) {
					if (dvert->dw[i].def_nr > def_nr) {
						dvert->dw[i].def_nr--;
					}
				}
			}
		}
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = ((Lattice *)(ob->data))->editlatt->latt;
		BPoint *bp;
		MDeformVert *dvert = lt->dvert;
		int a, tot;

		if (dvert) {
			tot = lt->pntsu * lt->pntsv * lt->pntsw;
			for (a = 0, bp = lt->def; a < tot; a++, bp++, dvert++) {
				for (i = 0; i < dvert->totweight; i++) {
					if (dvert->dw[i].def_nr > def_nr) {
						dvert->dw[i].def_nr--;
					}
				}
			}
		}
	}

	object_defgroup_remove_common(ob, dg, def_nr);
}

/**
 * Remove given vgroup from object. Work in Object and Edit modes.
 */
void BKE_object_defgroup_remove(Object *ob, bDeformGroup *defgroup)
{
	if ((ob) && (ob->type == OB_GPENCIL)) {
		BKE_gpencil_vgroup_remove(ob, defgroup);
	}
	else {
		if (BKE_object_is_in_editmode_vgroup(ob))
			object_defgroup_remove_edit_mode(ob, defgroup);
		else
			object_defgroup_remove_object_mode(ob, defgroup);

		BKE_mesh_batch_cache_dirty(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
	}
}

/**
 * Remove all vgroups from object. Work in Object and Edit modes.
 * When only_unlocked=true, locked vertex groups are not removed.
 */
void BKE_object_defgroup_remove_all_ex(struct Object *ob, bool only_unlocked)
{
	bDeformGroup *dg = (bDeformGroup *)ob->defbase.first;
	const bool edit_mode = BKE_object_is_in_editmode_vgroup(ob);

	if (dg) {
		while (dg) {
			bDeformGroup *next_dg = dg->next;

			if (!only_unlocked || (dg->flag & DG_LOCK_WEIGHT) == 0) {
				if (edit_mode)
					object_defgroup_remove_edit_mode(ob, dg);
				else
					object_defgroup_remove_object_mode(ob, dg);
			}

			dg = next_dg;
		}
	}
	else {  /* ob->defbase is empty... */
		/* remove all dverts */
		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			CustomData_free_layer_active(&me->vdata, CD_MDEFORMVERT, me->totvert);
			me->dvert = NULL;
		}
		else if (ob->type == OB_LATTICE) {
			Lattice *lt = object_defgroup_lattice_get((ID *)(ob->data));
			if (lt->dvert) {
				MEM_freeN(lt->dvert);
				lt->dvert = NULL;
			}
		}
		/* Fix counters/indices */
		ob->actdef = 0;
	}
}

/**
 * Remove all vgroups from object. Work in Object and Edit modes.
 */
void BKE_object_defgroup_remove_all(struct Object *ob)
{
	BKE_object_defgroup_remove_all_ex(ob, false);
}

/**
 * Compute mapping for vertex groups with matching name, -1 is used for no remapping.
 * Returns null if no remapping is required.
 * The returned array has to be freed.
 */
int *BKE_object_defgroup_index_map_create(Object *ob_src, Object *ob_dst, int *r_map_len)
{
	/* Build src to merged mapping of vgroup indices. */
	if (BLI_listbase_is_empty(&ob_src->defbase) || BLI_listbase_is_empty(&ob_dst->defbase)) {
		*r_map_len = 0;
		return NULL;
	}

	bDeformGroup *dg_src;
	*r_map_len = BLI_listbase_count(&ob_src->defbase);
	int *vgroup_index_map = MEM_malloc_arrayN(*r_map_len, sizeof(*vgroup_index_map), "defgroup index map create");
	bool is_vgroup_remap_needed = false;
	int i;

	for (dg_src = ob_src->defbase.first, i = 0; dg_src; dg_src = dg_src->next, i++) {
		vgroup_index_map[i] = defgroup_name_index(ob_dst, dg_src->name);
		is_vgroup_remap_needed = is_vgroup_remap_needed || (vgroup_index_map[i] != i);
	}

	if (!is_vgroup_remap_needed) {
		MEM_freeN(vgroup_index_map);
		vgroup_index_map = NULL;
		*r_map_len = 0;
	}

	return vgroup_index_map;
}

void BKE_object_defgroup_index_map_apply(MDeformVert *dvert, int dvert_len, const int *map, int map_len)
{
	if (map == NULL || map_len == 0) {
		return;
	}

	MDeformVert *dv = dvert;
	for (int i = 0; i < dvert_len; i++, dv++) {
		int totweight = dv->totweight;
		for (int j = 0; j < totweight; j++) {
			int def_nr = dv->dw[j].def_nr;
			if ((uint)def_nr < (uint)map_len && map[def_nr] != -1) {
				dv->dw[j].def_nr = map[def_nr];
			}
			else {
				totweight--;
				dv->dw[j] = dv->dw[totweight];
				j--;
			}
		}
		if (totweight != dv->totweight) {
			if (totweight) {
				dv->dw = MEM_reallocN(dv->dw, sizeof(*dv->dw) * totweight);
			}
			else {
				MEM_SAFE_FREE(dv->dw);
			}
			dv->totweight = totweight;
		}
	}
}

/**
 * Get MDeformVert vgroup data from given object. Should only be used in Object mode.
 *
 * \return True if the id type supports weights.
 */
bool BKE_object_defgroup_array_get(ID *id, MDeformVert **dvert_arr, int *dvert_tot)
{
	if (id) {
		switch (GS(id->name)) {
			case ID_ME:
			{
				Mesh *me = (Mesh *)id;
				*dvert_arr = me->dvert;
				*dvert_tot = me->totvert;
				return true;
			}
			case ID_LT:
			{
				Lattice *lt = object_defgroup_lattice_get(id);
				*dvert_arr = lt->dvert;
				*dvert_tot = lt->pntsu * lt->pntsv * lt->pntsw;
				return true;
			}
			default:
				break;
		}
	}

	*dvert_arr = NULL;
	*dvert_tot = 0;
	return false;
}
/** \} */

/* --- functions for getting vgroup aligned maps --- */

/**
 * gets the status of "flag" for each bDeformGroup
 * in ob->defbase and returns an array containing them
 */
bool *BKE_object_defgroup_lock_flags_get(Object *ob, const int defbase_tot)
{
	bool is_locked = false;
	int i;
	//int defbase_tot = BLI_listbase_count(&ob->defbase);
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

bool *BKE_object_defgroup_validmap_get(Object *ob, const int defbase_tot)
{
	bDeformGroup *dg;
	ModifierData *md;
	bool *defgroup_validmap;
	GHash *gh;
	int i, step1 = 1;
	//int defbase_tot = BLI_listbase_count(&ob->defbase);
	VirtualModifierData virtualModifierData;

	if (BLI_listbase_is_empty(&ob->defbase)) {
		return NULL;
	}

	gh = BLI_ghash_str_new_ex(__func__, defbase_tot);

	/* add all names to a hash table */
	for (dg = ob->defbase.first; dg; dg = dg->next) {
		BLI_ghash_insert(gh, dg->name, NULL);
	}

	BLI_assert(BLI_ghash_len(gh) == defbase_tot);

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

	defgroup_validmap = MEM_mallocN(sizeof(*defgroup_validmap) * defbase_tot, "wpaint valid map");

	/* add all names to a hash table */
	for (dg = ob->defbase.first, i = 0; dg; dg = dg->next, i++) {
		defgroup_validmap[i] = (BLI_ghash_lookup(gh, dg->name) != NULL);
	}

	BLI_assert(i == BLI_ghash_len(gh));

	BLI_ghash_free(gh, NULL, NULL);

	return defgroup_validmap;
}

/* Returns total selected vgroups,
 * wpi.defbase_sel is assumed malloc'd, all values are set */
bool *BKE_object_defgroup_selected_get(Object *ob, int defbase_tot, int *r_dg_flags_sel_tot)
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

/* Marks mirror vgroups in output and counts them. Output and counter assumed to be already initialized.
 * Designed to be usable after BKE_object_defgroup_selected_get to extend selection to mirror.
 */
void BKE_object_defgroup_mirror_selection(
        struct Object *ob, int defbase_tot, const bool *dg_selection,
        bool *dg_flags_sel, int *r_dg_flags_sel_tot)
{
	bDeformGroup *defgroup;
	unsigned int i;
	int i_mirr;

	for (i = 0, defgroup = ob->defbase.first; i < defbase_tot && defgroup; defgroup = defgroup->next, i++) {
		if (dg_selection[i]) {
			char name_flip[MAXBONENAME];

			BLI_string_flip_side_name(name_flip, defgroup->name, false, sizeof(name_flip));
			i_mirr = STREQ(name_flip, defgroup->name) ? i : defgroup_name_index(ob, name_flip);

			if ((i_mirr >= 0 && i_mirr < defbase_tot) && (dg_flags_sel[i_mirr] == false)) {
				dg_flags_sel[i_mirr] = true;
				(*r_dg_flags_sel_tot) += 1;
			}
		}
	}
}

/**
 * Return the subset type of the Vertex Group Selection
 */
bool *BKE_object_defgroup_subset_from_select_type(
        Object *ob, eVGroupSelect subset_type, int *r_defgroup_tot, int *r_subset_count)
{
	bool *defgroup_validmap = NULL;
	*r_defgroup_tot = BLI_listbase_count(&ob->defbase);

	switch (subset_type) {
		case WT_VGROUP_ACTIVE:
		{
			const int def_nr_active = ob->actdef - 1;
			defgroup_validmap = MEM_mallocN(*r_defgroup_tot * sizeof(*defgroup_validmap), __func__);
			memset(defgroup_validmap, false, *r_defgroup_tot * sizeof(*defgroup_validmap));
			if ((def_nr_active >= 0) && (def_nr_active < *r_defgroup_tot)) {
				*r_subset_count = 1;
				defgroup_validmap[def_nr_active] = true;
			}
			else {
				*r_subset_count = 0;
			}
			break;
		}
		case WT_VGROUP_BONE_SELECT:
		{
			defgroup_validmap = BKE_object_defgroup_selected_get(ob, *r_defgroup_tot, r_subset_count);
			break;
		}
		case WT_VGROUP_BONE_DEFORM:
		{
			int i;
			defgroup_validmap = BKE_object_defgroup_validmap_get(ob, *r_defgroup_tot);
			*r_subset_count = 0;
			for (i = 0; i < *r_defgroup_tot; i++) {
				if (defgroup_validmap[i] == true) {
					*r_subset_count += 1;
				}
			}
			break;
		}
		case WT_VGROUP_BONE_DEFORM_OFF:
		{
			int i;
			defgroup_validmap = BKE_object_defgroup_validmap_get(ob, *r_defgroup_tot);
			*r_subset_count = 0;
			for (i = 0; i < *r_defgroup_tot; i++) {
				defgroup_validmap[i] = !defgroup_validmap[i];
				if (defgroup_validmap[i] == true) {
					*r_subset_count += 1;
				}
			}
			break;
		}
		case WT_VGROUP_ALL:
		default:
		{
			defgroup_validmap = MEM_mallocN(*r_defgroup_tot * sizeof(*defgroup_validmap), __func__);
			memset(defgroup_validmap, true, *r_defgroup_tot * sizeof(*defgroup_validmap));
			*r_subset_count = *r_defgroup_tot;
			break;
		}
	}

	return defgroup_validmap;
}

/**
 * store indices from the defgroup_validmap (faster lookups in some cases)
 */
void BKE_object_defgroup_subset_to_index_array(
        const bool *defgroup_validmap, const int defgroup_tot, int *r_defgroup_subset_map)
{
	int i, j = 0;
	for (i = 0; i < defgroup_tot; i++) {
		if (defgroup_validmap[i]) {
			r_defgroup_subset_map[j++] = i;
		}
	}
}
