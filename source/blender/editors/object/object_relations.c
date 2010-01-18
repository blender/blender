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
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
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
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "object_intern.h"

/*********************** Make Vertex Parent Operator ************************/

static int vertex_parent_set_poll(bContext *C)
{
	return ED_operator_editmesh(C) || ED_operator_editsurfcurve(C) || ED_operator_editlattice(C);
}

static int vertex_parent_set_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditVert *eve;
	Curve *cu;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	Object *par;
	int a, v1=0, v2=0, v3=0, v4=0, nr=1;
	
	/* we need 1 to 3 selected vertices */
	
	if(obedit->type==OB_MESH) {
		Mesh *me= obedit->data;
		EditMesh *em = BKE_mesh_get_editmesh(me);

		eve= em->verts.first;
		while(eve) {
			if(eve->f & 1) {
				if(v1==0) v1= nr;
				else if(v2==0) v2= nr;
				else if(v3==0) v3= nr;
				else if(v4==0) v4= nr;
				else break;
			}
			nr++;
			eve= eve->next;
		}

		BKE_mesh_end_editmesh(me, em);
	}
	else if(ELEM(obedit->type, OB_SURF, OB_CURVE)) {
		ListBase *editnurb= curve_get_editcurve(obedit);
		
		cu= obedit->data;

		nu= editnurb->first;
		while(nu) {
			if(nu->type == CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
						if(v1==0) v1= nr;
						else if(v2==0) v2= nr;
						else if(v3==0) v3= nr;
						else if(v4==0) v4= nr;
						else break;
					}
					nr++;
					bezt++;
				}
			}
			else {
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				while(a--) {
					if(bp->f1 & SELECT) {
						if(v1==0) v1= nr;
						else if(v2==0) v2= nr;
						else if(v3==0) v3= nr;
						else if(v4==0) v4= nr;
						else break;
					}
					nr++;
					bp++;
				}
			}
			nu= nu->next;
		}
	}
	else if(obedit->type==OB_LATTICE) {
		Lattice *lt= obedit->data;
		
		a= lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
		bp= lt->editlatt->def;
		while(a--) {
			if(bp->f1 & SELECT) {
				if(v1==0) v1= nr;
				else if(v2==0) v2= nr;
				else if(v3==0) v3= nr;
				else if(v4==0) v4= nr;
				else break;
			}
			nr++;
			bp++;
		}
	}
	
	if(v4 || !((v1 && v2==0 && v3==0) || (v1 && v2 && v3)) ) {
		BKE_report(op->reports, RPT_ERROR, "Select either 1 or 3 vertices to parent to");
		return OPERATOR_CANCELLED;
	}
	
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(ob != obedit) {
			ob->recalc |= OB_RECALC;
			par= obedit->parent;
			
			while(par) {
				if(par==ob) break;
				par= par->parent;
			}
			if(par) {
				BKE_report(op->reports, RPT_ERROR, "Loop in parents");
			}
			else {
				Object workob;
				
				ob->parent= BASACT->object;
				if(v3) {
					ob->partype= PARVERT3;
					ob->par1= v1-1;
					ob->par2= v2-1;
					ob->par3= v3-1;

					/* inverse parent matrix */
					what_does_parent(scene, ob, &workob);
					invert_m4_m4(ob->parentinv, workob.obmat);
				}
				else {
					ob->partype= PARVERT1;
					ob->par1= v1-1;

					/* inverse parent matrix */
					what_does_parent(scene, ob, &workob);
					invert_m4_m4(ob->parentinv, workob.obmat);
				}
			}
		}
	}
	CTX_DATA_END;
	
	DAG_scene_sort(scene);

	WM_event_add_notifier(C, NC_OBJECT, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Vertex Parent";
	ot->description = "Parent selected objects to the selected vertices.";
	ot->idname= "OBJECT_OT_vertex_parent_set";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->poll= vertex_parent_set_poll;
	ot->exec= vertex_parent_set_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** Make Proxy Operator *************************/

/* set the object to proxify */
static int make_proxy_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	
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
		uiPopupMenu *pup= uiPupMenuBegin(C, "OK?", ICON_QUESTION);
		uiLayout *layout= uiPupMenuLayout(pup);
		PointerRNA props_ptr;
		
		/* create operator menu item with relevant properties filled in */
		props_ptr= uiItemFullO(layout, op->type->name, 0, op->idname, NULL, WM_OP_EXEC_REGION_WIN, UI_ITEM_O_RETURN_PROPS);
		
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

static int make_proxy_exec (bContext *C, wmOperator *op)
{
	Object *ob, *gob= CTX_data_active_object(C);
	GroupObject *go= BLI_findlink(&gob->dup_group->gobject, RNA_enum_get(op->ptr, "type"));
	Scene *scene= CTX_data_scene(C);
	ob= go->ob;
	
	if (ob) {
		Object *newob;
		Base *newbase, *oldbase= BASACT;
		char name[32];
		
		/* Add new object for the proxy */
		newob= add_object(scene, OB_EMPTY);
		if (gob)
			strcpy(name, gob->id.name+2);
		else
			strcpy(name, ob->id.name+2);
		strcat(name, "_proxy");
		rename_id(&newob->id, name);
		
		/* set layers OK */
		newbase= BASACT;	/* add_object sets active... */
		newbase->lay= oldbase->lay;
		newob->lay= newbase->lay;
		
		/* remove base, leave user count of object, it gets linked in object_make_proxy */
		if (gob==NULL) {
			BLI_remlink(&scene->base, oldbase);
			MEM_freeN(oldbase);
		}
		
		object_make_proxy(newob, ob, gob);
		
		/* depsgraph flushes are needed for the new data */
		DAG_scene_sort(scene);
		DAG_id_flush_update(&newob->id, OB_RECALC);
		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, newob);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No object to make proxy for");
		return OPERATOR_CANCELLED;
	}
	
	return OPERATOR_FINISHED;
}

/* Generic itemf's for operators that take library args */
static EnumPropertyItem *proxy_group_object_itemf(bContext *C, PointerRNA *ptr, int *free)
{
	EnumPropertyItem *item= NULL, item_tmp;
	int totitem= 0;
	int i= 0;
	Object *ob= CTX_data_active_object(C);
	GroupObject *go;

	if(!ob || !ob->dup_group)
		return DummyRNA_NULL_items;

	memset(&item_tmp, 0, sizeof(item_tmp));

	/* find the object to affect */
	for (go= ob->dup_group->gobject.first; go; go= go->next) {
		item_tmp.identifier= item_tmp.name= go->ob->id.name+2;
		item_tmp.value= i++;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

void OBJECT_OT_proxy_make (wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Make Proxy";
	ot->idname= "OBJECT_OT_proxy_make";
	ot->description= "Add empty object to become local replacement data of a library-linked object";
	
	/* callbacks */
	ot->invoke= make_proxy_invoke;
	ot->exec= make_proxy_exec;
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_string(ot->srna, "object", "", 19, "Proxy Object", "Name of lib-linked/grouped object to make a proxy for.");
	prop= RNA_def_enum(ot->srna, "type", DummyRNA_NULL_items, 0, "Type", "Group object"); /* XXX, relies on hard coded ID at the moment */
	RNA_def_enum_funcs(prop, proxy_group_object_itemf);
	ot->prop= prop;
}

/********************** Clear Parent Operator ******************* */

static EnumPropertyItem prop_clear_parent_types[] = {
	{0, "CLEAR", 0, "Clear Parent", ""},
	{1, "CLEAR_KEEP_TRANSFORM", 0, "Clear and Keep Transformation (Clear Track)", ""},
	{2, "CLEAR_INVERSE", 0, "Clear Parent Inverse", ""},
	{0, NULL, 0, NULL, NULL}
};

/* note, poll should check for editable scene */
static int parent_clear_exec(bContext *C, wmOperator *op)
{
	int type= RNA_enum_get(op->ptr, "type");
	
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {

		if(type == 0) {
			ob->parent= NULL;
		}			
		else if(type == 1) {
			ob->parent= NULL;
			ob->track= NULL;
			ED_object_apply_obmat(ob);
		}
		else if(type == 2)
			unit_m4(ob->parentinv);

		ob->recalc |= OB_RECALC;
	}
	CTX_DATA_END;
	
	DAG_scene_sort(CTX_data_scene(C));
	DAG_ids_flush_update(0);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Parent";
	ot->description = "Clear the object's parenting.";
	ot->idname= "OBJECT_OT_parent_clear";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= parent_clear_exec;
	
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	ot->prop= RNA_def_enum(ot->srna, "type", prop_clear_parent_types, 0, "Type", "");
}

/* ******************** Make Parent Operator *********************** */

#define PAR_OBJECT				0
#define PAR_ARMATURE			1
#define PAR_ARMATURE_NAME		2
#define PAR_ARMATURE_ENVELOPE	3
#define PAR_ARMATURE_AUTO		4
#define PAR_BONE				5
#define PAR_CURVE				6
#define PAR_FOLLOW				7
#define PAR_PATH_CONST			8
#define PAR_LATTICE				9
#define PAR_VERTEX				10
#define PAR_TRIA				11

static EnumPropertyItem prop_make_parent_types[] = {
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

static int test_parent_loop(Object *par, Object *ob)
{
	/* test if 'ob' is a parent somewhere in par's parents */
	
	if(par == NULL) return 0;
	if(ob == par) return 1;
	
	return test_parent_loop(par->parent, ob);
}

void ED_object_parent(Object *ob, Object *par, int type, const char *substr)
{
	if(!par || test_parent_loop(par, ob)) {
		ob->parent= NULL;
		ob->partype= PAROBJECT;
		ob->parsubstr[0]= 0;
		return;
	}

	/* this could use some more checks */

	ob->parent= par;
	ob->partype &= ~PARTYPE;
	ob->partype |= type;
	BLI_strncpy(ob->parsubstr, substr, sizeof(ob->parsubstr));
}

static int parent_set_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *par= CTX_data_active_object(C);
	bPoseChannel *pchan= NULL;
	int partype= RNA_enum_get(op->ptr, "type");
	int pararm= ELEM4(partype, PAR_ARMATURE, PAR_ARMATURE_NAME, PAR_ARMATURE_ENVELOPE, PAR_ARMATURE_AUTO);
	
	par->recalc |= OB_RECALC_OB;
	
	/* preconditions */
	if(partype==PAR_FOLLOW || partype==PAR_PATH_CONST) {
		if(par->type!=OB_CURVE)
			return OPERATOR_CANCELLED;
		else {
			Curve *cu= par->data;
			
			if((cu->flag & CU_PATH)==0) {
				cu->flag |= CU_PATH|CU_FOLLOW;
				makeDispListCurveTypes(scene, par, 0);  /* force creation of path data */
			}
			else cu->flag |= CU_FOLLOW;
			
			/* fall back on regular parenting now (for follow only) */
			if(partype == PAR_FOLLOW)
				partype= PAR_OBJECT;
		}		
	}
	else if(partype==PAR_BONE) {
		pchan= get_active_posechannel(par);
		
		if(pchan==NULL) {
			BKE_report(op->reports, RPT_ERROR, "No active Bone");
			return OPERATOR_CANCELLED;
		}
	}
	
	/* context itterator */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		
		if(ob!=par) {
			
			if( test_parent_loop(par, ob) ) {
				BKE_report(op->reports, RPT_ERROR, "Loop in parents");
			}
			else {
				Object workob;
				
				/* apply transformation of previous parenting */
				ED_object_apply_obmat(ob);
				
				/* set the parent (except for follow-path constraint option) */
				if(partype != PAR_PATH_CONST)
					ob->parent= par;
				
				/* handle types */
				if (pchan)
					strcpy(ob->parsubstr, pchan->name);
				else
					ob->parsubstr[0]= 0;
					
				if(partype == PAR_PATH_CONST)
					; /* don't do anything here, since this is not technically "parenting" */
				else if( ELEM(partype, PAR_CURVE, PAR_LATTICE) || pararm )
				{
					/* partype is now set to PAROBJECT so that invisible 'virtual' modifiers don't need to be created
					 * NOTE: the old (2.4x) method was to set ob->partype = PARSKEL, creating the virtual modifiers
					 */
					ob->partype= PAROBJECT;	/* note, dna define, not operator property */
					//ob->partype= PARSKEL; /* note, dna define, not operator property */
					
					/* BUT, to keep the deforms, we need a modifier, and then we need to set the object that it uses */
					// XXX currently this should only happen for meshes, curves, surfaces, and lattices - this stuff isn't available for metas yet
					if (ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_LATTICE)) 
					{
						ModifierData *md;

						switch (partype) {
						case PAR_CURVE: /* curve deform */
							md= ED_object_modifier_add(op->reports, scene, ob, NULL, eModifierType_Curve);
							((CurveModifierData *)md)->object= par;
							break;
						case PAR_LATTICE: /* lattice deform */
							md= ED_object_modifier_add(op->reports, scene, ob, NULL, eModifierType_Lattice);
							((LatticeModifierData *)md)->object= par;
							break;
						default: /* armature deform */
							md= ED_object_modifier_add(op->reports, scene, ob, NULL, eModifierType_Armature);
							((ArmatureModifierData *)md)->object= par;
							break;
						}
					}
				}
				else if (partype == PAR_BONE)
					ob->partype= PARBONE; /* note, dna define, not operator property */
				else
					ob->partype= PAROBJECT;	/* note, dna define, not operator property */
				
				/* constraint */
				if(partype == PAR_PATH_CONST) {
					bConstraint *con;
					bFollowPathConstraint *data;
					float cmat[4][4], vec[3];
					
					con = add_ob_constraint(ob, "AutoPath", CONSTRAINT_TYPE_FOLLOWPATH);
					
					data = con->data;
					data->tar = par;
					
					get_constraint_target_matrix(scene, con, 0, CONSTRAINT_OBTYPE_OBJECT, NULL, cmat, scene->r.cfra - give_timeoffset(ob));
					sub_v3_v3v3(vec, ob->obmat[3], cmat[3]);
					
					ob->loc[0] = vec[0];
					ob->loc[1] = vec[1];
					ob->loc[2] = vec[2];
				}
				else if(pararm && ob->type==OB_MESH && par->type == OB_ARMATURE) {
					if(partype == PAR_ARMATURE_NAME)
						create_vgroups_from_armature(scene, ob, par, ARM_GROUPS_NAME);
					else if(partype == PAR_ARMATURE_ENVELOPE)
						create_vgroups_from_armature(scene, ob, par, ARM_GROUPS_ENVELOPE);
					else if(partype == PAR_ARMATURE_AUTO)
						create_vgroups_from_armature(scene, ob, par, ARM_GROUPS_AUTO);
					
					/* get corrected inverse */
					ob->partype= PAROBJECT;
					what_does_parent(scene, ob, &workob);
					
					invert_m4_m4(ob->parentinv, workob.obmat);
				}
				else {
					/* calculate inverse parent matrix */
					what_does_parent(scene, ob, &workob);
					invert_m4_m4(ob->parentinv, workob.obmat);
				}
				
				ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA;
			}
		}
	}
	CTX_DATA_END;
	
	DAG_scene_sort(scene);
	DAG_ids_flush_update(0);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

static int parent_set_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *ob= CTX_data_active_object(C);
	uiPopupMenu *pup= uiPupMenuBegin(C, "Set Parent To", 0);
	uiLayout *layout= uiPupMenuLayout(pup);
	
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_OBJECT);
	
	/* ob becomes parent, make the associated menus */
	if(ob->type==OB_ARMATURE) {
		uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_ARMATURE);
		uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_ARMATURE_NAME);
		uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_ARMATURE_ENVELOPE);
		uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_ARMATURE_AUTO);
		uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_BONE);
	}
	else if(ob->type==OB_CURVE) {
		uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_CURVE);
		uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_FOLLOW);
		uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_PATH_CONST);
	}
	else if(ob->type == OB_LATTICE) {
		uiItemEnumO(layout, NULL, 0, "OBJECT_OT_parent_set", "type", PAR_LATTICE);
	}
	
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}


void OBJECT_OT_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Parent";
	ot->description = "Set the object's parenting.";
	ot->idname= "OBJECT_OT_parent_set";
	
	/* api callbacks */
	ot->invoke= parent_set_invoke;
	ot->exec= parent_set_exec;
	
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_make_parent_types, 0, "Type", "");
}

/* ************ Make Parent Without Inverse Operator ******************* */

static int parent_noinv_set_exec(bContext *C, wmOperator *op)
{
	Object *par= CTX_data_active_object(C);
	
	par->recalc |= OB_RECALC_OB;
	
	/* context itterator */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if (ob != par) {
			if (test_parent_loop(par, ob)) {
				BKE_report(op->reports, RPT_ERROR, "Loop in parents");
			}
			else {
				/* clear inverse matrix and also the object location */
				unit_m4(ob->parentinv);
				memset(ob->loc, 0, 3*sizeof(float));
				
				/* set recalc flags */
				ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA;
				
				/* set parenting type for object - object only... */
				ob->parent= par;
				ob->partype= PAROBJECT;	/* note, dna define, not operator property */
			}
		}
	}
	CTX_DATA_END;
	
	DAG_scene_sort(CTX_data_scene(C));
	DAG_ids_flush_update(0);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_no_inverse_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Parent without Inverse";
	ot->description = "Set the object's parenting without setting the inverse parent correction.";
	ot->idname= "OBJECT_OT_parent_no_inverse_set";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= parent_noinv_set_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/************************ Clear Slow Parent Operator *********************/

static int object_slow_parent_clear_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);

	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(ob->parent) {
			if(ob->partype & PARSLOW) {
				ob->partype -= PARSLOW;
				where_is_object(scene, ob);
				ob->partype |= PARSLOW;
				ob->recalc |= OB_RECALC_OB;
			}
		}
	}
	CTX_DATA_END;

	DAG_ids_flush_update(0);
	WM_event_add_notifier(C, NC_SCENE, scene);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_slow_parent_clear(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Clear Slow Parent";
	ot->description = "Clear the object's slow parent.";
	ot->idname= "OBJECT_OT_slow_parent_clear";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_slow_parent_clear_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** Make Slow Parent Operator *********************/

static int object_slow_parent_set_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);

	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(ob->parent)
			ob->partype |= PARSLOW;

		ob->recalc |= OB_RECALC_OB;
		
	}
	CTX_DATA_END;

	DAG_ids_flush_update(0);
	WM_event_add_notifier(C, NC_SCENE, scene);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_slow_parent_set(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Set Slow Parent";
	ot->description = "Set the object's slow parent.";
	ot->idname= "OBJECT_OT_slow_parent_set";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_slow_parent_set_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
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
	int type= RNA_enum_get(op->ptr, "type");

	if(CTX_data_edit_object(C)) {
		BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed in EditMode");
		return OPERATOR_CANCELLED;
	}
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		ob->track= NULL;
		ob->recalc |= OB_RECALC;
		
		if(type == 1)
			ED_object_apply_obmat(ob);
	}
	CTX_DATA_END;

	DAG_ids_flush_update(0);
	DAG_scene_sort(CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

void OBJECT_OT_track_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear track";
	ot->description = "Clear tracking constraint or flag from object.";
	ot->idname= "OBJECT_OT_track_clear";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_track_clear_exec;
	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	ot->prop= RNA_def_enum(ot->srna, "type", prop_clear_track_types, 0, "Type", "");
}

/************************** Make Track Operator *****************************/

static EnumPropertyItem prop_make_track_types[] = {
	{1, "TRACKTO", 0, "TrackTo Constraint", ""},
	{2, "LOCKTRACK", 0, "LockTrack Constraint", ""},
	{3, "OLDTRACK", 0, "Old Track", ""},
	{0, NULL, 0, NULL, NULL}
};

static int track_set_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obact= CTX_data_active_object(C); 
	
	int type= RNA_enum_get(op->ptr, "type");
	
	if(type == 1) {
		bConstraint *con;
		bTrackToConstraint *data;

		CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
			if(ob!=obact) {
				con = add_ob_constraint(ob, "AutoTrack", CONSTRAINT_TYPE_TRACKTO);

				data = con->data;
				data->tar = obact;
				ob->recalc |= OB_RECALC;
				
				/* Lamp and Camera track differently by default */
				if (ob->type == OB_LAMP || ob->type == OB_CAMERA) {
					data->reserved1 = TRACK_nZ;
					data->reserved2 = UP_Y;
				}
			}
		}
		CTX_DATA_END;
	}
	else if(type == 2) {
		bConstraint *con;
		bLockTrackConstraint *data;

		CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
			if(ob!=obact) {
				con = add_ob_constraint(ob, "AutoTrack", CONSTRAINT_TYPE_LOCKTRACK);

				data = con->data;
				data->tar = obact;
				ob->recalc |= OB_RECALC;
				
				/* Lamp and Camera track differently by default */
				if (ob->type == OB_LAMP || ob->type == OB_CAMERA) {
					data->trackflag = TRACK_nZ;
					data->lockflag = LOCK_Y;
				}
			}
		}
		CTX_DATA_END;
	}
	else {
		CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
			if(ob!=obact) {
				ob->track= obact;
				ob->recalc |= OB_RECALC;
			}
		}
		CTX_DATA_END;
	}
	DAG_scene_sort(scene);
	DAG_ids_flush_update(0);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_track_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Track";
	ot->description = "Make the object track another object, either by constraint or old way or locked track.";
	ot->idname= "OBJECT_OT_track_set";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= track_set_exec;
	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", prop_make_track_types, 0, "Type", "");
}

/************************** Move to Layer Operator *****************************/

static unsigned int move_to_layer_init(bContext *C, wmOperator *op)
{
	int values[20], a;
	unsigned int lay= 0;

	if(!RNA_property_is_set(op->ptr, "layer")) {
		CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			lay |= base->lay;
		}
		CTX_DATA_END;

		for(a=0; a<20; a++)
			values[a]= (lay & (1<<a));
		
		RNA_boolean_set_array(op->ptr, "layer", values);
	}
	else {
		RNA_boolean_get_array(op->ptr, "layer", values);

		for(a=0; a<20; a++)
			if(values[a])
				lay |= (1 << a);
	}

	return lay;
}

static int move_to_layer_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	View3D *v3d= CTX_wm_view3d(C);
	if(v3d && v3d->localvd) {
		return WM_operator_confirm_message(C, op, "Move from localview");
	}
	else {
		move_to_layer_init(C, op);
		return WM_operator_props_popup(C, op, event);
	}
}

static int move_to_layer_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	unsigned int lay, local;
	int islamp= 0;
	
	lay= move_to_layer_init(C, op);
	lay &= 0xFFFFFF;

	if(lay==0) return OPERATOR_CANCELLED;
	
	if(v3d && v3d->localvd) {
		/* now we can move out of localview. */
		// XXX if (!okee("Move from localview")) return;
		CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			lay= base->lay & ~v3d->lay;
			base->lay= lay;
			base->object->lay= lay;
			base->object->flag &= ~SELECT;
			base->flag &= ~SELECT;
			if(base->object->type==OB_LAMP) islamp= 1;
		}
		CTX_DATA_END;
	}
	else {
		/* normal non localview operation */
		CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			/* upper byte is used for local view */
			local= base->lay & 0xFF000000;  
			base->lay= lay + local;
			base->object->lay= lay;
			if(base->object->type==OB_LAMP) islamp= 1;
		}
		CTX_DATA_END;
	}

	if(islamp) reshadeall_displist(scene);	/* only frees */
	
	/* warning, active object may be hidden now */
	
	WM_event_add_notifier(C, NC_SCENE|NC_OBJECT|ND_DRAW, scene); /* is NC_SCENE needed ? */
	DAG_scene_sort(scene);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_move_to_layer(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move to Layer";
	ot->description = "Move the object to different layers.";
	ot->idname= "OBJECT_OT_move_to_layer";
	
	/* api callbacks */
	ot->invoke= move_to_layer_invoke;
	ot->exec= move_to_layer_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layer", 20, NULL, "Layer", "");
}

/************************** Link to Scene Operator *****************************/

void link_to_scene(unsigned short nr)
{
#if 0
	Scene *sce= (Scene*) BLI_findlink(&G.main->scene, G.curscreen->scenenr-1);
	Base *base, *nbase;
	
	if(sce==0) return;
	if(sce->id.lib) return;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASE(v3d, base)) {
			
			nbase= MEM_mallocN( sizeof(Base), "newbase");
			*nbase= *base;
			BLI_addhead( &(sce->base), nbase);
			id_us_plus((ID *)base->object);
		}
	}
#endif
}

static int make_links_scene_exec(bContext *C, wmOperator *op)
{
	Scene *scene_to= BLI_findlink(&CTX_data_main(C)->scene, RNA_enum_get(op->ptr, "type"));

	if(scene_to==NULL) {
		BKE_report(op->reports, RPT_ERROR, "Scene not found");
		return OPERATOR_CANCELLED;
	}

	if(scene_to == CTX_data_scene(C)) {
		BKE_report(op->reports, RPT_ERROR, "Can't link objects into the same scene");
		return OPERATOR_CANCELLED;
	}

	CTX_DATA_BEGIN(C, Base*, base, selected_bases)
	{
		if(!object_in_scene(base->object, scene_to)) {
			Base *nbase= MEM_mallocN( sizeof(Base), "newbase");
			*nbase= *base;
			BLI_addhead( &(scene_to->base), nbase);
			id_us_plus((ID *)base->object);
		}
	}
	CTX_DATA_END;

	DAG_ids_flush_update(0);

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

static int make_links_data_exec(bContext *C, wmOperator *op)
{
	int event = RNA_int_get(op->ptr, "type");
	Object *ob;
	ID *id;
	int a;

	ob= CTX_data_active_object(C);

	CTX_DATA_BEGIN(C, Object*, obt, selected_editable_objects) {
		if(ob != obt) {
			switch(event) {
			case MAKE_LINKS_OBDATA: /* obdata */
				id= obt->data;
				id->us--;

				id= ob->data;
				id_us_plus(id);
				obt->data= id;

				/* if amount of material indices changed: */
				test_object_materials(obt->data);

				obt->recalc |= OB_RECALC_DATA;
				break;
			case MAKE_LINKS_MATERIALS:
				/* new approach, using functions from kernel */
				for(a=0; a<ob->totcol; a++) {
					Material *ma= give_current_material(ob, a+1);
					assign_material(obt, ma, a+1);	/* also works with ma==NULL */
				}
				break;
			case MAKE_LINKS_ANIMDATA:
				BKE_copy_animdata_id((ID *)obt, (ID *)ob);
				BKE_copy_animdata_id((ID *)obt->data, (ID *)ob->data);
				break;
			case MAKE_LINKS_DUPLIGROUP:
				if(ob->dup_group) ob->dup_group->id.us--;
				obt->dup_group= ob->dup_group;
				if(obt->dup_group) {
					id_us_plus((ID *)obt->dup_group);
					obt->transflag |= OB_DUPLIGROUP;
				}
				break;
			case MAKE_LINKS_MODIFIERS:
				object_link_modifiers(obt, ob);
				obt->recalc |= OB_RECALC;
				break;
			}
		}
	}
	CTX_DATA_END;

	DAG_ids_flush_update(0);
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, CTX_wm_view3d(C));
	return OPERATOR_FINISHED;
}


void OBJECT_OT_make_links_scene(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Link Objects to Scene";
	ot->description = "Make linked data local to each object.";
	ot->idname= "OBJECT_OT_make_links_scene";

	/* api callbacks */
	ot->exec= make_links_scene_exec;
	/* better not run the poll check */

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	prop= RNA_def_enum(ot->srna, "type", DummyRNA_NULL_items, 0, "Type", "");
	RNA_def_enum_funcs(prop, RNA_scene_itemf);
}

void OBJECT_OT_make_links_data(wmOperatorType *ot)
{
	static EnumPropertyItem make_links_items[]= {
		{MAKE_LINKS_OBDATA,		"OBDATA", 0, "Object Data", ""},
		{MAKE_LINKS_MATERIALS,	"MATERIAL", 0, "Materials", ""},
		{MAKE_LINKS_ANIMDATA,	"ANIMATION", 0, "Animation Data", ""},
		{MAKE_LINKS_DUPLIGROUP,	"DUPLIGROUP", 0, "DupliGroup", ""},
		{MAKE_LINKS_MODIFIERS,	"MODIFIERS", 0, "Modifiers", ""},
		{0, NULL, 0, NULL, NULL}};

	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Link Data";
	ot->description = "Make links from the active object to other selected objects.";
	ot->idname= "OBJECT_OT_make_links_data";

	/* api callbacks */
	ot->exec= make_links_data_exec;
	ot->poll= ED_operator_scene_editable;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	prop= RNA_def_enum(ot->srna, "type", make_links_items, 0, "Type", "");
}


/**************************** Make Single User ********************************/

static void single_object_users__forwardModifierLinks(void *userData, Object *ob, Object **obpoin)
{
	ID_NEW(*obpoin);
}

void single_object_users(Scene *scene, View3D *v3d, int flag)	
{
	Base *base;
	Object *ob, *obn;
	
	clear_sca_new_poins();	/* sensor/contr/act */

	/* duplicate (must set newid) */
	for(base= FIRSTBASE; base; base= base->next) {
		ob= base->object;
		
		if( (base->flag & flag)==flag ) {
			if(ob->id.lib==NULL && ob->id.us>1) {
				/* base gets copy of object */
				obn= copy_object(ob);
				base->object= obn;
				ob->id.us--;
			}
		}
	}
	
	ID_NEW(scene->camera);
	if(v3d) ID_NEW(v3d->camera);
	
	/* object pointers */
	for(base= FIRSTBASE; base; base= base->next) {
		ob= base->object;
		if(ob->id.lib==NULL) {
			relink_constraints(&base->object->constraints);
			if (base->object->pose){
				bPoseChannel *chan;
				for (chan = base->object->pose->chanbase.first; chan; chan=chan->next){
					relink_constraints(&chan->constraints);
				}
			}
			modifiers_foreachObjectLink(base->object, single_object_users__forwardModifierLinks, NULL);
			
			ID_NEW(ob->parent);
			ID_NEW(ob->track);
		}
	}

	set_sca_new_poins();
}

void new_id_matar(Material **matar, int totcol)
{
	ID *id;
	int a;
	
	for(a=0; a<totcol; a++) {
		id= (ID *)matar[a];
		if(id && id->lib==0) {
			if(id->newid) {
				matar[a]= (Material *)id->newid;
				id_us_plus(id->newid);
				id->us--;
			}
			else if(id->us>1) {
				matar[a]= copy_material(matar[a]);
				id->us--;
				id->newid= (ID *)matar[a];
			}
		}
	}
}

void single_obdata_users(Scene *scene, int flag)
{
	Object *ob;
	Lamp *la;
	Curve *cu;
	//Camera *cam;
	Base *base;
	Mesh *me;
	ID *id;
	int a;

	for(base= FIRSTBASE; base; base= base->next) {
		ob= base->object;
		if(ob->id.lib==NULL && (base->flag & flag)==flag ) {
			id= ob->data;
			
			if(id && id->us>1 && id->lib==0) {
				ob->recalc= OB_RECALC_DATA;
				
				switch(ob->type) {
				case OB_LAMP:
					if(id && id->us>1 && id->lib==NULL) {
						ob->data= la= copy_lamp(ob->data);
						for(a=0; a<MAX_MTEX; a++) {
							if(la->mtex[a]) {
								ID_NEW(la->mtex[a]->object);
							}
						}
					}
					break;
				case OB_CAMERA:
					ob->data= copy_camera(ob->data);
					break;
				case OB_MESH:
					me= ob->data= copy_mesh(ob->data);
					//if(me && me->key)
					//	ipo_idnew(me->key->ipo);	/* drivers */
					break;
				case OB_MBALL:
					ob->data= copy_mball(ob->data);
					break;
				case OB_CURVE:
				case OB_SURF:
				case OB_FONT:
					ob->data= cu= copy_curve(ob->data);
					ID_NEW(cu->bevobj);
					ID_NEW(cu->taperobj);
					break;
				case OB_LATTICE:
					ob->data= copy_lattice(ob->data);
					break;
				case OB_ARMATURE:
					ob->recalc |= OB_RECALC_DATA;
					ob->data= copy_armature(ob->data);
					armature_rebuild_pose(ob, ob->data);
					break;
				default:
					printf("ERROR single_obdata_users: can't copy %s\n", id->name);
					return;
				}
				
				id->us--;
				id->newid= ob->data;
				
			}
			
#if 0 // XXX old animation system
			id= (ID *)ob->action;
			if (id && id->us>1 && id->lib==NULL){
				if(id->newid){
					ob->action= (bAction *)id->newid;
					id_us_plus(id->newid);
				}
				else {
					ob->action= copy_action(ob->action);
					id->us--;
					id->newid=(ID *)ob->action;
				}
			}
			id= (ID *)ob->ipo;
			if(id && id->us>1 && id->lib==NULL) {
				if(id->newid) {
					ob->ipo= (Ipo *)id->newid;
					id_us_plus(id->newid);
				}
				else {
					ob->ipo= copy_ipo(ob->ipo);
					id->us--;
					id->newid= (ID *)ob->ipo;
				}
				ipo_idnew(ob->ipo);	/* drivers */
			}
			/* other ipos */
			switch(ob->type) {
			case OB_LAMP:
				la= ob->data;
				if(la->ipo && la->ipo->id.us>1) {
					la->ipo->id.us--;
					la->ipo= copy_ipo(la->ipo);
					ipo_idnew(la->ipo);	/* drivers */
				}
				break;
			case OB_CAMERA:
				cam= ob->data;
				if(cam->ipo && cam->ipo->id.us>1) {
					cam->ipo->id.us--;
					cam->ipo= copy_ipo(cam->ipo);
					ipo_idnew(cam->ipo);	/* drivers */
				}
				break;
			}
#endif // XXX old animation system
		}
	}
	
	me= G.main->mesh.first;
	while(me) {
		ID_NEW(me->texcomesh);
		me= me->id.next;
	}
}

void single_ipo_users(Scene *scene, int flag)
{
#if 0 // XXX old animation system
	Object *ob;
	Base *base;
	ID *id;
	
	for(base= FIRSTBASE; base; base= base->next) {
		ob= base->object;
		if(ob->id.lib==NULL && (flag==0 || (base->flag & SELECT)) ) {
			ob->recalc= OB_RECALC_DATA;
			
			id= (ID *)ob->ipo;
			if(id && id->us>1 && id->lib==NULL) {
				ob->ipo= copy_ipo(ob->ipo);
				id->us--;
				ipo_idnew(ob->ipo);	/* drivers */
			}
		}
	}
#endif // XXX old animation system
}

static void single_mat_users(Scene *scene, int flag, int do_textures)
{
	Object *ob;
	Base *base;
	Material *ma, *man;
	Tex *tex;
	int a, b;
	
	for(base= FIRSTBASE; base; base= base->next) {
		ob= base->object;
		if(ob->id.lib==NULL && (flag==0 || (base->flag & SELECT)) ) {
	
			for(a=1; a<=ob->totcol; a++) {
				ma= give_current_material(ob, a);
				if(ma) {
					/* do not test for LIB_NEW: this functions guaranteed delivers single_users! */
					
					if(ma->id.us>1) {
						man= copy_material(ma);
					
						man->id.us= 0;
						assign_material(ob, man, a);
	
#if 0 // XXX old animation system						
						if(ma->ipo) {
							man->ipo= copy_ipo(ma->ipo);
							ma->ipo->id.us--;
							ipo_idnew(ma->ipo);	/* drivers */
						}
#endif // XXX old animation system
						if(do_textures) {
							for(b=0; b<MAX_MTEX; b++) {
								if(ma->mtex[b] && ma->mtex[b]->tex) {
									tex= ma->mtex[b]->tex;
									if(tex->id.us>1) {
										ma->mtex[b]->tex= copy_texture(tex);
										tex->id.us--;
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

void do_single_tex_user(Tex **from)
{
	Tex *tex, *texn;
	
	tex= *from;
	if(tex==0) return;
	
	if(tex->id.newid) {
		*from= (Tex *)tex->id.newid;
		id_us_plus(tex->id.newid);
		tex->id.us--;
	}
	else if(tex->id.us>1) {
		texn= copy_texture(tex);
		tex->id.newid= (ID *)texn;
		tex->id.us--;
		*from= texn;
	}
}

void single_tex_users_expand()
{
	/* only when 'parent' blocks are LIB_NEW */
	Main *bmain= G.main;
	Material *ma;
	Lamp *la;
	World *wo;
	int b;
		
	for(ma= bmain->mat.first; ma; ma=ma->id.next) {
		if(ma->id.flag & LIB_NEW) {
			for(b=0; b<MAX_MTEX; b++) {
				if(ma->mtex[b] && ma->mtex[b]->tex) {
					do_single_tex_user( &(ma->mtex[b]->tex) );
				}
			}
		}
	}

	for(la= bmain->lamp.first; la; la=la->id.next) {
		if(la->id.flag & LIB_NEW) {
			for(b=0; b<MAX_MTEX; b++) {
				if(la->mtex[b] && la->mtex[b]->tex) {
					do_single_tex_user( &(la->mtex[b]->tex) );
				}
			}
		}
	}

	for(wo= bmain->world.first; wo; wo=wo->id.next) {
		if(wo->id.flag & LIB_NEW) {
			for(b=0; b<MAX_MTEX; b++) {
				if(wo->mtex[b] && wo->mtex[b]->tex) {
					do_single_tex_user( &(wo->mtex[b]->tex) );
				}
			}
		}
	}
}

static void single_mat_users_expand(void)
{
	/* only when 'parent' blocks are LIB_NEW */
	Main *bmain= G.main;
	Object *ob;
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	Material *ma;
	int a;
	
	for(ob=bmain->object.first; ob; ob=ob->id.next)
		if(ob->id.flag & LIB_NEW)
			new_id_matar(ob->mat, ob->totcol);

	for(me=bmain->mesh.first; me; me=me->id.next)
		if(me->id.flag & LIB_NEW)
			new_id_matar(me->mat, me->totcol);

	for(cu=bmain->curve.first; cu; cu=cu->id.next)
		if(cu->id.flag & LIB_NEW)
			new_id_matar(cu->mat, cu->totcol);

	for(mb=bmain->mball.first; mb; mb=mb->id.next)
		if(mb->id.flag & LIB_NEW)
			new_id_matar(mb->mat, mb->totcol);

	/* material imats  */
	for(ma=bmain->mat.first; ma; ma=ma->id.next)
		if(ma->id.flag & LIB_NEW)
			for(a=0; a<MAX_MTEX; a++)
				if(ma->mtex[a])
					ID_NEW(ma->mtex[a]->object);
}

/* used for copying scenes */
void ED_object_single_users(Scene *scene, int full)
{
    single_object_users(scene, NULL, 0);

    if(full) {
        single_obdata_users(scene, 0);
        single_mat_users_expand();
        single_tex_users_expand();
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
	
	for(b=0; b<MAX_MTEX; b++)
		if(ma->mtex[b] && ma->mtex[b]->tex)
			id_make_local(&ma->mtex[b]->tex->id, 0);
	
	adt= BKE_animdata_from_id(&ma->id);
	if(adt) BKE_animdata_make_local(adt);

	/* nodetree? XXX */
}

static int make_local_exec(bContext *C, wmOperator *op)
{
	AnimData *adt;
	ParticleSystem *psys;
	Material *ma, ***matarar;
	Lamp *la;
	ID *id;
	int a, b, mode= RNA_enum_get(op->ptr, "type");;
	
	if(mode==3) {
		all_local(NULL, 0);	/* NULL is all libs */
		WM_event_add_notifier(C, NC_WINDOW, NULL);
		return OPERATOR_FINISHED;
	}

	clear_id_newpoins();
	
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(ob->id.lib)
			id_make_local(&ob->id, 0);
	}
	CTX_DATA_END;
	
	/* maybe object pointers */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(ob->id.lib==NULL) {
			ID_NEW(ob->parent);
			ID_NEW(ob->track);
		}
	}
	CTX_DATA_END;

	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		id= ob->data;
			
		if(id && mode>1) {
			id_make_local(id, 0);
			adt= BKE_animdata_from_id(id);
			if(adt) BKE_animdata_make_local(adt);
		}

		for(psys=ob->particlesystem.first; psys; psys=psys->next)
			id_make_local(&psys->part->id, 0);

		adt= BKE_animdata_from_id(&ob->id);
		if(adt) BKE_animdata_make_local(adt);
	}
	CTX_DATA_END;

	if(mode>1) {
		CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
			if(ob->type==OB_LAMP) {
				la= ob->data;

				for(b=0; b<MAX_MTEX; b++)
					if(la->mtex[b] && la->mtex[b]->tex)
						id_make_local(&la->mtex[b]->tex->id, 0);
			}
			else {
				for(a=0; a<ob->totcol; a++) {
					ma= ob->mat[a];
					if(ma)
						make_local_makelocalmaterial(ma);
				}
				
				matarar= (Material ***)give_matarar(ob);
				if(matarar) {
					for(a=0; a<ob->totcol; a++) {
						ma= (*matarar)[a];
						if(ma)
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
	static EnumPropertyItem type_items[]= {
		{1, "SELECTED_OBJECTS", 0, "Selected Objects", ""},
		{2, "SELECTED_OBJECTS_DATA", 0, "Selected Objects and Data", ""},
		{3, "ALL", 0, "All", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Make Local";
	ot->description = "Make library linked datablocks local to this file.";
	ot->idname= "OBJECT_OT_make_local";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= make_local_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

static int make_single_user_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C); /* ok if this is NULL */
	int flag= RNA_enum_get(op->ptr, "type"); /* 0==ALL, SELECTED==selected objecs */

	if(RNA_boolean_get(op->ptr, "object"))
	    single_object_users(scene, v3d, flag);

	if(RNA_boolean_get(op->ptr, "obdata"))
		single_obdata_users(scene, flag);

	if(RNA_boolean_get(op->ptr, "material"))
		single_mat_users(scene, flag, FALSE);

	if(RNA_boolean_get(op->ptr, "texture"))
		single_mat_users(scene, flag, TRUE);

	if(RNA_boolean_get(op->ptr, "animation"))
		single_ipo_users(scene, flag);

	clear_id_newpoins();

	WM_event_add_notifier(C, NC_WINDOW, NULL);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_make_single_user(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[]= {
		{SELECT, "SELECTED_OBJECTS", 0, "Selected Objects", ""},
		{0, "ALL", 0, "All", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Make Single User";
	ot->description = "Make linked data local to each object.";
	ot->idname= "OBJECT_OT_make_single_user";

	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= make_single_user_exec;
	ot->poll= ED_operator_scene_editable;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");

	RNA_def_boolean(ot->srna, "object", 0, "Object", "Make single user objects");
	RNA_def_boolean(ot->srna, "obdata", 0, "Object Data", "Make single user object data");
	RNA_def_boolean(ot->srna, "material", 0, "Materials", "Make materials local to each datablock");
	RNA_def_boolean(ot->srna, "texture", 0, "Textures", "Make textures local to each material");
	RNA_def_boolean(ot->srna, "animation", 0, "Animation Data", "Make animation data local to each object");
}
