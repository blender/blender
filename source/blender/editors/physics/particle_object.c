/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_main.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_particle.h"

#include "physics_intern.h"

/********************** particle system slot operators *********************/

static int particle_system_add_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Scene *scene = CTX_data_scene(C);

	if(!scene || !ob)
		return OPERATOR_CANCELLED;

	object_add_particle_system(scene, ob);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_particle_system_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Particle System Slot";
	ot->idname= "OBJECT_OT_particle_system_add";
	ot->description="Add a particle system.";
	
	/* api callbacks */
	ot->exec= particle_system_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int particle_system_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Scene *scene = CTX_data_scene(C);

	if(!scene || !ob)
		return OPERATOR_CANCELLED;

	object_remove_particle_system(scene, ob);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_particle_system_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Particle System Slot";
	ot->idname= "OBJECT_OT_particle_system_remove";
	ot->description="Remove the selected particle system.";
	
	/* api callbacks */
	ot->exec= particle_system_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new particle settings operator *********************/

static int psys_poll(bContext *C)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	return (ptr.data != NULL);
}

static int new_particle_settings_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Main *bmain= CTX_data_main(C);
	ParticleSystem *psys;
	ParticleSettings *part = NULL;
	Object *ob;
	PointerRNA ptr;

	ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);

	psys = ptr.data;

	/* add or copy particle setting */
	if(psys->part)
		part= psys_copy_settings(psys->part);
	else
		part= psys_new_settings("ParticleSettings", bmain);

	ob= ptr.id.data;

	if(psys->part)
		psys->part->id.us--;

	psys->part = part;

	psys_check_boid_data(psys);

	DAG_scene_sort(scene);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Particle Settings";
	ot->idname= "PARTICLE_OT_new";
	ot->description="Add new particle settings.";
	
	/* api callbacks */
	ot->exec= new_particle_settings_exec;
	ot->poll= psys_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** keyed particle target operators *********************/

static int new_particle_target_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;

	ParticleTarget *pt;

	if(!psys)
		return OPERATOR_CANCELLED;

	pt = psys->targets.first;
	for(; pt; pt=pt->next)
		pt->flag &= ~PTARGET_CURRENT;

	pt = MEM_callocN(sizeof(ParticleTarget), "keyed particle target");

	pt->flag |= PTARGET_CURRENT;
	pt->psys = 1;

	BLI_addtail(&psys->targets, pt);

	DAG_scene_sort(scene);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_new_target(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Particle Target";
	ot->idname= "PARTICLE_OT_new_target";
	ot->description="Add a new particle target.";
	
	/* api callbacks */
	ot->exec= new_particle_target_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int remove_particle_target_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;

	ParticleTarget *pt;

	if(!psys)
		return OPERATOR_CANCELLED;

	pt = psys->targets.first;
	for(; pt; pt=pt->next) {
		if(pt->flag & PTARGET_CURRENT) {
			BLI_remlink(&psys->targets, pt);
			MEM_freeN(pt);
			break;
		}

	}
	pt = psys->targets.last;

	if(pt)
		pt->flag |= PTARGET_CURRENT;

	DAG_scene_sort(scene);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_remove_target(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Particle Target";
	ot->idname= "PARTICLE_OT_remove_target";
	ot->description="Remove the selected particle target.";
	
	/* api callbacks */
	ot->exec= remove_particle_target_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move up particle target operator *********************/

static int target_move_up_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;
	ParticleTarget *pt;

	if(!psys)
		return OPERATOR_CANCELLED;
	
	pt = psys->targets.first;
	for(; pt; pt=pt->next) {
		if(pt->flag & PTARGET_CURRENT && pt->prev) {
			BLI_remlink(&psys->targets, pt);
			BLI_insertlink(&psys->targets, pt->prev->prev, pt);

			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_target_move_up(wmOperatorType *ot)
{
	ot->name= "Move Up Target";
	ot->idname= "PARTICLE_OT_target_move_up";
	ot->description= "Move particle target up in the list.";
	
	ot->exec= target_move_up_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move down particle target operator *********************/

static int target_move_down_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;
	ParticleTarget *pt;

	if(!psys)
		return OPERATOR_CANCELLED;
	pt = psys->targets.first;
	for(; pt; pt=pt->next) {
		if(pt->flag & PTARGET_CURRENT && pt->next) {
			BLI_remlink(&psys->targets, pt);
			BLI_insertlink(&psys->targets, pt->next, pt);

			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_target_move_down(wmOperatorType *ot)
{
	ot->name= "Move Down Target";
	ot->idname= "PARTICLE_OT_target_move_down";
	ot->description= "Move particle target down in the list.";
	
	ot->exec= target_move_down_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ connect/disconnect hair operators *********************/

static void disconnect_hair(Scene *scene, Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd = psys_get_modifier(ob,psys);
	ParticleData *pa;
	PTCacheEdit *edit;
	PTCacheEditPoint *point;
	PTCacheEditKey *ekey = NULL;
	HairKey *key;
	int i, k;
	float hairmat[4][4];

	if(!ob || !psys || psys->flag & PSYS_GLOBAL_HAIR)
		return;

	if(!psys->part || psys->part->type != PART_HAIR)
		return;
	
	edit = psys->edit;
	point= edit ? edit->points : NULL;

	for(i=0, pa=psys->particles; i<psys->totpart; i++,pa++) {
		if(point) {
			ekey = point->keys;
			point++;
		}

		psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, hairmat);

		for(k=0,key=pa->hair; k<pa->totkey; k++,key++) {
			Mat4MulVecfl(hairmat,key->co);
			
			if(ekey) {
				ekey->flag &= ~PEK_USE_WCO;
				ekey++;
			}
		}
	}

	psys_free_path_cache(psys, psys->edit);

	psys->flag |= PSYS_GLOBAL_HAIR;

	PE_update_object(scene, ob, 0);
}

static int disconnect_hair_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= NULL;
	int all = RNA_boolean_get(op->ptr, "all");

	if(!ob)
		return OPERATOR_CANCELLED;

	if(all) {
		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			disconnect_hair(scene, ob, psys);
		}
	}
	else {
		psys = ptr.data;
		disconnect_hair(scene, ob, psys);
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_disconnect_hair(wmOperatorType *ot)
{
	ot->name= "Disconnect Hair";
	ot->description= "Disconnect hair from the emitter mesh.";
	ot->idname= "PARTICLE_OT_disconnect_hair";
	
	ot->exec= disconnect_hair_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "all", 0, "All hair", "Disconnect all hair systems from the emitter mesh");
}

static void connect_hair(Scene *scene, Object *ob, ParticleSystem *psys)
{
	ParticleSystemModifierData *psmd = psys_get_modifier(ob,psys);
	ParticleData *pa;
	PTCacheEdit *edit;
	PTCacheEditPoint *point;
	PTCacheEditKey *ekey;
	HairKey *key;
	BVHTreeFromMesh bvhtree;
	BVHTreeNearest nearest;
	MFace *mface;
	DerivedMesh *dm = NULL;
	int numverts;
	int i, k;
	float hairmat[4][4], imat[4][4];
	float v[4][3], vec[3];

	if(!psys || !psys->part || psys->part->type != PART_HAIR)
		return;
	
	edit= psys->edit;
	point=  edit ? edit->points : NULL;
	
	if(psmd->dm->deformedOnly)
		dm= psmd->dm;
	else
		dm= mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);

	numverts = dm->getNumVerts (dm);

	memset( &bvhtree, 0, sizeof(bvhtree) );

	/* convert to global coordinates */
	for (i=0; i<numverts; i++)
		Mat4MulVecfl (ob->obmat, CDDM_get_vert(dm, i)->co);

	bvhtree_from_mesh_faces(&bvhtree, dm, 0.0, 2, 6);

	for(i=0, pa= psys->particles; i<psys->totpart; i++,pa++) {
		key = pa->hair;

		nearest.index = -1;
		nearest.dist = FLT_MAX;

		BLI_bvhtree_find_nearest(bvhtree.tree, key->co, &nearest, bvhtree.nearest_callback, &bvhtree);

		if(nearest.index == -1) {
			printf("No nearest point found for hair root!");
			continue;
		}

		mface = CDDM_get_face(dm,nearest.index);

		VecCopyf(v[0], CDDM_get_vert(dm,mface->v1)->co);
		VecCopyf(v[1], CDDM_get_vert(dm,mface->v2)->co);
		VecCopyf(v[2], CDDM_get_vert(dm,mface->v3)->co);
		if(mface->v4) {
			VecCopyf(v[3], CDDM_get_vert(dm,mface->v4)->co);
			MeanValueWeights(v, 4, nearest.co, pa->fuv);
		}
		else
			MeanValueWeights(v, 3, nearest.co, pa->fuv);

		pa->num = nearest.index;
		pa->num_dmcache = psys_particle_dm_face_lookup(ob,psmd->dm,pa->num,pa->fuv,NULL);
		
		psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, hairmat);
		Mat4Invert(imat,hairmat);

		VECSUB(vec, nearest.co, key->co);

		if(point) {
			ekey = point->keys;
			point++;
		}

		for(k=0,key=pa->hair; k<pa->totkey; k++,key++) {
			VECADD(key->co, key->co, vec);
			Mat4MulVecfl(imat,key->co);

			if(ekey) {
				ekey->flag |= PEK_USE_WCO;
				ekey++;
			}
		}
	}

	free_bvhtree_from_mesh(&bvhtree);
	if(!psmd->dm->deformedOnly)
		dm->release(dm);

	psys_free_path_cache(psys, psys->edit);

	psys->flag &= ~PSYS_GLOBAL_HAIR;

	PE_update_object(scene, ob, 0);
}

static int connect_hair_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= NULL;
	int all = RNA_boolean_get(op->ptr, "all");

	if(!ob)
		return OPERATOR_CANCELLED;

	if(all) {
		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			connect_hair(scene, ob, psys);
		}
	}
	else {
		psys = ptr.data;
		connect_hair(scene, ob, psys);
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void PARTICLE_OT_connect_hair(wmOperatorType *ot)
{
	ot->name= "Connect Hair";
	ot->description= "Connect hair to the emitter mesh.";
	ot->idname= "PARTICLE_OT_connect_hair";
	
	ot->exec= connect_hair_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "all", 0, "All hair", "Connect all hair systems to the emitter mesh");
}

