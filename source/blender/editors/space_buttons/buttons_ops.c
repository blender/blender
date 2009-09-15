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

#include "DNA_boid_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "BKE_bvhutils.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_group.h"
#include "BKE_font.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_world.h"

#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_listbase.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_mesh.h"
#include "ED_particle.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "buttons_intern.h"	// own include


/********************** material slot operators *********************/

static int material_slot_add_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ob)
		return OPERATOR_CANCELLED;

	object_add_material_slot(ob);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Material Slot";
	ot->idname= "OBJECT_OT_material_slot_add";
	ot->description="Add a new material slot or duplicate the selected one.";
	
	/* api callbacks */
	ot->exec= material_slot_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int material_slot_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ob)
		return OPERATOR_CANCELLED;

	object_remove_material_slot(ob);
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Material Slot";
	ot->idname= "OBJECT_OT_material_slot_remove";
	ot->description="Remove the selected material slot.";
	
	/* api callbacks */
	ot->exec= material_slot_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int material_slot_assign_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ob)
		return OPERATOR_CANCELLED;

	if(ob && ob->actcol>0) {
		if(ob->type == OB_MESH) {
			EditMesh *em= ((Mesh*)ob->data)->edit_mesh;
			EditFace *efa;

			if(em) {
				for(efa= em->faces.first; efa; efa=efa->next)
					if(efa->f & SELECT)
						efa->mat_nr= ob->actcol-1;
			}
		}
		else if(ELEM(ob->type, OB_CURVE, OB_SURF)) {
			ListBase *editnurb= ((Curve*)ob->data)->editnurb;
			Nurb *nu;

			if(editnurb) {
				for(nu= editnurb->first; nu; nu= nu->next)
					if(isNurbsel(nu))
						nu->mat_nr= nu->charidx= ob->actcol-1;
			}
		}
		else if(ob->type == OB_FONT) {
			EditFont *ef= ((Curve*)ob->data)->editfont;
    		int i, selstart, selend;

			if(ef && BKE_font_getselection(ob, &selstart, &selend)) {
				for(i=selstart; i<=selend; i++)
					ef->textbufinfo[i].mat_nr = ob->actcol-1;
			}
		}
	}

    DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
    WM_event_add_notifier(C, NC_GEOM|ND_DATA, ob->data);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_assign(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Assign Material Slot";
	ot->idname= "OBJECT_OT_material_slot_assign";
	ot->description="Assign the material in the selected material slot to the selected vertices.";
	
	/* api callbacks */
	ot->exec= material_slot_assign_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int material_slot_de_select(bContext *C, int select)
{
	Object *ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;

	if(!ob)
		return OPERATOR_CANCELLED;

	if(ob->type == OB_MESH) {
		EditMesh *em= ((Mesh*)ob->data)->edit_mesh;

		if(em) {
			if(select)
				EM_select_by_material(em, ob->actcol-1);
			else
				EM_deselect_by_material(em, ob->actcol-1);
		}
	}
	else if ELEM(ob->type, OB_CURVE, OB_SURF) {
		ListBase *editnurb= ((Curve*)ob->data)->editnurb;
		Nurb *nu;
		BPoint *bp;
		BezTriple *bezt;
		int a;

		for(nu= editnurb->first; nu; nu=nu->next) {
			if(nu->mat_nr==ob->actcol-1) {
				if(nu->bezt) {
					a= nu->pntsu;
					bezt= nu->bezt;
					while(a--) {
						if(bezt->hide==0) {
							if(select) {
								bezt->f1 |= SELECT;
								bezt->f2 |= SELECT;
								bezt->f3 |= SELECT;
							}
							else {
								bezt->f1 &= ~SELECT;
								bezt->f2 &= ~SELECT;
								bezt->f3 &= ~SELECT;
							}
						}
						bezt++;
					}
				}
				else if(nu->bp) {
					a= nu->pntsu*nu->pntsv;
					bp= nu->bp;
					while(a--) {
						if(bp->hide==0) {
							if(select) bp->f1 |= SELECT;
							else bp->f1 &= ~SELECT;
						}
						bp++;
					}
				}
			}
		}
	}

    WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);

	return OPERATOR_FINISHED;
}

static int material_slot_select_exec(bContext *C, wmOperator *op)
{
	return material_slot_de_select(C, 1);
}

void OBJECT_OT_material_slot_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Material Slot";
	ot->idname= "OBJECT_OT_material_slot_select";
	ot->description="Select vertices assigned to the selected material slot.";
	
	/* api callbacks */
	ot->exec= material_slot_select_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int material_slot_deselect_exec(bContext *C, wmOperator *op)
{
	return material_slot_de_select(C, 0);
}

void OBJECT_OT_material_slot_deselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Deselect Material Slot";
	ot->idname= "OBJECT_OT_material_slot_deselect";
	ot->description="Deselect vertices assigned to the selected material slot.";
	
	/* api callbacks */
	ot->exec= material_slot_deselect_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new material operator *********************/

static int new_material_exec(bContext *C, wmOperator *op)
{
	Material *ma= CTX_data_pointer_get_type(C, "material", &RNA_Material).data;
	Object *ob;
	PointerRNA ptr;
	int index;

	/* add or copy material */
	if(ma)
		ma= copy_material(ma);
	else
		ma= add_material("Material");

	ma->id.us--; /* compensating for us++ in assign_material */

	/* attempt to assign to material slot */
	ptr= CTX_data_pointer_get_type(C, "material_slot", &RNA_MaterialSlot);

	if(ptr.data) {
		ob= ptr.id.data;
		index= (Material**)ptr.data - ob->mat;

		assign_material(ob, ma, index+1);

		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}

	WM_event_add_notifier(C, NC_MATERIAL|NA_ADDED, ma);
	
	return OPERATOR_FINISHED;
}

void MATERIAL_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Material";
	ot->idname= "MATERIAL_OT_new";
	ot->description="Add a new material.";
	
	/* api callbacks */
	ot->exec= new_material_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new texture operator *********************/

static int new_texture_exec(bContext *C, wmOperator *op)
{
	Tex *tex= CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
	ID *id;
	MTex *mtex;
	PointerRNA ptr;

	/* add or copy texture */
	if(tex)
		tex= copy_texture(tex);
	else
		tex= add_texture("Texture");

	id_us_min(&tex->id);

	/* attempt to assign to texture slot */
	ptr= CTX_data_pointer_get_type(C, "texture_slot", &RNA_TextureSlot);

	if(ptr.data) {
		id= ptr.id.data;
		mtex= ptr.data;

		if(mtex) {
			if(mtex->tex)
				id_us_min(&mtex->tex->id);
			mtex->tex= tex;
			id_us_plus(&tex->id);
		}

		/* XXX nodes, notifier .. */
	}

	WM_event_add_notifier(C, NC_TEXTURE|NA_ADDED, tex);
	
	return OPERATOR_FINISHED;
}

void TEXTURE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Texture";
	ot->idname= "TEXTURE_OT_new";
	ot->description="Add a new texture.";
	
	/* api callbacks */
	ot->exec= new_texture_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new world operator *********************/

static int new_world_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	World *wo= CTX_data_pointer_get_type(C, "world", &RNA_World).data;

	/* add or copy world */
	if(wo)
		wo= copy_world(wo);
	else
		wo= add_world("World");

	/* assign to scene */
	if(scene->world)
		id_us_min(&scene->world->id);
	scene->world= wo;

	WM_event_add_notifier(C, NC_WORLD|NA_ADDED, wo);
	
	return OPERATOR_FINISHED;
}

void WORLD_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New World";
	ot->idname= "WORLD_OT_new";
	ot->description= "Add a new world.";
	
	/* api callbacks */
	ot->exec= new_world_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}



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
	ParticleData *pa = psys->particles;
	PTCacheEdit *edit = psys->edit;
	PTCacheEditPoint *point = edit ? edit->points : NULL;
	PTCacheEditKey *ekey = NULL;
	HairKey *key;
	int i, k;
	float hairmat[4][4];

	if(!ob || !psys || psys->flag & PSYS_GLOBAL_HAIR)
		return;

	if(!psys->part || psys->part->type != PART_HAIR)
		return;

	for(i=0; i<psys->totpart; i++,pa++) {
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
	ParticleData *pa = psys->particles;
	PTCacheEdit *edit = psys->edit;
	PTCacheEditPoint *point = edit ? edit->points : NULL;
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

	for(i=0; i<psys->totpart; i++,pa++) {
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

/********************** render layer operators *********************/

static int render_layer_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);

	scene_add_render_layer(scene);
	scene->r.actlay= BLI_countlist(&scene->r.layers) - 1;

	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS, scene);
	
	return OPERATOR_FINISHED;
}

void SCENE_OT_render_layer_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Render Layer";
	ot->idname= "SCENE_OT_render_layer_add";
	ot->description="Add a render layer.";
	
	/* api callbacks */
	ot->exec= render_layer_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int render_layer_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	SceneRenderLayer *rl;
	int act= scene->r.actlay;

	if(BLI_countlist(&scene->r.layers) <= 1)
		return OPERATOR_CANCELLED;
	
	rl= BLI_findlink(&scene->r.layers, scene->r.actlay);
	BLI_remlink(&scene->r.layers, rl);
	MEM_freeN(rl);

	scene->r.actlay= 0;
	
	if(scene->nodetree) {
		bNode *node;
		for(node= scene->nodetree->nodes.first; node; node= node->next) {
			if(node->type==CMP_NODE_R_LAYERS && node->id==NULL) {
				if(node->custom1==act)
					node->custom1= 0;
				else if(node->custom1>act)
					node->custom1--;
			}
		}
	}

	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS, scene);
	
	return OPERATOR_FINISHED;
}

void SCENE_OT_render_layer_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Render Layer";
	ot->idname= "SCENE_OT_render_layer_remove";
	ot->description="Remove the selected render layer.";
	
	/* api callbacks */
	ot->exec= render_layer_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** toolbox operator *********************/

static int toolbox_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bScreen *sc= CTX_wm_screen(C);
	SpaceButs *sbuts= CTX_wm_space_buts(C);
	PointerRNA ptr;
	uiPopupMenu *pup;
	uiLayout *layout;

	RNA_pointer_create(&sc->id, &RNA_SpaceProperties, sbuts, &ptr);

	pup= uiPupMenuBegin(C, "Align", 0);
	layout= uiPupMenuLayout(pup);
	uiItemsEnumR(layout, &ptr, "align");
	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void BUTTONS_OT_toolbox(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toolbox";
	ot->idname= "BUTTONS_OT_toolbox";
	ot->description="Toolbar panel? DOC_BROKEN";
	
	/* api callbacks */
	ot->invoke= toolbox_invoke;
}

/********************** filebrowse operator *********************/

typedef struct FileBrowseOp {
	PointerRNA ptr;
	PropertyRNA *prop;
} FileBrowseOp;

static int file_browse_exec(bContext *C, wmOperator *op)
{
	FileBrowseOp *fbo= op->customdata;
	char *str;
	
	if (RNA_property_is_set(op->ptr, "path")==0 || fbo==NULL)
		return OPERATOR_CANCELLED;
	
	str= RNA_string_get_alloc(op->ptr, "path", 0, 0);
	RNA_property_string_set(&fbo->ptr, fbo->prop, str);
	RNA_property_update(C, &fbo->ptr, fbo->prop);
	MEM_freeN(str);

	MEM_freeN(op->customdata);
	return OPERATOR_FINISHED;
}

static int file_browse_cancel(bContext *C, wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata= NULL;

	return OPERATOR_CANCELLED;
}

static int file_browse_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	FileBrowseOp *fbo;
	char *str;

	uiFileBrowseContextProperty(C, &ptr, &prop);

	if(!prop)
		return OPERATOR_CANCELLED;
	
	fbo= MEM_callocN(sizeof(FileBrowseOp), "FileBrowseOp");
	fbo->ptr= ptr;
	fbo->prop= prop;
	op->customdata= fbo;

	str= RNA_property_string_get_alloc(&ptr, prop, 0, 0);
	RNA_string_set(op->ptr, "filename", str);
	MEM_freeN(str);

	WM_event_add_fileselect(C, op); 
	
	return OPERATOR_RUNNING_MODAL;
}

void BUTTONS_OT_file_browse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "File Browse";
	ot->idname= "BUTTONS_OT_file_browse";
	ot->description="Open a file browser.";
	
	/* api callbacks */
	ot->invoke= file_browse_invoke;
	ot->exec= file_browse_exec;
	ot->cancel= file_browse_cancel;

	/* properties */
	WM_operator_properties_filesel(ot, 0, FILE_SPECIAL);
}

