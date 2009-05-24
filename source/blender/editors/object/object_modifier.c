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

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"

#include "BKE_curve.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_report.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"

/******************************** API ****************************/

int ED_object_modifier_delete(ReportList *reports, Object *ob, ModifierData *md)
{
	ModifierData *obmd;

	/* It seems on rapid delete it is possible to
	 * get called twice on same modifier, so make
	 * sure it is in list. */
	for (obmd=ob->modifiers.first; obmd; obmd=obmd->next)
		if (obmd==md)
			break;
	
	if (!obmd)
		return 0;

	if(md->type == eModifierType_ParticleSystem) {
		ParticleSystemModifierData *psmd=(ParticleSystemModifierData*)md;

		BLI_remlink(&ob->particlesystem, psmd->psys);
		psys_free(ob, psmd->psys);
	}

	BLI_remlink(&ob->modifiers, md);

	modifier_free(md);

	return 1;
}

int ED_object_modifier_move_up(ReportList *reports, Object *ob, ModifierData *md)
{
	if(md->prev) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if(mti->type!=eModifierTypeType_OnlyDeform) {
			ModifierTypeInfo *nmti = modifierType_getInfo(md->prev->type);

			if(nmti->flags&eModifierTypeFlag_RequiresOriginalData)
				BKE_report(reports, RPT_WARNING, "Cannot move above a modifier requiring original data.");
				return 0;
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
	if(G.f & G_PARTICLEEDIT) return 0;

	psys=((ParticleSystemModifierData *)md)->psys;
	part= psys->part;

	if(part->draw_as == PART_DRAW_GR || part->draw_as == PART_DRAW_OB) {
		; // XXX make_object_duplilist_real(NULL);
	}
	else {
		if(part->draw_as != PART_DRAW_PATH || psys->pathcache == 0)
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

int ED_object_modifier_apply(ReportList *reports, Scene *scene, Object *ob, ModifierData *md)
{
	DerivedMesh *dm;
	Mesh *me = ob->data;
	int converted = 0;

	if (scene->obedit) {
		BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied in editmode");
		return 0;
	} else if (((ID*) ob->data)->us>1) {
		BKE_report(reports, RPT_ERROR, "Modifiers cannot be applied to multi-user data");
		return 0;
	}

	if (md!=ob->modifiers.first)
		BKE_report(reports, RPT_INFO, "Applied modifier was not first, result may not be as expected.");

	if (ob->type==OB_MESH) {
		if(me->key) {
			BKE_report(reports, RPT_ERROR, "Modifier cannot be applied to Mesh with Shape Keys");
			return 0;
		}
	
		mesh_pmv_off(ob, me);
	
		dm = mesh_create_derived_for_modifier(scene, ob, md);
		if (!dm) {
			BKE_report(reports, RPT_ERROR, "Modifier is disabled or returned error, skipping apply");
			return 0;
		}

		DM_to_mesh(dm, me);
		converted = 1;

		dm->release(dm);
	} 
	else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		Curve *cu = ob->data;
		int numVerts;
		float (*vertexCos)[3];

		BKE_report(reports, RPT_INFO, "Applied modifier only changed CV points, not tesselated/bevel vertices");

		if (!(md->mode&eModifierMode_Realtime) || (mti->isDisabled && mti->isDisabled(md))) {
			BKE_report(reports, RPT_ERROR, "Modifier is disabled, skipping apply");
			return 0;
		}

		vertexCos = curve_getVertexCos(cu, &cu->nurb, &numVerts);
		mti->deformVerts(md, ob, NULL, vertexCos, numVerts);
		curve_applyVertexCos(cu, &cu->nurb, vertexCos);

		converted = 1;

		MEM_freeN(vertexCos);

		DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	}
	else {
		BKE_report(reports, RPT_ERROR, "Cannot apply modifier for this object type");
		return 0;
	}

	if (converted) {
		BLI_remlink(&ob->modifiers, md);
		modifier_free(md);

		return 1;
	}

	return 0;
}

int ED_object_modifier_copy(ReportList *reports, Object *ob, ModifierData *md)
{
	ModifierData *nmd;
	
	nmd = modifier_new(md->type);
	modifier_copyData(md, nmd);
	BLI_insertlink(&ob->modifiers, md, nmd);

	return 1;
}

/***************************** OPERATORS ****************************/

/************************ add modifier operator *********************/

static int modifier_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
    Object *ob = CTX_data_active_object(C);
	ModifierData *md;
	int type= RNA_enum_get(op->ptr, "type");
	ModifierTypeInfo *mti = modifierType_getInfo(type);

	if(mti->flags&eModifierTypeFlag_RequiresOriginalData) {
		md = ob->modifiers.first;

		while(md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform)
			md = md->next;

		BLI_insertlinkbefore(&ob->modifiers, md, modifier_new(type));
	}
	else
		BLI_addtail(&ob->modifiers, modifier_new(type));

	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_modifier_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Modifier";
	ot->description = "Add a modifier to the active object.";
	ot->idname= "OBJECT_OT_modifier_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= modifier_add_exec;
	
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* XXX only some types should be here */
	RNA_def_enum(ot->srna, "type", modifier_type_items, 0, "Type", "");
}

static int multires_subdivide_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	MultiresModifierData *mmd = find_multires_modifier(ob);

	if(mmd) {
		multiresModifier_subdivide(mmd, ob, 1, 0, mmd->simple);
		WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	}
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_multires_subdivide(wmOperatorType *ot)
{
	ot->name= "Multires Subdivide";
	ot->description= "Add a new level of subdivision.";
	ot->idname= "OBJECT_OT_multires_subdivide";
	ot->poll= ED_operator_object_active;

	ot->exec= multires_subdivide_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

#if 0
static void modifiers_add(void *ob_v, int type)
{
	Object *ob = ob_v;
	ModifierTypeInfo *mti = modifierType_getInfo(type);
	
	if (mti->flags&eModifierTypeFlag_RequiresOriginalData) {
		ModifierData *md = ob->modifiers.first;

		while (md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform) {
			md = md->next;
		}

		BLI_insertlinkbefore(&ob->modifiers, md, modifier_new(type));
	} else {
		BLI_addtail(&ob->modifiers, modifier_new(type));
	}
	ED_undo_push("Add modifier");
}

typedef struct MenuEntry {
	char *name;
	int ID;
} MenuEntry;

static int menuEntry_compare_names(const void *entry1, const void *entry2)
{
	return strcmp(((MenuEntry *)entry1)->name, ((MenuEntry *)entry2)->name);
}

static uiBlock *modifiers_add_menu(void *ob_v)
{
	Object *ob = ob_v;
	uiBlock *block;
	int i, yco=0;
	int numEntries = 0;
	MenuEntry entries[NUM_MODIFIER_TYPES];
	
	block= uiNewBlock(&curarea->uiblocks, "modifier_add_menu",
	                  UI_EMBOSSP, UI_HELV, curarea->win);
	uiBlockSetButmFunc(block, modifiers_add, ob);

	for (i=eModifierType_None+1; i<NUM_MODIFIER_TYPES; i++) {
		ModifierTypeInfo *mti = modifierType_getInfo(i);

		/* Only allow adding through appropriate other interfaces */
		if(ELEM3(i, eModifierType_Softbody, eModifierType_Hook, eModifierType_ParticleSystem)) continue;
		
		if(ELEM4(i, eModifierType_Cloth, eModifierType_Collision, eModifierType_Surface, eModifierType_Fluidsim)) continue;

		if((mti->flags&eModifierTypeFlag_AcceptsCVs) ||
		   (ob->type==OB_MESH && (mti->flags&eModifierTypeFlag_AcceptsMesh))) {
			entries[numEntries].name = mti->name;
			entries[numEntries].ID = i;

			++numEntries;
		}
	}

	qsort(entries, numEntries, sizeof(*entries), menuEntry_compare_names);


	for(i = 0; i < numEntries; ++i)
		uiDefBut(block, BUTM, B_MODIFIER_RECALC, entries[i].name,
		         0, yco -= 20, 160, 19, NULL, 0, 0, 1, entries[i].ID, "");

	uiTextBoundsBlock(block, 50);
	uiBlockSetDirection(block, UI_DOWN);

	return block;
}
#endif

