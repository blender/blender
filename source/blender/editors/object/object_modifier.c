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
* Contributor(s): Blender Foundation, 2009
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/editors/object/object_modifier.c
*  \ingroup edobj
*/


#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_force_types.h"
#include "DNA_scene_types.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_curve.h"
#include "BKE_context.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_report.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_ocean.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_softbody.h"
#include "BKE_editmesh.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_mesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

static void modifier_skin_customdata_delete(struct Object *ob);

/******************************** API ****************************/

ModifierData *ED_object_modifier_add(ReportList *reports, Main *bmain, Scene *scene, Object *ob, const char *name, int type)
{
	ModifierData *md = NULL, *new_md = NULL;
	const ModifierTypeInfo *mti = modifierType_getInfo(type);

	/* Check compatibility of modifier [T25291, T50373]. */
	if (!BKE_object_support_modifier_type_check(ob, type)) {
		BKE_reportf(reports, RPT_WARNING, "Modifiers cannot be added to object '%s'", ob->id.name + 2);
		return NULL;
	}

	if (mti->flags & eModifierTypeFlag_Single) {
		if (modifiers_findByType(ob, type)) {
			BKE_report(reports, RPT_WARNING, "Only one modifier of this type is allowed");
			return NULL;
		}
	}

	if (type == eModifierType_ParticleSystem) {
		/* don't need to worry about the new modifier's name, since that is set to the number
		* of particle systems which shouldn't have too many duplicates
		*/
		new_md = object_add_particle_system(bmain, scene, ob, name);
	}
	else {
		/* get new modifier data to add */
		new_md = modifier_new(type);

		if (mti->flags & eModifierTypeFlag_RequiresOriginalData) {
			md = ob->modifiers.first;

			while (md && modifierType_getInfo(md->type)->type == eModifierTypeType_OnlyDeform)
				md = md->next;

			BLI_insertlinkbefore(&ob->modifiers, md, new_md);
		}
		else
			BLI_addtail(&ob->modifiers, new_md);

		if (name) {
			BLI_strncpy_utf8(new_md->name, name, sizeof(new_md->name));
		}

		/* make sure modifier data has unique name */

		modifier_unique_name(&ob->modifiers, new_md);

		/* special cases */
		if (type == eModifierType_Softbody) {
			if (!ob->soft) {
				ob->soft = sbNew(scene);
				ob->softflag |= OB_SB_GOAL | OB_SB_EDGES;
			}
		}
		else if (type == eModifierType_Collision) {
			if (!ob->pd)
				ob->pd = object_add_collision_fields(0);

			ob->pd->deflect = 1;
		}
		else if (type == eModifierType_Surface) {
			/* pass */
		}
		else if (type == eModifierType_Multires) {
			/* set totlvl from existing MDISPS layer if object already had it */
			multiresModifier_set_levels_from_disps((MultiresModifierData *)new_md, ob);

			if (ob->mode & OB_MODE_SCULPT) {
				/* ensure that grid paint mask layer is created */
				BKE_sculpt_mask_layers_ensure(ob, (MultiresModifierData *)new_md);
			}
		}
		else if (type == eModifierType_Skin) {
			/* ensure skin-node customdata exists */
			BKE_mesh_ensure_skin_customdata(ob->data);
		}
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	DEG_relations_tag_update(bmain);

	return new_md;
}

/* Return true if the object has a modifier of type 'type' other than
* the modifier pointed to be 'exclude', otherwise returns false. */
static bool object_has_modifier(const Object *ob, const ModifierData *exclude,
	ModifierType type)
{
	ModifierData *md;

	for (md = ob->modifiers.first; md; md = md->next) {
		if ((md != exclude) && (md->type == type))
			return true;
	}

	return false;
}

/* If the object data of 'orig_ob' has other users, run 'callback' on
* each of them.
*
* If include_orig is true, the callback will run on 'orig_ob' too.
*
* If the callback ever returns true, iteration will stop and the
* function value will be true. Otherwise the function returns false.
*/
bool ED_object_iter_other(
        Main *bmain, Object *orig_ob, const bool include_orig,
        bool (*callback)(Object *ob, void *callback_data),
        void *callback_data)
{
	ID *ob_data_id = orig_ob->data;
	int users = ob_data_id->us;

	if (ob_data_id->flag & LIB_FAKEUSER)
		users--;

	/* First check that the object's data has multiple users */
	if (users > 1) {
		Object *ob;
		int totfound = include_orig ? 0 : 1;

		for (ob = bmain->object.first; ob && totfound < users;
		     ob = ob->id.next)
		{
			if (((ob != orig_ob) || include_orig) &&
			    (ob->data == orig_ob->data))
			{
				if (callback(ob, callback_data))
					return true;

				totfound++;
			}
		}
	}
	else if (include_orig) {
		return callback(orig_ob, callback_data);
	}

	return false;
}

static bool object_has_modifier_cb(Object *ob, void *data)
{
	ModifierType type = *((ModifierType *)data);

	return object_has_modifier(ob, NULL, type);
}

/* Use with ED_object_iter_other(). Sets the total number of levels
* for any multires modifiers on the object to the int pointed to by
* callback_data. */
bool ED_object_multires_update_totlevels_cb(Object *ob, void *totlevel_v)
{
	ModifierData *md;
	int totlevel = *((char *)totlevel_v);

	for (md = ob->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Multires) {
			multires_set_tot_level(ob, (MultiresModifierData *)md, totlevel);
			DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
	}
	return false;
}

/* Return true if no modifier of type 'type' other than 'exclude' */
static bool object_modifier_safe_to_delete(Main *bmain, Object *ob,
	ModifierData *exclude,
	ModifierType type)
{
	return (!object_has_modifier(ob, exclude, type) &&
		!ED_object_iter_other(bmain, ob, false,
			object_has_modifier_cb, &type));
}

static bool object_modifier_remove(Main *bmain, Object *ob, ModifierData *md,
	bool *r_sort_depsgraph)
{
	/* It seems on rapid delete it is possible to
	* get called twice on same modifier, so make
	* sure it is in list. */
	if (BLI_findindex(&ob->modifiers, md) == -1) {
		return 0;
	}

	/* special cases */
	if (md->type == eModifierType_ParticleSystem) {
		ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;

		BLI_remlink(&ob->particlesystem, psmd->psys);
		psys_free(ob, psmd->psys);
		psmd->psys = NULL;
	}
	else if (md->type == eModifierType_Softbody) {
		if (ob->soft) {
			sbFree(ob);
			ob->softflag = 0;  /* TODO(Sybren): this should probably be moved into sbFree() */
		}
	}
	else if (md->type == eModifierType_Collision) {
		if (ob->pd)
			ob->pd->deflect = 0;

		*r_sort_depsgraph = true;
	}
	else if (md->type == eModifierType_Surface) {
		*r_sort_depsgraph = true;
	}
	else if (md->type == eModifierType_Multires) {
		/* Delete MDisps layer if not used by another multires modifier */
		if (object_modifier_safe_to_delete(bmain, ob, md, eModifierType_Multires))
			multires_customdata_delete(ob->data);
	}
	else if (md->type == eModifierType_Skin) {
		/* Delete MVertSkin layer if not used by another skin modifier */
		if (object_modifier_safe_to_delete(bmain, ob, md, eModifierType_Skin))
			modifier_skin_customdata_delete(ob);
	}

	if (ELEM(md->type, eModifierType_Softbody, eModifierType_Cloth) &&
	    BLI_listbase_is_empty(&ob->particlesystem))
	{
		ob->mode &= ~OB_MODE_PARTICLE_EDIT;
	}

	BLI_remlink(&ob->modifiers, md);
	modifier_free(md);
	BKE_object_free_derived_caches(ob);

	return 1;
}

bool ED_object_modifier_remove(ReportList *reports, Main *bmain, Object *ob, ModifierData *md)
{
	bool sort_depsgraph = false;
	bool ok;

	ok = object_modifier_remove(bmain, ob, md, &sort_depsgraph);

	if (!ok) {
		BKE_reportf(reports, RPT_ERROR, "Modifier '%s' not in object '%s'", md->name, ob->id.name);
		return 0;
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	DEG_relations_tag_update(bmain);

	return 1;
}

void ED_object_modifier_clear(Main *bmain, Object *ob)
{
	ModifierData *md = ob->modifiers.first;
	bool sort_depsgraph = false;

	if (!md)
		return;

	while (md) {
		ModifierData *next_md;

		next_md = md->next;

		object_modifier_remove(bmain, ob, md, &sort_depsgraph);

		md = next_md;
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	DEG_relations_tag_update(bmain);
}

int ED_object_modifier_move_up(ReportList *reports, Object *ob, ModifierData *md)
{
	if (md->prev) {
		const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->type != eModifierTypeType_OnlyDeform) {
			const ModifierTypeInfo *nmti = modifierType_getInfo(md->prev->type);

			if (nmti->flags & eModifierTypeFlag_RequiresOriginalData) {
				BKE_report(reports, RPT_WARNING, "Cannot move above a modifier requiring original data");
				return 0;
			}
		}

		BLI_remlink(&ob->modifiers, md);
		BLI_insertlinkbefore(&ob->modifiers, md->prev, md);
	}

	return 1;
}

int ED_object_modifier_move_down(ReportList *reports, Object *ob, ModifierData *md)
{
	if (md->next) {
		const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->flags & eModifierTypeFlag_RequiresOriginalData) {
			const ModifierTypeInfo *nmti = modifierType_getInfo(md->next->type);

			if (nmti->type != eModifierTypeType_OnlyDeform) {
				BKE_report(reports, RPT_WARNING, "Cannot move beyond a non-deforming modifier");
				return 0;
			}
		}

		BLI_remlink(&ob->modifiers, md);
		BLI_insertlinkafter(&ob->modifiers, md->next, md);
	}

	return 1;
}

int ED_object_modifier_convert(ReportList *UNUSED(reports), Main *bmain, Scene *scene, ViewLayer *view_layer, Object *ob, ModifierData *md)
{
	Object *obn;
	ParticleSystem *psys;
	ParticleCacheKey *key, **cache;
	ParticleSettings *part;
	Mesh *me;
	MVert *mvert;
	MEdge *medge;
	int a, k, kmax;
	int totvert = 0, totedge = 0, cvert = 0;
	int totpart = 0, totchild = 0;

	if (md->type != eModifierType_ParticleSystem) return 0;
	if (ob && ob->mode & OB_MODE_PARTICLE_EDIT) return 0;

	psys = ((ParticleSystemModifierData *)md)->psys;
	part = psys->part;

	if (part->ren_as != PART_DRAW_PATH || psys->pathcache == NULL)
		return 0;

	totpart = psys->totcached;
	totchild = psys->totchildcache;

	if (totchild && (part->draw & PART_DRAW_PARENT) == 0)
		totpart = 0;

	/* count */
	cache = psys->pathcache;
	for (a = 0; a < totpart; a++) {
		key = cache[a];

		if (key->segments > 0) {
			totvert += key->segments + 1;
			totedge += key->segments;
		}
	}

	cache = psys->childcache;
	for (a = 0; a < totchild; a++) {
		key = cache[a];

		if (key->segments > 0) {
			totvert += key->segments + 1;
			totedge += key->segments;
		}
	}

	if (totvert == 0) return 0;

	/* add new mesh */
	obn = BKE_object_add(bmain, scene, view_layer, OB_MESH, NULL);
	me = obn->data;

	me->totvert = totvert;
	me->totedge = totedge;

	me->mvert = CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, totvert);
	me->medge = CustomData_add_layer(&me->edata, CD_MEDGE, CD_CALLOC, NULL, totedge);
	me->mface = CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, 0);

	mvert = me->mvert;
	medge = me->medge;

	/* copy coordinates */
	cache = psys->pathcache;
	for (a = 0; a < totpart; a++) {
		key = cache[a];
		kmax = key->segments;
		for (k = 0; k <= kmax; k++, key++, cvert++, mvert++) {
			copy_v3_v3(mvert->co, key->co);
			if (k) {
				medge->v1 = cvert - 1;
				medge->v2 = cvert;
				medge->flag = ME_EDGEDRAW | ME_EDGERENDER | ME_LOOSEEDGE;
				medge++;
			}
			else {
				/* cheap trick to select the roots */
				mvert->flag |= SELECT;
			}
		}
	}

	cache = psys->childcache;
	for (a = 0; a < totchild; a++) {
		key = cache[a];
		kmax = key->segments;
		for (k = 0; k <= kmax; k++, key++, cvert++, mvert++) {
			copy_v3_v3(mvert->co, key->co);
			if (k) {
				medge->v1 = cvert - 1;
				medge->v2 = cvert;
				medge->flag = ME_EDGEDRAW | ME_EDGERENDER | ME_LOOSEEDGE;
				medge++;
			}
			else {
				/* cheap trick to select the roots */
				mvert->flag |= SELECT;
			}
		}
	}

	DEG_relations_tag_update(bmain);

	return 1;
}

static int modifier_apply_shape(
	Main *bmain, ReportList *reports, Depsgraph *depsgraph, Scene *scene, Object *ob, ModifierData *md)
{
	const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	if (mti->isDisabled && mti->isDisabled(scene, md, 0)) {
		BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
		return 0;
	}

	/*
	* It should be ridiculously easy to extract the original verts that we want
	* and form the shape data.  We can probably use the CD KEYINDEX layer (or
	* whatever I ended up calling it, too tired to check now), though this would
	* by necessity have to make some potentially ugly assumptions about the order
	* of the mesh data :-/  you can probably assume in 99% of cases that the first
	* element of a given index is the original, and any subsequent duplicates are
	* copies/interpolates, but that's an assumption that would need to be tested
	* and then predominantly stated in comments in a half dozen headers.
	*/

	if (ob->type == OB_MESH) {
		Mesh *mesh_applied;
		Mesh *me = ob->data;
		Key *key = me->key;
		KeyBlock *kb;

		if (!modifier_isSameTopology(md) || mti->type == eModifierTypeType_NonGeometrical) {
			BKE_report(reports, RPT_ERROR, "Only deforming modifiers can be applied to shapes");
			return 0;
		}

		mesh_applied = BKE_mesh_create_derived_for_modifier(depsgraph, scene, ob, md, 0);
		if (!mesh_applied) {
			BKE_report(reports, RPT_ERROR, "Modifier is disabled or returned error, skipping apply");
			return 0;
		}

		if (key == NULL) {
			key = me->key = BKE_key_add(bmain, (ID *)me);
			key->type = KEY_RELATIVE;
			/* if that was the first key block added, then it was the basis.
			* Initialize it with the mesh, and add another for the modifier */
			kb = BKE_keyblock_add(key, NULL);
			BKE_keyblock_convert_from_mesh(me, key, kb);
		}

		kb = BKE_keyblock_add(key, md->name);
		BKE_mesh_nomain_to_meshkey(mesh_applied, me, kb);

		BKE_id_free(NULL, mesh_applied);
	}
	else {
		BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
		return 0;
	}
	return 1;
}

static int modifier_apply_obdata(ReportList *reports, Depsgraph *depsgraph, Scene *scene, Object *ob, ModifierData *md)
{
	const ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	if (mti->isDisabled && mti->isDisabled(scene, md, 0)) {
		BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
		return 0;
	}

	if (ob->type == OB_MESH) {
		Mesh *mesh_applied;
		Mesh *me = ob->data;
		MultiresModifierData *mmd = find_multires_modifier_before(scene, md);

		if (me->key && mti->type != eModifierTypeType_NonGeometrical) {
			BKE_report(reports, RPT_ERROR, "Modifier cannot be applied to a mesh with shape keys");
			return 0;
		}

		/* Multires: ensure that recent sculpting is applied */
		if (md->type == eModifierType_Multires)
			multires_force_update(ob);

		if (mmd && mmd->totlvl && mti->type == eModifierTypeType_OnlyDeform) {
			if (!multiresModifier_reshapeFromDeformMod(depsgraph, scene, mmd, ob, md)) {
				BKE_report(reports, RPT_ERROR, "Multires modifier returned error, skipping apply");
				return 0;
			}
		}
		else {
			mesh_applied = BKE_mesh_create_derived_for_modifier(depsgraph, scene, ob, md, 1);
			if (!mesh_applied) {
				BKE_report(reports, RPT_ERROR, "Modifier returned error, skipping apply");
				return 0;
			}

			BKE_mesh_nomain_to_mesh(mesh_applied, me, ob, CD_MASK_MESH, true);

			if (md->type == eModifierType_Multires)
				multires_customdata_delete(me);
		}
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		Curve *cu;
		int numVerts;
		float (*vertexCos)[3];
		ModifierEvalContext mectx = {depsgraph, ob, 0};

		if (ELEM(mti->type, eModifierTypeType_Constructive, eModifierTypeType_Nonconstructive)) {
			BKE_report(reports, RPT_ERROR, "Transform curve to mesh in order to apply constructive modifiers");
			return 0;
		}

		cu = ob->data;
		BKE_report(reports, RPT_INFO, "Applied modifier only changed CV points, not tessellated/bevel vertices");

		vertexCos = BKE_curve_nurbs_vertexCos_get(&cu->nurb, &numVerts);
		modifier_deformVerts(md, &mectx, NULL, vertexCos, numVerts);
		BK_curve_nurbs_vertexCos_apply(&cu->nurb, vertexCos);

		MEM_freeN(vertexCos);

		DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}
	else {
		BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
		return 0;
	}

	/* lattice modifier can be applied to particle system too */
	if (ob->particlesystem.first) {

		ParticleSystem *psys = ob->particlesystem.first;

		for (; psys; psys = psys->next) {

			if (psys->part->type != PART_HAIR)
				continue;

			psys_apply_hair_lattice(depsgraph, scene, ob, psys);
		}
	}

	return 1;
}

int ED_object_modifier_apply(
	Main *bmain, ReportList *reports, Depsgraph *depsgraph,
	Scene *scene, Object *ob, ModifierData *md, int mode)
{
	int prev_mode;

	if (BKE_object_is_in_editmode(ob)) {
		BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied in edit mode");
		return 0;
	}
	else if (((ID *) ob->data)->us > 1) {
		BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied to multi-user data");
		return 0;
	}
	else if ((ob->mode & OB_MODE_SCULPT) &&
	         (find_multires_modifier_before(scene, md)) &&
	         (modifier_isSameTopology(md) == false))
	{
		BKE_report(reports, RPT_ERROR, "Constructive modifier cannot be applied to multi-res data in sculpt mode");
		return 0;
	}

	if (md != ob->modifiers.first)
		BKE_report(reports, RPT_INFO, "Applied modifier was not first, result may not be as expected");

	/* Get evaluated modifier, so object links pointer to evaluated data,
	* but still use original object it is applied to the original mesh. */
	Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
	ModifierData *md_eval = (ob_eval) ? modifiers_findByName(ob_eval, md->name) : md;

	/* allow apply of a not-realtime modifier, by first re-enabling realtime. */
	prev_mode = md_eval->mode;
	md_eval->mode |= eModifierMode_Realtime;

	if (mode == MODIFIER_APPLY_SHAPE) {
		if (!modifier_apply_shape(bmain, reports, depsgraph, scene, ob, md_eval)) {
			md_eval->mode = prev_mode;
			return 0;
		}
	}
	else {
		if (!modifier_apply_obdata(reports, depsgraph, scene, ob, md_eval)) {
			md_eval->mode = prev_mode;
			return 0;
		}
	}

	md_eval->mode = prev_mode;
	BLI_remlink(&ob->modifiers, md);
	modifier_free(md);

	BKE_object_free_derived_caches(ob);

	return 1;
}

int ED_object_modifier_copy(ReportList *UNUSED(reports), Object *ob, ModifierData *md)
{
	ModifierData *nmd;

	nmd = modifier_new(md->type);
	modifier_copyData(md, nmd);
	BLI_insertlinkafter(&ob->modifiers, md, nmd);
	modifier_unique_name(&ob->modifiers, nmd);

	return 1;
}

/************************ add modifier operator *********************/

static int modifier_add_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	int type = RNA_enum_get(op->ptr, "type");

	if (!ED_object_modifier_add(op->reports, bmain, scene, ob, NULL, type))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static const EnumPropertyItem *modifier_add_itemf(
	bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	Object *ob = ED_object_active_context(C);
	EnumPropertyItem *item = NULL;
	const EnumPropertyItem *md_item, *group_item = NULL;
	const ModifierTypeInfo *mti;
	int totitem = 0, a;

	if (!ob)
		return rna_enum_object_modifier_type_items;

	for (a = 0; rna_enum_object_modifier_type_items[a].identifier; a++) {
		md_item = &rna_enum_object_modifier_type_items[a];

		if (md_item->identifier[0]) {
			mti = modifierType_getInfo(md_item->value);

			if (mti->flags & eModifierTypeFlag_NoUserAdd)
				continue;

			if (!BKE_object_support_modifier_type_check(ob, md_item->value))
				continue;
		}
		else {
			group_item = md_item;
			md_item = NULL;

			continue;
		}

		if (group_item) {
			RNA_enum_item_add(&item, &totitem, group_item);
			group_item = NULL;
		}

		RNA_enum_item_add(&item, &totitem, md_item);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

void OBJECT_OT_modifier_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Modifier";
	ot->description = "Add a procedural operation/effect to the active object";
	ot->idname = "OBJECT_OT_modifier_add";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = modifier_add_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "type", rna_enum_object_modifier_type_items, eModifierType_Subsurf, "Type", "");
	RNA_def_enum_funcs(prop, modifier_add_itemf);
	ot->prop = prop;
}

/************************ generic functions for operators using mod names and data context *********************/

bool edit_modifier_poll_generic(bContext *C, StructRNA *rna_type, int obtype_flag)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", rna_type);
	Object *ob = (ptr.id.data) ? ptr.id.data : ED_object_active_context(C);

	if (!ob || ID_IS_LINKED(ob)) return 0;
	if (obtype_flag && ((1 << ob->type) & obtype_flag) == 0) return 0;
	if (ptr.id.data && ID_IS_LINKED(ptr.id.data)) return 0;

	if (ID_IS_STATIC_OVERRIDE(ob)) {
		CTX_wm_operator_poll_msg_set(C, "Cannot edit modifiers comming from static override");
		return (((ModifierData *)ptr.data)->flag & eModifierFlag_StaticOverride_Local) != 0;
	}

	return 1;
}

bool edit_modifier_poll(bContext *C)
{
	return edit_modifier_poll_generic(C, &RNA_Modifier, 0);
}

void edit_modifier_properties(wmOperatorType *ot)
{
	RNA_def_string(ot->srna, "modifier", NULL, MAX_NAME, "Modifier", "Name of the modifier to edit");
}

int edit_modifier_invoke_properties(bContext *C, wmOperator *op)
{
	ModifierData *md;

	if (RNA_struct_property_is_set(op->ptr, "modifier")) {
		return true;
	}
	else {
		PointerRNA ptr = CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
		if (ptr.data) {
			md = ptr.data;
			RNA_string_set(op->ptr, "modifier", md->name);
			return true;
		}
	}

	return false;
}

ModifierData *edit_modifier_property_get(wmOperator *op, Object *ob, int type)
{
	char modifier_name[MAX_NAME];
	ModifierData *md;
	RNA_string_get(op->ptr, "modifier", modifier_name);

	md = modifiers_findByName(ob, modifier_name);

	if (md && type != 0 && md->type != type)
		md = NULL;

	return md;
}

/************************ remove modifier operator *********************/

static int modifier_remove_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Object *ob = ED_object_active_context(C);
	ModifierData *md = edit_modifier_property_get(op, ob, 0);
	int mode_orig = ob->mode;

	if (!md || !ED_object_modifier_remove(op->reports, bmain, ob, md))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	/* if cloth/softbody was removed, particle mode could be cleared */
	if (mode_orig & OB_MODE_PARTICLE_EDIT) {
		if ((ob->mode & OB_MODE_PARTICLE_EDIT) == 0) {
			if (ob == OBACT(view_layer)) {
				WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, NULL);
			}
		}
	}
	return OPERATOR_FINISHED;
}

static int modifier_remove_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return modifier_remove_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_modifier_remove(wmOperatorType *ot)
{
	ot->name = "Remove Modifier";
	ot->description = "Remove a modifier from the active object";
	ot->idname = "OBJECT_OT_modifier_remove";

	ot->invoke = modifier_remove_invoke;
	ot->exec = modifier_remove_exec;
	ot->poll = edit_modifier_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/************************ move up modifier operator *********************/

static int modifier_move_up_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	ModifierData *md = edit_modifier_property_get(op, ob, 0);

	if (!md || !ED_object_modifier_move_up(op->reports, ob, md))
		return OPERATOR_CANCELLED;

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int modifier_move_up_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return modifier_move_up_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_modifier_move_up(wmOperatorType *ot)
{
	ot->name = "Move Up Modifier";
	ot->description = "Move modifier up in the stack";
	ot->idname = "OBJECT_OT_modifier_move_up";

	ot->invoke = modifier_move_up_invoke;
	ot->exec = modifier_move_up_exec;
	ot->poll = edit_modifier_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/************************ move down modifier operator *********************/

static int modifier_move_down_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	ModifierData *md = edit_modifier_property_get(op, ob, 0);

	if (!md || !ED_object_modifier_move_down(op->reports, ob, md))
		return OPERATOR_CANCELLED;

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int modifier_move_down_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return modifier_move_down_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_modifier_move_down(wmOperatorType *ot)
{
	ot->name = "Move Down Modifier";
	ot->description = "Move modifier down in the stack";
	ot->idname = "OBJECT_OT_modifier_move_down";

	ot->invoke = modifier_move_down_invoke;
	ot->exec = modifier_move_down_exec;
	ot->poll = edit_modifier_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/************************ apply modifier operator *********************/

static int modifier_apply_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	ModifierData *md = edit_modifier_property_get(op, ob, 0);
	int apply_as = RNA_enum_get(op->ptr, "apply_as");

	if (!md || !ED_object_modifier_apply(bmain, op->reports, depsgraph, scene, ob, md, apply_as)) {
		return OPERATOR_CANCELLED;
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int modifier_apply_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return modifier_apply_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

static const EnumPropertyItem modifier_apply_as_items[] = {
	{MODIFIER_APPLY_DATA, "DATA", 0, "Object Data", "Apply modifier to the object's data"},
	{MODIFIER_APPLY_SHAPE, "SHAPE", 0, "New Shape", "Apply deform-only modifier to a new shape on this object"},
	{0, NULL, 0, NULL, NULL}
};

void OBJECT_OT_modifier_apply(wmOperatorType *ot)
{
	ot->name = "Apply Modifier";
	ot->description = "Apply modifier and remove from the stack";
	ot->idname = "OBJECT_OT_modifier_apply";

	ot->invoke = modifier_apply_invoke;
	ot->exec = modifier_apply_exec;
	ot->poll = edit_modifier_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

	RNA_def_enum(ot->srna, "apply_as", modifier_apply_as_items, MODIFIER_APPLY_DATA, "Apply as", "How to apply the modifier to the geometry");
	edit_modifier_properties(ot);
}

/************************ convert modifier operator *********************/

static int modifier_convert_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Object *ob = ED_object_active_context(C);
	ModifierData *md = edit_modifier_property_get(op, ob, 0);

	if (!md || !ED_object_modifier_convert(op->reports, bmain, scene, view_layer, ob, md))
		return OPERATOR_CANCELLED;

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int modifier_convert_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return modifier_convert_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_modifier_convert(wmOperatorType *ot)
{
	ot->name = "Convert Modifier";
	ot->description = "Convert particles to a mesh object";
	ot->idname = "OBJECT_OT_modifier_convert";

	ot->invoke = modifier_convert_invoke;
	ot->exec = modifier_convert_exec;
	ot->poll = edit_modifier_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/************************ copy modifier operator *********************/

static int modifier_copy_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	ModifierData *md = edit_modifier_property_get(op, ob, 0);

	if (!md || !ED_object_modifier_copy(op->reports, ob, md))
		return OPERATOR_CANCELLED;

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int modifier_copy_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return modifier_copy_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_modifier_copy(wmOperatorType *ot)
{
	ot->name = "Copy Modifier";
	ot->description = "Duplicate modifier at the same position in the stack";
	ot->idname = "OBJECT_OT_modifier_copy";

	ot->invoke = modifier_copy_invoke;
	ot->exec = modifier_copy_exec;
	ot->poll = edit_modifier_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/************* multires delete higher levels operator ****************/

static bool multires_poll(bContext *C)
{
	return edit_modifier_poll_generic(C, &RNA_MultiresModifier, (1 << OB_MESH));
}

static int multires_higher_levels_delete_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(op, ob, eModifierType_Multires);

	if (!mmd)
		return OPERATOR_CANCELLED;

	multiresModifier_del_levels(mmd, scene, ob, 1);

	ED_object_iter_other(CTX_data_main(C), ob, true,
		ED_object_multires_update_totlevels_cb,
		&mmd->totlvl);

	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int multires_higher_levels_delete_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return multires_higher_levels_delete_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_multires_higher_levels_delete(wmOperatorType *ot)
{
	ot->name = "Delete Higher Levels";
	ot->description = "Deletes the higher resolution mesh, potential loss of detail";
	ot->idname = "OBJECT_OT_multires_higher_levels_delete";

	ot->poll = multires_poll;
	ot->invoke = multires_higher_levels_delete_invoke;
	ot->exec = multires_higher_levels_delete_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/****************** multires subdivide operator *********************/

static int multires_subdivide_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(op, ob, eModifierType_Multires);

	if (!mmd)
		return OPERATOR_CANCELLED;

	multiresModifier_subdivide(mmd, scene, ob, 0, mmd->simple);

	ED_object_iter_other(CTX_data_main(C), ob, true,
		ED_object_multires_update_totlevels_cb,
		&mmd->totlvl);

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	if (ob->mode & OB_MODE_SCULPT) {
		/* ensure that grid paint mask layer is created */
		BKE_sculpt_mask_layers_ensure(ob, mmd);
	}

	return OPERATOR_FINISHED;
}

static int multires_subdivide_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return multires_subdivide_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_multires_subdivide(wmOperatorType *ot)
{
	ot->name = "Multires Subdivide";
	ot->description = "Add a new level of subdivision";
	ot->idname = "OBJECT_OT_multires_subdivide";

	ot->poll = multires_poll;
	ot->invoke = multires_subdivide_invoke;
	ot->exec = multires_subdivide_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/****************** multires reshape operator *********************/

static int multires_reshape_exec(bContext *C, wmOperator *op)
{
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Object *ob = ED_object_active_context(C), *secondob = NULL;
	Scene *scene = CTX_data_scene(C);
	MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(op, ob, eModifierType_Multires);

	if (!mmd)
		return OPERATOR_CANCELLED;

	if (mmd->lvl == 0) {
		BKE_report(op->reports, RPT_ERROR, "Reshape can work only with higher levels of subdivisions");
		return OPERATOR_CANCELLED;
	}

	CTX_DATA_BEGIN(C, Object *, selob, selected_editable_objects)
	{
		if (selob->type == OB_MESH && selob != ob) {
			secondob = selob;
			break;
		}
	}
	CTX_DATA_END;

	if (!secondob) {
		BKE_report(op->reports, RPT_ERROR, "Second selected mesh object required to copy shape from");
		return OPERATOR_CANCELLED;
	}

	if (!multiresModifier_reshape(depsgraph, scene, mmd, ob, secondob)) {
		BKE_report(op->reports, RPT_ERROR, "Objects do not have the same number of vertices");
		return OPERATOR_CANCELLED;
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int multires_reshape_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return multires_reshape_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_multires_reshape(wmOperatorType *ot)
{
	ot->name = "Multires Reshape";
	ot->description = "Copy vertex coordinates from other object";
	ot->idname = "OBJECT_OT_multires_reshape";

	ot->poll = multires_poll;
	ot->invoke = multires_reshape_invoke;
	ot->exec = multires_reshape_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}



/****************** multires save external operator *********************/

static int multires_external_save_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *ob = ED_object_active_context(C);
	Mesh *me = (ob) ? ob->data : op->customdata;
	char path[FILE_MAX];
	const bool relative = RNA_boolean_get(op->ptr, "relative_path");

	if (!me)
		return OPERATOR_CANCELLED;

	if (CustomData_external_test(&me->ldata, CD_MDISPS))
		return OPERATOR_CANCELLED;

	RNA_string_get(op->ptr, "filepath", path);

	if (relative)
		BLI_path_rel(path, BKE_main_blendfile_path(bmain));

	CustomData_external_add(&me->ldata, &me->id, CD_MDISPS, me->totloop, path);
	CustomData_external_write(&me->ldata, &me->id, CD_MASK_MESH, me->totloop, 0);

	return OPERATOR_FINISHED;
}

static int multires_external_save_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Object *ob = ED_object_active_context(C);
	MultiresModifierData *mmd;
	Mesh *me = ob->data;
	char path[FILE_MAX];

	if (!edit_modifier_invoke_properties(C, op))
		return OPERATOR_CANCELLED;

	mmd = (MultiresModifierData *)edit_modifier_property_get(op, ob, eModifierType_Multires);

	if (!mmd)
		return OPERATOR_CANCELLED;

	if (CustomData_external_test(&me->ldata, CD_MDISPS))
		return OPERATOR_CANCELLED;

	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return multires_external_save_exec(C, op);

	op->customdata = me;

	BLI_snprintf(path, sizeof(path), "//%s.btx", me->id.name + 2);
	RNA_string_set(op->ptr, "filepath", path);

	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

void OBJECT_OT_multires_external_save(wmOperatorType *ot)
{
	ot->name = "Multires Save External";
	ot->description = "Save displacements to an external file";
	ot->idname = "OBJECT_OT_multires_external_save";

	/* XXX modifier no longer in context after file browser .. ot->poll = multires_poll; */
	ot->exec = multires_external_save_exec;
	ot->invoke = multires_external_save_invoke;
	ot->poll = multires_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

	WM_operator_properties_filesel(
		ot, FILE_TYPE_FOLDER | FILE_TYPE_BTX, FILE_SPECIAL, FILE_SAVE,
		WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);
	edit_modifier_properties(ot);
}

/****************** multires pack operator *********************/

static int multires_external_pack_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_active_context(C);
	Mesh *me = ob->data;

	if (!CustomData_external_test(&me->ldata, CD_MDISPS))
		return OPERATOR_CANCELLED;

	/* XXX don't remove.. */
	CustomData_external_remove(&me->ldata, &me->id, CD_MDISPS, me->totloop);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_multires_external_pack(wmOperatorType *ot)
{
	ot->name = "Multires Pack External";
	ot->description = "Pack displacements from an external file";
	ot->idname = "OBJECT_OT_multires_external_pack";

	ot->poll = multires_poll;
	ot->exec = multires_external_pack_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* multires apply base ***********************/
static int multires_base_apply_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	MultiresModifierData *mmd = (MultiresModifierData *)edit_modifier_property_get(op, ob, eModifierType_Multires);

	if (!mmd)
		return OPERATOR_CANCELLED;

	multiresModifier_base_apply(mmd, scene, ob);

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int multires_base_apply_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return multires_base_apply_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}


void OBJECT_OT_multires_base_apply(wmOperatorType *ot)
{
	ot->name = "Multires Apply Base";
	ot->description = "Modify the base mesh to conform to the displaced mesh";
	ot->idname = "OBJECT_OT_multires_base_apply";

	ot->poll = multires_poll;
	ot->invoke = multires_base_apply_invoke;
	ot->exec = multires_base_apply_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}


/************************** skin modifier ***********************/

static void modifier_skin_customdata_delete(Object *ob)
{
	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;

	if (em)
		BM_data_layer_free(em->bm, &em->bm->vdata, CD_MVERT_SKIN);
	else
		CustomData_free_layer_active(&me->vdata, CD_MVERT_SKIN, me->totvert);
}

static bool skin_poll(bContext *C)
{
	return (!CTX_data_edit_object(C) &&
		edit_modifier_poll_generic(C, &RNA_SkinModifier, (1 << OB_MESH)));
}

static bool skin_edit_poll(bContext *C)
{
	return (CTX_data_edit_object(C) &&
		edit_modifier_poll_generic(C, &RNA_SkinModifier, (1 << OB_MESH)));
}

static void skin_root_clear(BMVert *bm_vert, GSet *visited, const int cd_vert_skin_offset)
{
	BMEdge *bm_edge;
	BMIter bm_iter;

	BM_ITER_ELEM (bm_edge, &bm_iter, bm_vert, BM_EDGES_OF_VERT) {
		BMVert *v2 = BM_edge_other_vert(bm_edge, bm_vert);

		if (BLI_gset_add(visited, v2)) {
			MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(v2, cd_vert_skin_offset);

			/* clear vertex root flag and add to visited set */
			vs->flag &= ~MVERT_SKIN_ROOT;

			skin_root_clear(v2, visited, cd_vert_skin_offset);
		}
	}
}

static int skin_root_mark_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMesh *bm = em->bm;
	BMVert *bm_vert;
	BMIter bm_iter;
	GSet *visited;

	visited = BLI_gset_ptr_new(__func__);

	BKE_mesh_ensure_skin_customdata(ob->data);

	const int cd_vert_skin_offset = CustomData_get_offset(&bm->vdata, CD_MVERT_SKIN);

	BM_ITER_MESH (bm_vert, &bm_iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(bm_vert, BM_ELEM_SELECT) &&
		    BLI_gset_add(visited, bm_vert))
		{
			MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(bm_vert, cd_vert_skin_offset);

			/* mark vertex as root and add to visited set */
			vs->flag |= MVERT_SKIN_ROOT;

			/* clear root flag from all connected vertices (recursively) */
			skin_root_clear(bm_vert, visited, cd_vert_skin_offset);
		}
	}

	BLI_gset_free(visited, NULL);

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_skin_root_mark(wmOperatorType *ot)
{
	ot->name = "Skin Root Mark";
	ot->description = "Mark selected vertices as roots";
	ot->idname = "OBJECT_OT_skin_root_mark";

	ot->poll = skin_edit_poll;
	ot->exec = skin_root_mark_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

typedef enum {
	SKIN_LOOSE_MARK,
	SKIN_LOOSE_CLEAR
} SkinLooseAction;

static int skin_loose_mark_clear_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMesh *bm = em->bm;
	BMVert *bm_vert;
	BMIter bm_iter;
	SkinLooseAction action = RNA_enum_get(op->ptr, "action");

	if (!CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
		return OPERATOR_CANCELLED;
	}

	BM_ITER_MESH (bm_vert, &bm_iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(bm_vert, BM_ELEM_SELECT)) {
			MVertSkin *vs = CustomData_bmesh_get(&bm->vdata,
				bm_vert->head.data,
				CD_MVERT_SKIN);


			switch (action) {
				case SKIN_LOOSE_MARK:
					vs->flag |= MVERT_SKIN_LOOSE;
					break;
				case SKIN_LOOSE_CLEAR:
					vs->flag &= ~MVERT_SKIN_LOOSE;
					break;
			}
		}
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_skin_loose_mark_clear(wmOperatorType *ot)
{
	static const EnumPropertyItem action_items[] = {
		{SKIN_LOOSE_MARK, "MARK", 0, "Mark", "Mark selected vertices as loose"},
		{SKIN_LOOSE_CLEAR, "CLEAR", 0, "Clear", "Set selected vertices as not loose"},
		{0, NULL, 0, NULL, NULL}
	};

	ot->name = "Skin Mark/Clear Loose";
	ot->description = "Mark/clear selected vertices as loose";
	ot->idname = "OBJECT_OT_skin_loose_mark_clear";

	ot->poll = skin_edit_poll;
	ot->exec = skin_loose_mark_clear_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "action", action_items, SKIN_LOOSE_MARK, "Action", NULL);
}

static int skin_radii_equalize_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMesh *bm = em->bm;
	BMVert *bm_vert;
	BMIter bm_iter;

	if (!CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
		return OPERATOR_CANCELLED;
	}

	BM_ITER_MESH (bm_vert, &bm_iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(bm_vert, BM_ELEM_SELECT)) {
			MVertSkin *vs = CustomData_bmesh_get(&bm->vdata,
				bm_vert->head.data,
				CD_MVERT_SKIN);
			float avg = (vs->radius[0] + vs->radius[1]) * 0.5f;

			vs->radius[0] = vs->radius[1] = avg;
		}
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_skin_radii_equalize(wmOperatorType *ot)
{
	ot->name = "Skin Radii Equalize";
	ot->description = "Make skin radii of selected vertices equal on each axis";
	ot->idname = "OBJECT_OT_skin_radii_equalize";

	ot->poll = skin_edit_poll;
	ot->exec = skin_radii_equalize_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void skin_armature_bone_create(Object *skin_ob,
	MVert *mvert, MEdge *medge,
	bArmature *arm,
	BLI_bitmap *edges_visited,
	const MeshElemMap *emap,
	EditBone *parent_bone,
	int parent_v)
{
	int i;

	for (i = 0; i < emap[parent_v].count; i++) {
		int endx = emap[parent_v].indices[i];
		const MEdge *e = &medge[endx];
		EditBone *bone;
		bDeformGroup *dg;
		int v;

		/* ignore edge if already visited */
		if (BLI_BITMAP_TEST(edges_visited, endx))
			continue;
		BLI_BITMAP_ENABLE(edges_visited, endx);

		v = (e->v1 == parent_v ? e->v2 : e->v1);

		bone = ED_armature_ebone_add(arm, "Bone");

		bone->parent = parent_bone;
		bone->flag |= BONE_CONNECTED;

		copy_v3_v3(bone->head, mvert[parent_v].co);
		copy_v3_v3(bone->tail, mvert[v].co);
		bone->rad_head = bone->rad_tail = 0.25;
		BLI_snprintf(bone->name, sizeof(bone->name), "Bone.%.2d", endx);

		/* add bDeformGroup */
		if ((dg = BKE_object_defgroup_add_name(skin_ob, bone->name))) {
			ED_vgroup_vert_add(skin_ob, dg, parent_v, 1, WEIGHT_REPLACE);
			ED_vgroup_vert_add(skin_ob, dg, v, 1, WEIGHT_REPLACE);
		}

		skin_armature_bone_create(skin_ob,
			mvert, medge,
			arm,
			edges_visited,
			emap,
			bone,
			v);
	}
}

static Object *modifier_skin_armature_create(Depsgraph *depsgraph, Main *bmain, Scene *scene, Object *skin_ob)
{
	BLI_bitmap *edges_visited;
	DerivedMesh *deform_dm;
	MVert *mvert;
	Mesh *me = skin_ob->data;
	Object *arm_ob;
	bArmature *arm;
	MVertSkin *mvert_skin;
	MeshElemMap *emap;
	int *emap_mem;
	int v;

	deform_dm = mesh_get_derived_deform(depsgraph, scene, skin_ob, CD_MASK_BAREMESH);
	mvert = deform_dm->getVertArray(deform_dm);

	/* add vertex weights to original mesh */
	CustomData_add_layer(&me->vdata,
		CD_MDEFORMVERT,
		CD_CALLOC,
		NULL,
		me->totvert);

	ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
	arm_ob = BKE_object_add(bmain, scene, view_layer, OB_ARMATURE, NULL);
	BKE_object_transform_copy(arm_ob, skin_ob);
	arm = arm_ob->data;
	arm->layer = 1;
	arm_ob->dtx |= OB_DRAWXRAY;
	arm->drawtype = ARM_LINE;
	arm->edbo = MEM_callocN(sizeof(ListBase), "edbo armature");

	mvert_skin = CustomData_get_layer(&me->vdata, CD_MVERT_SKIN);
	BKE_mesh_vert_edge_map_create(&emap, &emap_mem,
		me->medge, me->totvert, me->totedge);

	edges_visited = BLI_BITMAP_NEW(me->totedge, "edge_visited");

	/* note: we use EditBones here, easier to set them up and use
	* edit-armature functions to convert back to regular bones */
	for (v = 0; v < me->totvert; v++) {
		if (mvert_skin[v].flag & MVERT_SKIN_ROOT) {
			EditBone *bone = NULL;

			/* Unless the skin root has just one adjacent edge, create
			* a fake root bone (have it going off in the Y direction
			* (arbitrary) */
			if (emap[v].count > 1) {
				bone = ED_armature_ebone_add(arm, "Bone");

				copy_v3_v3(bone->head, me->mvert[v].co);
				copy_v3_v3(bone->tail, me->mvert[v].co);

				bone->head[1] = 1.0f;
				bone->rad_head = bone->rad_tail = 0.25;
			}

			if (emap[v].count >= 1) {
				skin_armature_bone_create(skin_ob,
					mvert, me->medge,
					arm,
					edges_visited,
					emap,
					bone,
					v);
			}
		}
	}

	MEM_freeN(edges_visited);
	MEM_freeN(emap);
	MEM_freeN(emap_mem);

	ED_armature_from_edit(bmain, arm);
	ED_armature_edit_free(arm);

	return arm_ob;
}

static int skin_armature_create_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C), *arm_ob;
	Mesh *me = ob->data;
	ModifierData *skin_md;
	ArmatureModifierData *arm_md;

	if (!CustomData_has_layer(&me->vdata, CD_MVERT_SKIN)) {
		BKE_reportf(op->reports, RPT_WARNING, "Mesh '%s' has no skin vertex data", me->id.name + 2);
		return OPERATOR_CANCELLED;
	}

	/* create new armature */
	arm_ob = modifier_skin_armature_create(depsgraph, bmain, scene, ob);

	/* add a modifier to connect the new armature to the mesh */
	arm_md = (ArmatureModifierData *)modifier_new(eModifierType_Armature);
	if (arm_md) {
		skin_md = edit_modifier_property_get(op, ob, eModifierType_Skin);
		BLI_insertlinkafter(&ob->modifiers, skin_md, arm_md);

		arm_md->object = arm_ob;
		arm_md->deformflag = ARM_DEF_VGROUP | ARM_DEF_QUATERNION;
		DEG_relations_tag_update(bmain);
		DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int skin_armature_create_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return skin_armature_create_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_skin_armature_create(wmOperatorType *ot)
{
	ot->name = "Skin Armature Create";
	ot->description = "Create an armature that parallels the skin layout";
	ot->idname = "OBJECT_OT_skin_armature_create";

	ot->poll = skin_poll;
	ot->invoke = skin_armature_create_invoke;
	ot->exec = skin_armature_create_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}
/************************ delta mush bind operator *********************/

static bool correctivesmooth_poll(bContext *C)
{
	return edit_modifier_poll_generic(C, &RNA_CorrectiveSmoothModifier, 0);
}

static int correctivesmooth_bind_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)edit_modifier_property_get(op, ob, eModifierType_CorrectiveSmooth);
	bool is_bind;

	if (!csmd) {
		return OPERATOR_CANCELLED;
	}

	if (!modifier_isEnabled(scene, &csmd->modifier, eModifierMode_Realtime)) {
		BKE_report(op->reports, RPT_ERROR, "Modifier is disabled");
		return OPERATOR_CANCELLED;
	}

	is_bind = (csmd->bind_coords != NULL);

	MEM_SAFE_FREE(csmd->bind_coords);
	MEM_SAFE_FREE(csmd->delta_cache);

	if (is_bind) {
		/* toggle off */
		csmd->bind_coords_num = 0;
	}
	else {
		/* signal to modifier to recalculate */
		csmd->bind_coords_num = (unsigned int)-1;
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int correctivesmooth_bind_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return correctivesmooth_bind_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_correctivesmooth_bind(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Corrective Smooth Bind";
	ot->description = "Bind base pose in Corrective Smooth modifier";
	ot->idname = "OBJECT_OT_correctivesmooth_bind";

	/* api callbacks */
	ot->poll = correctivesmooth_poll;
	ot->invoke = correctivesmooth_bind_invoke;
	ot->exec = correctivesmooth_bind_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/************************ mdef bind operator *********************/

static bool meshdeform_poll(bContext *C)
{
	return edit_modifier_poll_generic(C, &RNA_MeshDeformModifier, 0);
}

static int meshdeform_bind_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	MeshDeformModifierData *mmd = (MeshDeformModifierData *)edit_modifier_property_get(op, ob, eModifierType_MeshDeform);

	if (!mmd)
		return OPERATOR_CANCELLED;

	if (mmd->bindcagecos) {
		MEM_freeN(mmd->bindcagecos);
		if (mmd->dyngrid) MEM_freeN(mmd->dyngrid);
		if (mmd->dyninfluences) MEM_freeN(mmd->dyninfluences);
		if (mmd->bindinfluences) MEM_freeN(mmd->bindinfluences);
		if (mmd->bindoffsets) MEM_freeN(mmd->bindoffsets);
		if (mmd->dynverts) MEM_freeN(mmd->dynverts);
		if (mmd->bindweights) MEM_freeN(mmd->bindweights);  /* deprecated */
		if (mmd->bindcos) MEM_freeN(mmd->bindcos);  /* deprecated */

		mmd->bindcagecos = NULL;
		mmd->dyngrid = NULL;
		mmd->dyninfluences = NULL;
		mmd->bindinfluences = NULL;
		mmd->bindoffsets = NULL;
		mmd->dynverts = NULL;
		mmd->bindweights = NULL; /* deprecated */
		mmd->bindcos = NULL; /* deprecated */
		mmd->totvert = 0;
		mmd->totcagevert = 0;
		mmd->totinfluence = 0;

		DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	}
	else {
		DerivedMesh *dm;
		int mode = mmd->modifier.mode;

		/* force modifier to run, it will call binding routine */
		mmd->bindfunc = ED_mesh_deform_bind_callback;
		mmd->modifier.mode |= eModifierMode_Realtime;

		if (ob->type == OB_MESH) {
			dm = mesh_create_derived_view(depsgraph, scene, ob, 0);
			dm->release(dm);
		}
		else if (ob->type == OB_LATTICE) {
			BKE_lattice_modifiers_calc(depsgraph, scene, ob);
		}
		else if (ob->type == OB_MBALL) {
			BKE_displist_make_mball(depsgraph, scene, ob);
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
			BKE_displist_make_curveTypes(depsgraph, scene, ob, 0);
		}

		mmd->bindfunc = NULL;
		mmd->modifier.mode = mode;
	}

	return OPERATOR_FINISHED;
}

static int meshdeform_bind_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return meshdeform_bind_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_meshdeform_bind(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mesh Deform Bind";
	ot->description = "Bind mesh to cage in mesh deform modifier";
	ot->idname = "OBJECT_OT_meshdeform_bind";

	/* api callbacks */
	ot->poll = meshdeform_poll;
	ot->invoke = meshdeform_bind_invoke;
	ot->exec = meshdeform_bind_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/****************** explode refresh operator *********************/

static bool explode_poll(bContext *C)
{
	return edit_modifier_poll_generic(C, &RNA_ExplodeModifier, 0);
}

static int explode_refresh_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	ExplodeModifierData *emd = (ExplodeModifierData *)edit_modifier_property_get(op, ob, eModifierType_Explode);

	if (!emd)
		return OPERATOR_CANCELLED;

	emd->flag |= eExplodeFlag_CalcFaces;

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int explode_refresh_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return explode_refresh_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}


void OBJECT_OT_explode_refresh(wmOperatorType *ot)
{
	ot->name = "Explode Refresh";
	ot->description = "Refresh data in the Explode modifier";
	ot->idname = "OBJECT_OT_explode_refresh";

	ot->poll = explode_poll;
	ot->invoke = explode_refresh_invoke;
	ot->exec = explode_refresh_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}


/****************** ocean bake operator *********************/

static bool ocean_bake_poll(bContext *C)
{
	return edit_modifier_poll_generic(C, &RNA_OceanModifier, 0);
}

typedef struct OceanBakeJob {
	/* from wmJob */
	struct Object *owner;
	short *stop, *do_update;
	float *progress;
	int current_frame;
	struct OceanCache *och;
	struct Ocean *ocean;
	struct OceanModifierData *omd;
} OceanBakeJob;

static void oceanbake_free(void *customdata)
{
	OceanBakeJob *oj = customdata;
	MEM_freeN(oj);
}

/* called by oceanbake, only to check job 'stop' value */
static int oceanbake_breakjob(void *UNUSED(customdata))
{
	//OceanBakeJob *ob = (OceanBakeJob *)customdata;
	//return *(ob->stop);

	/* this is not nice yet, need to make the jobs list template better
	* for identifying/acting upon various different jobs */
	/* but for now we'll reuse the render break... */
	return (G.is_break);
}

/* called by oceanbake, wmJob sends notifier */
static void oceanbake_update(void *customdata, float progress, int *cancel)
{
	OceanBakeJob *oj = customdata;

	if (oceanbake_breakjob(oj))
		*cancel = 1;

	*(oj->do_update) = true;
	*(oj->progress) = progress;
}

static void oceanbake_startjob(void *customdata, short *stop, short *do_update, float *progress)
{
	OceanBakeJob *oj = customdata;

	oj->stop = stop;
	oj->do_update = do_update;
	oj->progress = progress;

	G.is_break = false;   /* XXX shared with render - replace with job 'stop' switch */

	BKE_ocean_bake(oj->ocean, oj->och, oceanbake_update, (void *)oj);

	*do_update = true;
	*stop = 0;
}

static void oceanbake_endjob(void *customdata)
{
	OceanBakeJob *oj = customdata;

	if (oj->ocean) {
		BKE_ocean_free(oj->ocean);
		oj->ocean = NULL;
	}

	oj->omd->oceancache = oj->och;
	oj->omd->cached = true;

	Object *ob = oj->owner;
	DEG_id_tag_update(&ob->id, DEG_TAG_COPY_ON_WRITE);
}

static int ocean_bake_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *ob = ED_object_active_context(C);
	OceanModifierData *omd = (OceanModifierData *)edit_modifier_property_get(op, ob, eModifierType_Ocean);
	Scene *scene = CTX_data_scene(C);
	OceanCache *och;
	struct Ocean *ocean;
	int f, cfra, i = 0;
	const bool free = RNA_boolean_get(op->ptr, "free");

	wmJob *wm_job;
	OceanBakeJob *oj;

	if (!omd)
		return OPERATOR_CANCELLED;

	if (free) {
		BKE_ocean_free_modifier_cache(omd);
		DEG_id_tag_update(&ob->id, OB_RECALC_DATA | DEG_TAG_COPY_ON_WRITE);
		WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
		return OPERATOR_FINISHED;
	}

	och = BKE_ocean_init_cache(omd->cachepath, modifier_path_relbase(bmain, ob),
		omd->bakestart, omd->bakeend, omd->wave_scale,
		omd->chop_amount, omd->foam_coverage, omd->foam_fade, omd->resolution);

	och->time = MEM_mallocN(och->duration * sizeof(float), "foam bake time");

	cfra = scene->r.cfra;

	/* precalculate time variable before baking */
	for (f = omd->bakestart; f <= omd->bakeend; f++) {
		/* from physics_fluid.c:
		*
		* XXX: This can't be used due to an anim sys optimization that ignores recalc object animation,
		* leaving it for the depgraph (this ignores object animation such as modifier properties though... :/ )
		* --> BKE_animsys_evaluate_all_animation(bmain, eval_time);
		* This doesn't work with drivers:
		* --> BKE_animsys_evaluate_animdata(&fsDomain->id, fsDomain->adt, eval_time, ADT_RECALC_ALL);
		*/

		/* Modifying the global scene isn't nice, but we can do it in
		* this part of the process before a threaded job is created */

		//scene->r.cfra = f;
		//ED_update_for_newframe(bmain, scene);

		/* ok, this doesn't work with drivers, but is way faster.
		* let's use this for now and hope nobody wants to drive the time value... */
		BKE_animsys_evaluate_animdata(CTX_data_depsgraph(C), scene, (ID *)ob, ob->adt, f, ADT_RECALC_ANIM);

		och->time[i] = omd->time;
		i++;
	}

	/* make a copy of ocean to use for baking - threadsafety */
	ocean = BKE_ocean_add();
	BKE_ocean_init_from_modifier(ocean, omd);

#if 0
	BKE_ocean_bake(ocean, och);

	omd->oceancache = och;
	omd->cached = true;

	scene->r.cfra = cfra;

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
#endif

	/* job stuff */

	scene->r.cfra = cfra;

	/* setup job */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, "Ocean Simulation",
		WM_JOB_PROGRESS, WM_JOB_TYPE_OBJECT_SIM_OCEAN);
	oj = MEM_callocN(sizeof(OceanBakeJob), "ocean bake job");
	oj->owner = ob;
	oj->ocean = ocean;
	oj->och = och;
	oj->omd = omd;

	WM_jobs_customdata_set(wm_job, oj, oceanbake_free);
	WM_jobs_timer(wm_job, 0.1, NC_OBJECT | ND_MODIFIER, NC_OBJECT | ND_MODIFIER);
	WM_jobs_callbacks(wm_job, oceanbake_startjob, NULL, NULL, oceanbake_endjob);

	WM_jobs_start(CTX_wm_manager(C), wm_job);



	return OPERATOR_FINISHED;
}

static int ocean_bake_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return ocean_bake_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}


void OBJECT_OT_ocean_bake(wmOperatorType *ot)
{
	ot->name = "Bake Ocean";
	ot->description = "Bake an image sequence of ocean data";
	ot->idname = "OBJECT_OT_ocean_bake";

	ot->poll = ocean_bake_poll;
	ot->invoke = ocean_bake_invoke;
	ot->exec = ocean_bake_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);

	RNA_def_boolean(ot->srna, "free", false, "Free", "Free the bake, rather than generating it");
}

/************************ LaplacianDeform bind operator *********************/

static bool laplaciandeform_poll(bContext *C)
{
	return edit_modifier_poll_generic(C, &RNA_LaplacianDeformModifier, 0);
}

static int laplaciandeform_bind_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)edit_modifier_property_get(op, ob, eModifierType_LaplacianDeform);

	if (!lmd)
		return OPERATOR_CANCELLED;
	if (lmd->flag & MOD_LAPLACIANDEFORM_BIND) {
		/* Un-binding happens inside the modifier when it's evaluated. */
		lmd->flag &= ~MOD_LAPLACIANDEFORM_BIND;
	}
	else {
		DerivedMesh *dm;
		int mode = lmd->modifier.mode;

		/* Force modifier to run, it will call binding routine. */
		/* TODO(Sybren): deduplicate the code below, it's used multiple times here. */
		lmd->modifier.mode |= eModifierMode_Realtime;
		lmd->flag |= MOD_LAPLACIANDEFORM_BIND;

		if (ob->type == OB_MESH) {
			dm = mesh_create_derived_view(depsgraph, scene, ob, 0);
			dm->release(dm);
		}
		else if (ob->type == OB_LATTICE) {
			BKE_lattice_modifiers_calc(depsgraph, scene, ob);
		}
		else if (ob->type == OB_MBALL) {
			BKE_displist_make_mball(depsgraph, scene, ob);
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
			BKE_displist_make_curveTypes(depsgraph, scene, ob, 0);
		}

		lmd->modifier.mode = mode;
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	return OPERATOR_FINISHED;
}

static int laplaciandeform_bind_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return laplaciandeform_bind_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_laplaciandeform_bind(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Laplacian Deform Bind";
	ot->description = "Bind mesh to system in laplacian deform modifier";
	ot->idname = "OBJECT_OT_laplaciandeform_bind";

	/* api callbacks */
	ot->poll = laplaciandeform_poll;
	ot->invoke = laplaciandeform_bind_invoke;
	ot->exec = laplaciandeform_bind_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}

/************************ sdef bind operator *********************/

static bool surfacedeform_bind_poll(bContext *C)
{
	return edit_modifier_poll_generic(C, &RNA_SurfaceDeformModifier, 0);
}

static int surfacedeform_bind_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)edit_modifier_property_get(op, ob, eModifierType_SurfaceDeform);

	if (!smd)
		return OPERATOR_CANCELLED;

	if (smd->flags & MOD_SDEF_BIND) {
		/* Un-binding happens inside the modifier when it's evaluated. */
		smd->flags &= ~MOD_SDEF_BIND;
	}
	else if (smd->target) {
		DerivedMesh *dm;
		int mode = smd->modifier.mode;

		/* Force modifier to run, it will call binding routine. */
		smd->modifier.mode |= eModifierMode_Realtime;
		smd->flags |= MOD_SDEF_BIND;

		if (ob->type == OB_MESH) {
			dm = mesh_create_derived_view(depsgraph, scene, ob, 0);
			dm->release(dm);
		}
		else if (ob->type == OB_LATTICE) {
			BKE_lattice_modifiers_calc(depsgraph, scene, ob);
		}
		else if (ob->type == OB_MBALL) {
			BKE_displist_make_mball(depsgraph, scene, ob);
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
			BKE_displist_make_curveTypes(depsgraph, scene, ob, 0);
		}

		smd->modifier.mode = mode;
	}

	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

static int surfacedeform_bind_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	if (edit_modifier_invoke_properties(C, op))
		return surfacedeform_bind_exec(C, op);
	else
		return OPERATOR_CANCELLED;
}

void OBJECT_OT_surfacedeform_bind(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Surface Deform Bind";
	ot->description = "Bind mesh to target in surface deform modifier";
	ot->idname = "OBJECT_OT_surfacedeform_bind";

	/* api callbacks */
	ot->poll = surfacedeform_bind_poll;
	ot->invoke = surfacedeform_bind_invoke;
	ot->exec = surfacedeform_bind_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	edit_modifier_properties(ot);
}
