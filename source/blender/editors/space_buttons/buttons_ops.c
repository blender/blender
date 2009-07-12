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

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_font.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_particle.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_world.h"

#include "BLI_editVert.h"
#include "BLI_listbase.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_curve.h"
#include "ED_mesh.h"

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
	
	/* api callbacks */
	ot->exec= material_slot_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int material_slot_assign_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
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

    DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
    WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_DATA, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_material_slot_assign(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Assign Material Slot";
	ot->idname= "OBJECT_OT_material_slot_assign";
	
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

    WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, ob);

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
	
	/* api callbacks */
	ot->exec= particle_system_remove_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new particle settings operator *********************/

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
		part= psys_new_settings("PSys", bmain);

	ob= ptr.id.data;

	if(psys->part)
		psys->part->id.us--;

	psys->part = part;

	DAG_scene_sort(scene);
	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Particle Settings";
	ot->idname= "PARTICLE_OT_new";
	
	/* api callbacks */
	ot->exec= new_particle_settings_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** keyed particle target operators *********************/

static int new_keyed_particle_target_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;

	KeyedParticleTarget *kpt;

	if(!psys)
		return OPERATOR_CANCELLED;

	kpt = psys->keyed_targets.first;
	for(; kpt; kpt=kpt->next)
		kpt->flag &= ~KEYED_TARGET_CURRENT;

	kpt = MEM_callocN(sizeof(KeyedParticleTarget), "keyed particle target");

	kpt->flag |= KEYED_TARGET_CURRENT;
	kpt->psys = 1;

	BLI_addtail(&psys->keyed_targets, kpt);

	DAG_scene_sort(scene);
	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_new_keyed_target(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Keyed Particle Target";
	ot->idname= "PARTICLE_OT_new_keyed_target";
	
	/* api callbacks */
	ot->exec= new_keyed_particle_target_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int remove_keyed_particle_target_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;

	KeyedParticleTarget *kpt;

	if(!psys)
		return OPERATOR_CANCELLED;

	kpt = psys->keyed_targets.first;
	for(; kpt; kpt=kpt->next) {
		if(kpt->flag & KEYED_TARGET_CURRENT) {
			BLI_remlink(&psys->keyed_targets, kpt);
			MEM_freeN(kpt);
			break;
		}

	}
	kpt = psys->keyed_targets.last;

	if(kpt)
		kpt->flag |= KEYED_TARGET_CURRENT;

	DAG_scene_sort(scene);
	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_remove_keyed_target(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Keyed Particle Target";
	ot->idname= "PARTICLE_OT_remove_keyed_target";
	
	/* api callbacks */
	ot->exec= remove_keyed_particle_target_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move up modifier operator *********************/

static int keyed_target_move_up_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;
	KeyedParticleTarget *kpt;

	if(!psys)
		return OPERATOR_CANCELLED;
	
	kpt = psys->keyed_targets.first;
	for(; kpt; kpt=kpt->next) {
		if(kpt->flag & KEYED_TARGET_CURRENT && kpt->prev) {
			BLI_remlink(&psys->keyed_targets, kpt);
			BLI_insertlink(&psys->keyed_targets, kpt->prev->prev, kpt);

			DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_keyed_target_move_up(wmOperatorType *ot)
{
	ot->name= "Move Up Keyed Target";
	ot->description= "Move keyed particle target up in the list.";
	ot->idname= "PARTICLE_OT_keyed_target_move_up";

	ot->exec= keyed_target_move_up_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move down modifier operator *********************/

static int keyed_target_move_down_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	PointerRNA ptr = CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem);
	ParticleSystem *psys= ptr.data;
	Object *ob = ptr.id.data;
	KeyedParticleTarget *kpt;

	if(!psys)
		return OPERATOR_CANCELLED;
	kpt = psys->keyed_targets.first;
	for(; kpt; kpt=kpt->next) {
		if(kpt->flag & KEYED_TARGET_CURRENT && kpt->next) {
			BLI_remlink(&psys->keyed_targets, kpt);
			BLI_insertlink(&psys->keyed_targets, kpt->next, kpt);

			DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

void PARTICLE_OT_keyed_target_move_down(wmOperatorType *ot)
{
	ot->name= "Move Down Keyed Target";
	ot->description= "Move keyed particle target down in the list.";
	ot->idname= "PARTICLE_OT_keyed_target_move_down";

	ot->exec= keyed_target_move_down_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

