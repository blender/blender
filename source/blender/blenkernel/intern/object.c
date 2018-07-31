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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/object.c
 *  \ingroup bke
 */


#include <string.h>
#include <math.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_smoke_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_rigidbody_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_linklist.h"
#include "BLI_kdtree.h"

#include "BLT_translation.h"

#include "BKE_pbvh.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_animsys.h"
#include "BKE_anim.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_icons.h"
#include "BKE_key.h"
#include "BKE_lamp.h"
#include "BKE_layer.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_linestyle.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_object_facemap.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_lightprobe.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_shader_fx.h"
#include "BKE_speaker.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_material.h"
#include "BKE_camera.h"
#include "BKE_image.h"
#include "BKE_gpencil.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DRW_engine.h"

#ifdef WITH_MOD_FLUID
#include "LBM_fluidsim.h"
#endif

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "CCGSubSurf.h"
#include "atomic_ops.h"

/* Vertex parent modifies original BMesh which is not safe for threading.
 * Ideally such a modification should be handled as a separate DAG update
 * callback for mesh datablock, but for until it is actually supported use
 * simpler solution with a mutex lock.
 *                                               - sergey -
 */
#define VPARENT_THREADING_HACK

#ifdef VPARENT_THREADING_HACK
static ThreadMutex vparent_lock = BLI_MUTEX_INITIALIZER;
#endif

void BKE_object_workob_clear(Object *workob)
{
	memset(workob, 0, sizeof(Object));

	workob->size[0] = workob->size[1] = workob->size[2] = 1.0f;
	workob->dscale[0] = workob->dscale[1] = workob->dscale[2] = 1.0f;
	workob->rotmode = ROT_MODE_EUL;
}

void BKE_object_free_particlesystems(Object *ob)
{
	ParticleSystem *psys;

	while ((psys = BLI_pophead(&ob->particlesystem))) {
		psys_free(ob, psys);
	}
}

void BKE_object_free_softbody(Object *ob)
{
	sbFree(ob);
}

void BKE_object_free_curve_cache(Object *ob)
{
	if (ob->runtime.curve_cache) {
		BKE_displist_free(&ob->runtime.curve_cache->disp);
		BKE_curve_bevelList_free(&ob->runtime.curve_cache->bev);
		if (ob->runtime.curve_cache->path) {
			free_path(ob->runtime.curve_cache->path);
		}
		BKE_nurbList_free(&ob->runtime.curve_cache->deformed_nurbs);
		MEM_freeN(ob->runtime.curve_cache);
		ob->runtime.curve_cache = NULL;
	}
}

void BKE_object_free_modifiers(Object *ob, const int flag)
{
	ModifierData *md;
	GpencilModifierData *gp_md;

	while ((md = BLI_pophead(&ob->modifiers))) {
		modifier_free_ex(md, flag);
	}

	while ((gp_md = BLI_pophead(&ob->greasepencil_modifiers))) {
		BKE_gpencil_modifier_free_ex(gp_md, flag);
	}
	/* particle modifiers were freed, so free the particlesystems as well */
	BKE_object_free_particlesystems(ob);

	/* same for softbody */
	BKE_object_free_softbody(ob);

	/* modifiers may have stored data in the DM cache */
	BKE_object_free_derived_caches(ob);
}

void BKE_object_free_shaderfx(Object *ob, const int flag)
{
	ShaderFxData *fx;

	while ((fx = BLI_pophead(&ob->shader_fx))) {
		BKE_shaderfx_free_ex(fx, flag);
	}
}

void BKE_object_modifier_hook_reset(Object *ob, HookModifierData *hmd)
{
	/* reset functionality */
	if (hmd->object) {
		bPoseChannel *pchan = BKE_pose_channel_find_name(hmd->object->pose, hmd->subtarget);

		if (hmd->subtarget[0] && pchan) {
			float imat[4][4], mat[4][4];

			/* calculate the world-space matrix for the pose-channel target first, then carry on as usual */
			mul_m4_m4m4(mat, hmd->object->obmat, pchan->pose_mat);

			invert_m4_m4(imat, mat);
			mul_m4_m4m4(hmd->parentinv, imat, ob->obmat);
		}
		else {
			invert_m4_m4(hmd->object->imat, hmd->object->obmat);
			mul_m4_m4m4(hmd->parentinv, hmd->object->imat, ob->obmat);
		}
	}
}

void BKE_object_modifier_gpencil_hook_reset(Object *ob, HookGpencilModifierData *hmd)
{
	if (hmd->object == NULL) {
		return;
	}
	/* reset functionality */
	bPoseChannel *pchan = BKE_pose_channel_find_name(hmd->object->pose, hmd->subtarget);

	if (hmd->subtarget[0] && pchan) {
		float imat[4][4], mat[4][4];

		/* calculate the world-space matrix for the pose-channel target first, then carry on as usual */
		mul_m4_m4m4(mat, hmd->object->obmat, pchan->pose_mat);

		invert_m4_m4(imat, mat);
		mul_m4_m4m4(hmd->parentinv, imat, ob->obmat);
	}
	else {
		invert_m4_m4(hmd->object->imat, hmd->object->obmat);
		mul_m4_m4m4(hmd->parentinv, hmd->object->imat, ob->obmat);
	}
}

bool BKE_object_support_modifier_type_check(const Object *ob, int modifier_type)
{
	const ModifierTypeInfo *mti;

	mti = modifierType_getInfo(modifier_type);

	/* only geometry objects should be able to get modifiers [#25291] */
	if (!ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_LATTICE)) {
		return false;
	}

	if (ob->type == OB_LATTICE && (mti->flags & eModifierTypeFlag_AcceptsLattice) == 0) {
		return false;
	}

	if (!((mti->flags & eModifierTypeFlag_AcceptsCVs) ||
	      (ob->type == OB_MESH && (mti->flags & eModifierTypeFlag_AcceptsMesh))))
	{
		return false;
	}

	return true;
}

void BKE_object_link_modifiers(Scene *scene, struct Object *ob_dst, const struct Object *ob_src)
{
	ModifierData *md;
	BKE_object_free_modifiers(ob_dst, 0);

	if (!ELEM(ob_dst->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_LATTICE)) {
		/* only objects listed above can have modifiers and linking them to objects
		 * which doesn't have modifiers stack is quite silly */
		return;
	}

	for (md = ob_src->modifiers.first; md; md = md->next) {
		ModifierData *nmd = NULL;

		if (ELEM(md->type,
		         eModifierType_Hook,
		         eModifierType_Collision))
		{
			continue;
		}

		if (!BKE_object_support_modifier_type_check(ob_dst, md->type))
			continue;

		switch (md->type) {
			case eModifierType_Softbody:
				BKE_object_copy_softbody(ob_dst, ob_src, 0);
				break;
			case eModifierType_Skin:
				/* ensure skin-node customdata exists */
				BKE_mesh_ensure_skin_customdata(ob_dst->data);
				break;
		}

		nmd = modifier_new(md->type);
		BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));

		if (md->type == eModifierType_Multires) {
			/* Has to be done after mod creation, but *before* we actually copy its settings! */
			multiresModifier_sync_levels_ex(scene, ob_dst, (MultiresModifierData *)md, (MultiresModifierData *)nmd);
		}

		modifier_copyData(md, nmd);
		BLI_addtail(&ob_dst->modifiers, nmd);
		modifier_unique_name(&ob_dst->modifiers, nmd);
	}

	BKE_object_copy_particlesystems(ob_dst, ob_src, 0);

	/* TODO: smoke?, cloth? */
}

/* free data derived from mesh, called when mesh changes or is freed */
void BKE_object_free_derived_caches(Object *ob)
{
	/* Also serves as signal to remake texspace.
	 *
	 * NOTE: This function can be called from threads on different objects
	 * sharing same data datablock. So we need to ensure atomic nature of
	 * data modification here.
	 */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (me && me->bb) {
			atomic_fetch_and_or_int32(&me->bb->flag, BOUNDBOX_DIRTY);
		}
	}
	else if (ELEM(ob->type, OB_SURF, OB_CURVE, OB_FONT)) {
		Curve *cu = ob->data;

		if (cu && cu->bb) {
			atomic_fetch_and_or_int32(&cu->bb->flag, BOUNDBOX_DIRTY);
		}
	}

	if (ob->bb) {
		MEM_freeN(ob->bb);
		ob->bb = NULL;
	}

	BKE_object_free_derived_mesh_caches(ob);

	if (ob->runtime.mesh_eval != NULL) {
		Mesh *mesh_eval = ob->runtime.mesh_eval;
		/* Restore initial pointer. */
		if (ob->data == mesh_eval) {
			ob->data = ob->runtime.mesh_orig;
		}
		/* Evaluated mesh points to edit mesh, but does not own it. */
		mesh_eval->edit_btmesh = NULL;
		BKE_mesh_free(mesh_eval);
		BKE_libblock_free_data(&mesh_eval->id, false);
		MEM_freeN(mesh_eval);
		ob->runtime.mesh_eval = NULL;
	}
	if (ob->runtime.mesh_deform_eval != NULL) {
		Mesh *mesh_deform_eval = ob->runtime.mesh_deform_eval;
		BKE_mesh_free(mesh_deform_eval);
		BKE_libblock_free_data(&mesh_deform_eval->id, false);
		MEM_freeN(mesh_deform_eval);
		ob->runtime.mesh_deform_eval = NULL;
	}

	BKE_object_free_curve_cache(ob);
}

void BKE_object_free_derived_mesh_caches(struct Object *ob)
{
	if (ob->derivedFinal) {
		ob->derivedFinal->needsFree = 1;
		ob->derivedFinal->release(ob->derivedFinal);
		ob->derivedFinal = NULL;
	}
	if (ob->derivedDeform) {
		ob->derivedDeform->needsFree = 1;
		ob->derivedDeform->release(ob->derivedDeform);
		ob->derivedDeform = NULL;
	}
}

void BKE_object_free_caches(Object *object)
{
	ModifierData *md;
	short update_flag = 0;

	/* Free particle system caches holding paths. */
	if (object->particlesystem.first) {
		ParticleSystem *psys;
		for (psys = object->particlesystem.first;
		     psys != NULL;
		     psys = psys->next)
		{
			psys_free_path_cache(psys, psys->edit);
			update_flag |= PSYS_RECALC_REDO;
		}
	}

	/* Free memory used by cached derived meshes in the particle system modifiers. */
	for (md = object->modifiers.first; md != NULL; md = md->next) {
		if (md->type == eModifierType_ParticleSystem) {
			ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;
			if (psmd->mesh_final) {
				BKE_id_free(NULL, psmd->mesh_final);
				psmd->mesh_final = NULL;
				if (psmd->mesh_original) {
					BKE_id_free(NULL, psmd->mesh_original);
					psmd->mesh_original = NULL;
				}
				psmd->flag |= eParticleSystemFlag_file_loaded;
				update_flag |= OB_RECALC_DATA;
			}
		}
	}

	/* NOTE: If object is coming from a duplicator, it might be a temporary
	 * object created by dependency graph, which shares pointers with original
	 * object. In this case we can not free anything.
	 */
	if ((object->base_flag & BASE_FROMDUPLI) == 0) {
		BKE_object_free_derived_caches(object);
		update_flag |= OB_RECALC_DATA;
	}

	/* Tag object for update, so once memory critical operation is over and
	 * scene update routines are back to it's business the object will be
	 * guaranteed to be in a known state.
	 */
	if (update_flag != 0) {
		DEG_id_tag_update(&object->id, update_flag);
	}
}

/** Free (or release) any data used by this object (does not free the object itself). */
void BKE_object_free(Object *ob)
{
	BKE_animdata_free((ID *)ob, false);

	DRW_drawdata_free((ID *)ob);

	/* BKE_<id>_free shall never touch to ID->us. Never ever. */
	BKE_object_free_modifiers(ob, LIB_ID_CREATE_NO_USER_REFCOUNT);
	BKE_object_free_shaderfx(ob, LIB_ID_CREATE_NO_USER_REFCOUNT);

	MEM_SAFE_FREE(ob->mat);
	MEM_SAFE_FREE(ob->matbits);
	MEM_SAFE_FREE(ob->iuser);
	MEM_SAFE_FREE(ob->bb);

	BLI_freelistN(&ob->defbase);
	BLI_freelistN(&ob->fmaps);
	if (ob->pose) {
		BKE_pose_free_ex(ob->pose, false);
		ob->pose = NULL;
	}
	if (ob->mpath) {
		animviz_free_motionpath(ob->mpath);
		ob->mpath = NULL;
	}

	BKE_constraints_free_ex(&ob->constraints, false);

	free_partdeflect(ob->pd);
	BKE_rigidbody_free_object(ob, NULL);
	BKE_rigidbody_free_constraint(ob);

	sbFree(ob);

	BKE_sculptsession_free(ob);

	BLI_freelistN(&ob->pc_ids);

	BLI_freelistN(&ob->lodlevels);

	/* Free runtime curves data. */
	if (ob->runtime.curve_cache) {
		BKE_curve_bevelList_free(&ob->runtime.curve_cache->bev);
		if (ob->runtime.curve_cache->path)
			free_path(ob->runtime.curve_cache->path);
		MEM_freeN(ob->runtime.curve_cache);
		ob->runtime.curve_cache = NULL;
	}

	BKE_previewimg_free(&ob->preview);
}

/* actual check for internal data, not context or flags */
bool BKE_object_is_in_editmode(const Object *ob)
{
	if (ob->data == NULL)
		return false;

	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		if (me->edit_btmesh)
			return true;
	}
	else if (ob->type == OB_ARMATURE) {
		bArmature *arm = ob->data;

		if (arm->edbo)
			return true;
	}
	else if (ob->type == OB_FONT) {
		Curve *cu = ob->data;

		if (cu->editfont)
			return true;
	}
	else if (ob->type == OB_MBALL) {
		MetaBall *mb = ob->data;

		if (mb->editelems)
			return true;
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = ob->data;

		if (lt->editlatt)
			return true;
	}
	else if (ob->type == OB_SURF || ob->type == OB_CURVE) {
		Curve *cu = ob->data;

		if (cu->editnurb)
			return true;
	}
	return false;
}

bool BKE_object_is_in_editmode_vgroup(const Object *ob)
{
	return (OB_TYPE_SUPPORT_VGROUP(ob->type) &&
	        BKE_object_is_in_editmode(ob));
}

bool BKE_object_data_is_in_editmode(const ID *id)
{
	const short type = GS(id->name);
	BLI_assert(OB_DATA_SUPPORT_EDITMODE(type));
	switch (type) {
		case ID_ME:
			return ((const Mesh *)id)->edit_btmesh != NULL;
		case ID_CU:
			return (
			        (((const Curve *)id)->editnurb != NULL) ||
			        (((const Curve *)id)->editfont != NULL)
			);
		case ID_MB:
			return ((const MetaBall *)id)->editelems != NULL;
		case ID_LT:
			return ((const Lattice *)id)->editlatt != NULL;
		case ID_AR:
			return ((const bArmature *)id)->edbo != NULL;
		default:
			BLI_assert(0);
			return false;
	}
}

bool BKE_object_is_in_wpaint_select_vert(const Object *ob)
{
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		return ((ob->mode & OB_MODE_WEIGHT_PAINT) &&
		        (me->edit_btmesh == NULL) &&
		        (ME_EDIT_PAINT_SEL_MODE(me) == SCE_SELECT_VERTEX));
	}

	return false;
}

bool BKE_object_has_mode_data(const struct Object *ob, eObjectMode object_mode)
{
	if (object_mode & OB_MODE_EDIT) {
		if (BKE_object_is_in_editmode(ob)) {
			return true;
		}
	}
	else if (object_mode & OB_MODE_VERTEX_PAINT) {
		if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_VERTEX_PAINT)) {
			return true;
		}
	}
	else if (object_mode & OB_MODE_WEIGHT_PAINT) {
		if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_WEIGHT_PAINT)) {
			return true;
		}
	}
	else if (object_mode & OB_MODE_SCULPT) {
		if (ob->sculpt && (ob->sculpt->mode_type == OB_MODE_SCULPT)) {
			return true;
		}
	}
	else if (object_mode & OB_MODE_POSE) {
		if (ob->pose != NULL) {
			return true;
		}
	}
	return false;
}

bool BKE_object_is_mode_compat(const struct Object *ob, eObjectMode object_mode)
{
	return ((ob->mode == object_mode) ||
	        (ob->mode & object_mode) != 0);
}

/**
 * Return if the object is visible, as evaluated by depsgraph
 */
bool BKE_object_is_visible(Object *ob, const eObjectVisibilityCheck mode)
{
	if ((ob->base_flag & BASE_VISIBLE) == 0) {
		return false;
	}

	if (mode == OB_VISIBILITY_CHECK_UNKNOWN_RENDER_MODE) {
		return true;
	}

	if (((ob->transflag & OB_DUPLI) == 0) &&
	    (ob->particlesystem.first == NULL))
	{
		return true;
	}

	switch (mode) {
		case OB_VISIBILITY_CHECK_FOR_VIEWPORT:
			return ((ob->duplicator_visibility_flag & OB_DUPLI_FLAG_VIEWPORT) != 0);
		case OB_VISIBILITY_CHECK_FOR_RENDER:
			return ((ob->duplicator_visibility_flag & OB_DUPLI_FLAG_RENDER) != 0);
		default:
			BLI_assert(!"Object visible test mode not supported.");
			return false;
	}
}

bool BKE_object_exists_check(Main *bmain, const Object *obtest)
{
	Object *ob;

	if (obtest == NULL) return false;

	ob = bmain->object.first;
	while (ob) {
		if (ob == obtest) return true;
		ob = ob->id.next;
	}
	return false;
}

/* *************************************************** */

static const char *get_obdata_defname(int type)
{
	switch (type) {
		case OB_MESH: return DATA_("Mesh");
		case OB_CURVE: return DATA_("Curve");
		case OB_SURF: return DATA_("Surf");
		case OB_FONT: return DATA_("Text");
		case OB_MBALL: return DATA_("Mball");
		case OB_CAMERA: return DATA_("Camera");
		case OB_LAMP: return DATA_("Light");
		case OB_LATTICE: return DATA_("Lattice");
		case OB_ARMATURE: return DATA_("Armature");
		case OB_SPEAKER: return DATA_("Speaker");
		case OB_EMPTY: return DATA_("Empty");
		case OB_GPENCIL: return DATA_("GPencil");
		default:
			printf("get_obdata_defname: Internal error, bad type: %d\n", type);
			return DATA_("Empty");
	}
}

void *BKE_object_obdata_add_from_type(Main *bmain, int type, const char *name)
{
	if (name == NULL) {
		name = get_obdata_defname(type);
	}

	switch (type) {
		case OB_MESH:      return BKE_mesh_add(bmain, name);
		case OB_CURVE:     return BKE_curve_add(bmain, name, OB_CURVE);
		case OB_SURF:      return BKE_curve_add(bmain, name, OB_SURF);
		case OB_FONT:      return BKE_curve_add(bmain, name, OB_FONT);
		case OB_MBALL:     return BKE_mball_add(bmain, name);
		case OB_CAMERA:    return BKE_camera_add(bmain, name);
		case OB_LAMP:      return BKE_lamp_add(bmain, name);
		case OB_LATTICE:   return BKE_lattice_add(bmain, name);
		case OB_ARMATURE:  return BKE_armature_add(bmain, name);
		case OB_SPEAKER:   return BKE_speaker_add(bmain, name);
		case OB_LIGHTPROBE:return BKE_lightprobe_add(bmain, name);
		case OB_GPENCIL:   return BKE_gpencil_data_addnew(bmain, name);
		case OB_EMPTY:     return NULL;
		default:
			printf("%s: Internal error, bad type: %d\n", __func__, type);
			return NULL;
	}
}

void BKE_object_init(Object *ob)
{
	/* BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(ob, id)); */  /* ob->type is already initialized... */

	ob->col[0] = ob->col[1] = ob->col[2] = 1.0;
	ob->col[3] = 1.0;

	ob->size[0] = ob->size[1] = ob->size[2] = 1.0;
	ob->dscale[0] = ob->dscale[1] = ob->dscale[2] = 1.0;

	/* objects should default to having Euler XYZ rotations,
	 * but rotations default to quaternions
	 */
	ob->rotmode = ROT_MODE_EUL;

	unit_axis_angle(ob->rotAxis, &ob->rotAngle);
	unit_axis_angle(ob->drotAxis, &ob->drotAngle);

	unit_qt(ob->quat);
	unit_qt(ob->dquat);

	/* rotation locks should be 4D for 4 component rotations by default... */
	ob->protectflag = OB_LOCK_ROT4D;

	unit_m4(ob->constinv);
	unit_m4(ob->parentinv);
	unit_m4(ob->obmat);
	ob->dt = OB_TEXTURE;
	ob->empty_drawtype = OB_PLAINAXES;
	ob->empty_drawsize = 1.0;
	if (ob->type == OB_EMPTY) {
		copy_v2_fl(ob->ima_ofs, -0.5f);
	}

	if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
		ob->trackflag = OB_NEGZ;
		ob->upflag = OB_POSY;
	}
	else {
		ob->trackflag = OB_POSY;
		ob->upflag = OB_POSZ;
	}

	ob->dupon = 1; ob->dupoff = 0;
	ob->dupsta = 1; ob->dupend = 100;
	ob->dupfacesca = 1.0;

	ob->col_group = 0x01;
	ob->col_mask = 0xffff;
	ob->preview = NULL;
	ob->duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT | OB_DUPLI_FLAG_RENDER;

	/* NT fluid sim defaults */
	ob->fluidsimSettings = NULL;

	BLI_listbase_clear(&ob->pc_ids);

	/* Animation Visualization defaults */
	animviz_settings_init(&ob->avs);

	ob->display.flag = OB_SHOW_SHADOW;
}

/* more general add: creates minimum required data, but without vertices etc. */
Object *BKE_object_add_only_object(Main *bmain, int type, const char *name)
{
	Object *ob;

	if (!name)
		name = get_obdata_defname(type);

	ob = BKE_libblock_alloc(bmain, ID_OB, name, 0);

	/* We increase object user count when linking to Collections. */
	id_us_min(&ob->id);

	/* default object vars */
	ob->type = type;

	BKE_object_init(ob);

	return ob;
}


static Object *object_add_common(Main *bmain, ViewLayer *view_layer, int type, const char *name)
{
	Object *ob;

	ob = BKE_object_add_only_object(bmain, type, name);
	ob->data = BKE_object_obdata_add_from_type(bmain, type, name);
	BKE_view_layer_base_deselect_all(view_layer);

	DEG_id_tag_update_ex(bmain, &ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
	return ob;
}

/**
 * General add: to scene, with layer from area and default name
 *
 * Object is added to the active Collection.
 * If there is no linked collection to the active ViewLayer we create a new one.
 */
/* creates minimum required data, but without vertices etc. */
Object *BKE_object_add(
        Main *bmain, Scene *UNUSED(scene), ViewLayer *view_layer,
        int type, const char *name)
{
	Object *ob;
	Base *base;
	LayerCollection *layer_collection;

	ob = object_add_common(bmain, view_layer, type, name);

	layer_collection = BKE_layer_collection_get_active(view_layer);
	BKE_collection_object_add(bmain, layer_collection->collection, ob);

	base = BKE_view_layer_base_find(view_layer, ob);
	BKE_view_layer_base_select(view_layer, base);

	return ob;
}

/**
 * Add a new object, using another one as a reference
 *
 * \param ob_src object to use to determine the collections of the new object.
 */
Object *BKE_object_add_from(
        Main *bmain, Scene *scene, ViewLayer *view_layer,
        int type, const char *name, Object *ob_src)
{
	Object *ob;
	Base *base;

	ob = object_add_common(bmain, view_layer, type, name);
	BKE_collection_object_add_from(bmain, scene, ob_src, ob);

	base = BKE_view_layer_base_find(view_layer, ob);
	BKE_view_layer_base_select(view_layer, base);

	return ob;
}

/**
 * Add a new object, but assign the given datablock as the ob->data
 * for the newly created object.
 *
 * \param data The datablock to assign as ob->data for the new object.
 *             This is assumed to be of the correct type.
 * \param do_id_user If true, id_us_plus() will be called on data when
 *                 assigning it to the object.
 */
Object *BKE_object_add_for_data(
        Main *bmain, ViewLayer *view_layer,
        int type, const char *name, ID *data, bool do_id_user)
{
	Object *ob;
	Base *base;
	LayerCollection *layer_collection;

	/* same as object_add_common, except we don't create new ob->data */
	ob = BKE_object_add_only_object(bmain, type, name);
	ob->data = data;
	if (do_id_user) id_us_plus(data);

	BKE_view_layer_base_deselect_all(view_layer);
	DEG_id_tag_update_ex(bmain, &ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

	layer_collection = BKE_layer_collection_get_active(view_layer);
	BKE_collection_object_add(bmain, layer_collection->collection, ob);

	base = BKE_view_layer_base_find(view_layer, ob);
	BKE_view_layer_base_select(view_layer, base);

	return ob;
}


void BKE_object_copy_softbody(struct Object *ob_dst, const struct Object *ob_src, const int flag)
{
	SoftBody *sb = ob_src->soft;
	SoftBody *sbn;
	bool tagged_no_main = ob_dst->id.tag & LIB_TAG_NO_MAIN;

	ob_dst->softflag = ob_src->softflag;
	if (sb == NULL) {
		ob_dst->soft = NULL;
		return;
	}

	sbn = MEM_dupallocN(sb);

	if ((flag & LIB_ID_COPY_CACHES) == 0) {
		sbn->totspring = sbn->totpoint = 0;
		sbn->bpoint = NULL;
		sbn->bspring = NULL;
	}
	else {
		sbn->totspring = sb->totspring;
		sbn->totpoint = sb->totpoint;

		if (sbn->bpoint) {
			int i;

			sbn->bpoint = MEM_dupallocN(sbn->bpoint);

			for (i = 0; i < sbn->totpoint; i++) {
				if (sbn->bpoint[i].springs)
					sbn->bpoint[i].springs = MEM_dupallocN(sbn->bpoint[i].springs);
			}
		}

		if (sb->bspring)
			sbn->bspring = MEM_dupallocN(sb->bspring);
	}

	sbn->keys = NULL;
	sbn->totkey = sbn->totpointkey = 0;

	sbn->scratch = NULL;

	if (tagged_no_main == 0) {
		sbn->shared = MEM_dupallocN(sb->shared);
		sbn->shared->pointcache = BKE_ptcache_copy_list(&sbn->shared->ptcaches, &sb->shared->ptcaches, flag);
	}

	if (sb->effector_weights)
		sbn->effector_weights = MEM_dupallocN(sb->effector_weights);

	ob_dst->soft = sbn;
}

ParticleSystem *BKE_object_copy_particlesystem(ParticleSystem *psys, const int flag)
{
	ParticleSystem *psysn = MEM_dupallocN(psys);

	psys_copy_particles(psysn, psys);

	if (psys->clmd) {
		psysn->clmd = (ClothModifierData *)modifier_new(eModifierType_Cloth);
		modifier_copyData_ex((ModifierData *)psys->clmd, (ModifierData *)psysn->clmd, flag);
		psys->hair_in_mesh = psys->hair_out_mesh = NULL;
	}

	BLI_duplicatelist(&psysn->targets, &psys->targets);

	psysn->pathcache = NULL;
	psysn->childcache = NULL;
	psysn->edit = NULL;
	psysn->pdd = NULL;
	psysn->effectors = NULL;
	psysn->tree = NULL;
	psysn->bvhtree = NULL;
	psysn->batch_cache = NULL;

	BLI_listbase_clear(&psysn->pathcachebufs);
	BLI_listbase_clear(&psysn->childcachebufs);

	psysn->pointcache = BKE_ptcache_copy_list(&psysn->ptcaches, &psys->ptcaches, flag);

	/* XXX - from reading existing code this seems correct but intended usage of
	 * pointcache should /w cloth should be added in 'ParticleSystem' - campbell */
	if (psysn->clmd) {
		psysn->clmd->point_cache = psysn->pointcache;
	}

	if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
		id_us_plus((ID *)psysn->part);
	}

	return psysn;
}

void BKE_object_copy_particlesystems(Object *ob_dst, const Object *ob_src, const int flag)
{
	ParticleSystem *psys, *npsys;
	ModifierData *md;

	if (ob_dst->type != OB_MESH) {
		/* currently only mesh objects can have soft body */
		return;
	}

	BLI_listbase_clear(&ob_dst->particlesystem);
	for (psys = ob_src->particlesystem.first; psys; psys = psys->next) {
		npsys = BKE_object_copy_particlesystem(psys, flag);

		BLI_addtail(&ob_dst->particlesystem, npsys);

		/* need to update particle modifiers too */
		for (md = ob_dst->modifiers.first; md; md = md->next) {
			if (md->type == eModifierType_ParticleSystem) {
				ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
				if (psmd->psys == psys)
					psmd->psys = npsys;
			}
			else if (md->type == eModifierType_DynamicPaint) {
				DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
				if (pmd->brush) {
					if (pmd->brush->psys == psys) {
						pmd->brush->psys = npsys;
					}
				}
			}
			else if (md->type == eModifierType_Smoke) {
				SmokeModifierData *smd = (SmokeModifierData *) md;

				if (smd->type == MOD_SMOKE_TYPE_FLOW) {
					if (smd->flow) {
						if (smd->flow->psys == psys)
							smd->flow->psys = npsys;
					}
				}
			}
		}
	}
}

static void copy_object_pose(Object *obn, const Object *ob, const int flag)
{
	bPoseChannel *chan;

	/* note: need to clear obn->pose pointer first, so that BKE_pose_copy_data works (otherwise there's a crash) */
	obn->pose = NULL;
	BKE_pose_copy_data_ex(&obn->pose, ob->pose, flag, true);  /* true = copy constraints */

	for (chan = obn->pose->chanbase.first; chan; chan = chan->next) {
		bConstraint *con;

		chan->flag &= ~(POSE_LOC | POSE_ROT | POSE_SIZE);

		/* XXX Remapping object pointing onto itself should be handled by generic BKE_library_remap stuff, but...
		 *     the flush_constraint_targets callback am not sure about, so will delay that for now. */
		for (con = chan->constraints.first; con; con = con->next) {
			const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;

			if (cti && cti->get_constraint_targets) {
				cti->get_constraint_targets(con, &targets);

				for (ct = targets.first; ct; ct = ct->next) {
					if (ct->tar == ob)
						ct->tar = obn;
				}

				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 0);
			}
		}
	}
}

static void copy_object_lod(Object *obn, const Object *ob, const int UNUSED(flag))
{
	BLI_duplicatelist(&obn->lodlevels, &ob->lodlevels);

	obn->currentlod = (LodLevel *)obn->lodlevels.first;
}

bool BKE_object_pose_context_check(const Object *ob)
{
	if ((ob) &&
	    (ob->type == OB_ARMATURE) &&
	    (ob->pose) &&
	    (ob->mode & OB_MODE_POSE))
	{
		return true;
	}
	else {
		return false;
	}
}

Object *BKE_object_pose_armature_get(Object *ob)
{
	if (ob == NULL)
		return NULL;

	if (BKE_object_pose_context_check(ob))
		return ob;

	ob = modifiers_isDeformedByArmature(ob);

	/* Only use selected check when non-active. */
	if (BKE_object_pose_context_check(ob))
		return ob;

	return NULL;
}

Object *BKE_object_pose_armature_get_visible(Object *ob, ViewLayer *view_layer)
{
	Object *ob_armature = BKE_object_pose_armature_get(ob);
	if (ob_armature) {
		Base *base = BKE_view_layer_base_find(view_layer, ob_armature);
		if (base) {
			if (BASE_VISIBLE(base)) {
				return ob_armature;
			}
		}
	}
	return NULL;
}

/**
 * Access pose array with special check to get pose object when in weight paint mode.
 */
Object **BKE_object_pose_array_get_ex(ViewLayer *view_layer, uint *r_objects_len, bool unique)
{
	Object *ob_active = OBACT(view_layer);
	Object *ob_pose = BKE_object_pose_armature_get(ob_active);
	Object **objects = NULL;
	if (ob_pose == ob_active) {
		objects = BKE_view_layer_array_from_objects_in_mode(
		        view_layer, r_objects_len, {
		            .object_mode = OB_MODE_POSE,
		            .no_dup_data = unique});
	}
	else if (ob_pose != NULL) {
		*r_objects_len = 1;
		objects = MEM_mallocN(sizeof(*objects), __func__);
		objects[0] = ob_pose;
	}
	else {
		*r_objects_len = 0;
		objects = MEM_mallocN(0, __func__);
	}
	return objects;
}
Object **BKE_object_pose_array_get_unique(ViewLayer *view_layer, uint *r_objects_len)
{
	return BKE_object_pose_array_get_ex(view_layer, r_objects_len, true);
}
Object **BKE_object_pose_array_get(ViewLayer *view_layer, uint *r_objects_len)
{
	return BKE_object_pose_array_get_ex(view_layer, r_objects_len, false);
}

Base **BKE_object_pose_base_array_get_ex(ViewLayer *view_layer, uint *r_bases_len, bool unique)
{
	Base *base_active = BASACT(view_layer);
	Object *ob_pose = base_active ? BKE_object_pose_armature_get(base_active->object) : NULL;
	Base *base_pose = NULL;
	Base **bases = NULL;

	if (base_active) {
		if (ob_pose == base_active->object) {
			base_pose = base_active;
		}
		else {
			base_pose = BKE_view_layer_base_find(view_layer, ob_pose);
		}
	}

	if (base_active && (base_pose == base_active)) {
		bases = BKE_view_layer_array_from_bases_in_mode(
		        view_layer, r_bases_len, {
		            .object_mode = OB_MODE_POSE,
		            .no_dup_data = unique});
	}
	else if (base_pose != NULL) {
		*r_bases_len = 1;
		bases = MEM_mallocN(sizeof(*bases), __func__);
		bases[0] = base_pose;
	}
	else {
		*r_bases_len = 0;
		bases = MEM_mallocN(0, __func__);
	}
	return bases;
}
Base **BKE_object_pose_base_array_get_unique(ViewLayer *view_layer, uint *r_bases_len)
{
	return BKE_object_pose_base_array_get_ex(view_layer, r_bases_len, true);
}
Base **BKE_object_pose_base_array_get(ViewLayer *view_layer, uint *r_bases_len)
{
	return BKE_object_pose_base_array_get_ex(view_layer, r_bases_len, false);
}

void BKE_object_transform_copy(Object *ob_tar, const Object *ob_src)
{
	copy_v3_v3(ob_tar->loc, ob_src->loc);
	copy_v3_v3(ob_tar->rot, ob_src->rot);
	copy_v3_v3(ob_tar->quat, ob_src->quat);
	copy_v3_v3(ob_tar->rotAxis, ob_src->rotAxis);
	ob_tar->rotAngle = ob_src->rotAngle;
	ob_tar->rotmode = ob_src->rotmode;
	copy_v3_v3(ob_tar->size, ob_src->size);
}

/**
 * Only copy internal data of Object ID from source to already allocated/initialized destination.
 * You probably nerver want to use that directly, use id_copy or BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_object_copy_data(Main *bmain, Object *ob_dst, const Object *ob_src, const int flag)
{
	ModifierData *md;
	GpencilModifierData *gmd;
	ShaderFxData *fx;

	/* Do not copy runtime data. */
	BKE_object_runtime_reset(ob_dst);

	/* We never handle usercount here for own data. */
	const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

	if (ob_src->totcol) {
		ob_dst->mat = MEM_dupallocN(ob_src->mat);
		ob_dst->matbits = MEM_dupallocN(ob_src->matbits);
		ob_dst->totcol = ob_src->totcol;
	}

	if (ob_src->iuser) ob_dst->iuser = MEM_dupallocN(ob_src->iuser);

	if (ob_src->bb) ob_dst->bb = MEM_dupallocN(ob_src->bb);

	BLI_listbase_clear(&ob_dst->modifiers);

	for (md = ob_src->modifiers.first; md; md = md->next) {
		ModifierData *nmd = modifier_new(md->type);
		BLI_strncpy(nmd->name, md->name, sizeof(nmd->name));
		modifier_copyData_ex(md, nmd, flag_subdata);
		BLI_addtail(&ob_dst->modifiers, nmd);
	}

	BLI_listbase_clear(&ob_dst->greasepencil_modifiers);

	for (gmd = ob_src->greasepencil_modifiers.first; gmd; gmd = gmd->next) {
		GpencilModifierData *nmd = BKE_gpencil_modifier_new(gmd->type);
		BLI_strncpy(nmd->name, gmd->name, sizeof(nmd->name));
		BKE_gpencil_modifier_copyData_ex(gmd, nmd, flag_subdata);
		BLI_addtail(&ob_dst->greasepencil_modifiers, nmd);
	}

	BLI_listbase_clear(&ob_dst->shader_fx);

	for (fx = ob_src->shader_fx.first; fx; fx = fx->next) {
		ShaderFxData *nfx = BKE_shaderfx_new(fx->type);
		BLI_strncpy(nfx->name, fx->name, sizeof(nfx->name));
		BKE_shaderfx_copyData_ex(fx, nfx, flag_subdata);
		BLI_addtail(&ob_dst->shader_fx, nfx);
	}

	if (ob_src->pose) {
		copy_object_pose(ob_dst, ob_src, flag_subdata);
		/* backwards compat... non-armatures can get poses in older files? */
		if (ob_src->type == OB_ARMATURE)
			BKE_pose_rebuild(bmain, ob_dst, ob_dst->data);
	}
	defgroup_copy_list(&ob_dst->defbase, &ob_src->defbase);
	BKE_object_facemap_copy_list(&ob_dst->fmaps, &ob_src->fmaps);
	BKE_constraints_copy_ex(&ob_dst->constraints, &ob_src->constraints, flag_subdata, true);

	ob_dst->mode = OB_MODE_OBJECT;
	ob_dst->sculpt = NULL;

	if (ob_src->pd) {
		ob_dst->pd = MEM_dupallocN(ob_src->pd);
		if (ob_dst->pd->rng) {
			ob_dst->pd->rng = MEM_dupallocN(ob_src->pd->rng);
		}
	}
	BKE_object_copy_softbody(ob_dst, ob_src, flag_subdata);
	ob_dst->rigidbody_object = BKE_rigidbody_copy_object(ob_src, flag_subdata);
	ob_dst->rigidbody_constraint = BKE_rigidbody_copy_constraint(ob_src, flag_subdata);

	BKE_object_copy_particlesystems(ob_dst, ob_src, flag_subdata);

	ob_dst->derivedDeform = NULL;
	ob_dst->derivedFinal = NULL;

	BLI_listbase_clear((ListBase *)&ob_dst->drawdata);
	BLI_listbase_clear(&ob_dst->pc_ids);

	/* grease pencil: clean derived data */
	if (ob_dst->type == OB_GPENCIL)
		BKE_gpencil_free_derived_frames(ob_dst->data);

	ob_dst->avs = ob_src->avs;
	ob_dst->mpath = animviz_copy_motionpath(ob_src->mpath);

	copy_object_lod(ob_dst, ob_src, flag_subdata);

	/* Do not copy object's preview (mostly due to the fact renderers create temp copy of objects). */
	if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0 && false) {  /* XXX TODO temp hack */
		BKE_previewimg_id_copy(&ob_dst->id, &ob_src->id);
	}
	else {
		ob_dst->preview = NULL;
	}
}

/* copy objects, will re-initialize cached simulation data */
Object *BKE_object_copy(Main *bmain, const Object *ob)
{
	Object *ob_copy;
	BKE_id_copy_ex(bmain, &ob->id, (ID **)&ob_copy, 0, false);

	/* We increase object user count when linking to Collections. */
	id_us_min(&ob_copy->id);

	return ob_copy;
}

void BKE_object_make_local_ex(Main *bmain, Object *ob, const bool lib_local, const bool clear_proxy)
{
	bool is_local = false, is_lib = false;

	/* - only lib users: do nothing (unless force_local is set)
	 * - only local users: set flag
	 * - mixed: make copy
	 * In case we make a whole lib's content local, we always want to localize, and we skip remapping (done later).
	 */

	if (!ID_IS_LINKED(ob)) {
		return;
	}

	BKE_library_ID_test_usages(bmain, ob, &is_local, &is_lib);

	if (lib_local || is_local) {
		if (!is_lib) {
			id_clear_lib_data(bmain, &ob->id);
			BKE_id_expand_local(bmain, &ob->id);
			if (clear_proxy) {
				if (ob->proxy_from != NULL) {
					ob->proxy_from->proxy = NULL;
					ob->proxy_from->proxy_group = NULL;
				}
				ob->proxy = ob->proxy_from = ob->proxy_group = NULL;
			}
		}
		else {
			Object *ob_new = BKE_object_copy(bmain, ob);

			ob_new->id.us = 0;
			ob_new->proxy = ob_new->proxy_from = ob_new->proxy_group = NULL;

			/* setting newid is mandatory for complex make_lib_local logic... */
			ID_NEW_SET(ob, ob_new);

			if (!lib_local) {
				BKE_libblock_remap(bmain, ob, ob_new, ID_REMAP_SKIP_INDIRECT_USAGE);
			}
		}
	}
}

void BKE_object_make_local(Main *bmain, Object *ob, const bool lib_local)
{
	BKE_object_make_local_ex(bmain, ob, lib_local, true);
}

/* Returns true if the Object is from an external blend file (libdata) */
bool BKE_object_is_libdata(const Object *ob)
{
	return (ob && ID_IS_LINKED(ob));
}

/* Returns true if the Object data is from an external blend file (libdata) */
bool BKE_object_obdata_is_libdata(const Object *ob)
{
	/* Linked objects with local obdata are forbidden! */
	BLI_assert(!ob || !ob->data || (ID_IS_LINKED(ob) ? ID_IS_LINKED(ob->data) : true));
	return (ob && ob->data && ID_IS_LINKED(ob->data));
}

/* *************** PROXY **************** */

/* when you make proxy, ensure the exposed layers are extern */
static void armature_set_id_extern(Object *ob)
{
	bArmature *arm = ob->data;
	bPoseChannel *pchan;
	unsigned int lay = arm->layer_protected;

	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		if (!(pchan->bone->layer & lay))
			id_lib_extern((ID *)pchan->custom);
	}

}

void BKE_object_copy_proxy_drivers(Object *ob, Object *target)
{
	if ((target->adt) && (target->adt->drivers.first)) {
		FCurve *fcu;

		/* add new animdata block */
		if (!ob->adt)
			ob->adt = BKE_animdata_add_id(&ob->id);

		/* make a copy of all the drivers (for now), then correct any links that need fixing */
		free_fcurves(&ob->adt->drivers);
		copy_fcurves(&ob->adt->drivers, &target->adt->drivers);

		for (fcu = ob->adt->drivers.first; fcu; fcu = fcu->next) {
			ChannelDriver *driver = fcu->driver;
			DriverVar *dvar;

			for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
				/* all drivers */
				DRIVER_TARGETS_LOOPER(dvar)
				{
					if (dtar->id) {
						if ((Object *)dtar->id == target)
							dtar->id = (ID *)ob;
						else {
							/* only on local objects because this causes indirect links
							 * 'a -> b -> c', blend to point directly to a.blend
							 * when a.blend has a proxy thats linked into c.blend  */
							if (!ID_IS_LINKED(ob))
								id_lib_extern((ID *)dtar->id);
						}
					}
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
	}
}

/* proxy rule: lib_object->proxy_from == the one we borrow from, set temporally while object_update */
/*             local_object->proxy == pointer to library object, saved in files and read */
/*             local_object->proxy_group == pointer to collection dupli-object, saved in files and read */

void BKE_object_make_proxy(Main *bmain, Object *ob, Object *target, Object *cob)
{
	/* paranoia checks */
	if (ID_IS_LINKED(ob) || !ID_IS_LINKED(target)) {
		printf("cannot make proxy\n");
		return;
	}

	ob->proxy = target;
	ob->proxy_group = cob;
	id_lib_extern(&target->id);

	DEG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
	DEG_id_tag_update(&target->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

	/* copy transform
	 * - cob means this proxy comes from a collection, just apply the matrix
	 *   so the object wont move from its dupli-transform.
	 *
	 * - no cob means this is being made from a linked object,
	 *   this is closer to making a copy of the object - in-place. */
	if (cob) {
		ob->rotmode = target->rotmode;
		mul_m4_m4m4(ob->obmat, cob->obmat, target->obmat);
		if (cob->dup_group) { /* should always be true */
			float tvec[3];
			mul_v3_mat3_m4v3(tvec, ob->obmat, cob->dup_group->dupli_ofs);
			sub_v3_v3(ob->obmat[3], tvec);
		}
		BKE_object_apply_mat4(ob, ob->obmat, false, true);
	}
	else {
		BKE_object_transform_copy(ob, target);
		ob->parent = target->parent; /* libdata */
		copy_m4_m4(ob->parentinv, target->parentinv);
	}

	/* copy animdata stuff - drivers only for now... */
	BKE_object_copy_proxy_drivers(ob, target);

	/* skip constraints? */
	/* FIXME: this is considered by many as a bug */

	/* set object type and link to data */
	ob->type = target->type;
	ob->data = target->data;
	id_us_plus((ID *)ob->data);     /* ensures lib data becomes LIB_TAG_EXTERN */

	/* copy vertex groups */
	defgroup_copy_list(&ob->defbase, &target->defbase);

	/* copy material and index information */
	ob->actcol = ob->totcol = 0;
	if (ob->mat) MEM_freeN(ob->mat);
	if (ob->matbits) MEM_freeN(ob->matbits);
	ob->mat = NULL;
	ob->matbits = NULL;
	if ((target->totcol) && (target->mat) && OB_TYPE_SUPPORT_MATERIAL(ob->type)) {
		int i;

		ob->actcol = target->actcol;
		ob->totcol = target->totcol;

		ob->mat = MEM_dupallocN(target->mat);
		ob->matbits = MEM_dupallocN(target->matbits);
		for (i = 0; i < target->totcol; i++) {
			/* don't need to run test_object_materials since we know this object is new and not used elsewhere */
			id_us_plus((ID *)ob->mat[i]);
		}
	}

	/* type conversions */
	if (target->type == OB_ARMATURE) {
		copy_object_pose(ob, target, 0);   /* data copy, object pointers in constraints */
		BKE_pose_rest(ob->pose);            /* clear all transforms in channels */
		BKE_pose_rebuild(bmain, ob, ob->data); /* set all internal links */

		armature_set_id_extern(ob);
	}
	else if (target->type == OB_EMPTY) {
		ob->empty_drawtype = target->empty_drawtype;
		ob->empty_drawsize = target->empty_drawsize;
	}

	/* copy IDProperties */
	if (ob->id.properties) {
		IDP_FreeProperty(ob->id.properties);
		MEM_freeN(ob->id.properties);
		ob->id.properties = NULL;
	}
	if (target->id.properties) {
		ob->id.properties = IDP_CopyProperty(target->id.properties);
	}

	/* copy drawtype info */
	ob->dt = target->dt;
}

/**
 * Use with newly created objects to set their size
 * (used to apply scene-scale).
 */
void BKE_object_obdata_size_init(struct Object *ob, const float size)
{
	/* apply radius as a scale to types that support it */
	switch (ob->type) {
		case OB_EMPTY:
		{
			ob->empty_drawsize *= size;
			break;
		}
		case OB_GPENCIL:
		{
			ob->empty_drawsize *= size;
			break;
		}
		case OB_FONT:
		{
			Curve *cu = ob->data;
			cu->fsize *= size;
			break;
		}
		case OB_CAMERA:
		{
			Camera *cam = ob->data;
			cam->drawsize *= size;
			break;
		}
		case OB_LAMP:
		{
			Lamp *lamp = ob->data;
			lamp->dist *= size;
			lamp->area_size  *= size;
			lamp->area_sizey *= size;
			lamp->area_sizez *= size;
			break;
		}
		/* Only lattice (not mesh, curve, mball...),
		 * because its got data when newly added */
		case OB_LATTICE:
		{
			struct Lattice *lt = ob->data;
			float mat[4][4];

			unit_m4(mat);
			scale_m4_fl(mat, size);

			BKE_lattice_transform(lt, (float (*)[4])mat, false);
			break;
		}
	}
}

/* *************** CALC ****************** */

void BKE_object_scale_to_mat3(Object *ob, float mat[3][3])
{
	float vec[3];
	mul_v3_v3v3(vec, ob->size, ob->dscale);
	size_to_mat3(mat, vec);
}

void BKE_object_rot_to_mat3(Object *ob, float mat[3][3], bool use_drot)
{
	float rmat[3][3], dmat[3][3];

	/* 'dmat' is the delta-rotation matrix, which will get (pre)multiplied
	 * with the rotation matrix to yield the appropriate rotation
	 */

	/* rotations may either be quats, eulers (with various rotation orders), or axis-angle */
	if (ob->rotmode > 0) {
		/* euler rotations (will cause gimble lock, but this can be alleviated a bit with rotation orders) */
		eulO_to_mat3(rmat, ob->rot, ob->rotmode);
		eulO_to_mat3(dmat, ob->drot, ob->rotmode);
	}
	else if (ob->rotmode == ROT_MODE_AXISANGLE) {
		/* axis-angle - not really that great for 3D-changing orientations */
		axis_angle_to_mat3(rmat, ob->rotAxis, ob->rotAngle);
		axis_angle_to_mat3(dmat, ob->drotAxis, ob->drotAngle);
	}
	else {
		/* quats are normalized before use to eliminate scaling issues */
		float tquat[4];

		normalize_qt_qt(tquat, ob->quat);
		quat_to_mat3(rmat, tquat);

		normalize_qt_qt(tquat, ob->dquat);
		quat_to_mat3(dmat, tquat);
	}

	/* combine these rotations */
	if (use_drot)
		mul_m3_m3m3(mat, dmat, rmat);
	else
		copy_m3_m3(mat, rmat);
}

void BKE_object_mat3_to_rot(Object *ob, float mat[3][3], bool use_compat)
{
	BLI_ASSERT_UNIT_M3(mat);

	switch (ob->rotmode) {
		case ROT_MODE_QUAT:
		{
			float dquat[4];
			mat3_normalized_to_quat(ob->quat, mat);
			normalize_qt_qt(dquat, ob->dquat);
			invert_qt_normalized(dquat);
			mul_qt_qtqt(ob->quat, dquat, ob->quat);
			break;
		}
		case ROT_MODE_AXISANGLE:
		{
			float quat[4];
			float dquat[4];

			/* without drot we could apply 'mat' directly */
			mat3_normalized_to_quat(quat, mat);
			axis_angle_to_quat(dquat, ob->drotAxis, ob->drotAngle);
			invert_qt_normalized(dquat);
			mul_qt_qtqt(quat, dquat, quat);
			quat_to_axis_angle(ob->rotAxis, &ob->rotAngle, quat);
			break;
		}
		default: /* euler */
		{
			float quat[4];
			float dquat[4];

			/* without drot we could apply 'mat' directly */
			mat3_normalized_to_quat(quat, mat);
			eulO_to_quat(dquat, ob->drot, ob->rotmode);
			invert_qt_normalized(dquat);
			mul_qt_qtqt(quat, dquat, quat);
			/* end drot correction */

			if (use_compat) quat_to_compatible_eulO(ob->rot, ob->rot, ob->rotmode, quat);
			else            quat_to_eulO(ob->rot, ob->rotmode, quat);
			break;
		}
	}
}

void BKE_object_tfm_protected_backup(const Object *ob,
                                     ObjectTfmProtectedChannels *obtfm)
{

#define TFMCPY(_v) (obtfm->_v = ob->_v)
#define TFMCPY3D(_v) copy_v3_v3(obtfm->_v, ob->_v)
#define TFMCPY4D(_v) copy_v4_v4(obtfm->_v, ob->_v)

	TFMCPY3D(loc);
	TFMCPY3D(dloc);
	TFMCPY3D(size);
	TFMCPY3D(dscale);
	TFMCPY3D(rot);
	TFMCPY3D(drot);
	TFMCPY4D(quat);
	TFMCPY4D(dquat);
	TFMCPY3D(rotAxis);
	TFMCPY3D(drotAxis);
	TFMCPY(rotAngle);
	TFMCPY(drotAngle);

#undef TFMCPY
#undef TFMCPY3D
#undef TFMCPY4D

}

void BKE_object_tfm_protected_restore(Object *ob,
                                      const ObjectTfmProtectedChannels *obtfm,
                                      const short protectflag)
{
	unsigned int i;

	for (i = 0; i < 3; i++) {
		if (protectflag & (OB_LOCK_LOCX << i)) {
			ob->loc[i] =  obtfm->loc[i];
			ob->dloc[i] = obtfm->dloc[i];
		}

		if (protectflag & (OB_LOCK_SCALEX << i)) {
			ob->size[i] =  obtfm->size[i];
			ob->dscale[i] = obtfm->dscale[i];
		}

		if (protectflag & (OB_LOCK_ROTX << i)) {
			ob->rot[i] =  obtfm->rot[i];
			ob->drot[i] = obtfm->drot[i];

			ob->quat[i + 1] =  obtfm->quat[i + 1];
			ob->dquat[i + 1] = obtfm->dquat[i + 1];

			ob->rotAxis[i] =  obtfm->rotAxis[i];
			ob->drotAxis[i] = obtfm->drotAxis[i];
		}
	}

	if ((protectflag & OB_LOCK_ROT4D) && (protectflag & OB_LOCK_ROTW)) {
		ob->quat[0] =  obtfm->quat[0];
		ob->dquat[0] = obtfm->dquat[0];

		ob->rotAngle =  obtfm->rotAngle;
		ob->drotAngle = obtfm->drotAngle;
	}
}

void BKE_object_to_mat3(Object *ob, float mat[3][3]) /* no parent */
{
	float smat[3][3];
	float rmat[3][3];
	/*float q1[4];*/

	/* size */
	BKE_object_scale_to_mat3(ob, smat);

	/* rot */
	BKE_object_rot_to_mat3(ob, rmat, true);
	mul_m3_m3m3(mat, rmat, smat);
}

void BKE_object_to_mat4(Object *ob, float mat[4][4])
{
	float tmat[3][3];

	BKE_object_to_mat3(ob, tmat);

	copy_m4_m3(mat, tmat);

	add_v3_v3v3(mat[3], ob->loc, ob->dloc);
}

void BKE_object_matrix_local_get(struct Object *ob, float mat[4][4])
{
	if (ob->parent) {
		float par_imat[4][4];

		BKE_object_get_parent_matrix(NULL, NULL, ob, ob->parent, par_imat);
		invert_m4(par_imat);
		mul_m4_m4m4(mat, par_imat, ob->obmat);
	}
	else {
		copy_m4_m4(mat, ob->obmat);
	}
}

/* extern */
int enable_cu_speed = 1;

/**
 * \param depsgraph: Used for dupli-frame time.
 * \return success if \a mat is set.
 */
static bool ob_parcurve(Depsgraph *depsgraph, Scene *UNUSED(scene), Object *ob, Object *par, float mat[4][4])
{
	Curve *cu = par->data;
	float vec[4], dir[3], quat[4], radius, ctime;

	/* TODO: Make sure this doesn't crash. */
#if 0
	/* only happens on reload file, but violates depsgraph still... fix! */
	if (par->curve_cache == NULL) {
		if (scene == NULL) {
			return false;
		}
		BKE_displist_make_curveTypes(depsgraph, scene, par, 0);
	}
#endif

	if (par->runtime.curve_cache->path == NULL) {
		return false;
	}

	/* catch exceptions: curve paths used as a duplicator */
	if (enable_cu_speed) {
		/* ctime is now a proper var setting of Curve which gets set by Animato like any other var that's animated,
		 * but this will only work if it actually is animated...
		 *
		 * we divide the curvetime calculated in the previous step by the length of the path, to get a time
		 * factor, which then gets clamped to lie within 0.0 - 1.0 range
		 */
		if (cu->pathlen) {
			ctime = cu->ctime / cu->pathlen;
		}
		else {
			ctime = cu->ctime;
		}

		CLAMP(ctime, 0.0f, 1.0f);
	}
	else {
		/* For dupli-frames only */
		if (depsgraph == NULL) {
			return false;
		}

		ctime = DEG_get_ctime(depsgraph);
		if (cu->pathlen) {
			ctime /= cu->pathlen;
		}

		CLAMP(ctime, 0.0f, 1.0f);
	}

	unit_m4(mat);

	/* vec: 4 items! */
	if (where_on_path(par, ctime, vec, dir, (cu->flag & CU_FOLLOW) ? quat : NULL, &radius, NULL)) {

		if (cu->flag & CU_FOLLOW) {
#if 0
			float si, q[4];
			vec_to_quat(quat, dir, ob->trackflag, ob->upflag);

			/* the tilt */
			normalize_v3(dir);
			q[0] = cosf(0.5 * vec[3]);
			si = sinf(0.5 * vec[3]);
			q[1] = -si * dir[0];
			q[2] = -si * dir[1];
			q[3] = -si * dir[2];
			mul_qt_qtqt(quat, q, quat);
#else
			quat_apply_track(quat, ob->trackflag, ob->upflag);
#endif
			normalize_qt(quat);
			quat_to_mat4(mat, quat);
		}

		if (cu->flag & CU_PATH_RADIUS) {
			float tmat[4][4], rmat[4][4];
			scale_m4_fl(tmat, radius);
			mul_m4_m4m4(rmat, tmat, mat);
			copy_m4_m4(mat, rmat);
		}

		copy_v3_v3(mat[3], vec);

	}

	return true;
}

static void ob_parbone(Object *ob, Object *par, float mat[4][4])
{
	bPoseChannel *pchan;
	float vec[3];

	if (par->type != OB_ARMATURE) {
		unit_m4(mat);
		return;
	}

	/* Make sure the bone is still valid */
	pchan = BKE_pose_channel_find_name(par->pose, ob->parsubstr);
	if (!pchan || !pchan->bone) {
		printf("Object %s with Bone parent: bone %s doesn't exist\n", ob->id.name + 2, ob->parsubstr);
		unit_m4(mat);
		return;
	}

	/* get bone transform */
	if (pchan->bone->flag & BONE_RELATIVE_PARENTING) {
		/* the new option uses the root - expected bahaviour, but differs from old... */
		/* XXX check on version patching? */
		copy_m4_m4(mat, pchan->chan_mat);
	}
	else {
		copy_m4_m4(mat, pchan->pose_mat);

		/* but for backwards compatibility, the child has to move to the tail */
		copy_v3_v3(vec, mat[1]);
		mul_v3_fl(vec, pchan->bone->length);
		add_v3_v3(mat[3], vec);
	}
}

static void give_parvert(Object *par, int nr, float vec[3])
{
	zero_v3(vec);

	if (par->type == OB_MESH) {
		Mesh *me = par->data;
		BMEditMesh *em = me->edit_btmesh;
		DerivedMesh *dm;

		dm = (em) ? em->derivedFinal : par->derivedFinal;

		if (dm) {
			int count = 0;
			int numVerts = dm->getNumVerts(dm);

			if (nr < numVerts) {
				bool use_special_ss_case = false;

				if (dm->type == DM_TYPE_CCGDM) {
					ModifierData *md;
					VirtualModifierData virtualModifierData;
					use_special_ss_case = true;
					for (md = modifiers_getVirtualModifierList(par, &virtualModifierData);
					     md != NULL;
					     md = md->next)
					{
						const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
						/* TODO(sergey): Check for disabled modifiers. */
						if (mti->type != eModifierTypeType_OnlyDeform && md->next != NULL) {
							use_special_ss_case = false;
							break;
						}
					}
				}

				if (!use_special_ss_case) {
					/* avoid dm->getVertDataArray() since it allocates arrays in the dm (not thread safe) */
					if (em && dm->type == DM_TYPE_EDITBMESH) {
						if (em->bm->elem_table_dirty & BM_VERT) {
#ifdef VPARENT_THREADING_HACK
							BLI_mutex_lock(&vparent_lock);
							if (em->bm->elem_table_dirty & BM_VERT) {
								BM_mesh_elem_table_ensure(em->bm, BM_VERT);
							}
							BLI_mutex_unlock(&vparent_lock);
#else
							BLI_assert(!"Not safe for threading");
							BM_mesh_elem_table_ensure(em->bm, BM_VERT);
#endif
						}
					}
				}

				if (use_special_ss_case) {
					/* Special case if the last modifier is SS and no constructive modifier are in front of it. */
					CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
					CCGVert *ccg_vert = ccgSubSurf_getVert(ccgdm->ss, SET_INT_IN_POINTER(nr));
					/* In case we deleted some verts, nr may refer to inexistent one now, see T42557. */
					if (ccg_vert) {
						float *co = ccgSubSurf_getVertData(ccgdm->ss, ccg_vert);
						add_v3_v3(vec, co);
						count++;
					}
				}
				else if (CustomData_has_layer(&dm->vertData, CD_ORIGINDEX) &&
				         !(em && dm->type == DM_TYPE_EDITBMESH))
				{
					int i;

					/* Get the average of all verts with (original index == nr). */
					for (i = 0; i < numVerts; i++) {
						const int *index = dm->getVertData(dm, i, CD_ORIGINDEX);
						if (*index == nr) {
							float co[3];
							dm->getVertCo(dm, i, co);
							add_v3_v3(vec, co);
							count++;
						}
					}
				}
				else {
					if (nr < numVerts) {
						float co[3];
						dm->getVertCo(dm, nr, co);
						add_v3_v3(vec, co);
						count++;
					}
				}
			}

			if (count == 0) {
				/* keep as 0, 0, 0 */
			}
			else if (count > 0) {
				mul_v3_fl(vec, 1.0f / count);
			}
			else {
				/* use first index if its out of range */
				dm->getVertCo(dm, 0, vec);
			}
		}
		else {
			fprintf(stderr,
			        "%s: DerivedMesh is needed to solve parenting, "
			        "object position can be wrong now\n", __func__);
		}
	}
	else if (ELEM(par->type, OB_CURVE, OB_SURF)) {
		ListBase *nurb;

		/* Unless there's some weird depsgraph failure the cache should exist. */
		BLI_assert(par->runtime.curve_cache != NULL);

		if (par->runtime.curve_cache->deformed_nurbs.first != NULL) {
			nurb = &par->runtime.curve_cache->deformed_nurbs;
		}
		else {
			Curve *cu = par->data;
			nurb = BKE_curve_nurbs_get(cu);
		}

		BKE_nurbList_index_get_co(nurb, nr, vec);
	}
	else if (par->type == OB_LATTICE) {
		Lattice *latt  = par->data;
		DispList *dl   = par->runtime.curve_cache ? BKE_displist_find(&par->runtime.curve_cache->disp, DL_VERTS) : NULL;
		float (*co)[3] = dl ? (float (*)[3])dl->verts : NULL;
		int tot;

		if (latt->editlatt) latt = latt->editlatt->latt;

		tot = latt->pntsu * latt->pntsv * latt->pntsw;

		/* ensure dl is correct size */
		BLI_assert(dl == NULL || dl->nr == tot);

		if (nr < tot) {
			if (co) {
				copy_v3_v3(vec, co[nr]);
			}
			else {
				copy_v3_v3(vec, latt->def[nr].vec);
			}
		}
	}
}

static void ob_parvert3(Object *ob, Object *par, float mat[4][4])
{

	/* in local ob space */
	if (OB_TYPE_SUPPORT_PARVERT(par->type)) {
		float cmat[3][3], v1[3], v2[3], v3[3], q[4];

		give_parvert(par, ob->par1, v1);
		give_parvert(par, ob->par2, v2);
		give_parvert(par, ob->par3, v3);

		tri_to_quat(q, v1, v2, v3);
		quat_to_mat3(cmat, q);
		copy_m4_m3(mat, cmat);

		mid_v3_v3v3v3(mat[3], v1, v2, v3);
	}
	else {
		unit_m4(mat);
	}
}


void BKE_object_get_parent_matrix(Depsgraph *depsgraph, Scene *scene, Object *ob, Object *par, float parentmat[4][4])
{
	float tmat[4][4];
	float vec[3];
	bool ok;

	switch (ob->partype & PARTYPE) {
		case PAROBJECT:
			ok = 0;
			if (par->type == OB_CURVE) {
				if ((((Curve *)par->data)->flag & CU_PATH) &&
				    (ob_parcurve(depsgraph, scene, ob, par, tmat)))
				{
					ok = 1;
				}
			}

			if (ok) mul_m4_m4m4(parentmat, par->obmat, tmat);
			else copy_m4_m4(parentmat, par->obmat);

			break;
		case PARBONE:
			ob_parbone(ob, par, tmat);
			mul_m4_m4m4(parentmat, par->obmat, tmat);
			break;

		case PARVERT1:
			unit_m4(parentmat);
			give_parvert(par, ob->par1, vec);
			mul_v3_m4v3(parentmat[3], par->obmat, vec);
			break;
		case PARVERT3:
			ob_parvert3(ob, par, tmat);

			mul_m4_m4m4(parentmat, par->obmat, tmat);
			break;

		case PARSKEL:
			copy_m4_m4(parentmat, par->obmat);
			break;
	}

}

/**
 * \param r_originmat  Optional matrix that stores the space the object is in (without its own matrix applied)
 */
static void solve_parenting(Depsgraph *depsgraph,
                            Scene *scene, Object *ob, Object *par, float obmat[4][4], float slowmat[4][4],
                            float r_originmat[3][3], const bool set_origin)
{
	float totmat[4][4];
	float tmat[4][4];
	float locmat[4][4];

	BKE_object_to_mat4(ob, locmat);

	if (ob->partype & PARSLOW) copy_m4_m4(slowmat, obmat);

	BKE_object_get_parent_matrix(depsgraph, scene, ob, par, totmat);

	/* total */
	mul_m4_m4m4(tmat, totmat, ob->parentinv);
	mul_m4_m4m4(obmat, tmat, locmat);

	if (r_originmat) {
		/* usable originmat */
		copy_m3_m4(r_originmat, tmat);
	}

	/* origin, for help line */
	if (set_origin) {
		if ((ob->partype & PARTYPE) == PARSKEL) {
			copy_v3_v3(ob->orig, par->obmat[3]);
		}
		else {
			copy_v3_v3(ob->orig, totmat[3]);
		}
	}
}

static bool where_is_object_parslow(Object *ob, float obmat[4][4], float slowmat[4][4])
{
	float *fp1, *fp2;
	float fac1, fac2;
	int a;

	/* include framerate */
	fac1 = (1.0f / (1.0f + fabsf(ob->sf)));
	if (fac1 >= 1.0f) return false;
	fac2 = 1.0f - fac1;

	fp1 = obmat[0];
	fp2 = slowmat[0];
	for (a = 0; a < 16; a++, fp1++, fp2++) {
		fp1[0] = fac1 * fp1[0] + fac2 * fp2[0];
	}

	return true;
}

/* note, scene is the active scene while actual_scene is the scene the object resides in */
void BKE_object_where_is_calc_time_ex(
        Depsgraph *depsgraph, Scene *scene, Object *ob, float ctime,
        RigidBodyWorld *rbw, float r_originmat[3][3])
{
	if (ob == NULL) return;

	/* execute drivers only, as animation has already been done */
	BKE_animsys_evaluate_animdata(depsgraph, scene, &ob->id, ob->adt, ctime, ADT_RECALC_DRIVERS);

	if (ob->parent) {
		Object *par = ob->parent;
		float slowmat[4][4];

		/* calculate parent matrix */
		solve_parenting(depsgraph, scene, ob, par, ob->obmat, slowmat, r_originmat, true);

		/* "slow parent" is definitely not threadsafe, and may also give bad results jumping around
		 * An old-fashioned hack which probably doesn't really cut it anymore
		 */
		if (ob->partype & PARSLOW) {
			if (!where_is_object_parslow(ob, ob->obmat, slowmat))
				return;
		}
	}
	else {
		BKE_object_to_mat4(ob, ob->obmat);
	}

	/* try to fall back to the scene rigid body world if none given */
	rbw = rbw ? rbw : scene->rigidbody_world;
	/* read values pushed into RBO from sim/cache... */
	BKE_rigidbody_sync_transforms(rbw, ob, ctime);

	/* solve constraints */
	if (ob->constraints.first && !(ob->transflag & OB_NO_CONSTRAINTS)) {
		bConstraintOb *cob;
		cob = BKE_constraints_make_evalob(depsgraph, scene, ob, NULL, CONSTRAINT_OBTYPE_OBJECT);
		BKE_constraints_solve(depsgraph, &ob->constraints, cob, ctime);
		BKE_constraints_clear_evalob(cob);
	}

	/* set negative scale flag in object */
	if (is_negative_m4(ob->obmat)) ob->transflag |= OB_NEG_SCALE;
	else ob->transflag &= ~OB_NEG_SCALE;
}

void BKE_object_where_is_calc_time(Depsgraph *depsgraph, Scene *scene, Object *ob, float ctime)
{
	BKE_object_where_is_calc_time_ex(depsgraph, scene, ob, ctime, NULL, NULL);
}

/* get object transformation matrix without recalculating dependencies and
 * constraints -- assume dependencies are already solved by depsgraph.
 * no changes to object and it's parent would be done.
 * used for bundles orientation in 3d space relative to parented blender camera */
void BKE_object_where_is_calc_mat4(Depsgraph *depsgraph, Scene *scene, Object *ob, float obmat[4][4])
{

	if (ob->parent) {
		float slowmat[4][4];

		Object *par = ob->parent;

		solve_parenting(depsgraph, scene, ob, par, obmat, slowmat, NULL, false);

		if (ob->partype & PARSLOW)
			where_is_object_parslow(ob, obmat, slowmat);
	}
	else {
		BKE_object_to_mat4(ob, obmat);
	}
}

void BKE_object_where_is_calc_ex(Depsgraph *depsgraph, Scene *scene, RigidBodyWorld *rbw, Object *ob, float r_originmat[3][3])
{
	BKE_object_where_is_calc_time_ex(depsgraph, scene, ob, DEG_get_ctime(depsgraph), rbw, r_originmat);
}
void BKE_object_where_is_calc(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
	BKE_object_where_is_calc_time_ex(depsgraph, scene, ob, DEG_get_ctime(depsgraph), NULL, NULL);
}

/**
 * For calculation of the inverse parent transform, only used for editor.
 *
 * It assumes the object parent is already in the depsgraph.
 * Otherwise, after changing ob->parent you need to call:
 *  DEG_relations_tag_update(bmain);
 *  BKE_scene_graph_update_tagged(depsgraph, bmain);
 */
void BKE_object_workob_calc_parent(Depsgraph *depsgraph, Scene *scene, Object *ob, Object *workob)
{
	Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
	BKE_object_workob_clear(workob);

	unit_m4(workob->obmat);
	unit_m4(workob->parentinv);
	unit_m4(workob->constinv);

	/* Since this is used while calculating parenting, at this moment ob_eval->parent is still NULL. */
	workob->parent = DEG_get_evaluated_object(depsgraph, ob->parent);

	workob->trackflag = ob_eval->trackflag;
	workob->upflag = ob_eval->upflag;

	workob->partype = ob_eval->partype;
	workob->par1 = ob_eval->par1;
	workob->par2 = ob_eval->par2;
	workob->par3 = ob_eval->par3;

	workob->constraints = ob_eval->constraints;

	BLI_strncpy(workob->parsubstr, ob_eval->parsubstr, sizeof(workob->parsubstr));

	BKE_object_where_is_calc(depsgraph, scene, workob);
}

/**
 * Applies the global transformation \a mat to the \a ob using a relative parent space if supplied.
 *
 * \param mat the global transformation mat that the object should be set object to.
 * \param parent the parent space in which this object will be set relative to (should probably always be parent_eval).
 * \param use_compat true to ensure that rotations are set using the min difference between the old and new orientation.
 */
void BKE_object_apply_mat4_ex(Object *ob, float mat[4][4], Object *parent, float parentinv[4][4], const bool use_compat)
{
	/* see BKE_pchan_apply_mat4() for the equivalent 'pchan' function */

	float rot[3][3];

	if (parent != NULL) {
		float rmat[4][4], diff_mat[4][4], imat[4][4], parent_mat[4][4];

		BKE_object_get_parent_matrix(NULL, NULL, ob, parent, parent_mat);

		mul_m4_m4m4(diff_mat, parent_mat, parentinv);
		invert_m4_m4(imat, diff_mat);
		mul_m4_m4m4(rmat, imat, mat); /* get the parent relative matrix */

		/* same as below, use rmat rather than mat */
		mat4_to_loc_rot_size(ob->loc, rot, ob->size, rmat);
	}
	else {
		mat4_to_loc_rot_size(ob->loc, rot, ob->size, mat);
	}

	BKE_object_mat3_to_rot(ob, rot, use_compat);

	sub_v3_v3(ob->loc, ob->dloc);

	if (ob->dscale[0] != 0.0f) ob->size[0] /= ob->dscale[0];
	if (ob->dscale[1] != 0.0f) ob->size[1] /= ob->dscale[1];
	if (ob->dscale[2] != 0.0f) ob->size[2] /= ob->dscale[2];

	/* BKE_object_mat3_to_rot handles delta rotations */
}

/* XXX: should be removed after COW operators port to use BKE_object_apply_mat4_ex directly */
void BKE_object_apply_mat4(Object *ob, float mat[4][4], const bool use_compat, const bool use_parent)
{
	BKE_object_apply_mat4_ex(ob, mat, use_parent ? ob->parent : NULL, ob->parentinv, use_compat);
}

BoundBox *BKE_boundbox_alloc_unit(void)
{
	BoundBox *bb;
	const float min[3] = {-1.0f, -1.0f, -1.0f}, max[3] = {1.0f, 1.0f, 1.0f};

	bb = MEM_callocN(sizeof(BoundBox), "OB-BoundBox");
	BKE_boundbox_init_from_minmax(bb, min, max);

	return bb;
}

void BKE_boundbox_init_from_minmax(BoundBox *bb, const float min[3], const float max[3])
{
	bb->vec[0][0] = bb->vec[1][0] = bb->vec[2][0] = bb->vec[3][0] = min[0];
	bb->vec[4][0] = bb->vec[5][0] = bb->vec[6][0] = bb->vec[7][0] = max[0];

	bb->vec[0][1] = bb->vec[1][1] = bb->vec[4][1] = bb->vec[5][1] = min[1];
	bb->vec[2][1] = bb->vec[3][1] = bb->vec[6][1] = bb->vec[7][1] = max[1];

	bb->vec[0][2] = bb->vec[3][2] = bb->vec[4][2] = bb->vec[7][2] = min[2];
	bb->vec[1][2] = bb->vec[2][2] = bb->vec[5][2] = bb->vec[6][2] = max[2];
}

void BKE_boundbox_calc_center_aabb(const BoundBox *bb, float r_cent[3])
{
	r_cent[0] = 0.5f * (bb->vec[0][0] + bb->vec[4][0]);
	r_cent[1] = 0.5f * (bb->vec[0][1] + bb->vec[2][1]);
	r_cent[2] = 0.5f * (bb->vec[0][2] + bb->vec[1][2]);
}

void BKE_boundbox_calc_size_aabb(const BoundBox *bb, float r_size[3])
{
	r_size[0] = 0.5f * fabsf(bb->vec[0][0] - bb->vec[4][0]);
	r_size[1] = 0.5f * fabsf(bb->vec[0][1] - bb->vec[2][1]);
	r_size[2] = 0.5f * fabsf(bb->vec[0][2] - bb->vec[1][2]);
}

void BKE_boundbox_minmax(const BoundBox *bb, float obmat[4][4], float r_min[3], float r_max[3])
{
	int i;
	for (i = 0; i < 8; i++) {
		float vec[3];
		mul_v3_m4v3(vec, obmat, bb->vec[i]);
		minmax_v3v3_v3(r_min, r_max, vec);
	}
}

BoundBox *BKE_object_boundbox_get(Object *ob)
{
	BoundBox *bb = NULL;

	if (ob->type == OB_MESH) {
		bb = BKE_mesh_boundbox_get(ob);
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		bb = BKE_curve_boundbox_get(ob);
	}
	else if (ob->type == OB_MBALL) {
		bb = ob->bb;
	}
	else if (ob->type == OB_LATTICE) {
		bb = BKE_lattice_boundbox_get(ob);
	}
	else if (ob->type == OB_ARMATURE) {
		bb = BKE_armature_boundbox_get(ob);
	}
	return bb;
}

/* used to temporally disable/enable boundbox */
void BKE_object_boundbox_flag(Object *ob, int flag, const bool set)
{
	BoundBox *bb = BKE_object_boundbox_get(ob);
	if (bb) {
		if (set) bb->flag |= flag;
		else bb->flag &= ~flag;
	}
}

void BKE_object_dimensions_get(Object *ob, float vec[3])
{
	BoundBox *bb = NULL;

	bb = BKE_object_boundbox_get(ob);
	if (bb) {
		float scale[3];

		mat4_to_size(scale, ob->obmat);

		vec[0] = fabsf(scale[0]) * (bb->vec[4][0] - bb->vec[0][0]);
		vec[1] = fabsf(scale[1]) * (bb->vec[2][1] - bb->vec[0][1]);
		vec[2] = fabsf(scale[2]) * (bb->vec[1][2] - bb->vec[0][2]);
	}
	else {
		zero_v3(vec);
	}
}

void BKE_object_dimensions_set(Object *ob, const float value[3])
{
	BoundBox *bb = NULL;

	bb = BKE_object_boundbox_get(ob);
	if (bb) {
		float scale[3], len[3];

		mat4_to_size(scale, ob->obmat);

		len[0] = bb->vec[4][0] - bb->vec[0][0];
		len[1] = bb->vec[2][1] - bb->vec[0][1];
		len[2] = bb->vec[1][2] - bb->vec[0][2];

		if (len[0] > 0.f) ob->size[0] = value[0] / len[0];
		if (len[1] > 0.f) ob->size[1] = value[1] / len[1];
		if (len[2] > 0.f) ob->size[2] = value[2] / len[2];
	}
}

void BKE_object_minmax(Object *ob, float min_r[3], float max_r[3], const bool use_hidden)
{
	BoundBox bb;
	float vec[3];
	bool changed = false;

	switch (ob->type) {
		case OB_CURVE:
		case OB_FONT:
		case OB_SURF:
		{
			bb = *BKE_curve_boundbox_get(ob);
			BKE_boundbox_minmax(&bb, ob->obmat, min_r, max_r);
			changed = true;
			break;
		}
		case OB_LATTICE:
		{
			Lattice *lt = ob->data;
			BPoint *bp = lt->def;
			int u, v, w;

			for (w = 0; w < lt->pntsw; w++) {
				for (v = 0; v < lt->pntsv; v++) {
					for (u = 0; u < lt->pntsu; u++, bp++) {
						mul_v3_m4v3(vec, ob->obmat, bp->vec);
						minmax_v3v3_v3(min_r, max_r, vec);
					}
				}
			}
			changed = true;
			break;
		}
		case OB_ARMATURE:
		{
			changed = BKE_pose_minmax(ob, min_r, max_r, use_hidden, false);
			break;
		}
		case OB_MESH:
		{
			Mesh *me = BKE_mesh_from_object(ob);

			if (me) {
				bb = *BKE_mesh_boundbox_get(ob);
				BKE_boundbox_minmax(&bb, ob->obmat, min_r, max_r);
				changed = true;
			}
			break;
		}
		case OB_MBALL:
		{
			float ob_min[3], ob_max[3];

			changed = BKE_mball_minmax_ex(ob->data, ob_min, ob_max, ob->obmat, 0);
			if (changed) {
				minmax_v3v3_v3(min_r, max_r, ob_min);
				minmax_v3v3_v3(min_r, max_r, ob_max);
			}
			break;
		}
	}

	if (changed == false) {
		float size[3];

		copy_v3_v3(size, ob->size);
		if ((ob->type == OB_EMPTY) || (ob->type == OB_GPENCIL)) {
			mul_v3_fl(size, ob->empty_drawsize);
		}

		minmax_v3v3_v3(min_r, max_r, ob->obmat[3]);

		copy_v3_v3(vec, ob->obmat[3]);
		add_v3_v3(vec, size);
		minmax_v3v3_v3(min_r, max_r, vec);

		copy_v3_v3(vec, ob->obmat[3]);
		sub_v3_v3(vec, size);
		minmax_v3v3_v3(min_r, max_r, vec);
	}
}

void BKE_object_empty_draw_type_set(Object *ob, const int value)
{
	ob->empty_drawtype = value;

	if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE) {
		if (!ob->iuser) {
			ob->iuser = MEM_callocN(sizeof(ImageUser), "image user");
			ob->iuser->ok = 1;
			ob->iuser->flag |= IMA_ANIM_ALWAYS;
			ob->iuser->frames = 100;
			ob->iuser->sfra = 1;
		}
	}
	else {
		if (ob->iuser) {
			MEM_freeN(ob->iuser);
			ob->iuser = NULL;
		}
	}
}

bool BKE_object_minmax_dupli(Depsgraph *depsgraph, Scene *scene, Object *ob, float r_min[3], float r_max[3], const bool use_hidden)
{
	bool ok = false;
	if ((ob->transflag & OB_DUPLI) == 0) {
		return ok;
	}
	else {
		ListBase *lb;
		DupliObject *dob;
		lb = object_duplilist(depsgraph, scene, ob);
		for (dob = lb->first; dob; dob = dob->next) {
			if ((use_hidden == false) && (dob->no_draw != 0)) {
				/* pass */
			}
			else {
				BoundBox *bb = BKE_object_boundbox_get(dob->ob);

				if (bb) {
					int i;
					for (i = 0; i < 8; i++) {
						float vec[3];
						mul_v3_m4v3(vec, dob->mat, bb->vec[i]);
						minmax_v3v3_v3(r_min, r_max, vec);
					}

					ok = true;
				}
			}
		}
		free_object_duplilist(lb);  /* does restore */
	}

	return ok;
}

void BKE_object_foreach_display_point(
        Object *ob, float obmat[4][4],
        void (*func_cb)(const float[3], void *), void *user_data)
{
	float co[3];

	if (ob->runtime.mesh_eval) {
		const Mesh *me = ob->runtime.mesh_eval;
		const MVert *mv = me->mvert;
		const int totvert = me->totvert;
		for (int i = 0; i < totvert; i++, mv++) {
			mul_v3_m4v3(co, obmat, mv->co);
			func_cb(co, user_data);
		}
	}
	else if (ob->runtime.curve_cache && ob->runtime.curve_cache->disp.first) {
		DispList *dl;

		for (dl = ob->runtime.curve_cache->disp.first; dl; dl = dl->next) {
			const float *v3 = dl->verts;
			int totvert = dl->nr;
			int i;

			for (i = 0; i < totvert; i++, v3 += 3) {
				mul_v3_m4v3(co, obmat, v3);
				func_cb(co, user_data);
			}
		}
	}
}

void BKE_scene_foreach_display_point(
        Depsgraph *depsgraph,
        void (*func_cb)(const float[3], void *), void *user_data)
{
	DEG_OBJECT_ITER_BEGIN(
	        depsgraph, ob,
	        DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
	        DEG_ITER_OBJECT_FLAG_VISIBLE |
	        DEG_ITER_OBJECT_FLAG_DUPLI)
	{
		if ((ob->base_flag & BASE_SELECTED) != 0) {
			BKE_object_foreach_display_point(ob, ob->obmat, func_cb, user_data);
		}
	}
	DEG_OBJECT_ITER_END;
}

/* copied from DNA_object_types.h */
typedef struct ObTfmBack {
	float loc[3], dloc[3], orig[3];
	float size[3], dscale[3];   /* scale and delta scale */
	float rot[3], drot[3];      /* euler rotation */
	float quat[4], dquat[4];    /* quaternion rotation */
	float rotAxis[3], drotAxis[3];  /* axis angle rotation - axis part */
	float rotAngle, drotAngle;  /* axis angle rotation - angle part */
	float obmat[4][4];      /* final worldspace matrix with constraints & animsys applied */
	float parentinv[4][4]; /* inverse result of parent, so that object doesn't 'stick' to parent */
	float constinv[4][4]; /* inverse result of constraints. doesn't include effect of parent or object local transform */
	float imat[4][4];   /* inverse matrix of 'obmat' for during render, temporally: ipokeys of transform  */
} ObTfmBack;

void *BKE_object_tfm_backup(Object *ob)
{
	ObTfmBack *obtfm = MEM_mallocN(sizeof(ObTfmBack), "ObTfmBack");
	copy_v3_v3(obtfm->loc, ob->loc);
	copy_v3_v3(obtfm->dloc, ob->dloc);
	copy_v3_v3(obtfm->orig, ob->orig);
	copy_v3_v3(obtfm->size, ob->size);
	copy_v3_v3(obtfm->dscale, ob->dscale);
	copy_v3_v3(obtfm->rot, ob->rot);
	copy_v3_v3(obtfm->drot, ob->drot);
	copy_qt_qt(obtfm->quat, ob->quat);
	copy_qt_qt(obtfm->dquat, ob->dquat);
	copy_v3_v3(obtfm->rotAxis, ob->rotAxis);
	copy_v3_v3(obtfm->drotAxis, ob->drotAxis);
	obtfm->rotAngle = ob->rotAngle;
	obtfm->drotAngle = ob->drotAngle;
	copy_m4_m4(obtfm->obmat, ob->obmat);
	copy_m4_m4(obtfm->parentinv, ob->parentinv);
	copy_m4_m4(obtfm->constinv, ob->constinv);
	copy_m4_m4(obtfm->imat, ob->imat);

	return (void *)obtfm;
}

void BKE_object_tfm_restore(Object *ob, void *obtfm_pt)
{
	ObTfmBack *obtfm = (ObTfmBack *)obtfm_pt;
	copy_v3_v3(ob->loc, obtfm->loc);
	copy_v3_v3(ob->dloc, obtfm->dloc);
	copy_v3_v3(ob->orig, obtfm->orig);
	copy_v3_v3(ob->size, obtfm->size);
	copy_v3_v3(ob->dscale, obtfm->dscale);
	copy_v3_v3(ob->rot, obtfm->rot);
	copy_v3_v3(ob->drot, obtfm->drot);
	copy_qt_qt(ob->quat, obtfm->quat);
	copy_qt_qt(ob->dquat, obtfm->dquat);
	copy_v3_v3(ob->rotAxis, obtfm->rotAxis);
	copy_v3_v3(ob->drotAxis, obtfm->drotAxis);
	ob->rotAngle = obtfm->rotAngle;
	ob->drotAngle = obtfm->drotAngle;
	copy_m4_m4(ob->obmat, obtfm->obmat);
	copy_m4_m4(ob->parentinv, obtfm->parentinv);
	copy_m4_m4(ob->constinv, obtfm->constinv);
	copy_m4_m4(ob->imat, obtfm->imat);
}

bool BKE_object_parent_loop_check(const Object *par, const Object *ob)
{
	/* test if 'ob' is a parent somewhere in par's parents */
	if (par == NULL) return false;
	if (ob == par) return true;
	return BKE_object_parent_loop_check(par->parent, ob);
}

static void object_handle_update_proxy(Depsgraph *depsgraph,
                                       Scene *scene,
                                       Object *object,
                                       const bool do_proxy_update)
{
	/* The case when this is a collection proxy, object_update is called in collection.c */
	if (object->proxy == NULL) {
		return;
	}
	/* set pointer in library proxy target, for copying, but restore it */
	object->proxy->proxy_from = object;
	// printf("set proxy pointer for later collection stuff %s\n", ob->id.name);

	/* the no-group proxy case, we call update */
	if (object->proxy_group == NULL) {
		if (do_proxy_update) {
			// printf("call update, lib ob %s proxy %s\n", ob->proxy->id.name, ob->id.name);
			BKE_object_handle_update(depsgraph, scene, object->proxy);
		}
	}
}

/* proxy rule: lib_object->proxy_from == the one we borrow from, only set temporal and cleared here */
/*           local_object->proxy      == pointer to library object, saved in files and read */

/* function below is polluted with proxy exceptions, cleanup will follow! */

/* the main object update call, for object matrix, constraints, keys and displist (modifiers) */
/* requires flags to be set! */
/* Ideally we shouldn't have to pass the rigid body world, but need bigger restructuring to avoid id */
void BKE_object_handle_update_ex(Depsgraph *depsgraph,
                                 Scene *scene, Object *ob,
                                 RigidBodyWorld *rbw,
                                 const bool do_proxy_update)
{
	const ID *object_data = ob->data;
	const bool recalc_object = (ob->id.recalc & ID_RECALC) != 0;
	const bool recalc_data =
	        (object_data != NULL) ? ((object_data->recalc & ID_RECALC_ALL) != 0)
	                              : 0;
	if (!recalc_object && ! recalc_data) {
		object_handle_update_proxy(depsgraph, scene, ob, do_proxy_update);
		return;
	}
	/* Speed optimization for animation lookups. */
	if (ob->pose != NULL) {
		BKE_pose_channels_hash_make(ob->pose);
		if (ob->pose->flag & POSE_CONSTRAINTS_NEED_UPDATE_FLAGS) {
			BKE_pose_update_constraint_flags(ob->pose);
		}
	}
	if (recalc_data) {
		if (ob->type == OB_ARMATURE) {
			/* this happens for reading old files and to match library armatures
			 * with poses we do it ahead of BKE_object_where_is_calc to ensure animation
			 * is evaluated on the rebuilt pose, otherwise we get incorrect poses
			 * on file load */
			if (ob->pose == NULL || (ob->pose->flag & POSE_RECALC)) {
				/* No need to pass bmain here, we assume we do not need to rebuild DEG from here... */
				BKE_pose_rebuild(NULL, ob, ob->data);
			}
		}
	}
	/* XXX new animsys warning: depsgraph tag OB_RECALC_DATA should not skip drivers,
	 * which is only in BKE_object_where_is_calc now */
	/* XXX: should this case be OB_RECALC_OB instead? */
	if (recalc_object || recalc_data) {
		if (G.debug & G_DEBUG_DEPSGRAPH_EVAL) {
			printf("recalcob %s\n", ob->id.name + 2);
		}
		/* Handle proxy copy for target. */
		if (!BKE_object_eval_proxy_copy(depsgraph, ob)) {
			BKE_object_where_is_calc_ex(depsgraph, scene, rbw, ob, NULL);
		}
	}

	if (recalc_data) {
		BKE_object_handle_data_update(depsgraph, scene, ob);
	}

	ob->id.recalc &= ID_RECALC_ALL;

	object_handle_update_proxy(depsgraph, scene, ob, do_proxy_update);
}

/* WARNING: "scene" here may not be the scene object actually resides in.
 * When dealing with background-sets, "scene" is actually the active scene.
 * e.g. "scene" <-- set 1 <-- set 2 ("ob" lives here) <-- set 3 <-- ... <-- set n
 * rigid bodies depend on their world so use BKE_object_handle_update_ex() to also pass along the corrent rigid body world
 */
void BKE_object_handle_update(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
	BKE_object_handle_update_ex(depsgraph, scene, ob, NULL, true);
}

void BKE_object_sculpt_modifiers_changed(Object *ob)
{
	SculptSession *ss = ob->sculpt;

	if (ss && ss->building_vp_handle == false) {
		if (!ss->cache) {
			/* we free pbvh on changes, except during sculpt since it can't deal with
			 * changing PVBH node organization, we hope topology does not change in
			 * the meantime .. weak */
			if (ss->pbvh) {
				BKE_pbvh_free(ss->pbvh);
				ss->pbvh = NULL;
			}

			BKE_sculptsession_free_deformMats(ob->sculpt);

			/* In vertex/weight paint, force maps to be rebuilt. */
			BKE_sculptsession_free_vwpaint_data(ob->sculpt);
		}
		else {
			PBVHNode **nodes;
			int n, totnode;

			BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

			for (n = 0; n < totnode; n++)
				BKE_pbvh_node_mark_update(nodes[n]);

			MEM_freeN(nodes);
		}
	}
}

int BKE_object_obdata_texspace_get(Object *ob, short **r_texflag, float **r_loc, float **r_size, float **r_rot)
{

	if (ob->data == NULL)
		return 0;

	switch (GS(((ID *)ob->data)->name)) {
		case ID_ME:
		{
			BKE_mesh_texspace_get_reference((Mesh *)ob->data, r_texflag, r_loc, r_rot, r_size);
			break;
		}
		case ID_CU:
		{
			Curve *cu = ob->data;
			if (cu->bb == NULL || (cu->bb->flag & BOUNDBOX_DIRTY)) {
				BKE_curve_texspace_calc(cu);
			}
			if (r_texflag) *r_texflag = &cu->texflag;
			if (r_loc) *r_loc = cu->loc;
			if (r_size) *r_size = cu->size;
			if (r_rot) *r_rot = cu->rot;
			break;
		}
		case ID_MB:
		{
			MetaBall *mb = ob->data;
			if (r_texflag) *r_texflag = &mb->texflag;
			if (r_loc) *r_loc = mb->loc;
			if (r_size) *r_size = mb->size;
			if (r_rot) *r_rot = mb->rot;
			break;
		}
		default:
			return 0;
	}
	return 1;
}

/** Get evaluated mesh for given (main, original) object and depsgraph. */
Mesh *BKE_object_get_evaluated_mesh(const Depsgraph *depsgraph, Object *ob)
{
	Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
	return ob_eval->runtime.mesh_eval;
}

/* Get object's mesh with all modifiers applied. */
Mesh *BKE_object_get_final_mesh(Object *object)
{
	if (object->runtime.mesh_eval != NULL) {
		BLI_assert((object->id.tag & LIB_TAG_COPIED_ON_WRITE) != 0);
		BLI_assert(object->runtime.mesh_eval == object->data);
		BLI_assert((object->runtime.mesh_eval->id.tag & LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT) != 0);
		return object->runtime.mesh_eval;
	}
	/* Wasn't evaluated yet. */
	return object->data;
}

/* Get mesh which is not affected by modifiers:
 * - For original objects it will be same as object->data, and it is a mesh
 *   which is in the corresponding bmain.
 * - For copied-on-write objects it will give pointer to a copied-on-write
 *   mesh which corresponds to original object's mesh.
 */
Mesh *BKE_object_get_pre_modified_mesh(Object *object)
{
	if (object->runtime.mesh_orig != NULL) {
		BLI_assert(object->id.tag & LIB_TAG_COPIED_ON_WRITE);
		BLI_assert(object->id.orig_id != NULL);
		BLI_assert(object->runtime.mesh_orig->id.orig_id == ((Object *)object->id.orig_id)->data);
		Mesh *result = object->runtime.mesh_orig;
		BLI_assert((result->id.tag & LIB_TAG_COPIED_ON_WRITE) != 0);
		BLI_assert((result->id.tag & LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT) == 0);
		return result;
	}
	BLI_assert((object->id.tag & LIB_TAG_COPIED_ON_WRITE) == 0);
	return object->data;
}

/* Get a mesh which corresponds to very very original mesh from bmain.
 * - For original objects it will be object->data.
 * - For evaluated objects it will be same mesh as corresponding original
 *   object uses as data.
 */
Mesh *BKE_object_get_original_mesh(Object *object)
{
	Mesh *result = NULL;
	if (object->id.orig_id == NULL) {
		BLI_assert((object->id.tag & LIB_TAG_COPIED_ON_WRITE) == 0);
		result = object->data;
	}
	else {
		BLI_assert((object->id.tag & LIB_TAG_COPIED_ON_WRITE) != 0);
		result = ((Object *)object->id.orig_id)->data;
	}
	BLI_assert(result != NULL);
	BLI_assert((result->id.tag & (LIB_TAG_COPIED_ON_WRITE | LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT)) == 0);
	return result;
}

static int pc_cmp(const void *a, const void *b)
{
	const LinkData *ad = a, *bd = b;
	if (GET_INT_FROM_POINTER(ad->data) > GET_INT_FROM_POINTER(bd->data))
		return 1;
	else return 0;
}

int BKE_object_insert_ptcache(Object *ob)
{
	LinkData *link = NULL;
	int i = 0;

	BLI_listbase_sort(&ob->pc_ids, pc_cmp);

	for (link = ob->pc_ids.first, i = 0; link; link = link->next, i++) {
		int index = GET_INT_FROM_POINTER(link->data);

		if (i < index)
			break;
	}

	link = MEM_callocN(sizeof(LinkData), "PCLink");
	link->data = SET_INT_IN_POINTER(i);
	BLI_addtail(&ob->pc_ids, link);

	return i;
}

static int pc_findindex(ListBase *listbase, int index)
{
	LinkData *link = NULL;
	int number = 0;

	if (listbase == NULL) return -1;

	link = listbase->first;
	while (link) {
		if (GET_INT_FROM_POINTER(link->data) == index)
			return number;

		number++;
		link = link->next;
	}

	return -1;
}

void BKE_object_delete_ptcache(Object *ob, int index)
{
	int list_index = pc_findindex(&ob->pc_ids, index);
	LinkData *link = BLI_findlink(&ob->pc_ids, list_index);
	BLI_freelinkN(&ob->pc_ids, link);
}

/* shape key utility function */

/************************* Mesh ************************/
static KeyBlock *insert_meshkey(Main *bmain, Object *ob, const char *name, const bool from_mix)
{
	Mesh *me = ob->data;
	Key *key = me->key;
	KeyBlock *kb;
	int newkey = 0;

	if (key == NULL) {
		key = me->key = BKE_key_add(bmain, (ID *)me);
		key->type = KEY_RELATIVE;
		newkey = 1;
	}

	if (newkey || from_mix == false) {
		/* create from mesh */
		kb = BKE_keyblock_add_ctime(key, name, false);
		BKE_keyblock_convert_from_mesh(me, key, kb);
	}
	else {
		/* copy from current values */
		int totelem;
		float *data = BKE_key_evaluate_object(ob, &totelem);

		/* create new block with prepared data */
		kb = BKE_keyblock_add_ctime(key, name, false);
		kb->data = data;
		kb->totelem = totelem;
	}

	return kb;
}
/************************* Lattice ************************/
static KeyBlock *insert_lattkey(Main *bmain, Object *ob, const char *name, const bool from_mix)
{
	Lattice *lt = ob->data;
	Key *key = lt->key;
	KeyBlock *kb;
	int newkey = 0;

	if (key == NULL) {
		key = lt->key = BKE_key_add(bmain, (ID *)lt);
		key->type = KEY_RELATIVE;
		newkey = 1;
	}

	if (newkey || from_mix == false) {
		kb = BKE_keyblock_add_ctime(key, name, false);
		if (!newkey) {
			KeyBlock *basekb = (KeyBlock *)key->block.first;
			kb->data = MEM_dupallocN(basekb->data);
			kb->totelem = basekb->totelem;
		}
		else {
			BKE_keyblock_convert_from_lattice(lt, kb);
		}
	}
	else {
		/* copy from current values */
		int totelem;
		float *data = BKE_key_evaluate_object(ob, &totelem);

		/* create new block with prepared data */
		kb = BKE_keyblock_add_ctime(key, name, false);
		kb->totelem = totelem;
		kb->data = data;
	}

	return kb;
}
/************************* Curve ************************/
static KeyBlock *insert_curvekey(Main *bmain, Object *ob, const char *name, const bool from_mix)
{
	Curve *cu = ob->data;
	Key *key = cu->key;
	KeyBlock *kb;
	ListBase *lb = BKE_curve_nurbs_get(cu);
	int newkey = 0;

	if (key == NULL) {
		key = cu->key = BKE_key_add(bmain, (ID *)cu);
		key->type = KEY_RELATIVE;
		newkey = 1;
	}

	if (newkey || from_mix == false) {
		/* create from curve */
		kb = BKE_keyblock_add_ctime(key, name, false);
		if (!newkey) {
			KeyBlock *basekb = (KeyBlock *)key->block.first;
			kb->data = MEM_dupallocN(basekb->data);
			kb->totelem = basekb->totelem;
		}
		else {
			BKE_keyblock_convert_from_curve(cu, kb, lb);
		}
	}
	else {
		/* copy from current values */
		int totelem;
		float *data = BKE_key_evaluate_object(ob, &totelem);

		/* create new block with prepared data */
		kb = BKE_keyblock_add_ctime(key, name, false);
		kb->totelem = totelem;
		kb->data = data;
	}

	return kb;
}

KeyBlock *BKE_object_shapekey_insert(Main *bmain, Object *ob, const char *name, const bool from_mix)
{
	switch (ob->type) {
		case OB_MESH:
			return insert_meshkey(bmain, ob, name, from_mix);
		case OB_CURVE:
		case OB_SURF:
			return insert_curvekey(bmain, ob, name, from_mix);
		case OB_LATTICE:
			return insert_lattkey(bmain, ob, name, from_mix);
		default:
			return NULL;
	}

}

bool BKE_object_shapekey_free(Main *bmain, Object *ob)
{
	Key **key_p, *key;

	key_p = BKE_key_from_object_p(ob);
	if (ELEM(NULL, key_p, *key_p)) {
		return false;
	}

	key = *key_p;
	*key_p = NULL;

	BKE_libblock_free_us(bmain, key);

	return false;
}

bool BKE_object_shapekey_remove(Main *bmain, Object *ob, KeyBlock *kb)
{
	KeyBlock *rkb;
	Key *key = BKE_key_from_object(ob);
	short kb_index;

	if (key == NULL) {
		return false;
	}

	kb_index = BLI_findindex(&key->block, kb);
	BLI_assert(kb_index != -1);

	for (rkb = key->block.first; rkb; rkb = rkb->next) {
		if (rkb->relative == kb_index) {
			/* remap to the 'Basis' */
			rkb->relative = 0;
		}
		else if (rkb->relative >= kb_index) {
			/* Fix positional shift of the keys when kb is deleted from the list */
			rkb->relative -= 1;
		}
	}

	BLI_remlink(&key->block, kb);
	key->totkey--;
	if (key->refkey == kb) {
		key->refkey = key->block.first;

		if (key->refkey) {
			/* apply new basis key on original data */
			switch (ob->type) {
				case OB_MESH:
					BKE_keyblock_convert_to_mesh(key->refkey, ob->data);
					break;
				case OB_CURVE:
				case OB_SURF:
					BKE_keyblock_convert_to_curve(key->refkey, ob->data, BKE_curve_nurbs_get(ob->data));
					break;
				case OB_LATTICE:
					BKE_keyblock_convert_to_lattice(key->refkey, ob->data);
					break;
			}
		}
	}

	if (kb->data) {
		MEM_freeN(kb->data);
	}
	MEM_freeN(kb);

	if (ob->shapenr > 1) {
		ob->shapenr--;
	}

	if (key->totkey == 0) {
		BKE_object_shapekey_free(bmain, ob);
	}

	return true;
}

bool BKE_object_flag_test_recursive(const Object *ob, short flag)
{
	if (ob->flag & flag) {
		return true;
	}
	else if (ob->parent) {
		return BKE_object_flag_test_recursive(ob->parent, flag);
	}
	else {
		return false;
	}
}

bool BKE_object_is_child_recursive(const Object *ob_parent, const Object *ob_child)
{
	for (ob_child = ob_child->parent; ob_child; ob_child = ob_child->parent) {
		if (ob_child == ob_parent) {
			return true;
		}
	}
	return false;
}

/* most important if this is modified it should _always_ return True, in certain
 * cases false positives are hard to avoid (shape keys for example) */
int BKE_object_is_modified(Scene *scene, Object *ob)
{
	int flag = 0;

	if (BKE_key_from_object(ob)) {
		flag |= eModifierMode_Render | eModifierMode_Realtime;
	}
	else {
		ModifierData *md;
		VirtualModifierData virtualModifierData;
		/* cloth */
		for (md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
		     md && (flag != (eModifierMode_Render | eModifierMode_Realtime));
		     md = md->next)
		{
			if ((flag & eModifierMode_Render) == 0 && modifier_isEnabled(scene, md, eModifierMode_Render))
				flag |= eModifierMode_Render;

			if ((flag & eModifierMode_Realtime) == 0 && modifier_isEnabled(scene, md, eModifierMode_Realtime))
				flag |= eModifierMode_Realtime;
		}
	}

	return flag;
}

/* Check of objects moves in time. */
/* NOTE: This function is currently optimized for usage in combination
 * with mti->canDeform, so modifiers can quickly check if their target
 * objects moves (causing deformation motion blur) or not.
 *
 * This makes it possible to give some degree of false-positives here,
 * but it's currently an acceptable tradeoff between complexity and check
 * speed. In combination with checks of modifier stack and real life usage
 * percentage of false-positives shouldn't be that hight.
 */
static bool object_moves_in_time(Object *object)
{
	AnimData *adt = object->adt;
	if (adt != NULL) {
		/* If object has any sort of animation data assume it is moving. */
		if (adt->action != NULL ||
		    !BLI_listbase_is_empty(&adt->nla_tracks) ||
		    !BLI_listbase_is_empty(&adt->drivers) ||
		    !BLI_listbase_is_empty(&adt->overrides))
		{
			return true;
		}
	}
	if (!BLI_listbase_is_empty(&object->constraints)) {
		return true;
	}
	if (object->parent != NULL) {
		/* TODO(sergey): Do recursive check here? */
		return true;
	}
	return false;
}

static bool object_deforms_in_time(Object *object)
{
	if (BKE_key_from_object(object) != NULL) {
		return true;
	}
	if (!BLI_listbase_is_empty(&object->modifiers)) {
		return true;
	}
	return object_moves_in_time(object);
}

static bool constructive_modifier_is_deform_modified(ModifierData *md)
{
	/* TODO(sergey): Consider generalizing this a bit so all modifier logic
	 * is concentrated in MOD_{modifier}.c file,
	 */
	if (md->type == eModifierType_Array) {
		ArrayModifierData *amd = (ArrayModifierData *)md;
		/* TODO(sergey): Check if curve is deformed. */
		return (amd->start_cap != NULL && object_moves_in_time(amd->start_cap)) ||
		       (amd->end_cap != NULL && object_moves_in_time(amd->end_cap)) ||
		       (amd->curve_ob != NULL && object_moves_in_time(amd->curve_ob)) ||
		       (amd->offset_ob != NULL && object_moves_in_time(amd->offset_ob));
	}
	else if (md->type == eModifierType_Mirror) {
		MirrorModifierData *mmd = (MirrorModifierData *)md;
		return mmd->mirror_ob != NULL && object_moves_in_time(mmd->mirror_ob);
	}
	else if (md->type == eModifierType_Screw) {
		ScrewModifierData *smd = (ScrewModifierData *)md;
		return smd->ob_axis != NULL && object_moves_in_time(smd->ob_axis);
	}
	else if (md->type == eModifierType_MeshSequenceCache) {
		/* NOTE: Not ideal because it's unknown whether topology changes or not.
		 * This will be detected later, so by assuming it's only deformation
		 * going on here we allow to bake deform-only mesh to Alembic and have
		 * proper motion blur after that.
		 */
		return true;
	}
	return false;
}

static bool modifiers_has_animation_check(Object *ob)
{
	/* TODO(sergey): This is a bit code duplication with depsgraph, but
	 * would be nicer to solve this as a part of new dependency graph
	 * work, so we avoid conflicts and so.
	 */
	if (ob->adt != NULL) {
		AnimData *adt = ob->adt;
		FCurve *fcu;
		if (adt->action != NULL) {
			for (fcu = adt->action->curves.first; fcu; fcu = fcu->next) {
				if (fcu->rna_path && strstr(fcu->rna_path, "modifiers[")) {
					return true;
				}
			}
		}
		for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
			if (fcu->rna_path && strstr(fcu->rna_path, "modifiers[")) {
				return true;
			}
		}
	}
	return false;
}

/* test if object is affected by deforming modifiers (for motion blur). again
 * most important is to avoid false positives, this is to skip computations
 * and we can still if there was actual deformation afterwards */
int BKE_object_is_deform_modified(Scene *scene, Object *ob)
{
	ModifierData *md;
	VirtualModifierData virtualModifierData;
	int flag = 0;
	const bool is_modifier_animated = modifiers_has_animation_check(ob);

	if (BKE_key_from_object(ob)) {
		flag |= eModifierMode_Realtime | eModifierMode_Render;
	}

	if (ob->type == OB_CURVE) {
		Curve *cu = (Curve *)ob->data;
		if (cu->taperobj != NULL && object_deforms_in_time(cu->taperobj)) {
			flag |= eModifierMode_Realtime | eModifierMode_Render;
		}
	}

	/* cloth */
	for (md = modifiers_getVirtualModifierList(ob, &virtualModifierData);
	     md && (flag != (eModifierMode_Render | eModifierMode_Realtime));
	     md = md->next)
	{
		const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		bool can_deform = mti->type == eModifierTypeType_OnlyDeform ||
		                  is_modifier_animated;

		if (!can_deform) {
			can_deform = constructive_modifier_is_deform_modified(md);
		}

		if (can_deform) {
			if (!(flag & eModifierMode_Render) && modifier_isEnabled(scene, md, eModifierMode_Render))
				flag |= eModifierMode_Render;

			if (!(flag & eModifierMode_Realtime) && modifier_isEnabled(scene, md, eModifierMode_Realtime))
				flag |= eModifierMode_Realtime;
		}
	}

	return flag;
}

/* See if an object is using an animated modifier */
bool BKE_object_is_animated(Scene *scene, Object *ob)
{
	ModifierData *md;
	VirtualModifierData virtualModifierData;

	for (md = modifiers_getVirtualModifierList(ob, &virtualModifierData); md; md = md->next)
		if (modifier_dependsOnTime(md) &&
		    (modifier_isEnabled(scene, md, eModifierMode_Realtime) ||
		     modifier_isEnabled(scene, md, eModifierMode_Render)))
		{
			return true;
		}
	return false;
}

MovieClip *BKE_object_movieclip_get(Scene *scene, Object *ob, bool use_default)
{
	MovieClip *clip = use_default ? scene->clip : NULL;
	bConstraint *con = ob->constraints.first, *scon = NULL;

	while (con) {
		if (con->type == CONSTRAINT_TYPE_CAMERASOLVER) {
			if (scon == NULL || (scon->flag & CONSTRAINT_OFF))
				scon = con;
		}

		con = con->next;
	}

	if (scon) {
		bCameraSolverConstraint *solver = scon->data;
		if ((solver->flag & CAMERASOLVER_ACTIVECLIP) == 0)
			clip = solver->clip;
		else
			clip = scene->clip;
	}

	return clip;
}

void BKE_object_runtime_reset(Object *object)
{
	memset(&object->runtime, 0, sizeof(object->runtime));
}

/*
 * Find an associated Armature object
 */
static Object *obrel_armature_find(Object *ob)
{
	Object *ob_arm = NULL;

	if (ob->parent && ob->partype == PARSKEL && ob->parent->type == OB_ARMATURE) {
		ob_arm = ob->parent;
	}
	else {
		ModifierData *mod;
		for (mod = (ModifierData *)ob->modifiers.first; mod; mod = mod->next) {
			if (mod->type == eModifierType_Armature) {
				ob_arm = ((ArmatureModifierData *)mod)->object;
			}
		}
	}

	return ob_arm;
}

static bool obrel_list_test(Object *ob)
{
	return ob && !(ob->id.tag & LIB_TAG_DOIT);
}

static void obrel_list_add(LinkNode **links, Object *ob)
{
	BLI_linklist_prepend(links, ob);
	ob->id.tag |= LIB_TAG_DOIT;
}

/*
 * Iterates over all objects of the given scene layer.
 * Depending on the eObjectSet flag:
 * collect either OB_SET_ALL, OB_SET_VISIBLE or OB_SET_SELECTED objects.
 * If OB_SET_VISIBLE or OB_SET_SELECTED are collected,
 * then also add related objects according to the given includeFilters.
 */
LinkNode *BKE_object_relational_superset(struct ViewLayer *view_layer, eObjectSet objectSet, eObRelationTypes includeFilter)
{
	LinkNode *links = NULL;

	Base *base;

	/* Remove markers from all objects */
	for (base = view_layer->object_bases.first; base; base = base->next) {
		base->object->id.tag &= ~LIB_TAG_DOIT;
	}

	/* iterate over all selected and visible objects */
	for (base = view_layer->object_bases.first; base; base = base->next) {
		if (objectSet == OB_SET_ALL) {
			/* as we get all anyways just add it */
			Object *ob = base->object;
			obrel_list_add(&links, ob);
		}
		else {
			if ((objectSet == OB_SET_SELECTED && TESTBASELIB_BGMODE(base)) ||
			    (objectSet == OB_SET_VISIBLE  && BASE_EDITABLE_BGMODE(base)))
			{
				Object *ob = base->object;

				if (obrel_list_test(ob))
					obrel_list_add(&links, ob);

				/* parent relationship */
				if (includeFilter & (OB_REL_PARENT | OB_REL_PARENT_RECURSIVE)) {
					Object *parent = ob->parent;
					if (obrel_list_test(parent)) {

						obrel_list_add(&links, parent);

						/* recursive parent relationship */
						if (includeFilter & OB_REL_PARENT_RECURSIVE) {
							parent = parent->parent;
							while (obrel_list_test(parent)) {

								obrel_list_add(&links, parent);
								parent = parent->parent;
							}
						}
					}
				}

				/* child relationship */
				if (includeFilter & (OB_REL_CHILDREN | OB_REL_CHILDREN_RECURSIVE)) {
					Base *local_base;
					for (local_base = view_layer->object_bases.first; local_base; local_base = local_base->next) {
						if (BASE_EDITABLE_BGMODE(local_base)) {

							Object *child = local_base->object;
							if (obrel_list_test(child)) {
								if ((includeFilter & OB_REL_CHILDREN_RECURSIVE && BKE_object_is_child_recursive(ob, child)) ||
								    (includeFilter & OB_REL_CHILDREN && child->parent && child->parent == ob))
								{
									obrel_list_add(&links, child);
								}
							}
						}
					}
				}


				/* include related armatures */
				if (includeFilter & OB_REL_MOD_ARMATURE) {
					Object *arm = obrel_armature_find(ob);
					if (obrel_list_test(arm)) {
						obrel_list_add(&links, arm);
					}
				}

			}
		}
	}

	return links;
}

/**
 * return all groups this object is apart of, caller must free.
 */
struct LinkNode *BKE_object_groups(Main *bmain, Object *ob)
{
	LinkNode *collection_linknode = NULL;
	Collection *collection = NULL;
	while ((collection = BKE_collection_object_find(bmain, collection, ob))) {
		BLI_linklist_prepend(&collection_linknode, collection);
	}

	return collection_linknode;
}

void BKE_object_groups_clear(Main *bmain, Object *ob)
{
	Collection *collection = NULL;
	while ((collection = BKE_collection_object_find(bmain, collection, ob))) {
		BKE_collection_object_remove(bmain, collection, ob, false);
		DEG_id_tag_update(&collection->id, DEG_TAG_COPY_ON_WRITE);
	}
}

/**
 * Return a KDTree from the deformed object (in worldspace)
 *
 * \note Only mesh objects currently support deforming, others are TODO.
 *
 * \param ob
 * \param r_tot
 * \return The kdtree or NULL if it can't be created.
 */
KDTree *BKE_object_as_kdtree(Object *ob, int *r_tot)
{
	KDTree *tree = NULL;
	unsigned int tot = 0;

	switch (ob->type) {
		case OB_MESH:
		{
			Mesh *me = ob->data;
			unsigned int i;

			DerivedMesh *dm = ob->derivedDeform ? ob->derivedDeform : ob->derivedFinal;
			const int *index;

			if (dm && (index = CustomData_get_layer(&dm->vertData, CD_ORIGINDEX))) {
				MVert *mvert = dm->getVertArray(dm);
				unsigned int totvert = dm->getNumVerts(dm);

				/* tree over-allocs in case where some verts have ORIGINDEX_NONE */
				tot = 0;
				tree = BLI_kdtree_new(totvert);

				/* we don't how how many verts from the DM we can use */
				for (i = 0; i < totvert; i++) {
					if (index[i] != ORIGINDEX_NONE) {
						float co[3];
						mul_v3_m4v3(co, ob->obmat, mvert[i].co);
						BLI_kdtree_insert(tree, index[i], co);
						tot++;
					}
				}
			}
			else {
				MVert *mvert = me->mvert;

				tot = me->totvert;
				tree = BLI_kdtree_new(tot);

				for (i = 0; i < tot; i++) {
					float co[3];
					mul_v3_m4v3(co, ob->obmat, mvert[i].co);
					BLI_kdtree_insert(tree, i, co);
				}
			}

			BLI_kdtree_balance(tree);
			break;
		}
		case OB_CURVE:
		case OB_SURF:
		{
			/* TODO: take deformation into account */
			Curve *cu = ob->data;
			unsigned int i, a;

			Nurb *nu;

			tot = BKE_nurbList_verts_count_without_handles(&cu->nurb);
			tree = BLI_kdtree_new(tot);
			i = 0;

			nu = cu->nurb.first;
			while (nu) {
				if (nu->bezt) {
					BezTriple *bezt;

					bezt = nu->bezt;
					a = nu->pntsu;
					while (a--) {
						float co[3];
						mul_v3_m4v3(co, ob->obmat, bezt->vec[1]);
						BLI_kdtree_insert(tree, i++, co);
						bezt++;
					}
				}
				else {
					BPoint *bp;

					bp = nu->bp;
					a = nu->pntsu * nu->pntsv;
					while (a--) {
						float co[3];
						mul_v3_m4v3(co, ob->obmat, bp->vec);
						BLI_kdtree_insert(tree, i++, co);
						bp++;
					}
				}
				nu = nu->next;
			}

			BLI_kdtree_balance(tree);
			break;
		}
		case OB_LATTICE:
		{
			/* TODO: take deformation into account */
			Lattice *lt = ob->data;
			BPoint *bp;
			unsigned int i;

			tot = lt->pntsu * lt->pntsv * lt->pntsw;
			tree = BLI_kdtree_new(tot);
			i = 0;

			for (bp = lt->def; i < tot; bp++) {
				float co[3];
				mul_v3_m4v3(co, ob->obmat, bp->vec);
				BLI_kdtree_insert(tree, i++, co);
			}

			BLI_kdtree_balance(tree);
			break;
		}
	}

	*r_tot = tot;
	return tree;
}

bool BKE_object_modifier_use_time(Object *ob, ModifierData *md)
{
	if (modifier_dependsOnTime(md)) {
		return true;
	}

	/* Check whether modifier is animated. */
	/* TODO: this should be handled as part of build_animdata() -- Aligorith */
	if (ob->adt) {
		AnimData *adt = ob->adt;
		FCurve *fcu;

		char pattern[MAX_NAME + 16];
		BLI_snprintf(pattern, sizeof(pattern), "modifiers[\"%s\"]", md->name);

		/* action - check for F-Curves with paths containing 'modifiers[' */
		if (adt->action) {
			for (fcu = (FCurve *)adt->action->curves.first;
			     fcu != NULL;
			     fcu = (FCurve *)fcu->next)
			{
				if (fcu->rna_path && strstr(fcu->rna_path, pattern))
					return true;
			}
		}

		/* This here allows modifier properties to get driven and still update properly
		 *
		 * Workaround to get [#26764] (e.g. subsurf levels not updating when animated/driven)
		 * working, without the updating problems ([#28525] [#28690] [#28774] [#28777]) caused
		 * by the RNA updates cache introduced in r.38649
		 */
		for (fcu = (FCurve *)adt->drivers.first;
		     fcu != NULL;
		     fcu = (FCurve *)fcu->next)
		{
			if (fcu->rna_path && strstr(fcu->rna_path, pattern))
				return true;
		}

		/* XXX: also, should check NLA strips, though for now assume that nobody uses
		 * that and we can omit that for performance reasons... */
	}

	return false;
}

bool BKE_object_modifier_gpencil_use_time(Object *ob, GpencilModifierData *md)
{
	if (BKE_gpencil_modifier_dependsOnTime(md)) {
		return true;
	}

	/* Check whether modifier is animated. */
	/* TODO (Aligorith): this should be handled as part of build_animdata() */
	if (ob->adt) {
		AnimData *adt = ob->adt;
		FCurve *fcu;

		char pattern[MAX_NAME + 32];
		BLI_snprintf(pattern, sizeof(pattern), "grease_pencil_modifiers[\"%s\"]", md->name);

		/* action - check for F-Curves with paths containing 'grease_pencil_modifiers[' */
		if (adt->action) {
			for (fcu = (FCurve *)adt->action->curves.first;
				fcu != NULL;
				fcu = (FCurve *)fcu->next)
			{
				if (fcu->rna_path && strstr(fcu->rna_path, pattern))
					return true;
			}
		}

		/* This here allows modifier properties to get driven and still update properly
		*
		*/
		for (fcu = (FCurve *)adt->drivers.first;
			fcu != NULL;
			fcu = (FCurve *)fcu->next)
		{
			if (fcu->rna_path && strstr(fcu->rna_path, pattern))
				return true;
		}
	}

	return false;
}

bool BKE_object_shaderfx_use_time(Object *ob, ShaderFxData *fx)
{
	if (BKE_shaderfx_dependsOnTime(fx)) {
		return true;
	}

	/* Check whether effect is animated. */
	/* TODO (Aligorith): this should be handled as part of build_animdata() */
	if (ob->adt) {
		AnimData *adt = ob->adt;
		FCurve *fcu;

		char pattern[MAX_NAME + 32];
		BLI_snprintf(pattern, sizeof(pattern), "shader_effects[\"%s\"]", fx->name);

		/* action - check for F-Curves with paths containing string[' */
		if (adt->action) {
			for (fcu = (FCurve *)adt->action->curves.first;
				fcu != NULL;
				fcu = (FCurve *)fcu->next)
			{
				if (fcu->rna_path && strstr(fcu->rna_path, pattern))
					return true;
			}
		}

		/* This here allows properties to get driven and still update properly
		*
		*/
		for (fcu = (FCurve *)adt->drivers.first;
			fcu != NULL;
			fcu = (FCurve *)fcu->next)
		{
			if (fcu->rna_path && strstr(fcu->rna_path, pattern))
				return true;
		}
	}

	return false;
}

/* set "ignore cache" flag for all caches on this object */
static void object_cacheIgnoreClear(Object *ob, int state)
{
	ListBase pidlist;
	PTCacheID *pid;
	BKE_ptcache_ids_from_object(&pidlist, ob, NULL, 0);

	for (pid = pidlist.first; pid; pid = pid->next) {
		if (pid->cache) {
			if (state)
				pid->cache->flag |= PTCACHE_IGNORE_CLEAR;
			else
				pid->cache->flag &= ~PTCACHE_IGNORE_CLEAR;
		}
	}

	BLI_freelistN(&pidlist);
}

/* Note: this function should eventually be replaced by depsgraph functionality.
 * Avoid calling this in new code unless there is a very good reason for it!
 */
bool BKE_object_modifier_update_subframe(
        Depsgraph *depsgraph, Scene *scene, Object *ob, bool update_mesh,
        int parent_recursion, float frame, int type)
{
	ModifierData *md = modifiers_findByType(ob, (ModifierType)type);
	bConstraint *con;

	if (type == eModifierType_DynamicPaint) {
		DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;

		/* if other is dynamic paint canvas, don't update */
		if (pmd && pmd->canvas)
			return true;
	}
	else if (type == eModifierType_Smoke) {
		SmokeModifierData *smd = (SmokeModifierData *)md;

		if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) != 0)
			return true;
	}

	/* if object has parents, update them too */
	if (parent_recursion) {
		int recursion = parent_recursion - 1;
		bool no_update = false;
		if (ob->parent) no_update |= BKE_object_modifier_update_subframe(depsgraph, scene, ob->parent, 0, recursion, frame, type);
		if (ob->track) no_update |= BKE_object_modifier_update_subframe(depsgraph, scene, ob->track, 0, recursion, frame, type);

		/* skip subframe if object is parented
		 *  to vertex of a dynamic paint canvas */
		if (no_update && (ob->partype == PARVERT1 || ob->partype == PARVERT3))
			return false;

		/* also update constraint targets */
		for (con = ob->constraints.first; con; con = con->next) {
			const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
			ListBase targets = {NULL, NULL};

			if (cti && cti->get_constraint_targets) {
				bConstraintTarget *ct;
				cti->get_constraint_targets(con, &targets);
				for (ct = targets.first; ct; ct = ct->next) {
					if (ct->tar)
						BKE_object_modifier_update_subframe(depsgraph, scene, ct->tar, 0, recursion, frame, type);
				}
				/* free temp targets */
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 0);
			}
		}
	}

	/* was originally OB_RECALC_ALL - TODO - which flags are really needed??? */
	/* TODO(sergey): What about animation? */
	ob->id.recalc |= ID_RECALC_ALL;
	BKE_animsys_evaluate_animdata(depsgraph, scene, &ob->id, ob->adt, frame, ADT_RECALC_ANIM);
	if (update_mesh) {
		/* ignore cache clear during subframe updates
		 *  to not mess up cache validity */
		object_cacheIgnoreClear(ob, 1);
		BKE_object_handle_update(depsgraph, scene, ob);
		object_cacheIgnoreClear(ob, 0);
	}
	else
		BKE_object_where_is_calc_time(depsgraph, scene, ob, frame);

	/* for curve following objects, parented curve has to be updated too */
	if (ob->type == OB_CURVE) {
		Curve *cu = ob->data;
		BKE_animsys_evaluate_animdata(depsgraph, scene, &cu->id, cu->adt, frame, ADT_RECALC_ANIM);
	}
	/* and armatures... */
	if (ob->type == OB_ARMATURE) {
		bArmature *arm = ob->data;
		BKE_animsys_evaluate_animdata(depsgraph, scene, &arm->id, arm->adt, frame, ADT_RECALC_ANIM);
		BKE_pose_where_is(depsgraph, scene, ob);
	}

	return false;
}
