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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_particle_types.h"
#include "DNA_property_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_vfont_types.h"
#include "DNA_world_types.h"
#include "DNA_modifier_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"
#include "BLI_rand.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_booleanops.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_blender.h"
#include "BKE_booleanops.h"
#include "BKE_cloth.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_property.h"
#include "BKE_report.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_modifier.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_particle.h"
#include "ED_mesh.h"
#include "ED_mball.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_types.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

/* for menu/popup icons etc etc*/
#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"	// own include

/* ************* XXX **************** */
static void error() {}
static void waitcursor(int val) {}
static int pupmenu(const char *msg) {return 0;}
static int pupmenu_col(const char *msg, int val) {return 0;}
static int okee(const char *msg) {return 0;}

/* port over here */
static bContext *C;
static void error_libdata() {}

/* ********************************** */

/* --------------------------------- */

/* simple API for object selection, rather than just using the flag
 * this takes into account the 'restrict selection in 3d view' flag.
 * deselect works always, the restriction just prevents selection */

/* Note: send a NC_SCENE|ND_OB_SELECT notifier yourself! */

void ED_base_object_select(Base *base, short mode)
{
	if (base) {
		if (mode==BA_SELECT) {
			if (!(base->object->restrictflag & OB_RESTRICT_SELECT))
				if (mode==BA_SELECT) base->flag |= SELECT;
		}
		else if (mode==BA_DESELECT) {
			base->flag &= ~SELECT;
		}
		base->object->flag= base->flag;
	}
}

/* also to set active NULL */
void ED_base_object_activate(bContext *C, Base *base)
{
	Scene *scene= CTX_data_scene(C);
	Base *tbase;
	
	/* sets scene->basact */
	BASACT= base;
	
	if(base) {
		
		/* XXX old signals, remember to handle notifiers now! */
		//		select_actionchannel_by_name(base->object->action, "Object", 1);
		
		/* disable temporal locks */
		for(tbase=FIRSTBASE; tbase; tbase= tbase->next) {
			if(base!=tbase && (tbase->object->shapeflag & OB_SHAPE_TEMPLOCK)) {
				tbase->object->shapeflag &= ~OB_SHAPE_TEMPLOCK;
				DAG_object_flush_update(scene, tbase->object, OB_RECALC_DATA);
			}
		}
		WM_event_add_notifier(C, NC_SCENE|ND_OB_ACTIVE, scene);
	}
	else
		WM_event_add_notifier(C, NC_SCENE|ND_OB_ACTIVE, NULL);
}


/* exported */
void ED_object_base_init_from_view(bContext *C, Base *base)
{
	View3D *v3d= CTX_wm_view3d(C);
	Scene *scene= CTX_data_scene(C);
	Object *ob= base->object;
	
	if (scene==NULL)
		return;
	
	if (v3d==NULL) {
		base->lay = scene->lay;
		VECCOPY(ob->loc, scene->cursor);
	} 
	else {
		if (v3d->localview) {
			base->lay= ob->lay= v3d->layact | v3d->lay;
			VECCOPY(ob->loc, v3d->cursor);
		} 
		else {
			base->lay= ob->lay= v3d->layact;
			VECCOPY(ob->loc, scene->cursor);
		}
		
		if (U.flag & USER_ADD_VIEWALIGNED) {
			ARegion *ar= CTX_wm_region(C);
			if(ar) {
				RegionView3D *rv3d= ar->regiondata;
				
				rv3d->viewquat[0]= -rv3d->viewquat[0];
				QuatToEul(rv3d->viewquat, ob->rot);
				rv3d->viewquat[0]= -rv3d->viewquat[0];
			}
		}
	}
	where_is_object(scene, ob);
}

/* ******************* add object operator ****************** */

static EnumPropertyItem prop_object_types[] = {
	{OB_MESH, "MESH", 0, "Mesh", ""},
	{OB_CURVE, "CURVE", 0, "Curve", ""},
	{OB_SURF, "SURFACE", 0, "Surface", ""},
	{OB_MBALL, "META", 0, "Meta", ""},
	{OB_FONT, "TEXT", 0, "Text", ""},
	{0, "", 0, NULL, NULL},
	{OB_ARMATURE, "ARMATURE", 0, "Armature", ""},
	{OB_LATTICE, "LATTICE", 0, "Lattice", ""},
	{OB_EMPTY, "EMPTY", 0, "Empty", ""},
	{0, "", 0, NULL, NULL},
	{OB_CAMERA, "CAMERA", 0, "Camera", ""},
	{OB_LAMP, "LAMP", 0, "Lamp", ""},
	{0, NULL, 0, NULL, NULL}
};



void add_object_draw(Scene *scene, View3D *v3d, int type)	/* for toolbox or menus, only non-editmode stuff */
{
	/* keep here to get things compile, remove later */
}

/* for object add primitive operators */
static Object *object_add_type(bContext *C, int type)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	
	/* for as long scene has editmode... */
	if (CTX_data_edit_object(C)) 
		ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); /* freedata, and undo */
	
	/* deselects all, sets scene->basact */
	ob= add_object(scene, type);
	/* editor level activate, notifiers */
	ED_base_object_activate(C, BASACT);

	/* more editor stuff */
	ED_object_base_init_from_view(C, BASACT);

	DAG_scene_sort(scene);

	return ob;
}

/* for object add operator */
static int object_add_exec(bContext *C, wmOperator *op)
{
	object_add_type(C, RNA_int_get(op->ptr, "type"));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_object_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Object";
	ot->description = "Add an object to the scene.";
	ot->idname= "OBJECT_OT_object_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_add_exec;
	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_object_types, 0, "Type", "");
}

/* ***************** add primitives *************** */
/* ******  work both in and outside editmode ****** */

static EnumPropertyItem prop_mesh_types[] = {
	{0, "PLANE", ICON_MESH_PLANE, "Plane", ""},
	{1, "CUBE", ICON_MESH_CUBE, "Cube", ""},
	{2, "CIRCLE", ICON_MESH_CIRCLE, "Circle", ""},
	{3, "UVSPHERE", ICON_MESH_UVSPHERE, "UVsphere", ""},
	{4, "ICOSPHERE", ICON_MESH_ICOSPHERE, "Icosphere", ""},
	{5, "CYLINDER", ICON_MESH_TUBE, "Cylinder", ""},
	{6, "CONE", ICON_MESH_CONE, "Cone", ""},
	{0, "", 0, NULL, NULL},
	{7, "GRID", ICON_MESH_GRID, "Grid", ""},
	{8, "MONKEY", ICON_MESH_MONKEY, "Monkey", ""},
	{0, NULL, 0, NULL, NULL}
};

static int object_add_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	int newob= 0;
	
	if(obedit==NULL || obedit->type!=OB_MESH) {
		object_add_type(C, OB_MESH);
		ED_object_enter_editmode(C, EM_DO_UNDO);
		newob = 1;
	}
	else DAG_object_flush_update(CTX_data_scene(C), obedit, OB_RECALC_DATA);

	switch(RNA_enum_get(op->ptr, "type")) {
		case 0:
			WM_operator_name_call(C, "MESH_OT_primitive_plane_add", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case 1:
			WM_operator_name_call(C, "MESH_OT_primitive_cube_add", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case 2:
			WM_operator_name_call(C, "MESH_OT_primitive_circle_add", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case 3:
			WM_operator_name_call(C, "MESH_OT_primitive_uv_sphere_add", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case 4:
			WM_operator_name_call(C, "MESH_OT_primitive_ico_sphere_add", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case 5:
			WM_operator_name_call(C, "MESH_OT_primitive_cylinder_add", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case 6:
			WM_operator_name_call(C, "MESH_OT_primitive_cone_add", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case 7:
			WM_operator_name_call(C, "MESH_OT_primitive_grid_add", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		case 8:
			WM_operator_name_call(C, "MESH_OT_primitive_monkey_add", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
	}
	/* userdef */
	if (newob && (U.flag & USER_ADD_EDITMODE)==0) {
		ED_object_exit_editmode(C, EM_FREEDATA);
	}
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}


void OBJECT_OT_mesh_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Mesh";
	ot->description = "Add a mesh object to the scene.";
	ot->idname= "OBJECT_OT_mesh_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_add_mesh_exec;
	
	ot->poll= ED_operator_scene_editable;
	
	/* flags: no register or undo, this operator calls operators */
	ot->flag= 0; //OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_mesh_types, 0, "Primitive", "");
}

static EnumPropertyItem prop_curve_types[] = {
	{CU_BEZIER|CU_2D|CU_PRIM_CURVE, "BEZIER_CURVE", ICON_CURVE_BEZCURVE, "Bezier Curve", ""},
	{CU_BEZIER|CU_2D|CU_PRIM_CIRCLE, "BEZIER_CIRCLE", ICON_CURVE_BEZCIRCLE, "Bezier Circle", ""},
	{CU_NURBS|CU_2D|CU_PRIM_CURVE, "NURBS_CURVE", ICON_CURVE_NCURVE, "NURBS Curve", ""},
	{CU_NURBS|CU_2D|CU_PRIM_CIRCLE, "NURBS_CIRCLE", ICON_CURVE_NCIRCLE, "NURBS Circle", ""},
	{CU_NURBS|CU_2D|CU_PRIM_PATH, "PATH", ICON_CURVE_PATH, "Path", ""},
	{0, NULL, 0, NULL, NULL}
};

static int object_add_curve_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	ListBase *editnurb;
	Nurb *nu;
	int newob= 0;
	
	if(obedit==NULL || obedit->type!=OB_CURVE) {
		object_add_type(C, OB_CURVE);
		ED_object_enter_editmode(C, 0);
		newob = 1;
	}
	else DAG_object_flush_update(CTX_data_scene(C), obedit, OB_RECALC_DATA);
	
	obedit= CTX_data_edit_object(C);
	nu= add_nurbs_primitive(C, RNA_enum_get(op->ptr, "type"), newob);
	editnurb= curve_get_editcurve(obedit);
	BLI_addtail(editnurb, nu);
	
	/* userdef */
	if (newob && (U.flag & USER_ADD_EDITMODE)==0) {
		ED_object_exit_editmode(C, EM_FREEDATA);
	}
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

static int object_add_curve_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit= CTX_data_edit_object(C);
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, op->type->name, 0);
	layout= uiPupMenuLayout(pup);
	if(!obedit || obedit->type == OB_CURVE)
		uiItemsEnumO(layout, op->type->idname, "type");
	else
		uiItemsEnumO(layout, "OBJECT_OT_surface_add", "type");
	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void OBJECT_OT_curve_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Curve";
	ot->description = "Add a curve object to the scene.";
	ot->idname= "OBJECT_OT_curve_add";
	
	/* api callbacks */
	ot->invoke= object_add_curve_invoke;
	ot->exec= object_add_curve_exec;
	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_curve_types, 0, "Primitive", "");
}

static EnumPropertyItem prop_surface_types[]= {
	{CU_PRIM_CURVE|CU_NURBS, "NURBS_CURVE", ICON_SURFACE_NCURVE, "NURBS Curve", ""},
	{CU_PRIM_CIRCLE|CU_NURBS, "NURBS_CIRCLE", ICON_SURFACE_NCIRCLE, "NURBS Circle", ""},
	{CU_PRIM_PATCH|CU_NURBS, "NURBS_SURFACE", ICON_SURFACE_NSURFACE, "NURBS Surface", ""},
	{CU_PRIM_TUBE|CU_NURBS, "NURBS_TUBE", ICON_SURFACE_NTUBE, "NURBS Tube", ""},
	{CU_PRIM_SPHERE|CU_NURBS, "NURBS_SPHERE", ICON_SURFACE_NSPHERE, "NURBS Sphere", ""},
	{CU_PRIM_DONUT|CU_NURBS, "NURBS_DONUT", ICON_SURFACE_NDONUT, "NURBS Donut", ""},
	{0, NULL, 0, NULL, NULL}
};

static int object_add_surface_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	ListBase *editnurb;
	Nurb *nu;
	int newob= 0;
	
	if(obedit==NULL || obedit->type!=OB_SURF) {
		object_add_type(C, OB_SURF);
		ED_object_enter_editmode(C, 0);
		newob = 1;
	}
	else DAG_object_flush_update(CTX_data_scene(C), obedit, OB_RECALC_DATA);
	
	obedit= CTX_data_edit_object(C);
	nu= add_nurbs_primitive(C, RNA_enum_get(op->ptr, "type"), newob);
	editnurb= curve_get_editcurve(obedit);
	BLI_addtail(editnurb, nu);
	
	/* userdef */
	if (newob && (U.flag & USER_ADD_EDITMODE)==0) {
		ED_object_exit_editmode(C, EM_FREEDATA);
	}
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_surface_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Surface";
	ot->description = "Add a surface object to the scene.";
	ot->idname= "OBJECT_OT_surface_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_add_surface_exec;
	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_surface_types, 0, "Primitive", "");
}

static EnumPropertyItem prop_metaball_types[]= {
	{MB_BALL, "MBALL_BALL", ICON_META_BALL, "Meta Ball", ""},
	{MB_TUBE, "MBALL_TUBE", ICON_META_TUBE, "Meta Tube", ""},
	{MB_PLANE, "MBALL_PLANE", ICON_META_PLANE, "Meta Plane", ""},
	{MB_CUBE, "MBALL_CUBE", ICON_META_CUBE, "Meta Cube", ""},
	{MB_ELIPSOID, "MBALL_ELLIPSOID", ICON_META_ELLIPSOID, "Meta Ellipsoid", ""},
	{0, NULL, 0, NULL, NULL}
};

static int object_metaball_add_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	MetaBall *mball;
	MetaElem *elem;
	int newob= 0;
	
	if(obedit==NULL || obedit->type!=OB_MBALL) {
		object_add_type(C, OB_MBALL);
		ED_object_enter_editmode(C, 0);
		newob = 1;
	}
	else DAG_object_flush_update(CTX_data_scene(C), obedit, OB_RECALC_DATA);
	
	obedit= CTX_data_edit_object(C);
	elem= (MetaElem*)add_metaball_primitive(C, RNA_enum_get(op->ptr, "type"), newob);
	mball= (MetaBall*)obedit->data;
	BLI_addtail(mball->editelems, elem);
	
	/* userdef */
	if (newob && (U.flag & USER_ADD_EDITMODE)==0) {
		ED_object_exit_editmode(C, EM_FREEDATA);
	}
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

static int object_metaball_add_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit= CTX_data_edit_object(C);
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, op->type->name, 0);
	layout= uiPupMenuLayout(pup);
	if(!obedit || obedit->type == OB_MBALL)
		uiItemsEnumO(layout, op->type->idname, "type");
	else
		uiItemsEnumO(layout, "OBJECT_OT_metaball_add", "type");
	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void OBJECT_OT_metaball_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Metaball";
	ot->description= "Add an metaball object to the scene.";
	ot->idname= "OBJECT_OT_metaball_add";

	/* api callbacks */
	ot->invoke= object_metaball_add_invoke;
	ot->exec= object_metaball_add_exec;
	ot->poll= ED_operator_scene_editable;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_metaball_types, 0, "Primitive", "");
}
static int object_add_text_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	
	if(obedit && obedit->type==OB_FONT)
		return OPERATOR_CANCELLED;

	object_add_type(C, OB_FONT);
	obedit= CTX_data_active_object(C);

	if(U.flag & USER_ADD_EDITMODE)
		ED_object_enter_editmode(C, 0);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_text_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Text";
	ot->description = "Add a text object to the scene";
	ot->idname= "OBJECT_OT_text_add";
	
	/* api callbacks */
	ot->exec= object_add_text_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_armature_add_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	View3D *v3d= CTX_wm_view3d(C);
	RegionView3D *rv3d= NULL;
	int newob= 0;
	
	if ((obedit==NULL) || (obedit->type != OB_ARMATURE)) {
		object_add_type(C, OB_ARMATURE);
		ED_object_enter_editmode(C, 0);
		newob = 1;
	}
	else DAG_object_flush_update(CTX_data_scene(C), obedit, OB_RECALC_DATA);
	
	if(v3d) 
		rv3d= CTX_wm_region(C)->regiondata;
	
	/* v3d and rv3d are allowed to be NULL */
	add_primitive_bone(CTX_data_scene(C), v3d, rv3d);

	/* userdef */
	if (newob && (U.flag & USER_ADD_EDITMODE)==0) {
		ED_object_exit_editmode(C, EM_FREEDATA);
	}
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_armature_add(wmOperatorType *ot)
{	
	/* identifiers */
	ot->name= "Add Armature";
	ot->description = "Add an armature object to the scene.";
	ot->idname= "OBJECT_OT_armature_add";
	
	/* api callbacks */
	ot->exec= object_armature_add_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_primitive_add_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup= uiPupMenuBegin(C, "Add Object", 0);
	uiLayout *layout= uiPupMenuLayout(pup);
	
	uiItemMenuEnumO(layout, "Mesh", ICON_OUTLINER_OB_MESH, "OBJECT_OT_mesh_add", "type");
	uiItemMenuEnumO(layout, "Curve", ICON_OUTLINER_OB_CURVE, "OBJECT_OT_curve_add", "type");
	uiItemMenuEnumO(layout, "Surface", ICON_OUTLINER_OB_SURFACE, "OBJECT_OT_surface_add", "type");
	uiItemMenuEnumO(layout, NULL, ICON_OUTLINER_OB_META, "OBJECT_OT_metaball_add", "type");
	uiItemO(layout, "Text", ICON_OUTLINER_OB_FONT, "OBJECT_OT_text_add");
	uiItemS(layout);
	uiItemO(layout, "Armature", ICON_OUTLINER_OB_ARMATURE, "OBJECT_OT_armature_add");
	uiItemEnumO(layout, NULL, ICON_OUTLINER_OB_LATTICE, "OBJECT_OT_object_add", "type", OB_LATTICE);
	uiItemEnumO(layout, NULL, ICON_OUTLINER_OB_EMPTY, "OBJECT_OT_object_add", "type", OB_EMPTY);
	uiItemS(layout);
	uiItemEnumO(layout, NULL, ICON_OUTLINER_OB_CAMERA, "OBJECT_OT_object_add", "type", OB_CAMERA);
	uiItemEnumO(layout, NULL, ICON_OUTLINER_OB_LAMP, "OBJECT_OT_object_add", "type", OB_LAMP);
	
	uiPupMenuEnd(C, pup);
	
	/* this operator is only for a menu, not used further */
	return OPERATOR_CANCELLED;
}

/* only used as menu */
void OBJECT_OT_primitive_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Primitive";
	ot->description = "Add a primitive object.";
	ot->idname= "OBJECT_OT_primitive_add";
	
	/* api callbacks */
	ot->invoke= object_primitive_add_invoke;
	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= 0;
}


/* ******************************* */

/* remove base from a specific scene */
/* note: now unlinks constraints as well */
void ED_base_object_free_and_unlink(Scene *scene, Base *base)
{
	BLI_remlink(&scene->base, base);
	free_libblock_us(&G.main->object, base->object);
	if(scene->basact==base) scene->basact= NULL;
	MEM_freeN(base);
}

static int object_delete_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	int islamp= 0;
	
	if(CTX_data_edit_object(C)) 
		return OPERATOR_CANCELLED;
	
	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {

		if(base->object->type==OB_LAMP) islamp= 1;
		
		/* remove from current scene only */
		ED_base_object_free_and_unlink(scene, base);
	}
	CTX_DATA_END;

	if(islamp) reshadeall_displist(scene);	/* only frees displist */
	
	DAG_scene_sort(scene);
	ED_anim_dag_flush_update(C);
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_ACTIVE, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_delete(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Delete";
	ot->description = "Delete selected objects.";
	ot->idname= "OBJECT_OT_delete";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_delete_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}


static void single_object_users__forwardModifierLinks(void *userData, Object *ob, Object **obpoin)
{
	ID_NEW(*obpoin);
}

static void copy_object__forwardModifierLinks(void *userData, Object *ob,
                                              ID **idpoin)
{
	/* this is copied from ID_NEW; it might be better to have a macro */
	if(*idpoin && (*idpoin)->newid) *idpoin = (*idpoin)->newid;
}


/* after copying objects, copied data should get new pointers */
static void copy_object_set_idnew(Scene *scene, View3D *v3d, int dupflag)
{
	Base *base;
	Object *ob;
	Material *ma, *mao;
	ID *id;
#if 0 // XXX old animation system
	Ipo *ipo;
	bActionStrip *strip;
#endif // XXX old animation system
	int a;
	
	/* XXX check object pointers */
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB_BGMODE(v3d, base)) {
			ob= base->object;
			relink_constraints(&ob->constraints);
			if (ob->pose){
				bPoseChannel *chan;
				for (chan = ob->pose->chanbase.first; chan; chan=chan->next){
					relink_constraints(&chan->constraints);
				}
			}
			modifiers_foreachIDLink(ob, copy_object__forwardModifierLinks, NULL);
			ID_NEW(ob->parent);
			ID_NEW(ob->track);
			ID_NEW(ob->proxy);
			ID_NEW(ob->proxy_group);
			
#if 0 // XXX old animation system
			for(strip= ob->nlastrips.first; strip; strip= strip->next) {
				bActionModifier *amod;
				for(amod= strip->modifiers.first; amod; amod= amod->next)
					ID_NEW(amod->ob);
			}
#endif // XXX old animation system
		}
	}
	
	/* materials */
	if( dupflag & USER_DUP_MAT) {
		mao= G.main->mat.first;
		while(mao) {
			if(mao->id.newid) {
				
				ma= (Material *)mao->id.newid;
				
				if(dupflag & USER_DUP_TEX) {
					for(a=0; a<MAX_MTEX; a++) {
						if(ma->mtex[a]) {
							id= (ID *)ma->mtex[a]->tex;
							if(id) {
								ID_NEW_US(ma->mtex[a]->tex)
								else ma->mtex[a]->tex= copy_texture(ma->mtex[a]->tex);
								id->us--;
							}
						}
					}
				}
#if 0 // XXX old animation system
				id= (ID *)ma->ipo;
				if(id) {
					ID_NEW_US(ma->ipo)
					else ma->ipo= copy_ipo(ma->ipo);
					id->us--;
				}
#endif // XXX old animation system
			}
			mao= mao->id.next;
		}
	}
	
#if 0 // XXX old animation system
	/* lamps */
	if( dupflag & USER_DUP_IPO) {
		Lamp *la= G.main->lamp.first;
		while(la) {
			if(la->id.newid) {
				Lamp *lan= (Lamp *)la->id.newid;
				id= (ID *)lan->ipo;
				if(id) {
					ID_NEW_US(lan->ipo)
					else lan->ipo= copy_ipo(lan->ipo);
					id->us--;
				}
			}
			la= la->id.next;
		}
	}
	
	/* ipos */
	ipo= G.main->ipo.first;
	while(ipo) {
		if(ipo->id.lib==NULL && ipo->id.newid) {
			Ipo *ipon= (Ipo *)ipo->id.newid;
			IpoCurve *icu;
			for(icu= ipon->curve.first; icu; icu= icu->next) {
				if(icu->driver) {
					ID_NEW(icu->driver->ob);
				}
			}
		}
		ipo= ipo->id.next;
	}
#endif // XXX old animation system
	
	set_sca_new_poins();
	
	clear_id_newpoins();
	
}

static int return_editmesh_indexar(EditMesh *em, int *tot, int **indexar, float *cent)
{
	EditVert *eve;
	int *index, nr, totvert=0;
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & SELECT) totvert++;
	}
	if(totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	cent[0]= cent[1]= cent[2]= 0.0;
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & SELECT) {
			*index= nr; index++;
			VecAddf(cent, cent, eve->co);
		}
		nr++;
	}
	
	VecMulf(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static int return_editmesh_vgroup(Object *obedit, EditMesh *em, char *name, float *cent)
{
	MDeformVert *dvert;
	EditVert *eve;
	int i, totvert=0;
	
	cent[0]= cent[1]= cent[2]= 0.0;
	
	if(obedit->actdef) {
		
		/* find the vertices */
		for(eve= em->verts.first; eve; eve= eve->next) {
			dvert= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);

			if(dvert) {
				for(i=0; i<dvert->totweight; i++){
					if(dvert->dw[i].def_nr == (obedit->actdef-1)) {
						totvert++;
						VecAddf(cent, cent, eve->co);
					}
				}
			}
		}
		if(totvert) {
			bDeformGroup *defGroup = BLI_findlink(&obedit->defbase, obedit->actdef-1);
			strcpy(name, defGroup->name);
			VecMulf(cent, 1.0f/(float)totvert);
			return 1;
		}
	}
	
	return 0;
}	

static void select_editmesh_hook(Object *ob, HookModifierData *hmd)
{
	Mesh *me= ob->data;
	EditMesh *em= BKE_mesh_get_editmesh(me);
	EditVert *eve;
	int index=0, nr=0;
	
	for(eve= em->verts.first; eve; eve= eve->next, nr++) {
		if(nr==hmd->indexar[index]) {
			eve->f |= SELECT;
			if(index < hmd->totindex-1) index++;
		}
	}
	EM_select_flush(em);

	BKE_mesh_end_editmesh(me, em);
}

static int return_editlattice_indexar(Lattice *editlatt, int *tot, int **indexar, float *cent)
{
	BPoint *bp;
	int *index, nr, totvert=0, a;
	
	/* count */
	a= editlatt->pntsu*editlatt->pntsv*editlatt->pntsw;
	bp= editlatt->def;
	while(a--) {
		if(bp->f1 & SELECT) {
			if(bp->hide==0) totvert++;
		}
		bp++;
	}

	if(totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	cent[0]= cent[1]= cent[2]= 0.0;
	
	a= editlatt->pntsu*editlatt->pntsv*editlatt->pntsw;
	bp= editlatt->def;
	while(a--) {
		if(bp->f1 & SELECT) {
			if(bp->hide==0) {
				*index= nr; index++;
				VecAddf(cent, cent, bp->vec);
			}
		}
		bp++;
		nr++;
	}
	
	VecMulf(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static void select_editlattice_hook(Object *obedit, HookModifierData *hmd)
{
	Lattice *lt= obedit->data;
	BPoint *bp;
	int index=0, nr=0, a;
	
	/* count */
	a= lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
	bp= lt->editlatt->def;
	while(a--) {
		if(hmd->indexar[index]==nr) {
			bp->f1 |= SELECT;
			if(index < hmd->totindex-1) index++;
		}
		nr++;
		bp++;
	}
}

static int return_editcurve_indexar(Object *obedit, int *tot, int **indexar, float *cent)
{
	ListBase *editnurb= curve_get_editcurve(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int *index, a, nr, totvert=0;
	
	for(nu= editnurb->first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(bezt->f1 & SELECT) totvert++;
				if(bezt->f2 & SELECT) totvert++;
				if(bezt->f3 & SELECT) totvert++;
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(bp->f1 & SELECT) totvert++;
				bp++;
			}
		}
	}
	if(totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	cent[0]= cent[1]= cent[2]= 0.0;
	
	for(nu= editnurb->first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(bezt->f1 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bezt->vec[0]);
				}
				nr++;
				if(bezt->f2 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bezt->vec[1]);
				}
				nr++;
				if(bezt->f3 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bezt->vec[2]);
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
					*index= nr; index++;
					VecAddf(cent, cent, bp->vec);
				}
				nr++;
				bp++;
			}
		}
	}
	
	VecMulf(cent, 1.0f/(float)totvert);
	
	return totvert;
}

void ED_object_apply_obmat(Object *ob)
{
	float mat[3][3], imat[3][3], tmat[3][3];
	
	/* from obmat to loc rot size */
	
	if(ob==NULL) return;
	Mat3CpyMat4(mat, ob->obmat);
	
	VECCOPY(ob->loc, ob->obmat[3]);

	Mat3ToEul(mat, ob->rot);
	EulToMat3(ob->rot, tmat);

	Mat3Inv(imat, tmat);
	
	Mat3MulMat3(tmat, imat, mat);
	
	ob->size[0]= tmat[0][0];
	ob->size[1]= tmat[1][1];
	ob->size[2]= tmat[2][2];
	
}

int hook_getIndexArray(Object *obedit, int *tot, int **indexar, char *name, float *cent_r)
{
	*indexar= NULL;
	*tot= 0;
	name[0]= 0;
	
	switch(obedit->type) {
		case OB_MESH:
		{
			Mesh *me= obedit->data;
			EditMesh *em = BKE_mesh_get_editmesh(me);

			/* check selected vertices first */
			if( return_editmesh_indexar(em, tot, indexar, cent_r)) {
				BKE_mesh_end_editmesh(me, em);
				return 1;
			} else {
				int ret = return_editmesh_vgroup(obedit, em, name, cent_r);
				BKE_mesh_end_editmesh(me, em);
				return ret;
			}
		}
		case OB_CURVE:
		case OB_SURF:
			return return_editcurve_indexar(obedit, tot, indexar, cent_r);
		case OB_LATTICE:
		{
			Lattice *lt= obedit->data;
			return return_editlattice_indexar(lt->editlatt, tot, indexar, cent_r);
		}
		default:
			return 0;
	}
}

static void select_editcurve_hook(Object *obedit, HookModifierData *hmd)
{
	ListBase *editnurb= curve_get_editcurve(obedit);
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int index=0, a, nr=0;
	
	for(nu= editnurb->first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(nr == hmd->indexar[index]) {
					bezt->f1 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				if(nr == hmd->indexar[index]) {
					bezt->f2 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				if(nr == hmd->indexar[index]) {
					bezt->f3 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(nr == hmd->indexar[index]) {
					bp->f1 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				bp++;
			}
		}
	}
}

void obedit_hook_select(Object *ob, HookModifierData *hmd) 
{
	
	if(ob->type==OB_MESH) select_editmesh_hook(ob, hmd);
	else if(ob->type==OB_LATTICE) select_editlattice_hook(ob, hmd);
	else if(ob->type==OB_CURVE) select_editcurve_hook(ob, hmd);
	else if(ob->type==OB_SURF) select_editcurve_hook(ob, hmd);
}


void add_hook(Scene *scene, View3D *v3d, int mode)
{
	ModifierData *md = NULL;
	HookModifierData *hmd = NULL;
	Object *ob=NULL;
	Object *obedit= scene->obedit;  // XXX get from context
	
	if(obedit==NULL) return;
	
	/* preconditions */
	if(mode==2) { /* selected object */
		Base *base;
		for(base= FIRSTBASE; base; base= base->next) {
			if(TESTBASELIB(v3d, base)) {
				if(base!=BASACT) {
					ob= base->object;
					break;
				}
			}
		}
		if(ob==NULL) {
			error("Requires selected Object");
			return;
		}
	}
	else if(mode!=1) {
		int maxlen=0, a, nr;
		char *cp;
		
		/* make pupmenu with hooks */
		for(md=obedit->modifiers.first; md; md= md->next) {
			if (md->type==eModifierType_Hook) 
				maxlen+=32;
		}
		
		if(maxlen==0) {
			error("Object has no hooks yet");
			return;
		}
		
		cp= MEM_callocN(maxlen+32, "temp string");
		if(mode==3) strcpy(cp, "Remove %t|");
		else if(mode==4) strcpy(cp, "Reassign %t|");
		else if(mode==5) strcpy(cp, "Select %t|");
		else if(mode==6) strcpy(cp, "Clear Offset %t|");
		
		for(md=obedit->modifiers.first; md; md= md->next) {
			if (md->type==eModifierType_Hook) {
				strcat(cp, md->name);
				strcat(cp, " |");
			}
		}
		
		nr= pupmenu(cp);
		MEM_freeN(cp);
		
		if(nr<1) return;
		
		a= 1;
		for(md=obedit->modifiers.first; md; md=md->next) {
			if (md->type==eModifierType_Hook) {
				if(a==nr) break;
				a++;
			}
		}
		
		hmd = (HookModifierData*) md;
		ob= hmd->object;
	}

	/* do it, new hooks or reassign */
	if(mode==1 || mode==2 || mode==4) {
		float cent[3];
		int tot, ok, *indexar;
		char name[32];
		
		ok = hook_getIndexArray(obedit, &tot, &indexar, name, cent);
		
		if(ok==0) {
			error("Requires selected vertices or active Vertex Group");
		}
		else {
			
			if(mode==1) {
				Base *base= BASACT, *newbase;
				
				ob= add_object(scene, OB_EMPTY);
				/* set layers OK */
				newbase= BASACT;
				newbase->lay= base->lay;
				ob->lay= newbase->lay;
				
				/* transform cent to global coords for loc */
				VecMat4MulVecfl(ob->loc, obedit->obmat, cent);
				
				/* restore, add_object sets active */
				BASACT= base;
			}
			/* if mode is 2 or 4, ob has been set */
			
			/* new hook */
			if(mode==1 || mode==2) {
				ModifierData *md = obedit->modifiers.first;
				
				while (md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform) {
					md = md->next;
				}
				
				hmd = (HookModifierData*) modifier_new(eModifierType_Hook);
				BLI_insertlinkbefore(&obedit->modifiers, md, hmd);
				sprintf(hmd->modifier.name, "Hook-%s", ob->id.name+2);
			}
			else if (hmd->indexar) MEM_freeN(hmd->indexar); /* reassign, hook was set */
		
			hmd->object= ob;
			hmd->indexar= indexar;
			VECCOPY(hmd->cent, cent);
			hmd->totindex= tot;
			BLI_strncpy(hmd->name, name, 32);
			
			if(mode==1 || mode==2) {
				/* matrix calculus */
				/* vert x (obmat x hook->imat) x hook->obmat x ob->imat */
				/*        (parentinv         )                          */
				
				where_is_object(scene, ob);
				
				Mat4Invert(ob->imat, ob->obmat);
				/* apparently this call goes from right to left... */
				Mat4MulSerie(hmd->parentinv, ob->imat, obedit->obmat, NULL, 
							 NULL, NULL, NULL, NULL, NULL);
			}
		}
	}
	else if(mode==3) { /* remove */
		BLI_remlink(&obedit->modifiers, md);
		modifier_free(md);
	}
	else if(mode==5) { /* select */
		obedit_hook_select(obedit, hmd);
	}
	else if(mode==6) { /* clear offset */
		where_is_object(scene, ob);	/* ob is hook->parent */

		Mat4Invert(ob->imat, ob->obmat);
		/* this call goes from right to left... */
		Mat4MulSerie(hmd->parentinv, ob->imat, obedit->obmat, NULL, 
					 NULL, NULL, NULL, NULL, NULL);
	}

	DAG_scene_sort(scene);
}


/* use this when the loc/size/rot of the parent has changed but the children should stay in the same place
 * apply-size-rot or object center for eg */
static void ignore_parent_tx(Scene *scene, Object *ob ) 
{
	Object workob;
	Object *ob_child;
	
	/* a change was made, adjust the children to compensate */
	for (ob_child=G.main->object.first; ob_child; ob_child=ob_child->id.next) {
		if (ob_child->parent == ob) {
			ED_object_apply_obmat(ob_child);
			what_does_parent(scene, ob_child, &workob);
			Mat4Invert(ob_child->parentinv, workob.obmat);
		}
	}
}


void add_hook_menu(Scene *scene, View3D *v3d)
{
	Object *obedit= scene->obedit;  // XXX get from context
	int mode;
	
	if(obedit==NULL) return;
	
	if(modifiers_findByType(obedit, eModifierType_Hook))
		mode= pupmenu("Hooks %t|Add, To New Empty %x1|Add, To Selected Object %x2|Remove... %x3|Reassign... %x4|Select... %x5|Clear Offset...%x6");
	else
		mode= pupmenu("Hooks %t|Add, New Empty %x1|Add, To Selected Object %x2");

	if(mode<1) return;
		
	/* do operations */
	add_hook(scene, v3d, mode);
}

/* ******************** clear parent operator ******************* */

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

/* ******************** clear track operator ******************* */


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

/* *****************Selection Operators******************* */
static EnumPropertyItem prop_select_types[] = {
	{0, "EXCLUSIVE", 0, "Exclusive", ""},
	{1, "EXTEND", 0, "Extend", ""},
	{0, NULL, 0, NULL, NULL}
};

/* ****** Select by Type ****** */

static int object_select_by_type_exec(bContext *C, wmOperator *op)
{
	short obtype, seltype;
	
	obtype = RNA_enum_get(op->ptr, "type");
	seltype = RNA_enum_get(op->ptr, "seltype");
		
	if (seltype == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
	
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if(base->object->type==obtype) {
			ED_base_object_select(base, BA_SELECT);
		}
	}
	CTX_DATA_END;
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_by_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select By Type";
	ot->description = "Select all visible objects that are of a type.";
	ot->idname= "OBJECT_OT_select_by_type";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_select_by_type_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "seltype", prop_select_types, 0, "Selection", "Extend selection or clear selection then select");
	RNA_def_enum(ot->srna, "type", prop_object_types, 1, "Type", "");

}
/* ****** selection by links *******/

static EnumPropertyItem prop_select_linked_types[] = {
	{1, "IPO", 0, "Object IPO", ""}, // XXX depreceated animation system stuff...
	{2, "OBDATA", 0, "Ob Data", ""},
	{3, "MATERIAL", 0, "Material", ""},
	{4, "TEXTURE", 0, "Texture", ""},
	{5, "DUPGROUP", 0, "Dupligroup", ""},
	{6, "PARTICLE", 0, "Particle System", ""},
	{0, NULL, 0, NULL, NULL}
};

static int object_select_linked_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	void *obdata = NULL;
	Material *mat = NULL, *mat1;
	Tex *tex=0;
	int a, b;
	int nr = RNA_enum_get(op->ptr, "type");
	short changed = 0, seltype;
	/* events (nr):
	 * Object Ipo: 1
	 * ObData: 2
	 * Current Material: 3
	 * Current Texture: 4
	 * DupliGroup: 5
	 * PSys: 6
	 */

	seltype = RNA_enum_get(op->ptr, "seltype");
	
	if (seltype == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
	
	ob= OBACT;
	if(ob==0){ 
		BKE_report(op->reports, RPT_ERROR, "No Active Object");
		return OPERATOR_CANCELLED;
	}
	
	if(nr==1) {	
			// XXX old animation system
		//ipo= ob->ipo;
		//if(ipo==0) return OPERATOR_CANCELLED;
		return OPERATOR_CANCELLED;
	}
	else if(nr==2) {
		if(ob->data==0) return OPERATOR_CANCELLED;
		obdata= ob->data;
	}
	else if(nr==3 || nr==4) {
		mat= give_current_material(ob, ob->actcol);
		if(mat==0) return OPERATOR_CANCELLED;
		if(nr==4) {
			if(mat->mtex[ (int)mat->texact ]) tex= mat->mtex[ (int)mat->texact ]->tex;
			if(tex==0) return OPERATOR_CANCELLED;
		}
	}
	else if(nr==5) {
		if(ob->dup_group==NULL) return OPERATOR_CANCELLED;
	}
	else if(nr==6) {
		if(ob->particlesystem.first==NULL) return OPERATOR_CANCELLED;
	}
	else return OPERATOR_CANCELLED;
	
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if(nr==1) {
				// XXX old animation system
			//if(base->object->ipo==ipo) base->flag |= SELECT;
			//changed = 1;
		}
		else if(nr==2) {
			if(base->object->data==obdata) base->flag |= SELECT;
			changed = 1;
		}
		else if(nr==3 || nr==4) {
			ob= base->object;
			
			for(a=1; a<=ob->totcol; a++) {
				mat1= give_current_material(ob, a);
				if(nr==3) {
					if(mat1==mat) base->flag |= SELECT;
					changed = 1;
				}
				else if(mat1 && nr==4) {
					for(b=0; b<MAX_MTEX; b++) {
						if(mat1->mtex[b]) {
							if(tex==mat1->mtex[b]->tex) {
								base->flag |= SELECT;
								changed = 1;
								break;
							}
						}
					}
				}
			}
		}
		else if(nr==5) {
			if(base->object->dup_group==ob->dup_group) {
				 base->flag |= SELECT;
				 changed = 1;
			}
		}
		else if(nr==6) {
			/* loop through other, then actives particles*/
			ParticleSystem *psys;
			ParticleSystem *psys_act;
			
			for(psys=base->object->particlesystem.first; psys; psys=psys->next) {
				for(psys_act=ob->particlesystem.first; psys_act; psys_act=psys_act->next) {
					if (psys->part == psys_act->part) {
						base->flag |= SELECT;
						changed = 1;
						break;
					}
				}
				
				if (base->flag & SELECT) {
					break;
				}
			}
		}
		base->object->flag= base->flag;
	}
	CTX_DATA_END;
	
	if (changed) {
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED;
}

void OBJECT_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked";
	ot->description = "Select all visible objects that are linked.";
	ot->idname= "OBJECT_OT_select_linked";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_select_linked_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_select_linked_types, 0, "Type", "");
	RNA_def_enum(ot->srna, "seltype", prop_select_types, 1, "Selection", "Extend selection or clear selection then select");

}

/* ****** selection grouped *******/

static EnumPropertyItem prop_select_grouped_types[] = {
	{1, "CHILDREN_RECURSIVE", 0, "Children", ""}, // XXX depreceated animation system stuff...
	{2, "CHILDREN", 0, "Immediate Children", ""},
	{3, "PARENT", 0, "Parent", ""},
	{4, "SIBLINGS", 0, "Siblings", "Shared Parent"},
	{5, "TYPE", 0, "Type", "Shared object type"},
	{6, "LAYER", 0, "Layer", "Shared layers"},
	{7, "GROUP", 0, "Group", "Shared group"},
	{8, "HOOK", 0, "Hook", ""},
	{9, "PASS", 0, "Pass", "Render pass Index"},
	{10, "COLOR", 0, "Color", "Object Color"},
	{11, "PROPERTIES", 0, "Properties", "Game Properties"},
	{0, NULL, 0, NULL, NULL}
};


static short select_grouped_children(bContext *C, Object *ob, int recursive)
{
	short changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if (ob == base->object->parent) {
			if (!(base->flag & SELECT)) {
				ED_base_object_select(base, BA_SELECT);
				changed = 1;
			}

			if (recursive)
				changed |= select_grouped_children(C, base->object, 1);
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_parent(bContext *C)	/* Makes parent active and de-selected OBACT */
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);

	short changed = 0;
	Base *baspar, *basact= CTX_data_active_base(C);

	if (!basact || !(basact->object->parent)) return 0; /* we know OBACT is valid */

	baspar= object_in_scene(basact->object->parent, scene);

	/* can be NULL if parent in other scene */
	if(baspar && BASE_SELECTABLE(v3d, baspar)) {
		ED_base_object_select(basact, BA_DESELECT);
		ED_base_object_select(baspar, BA_SELECT);
		ED_base_object_activate(C, baspar);
		changed = 1;
	}
	return changed;
}


#define GROUP_MENU_MAX	24
static short select_grouped_group(bContext *C, Object *ob)	/* Select objects in the same group as the active */
{
	short changed = 0;
	Group *group, *ob_groups[GROUP_MENU_MAX];
	//char str[10 + (24*GROUP_MENU_MAX)];
	//char *p = str;
	int group_count=0; //, menu, i;

	for (	group=G.main->group.first;
			group && group_count < GROUP_MENU_MAX;
			group=group->id.next
		) {
		if (object_in_group (ob, group)) {
			ob_groups[group_count] = group;
			group_count++;
		}
	}

	if (!group_count)
		return 0;

	else if (group_count == 1) {
		group = ob_groups[0];
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			if (!(base->flag & SELECT) && object_in_group(base->object, group)) {
				ED_base_object_select(base, BA_SELECT);
				changed = 1;
			}
		}
		CTX_DATA_END;
		return changed;
	}
#if 0 // XXX hows this work in 2.5?
	/* build the menu. */
	p += sprintf(str, "Groups%%t");
	for (i=0; i<group_count; i++) {
		group = ob_groups[i];
		p += sprintf (p, "|%s%%x%i", group->id.name+2, i);
	}

	menu = pupmenu (str);
	if (menu == -1)
		return 0;

	group = ob_groups[menu];
	for (base= FIRSTBASE; base; base= base->next) {
		if (!(base->flag & SELECT) && object_in_group(base->object, group)) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
#endif
	return changed;
}

static short select_grouped_object_hooks(bContext *C, Object *ob)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);

	short changed = 0;
	Base *base;
	ModifierData *md;
	HookModifierData *hmd;

	for (md = ob->modifiers.first; md; md=md->next) {
		if (md->type==eModifierType_Hook) {
			hmd= (HookModifierData*) md;
			if (hmd->object && !(hmd->object->flag & SELECT)) {
				base= object_in_scene(hmd->object, scene);
				if (base && (BASE_SELECTABLE(v3d, base))) {
					ED_base_object_select(base, BA_SELECT);
					changed = 1;
				}
			}
		}
	}
	return changed;
}

/* Select objects woth the same parent as the active (siblings),
 * parent can be NULL also */
static short select_grouped_siblings(bContext *C, Object *ob)
{
	short changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if ((base->object->parent==ob->parent)  && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_type(bContext *C, Object *ob)
{
	short changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if ((base->object->type == ob->type) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_layer(bContext *C, Object *ob)
{
	char changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if ((base->lay & ob->lay) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_index_object(bContext *C, Object *ob)
{
	char changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if ((base->object->index == ob->index) && !(base->flag & SELECT)) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short select_grouped_color(bContext *C, Object *ob)
{
	char changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if (!(base->flag & SELECT) && (FloatCompare(base->object->col, ob->col, 0.005f))) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static short objects_share_gameprop(Object *a, Object *b)
{
	bProperty *prop;
	/*make a copy of all its properties*/

	for( prop= a->prop.first; prop; prop = prop->next ) {
		if ( get_ob_property(b, prop->name) )
			return 1;
	}
	return 0;
}

static short select_grouped_gameprops(bContext *C, Object *ob)
{
	char changed = 0;

	CTX_DATA_BEGIN(C, Base*, base, selectable_bases) {
		if (!(base->flag & SELECT) && (objects_share_gameprop(base->object, ob))) {
			ED_base_object_select(base, BA_SELECT);
			changed = 1;
		}
	}
	CTX_DATA_END;
	return changed;
}

static int object_select_grouped_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	int nr = RNA_enum_get(op->ptr, "type");
	short changed = 0, seltype;

	seltype = RNA_enum_get(op->ptr, "seltype");
	
	if (seltype == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
	
	ob= OBACT;
	if(ob==0){ 
		BKE_report(op->reports, RPT_ERROR, "No Active Object");
		return OPERATOR_CANCELLED;
	}
	
	if(nr==1)		changed = select_grouped_children(C, ob, 1);
	else if(nr==2)	changed = select_grouped_children(C, ob, 0);
	else if(nr==3)	changed = select_grouped_parent(C);
	else if(nr==4)	changed = select_grouped_siblings(C, ob);
	else if(nr==5)	changed = select_grouped_type(C, ob);
	else if(nr==6)	changed = select_grouped_layer(C, ob);
	else if(nr==7)	changed = select_grouped_group(C, ob);
	else if(nr==8)	changed = select_grouped_object_hooks(C, ob);
	else if(nr==9)	changed = select_grouped_index_object(C, ob);
	else if(nr==10)	changed = select_grouped_color(C, ob);
	else if(nr==11)	changed = select_grouped_gameprops(C, ob);
	
	if (changed) {
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED;
}

void OBJECT_OT_select_grouped(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Grouped";
	ot->description = "Select all visible objects grouped by various properties.";
	ot->idname= "OBJECT_OT_select_grouped";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_select_grouped_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_select_grouped_types, 0, "Type", "");
	RNA_def_enum(ot->srna, "seltype", prop_select_types, 1, "Selection", "Extend selection or clear selection then select");

}

/* ****** selection by layer *******/

static int object_select_by_layer_exec(bContext *C, wmOperator *op)
{
	unsigned int layernum;
	short seltype;
	
	seltype = RNA_enum_get(op->ptr, "seltype");
	layernum = RNA_int_get(op->ptr, "layer");
	
	if (seltype == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
		
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if(base->lay == (1<< (layernum -1)))
			ED_base_object_select(base, BA_SELECT);
	}
	CTX_DATA_END;
	
	/* undo? */
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_by_layer(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "select by layer";
	ot->description = "Select all visible objects on a layer.";
	ot->idname= "OBJECT_OT_select_by_layer";
	
	/* api callbacks */
	/*ot->invoke = XXX - need a int grid popup*/
	ot->exec= object_select_by_layer_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_int(ot->srna, "layer", 1, 1, 20, "Layer", "", 1, 20);
	RNA_def_enum(ot->srna, "seltype", prop_select_types, 1, "Selection", "Extend selection or clear selection then select");
}

/* ****** invert selection *******/
static int object_select_inverse_exec(bContext *C, wmOperator *op)
{
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if (base->flag & SELECT)
			ED_base_object_select(base, BA_DESELECT);
		else
			ED_base_object_select(base, BA_SELECT);
	}
	CTX_DATA_END;
	
	/* undo? */
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_inverse(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Select Inverse";
	ot->description = "Invert selection of all visible objects.";
	ot->idname= "OBJECT_OT_select_inverse";
	
	/* api callbacks */
	ot->exec= object_select_inverse_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}
/* ****** (de)select All *******/

static int object_select_de_select_all_exec(bContext *C, wmOperator *op)
{
	
	int a=0, ok=0; 
	
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if (base->flag & SELECT) {
			ok= a= 1;
			break;
		}
		else ok=1;
	}
	CTX_DATA_END;
	
	if (!ok) return OPERATOR_PASS_THROUGH;
	
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if (a) ED_base_object_select(base, BA_DESELECT);
		else ED_base_object_select(base, BA_SELECT);
	}
	CTX_DATA_END;
	
	/* undo? */
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_all_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "deselect all";
	ot->description = "(de)select all visible objects in scene.";
	ot->idname= "OBJECT_OT_select_all_toggle";
	
	/* api callbacks */
	ot->exec= object_select_de_select_all_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}
/* ****** random selection *******/

static int object_select_random_exec(bContext *C, wmOperator *op)
{	
	float percent;
	short seltype;
	
	seltype = RNA_enum_get(op->ptr, "seltype");
	
	if (seltype == 0) {
		CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
			ED_base_object_select(base, BA_DESELECT);
		}
		CTX_DATA_END;
	}
	percent = RNA_float_get(op->ptr, "percent");
		
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if (BLI_frand() < percent) {
				ED_base_object_select(base, BA_SELECT);
		}
	}
	CTX_DATA_END;
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Random select";
	ot->description = "Set select on random visible objects.";
	ot->idname= "OBJECT_OT_select_random";
	
	/* api callbacks */
	/*ot->invoke= object_select_random_invoke XXX - need a number popup ;*/
	ot->exec = object_select_random_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_float_percentage(ot->srna, "percent", 0.5f, 0.0f, 1.0f, "Percent", "percentage of objects to randomly select", 0.0001f, 1.0f);
	RNA_def_enum(ot->srna, "seltype", prop_select_types, 1, "Selection", "Extend selection or clear selection then select");
}

/* ******** Clear object Translation *********** */

static int object_location_clear_exec(bContext *C, wmOperator *op)
{
	int armature_clear= 0;

	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(!(ob->mode & OB_MODE_WEIGHT_PAINT)) {
			if ((ob->protectflag & OB_LOCK_LOCX)==0)
				ob->loc[0]= ob->dloc[0]= 0.0f;
			if ((ob->protectflag & OB_LOCK_LOCY)==0)
				ob->loc[1]= ob->dloc[1]= 0.0f;
			if ((ob->protectflag & OB_LOCK_LOCZ)==0)
				ob->loc[2]= ob->dloc[2]= 0.0f;
		}
		ob->recalc |= OB_RECALC_OB;
	}
	CTX_DATA_END;

	if(armature_clear==0) /* in this case flush was done */
		ED_anim_dag_flush_update(C);	
	
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}


void OBJECT_OT_location_clear(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Clear Location";
	ot->description = "Clear the object's location.";
	ot->idname= "OBJECT_OT_location_clear";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_location_clear_exec;
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_rotation_clear_exec(bContext *C, wmOperator *op)
{
	int armature_clear= 0;

	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(!(ob->mode & OB_MODE_WEIGHT_PAINT)) {
			/* eulers can only get cleared if they are not protected */
			if ((ob->protectflag & OB_LOCK_ROTX)==0)
				ob->rot[0]= ob->drot[0]= 0.0f;
			if ((ob->protectflag & OB_LOCK_ROTY)==0)
				ob->rot[1]= ob->drot[1]= 0.0f;
			if ((ob->protectflag & OB_LOCK_ROTZ)==0)
				ob->rot[2]= ob->drot[2]= 0.0f;
		}
		ob->recalc |= OB_RECALC_OB;
	}
	CTX_DATA_END;

	if(armature_clear==0) /* in this case flush was done */
		ED_anim_dag_flush_update(C);	
	
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}


void OBJECT_OT_rotation_clear(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Clear Rotation";
	ot->description = "Clear the object's rotation.";
	ot->idname= "OBJECT_OT_rotation_clear";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_rotation_clear_exec;
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_scale_clear_exec(bContext *C, wmOperator *op)
{
	int armature_clear= 0;

	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(!(ob->mode & OB_MODE_WEIGHT_PAINT)) {
			if ((ob->protectflag & OB_LOCK_SCALEX)==0) {
				ob->dsize[0]= 0.0f;
				ob->size[0]= 1.0f;
			}
			if ((ob->protectflag & OB_LOCK_SCALEY)==0) {
				ob->dsize[1]= 0.0f;
				ob->size[1]= 1.0f;
			}
			if ((ob->protectflag & OB_LOCK_SCALEZ)==0) {
				ob->dsize[2]= 0.0f;
				ob->size[2]= 1.0f;
			}
		}
		ob->recalc |= OB_RECALC_OB;
	}
	CTX_DATA_END;
	
	if(armature_clear==0) /* in this case flush was done */
		ED_anim_dag_flush_update(C);	
	
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_scale_clear(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Clear Scale";
	ot->description = "Clear the object's scale.";
	ot->idname= "OBJECT_OT_scale_clear";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_scale_clear_exec;
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_origin_clear_exec(bContext *C, wmOperator *op)
{
	float *v1, *v3, mat[3][3];
	int armature_clear= 0;

	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		if(ob->parent) {
			v1= ob->loc;
			v3= ob->parentinv[3];
			
			Mat3CpyMat4(mat, ob->parentinv);
			VECCOPY(v3, v1);
			v3[0]= -v3[0];
			v3[1]= -v3[1];
			v3[2]= -v3[2];
			Mat3MulVecfl(mat, v3);
		}
		ob->recalc |= OB_RECALC_OB;
	}
	CTX_DATA_END;

	if(armature_clear==0) /* in this case flush was done */
		ED_anim_dag_flush_update(C);	
	
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_origin_clear(wmOperatorType *ot)
{

	/* identifiers */
	ot->name= "Clear Origin";
	ot->description = "Clear the object's origin.";
	ot->idname= "OBJECT_OT_origin_clear";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_origin_clear_exec;
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ********* clear/set restrict view *********/
static int object_restrictview_clear_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	Base *base;
	int changed = 0;
	
	/* XXX need a context loop to handle such cases */
	for(base = FIRSTBASE; base; base=base->next){
		if((base->lay & v3d->lay) && base->object->restrictflag & OB_RESTRICT_VIEW) {
			base->flag |= SELECT;
			base->object->flag = base->flag;
			base->object->restrictflag &= ~OB_RESTRICT_VIEW; 
			changed = 1;
		}
	}
	if (changed) {
		DAG_scene_sort(scene);
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_restrictview_clear(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Clear Restrict View";
	ot->description = "Reveal the object by setting the restrictview flag.";
	ot->idname= "OBJECT_OT_restrictview_clear";
	
	/* api callbacks */
	ot->exec= object_restrictview_clear_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_restrictview_set_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	short changed = 0;
	int unselected= RNA_boolean_get(op->ptr, "unselected");
	
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if(!unselected) {
			if (base->flag & SELECT){
				base->flag &= ~SELECT;
				base->object->flag = base->flag;
				base->object->restrictflag |= OB_RESTRICT_VIEW;
				changed = 1;
				if (base==BASACT) {
					ED_base_object_activate(C, NULL);
				}
			}
		}
		else {
			if (!(base->flag & SELECT)){
				base->object->restrictflag |= OB_RESTRICT_VIEW;
				changed = 1;
			}
		}	
	}
	CTX_DATA_END;

	if (changed) {
		DAG_scene_sort(scene);
		
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
		
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_restrictview_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Restrict View";
	ot->description = "Hide the object by setting the restrictview flag.";
	ot->idname= "OBJECT_OT_restrictview_set";
	
	/* api callbacks */
	ot->exec= object_restrictview_set_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected objects.");
	
}
/* ************* Slow Parent ******************* */
static int object_slowparent_set_exec(bContext *C, wmOperator *op)
{

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		
		if(base->object->parent) base->object->partype |= PARSLOW;
		base->object->recalc |= OB_RECALC_OB;
		
	}
	CTX_DATA_END;

	ED_anim_dag_flush_update(C);	
	
	WM_event_add_notifier(C, NC_SCENE, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_slowparent_set(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Set Slow Parent";
	ot->description = "Set the object's slow parent.";
	ot->idname= "OBJECT_OT_slow_parent_set";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_slowparent_set_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_slowparent_clear_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		if(base->object->parent) {
			if(base->object->partype & PARSLOW) {
				base->object->partype -= PARSLOW;
				where_is_object(scene, base->object);
				base->object->partype |= PARSLOW;
				base->object->recalc |= OB_RECALC_OB;
			}
		}
		
	}
	CTX_DATA_END;

	ED_anim_dag_flush_update(C);	
	
	WM_event_add_notifier(C, NC_SCENE, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_slowparent_clear(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Clear Slow Parent";
	ot->description = "Clear the object's slow parent.";
	ot->idname= "OBJECT_OT_slow_parent_clear";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_slowparent_clear_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
/* ******************** **************** */

// XXX
#define BEZSELECTED_HIDDENHANDLES(bezt)   ((G.f & G_HIDDENHANDLES) ? (bezt)->f2 & SELECT : BEZSELECTED(bezt))
/* only in edit mode */
void make_vertex_parent(Scene *scene, Object *obedit, View3D *v3d)
{
	EditVert *eve;
	Base *base;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	Object *par, *ob;
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

		nu= editnurb->first;
		while(nu) {
			if((nu->type & 7)==CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(BEZSELECTED_HIDDENHANDLES(bezt)) {
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
		error("Select either 1 or 3 vertices to parent to");
		return;
	}
	
	if(okee("Make vertex parent")==0) return;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			if(base!=BASACT) {
				
				ob= base->object;
				ob->recalc |= OB_RECALC;
				par= BASACT->object->parent;
				
				while(par) {
					if(par==ob) break;
					par= par->parent;
				}
				if(par) {
					error("Loop in parents");
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
	}
	
	DAG_scene_sort(scene);
}


/* ******************** make proxy operator *********************** */

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
			
			/* create operator properties, and assign the relevant pointers to that, 
			 * and add a menu entry which uses these props 
			 */
			WM_operator_properties_create(&props_ptr, op->idname);
				RNA_string_set(&props_ptr, "object", go->ob->id.name+2);
				RNA_string_set(&props_ptr, "group_object", go->ob->id.name+2);
			uiItemFullO(layout, go->ob->id.name+2, 0, op->idname, props_ptr.data, WM_OP_EXEC_REGION_WIN);
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
		
		/* create operator properties, and assign the relevant pointers to that, 
		 * and add a menu entry which uses these props 
		 */
		WM_operator_properties_create(&props_ptr, op->idname);
			RNA_string_set(&props_ptr, "object", ob->id.name+2);
		uiItemFullO(layout, op->type->name, 0, op->idname, props_ptr.data, WM_OP_EXEC_REGION_WIN);
		
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
		DAG_object_flush_update(scene, newob, OB_RECALC);
		
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

/* ******************** make parent operator *********************** */

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
			error("No active Bone");
			return OPERATOR_CANCELLED;
		}
	}
	
	/* context itterator */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		
		if(ob!=par) {
			
			if( test_parent_loop(par, ob) ) {
				error("Loop in parents");
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

/* *** make track ***** */
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
	
	RNA_def_enum(ot->srna, "type", prop_make_track_types, 0, "Type", "");
}

/* ************* Make Dupli Real ********* */
static void make_object_duplilist_real(Scene *scene, View3D *v3d, Base *base)
{
	Base *basen;
	Object *ob;
	ListBase *lb;
	DupliObject *dob;
	
	if(!base && !(base = BASACT))
		return;
	
	if(!(base->object->transflag & OB_DUPLI))
		return;
	
	lb= object_duplilist(scene, base->object);
	
	for(dob= lb->first; dob; dob= dob->next) {
		ob= copy_object(dob->ob);
		/* font duplis can have a totcol without material, we get them from parent
		* should be implemented better...
		*/
		if(ob->mat==NULL) ob->totcol= 0;
		
		basen= MEM_dupallocN(base);
		basen->flag &= ~OB_FROMDUPLI;
		BLI_addhead(&scene->base, basen);	/* addhead: othwise eternal loop */
		basen->object= ob;
		ob->ipo= NULL;		/* make sure apply works */
		ob->parent= ob->track= NULL;
		ob->disp.first= ob->disp.last= NULL;
		ob->transflag &= ~OB_DUPLI;	
		
		Mat4CpyMat4(ob->obmat, dob->mat);
		ED_object_apply_obmat(ob);
	}
	
	copy_object_set_idnew(scene, v3d, 0);
	
	free_object_duplilist(lb);
	
	base->object->transflag &= ~OB_DUPLI;	
}


static int object_duplicates_make_real_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	
	clear_id_newpoins();
		
	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		make_object_duplilist_real(scene, v3d, base);
	}
	CTX_DATA_END;

	DAG_scene_sort(CTX_data_scene(C));
	ED_anim_dag_flush_update(C);	
	WM_event_add_notifier(C, NC_SCENE, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_duplicates_make_real(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Make Duplicates Real";
	ot->description = "Make dupli objects attached to this object real.";
	ot->idname= "OBJECT_OT_duplicates_make_real";
	
	/* api callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= object_duplicates_make_real_exec;
	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
/* ******************* Set Object Center ********************** */

static EnumPropertyItem prop_set_center_types[] = {
	{0, "CENTER", 0, "ObData to Center", "Move object data around Object center"},
	{1, "CENTERNEW", 0, "Center New", "Move Object center to center of object data"},
	{2, "CENTERCURSOR", 0, "Center Cursor", "Move Object Center to position of the 3d cursor"},
	{0, NULL, 0, NULL, NULL}
};

/* 0 == do center, 1 == center new, 2 == center cursor */
static int object_center_set_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	Object *obedit= CTX_data_edit_object(C);
	Object *ob;
	Mesh *me, *tme;
	Curve *cu;
/*	BezTriple *bezt;
	BPoint *bp; */
	Nurb *nu, *nu1;
	EditVert *eve;
	float cent[3], centn[3], min[3], max[3], omat[3][3];
	int a, total= 0;
	int centermode = RNA_enum_get(op->ptr, "type");
	
	/* keep track of what is changed */
	int tot_change=0, tot_lib_error=0, tot_multiuser_arm_error=0;
	MVert *mvert;

	if(scene->id.lib || v3d==NULL){
		BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed on Lib data");
		 return OPERATOR_CANCELLED;
	}
	if (obedit && centermode > 0) {
		BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed in EditMode");
		return OPERATOR_CANCELLED;
	}	
	cent[0]= cent[1]= cent[2]= 0.0;	
	
	if(obedit) {

		INIT_MINMAX(min, max);
	
		if(obedit->type==OB_MESH) {
			Mesh *me= obedit->data;
			EditMesh *em = BKE_mesh_get_editmesh(me);

			for(eve= em->verts.first; eve; eve= eve->next) {
				if(v3d->around==V3D_CENTROID) {
					total++;
					VECADD(cent, cent, eve->co);
				}
				else {
					DO_MINMAX(eve->co, min, max);
				}
			}
			
			if(v3d->around==V3D_CENTROID) {
				VecMulf(cent, 1.0f/(float)total);
			}
			else {
				cent[0]= (min[0]+max[0])/2.0f;
				cent[1]= (min[1]+max[1])/2.0f;
				cent[2]= (min[2]+max[2])/2.0f;
			}
			
			for(eve= em->verts.first; eve; eve= eve->next) {
				VecSubf(eve->co, eve->co, cent);			
			}
			
			recalc_editnormals(em);
			tot_change++;
			DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
			BKE_mesh_end_editmesh(me, em);
		}
	}
	
	/* reset flags */
	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
			base->object->flag &= ~OB_DONE;
	}
	CTX_DATA_END;
	
	for (me= G.main->mesh.first; me; me= me->id.next) {
		me->flag &= ~ME_ISDONE;
	}
	
	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		if((base->object->flag & OB_DONE)==0) {
			base->object->flag |= OB_DONE;
				
			if(obedit==NULL && (me=get_mesh(base->object)) ) {
				if (me->id.lib) {
					tot_lib_error++;
				} else {
					if(centermode==2) {
						VECCOPY(cent, give_cursor(scene, v3d));
						Mat4Invert(base->object->imat, base->object->obmat);
						Mat4MulVecfl(base->object->imat, cent);
					} else {
						INIT_MINMAX(min, max);
						mvert= me->mvert;
						for(a=0; a<me->totvert; a++, mvert++) {
							DO_MINMAX(mvert->co, min, max);
						}
					
						cent[0]= (min[0]+max[0])/2.0f;
						cent[1]= (min[1]+max[1])/2.0f;
						cent[2]= (min[2]+max[2])/2.0f;
					}

					mvert= me->mvert;
					for(a=0; a<me->totvert; a++, mvert++) {
						VecSubf(mvert->co, mvert->co, cent);
					}
					
					if (me->key) {
						KeyBlock *kb;
						for (kb=me->key->block.first; kb; kb=kb->next) {
							float *fp= kb->data;
							
							for (a=0; a<kb->totelem; a++, fp+=3) {
								VecSubf(fp, fp, cent);
							}
						}
					}
						
					me->flag |= ME_ISDONE;
						
					if(centermode) {
						Mat3CpyMat4(omat, base->object->obmat);
						
						VECCOPY(centn, cent);
						Mat3MulVecfl(omat, centn);
						base->object->loc[0]+= centn[0];
						base->object->loc[1]+= centn[1];
						base->object->loc[2]+= centn[2];
						
						where_is_object(scene, base->object);
						ignore_parent_tx(scene, base->object);
						
						/* other users? */
						CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
							ob = base->object;
							if((ob->flag & OB_DONE)==0) {
								tme= get_mesh(ob);
								
								if(tme==me) {
									
									ob->flag |= OB_DONE;
									ob->recalc= OB_RECALC_OB|OB_RECALC_DATA;

									Mat3CpyMat4(omat, ob->obmat);
									VECCOPY(centn, cent);
									Mat3MulVecfl(omat, centn);
									ob->loc[0]+= centn[0];
									ob->loc[1]+= centn[1];
									ob->loc[2]+= centn[2];
									
									where_is_object(scene, ob);
									ignore_parent_tx(scene, ob);
									
									if(tme && (tme->flag & ME_ISDONE)==0) {
										mvert= tme->mvert;
										for(a=0; a<tme->totvert; a++, mvert++) {
											VecSubf(mvert->co, mvert->co, cent);
										}
										
										if (tme->key) {
											KeyBlock *kb;
											for (kb=tme->key->block.first; kb; kb=kb->next) {
												float *fp= kb->data;
												
												for (a=0; a<kb->totelem; a++, fp+=3) {
													VecSubf(fp, fp, cent);
												}
											}
										}
										
										tme->flag |= ME_ISDONE;
									}
								}
							}
							
							ob= ob->id.next;
						}
						CTX_DATA_END;
					}
					tot_change++;
				}
			}
			else if (ELEM(base->object->type, OB_CURVE, OB_SURF)) {
				
				/* weak code here... (ton) */
				if(obedit==base->object) {
					ListBase *editnurb= curve_get_editcurve(obedit);

					nu1= editnurb->first;
					cu= obedit->data;
				}
				else {
					cu= base->object->data;
					nu1= cu->nurb.first;
				}
				
				if (cu->id.lib) {
					tot_lib_error++;
				} else {
					if(centermode==2) {
						VECCOPY(cent, give_cursor(scene, v3d));
						Mat4Invert(base->object->imat, base->object->obmat);
						Mat4MulVecfl(base->object->imat, cent);

						/* don't allow Z change if curve is 2D */
						if( !( cu->flag & CU_3D ) )
							cent[2] = 0.0;
					} 
					else {
						INIT_MINMAX(min, max);
						
						nu= nu1;
						while(nu) {
							minmaxNurb(nu, min, max);
							nu= nu->next;
						}
						
						cent[0]= (min[0]+max[0])/2.0f;
						cent[1]= (min[1]+max[1])/2.0f;
						cent[2]= (min[2]+max[2])/2.0f;
					}
					
					nu= nu1;
					while(nu) {
						if( (nu->type & 7)==CU_BEZIER) {
							a= nu->pntsu;
							while (a--) {
								VecSubf(nu->bezt[a].vec[0], nu->bezt[a].vec[0], cent);
								VecSubf(nu->bezt[a].vec[1], nu->bezt[a].vec[1], cent);
								VecSubf(nu->bezt[a].vec[2], nu->bezt[a].vec[2], cent);
							}
						}
						else {
							a= nu->pntsu*nu->pntsv;
							while (a--)
								VecSubf(nu->bp[a].vec, nu->bp[a].vec, cent);
						}
						nu= nu->next;
					}
			
					if(centermode && obedit==0) {
						Mat3CpyMat4(omat, base->object->obmat);
						
						Mat3MulVecfl(omat, cent);
						base->object->loc[0]+= cent[0];
						base->object->loc[1]+= cent[1];
						base->object->loc[2]+= cent[2];
						
						where_is_object(scene, base->object);
						ignore_parent_tx(scene, base->object);
					}
					
					tot_change++;
					if(obedit) {
						if (centermode==0) {
							DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
						}
						break;
					}
				}
			}
			else if(base->object->type==OB_FONT) {
				/* get from bb */
				
				cu= base->object->data;
				
				if(cu->bb==0) {
					/* do nothing*/
				} else if (cu->id.lib) {
					tot_lib_error++;
				} else {
					cu->xof= -0.5f*( cu->bb->vec[4][0] - cu->bb->vec[0][0]);
					cu->yof= -0.5f -0.5f*( cu->bb->vec[0][1] - cu->bb->vec[2][1]);	/* extra 0.5 is the height o above line */
					
					/* not really ok, do this better once! */
					cu->xof /= cu->fsize;
					cu->yof /= cu->fsize;

					tot_change++;
				}
			}
			else if(base->object->type==OB_ARMATURE) {
				bArmature *arm = base->object->data;
				
				if (arm->id.lib) {
					tot_lib_error++;
				} else if(arm->id.us>1) {
					/*error("Can't apply to a multi user armature");
					return;*/
					tot_multiuser_arm_error++;
				} else {
					/* Function to recenter armatures in editarmature.c 
					 * Bone + object locations are handled there.
					 */
					docenter_armature(scene, v3d, base->object, centermode);
					tot_change++;
					
					where_is_object(scene, base->object);
					ignore_parent_tx(scene, base->object);
					
					if(obedit) 
						break;
				}
			}
			base->object->recalc= OB_RECALC_OB|OB_RECALC_DATA;
		}
	}
	CTX_DATA_END;
	
	if (tot_change) {
		ED_anim_dag_flush_update(C);
	}
	
	/* Warn if any errors occured */
	if (tot_lib_error+tot_multiuser_arm_error) {
		BKE_reportf(op->reports, RPT_WARNING, "%i Object(s) Not Centered, %i Changed:",tot_lib_error+tot_multiuser_arm_error, tot_change);		
		if (tot_lib_error)
			BKE_reportf(op->reports, RPT_WARNING, "|%i linked library objects",tot_lib_error);
		if (tot_multiuser_arm_error)
			BKE_reportf(op->reports, RPT_WARNING, "|%i multiuser armature object(s)",tot_multiuser_arm_error);
	}
	
	return OPERATOR_FINISHED;
}
void OBJECT_OT_center_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Center";
	ot->description = "Set the object's center, by either moving the data, or set to center of data, or use 3d cursor";
	ot->idname= "OBJECT_OT_center_set";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_center_set_exec;
	
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_set_center_types, 0, "Type", "");
}
/* ******************* toggle editmode operator  ***************** */

void ED_object_exit_editmode(bContext *C, int flag)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	int freedata = flag & EM_FREEDATA;
	
	if(obedit==NULL) return;
	
	if(flag & EM_WAITCURSOR) waitcursor(1);
	if(obedit->type==OB_MESH) {
		Mesh *me= obedit->data;
		
//		if(EM_texFaceCheck())
		
//		if(retopo_mesh_paint_check())
//			retopo_end_okee();
		
		if(me->edit_mesh->totvert>MESH_MAX_VERTS) {
			error("Too many vertices");
			return;
		}
		load_editMesh(scene, obedit);
		
		if(freedata) {
			free_editMesh(me->edit_mesh);
			MEM_freeN(me->edit_mesh);
			me->edit_mesh= NULL;
		}
		
		if(obedit->restore_mode & OB_MODE_WEIGHT_PAINT)
			mesh_octree_table(obedit, NULL, NULL, 'e');
	}
	else if (obedit->type==OB_ARMATURE) {	
		ED_armature_from_edit(scene, obedit);
		if(freedata)
			ED_armature_edit_free(obedit);
	}
	else if(ELEM(obedit->type, OB_CURVE, OB_SURF)) {
		load_editNurb(obedit);
		if(freedata) free_editNurb(obedit);
	}
	else if(obedit->type==OB_FONT && freedata) {
		load_editText(obedit);
		if(freedata) free_editText(obedit);
	}
	else if(obedit->type==OB_LATTICE) {
		load_editLatt(obedit);
		if(freedata) free_editLatt(obedit);
	}
	else if(obedit->type==OB_MBALL) {
		load_editMball(obedit);
		if(freedata) free_editMball(obedit);
	}

	/* freedata only 0 now on file saves */
	if(freedata) {
		/* for example; displist make is different in editmode */
		scene->obedit= NULL; // XXX for context
		
		/* also flush ob recalc, doesn't take much overhead, but used for particles */
		DAG_object_flush_update(scene, obedit, OB_RECALC_OB|OB_RECALC_DATA);
	
		ED_undo_push(C, "Editmode");
	
		if(flag & EM_WAITCURSOR) waitcursor(0);
	
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_OBJECT, scene);
	}

	obedit->mode &= ~OB_MODE_EDIT;
	ED_object_toggle_modes(C, obedit->restore_mode);
}


void ED_object_enter_editmode(bContext *C, int flag)
{
	Scene *scene= CTX_data_scene(C);
	Base *base= CTX_data_active_base(C);
	Object *ob;
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= NULL;
	int ok= 0;
	
	if(scene->id.lib) return;
	if(base==NULL) return;
	
	if(sa && sa->spacetype==SPACE_VIEW3D)
		v3d= sa->spacedata.first;
	
	if(v3d && (base->lay & v3d->lay)==0) return;
	else if(!v3d && (base->lay & scene->lay)==0) return;

	ob = base->object;

	if(ob==NULL) return;
	if(ob->data==NULL) return;
	
	if (object_data_is_libdata(ob)) {
		error_libdata();
		return;
	}
	
	if(flag & EM_WAITCURSOR) waitcursor(1);

	ob->restore_mode = ob->mode;
	ED_object_toggle_modes(C, ob->mode);

	ob->mode |= OB_MODE_EDIT;
	
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		
		if(me->pv) mesh_pmv_off(ob, me);
		ok= 1;
		scene->obedit= ob;	// context sees this
		
		make_editMesh(scene, ob);

		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_MESH, scene);
	}
	else if (ob->type==OB_ARMATURE){
		bArmature *arm= base->object->data;
		if (!arm) return;
		/*
		 * The function object_data_is_libdata make a problem here, the
		 * check for ob->proxy return 0 and let blender enter to edit mode
		 * this causa a crash when you try leave the edit mode.
		 * The problem is that i can't remove the ob->proxy check from
		 * object_data_is_libdata that prevent the bugfix #6614, so
		 * i add this little hack here.
		 */
		if(arm->id.lib) {
			error_libdata();
			return;
		}
		ok=1;
		scene->obedit= ob;
		ED_armature_to_edit(ob);
		/* to ensure all goes in restposition and without striding */
		DAG_object_flush_update(scene, ob, OB_RECALC);

		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_ARMATURE, scene);
	}
	else if(ob->type==OB_FONT) {
		scene->obedit= ob; // XXX for context
		ok= 1;
 		make_editText(ob);

		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_TEXT, scene);
	}
	else if(ob->type==OB_MBALL) {
		scene->obedit= ob; // XXX for context
		ok= 1;
		make_editMball(ob);

		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_MBALL, scene);
	}
	else if(ob->type==OB_LATTICE) {
		scene->obedit= ob; // XXX for context
		ok= 1;
		make_editLatt(ob);
		
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_LATTICE, scene);
	}
	else if(ob->type==OB_SURF || ob->type==OB_CURVE) {
		ok= 1;
		scene->obedit= ob; // XXX for context
		make_editNurb(ob);
		
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_CURVE, scene);
	}
	
	if(ok) {
		DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	}
	else {
		scene->obedit= NULL; // XXX for context
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_OBJECT, scene);
	}
	
	if(flag & EM_DO_UNDO) ED_undo_push(C, "Enter Editmode");
	if(flag & EM_WAITCURSOR) waitcursor(0);
}

static int editmode_toggle_exec(bContext *C, wmOperator *op)
{
	
	if(!CTX_data_edit_object(C))
		ED_object_enter_editmode(C, EM_WAITCURSOR);
	else
		ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR);
	
	return OPERATOR_FINISHED;
}

static int editmode_toggle_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && (ob->type == OB_MESH || ob->type == OB_ARMATURE ||
		      ob->type == OB_FONT || ob->type == OB_MBALL ||
		      ob->type == OB_LATTICE || ob->type == OB_SURF ||
		      ob->type == OB_CURVE);
}

void OBJECT_OT_editmode_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Toggle Editmode";
	ot->description = "Toggle object's editmode.";
	ot->idname= "OBJECT_OT_editmode_toggle";
	
	/* api callbacks */
	ot->exec= editmode_toggle_exec;
	
	ot->poll= editmode_toggle_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *************************** */

static int posemode_exec(bContext *C, wmOperator *op)
{
	Base *base= CTX_data_active_base(C);
	
	if(base->object->type==OB_ARMATURE) {
		if(base->object==CTX_data_edit_object(C)) {
			ED_object_exit_editmode(C, EM_FREEDATA);
			ED_armature_enter_posemode(C, base);
		}
		else if(base->object->mode & OB_MODE_POSE)
			ED_armature_exit_posemode(C, base);
		else
			ED_armature_enter_posemode(C, base);
		
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_PASS_THROUGH;
}

void OBJECT_OT_posemode_toggle(wmOperatorType *ot) 
{
	/* identifiers */
	ot->name= "Toggle Pose Mode";
	ot->idname= "OBJECT_OT_posemode_toggle";
	ot->description= "Enables or disables posing/selecting bones";
	
	/* api callbacks */
	ot->exec= posemode_exec;
	ot->poll= ED_operator_object_active;
	
	/* flag */
	ot->flag= OPTYPE_REGISTER;
}

/* *********************** */

void check_editmode(int type)
{
	Object *obedit= NULL; // XXX
	
	if (obedit==NULL || obedit->type==type) return;

// XXX	ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); /* freedata, and undo */
}
void movetolayer(Scene *scene, View3D *v3d)
{
	Base *base;
	unsigned int lay= 0, local;
	int islamp= 0;
	
	if(scene->id.lib) return;

	for(base= FIRSTBASE; base; base= base->next) {
		if (TESTBASE(v3d, base)) lay |= base->lay;
	}
	if(lay==0) return;
	lay &= 0xFFFFFF;
	
	if(lay==0) return;
	
	if(v3d->localview) {
		/* now we can move out of localview. */
		if (!okee("Move from localview")) return;
		for(base= FIRSTBASE; base; base= base->next) {
			if (TESTBASE(v3d, base)) {
				lay= base->lay & ~v3d->lay;
				base->lay= lay;
				base->object->lay= lay;
				base->object->flag &= ~SELECT;
				base->flag &= ~SELECT;
				if(base->object->type==OB_LAMP) islamp= 1;
			}
		}
	} else {
// XXX		if( movetolayer_buts(&lay, NULL)==0 ) return;
		
		/* normal non localview operation */
		for(base= FIRSTBASE; base; base= base->next) {
			if (TESTBASE(v3d, base)) {
				/* upper byte is used for local view */
				local= base->lay & 0xFF000000;  
				base->lay= lay + local;
				base->object->lay= lay;
				if(base->object->type==OB_LAMP) islamp= 1;
			}
		}
	}
	if(islamp) reshadeall_displist(scene);	/* only frees */
	
	/* warning, active object may be hidden now */
	
	DAG_scene_sort(scene);
	
}


#if 0
// XXX should be in view3d?

/* context: ob = lamp */
/* code should be replaced with proper (custom) transform handles for lamp properties */
static void spot_interactive(Object *ob, int mode)
{
	Lamp *la= ob->data;
	float transfac, dx, dy, ratio, origval;
	int keep_running= 1, center2d[2];
	short mval[2], mvalo[2];
	
//	getmouseco_areawin(mval);
//	getmouseco_areawin(mvalo);
	
	project_int(ob->obmat[3], center2d);
	if( center2d[0] > 100000 ) {		/* behind camera */
//		center2d[0]= curarea->winx/2;
//		center2d[1]= curarea->winy/2;
	}

//	helpline(mval, center2d);
	
	/* ratio is like scaling */
	dx = (float)(center2d[0] - mval[0]);
	dy = (float)(center2d[1] - mval[1]);
	transfac = (float)sqrt( dx*dx + dy*dy);
	if(transfac==0.0f) transfac= 1.0f;
	
	if(mode==1)	
		origval= la->spotsize;
	else if(mode==2)	
		origval= la->dist;
	else if(mode==3)	
		origval= la->clipsta;
	else	
		origval= la->clipend;
	
	while (keep_running>0) {
		
//		getmouseco_areawin(mval);
		
		/* essential for idling subloop */
		if(mval[0]==mvalo[0] && mval[1]==mvalo[1]) {
			PIL_sleep_ms(2);
		}
		else {
			char str[32];
			
			dx = (float)(center2d[0] - mval[0]);
			dy = (float)(center2d[1] - mval[1]);
			ratio = (float)(sqrt( dx*dx + dy*dy))/transfac;
			
			/* do the trick */
			
			if(mode==1) {	/* spot */
				la->spotsize = ratio*origval;
				CLAMP(la->spotsize, 1.0f, 180.0f);
				sprintf(str, "Spot size %.2f\n", la->spotsize);
			}
			else if(mode==2) {	/* dist */
				la->dist = ratio*origval;
				CLAMP(la->dist, 0.01f, 5000.0f);
				sprintf(str, "Distance %.2f\n", la->dist);
			}
			else if(mode==3) {	/* sta */
				la->clipsta = ratio*origval;
				CLAMP(la->clipsta, 0.001f, 5000.0f);
				sprintf(str, "Distance %.2f\n", la->clipsta);
			}
			else if(mode==4) {	/* end */
				la->clipend = ratio*origval;
				CLAMP(la->clipend, 0.1f, 5000.0f);
				sprintf(str, "Clip End %.2f\n", la->clipend);
			}

			/* cleanup */
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			/* handle shaded mode */
// XXX			shade_buttons_change_3d();

			/* DRAW */	
			headerprint(str);
			force_draw_plus(SPACE_BUTS, 0);

//			helpline(mval, center2d);
		}
		
		while( qtest() ) {
			short val;
			unsigned short event= extern_qread(&val);
			
			switch (event){
				case ESCKEY:
				case RIGHTMOUSE:
					keep_running= 0;
					break;
				case LEFTMOUSE:
				case SPACEKEY:
				case PADENTER:
				case RETKEY:
					if(val)
						keep_running= -1;
					break;
			}
		}
	}

	if(keep_running==0) {
		if(mode==1)	
			la->spotsize= origval;
		else if(mode==2)	
			la->dist= origval;
		else if(mode==3)	
			la->clipsta= origval;
		else	
			la->clipend= origval;
	}

}
#endif

void special_editmenu(Scene *scene, View3D *v3d)
{
// XXX	static short numcuts= 2;
	Object *ob= OBACT;
	Object *obedit= NULL; // XXX
	int nr,ret=0;
	
	if(ob==NULL) return;
	
	if(obedit==NULL) {
		
		if(ob->mode & OB_MODE_POSE) {
// XXX			pose_special_editmenu();
		}
		else if(paint_facesel_test(ob)) {
			Mesh *me= get_mesh(ob);
			MTFace *tface;
			MFace *mface;
			int a;
			
			if(me==0 || me->mtface==0) return;
			
			nr= pupmenu("Specials%t|Set     Tex%x1|         Shared%x2|         Light%x3|         Invisible%x4|         Collision%x5|         TwoSide%x6|Clr     Tex%x7|         Shared%x8|         Light%x9|         Invisible%x10|         Collision%x11|         TwoSide%x12");
			
			tface= me->mtface;
			mface= me->mface;
			for(a=me->totface; a>0; a--, tface++, mface++) {
				if(mface->flag & ME_FACE_SEL) {
					switch(nr) {
					case 1:
						tface->mode |= TF_TEX; break;
					case 2:
						tface->mode |= TF_SHAREDCOL; break;
					case 3:
						tface->mode |= TF_LIGHT; break; 
					case 4:
						tface->mode |= TF_INVISIBLE; break;
					case 5:
						tface->mode |= TF_DYNAMIC; break;
					case 6:
						tface->mode |= TF_TWOSIDE; break;
					case 7:
						tface->mode &= ~TF_TEX;
						tface->tpage= 0;
						break;
					case 8:
						tface->mode &= ~TF_SHAREDCOL; break;
					case 9:
						tface->mode &= ~TF_LIGHT; break;
					case 10:
						tface->mode &= ~TF_INVISIBLE; break;
					case 11:
						tface->mode &= ~TF_DYNAMIC; break;
					case 12:
						tface->mode &= ~TF_TWOSIDE; break;
					}
				}
			}
			DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
		}
		else if(ob->mode & OB_MODE_VERTEX_PAINT) {
			Mesh *me= get_mesh(ob);
			
			if(me==0 || (me->mcol==NULL && me->mtface==NULL) ) return;
			
			nr= pupmenu("Specials%t|Shared VertexCol%x1");
			if(nr==1) {
				
// XXX				do_shared_vertexcol(me);
				
				DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
			}
		}
		else if(ob->mode & OB_MODE_WEIGHT_PAINT) {
			Object *par= modifiers_isDeformedByArmature(ob);

			if(par && (par->mode & OB_MODE_POSE)) {
				nr= pupmenu("Specials%t|Apply Bone Envelopes to Vertex Groups %x1|Apply Bone Heat Weights to Vertex Groups %x2");

// XXX				if(nr==1 || nr==2)
// XXX					pose_adds_vgroups(ob, (nr == 2));
			}
		}
		else if(ob->mode & OB_MODE_PARTICLE_EDIT) {
#if 0
			// XXX
			ParticleSystem *psys = PE_get_current(ob);
			ParticleEditSettings *pset = PE_settings();

			if(!psys)
				return;

			if(pset->selectmode & SCE_SELECT_POINT)
				nr= pupmenu("Specials%t|Rekey%x1|Subdivide%x2|Select First%x3|Select Last%x4|Remove Doubles%x5");
			else
				nr= pupmenu("Specials%t|Rekey%x1|Remove Doubles%x5");
			
			switch(nr) {
			case 1:
// XXX				if(button(&pset->totrekey, 2, 100, "Number of Keys:")==0) return;
				waitcursor(1);
				PE_rekey();
				break;
			case 2:
				PE_subdivide();
				break;
			case 3:
				PE_select_root();
				break;
			case 4:
				PE_select_tip();
				break;
			case 5:
				PE_remove_doubles();
				break;
			}
			
			DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
			
			if(nr>0) waitcursor(0);
#endif
		}
		else {
			Base *base, *base_select= NULL;
			
			/* Get the active object mesh. */
			Mesh *me= get_mesh(ob);

			/* Booleans, if the active object is a mesh... */
			if (me && ob->id.lib==NULL) {
				
				/* Bring up a little menu with the boolean operation choices on. */
				nr= pupmenu("Boolean Tools%t|Intersect%x1|Union%x2|Difference%x3|Add Intersect Modifier%x4|Add Union Modifier%x5|Add Difference Modifier%x6");

				if (nr > 0) {
					/* user has made a choice of a menu element.
					   All of the boolean functions require 2 mesh objects 
					   we search through the object list to find the other 
					   selected item and make sure it is distinct and a mesh. */

					for(base= FIRSTBASE; base; base= base->next) {
						if(TESTBASELIB(v3d, base)) {
							if(base->object != ob) base_select= base;
						}
					}

					if (base_select) {
						if (get_mesh(base_select->object)) {
							if(nr <= 3){
								waitcursor(1);
// XXX								ret = NewBooleanMesh(BASACT,base_select,nr);
								if (ret==0) {
									error("An internal error occurred");
								} else if(ret==-1) {
									error("Selected meshes must have faces to perform boolean operations");
								} else if (ret==-2) {
									error("Both meshes must be a closed mesh");
								}
								waitcursor(0);
							} else {
								BooleanModifierData *bmd = NULL;
								bmd = (BooleanModifierData *)modifier_new(eModifierType_Boolean);
								BLI_addtail(&ob->modifiers, bmd);
								bmd->object = base_select->object;
								bmd->modifier.mode |= eModifierMode_Realtime;
								switch(nr){
									case 4: bmd->operation = eBooleanModifierOp_Intersect; break;
									case 5: bmd->operation = eBooleanModifierOp_Union; break;
									case 6:	bmd->operation = eBooleanModifierOp_Difference; break;
								}
// XXX								do_common_editbuts(B_CHANGEDEP);
							}								
						} else {
							error("Please select 2 meshes");
						}
					} else {
						error("Please select 2 meshes");
					}
				}

			}
			else if (ob->type == OB_FONT) {
				/* removed until this gets a decent implementation (ton) */
/*				nr= pupmenu("Split %t|Characters%x1");
				if (nr > 0) {
					switch(nr) {
						case 1: split_font();
					}
				}
*/
			}			
		}
	}
	else if(obedit->type==OB_MESH) {
	}
	else if(ELEM(obedit->type, OB_CURVE, OB_SURF)) {
	}
	else if(obedit->type==OB_ARMATURE) {
		nr= pupmenu("Specials%t|Subdivide %x1|Subdivide Multi%x2|Switch Direction%x7|Flip Left-Right Names%x3|%l|AutoName Left-Right%x4|AutoName Front-Back%x5|AutoName Top-Bottom%x6");
//		if(nr==1)
// XXX			subdivide_armature(1);
		if(nr==2) {
// XXX			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			waitcursor(1);
// XXX			subdivide_armature(numcuts);
		}
//		else if(nr==3)
// XXX			armature_flip_names();
		else if(ELEM3(nr, 4, 5, 6)) {
// XXX			armature_autoside_names(nr-4);
		}
//		else if(nr == 7)
// XXX			switch_direction_armature();
	}
	else if(obedit->type==OB_LATTICE) {
		Lattice *lt= obedit->data;
		static float weight= 1.0f;
		{ // XXX
// XXX		if(fbutton(&weight, 0.0f, 1.0f, 10, 10, "Set Weight")) {
			int a= lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
			BPoint *bp= lt->editlatt->def;
			
			while(a--) {
				if(bp->f1 & SELECT)
					bp->weight= weight;
				bp++;
			}	
		}
	}

}

static void curvetomesh(Scene *scene, Object *ob) 
{
	Curve *cu;
	DispList *dl;
	
	ob->flag |= OB_DONE;
	cu= ob->data;
	
	dl= cu->disp.first;
	if(dl==0) makeDispListCurveTypes(scene, ob, 0);		/* force creation */

	nurbs_to_mesh(ob); /* also does users */
	if (ob->type != OB_MESH) {
		error("can't convert curve to mesh");
	} else {
		object_free_modifiers(ob);
	}
}

void convertmenu(Scene *scene, View3D *v3d)
{
	Base *base, *basen=NULL, *basact, *basedel=NULL;
	Object *obact, *ob, *ob1;
	Object *obedit= NULL; // XXX
	Curve *cu;
	Nurb *nu;
	MetaBall *mb;
	Mesh *me;
	int ok=0, nr = 0, a;
	
	if(scene->id.lib) return;

	obact= OBACT;
	if (obact == NULL) return;
	if(!obact->flag & SELECT) return;
	if(obedit) return;
	
	basact= BASACT;	/* will be restored */
		
	if(obact->type==OB_FONT) {
		nr= pupmenu("Convert Font to%t|Curve%x1|Curve (Single filling group)%x2|Mesh%x3");
		if(nr>0) ok= 1;
	}
	else if(obact->type==OB_MBALL) {
		nr= pupmenu("Convert Metaball to%t|Mesh (keep original)%x1|Mesh (Delete Original)%x2");
		if(nr>0) ok= 1;
	}
	else if(obact->type==OB_CURVE) {
		nr= pupmenu("Convert Curve to%t|Mesh");
		if(nr>0) ok= 1;
	}
	else if(obact->type==OB_SURF) {
		nr= pupmenu("Convert Nurbs Surface to%t|Mesh");
		if(nr>0) ok= 1;
	}
	else if(obact->type==OB_MESH) {
		nr= pupmenu("Convert Modifiers to%t|Mesh (Keep Original)%x1|Mesh (Delete Original)%x2");
		if(nr>0) ok= 1;
	}
	if(ok==0) return;

	/* don't forget multiple users! */

	/* reset flags */
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			base->object->flag &= ~OB_DONE;
		}
	}

	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			
			ob= base->object;
			
			if(ob->flag & OB_DONE);
			else if(ob->type==OB_MESH && ob->modifiers.first) { /* converting a mesh with no modifiers causes a segfault */
				DerivedMesh *dm;
				
				basedel = base;

				ob->flag |= OB_DONE;

				ob1= copy_object(ob);
				ob1->recalc |= OB_RECALC;

				basen= MEM_mallocN(sizeof(Base), "duplibase");
				*basen= *base;
				BLI_addhead(&scene->base, basen);	/* addhead: otherwise eternal loop */
				basen->object= ob1;
				basen->flag |= SELECT;
				base->flag &= ~SELECT;
				ob->flag &= ~SELECT;

				/* decrement original mesh's usage count  */
				me= ob1->data;
				me->id.us--;

				/* make a new copy of the mesh */
				ob1->data= copy_mesh(me);

				/* make new mesh data from the original copy */
				dm= mesh_get_derived_final(scene, ob1, CD_MASK_MESH);
				/* dm= mesh_create_derived_no_deform(ob1, NULL);	this was called original (instead of get_derived). man o man why! (ton) */
				
				DM_to_mesh(dm, ob1->data);

				dm->release(dm);
				object_free_modifiers(ob1);	/* after derivedmesh calls! */
				
				/* If the original object is active then make this object active */
				if (ob == obact) {
					// XXX ED_base_object_activate(C, basen);
					basact = basen;
				}
			}
			else if(ob->type==OB_FONT) {
				ob->flag |= OB_DONE;

				ob->type= OB_CURVE;
				cu= ob->data;

				if(cu->vfont) {
					cu->vfont->id.us--;
					cu->vfont= 0;
				}
				if(cu->vfontb) {
					cu->vfontb->id.us--;
					cu->vfontb= 0;
				}
				if(cu->vfonti) {
					cu->vfonti->id.us--;
					cu->vfonti= 0;
				}
				if(cu->vfontbi) {
					cu->vfontbi->id.us--;
					cu->vfontbi= 0;
				}					
				/* other users */
				if(cu->id.us>1) {
					ob1= G.main->object.first;
					while(ob1) {
						if(ob1->data==cu) {
							ob1->type= OB_CURVE;
							ob1->recalc |= OB_RECALC;
						}
						ob1= ob1->id.next;
					}
				}
				if (nr==2 || nr==3) {
					nu= cu->nurb.first;
					while(nu) {
						nu->charidx= 0;
						nu= nu->next;
					}					
				}
				if (nr==3) {
					curvetomesh(scene, ob);
				}
			}
			else if(ELEM(ob->type, OB_CURVE, OB_SURF)) {
				if(nr==1) {
					curvetomesh(scene, ob);
 				}
			}
			else if(ob->type==OB_MBALL) {
			
				if(nr==1 || nr == 2) {
					ob= find_basis_mball(scene, ob);
					
					if(ob->disp.first && !(ob->flag&OB_DONE)) {
						basedel = base;
					
						ob->flag |= OB_DONE;

						ob1= copy_object(ob);
						ob1->recalc |= OB_RECALC;

						basen= MEM_mallocN(sizeof(Base), "duplibase");
						*basen= *base;
						BLI_addhead(&scene->base, basen);	/* addhead: othwise eternal loop */
						basen->object= ob1;
						basen->flag |= SELECT;
						basedel->flag &= ~SELECT;
						ob->flag &= ~SELECT;
						
						mb= ob1->data;
						mb->id.us--;
						
						ob1->data= add_mesh("Mesh");
						ob1->type= OB_MESH;
						
						me= ob1->data;
						me->totcol= mb->totcol;
						if(ob1->totcol) {
							me->mat= MEM_dupallocN(mb->mat);
							for(a=0; a<ob1->totcol; a++) id_us_plus((ID *)me->mat[a]);
						}
						
						mball_to_mesh(&ob->disp, ob1->data);
						
						/* So we can see the wireframe */
						BASACT= basen;
						
						/* If the original object is active then make this object active */
						if (ob == obact) {
							// XXX ED_base_object_activate(C, basen);
							basact = basen;
						}
						
					}
				}
			}
		}
		if(basedel != NULL && nr == 2) {
			ED_base_object_free_and_unlink(scene, basedel);	
		}
		basedel = NULL;				
	}
	
	/* delete object should renew depsgraph */
	if(nr==2)
		DAG_scene_sort(scene);

	/* texspace and normals */
	if(!basen) BASACT= base;

// XXX	ED_object_enter_editmode(C, 0);
// XXX	exit_editmode(C, EM_FREEDATA|EM_WAITCURSOR); /* freedata, but no undo */
	BASACT= basact;


	DAG_scene_sort(scene);
}

/* Change subdivision or particle properties of mesh object ob, if level==-1
 * then toggle subsurf, else set to level set allows to toggle multiple
 * selections */

static void object_has_subdivision_particles(Object *ob, int *havesubdiv, int *havepart, int depth)
{
	if(ob->type==OB_MESH) {
		if(modifiers_findByType(ob, eModifierType_Subsurf))
			*havesubdiv= 1;
		if(modifiers_findByType(ob, eModifierType_ParticleSystem))
			*havepart= 1;
	}

	if(ob->dup_group && depth <= 4) {
		GroupObject *go;

		for(go= ob->dup_group->gobject.first; go; go= go->next)
			object_has_subdivision_particles(go->ob, havesubdiv, havepart, depth+1);
	}
}

static void object_flip_subdivison_particles(Scene *scene, Object *ob, int *set, int level, int mode, int particles, int depth)
{
	ModifierData *md;

	if(ob->type==OB_MESH) {
		if(particles) {
			for(md=ob->modifiers.first; md; md=md->next) {
				if(md->type == eModifierType_ParticleSystem) {
					ParticleSystemModifierData *psmd = (ParticleSystemModifierData*)md;

					if(*set == -1)
						*set= psmd->modifier.mode&(mode);

					if (*set)
						psmd->modifier.mode &= ~(mode);
					else
						psmd->modifier.mode |= (mode);
				}
			}
		}
		else {
			md = modifiers_findByType(ob, eModifierType_Subsurf);

			if (md) {
				SubsurfModifierData *smd = (SubsurfModifierData*) md;

				if (level == -1) {
					if(*set == -1) 
						*set= smd->modifier.mode&(mode);

					if (*set)
						smd->modifier.mode &= ~(mode);
					else
						smd->modifier.mode |= (mode);
				} else {
					smd->levels = level;
				}
			} 
			else if(depth == 0 && *set != 0) {
				SubsurfModifierData *smd = (SubsurfModifierData*) modifier_new(eModifierType_Subsurf);

				BLI_addtail(&ob->modifiers, smd);

				if (level!=-1) {
					smd->levels = level;
				}
				
				if(*set == -1)
					*set= 1;
			}
		}

		DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	}

	if(ob->dup_group && depth<=4) {
		GroupObject *go;

		for(go= ob->dup_group->gobject.first; go; go= go->next)
			object_flip_subdivison_particles(scene, go->ob, set, level, mode, particles, depth+1);
	}
}

/* Change subdivision properties of mesh object ob, if
* level==-1 then toggle subsurf, else set to level.
*/

void flip_subdivison(Scene *scene, View3D *v3d, int level)
{
	Base *base;
	int set= -1;
	int mode, pupmode, particles= 0, havesubdiv= 0, havepart= 0;
	int alt= 0; // XXX
	
	if(alt)
		mode= eModifierMode_Realtime;
	else
		mode= eModifierMode_Render|eModifierMode_Realtime;
	
	if(level == -1) {
		if (scene->obedit) { // XXX get from context
			object_has_subdivision_particles(scene->obedit, &havesubdiv, &havepart, 0);			
		} else {
			for(base= scene->base.first; base; base= base->next) {
				if(((level==-1) && (TESTBASE(v3d, base))) || (TESTBASELIB(v3d, base))) {
					object_has_subdivision_particles(base->object, &havesubdiv, &havepart, 0);
				}
			}
		}
	}
	else
		havesubdiv= 1;
	
	if(havesubdiv && havepart) {
		pupmode= pupmenu("Switch%t|Subsurf %x1|Particle Systems %x2");
		if(pupmode <= 0)
			return;
		else if(pupmode == 2)
			particles= 1;
	}
	else if(havepart)
		particles= 1;

	if (scene->obedit) {	 // XXX get from context
		object_flip_subdivison_particles(scene, scene->obedit, &set, level, mode, particles, 0);
	} else {
		for(base= scene->base.first; base; base= base->next) {
			if(((level==-1) && (TESTBASE(v3d, base))) || (TESTBASELIB(v3d, base))) {
				object_flip_subdivison_particles(scene, base->object, &set, level, mode, particles, 0);
			}
		}
	}
	
	ED_anim_dag_flush_update(C);	
}
 
static void copymenu_properties(Scene *scene, View3D *v3d, Object *ob)
{	
	bProperty *prop;
	Base *base;
	int nr, tot=0;
	char *str;
	
	prop= ob->prop.first;
	while(prop) {
		tot++;
		prop= prop->next;
	}
	
	str= MEM_callocN(50 + 33*tot, "copymenu prop");
	
	if (tot)
		strcpy(str, "Copy Property %t|Replace All|Merge All|%l");
	else
		strcpy(str, "Copy Property %t|Clear All (no properties on active)");
	
	tot= 0;	
	prop= ob->prop.first;
	while(prop) {
		tot++;
		strcat(str, "|");
		strcat(str, prop->name);
		prop= prop->next;
	}

	nr= pupmenu(str);
	
	if ( nr==1 || nr==2 ) {
		for(base= FIRSTBASE; base; base= base->next) {
			if((base != BASACT) &&(TESTBASELIB(v3d, base))) {
				if (nr==1) { /* replace */
					copy_properties( &base->object->prop, &ob->prop );
				} else {
					for(prop = ob->prop.first; prop; prop= prop->next ) {
						set_ob_property(base->object, prop);
					}
				}
			}
		}
	} else if(nr>0) {
		prop = BLI_findlink(&ob->prop, nr-4); /* account for first 3 menu items & menu index starting at 1*/
		
		if(prop) {
			for(base= FIRSTBASE; base; base= base->next) {
				if((base != BASACT) &&(TESTBASELIB(v3d, base))) {
					set_ob_property(base->object, prop);
				}
			}
		}
	}
	MEM_freeN(str);
	
}

static void copymenu_logicbricks(Scene *scene, View3D *v3d, Object *ob)
{
	Base *base;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(base->object != ob) {
			if(TESTBASELIB(v3d, base)) {
				
				/* first: free all logic */
				free_sensors(&base->object->sensors);				
				unlink_controllers(&base->object->controllers);
				free_controllers(&base->object->controllers);
				unlink_actuators(&base->object->actuators);
				free_actuators(&base->object->actuators);
				
				/* now copy it, this also works without logicbricks! */
				clear_sca_new_poins_ob(ob);
				copy_sensors(&base->object->sensors, &ob->sensors);
				copy_controllers(&base->object->controllers, &ob->controllers);
				copy_actuators(&base->object->actuators, &ob->actuators);
				set_sca_new_poins_ob(base->object);
				
				/* some menu settings */
				base->object->scavisflag= ob->scavisflag;
				base->object->scaflag= ob->scaflag;
				
				/* set the initial state */
				base->object->state= ob->state;
				base->object->init_state= ob->init_state;
			}
		}
	}
}

static void copymenu_modifiers(Scene *scene, View3D *v3d, Object *ob)
{
	Base *base;
	int i, event;
	char str[512];
	char *errorstr= NULL;

	strcpy(str, "Copy Modifiers %t");

	sprintf(str+strlen(str), "|All%%x%d|%%l", NUM_MODIFIER_TYPES);

	for (i=eModifierType_None+1; i<NUM_MODIFIER_TYPES; i++) {
		ModifierTypeInfo *mti = modifierType_getInfo(i);

		if(ELEM3(i, eModifierType_Hook, eModifierType_Softbody, eModifierType_ParticleInstance)) continue;
		
		if(i == eModifierType_Collision)
			continue;

		if (	(mti->flags&eModifierTypeFlag_AcceptsCVs) || 
				(ob->type==OB_MESH && (mti->flags&eModifierTypeFlag_AcceptsMesh))) {
			sprintf(str+strlen(str), "|%s%%x%d", mti->name, i);
		}
	}

	event = pupmenu(str);
	if(event<=0) return;

	for (base= FIRSTBASE; base; base= base->next) {
		if(base->object != ob) {
			if(TESTBASELIB(v3d, base)) {
				ModifierData *md;

				base->object->recalc |= OB_RECALC_OB|OB_RECALC_DATA;

				if (base->object->type==ob->type) {
					/* copy all */
					if (event==NUM_MODIFIER_TYPES) {
						object_free_modifiers(base->object);

						for (md=ob->modifiers.first; md; md=md->next) {
							ModifierData *nmd = NULL;
							
							if(ELEM3(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_ParticleInstance)) continue;
		
							if(md->type == eModifierType_Collision)
								continue;
							
							nmd = modifier_new(md->type);
							modifier_copyData(md, nmd);
							BLI_addtail(&base->object->modifiers, nmd);
						}

						copy_object_particlesystems(base->object, ob);
						copy_object_softbody(base->object, ob);
					} else {
						/* copy specific types */
						ModifierData *md, *mdn;
						
						/* remove all with type 'event' */
						for (md=base->object->modifiers.first; md; md=mdn) {
							mdn= md->next;
							if(md->type==event) {
								BLI_remlink(&base->object->modifiers, md);
								modifier_free(md);
							}
						}
						
						/* copy all with type 'event' */
						for (md=ob->modifiers.first; md; md=md->next) {
							if (md->type==event) {
								
								mdn = modifier_new(event);
								BLI_addtail(&base->object->modifiers, mdn);

								modifier_copyData(md, mdn);
							}
						}

						if(event == eModifierType_ParticleSystem) {
							object_free_particlesystems(base->object);
							copy_object_particlesystems(base->object, ob);
						}
						else if(event == eModifierType_Softbody) {
							object_free_softbody(base->object);
							copy_object_softbody(base->object, ob);
						}
					}
				}
				else
					errorstr= "Did not copy modifiers to other Object types";
			}
		}
	}
	
//	if(errorstr) notice(errorstr);
	
	DAG_scene_sort(scene);
	
}

/* both pointers should exist */
static void copy_texture_space(Object *to, Object *ob)
{
	float *poin1= NULL, *poin2= NULL;
	int texflag= 0;
	
	if(ob->type==OB_MESH) {
		texflag= ((Mesh *)ob->data)->texflag;
		poin2= ((Mesh *)ob->data)->loc;
	}
	else if (ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		texflag= ((Curve *)ob->data)->texflag;
		poin2= ((Curve *)ob->data)->loc;
	}
	else if(ob->type==OB_MBALL) {
		texflag= ((MetaBall *)ob->data)->texflag;
		poin2= ((MetaBall *)ob->data)->loc;
	}
	else
		return;
		
	if(to->type==OB_MESH) {
		((Mesh *)to->data)->texflag= texflag;
		poin1= ((Mesh *)to->data)->loc;
	}
	else if (ELEM3(to->type, OB_CURVE, OB_SURF, OB_FONT)) {
		((Curve *)to->data)->texflag= texflag;
		poin1= ((Curve *)to->data)->loc;
	}
	else if(to->type==OB_MBALL) {
		((MetaBall *)to->data)->texflag= texflag;
		poin1= ((MetaBall *)to->data)->loc;
	}
	else
		return;
	
	memcpy(poin1, poin2, 9*sizeof(float));	/* this was noted in DNA_mesh, curve, mball */
	
	if(to->type==OB_MESH) ;
	else if(to->type==OB_MBALL) tex_space_mball(to);
	else tex_space_curve(to->data);
	
}

void copy_attr(Scene *scene, View3D *v3d, short event)
{
	Object *ob;
	Base *base;
	Curve *cu, *cu1;
	Nurb *nu;
	int do_scene_sort= 0;
	
	if(scene->id.lib) return;

	if(!(ob=OBACT)) return;
	
	if(scene->obedit) { // XXX get from context
		/* obedit_copymenu(); */
		return;
	}
	if(event==9) {
		copymenu_properties(scene, v3d, ob);
		return;
	}
	else if(event==10) {
		copymenu_logicbricks(scene, v3d, ob);
		return;
	}
	else if(event==24) {
		copymenu_modifiers(scene, v3d, ob);
		return;
	}

	for(base= FIRSTBASE; base; base= base->next) {
		if(base != BASACT) {
			if(TESTBASELIB(v3d, base)) {
				base->object->recalc |= OB_RECALC_OB;
				
				if(event==1) {  /* loc */
					VECCOPY(base->object->loc, ob->loc);
					VECCOPY(base->object->dloc, ob->dloc);
				}
				else if(event==2) {  /* rot */
					VECCOPY(base->object->rot, ob->rot);
					VECCOPY(base->object->drot, ob->drot);
					/* Quats arnt used yet */
					/*VECCOPY(base->object->quat, ob->quat);
					VECCOPY(base->object->dquat, ob->dquat);*/
				}
				else if(event==3) {  /* size */
					VECCOPY(base->object->size, ob->size);
					VECCOPY(base->object->dsize, ob->dsize);
				}
				else if(event==4) {  /* drawtype */
					base->object->dt= ob->dt;
					base->object->dtx= ob->dtx;
					base->object->empty_drawtype= ob->empty_drawtype;
					base->object->empty_drawsize= ob->empty_drawsize;
				}
				else if(event==5) {  /* time offs */
					base->object->sf= ob->sf;
				}
				else if(event==6) {  /* dupli */
					base->object->dupon= ob->dupon;
					base->object->dupoff= ob->dupoff;
					base->object->dupsta= ob->dupsta;
					base->object->dupend= ob->dupend;
					
					base->object->transflag &= ~OB_DUPLI;
					base->object->transflag |= (ob->transflag & OB_DUPLI);

					base->object->dup_group= ob->dup_group;
					if(ob->dup_group)
						id_us_plus((ID *)ob->dup_group);
				}
				else if(event==7) {	/* mass */
					base->object->mass= ob->mass;
				}
				else if(event==8) {	/* damping */
					base->object->damping= ob->damping;
					base->object->rdamping= ob->rdamping;
				}
				else if(event==11) {	/* all physical attributes */
					base->object->gameflag = ob->gameflag;
					base->object->inertia = ob->inertia;
					base->object->formfactor = ob->formfactor;
					base->object->damping= ob->damping;
					base->object->rdamping= ob->rdamping;
					base->object->min_vel= ob->min_vel;
					base->object->max_vel= ob->max_vel;
					if (ob->gameflag & OB_BOUNDS) {
						base->object->boundtype = ob->boundtype;
					}
					base->object->margin= ob->margin;
					base->object->bsoft= copy_bulletsoftbody(ob->bsoft);

				}
				else if(event==17) {	/* tex space */
					copy_texture_space(base->object, ob);
				}
				else if(event==18) {	/* font settings */
					
					if(base->object->type==ob->type) {
						cu= ob->data;
						cu1= base->object->data;
						
						cu1->spacemode= cu->spacemode;
						cu1->spacing= cu->spacing;
						cu1->linedist= cu->linedist;
						cu1->shear= cu->shear;
						cu1->fsize= cu->fsize;
						cu1->xof= cu->xof;
						cu1->yof= cu->yof;
						cu1->textoncurve= cu->textoncurve;
						cu1->wordspace= cu->wordspace;
						cu1->ulpos= cu->ulpos;
						cu1->ulheight= cu->ulheight;
						if(cu1->vfont) cu1->vfont->id.us--;
						cu1->vfont= cu->vfont;
						id_us_plus((ID *)cu1->vfont);
						if(cu1->vfontb) cu1->vfontb->id.us--;
						cu1->vfontb= cu->vfontb;
						id_us_plus((ID *)cu1->vfontb);
						if(cu1->vfonti) cu1->vfonti->id.us--;
						cu1->vfonti= cu->vfonti;
						id_us_plus((ID *)cu1->vfonti);
						if(cu1->vfontbi) cu1->vfontbi->id.us--;
						cu1->vfontbi= cu->vfontbi;
						id_us_plus((ID *)cu1->vfontbi);						

						BKE_text_to_curve(scene, base->object, 0);		/* needed? */

						
						strcpy(cu1->family, cu->family);
						
						base->object->recalc |= OB_RECALC_DATA;
					}
				}
				else if(event==19) {	/* bevel settings */
					
					if(ELEM(base->object->type, OB_CURVE, OB_FONT)) {
						cu= ob->data;
						cu1= base->object->data;
						
						cu1->bevobj= cu->bevobj;
						cu1->taperobj= cu->taperobj;
						cu1->width= cu->width;
						cu1->bevresol= cu->bevresol;
						cu1->ext1= cu->ext1;
						cu1->ext2= cu->ext2;
						
						base->object->recalc |= OB_RECALC_DATA;
					}
				}
				else if(event==25) {	/* curve resolution */

					if(ELEM(base->object->type, OB_CURVE, OB_FONT)) {
						cu= ob->data;
						cu1= base->object->data;
						
						cu1->resolu= cu->resolu;
						cu1->resolu_ren= cu->resolu_ren;
						
						nu= cu1->nurb.first;
						
						while(nu) {
							nu->resolu= cu1->resolu;
							nu= nu->next;
						}
						
						base->object->recalc |= OB_RECALC_DATA;
					}
				}
				else if(event==21){
					if (base->object->type==OB_MESH) {
						ModifierData *md = modifiers_findByType(ob, eModifierType_Subsurf);

						if (md) {
							ModifierData *tmd = modifiers_findByType(base->object, eModifierType_Subsurf);

							if (!tmd) {
								tmd = modifier_new(eModifierType_Subsurf);
								BLI_addtail(&base->object->modifiers, tmd);
							}

							modifier_copyData(md, tmd);
							base->object->recalc |= OB_RECALC_DATA;
						}
					}
				}
				else if(event==22) {
					/* Copy the constraint channels over */
					copy_constraints(&base->object->constraints, &ob->constraints);
					
					do_scene_sort= 1;
				}
				else if(event==23) {
					base->object->softflag= ob->softflag;
					if(base->object->soft) sbFree(base->object->soft);
					
					base->object->soft= copy_softbody(ob->soft);

					if (!modifiers_findByType(base->object, eModifierType_Softbody)) {
						BLI_addhead(&base->object->modifiers, modifier_new(eModifierType_Softbody));
					}
				}
				else if(event==26) {
#if 0 // XXX old animation system
					copy_nlastrips(&base->object->nlastrips, &ob->nlastrips);
#endif // XXX old animation system
				}
				else if(event==27) {	/* autosmooth */
					if (base->object->type==OB_MESH) {
						Mesh *me= ob->data;
						Mesh *cme= base->object->data;
						cme->smoothresh= me->smoothresh;
						if(me->flag & ME_AUTOSMOOTH)
							cme->flag |= ME_AUTOSMOOTH;
						else
							cme->flag &= ~ME_AUTOSMOOTH;
					}
				}
				else if(event==28) { /* UV orco */
					if(ELEM(base->object->type, OB_CURVE, OB_SURF)) {
						cu= ob->data;
						cu1= base->object->data;
						
						if(cu->flag & CU_UV_ORCO)
							cu1->flag |= CU_UV_ORCO;
						else
							cu1->flag &= ~CU_UV_ORCO;
					}		
				}
				else if(event==29) { /* protected bits */
					base->object->protectflag= ob->protectflag;
				}
				else if(event==30) { /* index object */
					base->object->index= ob->index;
				}
				else if(event==31) { /* object color */
					QUATCOPY(base->object->col, ob->col);
				}
			}
		}
	}
	
	if(do_scene_sort)
		DAG_scene_sort(scene);

	ED_anim_dag_flush_update(C);	

}

void copy_attr_menu(Scene *scene, View3D *v3d)
{
	Object *ob;
	short event;
	char str[512];
	
	if(!(ob=OBACT)) return;
	
	if (scene->obedit) { // XXX get from context
//		if (ob->type == OB_MESH)
// XXX			mesh_copy_menu();
		return;
	}
	
	/* Object Mode */
	
	/* If you change this menu, don't forget to update the menu in header_view3d.c
	 * view3d_edit_object_copyattrmenu() and in toolbox.c
	 */
	
	strcpy(str, "Copy Attributes %t|Location%x1|Rotation%x2|Size%x3|Draw Options%x4|Time Offset%x5|Dupli%x6|Object Color%x31|%l|Mass%x7|Damping%x8|All Physical Attributes%x11|Properties%x9|Logic Bricks%x10|Protected Transform%x29|%l");
	
	strcat (str, "|Object Constraints%x22");
	strcat (str, "|NLA Strips%x26");
	
// XXX	if (OB_SUPPORT_MATERIAL(ob)) {
//		strcat(str, "|Texture Space%x17");
//	}	
	
	if(ob->type == OB_FONT) strcat(str, "|Font Settings%x18|Bevel Settings%x19");
	if(ob->type == OB_CURVE) strcat(str, "|Bevel Settings%x19|UV Orco%x28");
	
	if((ob->type == OB_FONT) || (ob->type == OB_CURVE)) {
			strcat(str, "|Curve Resolution%x25");
	}

	if(ob->type==OB_MESH){
		strcat(str, "|Subsurf Settings%x21|AutoSmooth%x27");
	}

	if(ob->soft) strcat(str, "|Soft Body Settings%x23");
	
	strcat(str, "|Pass Index%x30");
	
	if(ob->type==OB_MESH || ob->type==OB_CURVE || ob->type==OB_LATTICE || ob->type==OB_SURF){
		strcat(str, "|Modifiers ...%x24");
	}

	event= pupmenu(str);
	if(event<= 0) return;
	
	copy_attr(scene, v3d, event);
}


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


void make_links(Scene *scene, View3D *v3d, short event)
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
				error("This is the current scene");
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

void make_links_menu(Scene *scene, View3D *v3d)
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
	
	make_links(scene, v3d, event);
}

static void apply_objects_internal(Scene *scene, View3D *v3d, int apply_scale, int apply_rot )
{
	Base *base, *basact;
	Object *ob;
	bArmature *arm;
	Mesh *me;
	Curve *cu;
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	MVert *mvert;
	float mat[3][3];
	int a, change = 0;
	
	if (!apply_scale && !apply_rot) {
		/* do nothing? */
		error("Nothing to do!");
		return;
	}
	/* first check if we can execute */
	for (base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			ob= base->object;
			if(ob->type==OB_MESH) {
				me= ob->data;
				
				if(me->id.us>1) {
					error("Can't apply to a multi user mesh, doing nothing.");
					return;
				}
			}
			else if (ob->type==OB_ARMATURE) {
				arm= ob->data;
				
				if(arm->id.us>1) {
					error("Can't apply to a multi user armature, doing nothing.");
					return;
				}
			}
			else if(ELEM(ob->type, OB_CURVE, OB_SURF)) {
				cu= ob->data;
				
				if(cu->id.us>1) {
					error("Can't apply to a multi user curve, doing nothing.");
					return;
				}
				if(cu->key) {
					error("Can't apply to a curve with vertex keys, doing nothing.");
					return;
				}
			}
		}
	}
	
	/* now execute */
	basact= BASACT;
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			ob= base->object;
			
			if(ob->type==OB_MESH) {
				/* calculate matrix */
				if (apply_scale && apply_rot)
					object_to_mat3(ob, mat);
				else if (apply_scale)
					object_scale_to_mat3(ob, mat);
				else
					object_rot_to_mat3(ob, mat);
				
				/* get object data */
				me= ob->data;
				
				/* adjust data */
				mvert= me->mvert;
				for(a=0; a<me->totvert; a++, mvert++) {
					Mat3MulVecfl(mat, mvert->co);
				}
				
				if (me->key) {
					KeyBlock *kb;
					
					for (kb=me->key->block.first; kb; kb=kb->next) {
						float *fp= kb->data;
						
						for (a=0; a<kb->totelem; a++, fp+=3)
							Mat3MulVecfl(mat, fp);
					}
				}
				
				/* adjust transforms */
				if (apply_scale)
					ob->size[0]= ob->size[1]= ob->size[2]= 1.0f;
				if (apply_rot)
					ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0f;
				/*QuatOne(ob->quat);*/ /* Quats arnt used yet */
				
				where_is_object(scene, ob);
				
				/* texspace and normals */
				BASACT= base;
// XXX				ED_object_enter_editmode(C, 0);
// XXX				ED_object_exit_editmode(C, EM_FREEDATA|EM_WAITCURSOR); /* freedata, but no undo */
				BASACT= basact;
				
				change = 1;
			}
			else if (ob->type==OB_ARMATURE) {
				if (apply_scale && apply_rot)
					object_to_mat3(ob, mat);
				else if (apply_scale)
					object_scale_to_mat3(ob, mat);
				else
					object_rot_to_mat3(ob, mat);
				arm= ob->data;
				
				/* see checks above */
// XXX				apply_rot_armature(ob, mat);
				
				/* Reset the object's transforms */
				if (apply_scale)
					ob->size[0]= ob->size[1]= ob->size[2]= 1.0;
				if (apply_rot)
					ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0;
				/*QuatOne(ob->quat); (not used anymore)*/
				
				where_is_object(scene, ob);
				
				change = 1;
			}
			else if(ELEM(ob->type, OB_CURVE, OB_SURF)) {
				float scale;
				if (apply_scale && apply_rot)
					object_to_mat3(ob, mat);
				else if (apply_scale)
					object_scale_to_mat3(ob, mat);
				else
					object_rot_to_mat3(ob, mat);
				scale = Mat3ToScalef(mat);
				cu= ob->data;
				
				/* see checks above */
				
				nu= cu->nurb.first;
				while(nu) {
					if( (nu->type & 7)==CU_BEZIER) {
						a= nu->pntsu;
						bezt= nu->bezt;
						while(a--) {
							Mat3MulVecfl(mat, bezt->vec[0]);
							Mat3MulVecfl(mat, bezt->vec[1]);
							Mat3MulVecfl(mat, bezt->vec[2]);
							bezt->radius *= scale;
							bezt++;
						}
					}
					else {
						a= nu->pntsu*nu->pntsv;
						bp= nu->bp;
						while(a--) {
							Mat3MulVecfl(mat, bp->vec);
							bp++;
						}
					}
					nu= nu->next;
				}
				if (apply_scale)
					ob->size[0]= ob->size[1]= ob->size[2]= 1.0;
				if (apply_rot)
					ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0;
				/*QuatOne(ob->quat); (quats arnt used anymore)*/
				
				where_is_object(scene, ob);
				
				/* texspace and normals */
				BASACT= base;
// XXX				ED_object_enter_editmode(C, 0);
// XXX				ED_object_exit_editmode(C, EM_FREEDATA|EM_WAITCURSOR); /* freedata, but no undo */
				BASACT= basact;
				
				change = 1;
			} else {
				continue;
			}
			
			ignore_parent_tx(scene, ob);
		}
	}
	if (change) {
	}
}

void apply_objects_locrot(Scene *scene, View3D *v3d)
{
	apply_objects_internal(scene, v3d, 1, 1);
}

void apply_objects_scale(Scene *scene, View3D *v3d)
{
	apply_objects_internal(scene, v3d, 1, 0);
}

void apply_objects_rot(Scene *scene, View3D *v3d)
{
	apply_objects_internal(scene, v3d, 0, 1);
}

void apply_objects_visual_tx( Scene *scene, View3D *v3d )
{
	Base *base;
	Object *ob;
	int change = 0;
	
	for (base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			ob= base->object;
			where_is_object(scene, ob);
			VECCOPY(ob->loc, ob->obmat[3]);
			Mat4ToSize(ob->obmat, ob->size);
			Mat4ToEul(ob->obmat, ob->rot);
			
			where_is_object(scene, ob);
			
			change = 1;
		}
	}
	if (change) {
	}
}

/* ************************************** */


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
					printf("ERROR single_obdata_users: %s\n", id->name);
					error("Read console");
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
	Material *ma;
	Lamp *la;
	World *wo;
	int b;
		
	ma= G.main->mat.first;
	while(ma) {
		if(ma->id.flag & LIB_NEW) {
			for(b=0; b<MAX_MTEX; b++) {
				if(ma->mtex[b] && ma->mtex[b]->tex) {
					do_single_tex_user( &(ma->mtex[b]->tex) );
				}
			}
		}
		ma= ma->id.next;
	}

	la= G.main->lamp.first;
	while(la) {
		if(la->id.flag & LIB_NEW) {
			for(b=0; b<MAX_MTEX; b++) {
				if(la->mtex[b] && la->mtex[b]->tex) {
					do_single_tex_user( &(la->mtex[b]->tex) );
				}
			}
		}
		la= la->id.next;
	}
	wo= G.main->world.first;
	while(wo) {
		if(wo->id.flag & LIB_NEW) {
			for(b=0; b<MAX_MTEX; b++) {
				if(wo->mtex[b] && wo->mtex[b]->tex) {
					do_single_tex_user( &(wo->mtex[b]->tex) );
				}
			}
		}
		wo= wo->id.next;
	}
}

void single_mat_users_expand(void)
{
	/* only when 'parent' blocks are LIB_NEW */

	Object *ob;
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	Material *ma;
	int a;
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->id.flag & LIB_NEW) {
			new_id_matar(ob->mat, ob->totcol);
		}
		ob= ob->id.next;
	}

	me= G.main->mesh.first;
	while(me) {
		if(me->id.flag & LIB_NEW) {
			new_id_matar(me->mat, me->totcol);
		}
		me= me->id.next;
	}

	cu= G.main->curve.first;
	while(cu) {
		if(cu->id.flag & LIB_NEW) {
			new_id_matar(cu->mat, cu->totcol);
		}
		cu= cu->id.next;
	}

	mb= G.main->mball.first;
	while(mb) {
		if(mb->id.flag & LIB_NEW) {
			new_id_matar(mb->mat, mb->totcol);
		}
		mb= mb->id.next;
	}

	/* material imats  */
	ma= G.main->mat.first;
	while(ma) {
		if(ma->id.flag & LIB_NEW) {
			for(a=0; a<MAX_MTEX; a++) {
				if(ma->mtex[a]) {
					ID_NEW(ma->mtex[a]->object);
				}
			}
		}
		ma= ma->id.next;
	}
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

/* ************************************************************* */

/* helper for below, ma was checked to be not NULL */
static void make_local_makelocalmaterial(Material *ma)
{
	//ID *id;
	int b;
	
	make_local_material(ma);
	
	for(b=0; b<MAX_MTEX; b++) {
		if(ma->mtex[b] && ma->mtex[b]->tex) {
			make_local_texture(ma->mtex[b]->tex);
		}
	}
	
#if 0 // XXX old animation system
	id= (ID *)ma->ipo;
	if(id && id->lib) make_local_ipo(ma->ipo);
#endif // XXX old animation system	
	
	/* nodetree? XXX */
}

void make_local(Scene *scene, View3D *v3d, int mode)
{
	Base *base;
	Object *ob;
	//bActionStrip *strip;
	ParticleSystem *psys;
	Material *ma, ***matarar;
	Lamp *la;
	Curve *cu;
	ID *id;
	int a, b;
	
	/* WATCH: the function new_id(..) re-inserts the id block!!! */
	if(scene->id.lib) return;
	
	if(mode==3) {
		all_local(NULL, 0);	/* NULL is all libs */
		return;
	}
	else if(mode<1) return;

	clear_id_newpoins();
	
	for(base= FIRSTBASE; base; base= base->next) {
		if( TESTBASE(v3d, base) ) {
			ob= base->object;
			if(ob->id.lib) {
				make_local_object(ob);
			}
		}
	}
	
	/* maybe object pointers */
	for(base= FIRSTBASE; base; base= base->next) {
		if( TESTBASE(v3d, base) ) {
			ob= base->object;
			if(ob->id.lib==NULL) {
				ID_NEW(ob->parent);
				ID_NEW(ob->track);
			}
		}
	}

	for(base= FIRSTBASE; base; base= base->next) {
		if( TESTBASE(v3d, base) ) {
			ob= base->object;
			id= ob->data;
			
			if(id && mode>1) {
				
				switch(ob->type) {
				case OB_LAMP:
					make_local_lamp((Lamp *)id);
					
					la= ob->data;
#if 0 // XXX old animation system
					id= (ID *)la->ipo;
					if(id && id->lib) make_local_ipo(la->ipo);
#endif // XXX old animation system
					break;
				case OB_CAMERA:
					make_local_camera((Camera *)id);
					break;
				case OB_MESH:
					make_local_mesh((Mesh *)id);
					make_local_key( ((Mesh *)id)->key );
					break;
				case OB_MBALL:
					make_local_mball((MetaBall *)id);
					break;
				case OB_CURVE:
				case OB_SURF:
				case OB_FONT:
					cu= (Curve *)id;
					make_local_curve(cu);
#if 0 // XXX old animation system
					id= (ID *)cu->ipo;
					if(id && id->lib) make_local_ipo(cu->ipo);
#endif // XXX old animation system
					make_local_key( cu->key );
					break;
				case OB_LATTICE:
					make_local_lattice((Lattice *)id);
					make_local_key( ((Lattice *)id)->key );
					break;
				case OB_ARMATURE:
					make_local_armature ((bArmature *)id);
					break;
				}

				for(psys=ob->particlesystem.first; psys; psys=psys->next)
					make_local_particlesettings(psys->part);
			}
			
#if 0 // XXX old animation system
			id= (ID *)ob->ipo;
			if(id && id->lib) make_local_ipo(ob->ipo);

			id= (ID *)ob->action;
			if(id && id->lib) make_local_action(ob->action);
			
			for(strip=ob->nlastrips.first; strip; strip=strip->next) {
				if(strip->act && strip->act->id.lib)
					make_local_action(strip->act);
			}
#endif // XXX old animation system
		}
	}

	if(mode>1) {
		for(base= FIRSTBASE; base; base= base->next) {
			if( TESTBASE(v3d, base) ) {
				ob= base->object;
				if(ob->type==OB_LAMP) {
					la= ob->data;
					for(b=0; b<MAX_MTEX; b++) {
						if(la->mtex[b] && la->mtex[b]->tex) {
							make_local_texture(la->mtex[b]->tex);
						}
					}
				}
				else {
					
					for(a=0; a<ob->totcol; a++) {
						ma= ob->mat[a];
						if(ma)
							make_local_makelocalmaterial(ma);
					}
					
					matarar= (Material ***)give_matarar(ob);
					if (matarar) {
						for(a=0; a<ob->totcol; a++) {
							ma= (*matarar)[a];
							if(ma)
								make_local_makelocalmaterial(ma);
						}
					}
				}
			}
		}
	}

}

void make_local_menu(Scene *scene, View3D *v3d)
{
	int mode;
	
	/* If you modify this menu, please remember to update view3d_edit_object_makelocalmenu
	* in header_view3d.c and the menu in toolbox.c
	*/
	
	if(scene->id.lib) return;
	
	mode = pupmenu("Make Local%t|Selected Objects %x1|Selected Objects and Data %x2|All %x3");
	
	if (mode <= 0) return;
	
	make_local(scene, v3d, mode);
}

/* ************************ ADD DUPLICATE ******************** */

/* 
	dupflag: a flag made from constants declared in DNA_userdef_types.h
	The flag tells adduplicate() weather to copy data linked to the object, or to reference the existing data.
	U.dupflag for default operations or you can construct a flag as python does
	if the dupflag is 0 then no data will be copied (linked duplicate) */

/* used below, assumes id.new is correct */
/* leaves selection of base/object unaltered */
static Base *object_add_duplicate_internal(Scene *scene, Base *base, int dupflag)
{
	Base *basen= NULL;
	Material ***matarar;
	Object *ob, *obn;
	ID *id;
	int a, didit;

	ob= base->object;
	if(ob->mode & OB_MODE_POSE) {
		; /* nothing? */
	}
	else {
		obn= copy_object(ob);
		obn->recalc |= OB_RECALC;
		
		basen= MEM_mallocN(sizeof(Base), "duplibase");
		*basen= *base;
		BLI_addhead(&scene->base, basen);	/* addhead: prevent eternal loop */
		basen->object= obn;
		
		if(basen->flag & OB_FROMGROUP) {
			Group *group;
			for(group= G.main->group.first; group; group= group->id.next) {
				if(object_in_group(ob, group))
					add_to_group(group, obn);
			}
			obn->flag |= OB_FROMGROUP; /* this flag is unset with copy_object() */
		}
		
		/* duplicates using userflags */
#if 0 // XXX old animation system				
		if(dupflag & USER_DUP_IPO) {
			bConstraintChannel *chan;
			id= (ID *)obn->ipo;
			
			if(id) {
				ID_NEW_US( obn->ipo)
				else obn->ipo= copy_ipo(obn->ipo);
				id->us--;
			}
			/* Handle constraint ipos */
			for (chan=obn->constraintChannels.first; chan; chan=chan->next){
				id= (ID *)chan->ipo;
				if(id) {
					ID_NEW_US( chan->ipo)
					else chan->ipo= copy_ipo(chan->ipo);
					id->us--;
				}
			}
		}
		if(dupflag & USER_DUP_ACT){ /* Not buttons in the UI to modify this, add later? */
			id= (ID *)obn->action;
			if (id){
				ID_NEW_US(obn->action)
				else{
					obn->action= copy_action(obn->action);
				}
				id->us--;
			}
		}
#endif // XXX old animation system
		if(dupflag & USER_DUP_MAT) {
			for(a=0; a<obn->totcol; a++) {
				id= (ID *)obn->mat[a];
				if(id) {
					ID_NEW_US(obn->mat[a])
					else obn->mat[a]= copy_material(obn->mat[a]);
					id->us--;
				}
			}
		}
		
		id= obn->data;
		didit= 0;
		
		switch(obn->type) {
			case OB_MESH:
				if(dupflag & USER_DUP_MESH) {
					ID_NEW_US2( obn->data )
					else {
						obn->data= copy_mesh(obn->data);
						
						if(obn->fluidsimSettings) {
							obn->fluidsimSettings->orgMesh = (Mesh *)obn->data;
						}
						
						didit= 1;
					}
					id->us--;
				}
				break;
			case OB_CURVE:
				if(dupflag & USER_DUP_CURVE) {
					ID_NEW_US2(obn->data )
					else {
						obn->data= copy_curve(obn->data);
						didit= 1;
					}
					id->us--;
				}
				break;
			case OB_SURF:
				if(dupflag & USER_DUP_SURF) {
					ID_NEW_US2( obn->data )
					else {
						obn->data= copy_curve(obn->data);
						didit= 1;
					}
					id->us--;
				}
				break;
			case OB_FONT:
				if(dupflag & USER_DUP_FONT) {
					ID_NEW_US2( obn->data )
					else {
						obn->data= copy_curve(obn->data);
						didit= 1;
					}
					id->us--;
				}
				break;
			case OB_MBALL:
				if(dupflag & USER_DUP_MBALL) {
					ID_NEW_US2(obn->data )
					else {
						obn->data= copy_mball(obn->data);
						didit= 1;
					}
					id->us--;
				}
				break;
			case OB_LAMP:
				if(dupflag & USER_DUP_LAMP) {
					ID_NEW_US2(obn->data )
					else obn->data= copy_lamp(obn->data);
					id->us--;
				}
				break;
				
			case OB_ARMATURE:
				obn->recalc |= OB_RECALC_DATA;
				if(obn->pose) obn->pose->flag |= POSE_RECALC;
					
					if(dupflag & USER_DUP_ARM) {
						ID_NEW_US2(obn->data )
						else {
							obn->data= copy_armature(obn->data);
							armature_rebuild_pose(obn, obn->data);
							didit= 1;
						}
						id->us--;
					}
						
						break;
				
			case OB_LATTICE:
				if(dupflag!=0) {
					ID_NEW_US2(obn->data )
					else obn->data= copy_lattice(obn->data);
					id->us--;
				}
				break;
			case OB_CAMERA:
				if(dupflag!=0) {
					ID_NEW_US2(obn->data )
					else obn->data= copy_camera(obn->data);
					id->us--;
				}
				break;
		}
		
		if(dupflag & USER_DUP_MAT) {
			matarar= give_matarar(obn);
			if(didit && matarar) {
				for(a=0; a<obn->totcol; a++) {
					id= (ID *)(*matarar)[a];
					if(id) {
						ID_NEW_US( (*matarar)[a] )
						else (*matarar)[a]= copy_material((*matarar)[a]);
						
						id->us--;
					}
				}
			}
		}
	}
	return basen;
}

/* single object duplicate, if dupflag==0, fully linked, else it uses the flags given */
/* leaves selection of base/object unaltered */
Base *ED_object_add_duplicate(Scene *scene, Base *base, int dupflag)
{
	Base *basen;

	clear_id_newpoins();
	clear_sca_new_poins();	/* sensor/contr/act */
	
	basen= object_add_duplicate_internal(scene, base, dupflag);
	
	DAG_scene_sort(scene);
	
	return basen;
}

/* contextual operator dupli */
static int duplicate_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	int linked= RNA_boolean_get(op->ptr, "linked");
	int dupflag= (linked)? 0: U.dupflag;
	
	clear_id_newpoins();
	clear_sca_new_poins();	/* sensor/contr/act */
	
	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		Base *basen= object_add_duplicate_internal(scene, base, dupflag);
		
		/* XXX context conflict maybe, itterator could solve this? */
		ED_base_object_select(base, BA_DESELECT);
		/* new object becomes active */
		if(BASACT==base)
			ED_base_object_activate(C, basen);
		
	}
	CTX_DATA_END;

	/* XXX fix this for context */
	copy_object_set_idnew(scene, v3d, dupflag);

	DAG_scene_sort(scene);
	ED_anim_dag_flush_update(C);	

	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);

	return OPERATOR_FINISHED;
}

static int duplicate_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	duplicate_exec(C, op);
	
//	RNA_int_set(op->ptr, "mode", TFM_TRANSLATION);
//	WM_operator_name_call(C, "TFM_OT_transform", WM_OP_INVOKE_REGION_WIN, op->ptr);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_duplicate(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Duplicate";
	ot->description = "Duplicate selected objects.";
	ot->idname= "OBJECT_OT_duplicate";
	
	/* api callbacks */
	ot->invoke= duplicate_invoke;
	ot->exec= duplicate_exec;
	
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* to give to transform */
	RNA_def_boolean(ot->srna, "linked", 0, "Linked", "Duplicate object but not object data, linking to the original data.");
	RNA_def_int(ot->srna, "mode", TFM_TRANSLATION, 0, INT_MAX, "Mode", "", 0, INT_MAX);
}

/* ************************** JOIN *********************** */

static int join_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);

	if(scene->obedit) {
		BKE_report(op->reports, RPT_ERROR, "This data does not support joining in editmode.");
		return OPERATOR_CANCELLED;
	}
	else if(!ob) {
		BKE_report(op->reports, RPT_ERROR, "Can't join unless there is an active object.");
		return OPERATOR_CANCELLED;
	}
	else if(object_data_is_libdata(ob)) {
		BKE_report(op->reports, RPT_ERROR, "Can't edit external libdata.");
		return OPERATOR_CANCELLED;
	}

	if(ob->type == OB_MESH)
		return join_mesh_exec(C, op);
	else if(ELEM(ob->type, OB_CURVE, OB_SURF))
		return join_curve_exec(C, op);
	else if(ob->type == OB_ARMATURE)
		return join_armature_exec(C, op);

	BKE_report(op->reports, RPT_ERROR, "This object type doesn't support joining.");

	return OPERATOR_CANCELLED;
}

void OBJECT_OT_join(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Join";
	ot->description = "Join selected objects into active object.";
	ot->idname= "OBJECT_OT_join";
	
	/* api callbacks */
	ot->exec= join_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** Smooth/Flat *********************/

static int shade_smooth_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	Curve *cu;
	Nurb *nu;
	int clear= (strcmp(op->idname, "OBJECT_OT_shade_flat") == 0);
	int done= 0;

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		ob= base->object;

		if(ob->type==OB_MESH) {
			mesh_set_smooth_flag(ob, !clear);

			DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

			done= 1;
		}
		else if ELEM(ob->type, OB_SURF, OB_CURVE) {
			cu= ob->data;

			for(nu=cu->nurb.first; nu; nu=nu->next) {
				if(!clear) nu->flag |= ME_SMOOTH;
				else nu->flag &= ~ME_SMOOTH;
				nu= nu->next;
			}

			DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

			done= 1;
		}
	}
	CTX_DATA_END;

	return (done)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void OBJECT_OT_shade_flat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Shade Flat";
	ot->idname= "OBJECT_OT_shade_flat";
	
	/* api callbacks */
	ot->exec= shade_smooth_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void OBJECT_OT_shade_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Shade Smooth";
	ot->idname= "OBJECT_OT_shade_smooth";
	
	/* api callbacks */
	ot->exec= shade_smooth_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}



/* ********************** */

void image_aspect(Scene *scene, View3D *v3d)
{
	/* all selected objects with an image map: scale in image aspect */
	Base *base;
	Object *ob;
	Material *ma;
	Tex *tex;
	float x, y, space;
	int a, b, done;
	
	if(scene->obedit) return; // XXX get from context
	if(scene->id.lib) return;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			ob= base->object;
			done= 0;
			
			for(a=1; a<=ob->totcol; a++) {
				ma= give_current_material(ob, a);
				if(ma) {
					for(b=0; b<MAX_MTEX; b++) {
						if(ma->mtex[b] && ma->mtex[b]->tex) {
							tex= ma->mtex[b]->tex;
							if(tex->type==TEX_IMAGE && tex->ima) {
								ImBuf *ibuf= BKE_image_get_ibuf(tex->ima, NULL);
								
								/* texturespace */
								space= 1.0;
								if(ob->type==OB_MESH) {
									float size[3];
									mesh_get_texspace(ob->data, NULL, NULL, size);
									space= size[0]/size[1];
								}
								else if(ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
									Curve *cu= ob->data;
									space= cu->size[0]/cu->size[1];
								}
							
								x= ibuf->x/space;
								y= ibuf->y;
								
								if(x>y) ob->size[0]= ob->size[1]*x/y;
								else ob->size[1]= ob->size[0]*y/x;
								
								done= 1;
								DAG_object_flush_update(scene, ob, OB_RECALC_OB);								
							}
						}
						if(done) break;
					}
				}
				if(done) break;
			}
		}
	}
	
}

void set_ob_ipoflags(Scene *scene, View3D *v3d)
{
#if 0 // XXX old animation system
	Base *base;
	int set= 1;
	
	if (!v3d) {
		error("Can't do this! Open a 3D window");
		return;
	}
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			if(base->object->ipoflag & OB_DRAWKEY) {
				set= 0;
				break;
			}
		}
	}
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			if(set) {
				base->object->ipoflag |= OB_DRAWKEY;
				if(base->object->ipo) base->object->ipo->showkey= 1;
			}
			else {
				base->object->ipoflag &= ~OB_DRAWKEY;
				if(base->object->ipo) base->object->ipo->showkey= 0;
			}
		}
	}
#endif // XXX old animation system
}


void select_select_keys(Scene *scene, View3D *v3d)
{
#if 0 // XXX old animation system
	Base *base;
	IpoCurve *icu;
	BezTriple *bezt;
	int a;
	
	if (!v3d) {
		error("Can't do this! Open a 3D window");
		return;
	}
	
	if(scene->id.lib) return;

	if(okee("Show and select all keys")==0) return;

	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			if(base->object->ipo) {
				base->object->ipoflag |= OB_DRAWKEY;
				base->object->ipo->showkey= 1;
				icu= base->object->ipo->curve.first;
				while(icu) {
					a= icu->totvert;
					bezt= icu->bezt;
					while(a--) {
						bezt->f1 |= SELECT;
						bezt->f2 |= SELECT;
						bezt->f3 |= SELECT;
						bezt++;
					}
					icu= icu->next;
				}
			}
		}
	}


#endif  // XXX old animation system
}


int vergbaseco(const void *a1, const void *a2)
{
	Base **x1, **x2;
	
	x1= (Base **) a1;
	x2= (Base **) a2;
	
	if( (*x1)->sy > (*x2)->sy ) return 1;
	else if( (*x1)->sy < (*x2)->sy) return -1;
	else if( (*x1)->sx > (*x2)->sx ) return 1;
	else if( (*x1)->sx < (*x2)->sx ) return -1;

	return 0;
}


void auto_timeoffs(Scene *scene, View3D *v3d)
{
	Base *base, **basesort, **bs;
	float start, delta;
	int tot=0, a;
	short offset=25;

	if(BASACT==0 || v3d==NULL) return;
// XXX	if(button(&offset, 0, 1000,"Total time")==0) return;

	/* make array of all bases, xco yco (screen) */
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			tot++;
		}
	}

	delta= (float)offset/(float)tot;
	start= OBACT->sf;

	bs= basesort= MEM_mallocN(sizeof(void *)*tot,"autotimeoffs");
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			*bs= base;
			bs++;
		}
	}
	qsort(basesort, tot, sizeof(void *), vergbaseco);

	bs= basesort;
	for(a=0; a<tot; a++) {
		
		(*bs)->object->sf= start;
		start+= delta;

		bs++;
	}
	MEM_freeN(basesort);

}

void ofs_timeoffs(Scene *scene, View3D *v3d)
{
	Base *base;
	float offset=0.0f;

	if(BASACT==0 || v3d==NULL) return;
	
// XXX	if(fbutton(&offset, -10000.0f, 10000.0f, 10, 10, "Offset")==0) return;

	/* make array of all bases, xco yco (screen) */
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			base->object->sf += offset;
			if (base->object->sf < -MAXFRAMEF)		base->object->sf = -MAXFRAMEF;
			else if (base->object->sf > MAXFRAMEF)	base->object->sf = MAXFRAMEF;
		}
	}

}


void rand_timeoffs(Scene *scene, View3D *v3d)
{
	Base *base;
	float rand=0.0f;

	if(BASACT==0 || v3d==NULL) return;
	
// XXX	if(fbutton(&rand, 0.0f, 10000.0f, 10, 10, "Randomize")==0) return;
	
	rand *= 2;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			base->object->sf += (BLI_drand()-0.5) * rand;
			if (base->object->sf < -MAXFRAMEF)		base->object->sf = -MAXFRAMEF;
			else if (base->object->sf > MAXFRAMEF)	base->object->sf = MAXFRAMEF;
		}
	}

}


void texspace_edit(Scene *scene, View3D *v3d)
{
	Base *base;
	int nr=0;
	
	/* first test if from visible and selected objects
	 * texspacedraw is set:
	 */
	
	if(scene->obedit) return; // XXX get from context
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			break;
		}
	}

	if(base==0) {
		return;
	}
	
	nr= pupmenu("Texture Space %t|Grab/Move%x1|Size%x2");
	if(nr<1) return;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			base->object->dtx |= OB_TEXSPACE;
		}
	}
	

	if(nr==1) {
// XXX		initTransform(TFM_TRANSLATION, CTX_TEXTURE);
// XXX		Transform();
	}
	else if(nr==2) {
// XXX		initTransform(TFM_RESIZE, CTX_TEXTURE);
// XXX		Transform();
	}
	else if(nr==3) {
// XXX		initTransform(TFM_ROTATION, CTX_TEXTURE);
// XXX		Transform();
	}
}

/* ******************************************************************** */
/* Mirror function in Edit Mode */

void mirrormenu(void)
{
// XXX		initTransform(TFM_MIRROR, CTX_NO_PET);
// XXX		Transform();
}

void hookmenu(Scene *scene, View3D *v3d)
{
	/* only called in object mode */
	short event, changed=0;
	Object *ob;
	Base *base;
	ModifierData *md;
	HookModifierData *hmd;
	
	event= pupmenu("Modify Hooks for Selected...%t|Reset Offset%x1|Recenter at Cursor%x2");
	if (event==-1) return;
	if (event==2 && !(v3d)) {
		error("Cannot perform this operation without a 3d view");
		return;
	}
	
	for (base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			for (md = base->object->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Hook) {
					ob = base->object;
					hmd = (HookModifierData*) md;
					
					/*
					 * Copied from modifiers_cursorHookCenter and
					 * modifiers_clearHookOffset, should consolidate
					 * */
					
					if (event==1) {
						if(hmd->object) {
							Mat4Invert(hmd->object->imat, hmd->object->obmat);
							Mat4MulSerie(hmd->parentinv, hmd->object->imat, ob->obmat, NULL, NULL, NULL, NULL, NULL, NULL);
							
							changed= 1;
							DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
						}
					} else {
						float *curs = give_cursor(scene, v3d);
						float bmat[3][3], imat[3][3];
						
						where_is_object(scene, ob);
					
						Mat3CpyMat4(bmat, ob->obmat);
						Mat3Inv(imat, bmat);
				
						curs= give_cursor(scene, v3d);
						hmd->cent[0]= curs[0]-ob->obmat[3][0];
						hmd->cent[1]= curs[1]-ob->obmat[3][1];
						hmd->cent[2]= curs[2]-ob->obmat[3][2];
						Mat3MulVecfl(imat, hmd->cent);
						
						changed= 1;
						DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
					} 
				}
			}
		}
	}
	
	if (changed) {
	}	
}

static EnumPropertyItem *object_mode_set_itemsf(bContext *C, PointerRNA *ptr, int *free)
{	
	EnumPropertyItem *input = object_mode_items;
	EnumPropertyItem *item= NULL;
	Object *ob;
	int totitem= 0;
	
	if(!C) /* needed for docs */
		return object_mode_items;

	ob = CTX_data_active_object(C);
	while(ob && input->identifier) {
		if((input->value == OB_MODE_EDIT && ((ob->type == OB_MESH) || (ob->type == OB_ARMATURE) ||
						    (ob->type == OB_CURVE) || (ob->type == OB_SURF) ||
						     (ob->type == OB_FONT) || (ob->type == OB_MBALL) || (ob->type == OB_LATTICE))) ||
		   (input->value == OB_MODE_POSE && (ob->type == OB_ARMATURE)) ||
		   (input->value == OB_MODE_PARTICLE_EDIT && ob->particlesystem.first) ||
		   ((input->value == OB_MODE_SCULPT || input->value == OB_MODE_VERTEX_PAINT ||
		     input->value == OB_MODE_WEIGHT_PAINT || input->value == OB_MODE_TEXTURE_PAINT) && (ob->type == OB_MESH)) ||
		   (input->value == OB_MODE_OBJECT))
			RNA_enum_item_add(&item, &totitem, input);
		++input;
	}

	RNA_enum_item_end(&item, &totitem);

	*free= 1;

	return item;
}

static int object_mode_set_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	int mode = RNA_enum_get(op->ptr, "mode");

	if(!ob)
		return OPERATOR_CANCELLED;

	if((mode == OB_MODE_EDIT) == !(ob->mode & OB_MODE_EDIT))
		WM_operator_name_call(C, "OBJECT_OT_editmode_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if((mode == OB_MODE_SCULPT) == !(ob->mode & OB_MODE_SCULPT))
		WM_operator_name_call(C, "SCULPT_OT_sculptmode_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if((mode == OB_MODE_VERTEX_PAINT) == !(ob->mode & OB_MODE_VERTEX_PAINT))
		WM_operator_name_call(C, "PAINT_OT_vertex_paint_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if((mode == OB_MODE_WEIGHT_PAINT) == !(ob->mode & OB_MODE_WEIGHT_PAINT))
		WM_operator_name_call(C, "PAINT_OT_weight_paint_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if((mode == OB_MODE_TEXTURE_PAINT) == !(ob->mode & OB_MODE_TEXTURE_PAINT))
		WM_operator_name_call(C, "PAINT_OT_texture_paint_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if((mode == OB_MODE_PARTICLE_EDIT) == !(ob->mode & OB_MODE_PARTICLE_EDIT))
		WM_operator_name_call(C, "PARTICLE_OT_particle_edit_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if((mode == OB_MODE_POSE) == !(ob->mode & OB_MODE_POSE))
		WM_operator_name_call(C, "OBJECT_OT_posemode_toggle", WM_OP_EXEC_REGION_WIN, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_mode_set(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Set Object Mode";
	ot->description = "Sets the object interaction mode.";
	ot->idname= "OBJECT_OT_mode_set";
	
	/* api callbacks */
	ot->exec= object_mode_set_exec;
	
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	prop= RNA_def_enum(ot->srna, "mode", object_mode_items, 0, "Mode", "");
	RNA_def_enum_funcs(prop, object_mode_set_itemsf);
}



void ED_object_toggle_modes(bContext *C, int mode)
{
	if(mode & OB_MODE_SCULPT)
		WM_operator_name_call(C, "SCULPT_OT_sculptmode_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if(mode & OB_MODE_VERTEX_PAINT)
		WM_operator_name_call(C, "PAINT_OT_vertex_paint_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if(mode & OB_MODE_WEIGHT_PAINT)
		WM_operator_name_call(C, "PAINT_OT_weight_paint_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if(mode & OB_MODE_TEXTURE_PAINT)
		WM_operator_name_call(C, "PAINT_OT_texture_paint_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if(mode & OB_MODE_PARTICLE_EDIT)
		WM_operator_name_call(C, "PARTICLE_OT_particle_edit_toggle", WM_OP_EXEC_REGION_WIN, NULL);
}
