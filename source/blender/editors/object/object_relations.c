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

#include "BLI_arithb.h"
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
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "object_intern.h"

/* ************* XXX **************** */
static int pupmenu(const char *msg) {return 0;}
static int pupmenu_col(const char *msg, int val) {return 0;}

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
					Mat4Invert(ob->parentinv, workob.obmat);
				}
				else {
					ob->partype= PARVERT1;
					ob->par1= v1-1;

					/* inverse parent matrix */
					what_does_parent(scene, ob, &workob);
					Mat4Invert(ob->parentinv, workob.obmat);
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
	ot->poll= vertex_parent_set_poll;
	ot->exec= vertex_parent_set_exec;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** Make Proxy Operator *************************/

/* present menu listing the possible objects within the group to proxify */
static void proxy_group_objects_menu (bContext *C, wmOperator *op, Object *ob, Group *group)
{
	uiPopupMenu *pup;
	uiLayout *layout;
	GroupObject *go;
	int len=0;
	
	/* check if there are any objects within the group to assign for */
	for (go= group->gobject.first; go; go= go->next) {
		if (go->ob) len++;
	}
	if (len==0) return;
	
	/* now create the menu to draw */
	pup= uiPupMenuBegin(C, "Make Proxy For:", 0);
	layout= uiPupMenuLayout(pup);
	
	for (go= group->gobject.first; go; go= go->next) {
		if (go->ob) {
			PointerRNA props_ptr;
			
			/* create operator menu item with relevant properties filled in */
			props_ptr= uiItemFullO(layout, go->ob->id.name+2, 0, op->idname, NULL, WM_OP_EXEC_REGION_WIN, UI_ITEM_O_RETURN_PROPS);
			RNA_string_set(&props_ptr, "object", go->ob->id.name+2);
			RNA_string_set(&props_ptr, "group_object", go->ob->id.name+2);
		}
	}
	
	/* display the menu, and be done */
	uiPupMenuEnd(C, pup);
}

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
		proxy_group_objects_menu(C, op, ob, ob->dup_group);
	}
	else if (ob->id.lib) {
		uiPopupMenu *pup= uiPupMenuBegin(C, "OK?", ICON_QUESTION);
		uiLayout *layout= uiPupMenuLayout(pup);
		PointerRNA props_ptr;
		
		/* create operator menu item with relevant properties filled in */
		props_ptr= uiItemFullO(layout, op->type->name, 0, op->idname, NULL, WM_OP_EXEC_REGION_WIN, UI_ITEM_O_RETURN_PROPS);
		RNA_string_set(&props_ptr, "object", ob->id.name+2);
		
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
	Object *ob=NULL, *gob=NULL;
	Scene *scene= CTX_data_scene(C);
	char ob_name[21], gob_name[21];
	
	/* get object and group object
	 *	- firstly names
	 *	- then pointers from context 
	 */
	RNA_string_get(op->ptr, "object", ob_name);
	RNA_string_get(op->ptr, "group_object", gob_name);
	
	if (gob_name[0]) {
		Group *group;
		GroupObject *go;
		
		/* active object is group object... */
		// FIXME: we should get the nominated name instead
		gob= CTX_data_active_object(C);
		group= gob->dup_group;
		
		/* find the object to affect */
		for (go= group->gobject.first; go; go= go->next) {
			if ((go->ob) && strcmp(go->ob->id.name+2, gob_name)==0) {
				ob= go->ob;
				break;
			}
		}
	}
	else {
		/* just use the active object for now */
		// FIXME: we should get the nominated name instead
		ob= CTX_data_active_object(C);
	}
	
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
		
		WM_event_add_notifier(C, NC_OBJECT, NULL);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No object to make proxy for");
		return OPERATOR_CANCELLED;
	}
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_proxy_make (wmOperatorType *ot)
{
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
	RNA_def_string(ot->srna, "group_object", "", 19, "Group Object", "Name of group instancer (if applicable).");
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
			Mat4One(ob->parentinv);

		ob->recalc |= OB_RECALC;
	}
	CTX_DATA_END;
	
	DAG_scene_sort(CTX_data_scene(C));
	ED_anim_dag_flush_update(C);
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
	
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_clear_parent_types, 0, "Type", "");
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
			
			/* fall back on regular parenting now */
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
				
				ob->parent= par;
				
				/* handle types */
				if (pchan)
					strcpy (ob->parsubstr, pchan->name);
				else
					ob->parsubstr[0]= 0;
				
				/* constraint */
				if(partype==PAR_PATH_CONST) {
					bConstraint *con;
					bFollowPathConstraint *data;
					float cmat[4][4], vec[3];
					
					con = add_new_constraint(CONSTRAINT_TYPE_FOLLOWPATH);
					strcpy (con->name, "AutoPath");
					
					data = con->data;
					data->tar = par;
					
					add_constraint_to_object(con, ob);
					
					get_constraint_target_matrix(con, 0, CONSTRAINT_OBTYPE_OBJECT, NULL, cmat, scene->r.cfra - give_timeoffset(ob));
					VecSubf(vec, ob->obmat[3], cmat[3]);
					
					ob->loc[0] = vec[0];
					ob->loc[1] = vec[1];
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
					
					ob->partype= PARSKEL;

					Mat4Invert(ob->parentinv, workob.obmat);
				}
				else {
					/* calculate inverse parent matrix */
					what_does_parent(scene, ob, &workob);
					Mat4Invert(ob->parentinv, workob.obmat);
				}
				
				ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA;
				
				if( ELEM(partype, PAR_CURVE, PAR_LATTICE) || pararm )
					ob->partype= PARSKEL; /* note, dna define, not operator property */
				else
					ob->partype= PAROBJECT;	/* note, dna define, not operator property */
			}
		}
	}
	CTX_DATA_END;
	
	DAG_scene_sort(CTX_data_scene(C));
	ED_anim_dag_flush_update(C);
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
	
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_make_parent_types, 0, "Type", "");
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

	ED_anim_dag_flush_update(C);	
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

	ED_anim_dag_flush_update(C);	
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

	DAG_scene_sort(CTX_data_scene(C));
	ED_anim_dag_flush_update(C);

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
	
	RNA_def_enum(ot->srna, "type", prop_clear_track_types, 0, "Type", "");
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
	int type= RNA_enum_get(op->ptr, "type");
		
	if(type == 1) {
		bConstraint *con;
		bTrackToConstraint *data;

		CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			if(base!=BASACT) {
				con = add_new_constraint(CONSTRAINT_TYPE_TRACKTO);
				strcpy (con->name, "AutoTrack");

				data = con->data;
				data->tar = BASACT->object;
				base->object->recalc |= OB_RECALC;
				
				/* Lamp and Camera track differently by default */
				if (base->object->type == OB_LAMP || base->object->type == OB_CAMERA) {
					data->reserved1 = TRACK_nZ;
					data->reserved2 = UP_Y;
				}

				add_constraint_to_object(con, base->object);
			}
		}
		CTX_DATA_END;
	}
	else if(type == 2) {
		bConstraint *con;
		bLockTrackConstraint *data;

		CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			if(base!=BASACT) {
				con = add_new_constraint(CONSTRAINT_TYPE_LOCKTRACK);
				strcpy (con->name, "AutoTrack");

				data = con->data;
				data->tar = BASACT->object;
				base->object->recalc |= OB_RECALC;
				
				/* Lamp and Camera track differently by default */
				if (base->object->type == OB_LAMP || base->object->type == OB_CAMERA) {
					data->trackflag = TRACK_nZ;
					data->lockflag = LOCK_Y;
				}

				add_constraint_to_object(con, base->object);
			}
		}
		CTX_DATA_END;
	}
	else {
		CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			if(base!=BASACT) {
				base->object->track= BASACT->object;
				base->object->recalc |= OB_RECALC;
			}
		}
		CTX_DATA_END;
	}
	DAG_scene_sort(CTX_data_scene(C));
	ED_anim_dag_flush_update(C);	
	
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
	RNA_def_enum(ot->srna, "type", prop_make_track_types, 0, "Type", "");
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
	move_to_layer_init(C, op);
	return WM_operator_props_popup(C, op, event);
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
	
	if(v3d && v3d->localview) {
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
	
	WM_event_add_notifier(C, NC_SCENE, scene);
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


void make_links(bContext *C, wmOperator *op, Scene *scene, View3D *v3d, short event)
{
	Object *ob, *obt;
	Base *base, *nbase, *sbase;
	Scene *sce = NULL;
	ID *id;
	int a;
	short nr=0;
	char *strp;

	if(!(ob=OBACT)) return;

	if(event==1) {
		IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->scene), 0, &nr);
		
		if(nr == -2) {
			MEM_freeN(strp);

// XXX			activate_databrowse((ID *)scene, ID_SCE, 0, B_INFOSCE, &(G.curscreen->scenenr), link_to_scene );
			
			return;			
		}
		else {
			event= pupmenu_col(strp, 20);
			MEM_freeN(strp);
		
			if(event<= 0) return;
		
			nr= 1;
			sce= G.main->scene.first;
			while(sce) {
				if(nr==event) break;
				nr++;
				sce= sce->id.next;
			}
			if(sce==scene) {
				BKE_report(op->reports, RPT_ERROR, "This is the current scene");
				return;
			}
			if(sce==0 || sce->id.lib) return;
			
			/* remember: is needed below */
			event= 1;
		}
	}

	/* All non group linking */
	for(base= FIRSTBASE; base; base= base->next) {
		if(event==1 || base != BASACT) {
			
			obt= base->object;

			if(TESTBASE(v3d, base)) {
				
				if(event==1) {		/* to scene */
					
					/* test if already linked */
					sbase= sce->base.first;
					while(sbase) {
						if(sbase->object==base->object) break;
						sbase= sbase->next;
					}
					if(sbase) {	/* remove */
						continue;
					}
					
					nbase= MEM_mallocN( sizeof(Base), "newbase");
					*nbase= *base;
					BLI_addhead( &(sce->base), nbase);
					id_us_plus((ID *)base->object);
				}
			}
			if(TESTBASELIB(v3d, base)) {
				if(event==2 || event==5) {  /* obdata */
					if(ob->type==obt->type) {
						
							id= obt->data;
							id->us--;
							
							id= ob->data;
							id_us_plus(id);
							obt->data= id;
							
							/* if amount of material indices changed: */
							test_object_materials(obt->data);

							obt->recalc |= OB_RECALC_DATA;
						}
					}
				else if(event==4) {  /* ob ipo */
#if 0 // XXX old animation system
					if(obt->ipo) obt->ipo->id.us--;
					obt->ipo= ob->ipo;
					if(obt->ipo) {
						id_us_plus((ID *)obt->ipo);
						do_ob_ipo(scene, obt);
					}
#endif // XXX old animation system
				}
				else if(event==6) {
					if(ob->dup_group) ob->dup_group->id.us--;
					obt->dup_group= ob->dup_group;
					if(obt->dup_group) {
						id_us_plus((ID *)obt->dup_group);
						obt->transflag |= OB_DUPLIGROUP;
					}
				}
				else if(event==3) {  /* materials */
					
					/* new approach, using functions from kernel */
					for(a=0; a<ob->totcol; a++) {
						Material *ma= give_current_material(ob, a+1);
						assign_material(obt, ma, a+1);	/* also works with ma==NULL */
					}
				}
			}
		}
	}
	
	ED_anim_dag_flush_update(C);	

}

void make_links_menu(bContext *C, Scene *scene, View3D *v3d)
{
	Object *ob;
	short event=0;
	char str[140];
	
	if(!(ob=OBACT)) return;
	
	strcpy(str, "Make Links %t|To Scene...%x1|%l|Object Ipo%x4");
	
	if(ob->type==OB_MESH)
		strcat(str, "|Mesh Data%x2|Materials%x3");
	else if(ob->type==OB_CURVE)
		strcat(str, "|Curve Data%x2|Materials%x3");
	else if(ob->type==OB_FONT)
		strcat(str, "|Text Data%x2|Materials%x3");
	else if(ob->type==OB_SURF)
		strcat(str, "|Surface Data%x2|Materials%x3");
	else if(ob->type==OB_MBALL)
		strcat(str, "|Materials%x3");
	else if(ob->type==OB_CAMERA)
		strcat(str, "|Camera Data%x2");
	else if(ob->type==OB_LAMP)
		strcat(str, "|Lamp Data%x2");
	else if(ob->type==OB_LATTICE)
		strcat(str, "|Lattice Data%x2");
	else if(ob->type==OB_ARMATURE)
		strcat(str, "|Armature Data%x2");
	
	event= pupmenu(str);
	
	if(event<= 0) return;
	
	make_links(C, NULL, scene, v3d, event);
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

void single_mat_users(Scene *scene, int flag)
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

void single_user(Scene *scene, View3D *v3d)
{
	int nr;
	
	if(scene->id.lib) return;

	clear_id_newpoins();
	
	nr= pupmenu("Make Single User%t|Object|Object & ObData|Object & ObData & Materials+Tex|Materials+Tex|Ipos");
	if(nr>0) {
	
		if(nr==1) single_object_users(scene, v3d, 1);
	
		else if(nr==2) {
			single_object_users(scene, v3d, 1);
			single_obdata_users(scene, 1);
		}
		else if(nr==3) {
			single_object_users(scene, v3d, 1);
			single_obdata_users(scene, 1);
			single_mat_users(scene, 1); /* also tex */
			
		}
		else if(nr==4) {
			single_mat_users(scene, 1);
		}
		else if(nr==5) {
			single_ipo_users(scene, 1);
		}
		
		
		clear_id_newpoins();

	}
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
	int a, b, mode= RNA_boolean_get(op->ptr, "type");
	
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
	RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

