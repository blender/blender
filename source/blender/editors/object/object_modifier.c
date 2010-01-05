/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_path_util.h"

#include "BKE_action.h"
#include "BKE_curve.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_report.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/******************************** API ****************************/

ModifierData *ED_object_modifier_add(ReportList *reports, Scene *scene, Object *ob, char *name, int type)
{
	ModifierData *md=NULL, *new_md=NULL;
	ModifierTypeInfo *mti = modifierType_getInfo(type);

	if(mti->flags&eModifierTypeFlag_Single) {
		if(modifiers_findByType(ob, type)) {
			BKE_report(reports, RPT_WARNING, "Only one modifier of this type allowed.");
			return NULL;
		}
	}

	if(type == eModifierType_ParticleSystem) {
		/* don't need to worry about the new modifier's name, since that is set to the number
		 * of particle systems which shouldn't have too many duplicates 
		 */
		new_md = object_add_particle_system(scene, ob, name);
	}
	else {
		/* get new modifier data to add */
		new_md= modifier_new(type);
		
		if(mti->flags&eModifierTypeFlag_RequiresOriginalData) {
			md = ob->modifiers.first;
			
			while(md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform)
				md = md->next;
			
			BLI_insertlinkbefore(&ob->modifiers, md, new_md);
		}
		else
			BLI_addtail(&ob->modifiers, new_md);

		if(name)
			BLI_strncpy(new_md->name, name, sizeof(new_md->name));

		/* make sure modifier data has unique name */

		modifier_unique_name(&ob->modifiers, new_md);
		
		/* special cases */
		if(type == eModifierType_Softbody) {
			if(!ob->soft) {
				ob->soft= sbNew(scene);
				ob->softflag |= OB_SB_GOAL|OB_SB_EDGES;
			}
		}
		else if(type == eModifierType_Collision) {
			if(!ob->pd)
				ob->pd= object_add_collision_fields(0);
			
			ob->pd->deflect= 1;
			DAG_scene_sort(scene);
		}
		else if(type == eModifierType_Surface)
			DAG_scene_sort(scene);
	}

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	return new_md;
}

int ED_object_modifier_remove(ReportList *reports, Scene *scene, Object *ob, ModifierData *md)
{
	ModifierData *obmd;

	/* It seems on rapid delete it is possible to
	 * get called twice on same modifier, so make
	 * sure it is in list. */
	for(obmd=ob->modifiers.first; obmd; obmd=obmd->next)
		if(obmd==md)
			break;
	
	if(!obmd) {
		BKE_reportf(reports, RPT_ERROR, "Modifier '%s' not in object '%s'.", ob->id.name, md->name);
		return 0;
	}

	/* special cases */
	if(md->type == eModifierType_ParticleSystem) {
		ParticleSystemModifierData *psmd=(ParticleSystemModifierData*)md;

		BLI_remlink(&ob->particlesystem, psmd->psys);
		psys_free(ob, psmd->psys);
		psmd->psys= NULL;
	}
	else if(md->type == eModifierType_Softbody) {
		if(ob->soft) {
			sbFree(ob->soft);
			ob->soft= NULL;
			ob->softflag= 0;
		}
	}
	else if(md->type == eModifierType_Collision) {
		if(ob->pd)
			ob->pd->deflect= 0;

        DAG_scene_sort(scene);
	}
	else if(md->type == eModifierType_Surface) {
		if(ob->pd && ob->pd->shape == PFIELD_SHAPE_SURFACE)
			ob->pd->shape = PFIELD_SHAPE_PLANE;

        DAG_scene_sort(scene);
	}
	else if(md->type == eModifierType_Smoke) {
		ob->dt = OB_TEXTURE;
	}
	else if(md->type == eModifierType_Multires) {
		Mesh *me= ob->data;

		CustomData_external_remove(&me->fdata, &me->id, CD_MDISPS, me->totface);
		CustomData_free_layer_active(&me->fdata, CD_MDISPS, me->totface);
	}

	BLI_remlink(&ob->modifiers, md);
	modifier_free(md);

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	return 1;
}

int ED_object_modifier_move_up(ReportList *reports, Object *ob, ModifierData *md)
{
	if(md->prev) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if(mti->type!=eModifierTypeType_OnlyDeform) {
			ModifierTypeInfo *nmti = modifierType_getInfo(md->prev->type);

			if(nmti->flags&eModifierTypeFlag_RequiresOriginalData) {
				BKE_report(reports, RPT_WARNING, "Cannot move above a modifier requiring original data.");
				return 0;
			}
		}

		BLI_remlink(&ob->modifiers, md);
		BLI_insertlink(&ob->modifiers, md->prev->prev, md);
	}

	return 1;
}

int ED_object_modifier_move_down(ReportList *reports, Object *ob, ModifierData *md)
{
	if(md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if(mti->flags&eModifierTypeFlag_RequiresOriginalData) {
			ModifierTypeInfo *nmti = modifierType_getInfo(md->next->type);

			if(nmti->type!=eModifierTypeType_OnlyDeform) {
				BKE_report(reports, RPT_WARNING, "Cannot move beyond a non-deforming modifier.");
				return 0;
			}
		}

		BLI_remlink(&ob->modifiers, md);
		BLI_insertlink(&ob->modifiers, md->next, md);
	}

	return 1;
}

int ED_object_modifier_convert(ReportList *reports, Scene *scene, Object *ob, ModifierData *md)
{
	Object *obn;
	ParticleSystem *psys;
	ParticleCacheKey *key, **cache;
	ParticleSettings *part;
	Mesh *me;
	MVert *mvert;
	MEdge *medge;
	int a, k, kmax;
	int totvert=0, totedge=0, cvert=0;
	int totpart=0, totchild=0;

	if(md->type != eModifierType_ParticleSystem) return 0;
	if(ob && ob->mode & OB_MODE_PARTICLE_EDIT) return 0;

	psys=((ParticleSystemModifierData *)md)->psys;
	part= psys->part;

	if(part->ren_as == PART_DRAW_GR || part->ren_as == PART_DRAW_OB) {
		; // XXX make_object_duplilist_real(NULL);
	}
	else {
		if(part->ren_as != PART_DRAW_PATH || psys->pathcache == 0)
			return 0;

		totpart= psys->totcached;
		totchild= psys->totchildcache;

		if(totchild && (part->draw&PART_DRAW_PARENT)==0)
			totpart= 0;

		/* count */
		cache= psys->pathcache;
		for(a=0; a<totpart; a++) {
			key= cache[a];
			totvert+= key->steps+1;
			totedge+= key->steps;
		}

		cache= psys->childcache;
		for(a=0; a<totchild; a++) {
			key= cache[a];
			totvert+= key->steps+1;
			totedge+= key->steps;
		}

		if(totvert==0) return 0;

		/* add new mesh */
		obn= add_object(scene, OB_MESH);
		me= obn->data;
		
		me->totvert= totvert;
		me->totedge= totedge;
		
		me->mvert= CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, totvert);
		me->medge= CustomData_add_layer(&me->edata, CD_MEDGE, CD_CALLOC, NULL, totedge);
		me->mface= CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, 0);
		
		mvert= me->mvert;
		medge= me->medge;

		/* copy coordinates */
		cache= psys->pathcache;
		for(a=0; a<totpart; a++) {
			key= cache[a];
			kmax= key->steps;
			for(k=0; k<=kmax; k++,key++,cvert++,mvert++) {
				VECCOPY(mvert->co,key->co);
				if(k) {
					medge->v1= cvert-1;
					medge->v2= cvert;
					medge->flag= ME_EDGEDRAW|ME_EDGERENDER|ME_LOOSEEDGE;
					medge++;
				}
			}
		}

		cache=psys->childcache;
		for(a=0; a<totchild; a++) {
			key=cache[a];
			kmax=key->steps;
			for(k=0; k<=kmax; k++,key++,cvert++,mvert++) {
				VECCOPY(mvert->co,key->co);
				if(k) {
					medge->v1=cvert-1;
					medge->v2=cvert;
					medge->flag= ME_EDGEDRAW|ME_EDGERENDER|ME_LOOSEEDGE;
					medge++;
				}
			}
		}
	}

	DAG_scene_sort(scene);

	return 1;
}

static int modifier_apply_shape(ReportList *reports, Scene *scene, Object *ob, ModifierData *md)
{
	if (ob->type==OB_MESH) {
		DerivedMesh *dm;
		Mesh *me= ob->data;
		Key *key=me->key;
		KeyBlock *kb;
		
		if(!modifier_sameTopology(md)) {
			BKE_report(reports, RPT_ERROR, "Only deforming modifiers can be applied to Shapes");
			return 0;
		}
		mesh_pmv_off(ob, me);
		
		dm = mesh_create_derived_for_modifier(scene, ob, md);
		if (!dm) {
			BKE_report(reports, RPT_ERROR, "Modifier is disabled or returned error, skipping apply");
			return 0;
		}
		
		if(key == NULL) {
			key= me->key= add_key((ID *)me);
			key->type= KEY_RELATIVE;
			/* if that was the first key block added, then it was the basis.
			 * Initialise it with the mesh, and add another for the modifier */
			kb= add_keyblock(key, NULL);
			mesh_to_key(me, kb);
		}

		kb= add_keyblock(key, md->name);
		DM_to_meshkey(dm, me, kb);
		
		dm->release(dm);
	}
	else {
		BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
		return 0;
	}
	return 1;
}

static int modifier_apply_obdata(ReportList *reports, Scene *scene, Object *ob, ModifierData *md)
{
	if (ob->type==OB_MESH) {
		DerivedMesh *dm;
		Mesh *me = ob->data;
		if( me->key) {
			BKE_report(reports, RPT_ERROR, "Modifier cannot be applied to Mesh with Shape Keys");
			return 0;
		}
		
		mesh_pmv_off(ob, me);
		
		/* Multires: ensure that recent sculpting is applied */
		if(md->type == eModifierType_Multires)
			multires_force_update(ob);
		
		dm = mesh_create_derived_for_modifier(scene, ob, md);
		if (!dm) {
			BKE_report(reports, RPT_ERROR, "Modifier is disabled or returned error, skipping apply");
			return 0;
		}
		
		DM_to_mesh(dm, me);
		
		dm->release(dm);
	} 
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		Curve *cu = ob->data;
		int numVerts;
		float (*vertexCos)[3];
		
		
		BKE_report(reports, RPT_INFO, "Applied modifier only changed CV points, not tesselated/bevel vertices");
		
		if (!(md->mode&eModifierMode_Realtime) || (mti->isDisabled && mti->isDisabled(md, 0))) {
			BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
			return 0;
		}
		
		vertexCos = curve_getVertexCos(cu, &cu->nurb, &numVerts);
		mti->deformVerts(md, ob, NULL, vertexCos, numVerts, 0, 0);
		curve_applyVertexCos(cu, &cu->nurb, vertexCos);

		MEM_freeN(vertexCos);
		
		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	}
	else {
		BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
		return 0;
	}
	return 1;
}

int ED_object_modifier_apply(ReportList *reports, Scene *scene, Object *ob, ModifierData *md, int mode)
{
	if (scene->obedit) {
		BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied in editmode");
		return 0;
	} else if (((ID*) ob->data)->us>1) {
		BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied to multi-user data");
		return 0;
	}

	if (md!=ob->modifiers.first)
		BKE_report(reports, RPT_INFO, "Applied modifier was not first, result may not be as expected.");

	if (mode == MODIFIER_APPLY_SHAPE) {
		if (!modifier_apply_shape(reports, scene, ob, md))
			return 0;
	} else {
		if (!modifier_apply_obdata(reports, scene, ob, md))
			return 0;
	}

	BLI_remlink(&ob->modifiers, md);
	modifier_free(md);

	return 1;
}

int ED_object_modifier_copy(ReportList *reports, Object *ob, ModifierData *md)
{
	ModifierData *nmd;
	
	nmd = modifier_new(md->type);
	modifier_copyData(md, nmd);
	BLI_insertlink(&ob->modifiers, md, nmd);
	modifier_unique_name(&ob->modifiers, nmd);

	return 1;
}

/***************************** OPERATORS ****************************/

static int modifier_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
	return (ptr.data != NULL && !((ID*)ptr.id.data)->lib);
}

/************************ add modifier operator *********************/

static int modifier_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
    Object *ob = CTX_data_active_object(C);
	int type= RNA_enum_get(op->ptr, "type");

	if(!ED_object_modifier_add(op->reports, scene, ob, NULL, type))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

static EnumPropertyItem *modifier_add_itemf(bContext *C, PointerRNA *ptr, int *free)
{	
	Object *ob= CTX_data_active_object(C);
	EnumPropertyItem *item= NULL, *md_item;
	ModifierTypeInfo *mti;
	int totitem= 0, a;
	
	if(!ob)
		return modifier_type_items;

	for(a=0; modifier_type_items[a].identifier; a++) {
		md_item= &modifier_type_items[a];

		if(md_item->identifier[0]) {
			mti= modifierType_getInfo(md_item->value);

			if(mti->flags & eModifierTypeFlag_NoUserAdd)
				continue;

			if(!((mti->flags & eModifierTypeFlag_AcceptsCVs) ||
			   (ob->type==OB_MESH && (mti->flags & eModifierTypeFlag_AcceptsMesh))))
				continue;
		}

		RNA_enum_item_add(&item, &totitem, md_item);
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

void OBJECT_OT_modifier_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Add Modifier";
	ot->description = "Add a modifier to the active object.";
	ot->idname= "OBJECT_OT_modifier_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= modifier_add_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "type", modifier_type_items, eModifierType_Subsurf, "Type", "");
	RNA_def_enum_funcs(prop, modifier_add_itemf);
}

/************************ remove modifier operator *********************/

static int modifier_remove_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
	Object *ob= ptr.id.data;
	ModifierData *md= ptr.data;

	if(!ED_object_modifier_remove(op->reports, scene, ob, md))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_modifier_remove(wmOperatorType *ot)
{
	ot->name= "Remove Modifier";
	ot->description= "Remove a modifier from the active object.";
	ot->idname= "OBJECT_OT_modifier_remove";

	ot->exec= modifier_remove_exec;
	ot->poll= modifier_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move up modifier operator *********************/

static int modifier_move_up_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
	Object *ob= ptr.id.data;
	ModifierData *md= ptr.data;

	if(!ED_object_modifier_move_up(op->reports, ob, md))
		return OPERATOR_CANCELLED;

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_modifier_move_up(wmOperatorType *ot)
{
	ot->name= "Move Up Modifier";
	ot->description= "Move modifier up in the stack.";
	ot->idname= "OBJECT_OT_modifier_move_up";

	ot->exec= modifier_move_up_exec;
	ot->poll= modifier_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ move down modifier operator *********************/

static int modifier_move_down_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
	Object *ob= ptr.id.data;
	ModifierData *md= ptr.data;

	if(!ob || !md || !ED_object_modifier_move_down(op->reports, ob, md))
		return OPERATOR_CANCELLED;

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_modifier_move_down(wmOperatorType *ot)
{
	ot->name= "Move Down Modifier";
	ot->description= "Move modifier down in the stack.";
	ot->idname= "OBJECT_OT_modifier_move_down";

	ot->exec= modifier_move_down_exec;
	ot->poll= modifier_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ apply modifier operator *********************/

static int modifier_apply_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
	Object *ob= ptr.id.data;
	ModifierData *md= ptr.data;
	int apply_as= RNA_enum_get(op->ptr, "apply_as");

	if(!ob || !md || !ED_object_modifier_apply(op->reports, scene, ob, md, apply_as))
		return OPERATOR_CANCELLED;

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

static EnumPropertyItem modifier_apply_as_items[] = {
	{MODIFIER_APPLY_DATA, "DATA", 0, "Object Data", "Apply modifier to the object's data"},
	{MODIFIER_APPLY_SHAPE, "SHAPE", 0, "New Shape", "Apply deform-only modifier to a new shape on this object"},
	{0, NULL, 0, NULL, NULL}};

void OBJECT_OT_modifier_apply(wmOperatorType *ot)
{
	ot->name= "Apply Modifier";
	ot->description= "Apply modifier and remove from the stack.";
	ot->idname= "OBJECT_OT_modifier_apply";

	//ot->invoke= WM_menu_invoke;
	ot->exec= modifier_apply_exec;
	ot->poll= modifier_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "apply_as", modifier_apply_as_items, MODIFIER_APPLY_DATA, "Apply as", "How to apply the modifier to the geometry");
}

/************************ convert modifier operator *********************/

static int modifier_convert_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
	Object *ob= ptr.id.data;
	ModifierData *md= ptr.data;

	if(!ob || !md || !ED_object_modifier_convert(op->reports, scene, ob, md))
		return OPERATOR_CANCELLED;

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_modifier_convert(wmOperatorType *ot)
{
	ot->name= "Convert Modifier";
	ot->description= "Convert particles to a mesh object.";
	ot->idname= "OBJECT_OT_modifier_convert";

	ot->exec= modifier_convert_exec;
	ot->poll= modifier_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ copy modifier operator *********************/

static int modifier_copy_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_Modifier);
	Object *ob= ptr.id.data;
	ModifierData *md= ptr.data;

	if(!ob || !md || !ED_object_modifier_copy(op->reports, ob, md))
		return OPERATOR_CANCELLED;

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_modifier_copy(wmOperatorType *ot)
{
	ot->name= "Copy Modifier";
	ot->description= "Duplicate modifier at the same position in the stack.";
	ot->idname= "OBJECT_OT_modifier_copy";

	ot->exec= modifier_copy_exec;
	ot->poll= modifier_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************* multires delete higher levels operator ****************/

static int multires_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_MultiresModifier);
	ID *id= ptr.id.data;
	return (ptr.data && id && !id->lib);
}

static int multires_higher_levels_delete_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_MultiresModifier);
	Object *ob= ptr.id.data;
	MultiresModifierData *mmd= ptr.data;

	if(mmd) {
		multiresModifier_del_levels(mmd, ob, 1);
		WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	}
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_multires_higher_levels_delete(wmOperatorType *ot)
{
	ot->name= "Delete Higher Levels";
	ot->idname= "OBJECT_OT_multires_higher_levels_delete";

	ot->poll= multires_poll;
	ot->exec= multires_higher_levels_delete_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/****************** multires subdivide operator *********************/

static int multires_subdivide_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_MultiresModifier);
	Object *ob= ptr.id.data;
	MultiresModifierData *mmd= ptr.data;

	multiresModifier_subdivide(mmd, ob, 0, mmd->simple);

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_multires_subdivide(wmOperatorType *ot)
{
	ot->name= "Multires Subdivide";
	ot->description= "Add a new level of subdivision.";
	ot->idname= "OBJECT_OT_multires_subdivide";

	ot->poll= multires_poll;
	ot->exec= multires_subdivide_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/****************** multires reshape operator *********************/

static int multires_reshape_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_MultiresModifier);
	Object *ob= ptr.id.data, *secondob= NULL;
	MultiresModifierData *mmd= ptr.data;

	CTX_DATA_BEGIN(C, Object*, selob, selected_editable_objects) {
		if(selob->type == OB_MESH && selob != ob) {
			secondob= selob;
			break;
		}
	}
	CTX_DATA_END;

	if(!secondob) {
		BKE_report(op->reports, RPT_ERROR, "Second selected mesh object require to copy shape from.");
		return OPERATOR_CANCELLED;
	}
	
	if(!multiresModifier_reshape(mmd, ob, secondob)) {
		BKE_report(op->reports, RPT_ERROR, "Objects do not have the same number of vertices.");
		return OPERATOR_CANCELLED;
	}

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_multires_reshape(wmOperatorType *ot)
{
	ot->name= "Multires Reshape";
	ot->description= "Copy vertex coordinates from other object.";
	ot->idname= "OBJECT_OT_multires_reshape";

	ot->poll= multires_poll;
	ot->exec= multires_reshape_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/****************** multires save external operator *********************/

static int multires_save_external_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_MultiresModifier);
	Object *ob= ptr.id.data;
	Mesh *me= (ob)? ob->data: op->customdata;
	char path[FILE_MAX];

	if(CustomData_external_test(&me->fdata, CD_MDISPS))
		return OPERATOR_CANCELLED;
	
	RNA_string_get(op->ptr, "path", path);
	if(G.save_over)
		BLI_makestringcode(G.sce, path); /* make relative */

	CustomData_external_add(&me->fdata, &me->id, CD_MDISPS, me->totface, path);
	CustomData_external_write(&me->fdata, &me->id, CD_MASK_MESH, me->totface, 0);
	
	return OPERATOR_FINISHED;
}

static int multires_save_external_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_MultiresModifier);
	Object *ob= ptr.id.data;
	Mesh *me= ob->data;
	char path[FILE_MAX];

	if(CustomData_external_test(&me->fdata, CD_MDISPS))
		return OPERATOR_CANCELLED;

	if(RNA_property_is_set(op->ptr, "path"))
		return multires_save_external_exec(C, op);
	
	op->customdata= me;

	BLI_snprintf(path, sizeof(path), "//%s.btx", me->id.name+2);
	RNA_string_set(op->ptr, "path", path);
	
	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

void OBJECT_OT_multires_save_external(wmOperatorType *ot)
{
	ot->name= "Multires Save External";
	ot->description= "Save displacements to an external file.";
	ot->idname= "OBJECT_OT_multires_save_external";

	ot->poll= multires_poll;
	ot->exec= multires_save_external_exec;
	ot->invoke= multires_save_external_invoke;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	WM_operator_properties_filesel(ot, FOLDERFILE|BTXFILE, FILE_SPECIAL);
}

/****************** multires pack operator *********************/

static int multires_pack_external_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_MultiresModifier);
	Object *ob= ptr.id.data;
	Mesh *me= ob->data;

	if(!CustomData_external_test(&me->fdata, CD_MDISPS))
		return OPERATOR_CANCELLED;

	// XXX don't remove..
	CustomData_external_remove(&me->fdata, &me->id, CD_MDISPS, me->totface);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_multires_pack_external(wmOperatorType *ot)
{
	ot->name= "Multires Pack External";
	ot->description= "Pack displacements from an external file.";
	ot->idname= "OBJECT_OT_multires_pack_external";

	ot->poll= multires_poll;
	ot->exec= multires_pack_external_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ mdef bind operator *********************/

static int meshdeform_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_MeshDeformModifier);
	ID *id= ptr.id.data;
	return (ptr.data && id && !id->lib);
}

static int meshdeform_bind_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	PointerRNA ptr= CTX_data_pointer_get(C, "modifier");
	Object *ob= ptr.id.data;
	MeshDeformModifierData *mmd= ptr.data;

	if(mmd->bindcos) {
		if(mmd->bindweights) MEM_freeN(mmd->bindweights);
		if(mmd->bindcos) MEM_freeN(mmd->bindcos);
		if(mmd->dyngrid) MEM_freeN(mmd->dyngrid);
		if(mmd->dyninfluences) MEM_freeN(mmd->dyninfluences);
		if(mmd->dynverts) MEM_freeN(mmd->dynverts);
		mmd->bindweights= NULL;
		mmd->bindcos= NULL;
		mmd->dyngrid= NULL;
		mmd->dyninfluences= NULL;
		mmd->dynverts= NULL;
		mmd->totvert= 0;
		mmd->totcagevert= 0;
		mmd->totinfluence= 0;
	}
	else {
		DerivedMesh *dm;
		int mode= mmd->modifier.mode;

		/* force modifier to run, it will call binding routine */
		mmd->bindfunc= mesh_deform_bind;
		mmd->modifier.mode |= eModifierMode_Realtime;

		if(ob->type == OB_MESH) {
			dm= mesh_create_derived_view(scene, ob, 0);
			dm->release(dm);
		}
		else if(ob->type == OB_LATTICE) {
			lattice_calc_modifiers(scene, ob);
		}
		else if(ob->type==OB_MBALL) {
			makeDispListMBall(scene, ob);
		}
		else if(ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
			makeDispListCurveTypes(scene, ob, 0);
		}

		mmd->bindfunc= NULL;
		mmd->modifier.mode= mode;
	}
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_meshdeform_bind(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mesh Deform Bind";
	ot->description = "Bind mesh to cage in mesh deform modifier.";
	ot->idname= "OBJECT_OT_meshdeform_bind";
	
	/* api callbacks */
	ot->poll= meshdeform_poll;
	ot->exec= meshdeform_bind_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/****************** explode refresh operator *********************/

static int explode_refresh_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_ExplodeModifier);
	ID *id= ptr.id.data;
	return (ptr.data && id && !id->lib);
}

static int explode_refresh_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_ExplodeModifier);
	Object *ob= ptr.id.data;
	ExplodeModifierData *emd= ptr.data;

	emd->flag |= eExplodeFlag_CalcFaces;

	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_explode_refresh(wmOperatorType *ot)
{
	ot->name= "Explode Refresh";
	ot->description= "Refresh data in the Explode modifier.";
	ot->idname= "OBJECT_OT_explode_refresh";

	ot->exec= explode_refresh_exec;
	ot->poll= explode_refresh_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

