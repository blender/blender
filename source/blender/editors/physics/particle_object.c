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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/physics/particle_object.c
 *  \ingroup edphys
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_object.h"

#include "UI_resources.h"

#include "physics_intern.h"

extern void PE_create_particle_edit(Scene *scene, Object *ob, PointCache *cache, ParticleSystem *psys);
extern void PTCacheUndo_clear(PTCacheEdit *edit);
extern void recalc_lengths(PTCacheEdit *edit);
extern void recalc_emitter_field(Object *ob, ParticleSystem *psys);
extern void update_world_cos(Object *ob, PTCacheEdit *edit);

#define KEY_K					PTCacheEditKey *key; int k
#define POINT_P					PTCacheEditPoint *point; int p
#define LOOP_POINTS				for (p=0, point=edit->points; p<edit->totpoint; p++, point++)
#if 0
#define LOOP_VISIBLE_POINTS		for (p=0, point=edit->points; p<edit->totpoint; p++, point++) if (!(point->flag & PEP_HIDE))
#define LOOP_SELECTED_POINTS	for (p=0, point=edit->points; p<edit->totpoint; p++, point++) if (point_is_selected(point))
#define LOOP_UNSELECTED_POINTS	for (p=0, point=edit->points; p<edit->totpoint; p++, point++) if (!point_is_selected(point))
#define LOOP_EDITED_POINTS		for (p=0, point=edit->points; p<edit->totpoint; p++, point++) if (point->flag & PEP_EDIT_RECALC)
#define LOOP_TAGGED_POINTS		for (p=0, point=edit->points; p<edit->totpoint; p++, point++) if (point->flag & PEP_TAG)
#endif
#define LOOP_KEYS				for (k=0, key=point->keys; k<point->totkey; k++, key++)
#if 0
#define LOOP_VISIBLE_KEYS		for (k=0, key=point->keys; k<point->totkey; k++, key++) if (!(key->flag & PEK_HIDE))
#define LOOP_SELECTED_KEYS		for (k=0, key=point->keys; k<point->totkey; k++, key++) if ((key->flag & PEK_SELECT) && !(key->flag & PEK_HIDE))
#define LOOP_TAGGED_KEYS		for (k=0, key=point->keys; k<point->totkey; k++, key++) if (key->flag & PEK_TAG)

#define KEY_WCO					(key->flag & PEK_USE_WCO ? key->world_co : key->co)
#endif

static float I[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};

/********************** particle system slot operators *********************/

static int particle_system_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob= ED_object_context(C);
	Scene *scene = CTX_data_scene(C);

	if (!scene || !ob)
		return OPERATOR_CANCELLED;

	object_add_particle_system(scene, ob, NULL);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, ob);
	WM_event_add_notifier(C, NC_OBJECT|ND_POINTCACHE, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_particle_system_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Particle System Slot";
	ot->idname = "OBJECT_OT_particle_system_add";
	ot->description = "Add a particle system";
	
	/* api callbacks */
	ot->poll = ED_operator_object_active_editable;
	ot->exec = particle_system_add_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int particle_system_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Scene *scene = CTX_data_scene(C);
	int mode_orig;

	if (!scene || !ob)
		return OPERATOR_CANCELLED;

	mode_orig = ob->mode;
	object_remove_particle_system(scene, ob);

	/* possible this isn't the active object
	 * object_remove_particle_system() clears the mode on the last psys
	 */
	if (mode_orig & OB_MODE_PARTICLE_EDIT) {
		if ((ob->mode & OB_MODE_PARTICLE_EDIT) == 0) {
			if (scene->basact && scene->basact->object == ob) {
				WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_OBJECT, NULL);
			}
		}
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, ob);
	WM_event_add_notifier(C, NC_OBJECT|ND_POINTCACHE, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_particle_system_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Particle System Slot";
	ot->idname = "OBJECT_OT_particle_system_remove";
	ot->description = "Remove the selected particle system";
	
	/* api callbacks */
	ot->poll = ED_operator_object_active_editable;
	ot->exec = particle_system_remove_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new particle settings operator *********************/

static int psys_poll(bContext *C)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	return (ptr.data != NULL);
}

static int new_particle_settings_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain= CTX_data_main(C);
	ParticleSystem *psys;
	ParticleSettings *part = NULL;
	Object *ob;
	PointerRNA ptr;

	ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);

	psys = ptr.data;

	/* add or copy particle setting */
	if (psys->part)
		part= BKE_particlesettings_copy(bmain, psys->part);
	else
		part= psys_new_settings("ParticleSettings", bmain);

	ob= ptr.id.data;

	if (psys->part)
		id_us_min(&psys->part->id);

	psys->part = part;

	psys_check_boid_data(psys);

	DAG_relations_tag_update(bmain);
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Particle Settings";
	ot->idname = "PARTICLE_OT_new";
	ot->description = "Add new particle settings";
	
	/* api callbacks */
	ot->exec = new_particle_settings_exec;
	ot->poll = psys_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** keyed particle target operators *********************/

static int new_particle_target_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;

	ParticleTarget *pt;

	if (!psys)
		return OPERATOR_CANCELLED;

	pt = psys->targets.first;
	for (; pt; pt=pt->next)
		pt->flag &= ~PTARGET_CURRENT;

	pt = MEM_callocN(sizeof(ParticleTarget), "keyed particle target");

	pt->flag |= PTARGET_CURRENT;
	pt->psys = 1;

	BLI_addtail(&psys->targets, pt);

	DAG_relations_tag_update(bmain);
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_new_target(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Particle Target";
	ot->idname = "PARTICLE_OT_new_target";
	ot->description = "Add a new particle target";
	
	/* api callbacks */
	ot->exec = new_particle_target_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int remove_particle_target_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;

	ParticleTarget *pt;

	if (!psys)
		return OPERATOR_CANCELLED;

	pt = psys->targets.first;
	for (; pt; pt=pt->next) {
		if (pt->flag & PTARGET_CURRENT) {
			BLI_remlink(&psys->targets, pt);
			MEM_freeN(pt);
			break;
		}

	}
	pt = psys->targets.last;

	if (pt)
		pt->flag |= PTARGET_CURRENT;

	DAG_relations_tag_update(bmain);
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_target_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Particle Target";
	ot->idname = "PARTICLE_OT_target_remove";
	ot->description = "Remove the selected particle target";
	
	/* api callbacks */
	ot->exec = remove_particle_target_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move up particle target operator *********************/

static int target_move_up_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;
	ParticleTarget *pt;

	if (!psys)
		return OPERATOR_CANCELLED;
	
	pt = psys->targets.first;
	for (; pt; pt=pt->next) {
		if (pt->flag & PTARGET_CURRENT && pt->prev) {
			BLI_remlink(&psys->targets, pt);
			BLI_insertlinkbefore(&psys->targets, pt->prev, pt);

			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, ob);
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_target_move_up(wmOperatorType *ot)
{
	ot->name = "Move Up Target";
	ot->idname = "PARTICLE_OT_target_move_up";
	ot->description = "Move particle target up in the list";
	
	ot->exec = target_move_up_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move down particle target operator *********************/

static int target_move_down_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;
	ParticleTarget *pt;

	if (!psys)
		return OPERATOR_CANCELLED;
	pt = psys->targets.first;
	for (; pt; pt=pt->next) {
		if (pt->flag & PTARGET_CURRENT && pt->next) {
			BLI_remlink(&psys->targets, pt);
			BLI_insertlinkafter(&psys->targets, pt->next, pt);

			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, ob);
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_target_move_down(wmOperatorType *ot)
{
	ot->name = "Move Down Target";
	ot->idname = "PARTICLE_OT_target_move_down";
	ot->description = "Move particle target down in the list";
	
	ot->exec = target_move_down_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move up particle dupliweight operator *********************/

static int dupliob_move_up_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	ParticleSettings *part;
	ParticleDupliWeight *dw;

	if (!psys)
		return OPERATOR_CANCELLED;

	part = psys->part;
	for (dw=part->dupliweights.first; dw; dw=dw->next) {
		if (dw->flag & PART_DUPLIW_CURRENT && dw->prev) {
			BLI_remlink(&part->dupliweights, dw);
			BLI_insertlinkbefore(&part->dupliweights, dw->prev, dw);

			WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, NULL);
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_dupliob_move_up(wmOperatorType *ot)
{
	ot->name = "Move Up Dupli Object";
	ot->idname = "PARTICLE_OT_dupliob_move_up";
	ot->description = "Move dupli object up in the list";
	
	ot->exec = dupliob_move_up_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** particle dupliweight operators *********************/

static int copy_particle_dupliob_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	ParticleSettings *part;
	ParticleDupliWeight *dw;

	if (!psys)
		return OPERATOR_CANCELLED;
	part = psys->part;
	for (dw=part->dupliweights.first; dw; dw=dw->next) {
		if (dw->flag & PART_DUPLIW_CURRENT) {
			dw->flag &= ~PART_DUPLIW_CURRENT;
			dw = MEM_dupallocN(dw);
			dw->flag |= PART_DUPLIW_CURRENT;
			BLI_addhead(&part->dupliweights, dw);

			WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, NULL);
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_dupliob_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Particle Dupliob";
	ot->idname = "PARTICLE_OT_dupliob_copy";
	ot->description = "Duplicate the current dupliobject";
	
	/* api callbacks */
	ot->exec = copy_particle_dupliob_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int remove_particle_dupliob_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	ParticleSettings *part;
	ParticleDupliWeight *dw;

	if (!psys)
		return OPERATOR_CANCELLED;

	part = psys->part;
	for (dw=part->dupliweights.first; dw; dw=dw->next) {
		if (dw->flag & PART_DUPLIW_CURRENT) {
			BLI_remlink(&part->dupliweights, dw);
			MEM_freeN(dw);
			break;
		}

	}
	dw = part->dupliweights.last;

	if (dw)
		dw->flag |= PART_DUPLIW_CURRENT;

	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, NULL);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_dupliob_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Particle Dupliobject";
	ot->idname = "PARTICLE_OT_dupliob_remove";
	ot->description = "Remove the selected dupliobject";
	
	/* api callbacks */
	ot->exec = remove_particle_dupliob_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move down particle dupliweight operator *********************/

static int dupliob_move_down_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	ParticleSettings *part;
	ParticleDupliWeight *dw;

	if (!psys)
		return OPERATOR_CANCELLED;

	part = psys->part;
	for (dw=part->dupliweights.first; dw; dw=dw->next) {
		if (dw->flag & PART_DUPLIW_CURRENT && dw->next) {
			BLI_remlink(&part->dupliweights, dw);
			BLI_insertlinkafter(&part->dupliweights, dw->next, dw);

			WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, NULL);
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_dupliob_move_down(wmOperatorType *ot)
{
	ot->name = "Move Down Dupli Object";
	ot->idname = "PARTICLE_OT_dupliob_move_down";
	ot->description = "Move dupli object down in the list";
	
	ot->exec = dupliob_move_down_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ connect/disconnect hair operators *********************/

static void disconnect_hair(Scene *scene, Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd = psys_get_modifier(ob, psys);
	ParticleEditSettings *pset= PE_settings(scene);
	ParticleData *pa;
	PTCacheEdit *edit;
	PTCacheEditPoint *point;
	PTCacheEditKey *ekey = NULL;
	HairKey *key;
	int i, k;
	float hairmat[4][4];

	if (!ob || !psys || psys->flag & PSYS_GLOBAL_HAIR)
		return;

	if (!psys->part || psys->part->type != PART_HAIR)
		return;
	
	edit = psys->edit;
	point= edit ? edit->points : NULL;

	for (i=0, pa=psys->particles; i<psys->totpart; i++, pa++) {
		if (point) {
			ekey = point->keys;
			point++;
		}

		psys_mat_hair_to_global(ob, psmd->dm_final, psys->part->from, pa, hairmat);

		for (k=0, key=pa->hair; k<pa->totkey; k++, key++) {
			mul_m4_v3(hairmat, key->co);
			
			if (ekey) {
				ekey->flag &= ~PEK_USE_WCO;
				ekey++;
			}
		}
	}

	psys_free_path_cache(psys, psys->edit);

	psys->flag |= PSYS_GLOBAL_HAIR;

	if (ELEM(pset->brushtype, PE_BRUSH_ADD, PE_BRUSH_PUFF))
		pset->brushtype = PE_BRUSH_NONE;

	PE_update_object(scene, ob, 0);
}

static int disconnect_hair_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= ED_object_context(C);
	ParticleSystem *psys= NULL;
	const bool all = RNA_boolean_get(op->ptr, "all");

	if (!ob)
		return OPERATOR_CANCELLED;

	if (all) {
		for (psys=ob->particlesystem.first; psys; psys=psys->next) {
			disconnect_hair(scene, ob, psys);
		}
	}
	else {
		psys = psys_get_current(ob);
		disconnect_hair(scene, ob, psys);
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_disconnect_hair(wmOperatorType *ot)
{
	ot->name = "Disconnect Hair";
	ot->description = "Disconnect hair from the emitter mesh";
	ot->idname = "PARTICLE_OT_disconnect_hair";
	
	ot->exec = disconnect_hair_exec;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;  /* No REGISTER, redo does not work due to missing update, see T47750. */

	RNA_def_boolean(ot->srna, "all", 0, "All hair", "Disconnect all hair systems from the emitter mesh");
}

/* from/to_world_space : whether from/to particles are in world or hair space
 * from/to_mat : additional transform for from/to particles (e.g. for using object space copying)
 */
static bool remap_hair_emitter(Scene *scene, Object *ob, ParticleSystem *psys,
                               Object *target_ob, ParticleSystem *target_psys, PTCacheEdit *target_edit,
                               float from_mat[4][4], float to_mat[4][4], bool from_global, bool to_global)
{
	ParticleSystemModifierData *target_psmd = psys_get_modifier(target_ob, target_psys);
	ParticleData *pa, *tpa;
	PTCacheEditPoint *edit_point;
	PTCacheEditKey *ekey;
	BVHTreeFromMesh bvhtree= {NULL};
	MFace *mface = NULL, *mf;
	MEdge *medge = NULL, *me;
	MVert *mvert;
	DerivedMesh *dm, *target_dm;
	int numverts;
	int i, k;
	float from_ob_imat[4][4], to_ob_imat[4][4];
	float from_imat[4][4], to_imat[4][4];

	if (!target_psmd->dm_final)
		return false;
	if (!psys->part || psys->part->type != PART_HAIR)
		return false;
	if (!target_psys->part || target_psys->part->type != PART_HAIR)
		return false;
	
	edit_point = target_edit ? target_edit->points : NULL;
	
	invert_m4_m4(from_ob_imat, ob->obmat);
	invert_m4_m4(to_ob_imat, target_ob->obmat);
	invert_m4_m4(from_imat, from_mat);
	invert_m4_m4(to_imat, to_mat);
	
	if (target_psmd->dm_final->deformedOnly) {
		/* we don't want to mess up target_psmd->dm when converting to global coordinates below */
		dm = target_psmd->dm_final;
	}
	else {
		dm = target_psmd->dm_deformed;
	}
	target_dm = target_psmd->dm_final;
	if (dm == NULL) {
		return false;
	}
	/* don't modify the original vertices */
	dm = CDDM_copy(dm);

	/* BMESH_ONLY, deform dm may not have tessface */
	DM_ensure_tessface(dm);

	numverts = dm->getNumVerts(dm);
	mvert = dm->getVertArray(dm);

	/* convert to global coordinates */
	for (i=0; i<numverts; i++)
		mul_m4_v3(to_mat, mvert[i].co);

	if (dm->getNumTessFaces(dm) != 0) {
		mface = dm->getTessFaceArray(dm);
		bvhtree_from_mesh_faces(&bvhtree, dm, 0.0, 2, 6);
	}
	else if (dm->getNumEdges(dm) != 0) {
		medge = dm->getEdgeArray(dm);
		bvhtree_from_mesh_edges(&bvhtree, dm, 0.0, 2, 6);
	}
	else {
		dm->release(dm);
		return false;
	}

	for (i = 0, tpa = target_psys->particles, pa = psys->particles;
	     i < target_psys->totpart;
	     i++, tpa++, pa++) {

		float from_co[3];
		BVHTreeNearest nearest;

		if (from_global)
			mul_v3_m4v3(from_co, from_ob_imat, pa->hair[0].co);
		else
			mul_v3_m4v3(from_co, from_ob_imat, pa->hair[0].world_co);
		mul_m4_v3(from_mat, from_co);

		nearest.index = -1;
		nearest.dist_sq = FLT_MAX;

		BLI_bvhtree_find_nearest(bvhtree.tree, from_co, &nearest, bvhtree.nearest_callback, &bvhtree);

		if (nearest.index == -1) {
			if (G.debug & G_DEBUG)
				printf("No nearest point found for hair root!");
			continue;
		}

		if (mface) {
			float v[4][3];
			
			mf = &mface[nearest.index];

			copy_v3_v3(v[0], mvert[mf->v1].co);
			copy_v3_v3(v[1], mvert[mf->v2].co);
			copy_v3_v3(v[2], mvert[mf->v3].co);
			if (mf->v4) {
				copy_v3_v3(v[3], mvert[mf->v4].co);
				interp_weights_poly_v3(tpa->fuv, v, 4, nearest.co);
			}
			else
				interp_weights_poly_v3(tpa->fuv, v, 3, nearest.co);
			tpa->foffset = 0.0f;

			tpa->num = nearest.index;
			tpa->num_dmcache = psys_particle_dm_face_lookup(target_dm, dm, tpa->num, tpa->fuv, NULL);
		}
		else {
			me = &medge[nearest.index];

			tpa->fuv[1] = line_point_factor_v3(nearest.co,
			                                   mvert[me->v1].co,
			                                   mvert[me->v2].co);
			tpa->fuv[0] = 1.0f - tpa->fuv[1];
			tpa->fuv[2] = tpa->fuv[3] = 0.0f;
			tpa->foffset = 0.0f;

			tpa->num = nearest.index;
			tpa->num_dmcache = -1;
		}

		/* translate hair keys */
		{
			HairKey *key, *tkey;
			float hairmat[4][4], imat[4][4];
			float offset[3];
			
			if (to_global)
				copy_m4_m4(imat, target_ob->obmat);
			else {
				/* note: using target_dm here, which is in target_ob object space and has full modifiers */
				psys_mat_hair_to_object(target_ob, target_dm, target_psys->part->from, tpa, hairmat);
				invert_m4_m4(imat, hairmat);
			}
			mul_m4_m4m4(imat, imat, to_imat);
			
			/* offset in world space */
			sub_v3_v3v3(offset, nearest.co, from_co);
			
			if (edit_point) {
				for (k=0, key=pa->hair, tkey=tpa->hair, ekey = edit_point->keys; k<tpa->totkey; k++, key++, tkey++, ekey++) {
					float co_orig[3];
					
					if (from_global)
						mul_v3_m4v3(co_orig, from_ob_imat, key->co);
					else
						mul_v3_m4v3(co_orig, from_ob_imat, key->world_co);
					mul_m4_v3(from_mat, co_orig);
					
					add_v3_v3v3(tkey->co, co_orig, offset);
					
					mul_m4_v3(imat, tkey->co);
					
					ekey->flag |= PEK_USE_WCO;
				}
				
				edit_point++;
			}
			else {
				for (k=0, key=pa->hair, tkey=tpa->hair; k<tpa->totkey; k++, key++, tkey++) {
					float co_orig[3];
					
					if (from_global)
						mul_v3_m4v3(co_orig, from_ob_imat, key->co);
					else
						mul_v3_m4v3(co_orig, from_ob_imat, key->world_co);
					mul_m4_v3(from_mat, co_orig);
					
					add_v3_v3v3(tkey->co, co_orig, offset);
					
					mul_m4_v3(imat, tkey->co);
				}
			}
		}
	}

	free_bvhtree_from_mesh(&bvhtree);
	dm->release(dm);

	psys_free_path_cache(target_psys, target_edit);

	PE_update_object(scene, target_ob, 0);

	return true;
}

static bool connect_hair(Scene *scene, Object *ob, ParticleSystem *psys)
{
	bool ok;
	
	if (!psys)
		return false;
	
	ok = remap_hair_emitter(scene, ob, psys, ob, psys, psys->edit, ob->obmat, ob->obmat, psys->flag & PSYS_GLOBAL_HAIR, false);
	psys->flag &= ~PSYS_GLOBAL_HAIR;
	
	return ok;
}

static int connect_hair_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= ED_object_context(C);
	ParticleSystem *psys= NULL;
	const bool all = RNA_boolean_get(op->ptr, "all");
	bool any_connected = false;

	if (!ob)
		return OPERATOR_CANCELLED;

	if (all) {
		for (psys=ob->particlesystem.first; psys; psys=psys->next) {
			any_connected |= connect_hair(scene, ob, psys);
		}
	}
	else {
		psys = psys_get_current(ob);
		any_connected |= connect_hair(scene, ob, psys);
	}

	if (!any_connected) {
		BKE_report(op->reports, RPT_WARNING,
		           "No hair connected (can't connect hair if particle system modifier is disabled)");
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_connect_hair(wmOperatorType *ot)
{
	ot->name = "Connect Hair";
	ot->description = "Connect hair to the emitter mesh";
	ot->idname = "PARTICLE_OT_connect_hair";
	
	ot->exec = connect_hair_exec;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;  /* No REGISTER, redo does not work due to missing update, see T47750. */

	RNA_def_boolean(ot->srna, "all", 0, "All hair", "Connect all hair systems to the emitter mesh");
}

/************************ particle system copy operator *********************/

typedef enum eCopyParticlesSpace {
	PAR_COPY_SPACE_OBJECT   = 0,
	PAR_COPY_SPACE_WORLD    = 1,
} eCopyParticlesSpace;

static void copy_particle_edit(Scene *scene, Object *ob, ParticleSystem *psys, ParticleSystem *psys_from)
{
	PTCacheEdit *edit_from = psys_from->edit, *edit;
	ParticleData *pa;
	KEY_K;
	POINT_P;
	
	if (!edit_from)
		return;
	
	edit = MEM_dupallocN(edit_from);
	edit->psys = psys;
	psys->edit = edit;
	
	edit->pathcache = NULL;
	BLI_listbase_clear(&edit->pathcachebufs);
	
	edit->emitter_field = NULL;
	edit->emitter_cosnos = NULL;
	
	BLI_listbase_clear(&edit->undo);
	edit->curundo = NULL;
	
	edit->points = MEM_dupallocN(edit_from->points);
	pa = psys->particles;
	LOOP_POINTS {
		HairKey *hkey = pa->hair;
		
		point->keys= MEM_dupallocN(point->keys);
		LOOP_KEYS {
			key->co = hkey->co;
			key->time = &hkey->time;
			key->flag = hkey->editflag;
			if (!(psys->flag & PSYS_GLOBAL_HAIR)) {
				key->flag |= PEK_USE_WCO;
				hkey->editflag |= PEK_USE_WCO;
			}
			
			hkey++;
		}
		
		pa++;
	}
	update_world_cos(ob, edit);
	
	UI_GetThemeColor3ubv(TH_EDGE_SELECT, edit->sel_col);
	UI_GetThemeColor3ubv(TH_WIRE, edit->nosel_col);
	
	recalc_lengths(edit);
	recalc_emitter_field(ob, psys);
	PE_update_object(scene, ob, true);
	
	PTCacheUndo_clear(edit);
	PE_undo_push(scene, "Original");
}

static void remove_particle_systems_from_object(Object *ob_to)
{
	ModifierData *md, *md_next;
	
	if (ob_to->type != OB_MESH)
		return;
	if (!ob_to->data || ID_IS_LINKED_DATABLOCK(ob_to->data))
		return;
	
	for (md = ob_to->modifiers.first; md; md = md_next) {
		md_next = md->next;
		
		/* remove all particle system modifiers as well,
		 * these need to sync to the particle system list
		 */
		if (ELEM(md->type, eModifierType_ParticleSystem, eModifierType_DynamicPaint, eModifierType_Smoke)) {
			BLI_remlink(&ob_to->modifiers, md);
			modifier_free(md);
		}
	}
	
	BKE_object_free_particlesystems(ob_to);
}

/* single_psys_from is optional, if NULL all psys of ob_from are copied */
static bool copy_particle_systems_to_object(Main *bmain,
                                            Scene *scene,
                                            Object *ob_from,
                                            ParticleSystem *single_psys_from,
                                            Object *ob_to,
                                            int space,
                                            bool duplicate_settings)
{
	ModifierData *md;
	ParticleSystem *psys_start = NULL, *psys, *psys_from;
	ParticleSystem **tmp_psys;
	DerivedMesh *final_dm;
	CustomDataMask cdmask;
	int i, totpsys;
	
	if (ob_to->type != OB_MESH)
		return false;
	if (!ob_to->data || ID_IS_LINKED_DATABLOCK(ob_to->data))
		return false;
	
	/* For remapping we need a valid DM.
	 * Because the modifiers are appended at the end it's safe to use
	 * the final DM of the object without particles.
	 * However, when evaluating the DM all the particle modifiers must be valid,
	 * i.e. have the psys assigned already.
	 * To break this hen/egg problem we create all psys separately first (to collect required customdata masks),
	 * then create the DM, then add them to the object and make the psys modifiers ...
	 */
	#define PSYS_FROM_FIRST (single_psys_from ? single_psys_from : ob_from->particlesystem.first)
	#define PSYS_FROM_NEXT(cur) (single_psys_from ? NULL : (cur)->next)
	totpsys = single_psys_from ? 1 : BLI_listbase_count(&ob_from->particlesystem);
	
	tmp_psys = MEM_mallocN(sizeof(ParticleSystem*) * totpsys, "temporary particle system array");
	
	cdmask = 0;
	for (psys_from = PSYS_FROM_FIRST, i = 0;
	     psys_from;
	     psys_from = PSYS_FROM_NEXT(psys_from), ++i) {
		
		psys = BKE_object_copy_particlesystem(psys_from);
		tmp_psys[i] = psys;
		
		if (psys_start == NULL)
			psys_start = psys;
		
		cdmask |= psys_emitter_customdata_mask(psys);
	}
	/* to iterate source and target psys in sync,
	 * we need to know where the newly added psys start
	 */
	psys_start = totpsys > 0 ? tmp_psys[0] : NULL;
	
	/* get the DM (psys and their modifiers have not been appended yet) */
	final_dm = mesh_get_derived_final(scene, ob_to, cdmask);
	
	/* now append psys to the object and make modifiers */
	for (i = 0, psys_from = PSYS_FROM_FIRST;
	     i < totpsys;
	     ++i, psys_from = PSYS_FROM_NEXT(psys_from)) {
		
		ParticleSystemModifierData *psmd;
		
		psys = tmp_psys[i];
		
		/* append to the object */
		BLI_addtail(&ob_to->particlesystem, psys);
		
		/* add a particle system modifier for each system */
		md = modifier_new(eModifierType_ParticleSystem);
		psmd = (ParticleSystemModifierData *)md;
		/* push on top of the stack, no use trying to reproduce old stack order */
		BLI_addtail(&ob_to->modifiers, md);
		
		BLI_snprintf(md->name, sizeof(md->name), "ParticleSystem %i", i);
		modifier_unique_name(&ob_to->modifiers, (ModifierData *)psmd);
		
		psmd->psys = psys;
		psmd->dm_final = CDDM_copy(final_dm);
		CDDM_calc_normals(psmd->dm_final);
		DM_ensure_tessface(psmd->dm_final);
		
		if (psys_from->edit)
			copy_particle_edit(scene, ob_to, psys, psys_from);

		if (duplicate_settings) {
			id_us_min(&psys->part->id);
			psys->part = BKE_particlesettings_copy(bmain, psys->part);
		}
	}
	MEM_freeN(tmp_psys);
	
	/* note: do this after creating DM copies for all the particle system modifiers,
	 * the remapping otherwise makes final_dm invalid!
	 */
	for (psys = psys_start, psys_from = PSYS_FROM_FIRST, i = 0;
	     psys;
	     psys = psys->next, psys_from = PSYS_FROM_NEXT(psys_from), ++i) {
		
		float (*from_mat)[4], (*to_mat)[4];
		
		switch (space) {
			case PAR_COPY_SPACE_OBJECT:
				from_mat = I;
				to_mat = I;
				break;
			case PAR_COPY_SPACE_WORLD:
				from_mat = ob_from->obmat;
				to_mat = ob_to->obmat;
				break;
			default:
				/* should not happen */
				from_mat = to_mat = NULL;
				BLI_assert(false);
				break;
		}
		if (ob_from != ob_to) {
			remap_hair_emitter(scene, ob_from, psys_from, ob_to, psys, psys->edit, from_mat, to_mat, psys_from->flag & PSYS_GLOBAL_HAIR, psys->flag & PSYS_GLOBAL_HAIR);
		}
		
		/* tag for recalc */
//		psys->recalc |= PSYS_RECALC_RESET;
	}
	
	#undef PSYS_FROM_FIRST
	#undef PSYS_FROM_NEXT
	
	DAG_id_tag_update(&ob_to->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, ob_to);
	return true;
}

static int copy_particle_systems_poll(bContext *C)
{
	Object *ob;
	if (!ED_operator_object_active_editable(C))
		return false;
	
	ob = ED_object_active_context(C);
	if (BLI_listbase_is_empty(&ob->particlesystem))
		return false;
	
	return true;
}

static int copy_particle_systems_exec(bContext *C, wmOperator *op)
{
	const int space = RNA_enum_get(op->ptr, "space");
	const bool remove_target_particles = RNA_boolean_get(op->ptr, "remove_target_particles");
	const bool use_active = RNA_boolean_get(op->ptr, "use_active");
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob_from = ED_object_active_context(C);
	ParticleSystem *psys_from = use_active ? CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data : NULL;
	
	int changed_tot = 0;
	int fail = 0;
	
	CTX_DATA_BEGIN (C, Object *, ob_to, selected_editable_objects)
	{
		if (ob_from != ob_to) {
			bool changed = false;
			if (remove_target_particles) {
				remove_particle_systems_from_object(ob_to);
				changed = true;
			}
			if (copy_particle_systems_to_object(bmain, scene, ob_from, psys_from, ob_to, space, false))
				changed = true;
			else
				fail++;
			
			if (changed)
				changed_tot++;
		}
	}
	CTX_DATA_END;
	
	if ((changed_tot == 0 && fail == 0) || fail) {
		BKE_reportf(op->reports, RPT_ERROR,
		            "Copy particle systems to selected: %d done, %d failed",
		            changed_tot, fail);
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_copy_particle_systems(wmOperatorType *ot)
{
	static EnumPropertyItem space_items[] = {
		{PAR_COPY_SPACE_OBJECT, "OBJECT", 0, "Object", "Copy inside each object's local space"},
		{PAR_COPY_SPACE_WORLD, "WORLD", 0, "World", "Copy in world space"},
		{0, NULL, 0, NULL, NULL}
	};
	
	ot->name = "Copy Particle Systems";
	ot->description = "Copy particle systems from the active object to selected objects";
	ot->idname = "PARTICLE_OT_copy_particle_systems";
	
	ot->poll = copy_particle_systems_poll;
	ot->exec = copy_particle_systems_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "space", space_items, PAR_COPY_SPACE_OBJECT, "Space", "Space transform for copying from one object to another");
	RNA_def_boolean(ot->srna, "remove_target_particles", true, "Remove Target Particles", "Remove particle systems on the target objects");
	RNA_def_boolean(ot->srna, "use_active", false, "Use Active", "Use the active particle system from the context");
}

static int duplicate_particle_systems_poll(bContext *C)
{
	if (!ED_operator_object_active_editable(C)) {
		return false;
	}
	Object *ob = ED_object_active_context(C);
	if (BLI_listbase_is_empty(&ob->particlesystem)) {
		return false;
	}
	return true;
}

static int duplicate_particle_systems_exec(bContext *C, wmOperator *op)
{
	const bool duplicate_settings = RNA_boolean_get(op->ptr, "use_duplicate_settings");
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	ParticleSystem *psys = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data;
	copy_particle_systems_to_object(CTX_data_main(C), scene, ob, psys, ob,
	                                PAR_COPY_SPACE_OBJECT, duplicate_settings);
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_duplicate_particle_system(wmOperatorType *ot)
{
	ot->name = "Duplicate Particle Systems";
	ot->description = "Duplicate particle system within the active object";
	ot->idname = "PARTICLE_OT_duplicate_particle_system";

	ot->poll = duplicate_particle_systems_poll;
	ot->exec = duplicate_particle_systems_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "use_duplicate_settings", false, "Duplicate Settings",
	                "Duplicate settings as well, so new particle system uses own settings");
}
