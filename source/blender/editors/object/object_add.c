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

/** \file blender/editors/object/object_add.c
 *  \ingroup edobj
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_object_force.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"
#include "DNA_actuator_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BLF_translation.h"

#include "BKE_anim.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_group.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_report.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_speaker.h"
#include "BKE_texture.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_mball.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_object.h"
#include "ED_physics.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "GPU_material.h"

#include "object_intern.h"

/* this is an exact copy of the define in rna_lamp.c
 * kept here because of linking order.
 * Icons are only defined here */
EnumPropertyItem lamp_type_items[] = {
	{LA_LOCAL, "POINT", ICON_LAMP_POINT, "Point", "Omnidirectional point light source"},
	{LA_SUN, "SUN", ICON_LAMP_SUN, "Sun", "Constant direction parallel ray light source"},
	{LA_SPOT, "SPOT", ICON_LAMP_SPOT, "Spot", "Directional cone light source"},
	{LA_HEMI, "HEMI", ICON_LAMP_HEMI, "Hemi", "180 degree constant light source"},
	{LA_AREA, "AREA", ICON_LAMP_AREA, "Area", "Directional area light source"},
	{0, NULL, 0, NULL, NULL}
};

/* copy from rna_object_force.c */
static EnumPropertyItem field_type_items[] = {
	{PFIELD_FORCE, "FORCE", ICON_FORCE_FORCE, "Force", ""},
	{PFIELD_WIND, "WIND", ICON_FORCE_WIND, "Wind", ""},
	{PFIELD_VORTEX, "VORTEX", ICON_FORCE_VORTEX, "Vortex", ""},
	{PFIELD_MAGNET, "MAGNET", ICON_FORCE_MAGNETIC, "Magnetic", ""},
	{PFIELD_HARMONIC, "HARMONIC", ICON_FORCE_HARMONIC, "Harmonic", ""},
	{PFIELD_CHARGE, "CHARGE", ICON_FORCE_CHARGE, "Charge", ""},
	{PFIELD_LENNARDJ, "LENNARDJ", ICON_FORCE_LENNARDJONES, "Lennard-Jones", ""},
	{PFIELD_TEXTURE, "TEXTURE", ICON_FORCE_TEXTURE, "Texture", ""},
	{PFIELD_GUIDE, "GUIDE", ICON_FORCE_CURVE, "Curve Guide", ""},
	{PFIELD_BOID, "BOID", ICON_FORCE_BOID, "Boid", ""},
	{PFIELD_TURBULENCE, "TURBULENCE", ICON_FORCE_TURBULENCE, "Turbulence", ""},
	{PFIELD_DRAG, "DRAG", ICON_FORCE_DRAG, "Drag", ""},
	{PFIELD_SMOKEFLOW, "SMOKE", ICON_FORCE_SMOKEFLOW, "Smoke Flow", ""},
	{0, NULL, 0, NULL, NULL}
};

/************************** Exported *****************************/

void ED_object_location_from_view(bContext *C, float loc[3])
{
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);
	const float *cursor;

	cursor = ED_view3d_cursor3d_get(scene, v3d);

	copy_v3_v3(loc, cursor);
}

void ED_object_rotation_from_view(bContext *C, float rot[3], const char align_axis)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);

	BLI_assert(align_axis >= 'X' && align_axis <= 'Z');

	if (rv3d) {
		const float pi_2 = (float)M_PI / 2.0f;
		float quat[4];

		switch (align_axis) {
			case 'X':
			{
				float quat_y[4];
				axis_angle_to_quat(quat_y, rv3d->viewinv[1], -pi_2);
				mul_qt_qtqt(quat, rv3d->viewquat, quat_y);
				quat[0] = -quat[0];

				quat_to_eul(rot, quat);
				break;
			}
			case 'Y':
			{
				copy_qt_qt(quat, rv3d->viewquat);
				quat[0] = -quat[0];

				quat_to_eul(rot, quat);
				rot[0] -= pi_2;
				break;
			}
			case 'Z':
			{
				copy_qt_qt(quat, rv3d->viewquat);
				quat[0] = -quat[0];

				quat_to_eul(rot, quat);
				break;
			}
		}

	}
	else {
		zero_v3(rot);
	}
}

void ED_object_base_init_transform(bContext *C, Base *base, const float loc[3], const float rot[3])
{
	Object *ob = base->object;
	Scene *scene = CTX_data_scene(C);

	if (!scene) return;

	if (loc)
		copy_v3_v3(ob->loc, loc);

	if (rot)
		copy_v3_v3(ob->rot, rot);

	BKE_object_where_is_calc(scene, ob);
}

/* Uses context to figure out transform for primitive.
 * Returns standard diameter. */
float ED_object_new_primitive_matrix(bContext *C, Object *obedit,
                                     const float loc[3], const float rot[3], float primmat[4][4],
                                     bool apply_diameter)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	float mat[3][3], rmat[3][3], cmat[3][3], imat[3][3];

	unit_m4(primmat);

	eul_to_mat3(rmat, rot);
	invert_m3(rmat);

	/* inverse transform for initial rotation and object */
	copy_m3_m4(mat, obedit->obmat);
	mul_m3_m3m3(cmat, rmat, mat);
	invert_m3_m3(imat, cmat);
	copy_m4_m3(primmat, imat);

	/* center */
	copy_v3_v3(primmat[3], loc);
	sub_v3_v3v3(primmat[3], primmat[3], obedit->obmat[3]);
	invert_m3_m3(imat, mat);
	mul_m3_v3(imat, primmat[3]);

	{
		const float dia = v3d ? ED_view3d_grid_scale(scene, v3d, NULL) : ED_scene_grid_scale(scene, NULL);

		if (apply_diameter) {
			primmat[0][0] *= dia;
			primmat[1][1] *= dia;
			primmat[2][2] *= dia;
		}

		return dia;
	}

	// return 1.0f;
}

/********************* Add Object Operator ********************/

static void view_align_update(struct Main *UNUSED(main), struct Scene *UNUSED(scene), struct PointerRNA *ptr)
{
	RNA_struct_idprops_unset(ptr, "rotation");
}

void ED_object_add_unit_props(wmOperatorType *ot)
{
	PropertyRNA *prop;

	prop = RNA_def_float(ot->srna, "radius", 1.0f, 0.0, FLT_MAX, "Radius", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
}

void ED_object_add_generic_props(wmOperatorType *ot, bool do_editmode)
{
	PropertyRNA *prop;

	/* note: this property gets hidden for add-camera operator */
	prop = RNA_def_boolean(ot->srna, "view_align", 0, "Align to View", "Align the new object to the view");
	RNA_def_property_update_runtime(prop, view_align_update);

	if (do_editmode) {
		prop = RNA_def_boolean(ot->srna, "enter_editmode", 0, "Enter Editmode",
		                       "Enter editmode when adding this object");
		RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	}

	prop = RNA_def_float_vector_xyz(ot->srna, "location", 3, NULL, -FLT_MAX, FLT_MAX, "Location",
	                                "Location for the newly added object", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_float_rotation(ot->srna, "rotation", 3, NULL, -FLT_MAX, FLT_MAX, "Rotation",
	                              "Rotation for the newly added object", (float)-M_PI * 2.0f, (float)M_PI * 2.0f);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_boolean_layer_member(ot->srna, "layers", 20, NULL, "Layer", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

bool ED_object_add_generic_get_opts(bContext *C, wmOperator *op, const char view_align_axis,
                                    float loc[3], float rot[3],
                                    bool *enter_editmode, unsigned int *layer, bool *is_view_aligned)
{
	View3D *v3d = CTX_wm_view3d(C);
	unsigned int _layer;

	/* Switch to Edit mode? */
	if (RNA_struct_find_property(op->ptr, "enter_editmode")) { /* optional */
		bool _enter_editmode;
		if (!enter_editmode)
			enter_editmode = &_enter_editmode;

		if (RNA_struct_property_is_set(op->ptr, "enter_editmode") && enter_editmode)
			*enter_editmode = RNA_boolean_get(op->ptr, "enter_editmode");
		else {
			*enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
			RNA_boolean_set(op->ptr, "enter_editmode", *enter_editmode);
		}
	}

	/* Get layers! */
	{
		int a, layer_values[20];
		if (!layer)
			layer = &_layer;

		if (RNA_struct_property_is_set(op->ptr, "layers")) {
			RNA_boolean_get_array(op->ptr, "layers", layer_values);
			*layer = 0;
			for (a = 0; a < 20; a++) {
				if (layer_values[a])
					*layer |= (1 << a);
			}
		}
		else {
			Scene *scene = CTX_data_scene(C);
			*layer = BKE_screen_view3d_layer_active_ex(v3d, scene, false);
			for (a = 0; a < 20; a++) {
				layer_values[a] = *layer & (1 << a);
			}
			RNA_boolean_set_array(op->ptr, "layers", layer_values);
		}

		/* in local view we additionally add local view layers,
		 * not part of operator properties */
		if (v3d && v3d->localvd)
			*layer |= v3d->lay;
	}

	/* Location! */
	{
		float _loc[3];
		if (!loc)
			loc = _loc;

		if (RNA_struct_property_is_set(op->ptr, "location")) {
			RNA_float_get_array(op->ptr, "location", loc);
		}
		else {
			ED_object_location_from_view(C, loc);
			RNA_float_set_array(op->ptr, "location", loc);
		}
	}

	/* Rotation! */
	{
		bool _is_view_aligned;
		float _rot[3];
		if (!is_view_aligned)
			is_view_aligned = &_is_view_aligned;
		if (!rot)
			rot = _rot;

		if (RNA_struct_property_is_set(op->ptr, "rotation"))
			*is_view_aligned = false;
		else if (RNA_struct_property_is_set(op->ptr, "view_align"))
			*is_view_aligned = RNA_boolean_get(op->ptr, "view_align");
		else {
			*is_view_aligned = (U.flag & USER_ADD_VIEWALIGNED) != 0;
			RNA_boolean_set(op->ptr, "view_align", *is_view_aligned);
		}

		if (*is_view_aligned) {
			ED_object_rotation_from_view(C, rot, view_align_axis);
			RNA_float_set_array(op->ptr, "rotation", rot);
		}
		else
			RNA_float_get_array(op->ptr, "rotation", rot);
	}

	if (layer && *layer == 0) {
		BKE_report(op->reports, RPT_ERROR, "Property 'layer' has no values set");
		return false;
	}

	return true;
}

/* For object add primitive operators.
 * Do not call undo push in this function (users of this function have to). */
Object *ED_object_add_type(bContext *C, int type, const float loc[3], const float rot[3],
                           bool enter_editmode, unsigned int layer)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob;

	/* for as long scene has editmode... */
	if (CTX_data_edit_object(C)) 
		ED_object_editmode_exit(C, EM_FREEDATA | EM_FREEUNDO | EM_WAITCURSOR | EM_DO_UNDO);  /* freedata, and undo */

	/* deselects all, sets scene->basact */
	ob = BKE_object_add(bmain, scene, type);
	BASACT->lay = ob->lay = layer;
	/* editor level activate, notifiers */
	ED_base_object_activate(C, BASACT);

	/* more editor stuff */
	ED_object_base_init_transform(C, BASACT, loc, rot);

	/* Ignore collisions by default for non-mesh objects */
	if (type != OB_MESH) {
		ob->body_type = OB_BODY_TYPE_NO_COLLISION;
		ob->gameflag &= ~(OB_SENSOR | OB_RIGID_BODY | OB_SOFT_BODY | OB_COLLISION | OB_CHARACTER | OB_OCCLUDER | OB_DYNAMIC | OB_NAVMESH); /* copied from rna_object.c */
	}

	DAG_id_type_tag(bmain, ID_OB);
	DAG_relations_tag_update(bmain);
	if (ob->data) {
		ED_render_id_flush_update(bmain, ob->data);
	}

	if (enter_editmode)
		ED_object_editmode_enter(C, EM_IGNORE_LAYER);

	WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

	return ob;
}

/* for object add operator */
static int object_add_exec(bContext *C, wmOperator *op)
{
	Object *ob;
	bool enter_editmode;
	unsigned int layer;
	float loc[3], rot[3], radius;

	WM_operator_view3d_unit_defaults(C, op);
	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL))
		return OPERATOR_CANCELLED;

	radius = RNA_float_get(op->ptr, "radius");
	ob = ED_object_add_type(C, RNA_enum_get(op->ptr, "type"), loc, rot, enter_editmode, layer);

	if (ob->type == OB_LATTICE) {
		/* lattice is a special case!
		 * we never want to scale the obdata since that is the rest-state */
		copy_v3_fl(ob->size, radius);
	}
	else {
		BKE_object_obdata_size_init(ob, radius);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Object";
	ot->description = "Add an object to the scene";
	ot->idname = "OBJECT_OT_add";

	/* api callbacks */
	ot->exec = object_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ED_object_add_unit_props(ot);
	RNA_def_enum(ot->srna, "type", object_type_items, 0, "Type", "");

	ED_object_add_generic_props(ot, true);
}

/********************* Add Effector Operator ********************/

/* for object add operator */
static int effector_add_exec(bContext *C, wmOperator *op)
{
	Object *ob;
	int type;
	bool enter_editmode;
	unsigned int layer;
	float loc[3], rot[3];
	float mat[4][4];
	float dia;

	WM_operator_view3d_unit_defaults(C, op);
	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL))
		return OPERATOR_CANCELLED;

	type = RNA_enum_get(op->ptr, "type");
	dia = RNA_float_get(op->ptr, "radius");

	if (type == PFIELD_GUIDE) {
		Curve *cu;
		ob = ED_object_add_type(C, OB_CURVE, loc, rot, false, layer);

		rename_id(&ob->id, CTX_DATA_(BLF_I18NCONTEXT_ID_OBJECT, "CurveGuide"));
		cu = ob->data;
		cu->flag |= CU_PATH | CU_3D;
		ED_object_editmode_enter(C, 0);
		ED_object_new_primitive_matrix(C, ob, loc, rot, mat, false);
		BLI_addtail(&cu->editnurb->nurbs, add_nurbs_primitive(C, ob, mat, CU_NURBS | CU_PRIM_PATH, dia));
		if (!enter_editmode)
			ED_object_editmode_exit(C, EM_FREEDATA);
	}
	else {
		ob = ED_object_add_type(C, OB_EMPTY, loc, rot, false, layer);
		BKE_object_obdata_size_init(ob, dia);
		rename_id(&ob->id, CTX_DATA_(BLF_I18NCONTEXT_ID_OBJECT, "Field"));
		if (ELEM(type, PFIELD_WIND, PFIELD_VORTEX))
			ob->empty_drawtype = OB_SINGLE_ARROW;
	}

	ob->pd = object_add_collision_fields(type);

	DAG_relations_tag_update(CTX_data_main(C));

	return OPERATOR_FINISHED;
}

void OBJECT_OT_effector_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Effector";
	ot->description = "Add an empty object with a physics effector to the scene";
	ot->idname = "OBJECT_OT_effector_add";

	/* api callbacks */
	ot->exec = effector_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", field_type_items, 0, "Type", "");

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

/********************* Add Camera Operator ********************/

static int object_camera_add_exec(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob;
	Camera *cam;
	bool enter_editmode;
	unsigned int layer;
	float loc[3], rot[3];

	/* force view align for cameras */
	RNA_boolean_set(op->ptr, "view_align", true);

	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL))
		return OPERATOR_CANCELLED;

	ob = ED_object_add_type(C, OB_CAMERA, loc, rot, false, layer);

	if (v3d) {
		if (v3d->camera == NULL)
			v3d->camera = ob;
		if (v3d->scenelock && scene->camera == NULL) {
			scene->camera = ob;
		}
	}

	cam = ob->data;
	cam->drawsize = v3d ? ED_view3d_grid_scale(scene, v3d, NULL) : ED_scene_grid_scale(scene, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_camera_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Camera";
	ot->description = "Add a camera object to the scene";
	ot->idname = "OBJECT_OT_camera_add";

	/* api callbacks */
	ot->exec = object_camera_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, true);

	/* hide this for cameras, default */
	prop = RNA_struct_type_find_property(ot->srna, "view_align");
	RNA_def_property_flag(prop, PROP_HIDDEN);
}


/********************* Add Metaball Operator ********************/

static int object_metaball_add_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	bool newob = false;
	bool enter_editmode;
	unsigned int layer;
	float loc[3], rot[3];
	float mat[4][4];
	float dia;

	WM_operator_view3d_unit_defaults(C, op);
	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL))
		return OPERATOR_CANCELLED;

	if (obedit == NULL || obedit->type != OB_MBALL) {
		obedit = ED_object_add_type(C, OB_MBALL, loc, rot, true, layer);
		newob = true;
	}
	else {
		DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	}

	ED_object_new_primitive_matrix(C, obedit, loc, rot, mat, false);
	dia = RNA_float_get(op->ptr, "radius");

	add_metaball_primitive(C, obedit, mat, dia, RNA_enum_get(op->ptr, "type"));

	/* userdef */
	if (newob && !enter_editmode) {
		ED_object_editmode_exit(C, EM_FREEDATA);
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_metaball_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Metaball";
	ot->description = "Add an metaball object to the scene";
	ot->idname = "OBJECT_OT_metaball_add";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_metaball_add_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna, "type", metaelem_type_items, 0, "Primitive", "");

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

/********************* Add Text Operator ********************/

static int object_add_text_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	bool enter_editmode;
	unsigned int layer;
	float loc[3], rot[3];

	WM_operator_view3d_unit_defaults(C, op);
	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL))
		return OPERATOR_CANCELLED;

	if (obedit && obedit->type == OB_FONT)
		return OPERATOR_CANCELLED;

	obedit = ED_object_add_type(C, OB_FONT, loc, rot, enter_editmode, layer);
	BKE_object_obdata_size_init(obedit, RNA_float_get(op->ptr, "radius"));

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_text_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Text";
	ot->description = "Add a text object to the scene";
	ot->idname = "OBJECT_OT_text_add";

	/* api callbacks */
	ot->exec = object_add_text_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

/********************* Add Armature Operator ********************/

static int object_armature_add_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	bool newob = false;
	bool enter_editmode;
	unsigned int layer;
	float loc[3], rot[3], dia;
	bool view_aligned = rv3d && (U.flag & USER_ADD_VIEWALIGNED);

	WM_operator_view3d_unit_defaults(C, op);
	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL))
		return OPERATOR_CANCELLED;

	if ((obedit == NULL) || (obedit->type != OB_ARMATURE)) {
		obedit = ED_object_add_type(C, OB_ARMATURE, loc, rot, true, layer);
		ED_object_editmode_enter(C, 0);
		newob = true;
	}
	else {
		DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	}

	if (obedit == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Cannot create editmode armature");
		return OPERATOR_CANCELLED;
	}

	dia = RNA_float_get(op->ptr, "radius");
	ED_armature_edit_bone_add_primitive(obedit, dia, view_aligned);

	/* userdef */
	if (newob && !enter_editmode)
		ED_object_editmode_exit(C, EM_FREEDATA);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_armature_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Armature";
	ot->description = "Add an armature object to the scene";
	ot->idname = "OBJECT_OT_armature_add";

	/* api callbacks */
	ot->exec = object_armature_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

/********************* Add Empty Operator ********************/

static int object_empty_add_exec(bContext *C, wmOperator *op)
{
	Object *ob;
	int type = RNA_enum_get(op->ptr, "type");
	unsigned int layer;
	float loc[3], rot[3];

	WM_operator_view3d_unit_defaults(C, op);
	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, &layer, NULL))
		return OPERATOR_CANCELLED;

	ob = ED_object_add_type(C, OB_EMPTY, loc, rot, false, layer);

	BKE_object_empty_draw_type_set(ob, type);
	BKE_object_obdata_size_init(ob, RNA_float_get(op->ptr, "radius"));

	return OPERATOR_FINISHED;
}

void OBJECT_OT_empty_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Empty";
	ot->description = "Add an empty object to the scene";
	ot->idname = "OBJECT_OT_empty_add";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_empty_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", object_empty_drawtype_items, 0, "Type", "");

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, false);
}

static int empty_drop_named_image_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Scene *scene = CTX_data_scene(C);

	Base *base = NULL;
	Image *ima = NULL;
	Object *ob = NULL;

	ima = (Image *)WM_operator_drop_load_path(C, op, ID_IM);
	if (!ima) {
		return OPERATOR_CANCELLED;
	}
	/* handled below */
	id_us_min((ID *)ima);

	base = ED_view3d_give_base_under_cursor(C, event->mval);

	/* if empty under cursor, then set object */
	if (base && base->object->type == OB_EMPTY) {
		ob = base->object;
		WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
	}
	else {
		/* add new empty */
		unsigned int layer;
		float rot[3];

		if (!ED_object_add_generic_get_opts(C, op, 'Z', NULL, rot, NULL, &layer, NULL))
			return OPERATOR_CANCELLED;

		ob = ED_object_add_type(C, OB_EMPTY, NULL, rot, false, layer);

		/* add under the mouse */
		ED_object_location_from_view(C, ob->loc);
		ED_view3d_cursor3d_position(C, ob->loc, event->mval);
	}

	BKE_object_empty_draw_type_set(ob, OB_EMPTY_IMAGE);

	id_us_min(ob->data);
	ob->data = ima;
	id_us_plus(ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_drop_named_image(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Empty Image/Drop Image To Empty";
	ot->description = "Add an empty image type to scene with data";
	ot->idname = "OBJECT_OT_drop_named_image";

	/* api callbacks */
	ot->invoke = empty_drop_named_image_invoke;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_string(ot->srna, "filepath", NULL, FILE_MAX, "Filepath", "Path to image file");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	RNA_def_boolean(ot->srna, "relative_path", true, "Relative Path", "Select the file relative to the blend file");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	prop = RNA_def_string(ot->srna, "name", NULL, MAX_ID_NAME - 2, "Name", "Image name to assign");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
	ED_object_add_generic_props(ot, false);
}

/********************* Add Lamp Operator ********************/

static const char *get_lamp_defname(int type)
{
	switch (type) {
		case LA_LOCAL: return CTX_DATA_(BLF_I18NCONTEXT_ID_LAMP, "Point");
		case LA_SUN: return CTX_DATA_(BLF_I18NCONTEXT_ID_LAMP, "Sun");
		case LA_SPOT: return CTX_DATA_(BLF_I18NCONTEXT_ID_LAMP, "Spot");
		case LA_HEMI: return CTX_DATA_(BLF_I18NCONTEXT_ID_LAMP, "Hemi");
		case LA_AREA: return CTX_DATA_(BLF_I18NCONTEXT_ID_LAMP, "Area");
		default:
			return CTX_DATA_(BLF_I18NCONTEXT_ID_LAMP, "Lamp");
	}
}

static int object_lamp_add_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob;
	Lamp *la;
	int type = RNA_enum_get(op->ptr, "type");
	unsigned int layer;
	float loc[3], rot[3];

	WM_operator_view3d_unit_defaults(C, op);
	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, &layer, NULL))
		return OPERATOR_CANCELLED;

	ob = ED_object_add_type(C, OB_LAMP, loc, rot, false, layer);
	BKE_object_obdata_size_init(ob, RNA_float_get(op->ptr, "radius"));

	la = (Lamp *)ob->data;
	la->type = type;
	rename_id(&ob->id, get_lamp_defname(type));
	rename_id(&la->id, get_lamp_defname(type));

	if (BKE_scene_use_new_shading_nodes(scene)) {
		ED_node_shader_default(C, &la->id);
		la->use_nodes = true;
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_lamp_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Lamp";
	ot->description = "Add a lamp object to the scene";
	ot->idname = "OBJECT_OT_lamp_add";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_lamp_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", lamp_type_items, 0, "Type", "");
	RNA_def_property_translation_context(ot->prop, BLF_I18NCONTEXT_ID_LAMP);

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, false);
}

/********************* Add Group Instance Operator ********************/

static int group_instance_add_exec(bContext *C, wmOperator *op)
{
	Group *group;
	unsigned int layer;
	float loc[3], rot[3];
	
	if (RNA_struct_property_is_set(op->ptr, "name")) {
		char name[MAX_ID_NAME - 2];
		
		RNA_string_get(op->ptr, "name", name);
		group = (Group *)BKE_libblock_find_name(ID_GR, name);
		
		if (0 == RNA_struct_property_is_set(op->ptr, "location")) {
			wmEvent *event = CTX_wm_window(C)->eventstate;
			ARegion *ar = CTX_wm_region(C);
			const int mval[2] = {event->x - ar->winrct.xmin,
			                     event->y - ar->winrct.ymin};
			ED_object_location_from_view(C, loc);
			ED_view3d_cursor3d_position(C, loc, mval);
			RNA_float_set_array(op->ptr, "location", loc);
		}
	}
	else
		group = BLI_findlink(&CTX_data_main(C)->group, RNA_enum_get(op->ptr, "group"));

	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, &layer, NULL))
		return OPERATOR_CANCELLED;

	if (group) {
		Main *bmain = CTX_data_main(C);
		Scene *scene = CTX_data_scene(C);
		Object *ob = ED_object_add_type(C, OB_EMPTY, loc, rot, false, layer);
		rename_id(&ob->id, group->id.name + 2);
		ob->dup_group = group;
		ob->transflag |= OB_DUPLIGROUP;
		id_lib_extern(&group->id);

		/* works without this except if you try render right after, see: 22027 */
		DAG_relations_tag_update(bmain);

		WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

/* only used as menu */
void OBJECT_OT_group_instance_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Group Instance";
	ot->description = "Add a dupligroup instance";
	ot->idname = "OBJECT_OT_group_instance_add";

	/* api callbacks */
	ot->invoke = WM_enum_search_invoke;
	ot->exec = group_instance_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "name", "Group", MAX_ID_NAME - 2, "Name", "Group name to add");
	prop = RNA_def_enum(ot->srna, "group", DummyRNA_NULL_items, 0, "Group", "");
	RNA_def_enum_funcs(prop, RNA_group_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
	ED_object_add_generic_props(ot, false);
}

/********************* Add Speaker Operator ********************/

static int object_speaker_add_exec(bContext *C, wmOperator *op)
{
	Object *ob;
	unsigned int layer;
	float loc[3], rot[3];
	Scene *scene = CTX_data_scene(C);

	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, &layer, NULL))
		return OPERATOR_CANCELLED;

	ob = ED_object_add_type(C, OB_SPEAKER, loc, rot, false, layer);

	/* to make it easier to start using this immediately in NLA, a default sound clip is created
	 * ready to be moved around to retime the sound and/or make new sound clips
	 */
	{
		/* create new data for NLA hierarchy */
		AnimData *adt = BKE_id_add_animdata(&ob->id);
		NlaTrack *nlt = add_nlatrack(adt, NULL);
		NlaStrip *strip = add_nla_soundstrip(scene, ob->data);
		strip->start = CFRA;
		strip->end += strip->start;

		/* hook them up */
		BKE_nlatrack_add_strip(nlt, strip);

		/* auto-name the strip, and give the track an interesting name  */
		BLI_strncpy(nlt->name, DATA_("SoundTrack"), sizeof(nlt->name));
		BKE_nlastrip_validate_name(adt, strip);

		WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_speaker_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Speaker";
	ot->description = "Add a speaker object to the scene";
	ot->idname = "OBJECT_OT_speaker_add";

	/* api callbacks */
	ot->exec = object_speaker_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, true);
}

/**************************** Delete Object *************************/

static void object_delete_check_glsl_update(Object *ob)
{
	/* some objects could affect on GLSL shading, make sure GLSL settings
	 * are being tagged to be updated when object is removing from scene
	 */
	if (ob->type == OB_LAMP) {
		if (ob->gpulamp.first)
			GPU_lamp_free(ob);
	}
}

/* remove base from a specific scene */
/* note: now unlinks constraints as well */
void ED_base_object_free_and_unlink(Main *bmain, Scene *scene, Base *base)
{
	DAG_id_type_tag(bmain, ID_OB);
	BKE_scene_base_unlink(scene, base);
	object_delete_check_glsl_update(base->object);
	BKE_libblock_free_us(bmain, base->object);
	if (scene->basact == base) scene->basact = NULL;
	MEM_freeN(base);
}

static int object_delete_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win;
	const bool use_global = RNA_boolean_get(op->ptr, "use_global");
	bool changed = false;

	if (CTX_data_edit_object(C)) 
		return OPERATOR_CANCELLED;

	CTX_DATA_BEGIN (C, Base *, base, selected_bases)
	{
		/* deselect object -- it could be used in other scenes */
		base->object->flag &= ~SELECT;

		/* remove from current scene only */
		ED_base_object_free_and_unlink(bmain, scene, base);
		changed = true;

		if (use_global) {
			Scene *scene_iter;
			Base *base_other;

			for (scene_iter = bmain->scene.first; scene_iter; scene_iter = scene_iter->id.next) {
				if (scene_iter != scene && !(scene_iter->id.lib)) {
					base_other = BKE_scene_base_find(scene_iter, base->object);
					if (base_other) {
						ED_base_object_free_and_unlink(bmain, scene_iter, base_other);
					}
				}
			}
		}
		/* end global */

	}
	CTX_DATA_END;

	if (!changed)
		return OPERATOR_CANCELLED;

	/* delete has to handle all open scenes */
	BKE_main_id_flag_listbase(&bmain->scene, LIB_DOIT, 1);
	for (win = wm->windows.first; win; win = win->next) {
		scene = win->screen->scene;
		
		if (scene->id.flag & LIB_DOIT) {
			scene->id.flag &= ~LIB_DOIT;
			
			DAG_relations_tag_update(bmain);

			WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
			WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
		}
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->description = "Delete selected objects";
	ot->idname = "OBJECT_OT_delete";

	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = object_delete_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "use_global", 0, "Delete Globally", "Remove object from all scenes");
}

/**************************** Copy Utilities ******************************/

/* after copying objects, copied data should get new pointers */
static void copy_object_set_idnew(bContext *C, int dupflag)
{
	Main *bmain = CTX_data_main(C);
	Material *ma, *mao;
	ID *id;
	int a;

	/* XXX check object pointers */
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		BKE_object_relink(ob);
	}
	CTX_DATA_END;

	/* materials */
	if (dupflag & USER_DUP_MAT) {
		mao = bmain->mat.first;
		while (mao) {
			if (mao->id.newid) {
				ma = (Material *)mao->id.newid;

				if (dupflag & USER_DUP_TEX) {
					for (a = 0; a < MAX_MTEX; a++) {
						if (ma->mtex[a]) {
							id = (ID *)ma->mtex[a]->tex;
							if (id) {
								ID_NEW_US(ma->mtex[a]->tex)
								else
									ma->mtex[a]->tex = BKE_texture_copy(ma->mtex[a]->tex);
								id->us--;
							}
						}
					}
				}
#if 0 // XXX old animation system
				id = (ID *)ma->ipo;
				if (id) {
					ID_NEW_US(ma->ipo)
					else
						ma->ipo = copy_ipo(ma->ipo);
					id->us--;
				}
#endif // XXX old animation system
			}
			mao = mao->id.next;
		}
	}

#if 0 // XXX old animation system
	  /* lamps */
	if (dupflag & USER_DUP_IPO) {
		Lamp *la = bmain->lamp.first;
		while (la) {
			if (la->id.newid) {
				Lamp *lan = (Lamp *)la->id.newid;
				id = (ID *)lan->ipo;
				if (id) {
					ID_NEW_US(lan->ipo)
					else
						lan->ipo = copy_ipo(lan->ipo);
					id->us--;
				}
			}
			la = la->id.next;
		}
	}

	/* ipos */
	ipo = bmain->ipo.first;
	while (ipo) {
		if (ipo->id.lib == NULL && ipo->id.newid) {
			Ipo *ipon = (Ipo *)ipo->id.newid;
			IpoCurve *icu;
			for (icu = ipon->curve.first; icu; icu = icu->next) {
				if (icu->driver) {
					ID_NEW(icu->driver->ob);
				}
			}
		}
		ipo = ipo->id.next;
	}
#endif // XXX old animation system

	set_sca_new_poins();

	BKE_main_id_clear_newpoins(bmain);
}

/********************* Make Duplicates Real ************************/

/**
 * \note regarding hashing dupli-objects, skip the first member of #DupliObject.persistent_id
 * since its a unique index and we only want to know if the group objects are from the same dupli-group instance.
 */
static unsigned int dupliobject_hash(const void *ptr)
{
	const DupliObject *dob = ptr;
	unsigned int hash = BLI_ghashutil_ptrhash(dob->ob);
	unsigned int i;
	for (i = 1; (i < MAX_DUPLI_RECUR) && dob->persistent_id[i] != INT_MAX; i++) {
		hash ^= (dob->persistent_id[i] ^ i);
	}
	return hash;
}

static bool dupliobject_cmp(const void *a_, const void *b_)
{
	const DupliObject *a = a_;
	const DupliObject *b = b_;
	unsigned int i;

	if (a->ob != b->ob) {
		return true;
	}

	for (i = 1; (i < MAX_DUPLI_RECUR); i++) {
		if (a->persistent_id[i] != b->persistent_id[i]) {
			return true;
		}
		else if (a->persistent_id[i] == INT_MAX) {
			break;
		}
	}

	/* matching */
	return false;
}

static void make_object_duplilist_real(bContext *C, Scene *scene, Base *base,
                                       const bool use_base_parent,
                                       const bool use_hierarchy)
{
	Main *bmain = CTX_data_main(C);
	ListBase *lb;
	DupliObject *dob;
	GHash *dupli_gh = NULL, *parent_gh = NULL;
	Object *object;

	if (!(base->object->transflag & OB_DUPLI))
		return;

	lb = object_duplilist(bmain->eval_ctx, scene, base->object);

	if (use_hierarchy || use_base_parent) {
		dupli_gh = BLI_ghash_ptr_new(__func__);
		parent_gh = BLI_ghash_new(dupliobject_hash, dupliobject_cmp, __func__);
	}

	for (dob = lb->first; dob; dob = dob->next) {
		Base *basen;
		Object *ob = BKE_object_copy(dob->ob);

		/* font duplis can have a totcol without material, we get them from parent
		 * should be implemented better...
		 */
		if (ob->mat == NULL) ob->totcol = 0;

		basen = MEM_dupallocN(base);
		basen->flag &= ~(OB_FROMDUPLI | OB_FROMGROUP);
		ob->flag = basen->flag;
		basen->lay = base->lay;
		BLI_addhead(&scene->base, basen);   /* addhead: othwise eternal loop */
		basen->object = ob;

		/* make sure apply works */
		BKE_free_animdata(&ob->id);
		ob->adt = NULL;

		/* Proxies are not to be copied. */
		ob->proxy_from = NULL;
		ob->proxy_group = NULL;
		ob->proxy = NULL;

		ob->parent = NULL;
		BLI_listbase_clear(&ob->constraints);
		ob->curve_cache = NULL;
		ob->transflag &= ~OB_DUPLI;
		ob->lay = base->lay;

		copy_m4_m4(ob->obmat, dob->mat);
		BKE_object_apply_mat4(ob, ob->obmat, false, false);

		if (dupli_gh)
			BLI_ghash_insert(dupli_gh, dob, ob);
		if (parent_gh)
			BLI_ghash_insert(parent_gh, dob, ob);

		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}

	if (use_hierarchy) {
		for (dob = lb->first; dob; dob = dob->next) {
			/* original parents */
			Object *ob_src =     dob->ob;
			Object *ob_src_par = ob_src->parent;

			Object *ob_dst =     BLI_ghash_lookup(dupli_gh, dob);
			Object *ob_dst_par = NULL;

			/* find parent that was also made real */
			if (ob_src_par) {
				/* OK to keep most of the members uninitialized,
				 * they won't be read, this is simply for a hash lookup. */
				DupliObject dob_key;
				dob_key.ob = ob_src_par;
				memcpy(&dob_key.persistent_id[1],
				       &dob->persistent_id[1],
				       sizeof(dob->persistent_id[1]) * (MAX_DUPLI_RECUR - 1));
				ob_dst_par = BLI_ghash_lookup(parent_gh, &dob_key);
			}

			if (ob_dst_par) {
				/* allow for all possible parent types */
				ob_dst->partype = ob_src->partype;
				BLI_strncpy(ob_dst->parsubstr, ob_src->parsubstr, sizeof(ob_dst->parsubstr));
				ob_dst->par1 = ob_src->par1;
				ob_dst->par2 = ob_src->par2;
				ob_dst->par3 = ob_src->par3;

				copy_m4_m4(ob_dst->parentinv, ob_src->parentinv);

				ob_dst->parent = ob_dst_par;
			}
			else if (use_base_parent) {
				ob_dst->parent = base->object;
				ob_dst->partype = PAROBJECT;
			}

			if (ob_dst->parent) {
				invert_m4_m4(ob_dst->parentinv, dob->mat);

				/* note, this may be the parent of other objects, but it should
				 * still work out ok */
				BKE_object_apply_mat4(ob_dst, dob->mat, false, true);

				/* to set ob_dst->orig and in case theres any other discrepicies */
				DAG_id_tag_update(&ob_dst->id, OB_RECALC_OB);
			}
		}
	}
	else if (use_base_parent) {
		/* since we are ignoring the internal hierarchy - parent all to the
		 * base object */
		for (dob = lb->first; dob; dob = dob->next) {
			/* original parents */
			Object *ob_dst = BLI_ghash_lookup(dupli_gh, dob);

			ob_dst->parent = base->object;
			ob_dst->partype = PAROBJECT;

			/* similer to the code above, see comments */
			invert_m4_m4(ob_dst->parentinv, dob->mat);
			BKE_object_apply_mat4(ob_dst, dob->mat, false, true);
			DAG_id_tag_update(&ob_dst->id, OB_RECALC_OB);
		}
	}

	/* The same how BKE_object_unlink detects which object proxies to clear. */
	if (base->object->transflag & OB_DUPLIGROUP && base->object->dup_group) {
		for (object = bmain->object.first; object; object = object->id.next) {
			if (object->proxy_group == base->object) {
				object->proxy = NULL;
				object->proxy_from = NULL;
				DAG_id_tag_update(&object->id, OB_RECALC_OB);
			}
		}
	}

	if (dupli_gh)
		BLI_ghash_free(dupli_gh, NULL, NULL);
	if (parent_gh)
		BLI_ghash_free(parent_gh, NULL, NULL);

	copy_object_set_idnew(C, 0);

	free_object_duplilist(lb);

	base->object->transflag &= ~OB_DUPLI;
}

static int object_duplicates_make_real_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);

	const bool use_base_parent = RNA_boolean_get(op->ptr, "use_base_parent");
	const bool use_hierarchy = RNA_boolean_get(op->ptr, "use_hierarchy");

	BKE_main_id_clear_newpoins(bmain);

	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		make_object_duplilist_real(C, scene, base, use_base_parent, use_hierarchy);

		/* dependencies were changed */
		WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, base->object);
	}
	CTX_DATA_END;

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_SCENE, scene);
	WM_main_add_notifier(NC_OBJECT | ND_DRAW, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_duplicates_make_real(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Duplicates Real";
	ot->description = "Make dupli objects attached to this object real";
	ot->idname = "OBJECT_OT_duplicates_make_real";

	/* api callbacks */
	ot->exec = object_duplicates_make_real_exec;

	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "use_base_parent", 0, "Parent", "Parent newly created objects to the original duplicator");
	RNA_def_boolean(ot->srna, "use_hierarchy", 0, "Keep Hierarchy", "Maintain parent child relationships");
}

/**************************** Convert **************************/

static EnumPropertyItem convert_target_items[] = {
	{OB_CURVE, "CURVE", ICON_OUTLINER_OB_CURVE, "Curve from Mesh/Text", ""},
	{OB_MESH, "MESH", ICON_OUTLINER_OB_MESH, "Mesh from Curve/Meta/Surf/Text", ""},
	{0, NULL, 0, NULL, NULL}
};

static void convert_ensure_curve_cache(Main *bmain, Scene *scene, Object *ob)
{
	if (ob->curve_cache == NULL) {
		/* Force creation. This is normally not needed but on operator
		 * redo we might end up with an object which isn't evaluated yet.
		 */
		if (ELEM(ob->type, OB_SURF, OB_CURVE, OB_FONT)) {
			BKE_displist_make_curveTypes(scene, ob, false);
		}
		else if (ob->type == OB_MBALL) {
			BKE_displist_make_mball(bmain->eval_ctx, scene, ob);
		}
	}
}

static void curvetomesh(Main *bmain, Scene *scene, Object *ob)
{
	convert_ensure_curve_cache(bmain, scene, ob);
	BKE_mesh_from_nurbs(ob); /* also does users */

	if (ob->type == OB_MESH) {
		BKE_object_free_modifiers(ob);

		/* Game engine defaults for mesh objects */
		ob->body_type = OB_BODY_TYPE_STATIC;
		ob->gameflag = OB_PROP | OB_COLLISION;
	}
}

static int convert_poll(bContext *C)
{
	Object *obact = CTX_data_active_object(C);
	Scene *scene = CTX_data_scene(C);

	return (!scene->id.lib && obact && scene->obedit != obact && (obact->flag & SELECT) && !(obact->id.lib));
}

/* Helper for convert_exec */
static Base *duplibase_for_convert(Scene *scene, Base *base, Object *ob)
{
	Object *obn;
	Base *basen;

	if (ob == NULL) {
		ob = base->object;
	}

	obn = BKE_object_copy(ob);
	DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

	basen = MEM_mallocN(sizeof(Base), "duplibase");
	*basen = *base;
	BLI_addhead(&scene->base, basen);   /* addhead: otherwise eternal loop */
	basen->object = obn;
	basen->flag |= SELECT;
	obn->flag |= SELECT;
	base->flag &= ~SELECT;
	ob->flag &= ~SELECT;

	return basen;
}

static int convert_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Base *basen = NULL, *basact = NULL;
	Object *ob, *ob1, *newob, *obact = CTX_data_active_object(C);
	DerivedMesh *dm;
	Curve *cu;
	Nurb *nu;
	MetaBall *mb;
	Mesh *me;
	const short target = RNA_enum_get(op->ptr, "target");
	const bool keep_original = RNA_boolean_get(op->ptr, "keep_original");
	int a, mballConverted = 0;

	/* don't forget multiple users! */

	/* reset flags */
	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		ob = base->object;
		ob->flag &= ~OB_DONE;

		/* flag data thats not been edited (only needed for !keep_original) */
		if (ob->data) {
			((ID *)ob->data)->flag |= LIB_DOIT;
		}
	}
	CTX_DATA_END;

	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		ob = base->object;

		if (ob->flag & OB_DONE || !IS_TAGGED(ob->data)) {
			if (ob->type != target) {
				base->flag &= ~SELECT;
				ob->flag &= ~SELECT;
			}

			/* obdata already modified */
			if (!IS_TAGGED(ob->data)) {
				/* When 2 objects with linked data are selected, converting both
				 * would keep modifiers on all but the converted object [#26003] */
				if (ob->type == OB_MESH) {
					BKE_object_free_modifiers(ob);  /* after derivedmesh calls! */
				}
			}
		}
		else if (ob->type == OB_MESH && target == OB_CURVE) {
			ob->flag |= OB_DONE;

			if (keep_original) {
				basen = duplibase_for_convert(scene, base, NULL);
				newob = basen->object;

				/* decrement original mesh's usage count  */
				me = newob->data;
				me->id.us--;

				/* make a new copy of the mesh */
				newob->data = BKE_mesh_copy(me);
			}
			else {
				newob = ob;
			}

			BKE_mesh_to_curve(scene, newob);

			if (newob->type == OB_CURVE) {
				BKE_object_free_modifiers(newob);   /* after derivedmesh calls! */
				ED_rigidbody_object_remove(scene, newob);
			}
		}
		else if (ob->type == OB_MESH && ob->modifiers.first) { /* converting a mesh with no modifiers causes a segfault */
			ob->flag |= OB_DONE;

			if (keep_original) {
				basen = duplibase_for_convert(scene, base, NULL);
				newob = basen->object;

				/* decrement original mesh's usage count  */
				me = newob->data;
				me->id.us--;

				/* make a new copy of the mesh */
				newob->data = BKE_mesh_copy(me);
			}
			else {
				newob = ob;
				DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
			}

			/* make new mesh data from the original copy */
			/* note: get the mesh from the original, not from the copy in some
			 * cases this doesnt give correct results (when MDEF is used for eg)
			 */
			dm = mesh_get_derived_final(scene, newob, CD_MASK_MESH);
			// dm = mesh_create_derived_no_deform(ob1, NULL);  /* this was called original (instead of get_derived). man o man why! (ton) */

			DM_to_mesh(dm, newob->data, newob, CD_MASK_MESH);

			/* re-tessellation is called by DM_to_mesh */

			dm->release(dm);
			BKE_object_free_modifiers(newob);   /* after derivedmesh calls! */
		}
		else if (ob->type == OB_FONT) {
			ob->flag |= OB_DONE;

			if (keep_original) {
				basen = duplibase_for_convert(scene, base, NULL);
				newob = basen->object;

				/* decrement original curve's usage count  */
				((Curve *)newob->data)->id.us--;

				/* make a new copy of the curve */
				newob->data = BKE_curve_copy(ob->data);
			}
			else {
				newob = ob;
			}

			cu = newob->data;

			/* TODO(sergey): Ideally DAG will create nurbs list for a curve data
			 *               datablock, but for until we've got granular update
			 *               lets take care by selves.
			 */
			BKE_vfont_to_curve(bmain, newob, FO_EDIT);

			newob->type = OB_CURVE;
			cu->type = OB_CURVE;

			if (cu->vfont) {
				cu->vfont->id.us--;
				cu->vfont = NULL;
			}
			if (cu->vfontb) {
				cu->vfontb->id.us--;
				cu->vfontb = NULL;
			}
			if (cu->vfonti) {
				cu->vfonti->id.us--;
				cu->vfonti = NULL;
			}
			if (cu->vfontbi) {
				cu->vfontbi->id.us--;
				cu->vfontbi = NULL;
			}

			if (!keep_original) {
				/* other users */
				if (cu->id.us > 1) {
					for (ob1 = bmain->object.first; ob1; ob1 = ob1->id.next) {
						if (ob1->data == ob->data) {
							ob1->type = OB_CURVE;
							DAG_id_tag_update(&ob1->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
						}
					}
				}
			}

			for (nu = cu->nurb.first; nu; nu = nu->next)
				nu->charidx = 0;

			cu->flag &= ~CU_3D;
			BKE_curve_curve_dimension_update(cu);

			if (target == OB_MESH) {
				curvetomesh(bmain, scene, newob);

				/* meshes doesn't use displist */
				BKE_object_free_curve_cache(newob);
			}
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
			ob->flag |= OB_DONE;

			if (target == OB_MESH) {
				if (keep_original) {
					basen = duplibase_for_convert(scene, base, NULL);
					newob = basen->object;

					/* decrement original curve's usage count  */
					((Curve *)newob->data)->id.us--;

					/* make a new copy of the curve */
					newob->data = BKE_curve_copy(ob->data);
				}
				else {
					newob = ob;
				}

				curvetomesh(bmain, scene, newob);

				/* meshes doesn't use displist */
				BKE_object_free_curve_cache(newob);
			}
		}
		else if (ob->type == OB_MBALL && target == OB_MESH) {
			Object *baseob;

			base->flag &= ~SELECT;
			ob->flag &= ~SELECT;

			baseob = BKE_mball_basis_find(scene, ob);

			if (ob != baseob) {
				/* if motherball is converting it would be marked as done later */
				ob->flag |= OB_DONE;
			}

			if (!(baseob->flag & OB_DONE)) {
				baseob->flag |= OB_DONE;

				basen = duplibase_for_convert(scene, base, baseob);
				newob = basen->object;

				mb = newob->data;
				mb->id.us--;

				newob->data = BKE_mesh_add(bmain, "Mesh");
				newob->type = OB_MESH;

				me = newob->data;
				me->totcol = mb->totcol;
				if (newob->totcol) {
					me->mat = MEM_dupallocN(mb->mat);
					for (a = 0; a < newob->totcol; a++) id_us_plus((ID *)me->mat[a]);
				}

				convert_ensure_curve_cache(bmain, scene, baseob);
				BKE_mesh_from_metaball(&baseob->curve_cache->disp, newob->data);

				if (obact->type == OB_MBALL) {
					basact = basen;
				}

				mballConverted = 1;
			}
		}
		else {
			continue;
		}

		/* tag obdata if it was been changed */

		/* If the original object is active then make this object active */
		if (basen) {
			if (ob == obact) {
				/* store new active base to update BASACT */
				basact = basen;
			}

			basen = NULL;
		}

		if (!keep_original && (ob->flag & OB_DONE)) {
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			((ID *)ob->data)->flag &= ~LIB_DOIT; /* flag not to convert this datablock again */
		}
	}
	CTX_DATA_END;

	if (!keep_original) {
		if (mballConverted) {
			Base *base = scene->base.first, *tmpbase;
			while (base) {
				ob = base->object;
				tmpbase = base;
				base = base->next;

				if (ob->type == OB_MBALL) {
					ED_base_object_free_and_unlink(bmain, scene, tmpbase);
				}
			}
		}

		/* delete object should renew depsgraph */
		DAG_relations_tag_update(bmain);
	}

// XXX	ED_object_editmode_enter(C, 0);
// XXX	exit_editmode(C, EM_FREEDATA|EM_WAITCURSOR); /* freedata, but no undo */

	if (basact) {
		/* active base was changed */
		ED_base_object_activate(C, basact);
		BASACT = basact;
	}
	else if (BASACT->object->flag & OB_DONE) {
		WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, BASACT->object);
		WM_event_add_notifier(C, NC_OBJECT | ND_DATA, BASACT->object);
	}

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, scene);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

	return OPERATOR_FINISHED;
}


void OBJECT_OT_convert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Convert to";
	ot->description = "Convert selected objects to another type";
	ot->idname = "OBJECT_OT_convert";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = convert_exec;
	ot->poll = convert_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "target", convert_target_items, OB_MESH, "Target", "Type of object to convert to");
	RNA_def_boolean(ot->srna, "keep_original", 0, "Keep Original", "Keep original objects instead of replacing them");
}

/**************************** Duplicate ************************/

/* 
 * dupflag: a flag made from constants declared in DNA_userdef_types.h
 * The flag tells adduplicate() whether to copy data linked to the object, or to reference the existing data.
 * U.dupflag for default operations or you can construct a flag as python does
 * if the dupflag is 0 then no data will be copied (linked duplicate) */

/* used below, assumes id.new is correct */
/* leaves selection of base/object unaltered */
static Base *object_add_duplicate_internal(Main *bmain, Scene *scene, Base *base, int dupflag)
{
	Base *basen = NULL;
	Material ***matarar;
	Object *ob, *obn;
	ID *id;
	int a, didit;

	ob = base->object;
	if (ob->mode & OB_MODE_POSE) {
		; /* nothing? */
	}
	else {
		obn = BKE_object_copy(ob);
		DAG_id_tag_update(&obn->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

		basen = MEM_mallocN(sizeof(Base), "duplibase");
		*basen = *base;
		BLI_addhead(&scene->base, basen);   /* addhead: prevent eternal loop */
		basen->object = obn;

		/* 1) duplis should end up in same group as the original
		 * 2) Rigid Body sim participants MUST always be part of a group...
		 */
		// XXX: is 2) really a good measure here?
		if ((basen->flag & OB_FROMGROUP) || ob->rigidbody_object || ob->rigidbody_constraint) {
			Group *group;
			for (group = bmain->group.first; group; group = group->id.next) {
				if (BKE_group_object_exists(group, ob))
					BKE_group_object_add(group, obn, scene, basen);
			}
		}

		/* duplicates using userflags */
		if (dupflag & USER_DUP_ACT) {
			BKE_copy_animdata_id_action(&obn->id);
		}

		if (dupflag & USER_DUP_MAT) {
			for (a = 0; a < obn->totcol; a++) {
				id = (ID *)obn->mat[a];
				if (id) {
					ID_NEW_US(obn->mat[a])
					else
						obn->mat[a] = BKE_material_copy(obn->mat[a]);
					id->us--;

					if (dupflag & USER_DUP_ACT) {
						BKE_copy_animdata_id_action(&obn->mat[a]->id);
					}
				}
			}
		}
		if (dupflag & USER_DUP_PSYS) {
			ParticleSystem *psys;
			for (psys = obn->particlesystem.first; psys; psys = psys->next) {
				id = (ID *) psys->part;
				if (id) {
					ID_NEW_US(psys->part)
					else
						psys->part = BKE_particlesettings_copy(psys->part);

					if (dupflag & USER_DUP_ACT) {
						BKE_copy_animdata_id_action(&psys->part->id);
					}

					id->us--;
				}
			}
		}

		id = obn->data;
		didit = 0;

		switch (obn->type) {
			case OB_MESH:
				if (dupflag & USER_DUP_MESH) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_mesh_copy(obn->data);
						if (obn->fluidsimSettings) {
							obn->fluidsimSettings->orgMesh = (Mesh *)obn->data;
						}
						didit = 1;
					}
					id->us--;
				}
				break;
			case OB_CURVE:
				if (dupflag & USER_DUP_CURVE) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_curve_copy(obn->data);
						didit = 1;
					}
					id->us--;
				}
				break;
			case OB_SURF:
				if (dupflag & USER_DUP_SURF) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_curve_copy(obn->data);
						didit = 1;
					}
					id->us--;
				}
				break;
			case OB_FONT:
				if (dupflag & USER_DUP_FONT) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_curve_copy(obn->data);
						didit = 1;
					}
					id->us--;
				}
				break;
			case OB_MBALL:
				if (dupflag & USER_DUP_MBALL) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_mball_copy(obn->data);
						didit = 1;
					}
					id->us--;
				}
				break;
			case OB_LAMP:
				if (dupflag & USER_DUP_LAMP) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_lamp_copy(obn->data);
						didit = 1;
					}
					id->us--;
				}
				break;
			case OB_ARMATURE:
				DAG_id_tag_update(&obn->id, OB_RECALC_DATA);
				if (obn->pose)
					obn->pose->flag |= POSE_RECALC;
				if (dupflag & USER_DUP_ARM) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_armature_copy(obn->data);
						BKE_pose_rebuild(obn, obn->data);
						didit = 1;
					}
					id->us--;
				}
				break;
			case OB_LATTICE:
				if (dupflag != 0) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_lattice_copy(obn->data);
						didit = 1;
					}
					id->us--;
				}
				break;
			case OB_CAMERA:
				if (dupflag != 0) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_camera_copy(obn->data);
						didit = 1;
					}
					id->us--;
				}
				break;
			case OB_SPEAKER:
				if (dupflag != 0) {
					ID_NEW_US2(obn->data)
					else {
						obn->data = BKE_speaker_copy(obn->data);
						didit = 1;
					}
					id->us--;
				}
				break;
		}

		/* check if obdata is copied */
		if (didit) {
			Key *key = BKE_key_from_object(obn);

			if (dupflag & USER_DUP_ACT) {
				bActuator *act;

				BKE_copy_animdata_id_action((ID *)obn->data);
				if (key) {
					BKE_copy_animdata_id_action((ID *)key);
				}

				/* Update the duplicated action in the action actuators */
				for (act = obn->actuators.first; act; act = act->next) {
					if (act->type == ACT_ACTION) {
						bActionActuator *actact = (bActionActuator *) act->data;
						if (ob->adt && actact->act == ob->adt->action) {
							actact->act = obn->adt->action;
						}
					}
				}
			}

			if (dupflag & USER_DUP_MAT) {
				matarar = give_matarar(obn);
				if (matarar) {
					for (a = 0; a < obn->totcol; a++) {
						id = (ID *)(*matarar)[a];
						if (id) {
							ID_NEW_US((*matarar)[a])
							else
								(*matarar)[a] = BKE_material_copy((*matarar)[a]);
							id->us--;
						}
					}
				}
			}
		}
	}
	return basen;
}

/* single object duplicate, if dupflag==0, fully linked, else it uses the flags given */
/* leaves selection of base/object unaltered.
 * note: don't call this within a loop since clear_* funcs loop over the entire database.
 * note: caller must do DAG_relations_tag_update(bmain);
 *       this is not done automatic since we may duplicate many objects in a batch */
Base *ED_object_add_duplicate(Main *bmain, Scene *scene, Base *base, int dupflag)
{
	Base *basen;
	Object *ob;

	BKE_main_id_clear_newpoins(bmain);
	clear_sca_new_poins();  /* sensor/contr/act */

	basen = object_add_duplicate_internal(bmain, scene, base, dupflag);
	if (basen == NULL) {
		return NULL;
	}

	ob = basen->object;

	/* link own references to the newly duplicated data [#26816] */
	BKE_object_relink(ob);
	set_sca_new_poins_ob(ob);

	/* DAG_relations_tag_update(bmain); */ /* caller must do */

	if (ob->data) {
		ED_render_id_flush_update(bmain, ob->data);
	}

	return basen;
}

/* contextual operator dupli */
static int duplicate_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	const bool linked = RNA_boolean_get(op->ptr, "linked");
	int dupflag = (linked) ? 0 : U.dupflag;

	BKE_main_id_clear_newpoins(bmain);
	clear_sca_new_poins();  /* sensor/contr/act */

	CTX_DATA_BEGIN (C, Base *, base, selected_bases)
	{
		Base *basen = object_add_duplicate_internal(bmain, scene, base, dupflag);

		/* note that this is safe to do with this context iterator,
		 * the list is made in advance */
		ED_base_object_select(base, BA_DESELECT);

		if (basen == NULL) {
			continue;
		}

		/* new object becomes active */
		if (BASACT == base)
			ED_base_object_activate(C, basen);

		if (basen->object->data) {
			DAG_id_tag_update(basen->object->data, 0);
		}
	}
	CTX_DATA_END;

	copy_object_set_idnew(C, dupflag);

	DAG_relations_tag_update(bmain);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_duplicate(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Duplicate Objects";
	ot->description = "Duplicate selected objects";
	ot->idname = "OBJECT_OT_duplicate";

	/* api callbacks */
	ot->exec = duplicate_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* to give to transform */
	RNA_def_boolean(ot->srna, "linked", 0, "Linked", "Duplicate object but not object data, linking to the original data");
	prop = RNA_def_enum(ot->srna, "mode", transform_mode_types, TFM_TRANSLATION, "Mode", "");
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* **************** add named object, for dragdrop ************* */

static int add_named_exec(bContext *C, wmOperator *op)
{
	wmWindow *win = CTX_wm_window(C);
	const wmEvent *event = win ? win->eventstate : NULL;
	Main *bmain = CTX_data_main(C);
	View3D *v3d = CTX_wm_view3d(C);  /* may be NULL */
	Scene *scene = CTX_data_scene(C);
	Base *basen, *base;
	Object *ob;
	const bool linked = RNA_boolean_get(op->ptr, "linked");
	int dupflag = (linked) ? 0 : U.dupflag;
	char name[MAX_ID_NAME - 2];

	/* find object, create fake base */
	RNA_string_get(op->ptr, "name", name);
	ob = (Object *)BKE_libblock_find_name(ID_OB, name);

	if (ob == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Object not found");
		return OPERATOR_CANCELLED;
	}

	base = MEM_callocN(sizeof(Base), "duplibase");
	base->object = ob;
	base->flag = ob->flag;

	/* prepare dupli */
	BKE_main_id_clear_newpoins(bmain);
	clear_sca_new_poins();  /* sensor/contr/act */

	basen = object_add_duplicate_internal(bmain, scene, base, dupflag);

	if (basen == NULL) {
		MEM_freeN(base);
		BKE_report(op->reports, RPT_ERROR, "Object could not be duplicated");
		return OPERATOR_CANCELLED;
	}

	basen->lay = basen->object->lay = BKE_screen_view3d_layer_active(v3d, scene);
	basen->object->restrictflag &= ~OB_RESTRICT_VIEW;

	if (event) {
		ARegion *ar = CTX_wm_region(C);
		const int mval[2] = {event->x - ar->winrct.xmin,
		                     event->y - ar->winrct.ymin};
		ED_object_location_from_view(C, basen->object->loc);
		ED_view3d_cursor3d_position(C, basen->object->loc, mval);
	}
	
	ED_base_object_select(basen, BA_SELECT);
	ED_base_object_activate(C, basen);

	copy_object_set_idnew(C, dupflag);

	DAG_relations_tag_update(bmain);

	MEM_freeN(base);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT | ND_OB_ACTIVE, scene);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_add_named(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Named Object";
	ot->description = "Add named object";
	ot->idname = "OBJECT_OT_add_named";

	/* api callbacks */
	ot->exec = add_named_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "linked", 0, "Linked", "Duplicate object but not object data, linking to the original data");
	RNA_def_string(ot->srna, "name", NULL, MAX_ID_NAME - 2, "Name", "Object name to add");
}

/**************************** Join *************************/

static int join_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if (!ob || ob->id.lib) return 0;

	if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_ARMATURE))
		return ED_operator_screenactive(C);
	else
		return 0;
}

static int join_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);

	if (scene->obedit) {
		BKE_report(op->reports, RPT_ERROR, "This data does not support joining in edit mode");
		return OPERATOR_CANCELLED;
	}
	else if (BKE_object_obdata_is_libdata(ob)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot edit external libdata");
		return OPERATOR_CANCELLED;
	}

	if (ob->type == OB_MESH)
		return join_mesh_exec(C, op);
	else if (ELEM(ob->type, OB_CURVE, OB_SURF))
		return join_curve_exec(C, op);
	else if (ob->type == OB_ARMATURE)
		return join_armature_exec(C, op);

	return OPERATOR_CANCELLED;
}

void OBJECT_OT_join(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Join";
	ot->description = "Join selected objects into active object";
	ot->idname = "OBJECT_OT_join";

	/* api callbacks */
	ot->exec = join_exec;
	ot->poll = join_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/**************************** Join as Shape Key*************************/

static int join_shapes_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if (!ob || ob->id.lib) return 0;

	/* only meshes supported at the moment */
	if (ob->type == OB_MESH)
		return ED_operator_screenactive(C);
	else
		return 0;
}

static int join_shapes_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);

	if (scene->obedit) {
		BKE_report(op->reports, RPT_ERROR, "This data does not support joining in edit mode");
		return OPERATOR_CANCELLED;
	}
	else if (BKE_object_obdata_is_libdata(ob)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot edit external libdata");
		return OPERATOR_CANCELLED;
	}

	if (ob->type == OB_MESH)
		return join_mesh_shapes_exec(C, op);

	return OPERATOR_CANCELLED;
}

void OBJECT_OT_join_shapes(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Join as Shapes";
	ot->description = "Merge selected objects to shapes of active object";
	ot->idname = "OBJECT_OT_join_shapes";

	/* api callbacks */
	ot->exec = join_shapes_exec;
	ot->poll = join_shapes_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
