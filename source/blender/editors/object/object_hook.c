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
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_hook.c
 *  \ingroup edobj
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_deform.h"
#include "BKE_tessmesh.h"

#include "RNA_define.h"
#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "ED_curve.h"
#include "ED_mesh.h"
#include "ED_screen.h"

#include "WM_types.h"
#include "WM_api.h"

#include "UI_resources.h"

#include "object_intern.h"

static int return_editmesh_indexar(BMEditMesh *em, int *tot, int **indexar, float *cent)
{
	BMVert *eve;
	BMIter iter;
	int *index, nr, totvert=0;
	
	BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) totvert++;
	}
	if (totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	zero_v3(cent);
	
	BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
			*index= nr; index++;
			add_v3_v3(cent, eve->co);
		}
		nr++;
	}
	
	mul_v3_fl(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static int return_editmesh_vgroup(Object *obedit, BMEditMesh *em, char *name, float *cent)
{
	zero_v3(cent);

	if (obedit->actdef) {
		const int defgrp_index= obedit->actdef-1;
		int totvert=0;

		MDeformVert *dvert;
		BMVert *eve;
		BMIter iter;

		/* find the vertices */
		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			dvert= CustomData_bmesh_get(&em->bm->vdata, eve->head.data, CD_MDEFORMVERT);

			if (dvert) {
				if (defvert_find_weight(dvert, defgrp_index) > 0.0f) {
					add_v3_v3(cent, eve->co);
					totvert++;
				}
			}
		}
		if (totvert) {
			bDeformGroup *dg = BLI_findlink(&obedit->defbase, defgrp_index);
			BLI_strncpy(name, dg->name, sizeof(dg->name));
			mul_v3_fl(cent, 1.0f/(float)totvert);
			return 1;
		}
	}
	
	return 0;
}	

static void select_editbmesh_hook(Object *ob, HookModifierData *hmd)
{
	Mesh *me= ob->data;
	BMEditMesh *em= me->edit_btmesh;
	BMVert *eve;
	BMIter iter;
	int index=0, nr=0;
	
	if (hmd->indexar == NULL)
		return;
	
	BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
		if (nr==hmd->indexar[index]) {
			BM_elem_select_set(em->bm, eve, TRUE);
			if (index < hmd->totindex-1) index++;
		}

		nr++;
	}

	EDBM_select_flush(em);
}

static int return_editlattice_indexar(Lattice *editlatt, int *tot, int **indexar, float *cent)
{
	BPoint *bp;
	int *index, nr, totvert=0, a;
	
	/* count */
	a= editlatt->pntsu*editlatt->pntsv*editlatt->pntsw;
	bp= editlatt->def;
	while (a--) {
		if (bp->f1 & SELECT) {
			if (bp->hide==0) totvert++;
		}
		bp++;
	}

	if (totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	zero_v3(cent);
	
	a= editlatt->pntsu*editlatt->pntsv*editlatt->pntsw;
	bp= editlatt->def;
	while (a--) {
		if (bp->f1 & SELECT) {
			if (bp->hide==0) {
				*index= nr; index++;
				add_v3_v3(cent, bp->vec);
			}
		}
		bp++;
		nr++;
	}
	
	mul_v3_fl(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static void select_editlattice_hook(Object *obedit, HookModifierData *hmd)
{
	Lattice *lt= obedit->data, *editlt;
	BPoint *bp;
	int index=0, nr=0, a;

	editlt= lt->editlatt->latt;
	/* count */
	a= editlt->pntsu*editlt->pntsv*editlt->pntsw;
	bp= editlt->def;
	while (a--) {
		if (hmd->indexar[index]==nr) {
			bp->f1 |= SELECT;
			if (index < hmd->totindex-1) index++;
		}
		nr++;
		bp++;
	}
}

static int return_editcurve_indexar(Object *obedit, int *tot, int **indexar, float *cent)
{
	ListBase *editnurb= object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int *index, a, nr, totvert=0;
	
	for (nu= editnurb->first; nu; nu= nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while (a--) {
				if (bezt->f1 & SELECT) totvert++;
				if (bezt->f2 & SELECT) totvert++;
				if (bezt->f3 & SELECT) totvert++;
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while (a--) {
				if (bp->f1 & SELECT) totvert++;
				bp++;
			}
		}
	}
	if (totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	zero_v3(cent);
	
	for (nu= editnurb->first; nu; nu= nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while (a--) {
				if (bezt->f1 & SELECT) {
					*index= nr; index++;
					add_v3_v3(cent, bezt->vec[0]);
				}
				nr++;
				if (bezt->f2 & SELECT) {
					*index= nr; index++;
					add_v3_v3(cent, bezt->vec[1]);
				}
				nr++;
				if (bezt->f3 & SELECT) {
					*index= nr; index++;
					add_v3_v3(cent, bezt->vec[2]);
				}
				nr++;
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while (a--) {
				if (bp->f1 & SELECT) {
					*index= nr; index++;
					add_v3_v3(cent, bp->vec);
				}
				nr++;
				bp++;
			}
		}
	}
	
	mul_v3_fl(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static int object_hook_index_array(Scene *scene, Object *obedit, int *tot, int **indexar, char *name, float *cent_r)
{
	*indexar= NULL;
	*tot= 0;
	name[0]= 0;
	
	switch(obedit->type) {
		case OB_MESH:
		{
			Mesh *me= obedit->data;

			BMEditMesh *em;

			EDBM_mesh_load(obedit);
			EDBM_mesh_make(scene->toolsettings, scene, obedit);

			em = me->edit_btmesh;

			/* check selected vertices first */
			if ( return_editmesh_indexar(em, tot, indexar, cent_r)) {
				return 1;
			}
			else {
				int ret = return_editmesh_vgroup(obedit, em, name, cent_r);
				return ret;
			}
		}
		case OB_CURVE:
		case OB_SURF:
			return return_editcurve_indexar(obedit, tot, indexar, cent_r);
		case OB_LATTICE:
		{
			Lattice *lt= obedit->data;
			return return_editlattice_indexar(lt->editlatt->latt, tot, indexar, cent_r);
		}
		default:
			return 0;
	}
}

static void select_editcurve_hook(Object *obedit, HookModifierData *hmd)
{
	ListBase *editnurb= object_editcurve_get(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int index=0, a, nr=0;
	
	for (nu= editnurb->first; nu; nu= nu->next) {
		if (nu->type == CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while (a--) {
				if (nr == hmd->indexar[index]) {
					bezt->f1 |= SELECT;
					if (index<hmd->totindex-1) index++;
				}
				nr++;
				if (nr == hmd->indexar[index]) {
					bezt->f2 |= SELECT;
					if (index<hmd->totindex-1) index++;
				}
				nr++;
				if (nr == hmd->indexar[index]) {
					bezt->f3 |= SELECT;
					if (index<hmd->totindex-1) index++;
				}
				nr++;
				
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while (a--) {
				if (nr == hmd->indexar[index]) {
					bp->f1 |= SELECT;
					if (index<hmd->totindex-1) index++;
				}
				nr++;
				bp++;
			}
		}
	}
}

static void object_hook_select(Object *ob, HookModifierData *hmd) 
{
	if (hmd->indexar == NULL)
		return;
	
	if (ob->type==OB_MESH) select_editbmesh_hook(ob, hmd);
	else if (ob->type==OB_LATTICE) select_editlattice_hook(ob, hmd);
	else if (ob->type==OB_CURVE) select_editcurve_hook(ob, hmd);
	else if (ob->type==OB_SURF) select_editcurve_hook(ob, hmd);
}

/* special poll operators for hook operators */
// TODO: check for properties window modifier context too as alternative?
static int hook_op_edit_poll(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	
	if (obedit) {
		if (ED_operator_editmesh(C)) return 1;
		if (ED_operator_editsurfcurve(C)) return 1;
		if (ED_operator_editlattice(C)) return 1;
		//if (ED_operator_editmball(C)) return 1;
	}
	
	return 0;
}

static Object *add_hook_object_new(Scene *scene, Object *obedit)
{
	Base *base, *basedit;
	Object *ob;

	ob= add_object(scene, OB_EMPTY);
	
	basedit = object_in_scene(obedit, scene);
	base = object_in_scene(ob, scene);
	base->lay = ob->lay = obedit->lay;
	
	/* icky, add_object sets new base as active.
	 * so set it back to the original edit object */
	scene->basact = basedit;

	return ob;
}

static void add_hook_object(Main *bmain, Scene *scene, Object *obedit, Object *ob, int mode)
{
	ModifierData *md=NULL;
	HookModifierData *hmd = NULL;
	float cent[3];
	int tot, ok, *indexar;
	char name[MAX_NAME];
	
	ok = object_hook_index_array(scene, obedit, &tot, &indexar, name, cent);
	
	if (!ok) return;	// XXX error("Requires selected vertices or active Vertex Group");
	
	if (mode==OBJECT_ADDHOOK_NEWOB && !ob) {
		
		ob = add_hook_object_new(scene, obedit);
		
		/* transform cent to global coords for loc */
		mul_v3_m4v3(ob->loc, obedit->obmat, cent);
	}
	
	md = obedit->modifiers.first;
	while (md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform) {
		md = md->next;
	}
	
	hmd = (HookModifierData*) modifier_new(eModifierType_Hook);
	BLI_insertlinkbefore(&obedit->modifiers, md, hmd);
	BLI_snprintf(hmd->modifier.name, sizeof(hmd->modifier.name), "Hook-%s", ob->id.name+2);
	modifier_unique_name(&obedit->modifiers, (ModifierData*)hmd);
	
	hmd->object= ob;
	hmd->indexar= indexar;
	copy_v3_v3(hmd->cent, cent);
	hmd->totindex= tot;
	BLI_strncpy(hmd->name, name, sizeof(hmd->name));
	
	/* matrix calculus */
	/* vert x (obmat x hook->imat) x hook->obmat x ob->imat */
	/*        (parentinv         )                          */
	where_is_object(scene, ob);
	
	invert_m4_m4(ob->imat, ob->obmat);
	/* apparently this call goes from right to left... */
	mul_serie_m4(hmd->parentinv, ob->imat, obedit->obmat, NULL,
	             NULL, NULL, NULL, NULL, NULL);
	
	DAG_scene_sort(bmain, scene);
}

static int object_add_hook_selob_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Object *obsel=NULL;
	
	CTX_DATA_BEGIN(C, Object*, ob, selected_objects)
	{
		if (ob != obedit) {
			obsel = ob;
			break;
		}
	}
	CTX_DATA_END;
	
	if (!obsel) {
		BKE_report(op->reports, RPT_ERROR, "Can't add hook with no other selected objects");
		return OPERATOR_CANCELLED;
	}
	
	add_hook_object(bmain, scene, obedit, obsel, OBJECT_ADDHOOK_SELOB);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, obedit);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_hook_add_selobj(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hook to Selected Object";
	ot->description = "Hook selected vertices to the first selected Object";
	ot->idname = "OBJECT_OT_hook_add_selob";
	
	/* api callbacks */
	ot->exec = object_add_hook_selob_exec;
	ot->poll = hook_op_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_add_hook_newob_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);

	add_hook_object(bmain, scene, obedit, NULL, OBJECT_ADDHOOK_NEWOB);
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, obedit);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_hook_add_newobj(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hook to New Object";
	ot->description = "Hook selected vertices to the first selected Object";
	ot->idname = "OBJECT_OT_hook_add_newob";
	
	/* api callbacks */
	ot->exec = object_add_hook_newob_exec;
	ot->poll = hook_op_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_hook_remove_exec(bContext *C, wmOperator *op)
{
	int num= RNA_enum_get(op->ptr, "modifier");
	Object *ob=NULL;
	HookModifierData *hmd=NULL;

	ob = CTX_data_edit_object(C);
	hmd = (HookModifierData *)BLI_findlink(&ob->modifiers, num);

	if (!ob || !hmd) {
		BKE_report(op->reports, RPT_ERROR, "Couldn't find hook modifier");
		return OPERATOR_CANCELLED;
	}
	
	/* remove functionality */
	
	BLI_remlink(&ob->modifiers, (ModifierData *)hmd);
	modifier_free((ModifierData *)hmd);
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

static EnumPropertyItem *hook_mod_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), int *free)
{	
	Object *ob = CTX_data_edit_object(C);
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	EnumPropertyItem *item= NULL;
	ModifierData *md = NULL;
	int a, totitem= 0;
	
	if (!ob)
		return DummyRNA_NULL_items;
	
	for (a=0, md=ob->modifiers.first; md; md= md->next, a++) {
		if (md->type==eModifierType_Hook) {
			tmp.value= a;
			tmp.icon = ICON_HOOK;
			tmp.identifier= md->name;
			tmp.name= md->name;
			RNA_enum_item_add(&item, &totitem, &tmp);
		}
	}
	
	RNA_enum_item_end(&item, &totitem);
	*free= 1;
	
	return item;
}

void OBJECT_OT_hook_remove(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Remove Hook";
	ot->idname = "OBJECT_OT_hook_remove";
	ot->description = "Remove a hook from the active object";
	
	/* api callbacks */
	ot->exec = object_hook_remove_exec;
	ot->invoke = WM_menu_invoke;
	ot->poll = hook_op_edit_poll;
	
	/* flags */
	/* this operator removes modifier which isn't stored in local undo stack,
	 * so redoing it from redo panel gives totally weird results  */
	ot->flag = /*OPTYPE_REGISTER|*/OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "modifier", DummyRNA_NULL_items, 0, "Modifier", "Modifier number to remove");
	RNA_def_enum_funcs(prop, hook_mod_itemf);
	ot->prop = prop;
}

static int object_hook_reset_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_HookModifier);
	int num= RNA_enum_get(op->ptr, "modifier");
	Object *ob=NULL;
	HookModifierData *hmd=NULL;
	
	if (ptr.data) {		/* if modifier context is available, use that */
		ob = ptr.id.data;
		hmd= ptr.data;
	} 
	else {			/* use the provided property */
		ob = CTX_data_edit_object(C);
		hmd = (HookModifierData *)BLI_findlink(&ob->modifiers, num);
	}
	if (!ob || !hmd) {
		BKE_report(op->reports, RPT_ERROR, "Couldn't find hook modifier");
		return OPERATOR_CANCELLED;
	}
	
	/* reset functionality */
	if (hmd->object) {
		bPoseChannel *pchan= get_pose_channel(hmd->object->pose, hmd->subtarget);
		
		if (hmd->subtarget[0] && pchan) {
			float imat[4][4], mat[4][4];
			
			/* calculate the world-space matrix for the pose-channel target first, then carry on as usual */
			mult_m4_m4m4(mat, hmd->object->obmat, pchan->pose_mat);
			
			invert_m4_m4(imat, mat);
			mul_serie_m4(hmd->parentinv, imat, ob->obmat, NULL, NULL, NULL, NULL, NULL, NULL);
		}
		else {
			invert_m4_m4(hmd->object->imat, hmd->object->obmat);
			mul_serie_m4(hmd->parentinv, hmd->object->imat, ob->obmat, NULL, NULL, NULL, NULL, NULL, NULL);
		}
	}
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_hook_reset(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Reset Hook";
	ot->description = "Recalculate and clear offset transformation";
	ot->idname = "OBJECT_OT_hook_reset";
	
	/* callbacks */
	ot->exec = object_hook_reset_exec;
	ot->poll = hook_op_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "modifier", DummyRNA_NULL_items, 0, "Modifier", "Modifier number to assign to");
	RNA_def_enum_funcs(prop, hook_mod_itemf);
}

static int object_hook_recenter_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_HookModifier);
	int num= RNA_enum_get(op->ptr, "modifier");
	Object *ob=NULL;
	HookModifierData *hmd=NULL;
	Scene *scene = CTX_data_scene(C);
	float bmat[3][3], imat[3][3];
	
	if (ptr.data) {		/* if modifier context is available, use that */
		ob = ptr.id.data;
		hmd= ptr.data;
	} 
	else {			/* use the provided property */
		ob = CTX_data_edit_object(C);
		hmd = (HookModifierData *)BLI_findlink(&ob->modifiers, num);
	}
	if (!ob || !hmd) {
		BKE_report(op->reports, RPT_ERROR, "Couldn't find hook modifier");
		return OPERATOR_CANCELLED;
	}
	
	/* recenter functionality */
	copy_m3_m4(bmat, ob->obmat);
	invert_m3_m3(imat, bmat);
	
	sub_v3_v3v3(hmd->cent, scene->cursor, ob->obmat[3]);
	mul_m3_v3(imat, hmd->cent);
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_hook_recenter(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Recenter Hook";
	ot->description = "Set hook center to cursor position";
	ot->idname = "OBJECT_OT_hook_recenter";
	
	/* callbacks */
	ot->exec = object_hook_recenter_exec;
	ot->poll = hook_op_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "modifier", DummyRNA_NULL_items, 0, "Modifier", "Modifier number to assign to");
	RNA_def_enum_funcs(prop, hook_mod_itemf);
}

static int object_hook_assign_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_HookModifier);
	int num= RNA_enum_get(op->ptr, "modifier");
	Object *ob=NULL;
	HookModifierData *hmd=NULL;
	float cent[3];
	char name[MAX_NAME];
	int *indexar, tot;
	
	if (ptr.data) {		/* if modifier context is available, use that */
		ob = ptr.id.data;
		hmd= ptr.data;
	} 
	else {			/* use the provided property */
		ob = CTX_data_edit_object(C);
		hmd = (HookModifierData *)BLI_findlink(&ob->modifiers, num);
	}
	if (!ob || !hmd) {
		BKE_report(op->reports, RPT_ERROR, "Couldn't find hook modifier");
		return OPERATOR_CANCELLED;
	}
	
	/* assign functionality */
	
	if (!object_hook_index_array(scene, ob, &tot, &indexar, name, cent)) {
		BKE_report(op->reports, RPT_WARNING, "Requires selected vertices or active vertex group");
		return OPERATOR_CANCELLED;
	}
	if (hmd->indexar)
		MEM_freeN(hmd->indexar);
	
	copy_v3_v3(hmd->cent, cent);
	hmd->indexar= indexar;
	hmd->totindex= tot;
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_hook_assign(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Assign to Hook";
	ot->description = "Assign the selected vertices to a hook";
	ot->idname = "OBJECT_OT_hook_assign";
	
	/* callbacks */
	ot->exec = object_hook_assign_exec;
	ot->poll = hook_op_edit_poll;
	
	/* flags */
	/* this operator changes data stored in modifier which doesn't get pushed to undo stack,
	 * so redoing it from redo panel gives totally weird results  */
	ot->flag = /*OPTYPE_REGISTER|*/OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "modifier", DummyRNA_NULL_items, 0, "Modifier", "Modifier number to assign to");
	RNA_def_enum_funcs(prop, hook_mod_itemf);
}

static int object_hook_select_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "modifier", &RNA_HookModifier);
	int num= RNA_enum_get(op->ptr, "modifier");
	Object *ob=NULL;
	HookModifierData *hmd=NULL;
	
	if (ptr.data) {		/* if modifier context is available, use that */
		ob = ptr.id.data;
		hmd= ptr.data;
	} 
	else {			/* use the provided property */
		ob = CTX_data_edit_object(C);
		hmd = (HookModifierData *)BLI_findlink(&ob->modifiers, num);
	}
	if (!ob || !hmd) {
		BKE_report(op->reports, RPT_ERROR, "Couldn't find hook modifier");
		return OPERATOR_CANCELLED;
	}
	
	/* select functionality */
	object_hook_select(ob, hmd);
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, ob->data);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_hook_select(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Select Hook";
	ot->description = "Select affected vertices on mesh";
	ot->idname = "OBJECT_OT_hook_select";
	
	/* callbacks */
	ot->exec = object_hook_select_exec;
	ot->poll = hook_op_edit_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "modifier", DummyRNA_NULL_items, 0, "Modifier", "Modifier number to remove");
	RNA_def_enum_funcs(prop, hook_mod_itemf);
}

