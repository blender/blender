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

/** \file blender/editors/object/object_relations.c
 *  \ingroup edobj
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_mesh_types.h"
#include "DNA_constraint_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_speaker_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_fcurve.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_speaker.h"
#include "BKE_texture.h"
#include "BKE_tessmesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "object_intern.h"

/*********************** Make Vertex Parent Operator ************************/

static int vertex_parent_set_poll(bContext *C)
{
	return ED_operator_editmesh(C) || ED_operator_editsurfcurve(C) || ED_operator_editlattice(C);
}

static int vertex_parent_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	BMVert *eve;
	BMIter iter;
	Curve *cu;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	Object *par;
	int a, v1 = 0, v2 = 0, v3 = 0, v4 = 0, nr = 1;
	
	/* we need 1 to 3 selected vertices */
	
	if (obedit->type == OB_MESH) {
		Mesh *me = obedit->data;
		BMEditMesh *em;

		EDBM_mesh_load(obedit);
		EDBM_mesh_make(scene->toolsettings, scene, obedit);

		em = me->edit_btmesh;

		/* derivedMesh might be needed for solving parenting,
		 * so re-create it here */
		makeDerivedMesh(scene, obedit, em, CD_MASK_BAREMESH, 0);

		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
				if (v1 == 0) v1 = nr;
				else if (v2 == 0) v2 = nr;
				else if (v3 == 0) v3 = nr;
				else if (v4 == 0) v4 = nr;
				else break;
			}
			nr++;
		}
	}
	else if (ELEM(obedit->type, OB_SURF, OB_CURVE)) {
		ListBase *editnurb = object_editcurve_get(obedit);
		
		cu = obedit->data;

		nu = editnurb->first;
		while (nu) {
			if (nu->type == CU_BEZIER) {
				bezt = nu->bezt;
				a = nu->pntsu;
				while (a--) {
					if (BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
						if (v1 == 0) v1 = nr;
						else if (v2 == 0) v2 = nr;
						else if (v3 == 0) v3 = nr;
						else if (v4 == 0) v4 = nr;
						else break;
					}
					nr++;
					bezt++;
				}
			}
			else {
				bp = nu->bp;
				a = nu->pntsu * nu->pntsv;
				while (a--) {
					if (bp->f1 & SELECT) {
						if (v1 == 0) v1 = nr;
						else if (v2 == 0) v2 = nr;
						else if (v3 == 0) v3 = nr;
						else if (v4 == 0) v4 = nr;
						else break;
					}
					nr++;
					bp++;
				}
			}
			nu = nu->next;
		}
	}
	else if (obedit->type == OB_LATTICE) {
		Lattice *lt = obedit->data;
		
		a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
		bp = lt->editlatt->latt->def;
		while (a--) {
			if (bp->f1 & SELECT) {
				if (v1 == 0) v1 = nr;
				else if (v2 == 0) v2 = nr;
				else if (v3 == 0) v3 = nr;
				else if (v4 == 0) v4 = nr;
				else break;
			}
			nr++;
			bp++;
		}
	}
	
	if (v4 || !((v1 && v2 == 0 && v3 == 0) || (v1 && v2 && v3)) ) {
		BKE_report(op->reports, RPT_ERROR, "Select either 1 or 3 vertices to parent to");
		return OPERATOR_CANCELLED;
	}
	
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob != obedit) {
			ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
			par = obedit->parent;
			
			while (par) {
				if (par == ob) break;
				par = par->parent;
			}
			if (par) {
				BKE_report(op->reports, RPT_ERROR, "Loop in parents");
			}
			else {
				Object workob;
				
				ob->parent = BASACT->object;
				if (v3) {
					ob->partype = PARVERT3;
					ob->par1 = v1 - 1;
					ob->par2 = v2 - 1;
					ob->par3 = v3 - 1;

					/* inverse parent matrix */
					BKE_object_workob_calc_parent(scene, ob, &workob);
					invert_m4_m4(ob->parentinv, workob.obmat);
				}
				else {
					ob->partype = PARVERT1;
					ob->par1 = v1 - 1;

					/* inverse parent matrix */
					BKE_object_workob_calc_parent(scene, ob, &workob);
					invert_m4_m4(ob->parentinv, workob.obmat);
				}
			}
		}
	}
	CTX_DATA_END;
	
	DAG_scene_sort(bmain, scene);

	WM_event_add_notifier(C, NC_OBJECT, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Vertex Parent";
	ot->description = "Parent selected objects to the selected vertices";
	ot->idname = "OBJECT_OT_vertex_parent_set";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->poll = vertex_parent_set_poll;
	ot->exec = vertex_parent_set_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Make Proxy Operator *************************/

/* set the object to proxify */
static int make_proxy_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);
	
	/* sanity checks */
	if (!scene || scene->id.lib || !ob)
		return OPERATOR_CANCELLED;
		
	/* Get object to work on - use a menu if we need to... */
	if (ob->dup_group && ob->dup_group->id.lib) {
		/* gives menu with list of objects in group */
		//proxy_group_objects_menu(C, op, ob, ob->dup_group);
		WM_enum_search_invoke(C, op, evt);
		return OPERATOR_CANCELLED;

	}
	else if (ob->id.lib) {
		uiPopupMenu *pup = uiPupMenuBegin(C, "OK?", ICON_QUESTION);
		uiLayout *layout = uiPupMenuLayout(pup);
		
		/* create operator menu item with relevant properties filled in */
		uiItemFullO_ptr(layout, op->type, op->type->name, ICON_NONE, NULL, WM_OP_EXEC_REGION_WIN, UI_ITEM_O_RETURN_PROPS);
		
		/* present the menu and be done... */
		uiPupMenuEnd(C, pup);
	}
	else {
		/* error.. cannot continue */
		BKE_report(op->reports, RPT_ERROR, "Can only make proxy for a referenced object or group");
	}
	
	/* this invoke just calls another instance of this operator... */
	return OPERATOR_CANCELLED;
}

static int make_proxy_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *ob, *gob = ED_object_active_context(C);
	GroupObject *go;
	Scene *scene = CTX_data_scene(C);

	if (gob->dup_group != NULL) {
		go = BLI_findlink(&gob->dup_group->gobject, RNA_enum_get(op->ptr, "object"));
		ob = go->ob;
	}
	else {
		ob = gob;
		gob = NULL;
	}
	
	if (ob) {
		Object *newob;
		Base *newbase, *oldbase = BASACT;
		char name[MAX_ID_NAME + 4];
		
		/* Add new object for the proxy */
		newob = BKE_object_add(scene, OB_EMPTY);

		BLI_snprintf(name, sizeof(name), "%s_proxy", ((ID *)(gob ? gob : ob))->name + 2);

		rename_id(&newob->id, name);
		
		/* set layers OK */
		newbase = BASACT;    /* BKE_object_add sets active... */
		newbase->lay = oldbase->lay;
		newob->lay = newbase->lay;
		
		/* remove base, leave user count of object, it gets linked in BKE_object_make_proxy */
		if (gob == NULL) {
			BLI_remlink(&scene->base, oldbase);
			MEM_freeN(oldbase);
		}
		
		BKE_object_make_proxy(newob, ob, gob);
		
		/* depsgraph flushes are needed for the new data */
		DAG_scene_sort(bmain, scene);
		DAG_id_tag_update(&newob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, newob);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No object to make proxy for");
		return OPERATOR_CANCELLED;
	}
	
	return OPERATOR_FINISHED;
}

/* Generic itemf's for operators that take library args */
static EnumPropertyItem *proxy_group_object_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), int *free)
{
	EnumPropertyItem item_tmp = {0}, *item = NULL;
	int totitem = 0;
	int i = 0;
	Object *ob = ED_object_active_context(C);
	GroupObject *go;

	if (!ob || !ob->dup_group)
		return DummyRNA_DEFAULT_items;

	/* find the object to affect */
	for (go = ob->dup_group->gobject.first; go; go = go->next) {
		item_tmp.identifier = item_tmp.name = go->ob->id.name + 2;
		item_tmp.value = i++;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*free = 1;

	return item;
}

void OBJECT_OT_proxy_make(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Make Proxy";
	ot->idname = "OBJECT_OT_proxy_make";
	ot->description = "Add empty object to become local replacement data of a library-linked object";
	
	/* callbacks */
	ot->invoke = make_proxy_invoke;
	ot->exec = make_proxy_exec;
	ot->poll = ED_operator_object_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	prop = RNA_def_enum(ot->srna, "object", DummyRNA_DEFAULT_items, 0, "Proxy Object", "Name of lib-linked/grouped object to make a proxy for"); /* XXX, relies on hard coded ID at the moment */
	RNA_def_enum_funcs(prop, proxy_group_object_itemf);
	ot->prop = prop;
}

/********************** Clear Parent Operator ******************* */

EnumPropertyItem prop_clear_parent_types[] = {
	{0, "CLEAR", 0, "Clear Parent", ""},
	{1, "CLEAR_KEEP_TRANSFORM", 0, "Clear and Keep Transformation", ""},
	{2, "CLEAR_INVERSE", 0, "Clear Parent Inverse", ""},
	{0, NULL, 0, NULL, NULL}
};

void ED_object_parent_clear(bContext *C, int type)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob->parent == NULL)
			continue;
		
		if (type == 0) {
			ob->parent = NULL;
		}
		else if (type == 1) {
			ob->parent = NULL;
			BKE_object_apply_mat4(ob, ob->obmat, TRUE, FALSE);
		}
		else if (type == 2)
			unit_m4(ob->parentinv);
		
		ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
	}
	CTX_DATA_END;

	DAG_scene_sort(bmain, scene);
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
}

/* note, poll should check for editable scene */
static int parent_clear_exec(bContext *C, wmOperator *op)
{
	ED_object_parent_clear(C, RNA_enum_get(op->ptr, "type"));

	return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Parent";
	ot->description = "Clear the object's parenting";
	ot->idname = "OBJECT_OT_parent_clear";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = parent_clear_exec;
	
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_clear_parent_types, 0, "Type", "");
}

/* ******************** Make Parent Operator *********************** */

void ED_object_parent(Object *ob, Object *par, int type, const char *substr)
{
	if (!par || BKE_object_parent_loop_check(par, ob)) {
		ob->parent = NULL;
		ob->partype = PAROBJECT;
		ob->parsubstr[0] = 0;
		return;
	}

	/* this could use some more checks */

	ob->parent = par;
	ob->partype &= ~PARTYPE;
	ob->partype |= type;
	BLI_strncpy(ob->parsubstr, substr, sizeof(ob->parsubstr));
}

/* Operator Property */
EnumPropertyItem prop_make_parent_types[] = {
	{PAR_OBJECT, "OBJECT", 0, "Object", ""},
	{PAR_ARMATURE, "ARMATURE", 0, "Armature Deform", ""},
	{PAR_ARMATURE_NAME, "ARMATURE_NAME", 0, "   With Empty Groups", ""},
	{PAR_ARMATURE_AUTO, "ARMATURE_AUTO", 0, "   With Automatic Weights", ""},
	{PAR_ARMATURE_ENVELOPE, "ARMATURE_ENVELOPE", 0, "   With Envelope Weights", ""},
	{PAR_BONE, "BONE", 0, "Bone", ""},
	{PAR_CURVE, "CURVE", 0, "Curve Deform", ""},
	{PAR_FOLLOW, "FOLLOW", 0, "Follow Path", ""},
	{PAR_PATH_CONST, "PATH_CONST", 0, "Path Constraint", ""},
	{PAR_LATTICE, "LATTICE", 0, "Lattice Deform", ""},
	{PAR_VERTEX, "VERTEX", 0, "Vertex", ""},
	{PAR_TRIA, "TRIA", 0, "Triangle", ""},
	{0, NULL, 0, NULL, NULL}
};

int ED_object_parent_set(ReportList *reports, Main *bmain, Scene *scene, Object *ob, Object *par, int partype)
{
	bPoseChannel *pchan = NULL;
	int pararm = ELEM4(partype, PAR_ARMATURE, PAR_ARMATURE_NAME, PAR_ARMATURE_ENVELOPE, PAR_ARMATURE_AUTO);
	
	par->recalc |= OB_RECALC_OB;
	
	/* preconditions */
	if (partype == PAR_FOLLOW || partype == PAR_PATH_CONST) {
		if (par->type != OB_CURVE)
			return 0;
		else {
			Curve *cu = par->data;
			
			if ((cu->flag & CU_PATH) == 0) {
				cu->flag |= CU_PATH | CU_FOLLOW;
				BKE_displist_make_curveTypes(scene, par, 0);  /* force creation of path data */
			}
			else cu->flag |= CU_FOLLOW;
			
			/* if follow, add F-Curve for ctime (i.e. "eval_time") so that path-follow works */
			if (partype == PAR_FOLLOW) {
				/* get or create F-Curve */
				bAction *act = verify_adt_action(&cu->id, 1);
				FCurve *fcu = verify_fcurve(act, NULL, "eval_time", 0, 1);
				
				/* setup dummy 'generator' modifier here to get 1-1 correspondence still working */
				if (!fcu->bezt && !fcu->fpt && !fcu->modifiers.first)
					add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_GENERATOR);
			}
			
			/* fall back on regular parenting now (for follow only) */
			if (partype == PAR_FOLLOW)
				partype = PAR_OBJECT;
		}		
	}
	else if (partype == PAR_BONE) {
		pchan = BKE_pose_channel_active(par);
		
		if (pchan == NULL) {
			BKE_report(reports, RPT_ERROR, "No active Bone");
			return 0;
		}
	}
	
	if (ob != par) {
		if (BKE_object_parent_loop_check(par, ob)) {
			BKE_report(reports, RPT_ERROR, "Loop in parents");
			return 0;
		}
		else {
			Object workob;
			
			/* apply transformation of previous parenting */
			/* BKE_object_apply_mat4(ob, ob->obmat); */ /* removed because of bug [#23577] */
			
			/* set the parent (except for follow-path constraint option) */
			if (partype != PAR_PATH_CONST) {
				ob->parent = par;
			}
			
			/* handle types */
			if (pchan)
				BLI_strncpy(ob->parsubstr, pchan->name, sizeof(ob->parsubstr));
			else
				ob->parsubstr[0] = 0;
				
			if (partype == PAR_PATH_CONST) {
				/* don't do anything here, since this is not technically "parenting" */
			}
			else if (ELEM(partype, PAR_CURVE, PAR_LATTICE) || (pararm)) {
				/* partype is now set to PAROBJECT so that invisible 'virtual' modifiers don't need to be created
				 * NOTE: the old (2.4x) method was to set ob->partype = PARSKEL, creating the virtual modifiers
				 */
				ob->partype = PAROBJECT; /* note, dna define, not operator property */
				//ob->partype= PARSKEL; /* note, dna define, not operator property */
				
				/* BUT, to keep the deforms, we need a modifier, and then we need to set the object that it uses */
				// XXX currently this should only happen for meshes, curves, surfaces, and lattices - this stuff isn't available for metas yet
				if (ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_LATTICE)) {
					ModifierData *md;
					
					switch (partype) {
						case PAR_CURVE: /* curve deform */
							md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Curve);
							((CurveModifierData *)md)->object = par;
							break;
						case PAR_LATTICE: /* lattice deform */
							md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Lattice);
							((LatticeModifierData *)md)->object = par;
							break;
						default: /* armature deform */
							md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Armature);
							((ArmatureModifierData *)md)->object = par;
							break;
					}
				}
			}
			else if (partype == PAR_BONE)
				ob->partype = PARBONE;  /* note, dna define, not operator property */
			else
				ob->partype = PAROBJECT;  /* note, dna define, not operator property */
			
			/* constraint */
			if (partype == PAR_PATH_CONST) {
				bConstraint *con;
				bFollowPathConstraint *data;
				float cmat[4][4], vec[3];
				
				con = add_ob_constraint(ob, "AutoPath", CONSTRAINT_TYPE_FOLLOWPATH);
				
				data = con->data;
				data->tar = par;
				
				get_constraint_target_matrix(scene, con, 0, CONSTRAINT_OBTYPE_OBJECT, NULL, cmat, scene->r.cfra);
				sub_v3_v3v3(vec, ob->obmat[3], cmat[3]);
				
				ob->loc[0] = vec[0];
				ob->loc[1] = vec[1];
				ob->loc[2] = vec[2];
			}
			else if (pararm && ob->type == OB_MESH && par->type == OB_ARMATURE) {
				if (partype == PAR_ARMATURE_NAME)
					create_vgroups_from_armature(reports, scene, ob, par, ARM_GROUPS_NAME, 0);
				else if (partype == PAR_ARMATURE_ENVELOPE)
					create_vgroups_from_armature(reports, scene, ob, par, ARM_GROUPS_ENVELOPE, 0);
				else if (partype == PAR_ARMATURE_AUTO) {
					WM_cursor_wait(1);
					create_vgroups_from_armature(reports, scene, ob, par, ARM_GROUPS_AUTO, 0);
					WM_cursor_wait(0);
				}
				/* get corrected inverse */
				ob->partype = PAROBJECT;
				BKE_object_workob_calc_parent(scene, ob, &workob);
				
				invert_m4_m4(ob->parentinv, workob.obmat);
			}
			else {
				/* calculate inverse parent matrix */
				BKE_object_workob_calc_parent(scene, ob, &workob);
				invert_m4_m4(ob->parentinv, workob.obmat);
			}
			
			ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA;
		}
	}

	return 1;
}

static int parent_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *par = ED_object_active_context(C);
	int partype = RNA_enum_get(op->ptr, "type");
	int ok = 1;

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (!ED_object_parent_set(op->reports, bmain, scene, ob, par, partype)) {
			ok = 0;
			break;
		}
	}
	CTX_DATA_END;

	if (!ok)
		return OPERATOR_CANCELLED;

	DAG_scene_sort(bmain, scene);
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);

	return OPERATOR_FINISHED;
}


static int parent_set_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *UNUSED(event))
{
	Object *ob = ED_object_active_context(C);
	uiPopupMenu *pup = uiPupMenuBegin(C, "Set Parent To", ICON_NONE);
	uiLayout *layout = uiPupMenuLayout(pup);
	
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_OBJECT);
	
	/* ob becomes parent, make the associated menus */
	if (ob->type == OB_ARMATURE) {
		uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_ARMATURE);
		uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_ARMATURE_NAME);
		uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_ARMATURE_ENVELOPE);
		uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_ARMATURE_AUTO);
		uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_BONE);
	}
	else if (ob->type == OB_CURVE) {
		uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_CURVE);
		uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_FOLLOW);
		uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_PATH_CONST);
	}
	else if (ob->type == OB_LATTICE) {
		uiItemEnumO(layout, "OBJECT_OT_parent_set", NULL, 0, "type", PAR_LATTICE);
	}
	
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}


void OBJECT_OT_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Parent";
	ot->description = "Set the object's parenting";
	ot->idname = "OBJECT_OT_parent_set";
	
	/* api callbacks */
	ot->invoke = parent_set_invoke;
	ot->exec = parent_set_exec;
	
	ot->poll = ED_operator_object_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_make_parent_types, 0, "Type", "");
}

/* ************ Make Parent Without Inverse Operator ******************* */

static int parent_noinv_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *par = ED_object_active_context(C);
	
	par->recalc |= OB_RECALC_OB;
	
	/* context iterator */
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob != par) {
			if (BKE_object_parent_loop_check(par, ob)) {
				BKE_report(op->reports, RPT_ERROR, "Loop in parents");
			}
			else {
				/* clear inverse matrix and also the object location */
				unit_m4(ob->parentinv);
				memset(ob->loc, 0, 3 * sizeof(float));
				
				/* set recalc flags */
				ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA;
				
				/* set parenting type for object - object only... */
				ob->parent = par;
				ob->partype = PAROBJECT; /* note, dna define, not operator property */
			}
		}
	}
	CTX_DATA_END;
	
	DAG_scene_sort(bmain, CTX_data_scene(C));
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_no_inverse_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Parent without Inverse";
	ot->description = "Set the object's parenting without setting the inverse parent correction";
	ot->idname = "OBJECT_OT_parent_no_inverse_set";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = parent_noinv_set_exec;
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ Clear Slow Parent Operator *********************/

static int object_slow_parent_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob->parent) {
			if (ob->partype & PARSLOW) {
				ob->partype -= PARSLOW;
				BKE_object_where_is_calc(scene, ob);
				ob->partype |= PARSLOW;
				ob->recalc |= OB_RECALC_OB;
			}
		}
	}
	CTX_DATA_END;

	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_SCENE, scene);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_slow_parent_clear(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Clear Slow Parent";
	ot->description = "Clear the object's slow parent";
	ot->idname = "OBJECT_OT_slow_parent_clear";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = object_slow_parent_clear_exec;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Make Slow Parent Operator *********************/

static int object_slow_parent_set_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob->parent)
			ob->partype |= PARSLOW;

		ob->recalc |= OB_RECALC_OB;
		
	}
	CTX_DATA_END;

	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_SCENE, scene);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_slow_parent_set(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Set Slow Parent";
	ot->description = "Set the object's slow parent";
	ot->idname = "OBJECT_OT_slow_parent_set";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = object_slow_parent_set_exec;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Clear Track Operator ******************* */

static EnumPropertyItem prop_clear_track_types[] = {
	{0, "CLEAR", 0, "Clear Track", ""},
	{1, "CLEAR_KEEP_TRANSFORM", 0, "Clear and Keep Transformation (Clear Track)", ""},
	{0, NULL, 0, NULL, NULL}
};

/* note, poll should check for editable scene */
static int object_track_clear_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	int type = RNA_enum_get(op->ptr, "type");

	if (CTX_data_edit_object(C)) {
		BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed in EditMode");
		return OPERATOR_CANCELLED;
	}
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		bConstraint *con, *pcon;
		
		/* remove track-object for old track */
		ob->track = NULL;
		ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
		
		/* also remove all tracking constraints */
		for (con = ob->constraints.last; con; con = pcon) {
			pcon = con->prev;
			if (ELEM3(con->type, CONSTRAINT_TYPE_TRACKTO, CONSTRAINT_TYPE_LOCKTRACK, CONSTRAINT_TYPE_DAMPTRACK))
				remove_constraint(&ob->constraints, con);
		}
		
		if (type == 1)
			BKE_object_apply_mat4(ob, ob->obmat, TRUE, TRUE);
	}
	CTX_DATA_END;

	DAG_ids_flush_update(bmain, 0);
	DAG_scene_sort(bmain, scene);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_track_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear track";
	ot->description = "Clear tracking constraint or flag from object";
	ot->idname = "OBJECT_OT_track_clear";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_track_clear_exec;
	
	ot->poll = ED_operator_objectmode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_clear_track_types, 0, "Type", "");
}

/************************** Make Track Operator *****************************/

static EnumPropertyItem prop_make_track_types[] = {
	{1, "DAMPTRACK", 0, "Damped Track Constraint", ""},
	{2, "TRACKTO", 0, "Track To Constraint", ""},
	{3, "LOCKTRACK", 0, "Lock Track Constraint", ""},
	{0, NULL, 0, NULL, NULL}
};

static int track_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *obact = ED_object_active_context(C);
	
	int type = RNA_enum_get(op->ptr, "type");
	
	if (type == 1) {
		bConstraint *con;
		bDampTrackConstraint *data;

		CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
		{
			if (ob != obact) {
				con = add_ob_constraint(ob, "AutoTrack", CONSTRAINT_TYPE_DAMPTRACK);

				data = con->data;
				data->tar = obact;
				ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
				
				/* Lamp, Camera and Speaker track differently by default */
				if (ob->type == OB_LAMP || ob->type == OB_CAMERA || ob->type == OB_SPEAKER)
					data->trackflag = TRACK_nZ;
			}
		}
		CTX_DATA_END;
	}
	else if (type == 2) {
		bConstraint *con;
		bTrackToConstraint *data;

		CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
		{
			if (ob != obact) {
				con = add_ob_constraint(ob, "AutoTrack", CONSTRAINT_TYPE_TRACKTO);

				data = con->data;
				data->tar = obact;
				ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
				
				/* Lamp, Camera and Speaker track differently by default */
				if (ob->type == OB_LAMP || ob->type == OB_CAMERA || ob->type == OB_SPEAKER) {
					data->reserved1 = TRACK_nZ;
					data->reserved2 = UP_Y;
				}
			}
		}
		CTX_DATA_END;
	}
	else if (type == 3) {
		bConstraint *con;
		bLockTrackConstraint *data;

		CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
		{
			if (ob != obact) {
				con = add_ob_constraint(ob, "AutoTrack", CONSTRAINT_TYPE_LOCKTRACK);

				data = con->data;
				data->tar = obact;
				ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
				
				/* Lamp, Camera and Speaker track differently by default */
				if (ob->type == OB_LAMP || ob->type == OB_CAMERA || ob->type == OB_SPEAKER) {
					data->trackflag = TRACK_nZ;
					data->lockflag = LOCK_Y;
				}
			}
		}
		CTX_DATA_END;
	}
	
	DAG_scene_sort(bmain, scene);
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_track_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Track";
	ot->description = "Make the object track another object, either by constraint or old way or locked track";
	ot->idname = "OBJECT_OT_track_set";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = track_set_exec;
	
	ot->poll = ED_operator_objectmode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_make_track_types, 0, "Type", "");
}

/************************** Move to Layer Operator *****************************/

static unsigned int move_to_layer_init(bContext *C, wmOperator *op)
{
	int values[20], a;
	unsigned int lay = 0;

	if (!RNA_struct_property_is_set(op->ptr, "layers")) {
		/* note: layers are set in bases, library objects work for this */
		CTX_DATA_BEGIN (C, Base *, base, selected_bases)
		{
			lay |= base->lay;
		}
		CTX_DATA_END;

		for (a = 0; a < 20; a++)
			values[a] = (lay & (1 << a));
		
		RNA_boolean_set_array(op->ptr, "layers", values);
	}
	else {
		RNA_boolean_get_array(op->ptr, "layers", values);

		for (a = 0; a < 20; a++)
			if (values[a])
				lay |= (1 << a);
	}

	return lay;
}

static int move_to_layer_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		return WM_operator_confirm_message(C, op, "Move from localview");
	}
	else {
		move_to_layer_init(C, op);
		return WM_operator_props_popup(C, op, event);
	}
}

static int move_to_layer_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	unsigned int lay, local;
	/* int islamp= 0; */ /* UNUSED */
	
	lay = move_to_layer_init(C, op);
	lay &= 0xFFFFFF;

	if (lay == 0) return OPERATOR_CANCELLED;
	
	if (v3d && v3d->localvd) {
		/* now we can move out of localview. */
		/* note: layers are set in bases, library objects work for this */
		CTX_DATA_BEGIN (C, Base *, base, selected_bases)
		{
			lay = base->lay & ~v3d->lay;
			base->lay = lay;
			base->object->lay = lay;
			base->object->flag &= ~SELECT;
			base->flag &= ~SELECT;
			/* if (base->object->type==OB_LAMP) islamp= 1; */
		}
		CTX_DATA_END;
	}
	else {
		/* normal non localview operation */
		/* note: layers are set in bases, library objects work for this */
		CTX_DATA_BEGIN (C, Base *, base, selected_bases)
		{
			/* upper byte is used for local view */
			local = base->lay & 0xFF000000;
			base->lay = lay + local;
			base->object->lay = lay;
			/* if (base->object->type==OB_LAMP) islamp= 1; */
		}
		CTX_DATA_END;
	}
	
	/* warning, active object may be hidden now */
	
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, scene);
	WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

	DAG_scene_sort(bmain, scene);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_move_to_layer(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move to Layer";
	ot->description = "Move the object to different layers";
	ot->idname = "OBJECT_OT_move_to_layer";
	
	/* api callbacks */
	ot->invoke = move_to_layer_invoke;
	ot->exec = move_to_layer_exec;
	ot->poll = ED_operator_objectmode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layers", 20, NULL, "Layer", "");
}

/************************** Link to Scene Operator *****************************/

#if 0
static void link_to_scene(Main *UNUSED(bmain), unsigned short UNUSED(nr))
{
	Scene *sce = (Scene *) BLI_findlink(&bmain->scene, G.curscreen->scenenr - 1);
	Base *base, *nbase;
	
	if (sce == 0) return;
	if (sce->id.lib) return;
	
	for (base = FIRSTBASE; base; base = base->next) {
		if (TESTBASE(v3d, base)) {
			
			nbase = MEM_mallocN(sizeof(Base), "newbase");
			*nbase = *base;
			BLI_addhead(&(sce->base), nbase);
			id_us_plus((ID *)base->object);
		}
	}
}
#endif

static int make_links_scene_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene_to = BLI_findlink(&CTX_data_main(C)->scene, RNA_enum_get(op->ptr, "scene"));

	if (scene_to == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Scene not found");
		return OPERATOR_CANCELLED;
	}

	if (scene_to == CTX_data_scene(C)) {
		BKE_report(op->reports, RPT_ERROR, "Can't link objects into the same scene");
		return OPERATOR_CANCELLED;
	}

	if (scene_to->id.lib) {
		BKE_report(op->reports, RPT_ERROR, "Can't link objects into a linked scene");
		return OPERATOR_CANCELLED;
	}

	CTX_DATA_BEGIN (C, Base *, base, selected_bases)
	{
		if (!BKE_scene_base_find(scene_to, base->object)) {
			Base *nbase = MEM_mallocN(sizeof(Base), "newbase");
			*nbase = *base;
			BLI_addhead(&(scene_to->base), nbase);
			id_us_plus((ID *)base->object);
		}
	}
	CTX_DATA_END;

	DAG_ids_flush_update(bmain, 0);

	/* one day multiple scenes will be visible, then we should have some update function for them */
	return OPERATOR_FINISHED;
}

enum {
	MAKE_LINKS_OBDATA = 1,
	MAKE_LINKS_MATERIALS,
	MAKE_LINKS_ANIMDATA,
	MAKE_LINKS_DUPLIGROUP,
	MAKE_LINKS_MODIFIERS
};

/* Return 1 if make link data is allow, zero otherwise */
static int allow_make_links_data(int ev, Object *ob, Object *obt)
{
	switch (ev) {
		case MAKE_LINKS_OBDATA:
			if (ob->type == obt->type && ob->type != OB_EMPTY)
				return 1;
			break;
		case MAKE_LINKS_MATERIALS:
			if (OB_TYPE_SUPPORT_MATERIAL(ob->type) &&
			    OB_TYPE_SUPPORT_MATERIAL(obt->type))
			{
				return 1;
			}
			break;
		case MAKE_LINKS_ANIMDATA:
		case MAKE_LINKS_DUPLIGROUP:
			return 1;
		case MAKE_LINKS_MODIFIERS:
			if (ob->type != OB_EMPTY && obt->type != OB_EMPTY)
				return 1;
			break;
	}
	return 0;
}

static int make_links_data_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	int event = RNA_enum_get(op->ptr, "type");
	Object *ob;
	ID *id;
	int a;

	ob = ED_object_active_context(C);

	CTX_DATA_BEGIN (C, Object *, obt, selected_editable_objects)
	{
		if (ob != obt) {
			if (allow_make_links_data(event, ob, obt)) {
				switch (event) {
					case MAKE_LINKS_OBDATA: /* obdata */
						id = obt->data;
						id->us--;

						id = ob->data;
						id_us_plus(id);
						obt->data = id;

						/* if amount of material indices changed: */
						test_object_materials(obt->data);

						obt->recalc |= OB_RECALC_DATA;
						break;
					case MAKE_LINKS_MATERIALS:
						/* new approach, using functions from kernel */
						for (a = 0; a < ob->totcol; a++) {
							Material *ma = give_current_material(ob, a + 1);
							assign_material(obt, ma, a + 1); /* also works with ma==NULL */
						}
						break;
					case MAKE_LINKS_ANIMDATA:
						BKE_copy_animdata_id((ID *)obt, (ID *)ob, FALSE);
						BKE_copy_animdata_id((ID *)obt->data, (ID *)ob->data, FALSE);
						break;
					case MAKE_LINKS_DUPLIGROUP:
						obt->dup_group = ob->dup_group;
						if (obt->dup_group) {
							id_lib_extern(&obt->dup_group->id);
							obt->transflag |= OB_DUPLIGROUP;
						}
						break;
					case MAKE_LINKS_MODIFIERS:
						BKE_object_link_modifiers(obt, ob);
						obt->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;
						break;
				}
			}
		}
	}
	CTX_DATA_END;

	DAG_scene_sort(bmain, CTX_data_scene(C));
	
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));
	return OPERATOR_FINISHED;
}


void OBJECT_OT_make_links_scene(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Link Objects to Scene";
	ot->description = "Link selection to another scene";
	ot->idname = "OBJECT_OT_make_links_scene";

	/* api callbacks */
	ot->invoke = WM_enum_search_invoke;
	ot->exec = make_links_scene_exec;
	/* better not run the poll check */

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "scene", DummyRNA_NULL_items, 0, "Scene", "");
	RNA_def_enum_funcs(prop, RNA_scene_local_itemf);
	ot->prop = prop;
}

void OBJECT_OT_make_links_data(wmOperatorType *ot)
{
	static EnumPropertyItem make_links_items[] = {
		{MAKE_LINKS_OBDATA,     "OBDATA", 0, "Object Data", ""},
		{MAKE_LINKS_MATERIALS,  "MATERIAL", 0, "Materials", ""},
		{MAKE_LINKS_ANIMDATA,   "ANIMATION", 0, "Animation Data", ""},
		{MAKE_LINKS_DUPLIGROUP, "DUPLIGROUP", 0, "DupliGroup", ""},
		{MAKE_LINKS_MODIFIERS,  "MODIFIERS", 0, "Modifiers", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Link Data";
	ot->description = "Make links from the active object to other selected objects";
	ot->idname = "OBJECT_OT_make_links_data";

	/* api callbacks */
	ot->exec = make_links_data_exec;
	ot->poll = ED_operator_object_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", make_links_items, 0, "Type", "");
}


/**************************** Make Single User ********************************/

static void single_object_users(Scene *scene, View3D *v3d, int flag)	
{
	Base *base;
	Object *ob, *obn;
	
	clear_sca_new_poins();  /* sensor/contr/act */

	/* duplicate (must set newid) */
	for (base = FIRSTBASE; base; base = base->next) {
		ob = base->object;
		
		/* newid may still have some trash from Outliner tree building,
		 * so clear that first to avoid errors [#26002]
		 */
		ob->id.newid = NULL;
		
		if ( (base->flag & flag) == flag) {
			if (ob->id.lib == NULL && ob->id.us > 1) {
				/* base gets copy of object */
				obn = BKE_object_copy(ob);
				base->object = obn;
				ob->id.us--;
			}
		}
	}
	
	ID_NEW(scene->camera);
	if (v3d) ID_NEW(v3d->camera);
	
	/* object pointers */
	for (base = FIRSTBASE; base; base = base->next) {
		BKE_object_relink(base->object);
	}

	set_sca_new_poins();
}

/* not an especially efficient function, only added so the single user
 * button can be functional.*/
void ED_object_single_user(Scene *scene, Object *ob)
{
	Base *base;

	for (base = FIRSTBASE; base; base = base->next) {
		if (base->object == ob) base->flag |=  OB_DONE;
		else base->flag &= ~OB_DONE;
	}

	single_object_users(scene, NULL, OB_DONE);
}

static void new_id_matar(Material **matar, int totcol)
{
	ID *id;
	int a;
	
	for (a = 0; a < totcol; a++) {
		id = (ID *)matar[a];
		if (id && id->lib == NULL) {
			if (id->newid) {
				matar[a] = (Material *)id->newid;
				id_us_plus(id->newid);
				id->us--;
			}
			else if (id->us > 1) {
				matar[a] = BKE_material_copy(matar[a]);
				id->us--;
				id->newid = (ID *)matar[a];
			}
		}
	}
}

static void single_obdata_users(Main *bmain, Scene *scene, int flag)
{
	Object *ob;
	Lamp *la;
	Curve *cu;
	//Camera *cam;
	Base *base;
	Mesh *me;
	ID *id;
	int a;

	for (base = FIRSTBASE; base; base = base->next) {
		ob = base->object;
		if (ob->id.lib == NULL && (base->flag & flag) == flag) {
			id = ob->data;
			
			if (id && id->us > 1 && id->lib == NULL) {
				ob->recalc = OB_RECALC_DATA;
				
				BKE_copy_animdata_id_action(id);
				
				switch (ob->type) {
					case OB_LAMP:
						ob->data = la = BKE_lamp_copy(ob->data);
						for (a = 0; a < MAX_MTEX; a++) {
							if (la->mtex[a]) {
								ID_NEW(la->mtex[a]->object);
							}
						}
						break;
					case OB_CAMERA:
						ob->data = BKE_camera_copy(ob->data);
						break;
					case OB_MESH:
						ob->data = BKE_mesh_copy(ob->data);
						//me= ob->data;
						//if (me && me->key)
						//	ipo_idnew(me->key->ipo);	/* drivers */
						break;
					case OB_MBALL:
						ob->data = BKE_mball_copy(ob->data);
						break;
					case OB_CURVE:
					case OB_SURF:
					case OB_FONT:
						ob->data = cu = BKE_curve_copy(ob->data);
						ID_NEW(cu->bevobj);
						ID_NEW(cu->taperobj);
						break;
					case OB_LATTICE:
						ob->data = BKE_lattice_copy(ob->data);
						break;
					case OB_ARMATURE:
						ob->recalc |= OB_RECALC_DATA;
						ob->data = BKE_armature_copy(ob->data);
						BKE_pose_rebuild(ob, ob->data);
						break;
					case OB_SPEAKER:
						ob->data = BKE_speaker_copy(ob->data);
						break;
					default:
						if (G.debug & G_DEBUG)
							printf("ERROR %s: can't copy %s\n", __func__, id->name);
						return;
				}
				
				id->us--;
				id->newid = ob->data;
				
			}
			
		}
	}
	
	me = bmain->mesh.first;
	while (me) {
		ID_NEW(me->texcomesh);
		me = me->id.next;
	}
}

static void single_object_action_users(Scene *scene, int flag)
{
	Object *ob;
	Base *base;
	
	for (base = FIRSTBASE; base; base = base->next) {
		ob = base->object;
		if (ob->id.lib == NULL && (flag == 0 || (base->flag & SELECT)) ) {
			ob->recalc = OB_RECALC_DATA;
			BKE_copy_animdata_id_action(&ob->id);
		}
	}
}

static void single_mat_users(Scene *scene, int flag, int do_textures)
{
	Object *ob;
	Base *base;
	Material *ma, *man;
	Tex *tex;
	int a, b;
	
	for (base = FIRSTBASE; base; base = base->next) {
		ob = base->object;
		if (ob->id.lib == NULL && (flag == 0 || (base->flag & SELECT)) ) {
	
			for (a = 1; a <= ob->totcol; a++) {
				ma = give_current_material(ob, a);
				if (ma) {
					/* do not test for LIB_NEW: this functions guaranteed delivers single_users! */
					
					if (ma->id.us > 1) {
						man = BKE_material_copy(ma);
						BKE_copy_animdata_id_action(&man->id);
						
						man->id.us = 0;
						assign_material(ob, man, a);

						if (do_textures) {
							for (b = 0; b < MAX_MTEX; b++) {
								if (ma->mtex[b] && (tex = ma->mtex[b]->tex)) {
									if (tex->id.us > 1) {
										tex->id.us--;
										tex = BKE_texture_copy(tex);
										BKE_copy_animdata_id_action(&tex->id);
										man->mtex[b]->tex = tex;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

static void do_single_tex_user(Tex **from)
{
	Tex *tex, *texn;
	
	tex = *from;
	if (tex == NULL) return;
	
	if (tex->id.newid) {
		*from = (Tex *)tex->id.newid;
		id_us_plus(tex->id.newid);
		tex->id.us--;
	}
	else if (tex->id.us > 1) {
		texn = BKE_texture_copy(tex);
		BKE_copy_animdata_id_action(&texn->id);
		tex->id.newid = (ID *)texn;
		tex->id.us--;
		*from = texn;
	}
}

static void single_tex_users_expand(Main *bmain)
{
	/* only when 'parent' blocks are LIB_NEW */
	Material *ma;
	Lamp *la;
	World *wo;
	int b;
		
	for (ma = bmain->mat.first; ma; ma = ma->id.next) {
		if (ma->id.flag & LIB_NEW) {
			for (b = 0; b < MAX_MTEX; b++) {
				if (ma->mtex[b] && ma->mtex[b]->tex) {
					do_single_tex_user(&(ma->mtex[b]->tex));
				}
			}
		}
	}

	for (la = bmain->lamp.first; la; la = la->id.next) {
		if (la->id.flag & LIB_NEW) {
			for (b = 0; b < MAX_MTEX; b++) {
				if (la->mtex[b] && la->mtex[b]->tex) {
					do_single_tex_user(&(la->mtex[b]->tex));
				}
			}
		}
	}

	for (wo = bmain->world.first; wo; wo = wo->id.next) {
		if (wo->id.flag & LIB_NEW) {
			for (b = 0; b < MAX_MTEX; b++) {
				if (wo->mtex[b] && wo->mtex[b]->tex) {
					do_single_tex_user(&(wo->mtex[b]->tex));
				}
			}
		}
	}
}

static void single_mat_users_expand(Main *bmain)
{
	/* only when 'parent' blocks are LIB_NEW */
	Object *ob;
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	Material *ma;
	int a;
	
	for (ob = bmain->object.first; ob; ob = ob->id.next)
		if (ob->id.flag & LIB_NEW)
			new_id_matar(ob->mat, ob->totcol);

	for (me = bmain->mesh.first; me; me = me->id.next)
		if (me->id.flag & LIB_NEW)
			new_id_matar(me->mat, me->totcol);

	for (cu = bmain->curve.first; cu; cu = cu->id.next)
		if (cu->id.flag & LIB_NEW)
			new_id_matar(cu->mat, cu->totcol);

	for (mb = bmain->mball.first; mb; mb = mb->id.next)
		if (mb->id.flag & LIB_NEW)
			new_id_matar(mb->mat, mb->totcol);

	/* material imats  */
	for (ma = bmain->mat.first; ma; ma = ma->id.next)
		if (ma->id.flag & LIB_NEW)
			for (a = 0; a < MAX_MTEX; a++)
				if (ma->mtex[a])
					ID_NEW(ma->mtex[a]->object);
}

/* used for copying scenes */
void ED_object_single_users(Main *bmain, Scene *scene, int full)
{
	single_object_users(scene, NULL, 0);

	if (full) {
		single_obdata_users(bmain, scene, 0);
		single_object_action_users(scene, 0);
		single_mat_users_expand(bmain);
		single_tex_users_expand(bmain);
	}

	clear_id_newpoins();
}

/******************************* Make Local ***********************************/

/* helper for below, ma was checked to be not NULL */
static void make_local_makelocalmaterial(Material *ma)
{
	AnimData *adt;
	int b;
	
	id_make_local(&ma->id, 0);
	
	for (b = 0; b < MAX_MTEX; b++)
		if (ma->mtex[b] && ma->mtex[b]->tex)
			id_make_local(&ma->mtex[b]->tex->id, 0);
	
	adt = BKE_animdata_from_id(&ma->id);
	if (adt) BKE_animdata_make_local(adt);

	/* nodetree? XXX */
}

static int make_local_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	AnimData *adt;
	ParticleSystem *psys;
	Material *ma, ***matarar;
	Lamp *la;
	ID *id;
	int a, b, mode = RNA_enum_get(op->ptr, "type");
	
	if (mode == 3) {
		BKE_library_make_local(bmain, NULL, 0); /* NULL is all libs */
		WM_event_add_notifier(C, NC_WINDOW, NULL);
		return OPERATOR_FINISHED;
	}

	clear_id_newpoins();
	
	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		if (ob->id.lib)
			id_make_local(&ob->id, 0);
	}
	CTX_DATA_END;
	
	/* maybe object pointers */
	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		if (ob->id.lib == NULL) {
			ID_NEW(ob->parent);
		}
	}
	CTX_DATA_END;

	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		id = ob->data;
			
		if (id && mode > 1) {
			id_make_local(id, 0);
			adt = BKE_animdata_from_id(id);
			if (adt) BKE_animdata_make_local(adt);
			
			/* tag indirect data direct */
			matarar = (Material ***)give_matarar(ob);
			if (matarar) {
				for (a = 0; a < ob->totcol; a++) {
					ma = (*matarar)[a];
					if (ma)
						id_lib_extern(&ma->id);
				}
			}
		}

		for (psys = ob->particlesystem.first; psys; psys = psys->next)
			id_make_local(&psys->part->id, 0);

		adt = BKE_animdata_from_id(&ob->id);
		if (adt) BKE_animdata_make_local(adt);
	}
	CTX_DATA_END;

	if (mode > 1) {
		CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
		{
			if (ob->type == OB_LAMP) {
				la = ob->data;

				for (b = 0; b < MAX_MTEX; b++)
					if (la->mtex[b] && la->mtex[b]->tex)
						id_make_local(&la->mtex[b]->tex->id, 0);
			}
			else {
				for (a = 0; a < ob->totcol; a++) {
					ma = ob->mat[a];
					if (ma)
						make_local_makelocalmaterial(ma);
				}
				
				matarar = (Material ***)give_matarar(ob);
				if (matarar) {
					for (a = 0; a < ob->totcol; a++) {
						ma = (*matarar)[a];
						if (ma)
							make_local_makelocalmaterial(ma);
					}
				}
			}
		}
		CTX_DATA_END;
	}

	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_make_local(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{1, "SELECTED_OBJECTS", 0, "Selected Objects", ""},
		{2, "SELECTED_OBJECTS_DATA", 0, "Selected Objects and Data", ""},
		{3, "ALL", 0, "All", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Make Local";
	ot->description = "Make library linked datablocks local to this file";
	ot->idname = "OBJECT_OT_make_local";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = make_local_exec;
	ot->poll = ED_operator_objectmode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

static int make_single_user_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C); /* ok if this is NULL */
	int flag = RNA_enum_get(op->ptr, "type"); /* 0==ALL, SELECTED==selected objecs */

	if (RNA_boolean_get(op->ptr, "object"))
		single_object_users(scene, v3d, flag);

	if (RNA_boolean_get(op->ptr, "obdata"))
		single_obdata_users(bmain, scene, flag);

	if (RNA_boolean_get(op->ptr, "material"))
		single_mat_users(scene, flag, RNA_boolean_get(op->ptr, "texture"));

#if 0 /* can't do this separate from materials */
	if (RNA_boolean_get(op->ptr, "texture"))
		single_mat_users(scene, flag, TRUE);
#endif
	if (RNA_boolean_get(op->ptr, "animation"))
		single_object_action_users(scene, flag);

	clear_id_newpoins();

	WM_event_add_notifier(C, NC_WINDOW, NULL);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_make_single_user(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{SELECT, "SELECTED_OBJECTS", 0, "Selected Objects", ""},
		{0, "ALL", 0, "All", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Make Single User";
	ot->description = "Make linked data local to each object";
	ot->idname = "OBJECT_OT_make_single_user";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = make_single_user_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, SELECT, "Type", "");

	RNA_def_boolean(ot->srna, "object", 0, "Object", "Make single user objects");
	RNA_def_boolean(ot->srna, "obdata", 0, "Object Data", "Make single user object data");
	RNA_def_boolean(ot->srna, "material", 0, "Materials", "Make materials local to each datablock");
	RNA_def_boolean(ot->srna, "texture", 0, "Textures", "Make textures local to each material");
	RNA_def_boolean(ot->srna, "animation", 0, "Object Animation", "Make animation data local to each object");
}

static int drop_named_material_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Base *base = ED_view3d_give_base_under_cursor(C, event->mval);
	Material *ma;
	char name[MAX_ID_NAME - 2];
	
	RNA_string_get(op->ptr, "name", name);
	ma = (Material *)BKE_libblock_find_name(ID_MA, name);
	if (base == NULL || ma == NULL)
		return OPERATOR_CANCELLED;
	
	assign_material(base->object, ma, 1);
	
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));
	
	return OPERATOR_FINISHED;
}

/* used for dropbox */
/* assigns to object under cursor, only first material slot */
void OBJECT_OT_drop_named_material(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Drop Named Material on Object";
	ot->description = "";
	ot->idname = "OBJECT_OT_drop_named_material";
	
	/* api callbacks */
	ot->invoke = drop_named_material_invoke;
	ot->poll = ED_operator_objectmode;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
	
	/* properties */
	RNA_def_string(ot->srna, "name", "Material", MAX_ID_NAME - 2, "Name", "Material name to assign");
}
