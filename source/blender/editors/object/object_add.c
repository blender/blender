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
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_gpencil.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_speaker.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_gpencil.h"
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

#include "object_intern.h"

/* this is an exact copy of the define in rna_lamp.c
 * kept here because of linking order.
 * Icons are only defined here */
const EnumPropertyItem rna_enum_light_type_items[] = {
	{LA_LOCAL, "POINT", ICON_LIGHT_POINT, "Point", "Omnidirectional point light source"},
	{LA_SUN, "SUN", ICON_LIGHT_SUN, "Sun", "Constant direction parallel ray light source"},
	{LA_SPOT, "SPOT", ICON_LIGHT_SPOT, "Spot", "Directional cone light source"},
	{LA_HEMI, "HEMI", ICON_LIGHT_HEMI, "Hemi", "180 degree constant light source"},
	{LA_AREA, "AREA", ICON_LIGHT_AREA, "Area", "Directional area light source"},
	{0, NULL, 0, NULL, NULL}
};

/* copy from rna_object_force.c */
static const EnumPropertyItem field_type_items[] = {
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

static EnumPropertyItem lightprobe_type_items[] = {
	{LIGHTPROBE_TYPE_CUBE, "CUBEMAP", ICON_LIGHTPROBE_CUBEMAP, "Reflection Cubemap",
     "Reflection probe with spherical or cubic attenuation"},
	{LIGHTPROBE_TYPE_PLANAR, "PLANAR", ICON_LIGHTPROBE_PLANAR, "Reflection Plane",
     "Planar reflection probe"},
	{LIGHTPROBE_TYPE_GRID, "GRID", ICON_LIGHTPROBE_GRID, "Irradiance Volume",
     "Irradiance probe to capture diffuse indirect lighting"},
	{0, NULL, 0, NULL, NULL}
};

/************************** Exported *****************************/

void ED_object_location_from_view(bContext *C, float loc[3])
{
	View3D *v3d = CTX_wm_view3d(C);
	Scene *scene = CTX_data_scene(C);
	const float *cursor;

	cursor = ED_view3d_cursor3d_get(scene, v3d)->location;

	copy_v3_v3(loc, cursor);
}

void ED_object_rotation_from_quat(float rot[3], const float viewquat[4], const char align_axis)
{
	BLI_assert(align_axis >= 'X' && align_axis <= 'Z');

	switch (align_axis) {
		case 'X':
		{
			/* Same as 'rv3d->viewinv[1]' */
			float axis_y[4] = {0.0f, 1.0f, 0.0f};
			float quat_y[4], quat[4];
			axis_angle_to_quat(quat_y, axis_y, M_PI_2);
			mul_qt_qtqt(quat, viewquat, quat_y);
			quat_to_eul(rot, quat);
			break;
		}
		case 'Y':
		{
			quat_to_eul(rot, viewquat);
			rot[0] -= (float)M_PI_2;
			break;
		}
		case 'Z':
		{
			quat_to_eul(rot, viewquat);
			break;
		}
	}
}

void ED_object_rotation_from_view(bContext *C, float rot[3], const char align_axis)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	BLI_assert(align_axis >= 'X' && align_axis <= 'Z');
	if (rv3d) {
		float viewquat[4];
		copy_qt_qt(viewquat, rv3d->viewquat);
		viewquat[0] *= -1.0f;
		ED_object_rotation_from_quat(rot, viewquat, align_axis);
	}
	else {
		zero_v3(rot);
	}
}

void ED_object_base_init_transform(bContext *C, Base *base, const float loc[3], const float rot[3])
{
	Object *ob = base->object;
	Scene *scene = CTX_data_scene(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);

	if (!scene) return;

	if (loc)
		copy_v3_v3(ob->loc, loc);

	if (rot)
		copy_v3_v3(ob->rot, rot);

	BKE_object_where_is_calc(depsgraph, scene, ob);
}

/* Uses context to figure out transform for primitive.
 * Returns standard diameter. */
float ED_object_new_primitive_matrix(
        bContext *C, Object *obedit,
        const float loc[3], const float rot[3], float primmat[4][4])
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
	RNA_def_float_distance(ot->srna, "radius", 1.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Radius", "", 0.001, 100.00);
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

	prop = RNA_def_float_vector_xyz(ot->srna, "location", 3, NULL, -OBJECT_ADD_SIZE_MAXF, OBJECT_ADD_SIZE_MAXF,
	                                "Location", "Location for the newly added object", -1000.0f, 1000.0f);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_float_rotation(ot->srna, "rotation", 3, NULL, -OBJECT_ADD_SIZE_MAXF, OBJECT_ADD_SIZE_MAXF,
	                              "Rotation", "Rotation for the newly added object",
	                              DEG2RADF(-360.0f), DEG2RADF(360.0f));
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	prop = RNA_def_boolean_layer_member(ot->srna, "layers", 20, NULL, "Layer", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void ED_object_add_mesh_props(wmOperatorType *ot)
{
	RNA_def_boolean(ot->srna, "calc_uvs", true, "Generate UVs", "Generate a default UV map");
}

bool ED_object_add_generic_get_opts(bContext *C, wmOperator *op, const char view_align_axis,
                                    float loc[3], float rot[3],
                                    bool *enter_editmode, unsigned int *layer, bool *is_view_aligned)
{
	View3D *v3d = CTX_wm_view3d(C);
	unsigned int _layer;
	PropertyRNA *prop;

	/* Switch to Edit mode? optional prop */
	if ((prop = RNA_struct_find_property(op->ptr, "enter_editmode"))) {
		bool _enter_editmode;
		if (!enter_editmode)
			enter_editmode = &_enter_editmode;

		if (RNA_property_is_set(op->ptr, prop) && enter_editmode)
			*enter_editmode = RNA_property_boolean_get(op->ptr, prop);
		else {
			*enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
			RNA_property_boolean_set(op->ptr, prop, *enter_editmode);
		}
	}

	/* Get layers! */
	{
		int a;
		bool layer_values[20];
		if (!layer)
			layer = &_layer;

		prop = RNA_struct_find_property(op->ptr, "layers");
		if (RNA_property_is_set(op->ptr, prop)) {
			RNA_property_boolean_get_array(op->ptr, prop, layer_values);
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
				layer_values[a] = (*layer & (1 << a)) != 0;
			}
			RNA_property_boolean_set_array(op->ptr, prop, layer_values);
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
Object *ED_object_add_type(
        bContext *C,
        int type, const char *name,
        const float loc[3], const float rot[3],
        bool enter_editmode, unsigned int UNUSED(layer))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Object *ob;

	/* for as long scene has editmode... */
	if (CTX_data_edit_object(C)) {
		ED_object_editmode_exit(C, EM_FREEDATA | EM_WAITCURSOR);
	}

	/* deselects all, sets scene->basact */
	ob = BKE_object_add(bmain, scene, view_layer, type, name);
	/* editor level activate, notifiers */
	ED_object_base_activate(C, view_layer->basact);

	/* more editor stuff */
	ED_object_base_init_transform(C, view_layer->basact, loc, rot);

	/* TODO(sergey): This is weird to manually tag objects for update, better to
	 * use DEG_id_tag_update here perhaps.
	 */
	DEG_id_type_tag(bmain, ID_OB);
	DEG_relations_tag_update(bmain);
	if (ob->data != NULL) {
		DEG_id_tag_update_ex(bmain, (ID *)ob->data, DEG_TAG_EDITORS_UPDATE);
	}

	if (enter_editmode)
		ED_object_editmode_enter(C, EM_IGNORE_LAYER);

	WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

	/* TODO(sergey): Use proper flag for tagging here. */
	DEG_id_tag_update(&scene->id, 0);

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
	ob = ED_object_add_type(C, RNA_enum_get(op->ptr, "type"), NULL, loc, rot, enter_editmode, layer);

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
	RNA_def_enum(ot->srna, "type", rna_enum_object_type_items, 0, "Type", "");

	ED_object_add_generic_props(ot, true);
}

/********************** Add Probe Operator **********************/

/* for object add operator */
static int lightprobe_add_exec(bContext *C, wmOperator *op)
{
	Object *ob;
	LightProbe *probe;
	int type;
	bool enter_editmode;
	unsigned int layer;
	float loc[3], rot[3];
	float radius;

	WM_operator_view3d_unit_defaults(C, op);
	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL))
		return OPERATOR_CANCELLED;

	type = RNA_enum_get(op->ptr, "type");
	radius = RNA_float_get(op->ptr, "radius");

	const char *name = CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Light Probe");
	ob = ED_object_add_type(C, OB_LIGHTPROBE, name, loc, rot, false, layer);
	BKE_object_obdata_size_init(ob, radius);

	probe = (LightProbe *)ob->data;
	probe->type = type;

	switch (type) {
		case LIGHTPROBE_TYPE_GRID:
			probe->distinf = 0.3f;
			probe->falloff = 1.0f;
			probe->clipsta = 0.01f;
			break;
		case LIGHTPROBE_TYPE_PLANAR:
			probe->distinf = 0.1f;
			probe->falloff = 0.5f;
			probe->clipsta = 0.001f;
			ob->empty_drawsize = 0.5f;
			break;
		case LIGHTPROBE_TYPE_CUBE:
			probe->attenuation_type = LIGHTPROBE_SHAPE_ELIPSOID;
			break;
		default:
			BLI_assert(!"Lightprobe type not configured.");
			break;
	}

	DEG_relations_tag_update(CTX_data_main(C));

	return OPERATOR_FINISHED;
}

void OBJECT_OT_lightprobe_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Light Probe";
	ot->description = "Add a light probe object";
	ot->idname = "OBJECT_OT_lightprobe_add";

	/* api callbacks */
	ot->exec = lightprobe_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", lightprobe_type_items, 0, "Type", "");

	ED_object_add_unit_props(ot);
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
		const char *name = CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "CurveGuide");
		ob = ED_object_add_type(C, OB_CURVE, name, loc, rot, false, layer);

		cu = ob->data;
		cu->flag |= CU_PATH | CU_3D;
		ED_object_editmode_enter(C, 0);
		ED_object_new_primitive_matrix(C, ob, loc, rot, mat);
		BLI_addtail(&cu->editnurb->nurbs, ED_curve_add_nurbs_primitive(C, ob, mat, CU_NURBS | CU_PRIM_PATH, dia));
		if (!enter_editmode)
			ED_object_editmode_exit(C, EM_FREEDATA);
	}
	else {
		const char *name = CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Field");
		ob = ED_object_add_type(C, OB_EMPTY, name, loc, rot, false, layer);
		BKE_object_obdata_size_init(ob, dia);
		if (ELEM(type, PFIELD_WIND, PFIELD_VORTEX))
			ob->empty_drawtype = OB_SINGLE_ARROW;
	}

	ob->pd = object_add_collision_fields(type);

	DEG_relations_tag_update(CTX_data_main(C));

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

	ob = ED_object_add_type(C, OB_CAMERA, NULL, loc, rot, false, layer);

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
		obedit = ED_object_add_type(C, OB_MBALL, NULL, loc, rot, true, layer);
		newob = true;
	}
	else {
		DEG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	}

	ED_object_new_primitive_matrix(C, obedit, loc, rot, mat);
	dia = RNA_float_get(op->ptr, "radius");

	ED_mball_add_primitive(C, obedit, mat, dia, RNA_enum_get(op->ptr, "type"));

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

	ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_metaelem_type_items, 0, "Primitive", "");

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

	obedit = ED_object_add_type(C, OB_FONT, NULL, loc, rot, enter_editmode, layer);
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
		obedit = ED_object_add_type(C, OB_ARMATURE, NULL, loc, rot, true, layer);
		ED_object_editmode_enter(C, 0);
		newob = true;
	}
	else {
		DEG_id_tag_update(&obedit->id, OB_RECALC_DATA);
	}

	if (obedit == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Cannot create editmode armature");
		return OPERATOR_CANCELLED;
	}

	dia = RNA_float_get(op->ptr, "radius");
	ED_armature_ebone_add_primitive(obedit, dia, view_aligned);

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

	ob = ED_object_add_type(C, OB_EMPTY, NULL, loc, rot, false, layer);

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
	ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_object_empty_drawtype_items, 0, "Type", "");

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
		DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
	}
	else {
		/* add new empty */
		unsigned int layer;
		float rot[3];

		if (!ED_object_add_generic_get_opts(C, op, 'Z', NULL, rot, NULL, &layer, NULL))
			return OPERATOR_CANCELLED;

		ob = ED_object_add_type(C, OB_EMPTY, NULL, NULL, rot, false, layer);

		/* add under the mouse */
		ED_object_location_from_view(C, ob->loc);
		ED_view3d_cursor3d_position(C, event->mval, false, ob->loc);
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

/********************* Add Gpencil Operator ********************/

static int object_gpencil_add_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	bGPdata *gpd = (ob && (ob->type == OB_GPENCIL)) ? ob->data : NULL;

	const int type = RNA_enum_get(op->ptr, "type");

	float loc[3], rot[3];
	unsigned int layer;
	bool newob = false;

	/* Hack: Force view-align to be on by default
	 * since it's not nice for adding shapes in 2D
	 * for them to end up aligned oddly, but only for Monkey
	 */
	if ((RNA_struct_property_is_set(op->ptr, "view_align") == false) &&
	    (type == GP_MONKEY))
	{
		RNA_boolean_set(op->ptr, "view_align", true);
	}

	/* Note: We use 'Y' here (not 'Z'), as */
	WM_operator_view3d_unit_defaults(C, op);
	if (!ED_object_add_generic_get_opts(C, op, 'Y', loc, rot, NULL, &layer, NULL))
		return OPERATOR_CANCELLED;

	/* add new object if not currently editing a GP object,
	 * or if "empty" was chosen (i.e. user wants a blank GP canvas)
	 */
	if ((gpd == NULL) || (GPENCIL_ANY_MODE(gpd) == false) || (type == GP_EMPTY)) {
		const char *ob_name = (type == GP_MONKEY) ? "Suzanne" : NULL;
		float radius = RNA_float_get(op->ptr, "radius");

		ob = ED_object_add_type(C, OB_GPENCIL, ob_name, loc, rot, true, layer);
		gpd = ob->data;
		newob = true;

		BKE_object_obdata_size_init(ob, GP_OBGPENCIL_DEFAULT_SIZE * radius);
	}
	else {
		DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_ADDED, NULL);
	}

	/* create relevant geometry */
	switch (type) {
		case GP_STROKE:
		{
			float radius = RNA_float_get(op->ptr, "radius");
			float mat[4][4];

			ED_object_new_primitive_matrix(C, ob, loc, rot, mat);
			mul_v3_fl(mat[0], radius);
			mul_v3_fl(mat[1], radius);
			mul_v3_fl(mat[2], radius);

			ED_gpencil_create_stroke(C, mat);
			break;
		}
		case GP_MONKEY:
		{
			float radius = RNA_float_get(op->ptr, "radius");
			float mat[4][4];

			ED_object_new_primitive_matrix(C, ob, loc, rot, mat);
			mul_v3_fl(mat[0], radius);
			mul_v3_fl(mat[1], radius);
			mul_v3_fl(mat[2], radius);

			ED_gpencil_create_monkey(C, mat);
			break;
		}
		case GP_EMPTY:
			/* do nothing */
			break;

		default:
			BKE_report(op->reports, RPT_WARNING, "Not implemented");
			break;
	}

	/* if this is a new object, initialise default stuff (colors, etc.) */
	if (newob) {
		ED_gpencil_add_defaults(C);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_gpencil_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add GPencil";
	ot->description = "Add a grease pencil object to the scene";
	ot->idname = "OBJECT_OT_gpencil_add";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_gpencil_add_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, false);

	ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_object_gpencil_type_items, 0, "Type", "");
}

/********************* Add Light Operator ********************/

static const char *get_light_defname(int type)
{
	switch (type) {
		case LA_LOCAL: return CTX_DATA_(BLT_I18NCONTEXT_ID_LAMP, "Point");
		case LA_SUN: return CTX_DATA_(BLT_I18NCONTEXT_ID_LAMP, "Sun");
		case LA_SPOT: return CTX_DATA_(BLT_I18NCONTEXT_ID_LAMP, "Spot");
		case LA_HEMI: return CTX_DATA_(BLT_I18NCONTEXT_ID_LAMP, "Hemi");
		case LA_AREA: return CTX_DATA_(BLT_I18NCONTEXT_ID_LAMP, "Area");
		default:
			return CTX_DATA_(BLT_I18NCONTEXT_ID_LAMP, "Light");
	}
}

static int object_light_add_exec(bContext *C, wmOperator *op)
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

	ob = ED_object_add_type(C, OB_LAMP, get_light_defname(type), loc, rot, false, layer);

	float size = RNA_float_get(op->ptr, "radius");
	/* Better defaults for light size. */
	switch (type) {
		case LA_LOCAL:
		case LA_SPOT:
			break;
		case LA_AREA:
			size *= 4.0f;
			break;
		default:
			size *= 0.5f;
			break;
	}
	BKE_object_obdata_size_init(ob, size);

	la = (Lamp *)ob->data;
	la->type = type;

	if (BKE_scene_uses_cycles(scene)) {
		ED_node_shader_default(C, &la->id);
		la->use_nodes = true;
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_light_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Light";
	ot->description = "Add a light object to the scene";
	ot->idname = "OBJECT_OT_light_add";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_light_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_light_type_items, 0, "Type", "");
	RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_LAMP);

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, false);
}

/********************* Add Collection Instance Operator ********************/

static int collection_instance_add_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Collection *collection;
	unsigned int layer;
	float loc[3], rot[3];

	if (RNA_struct_property_is_set(op->ptr, "name")) {
		char name[MAX_ID_NAME - 2];

		RNA_string_get(op->ptr, "name", name);
		collection = (Collection *)BKE_libblock_find_name(bmain, ID_GR, name);

		if (0 == RNA_struct_property_is_set(op->ptr, "location")) {
			const wmEvent *event = CTX_wm_window(C)->eventstate;
			ARegion *ar = CTX_wm_region(C);
			const int mval[2] = {event->x - ar->winrct.xmin,
			                     event->y - ar->winrct.ymin};
			ED_object_location_from_view(C, loc);
			ED_view3d_cursor3d_position(C, mval, false, loc);
			RNA_float_set_array(op->ptr, "location", loc);
		}
	}
	else
		collection = BLI_findlink(&CTX_data_main(C)->collection, RNA_enum_get(op->ptr, "collection"));

	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, &layer, NULL))
		return OPERATOR_CANCELLED;

	if (collection) {
		Scene *scene = CTX_data_scene(C);
		ViewLayer *view_layer = CTX_data_view_layer(C);

		/* Avoid dependency cycles. */
		LayerCollection *active_lc = BKE_layer_collection_get_active(view_layer);
		while (BKE_collection_find_cycle(active_lc->collection, collection)) {
			active_lc = BKE_layer_collection_activate_parent(view_layer, active_lc);
		}

		Object *ob = ED_object_add_type(C, OB_EMPTY, collection->id.name + 2, loc, rot, false, layer);
		ob->dup_group = collection;
		ob->transflag |= OB_DUPLICOLLECTION;
		id_us_plus(&collection->id);

		/* works without this except if you try render right after, see: 22027 */
		DEG_relations_tag_update(bmain);
		DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

/* only used as menu */
void OBJECT_OT_collection_instance_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Collection Instance";
	ot->description = "Add a collection instance";
	ot->idname = "OBJECT_OT_collection_instance_add";

	/* api callbacks */
	ot->invoke = WM_enum_search_invoke;
	ot->exec = collection_instance_add_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "name", "Collection", MAX_ID_NAME - 2, "Name", "Collection name to add");
	prop = RNA_def_enum(ot->srna, "collection", DummyRNA_NULL_items, 0, "Collection", "");
	RNA_def_enum_funcs(prop, RNA_collection_itemf);
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

	ob = ED_object_add_type(C, OB_SPEAKER, NULL, loc, rot, false, layer);

	/* to make it easier to start using this immediately in NLA, a default sound clip is created
	 * ready to be moved around to retime the sound and/or make new sound clips
	 */
	{
		/* create new data for NLA hierarchy */
		AnimData *adt = BKE_animdata_add_id(&ob->id);
		NlaTrack *nlt = BKE_nlatrack_add(adt, NULL);
		NlaStrip *strip = BKE_nla_add_soundstrip(scene, ob->data);
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

/* remove base from a specific scene */
/* note: now unlinks constraints as well */
void ED_object_base_free_and_unlink(Main *bmain, Scene *scene, Object *ob)
{
	if (BKE_library_ID_is_indirectly_used(bmain, ob) &&
	    ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0)
	{
		/* We cannot delete indirectly used object... */
		printf("WARNING, undeletable object '%s', should have been catched before reaching this function!",
		       ob->id.name + 2);
		return;
	}

	DEG_id_tag_update_ex(bmain, &ob->id, DEG_TAG_BASE_FLAGS_UPDATE);

	BKE_scene_collections_object_remove(bmain, scene, ob, true);
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

	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		const bool is_indirectly_used = BKE_library_ID_is_indirectly_used(bmain, ob);
		if (ob->id.tag & LIB_TAG_INDIRECT) {
			/* Can this case ever happen? */
			BKE_reportf(op->reports, RPT_WARNING, "Cannot delete indirectly linked object '%s'", ob->id.name + 2);
			continue;
		}
		else if (is_indirectly_used && ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0) {
			BKE_reportf(op->reports, RPT_WARNING,
			        "Cannot delete object '%s' from scene '%s', indirectly used objects need at least one user",
			        ob->id.name + 2, scene->id.name + 2);
			continue;
		}

		/* if grease pencil object, set cache as dirty */
		if (ob->type == OB_GPENCIL) {
			bGPdata *gpd = (bGPdata *)ob->data;
			DEG_id_tag_update(&gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
		}

		/* This is sort of a quick hack to address T51243 - Proper thing to do here would be to nuke most of all this
		 * custom scene/object/base handling, and use generic lib remap/query for that.
		 * But this is for later (aka 2.8, once layers & co are settled and working).
		 */
		if (use_global && ob->id.lib == NULL) {
			/* We want to nuke the object, let's nuke it the easy way (not for linked data though)... */
			BKE_libblock_delete(bmain, &ob->id);
			changed = true;
			continue;
		}

		/* remove from Grease Pencil parent */
		/* XXX This is likely not correct? Will also remove parent from grease pencil from other scenes,
		 *     even when use_global is false... */
		for (bGPdata *gpd = bmain->gpencil.first; gpd; gpd = gpd->id.next) {
			for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
				if (gpl->parent != NULL) {
					if (gpl->parent == ob) {
						gpl->parent = NULL;
					}
				}
			}
		}

		/* remove from current scene only */
		ED_object_base_free_and_unlink(bmain, scene, ob);
		changed = true;

		if (use_global) {
			Scene *scene_iter;
			for (scene_iter = bmain->scene.first; scene_iter; scene_iter = scene_iter->id.next) {
				if (scene_iter != scene && !ID_IS_LINKED(scene_iter)) {
					if (is_indirectly_used && ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0) {
						BKE_reportf(op->reports, RPT_WARNING,
						            "Cannot delete object '%s' from scene '%s', indirectly used objects need at least one user",
						            ob->id.name + 2, scene_iter->id.name + 2);
						break;
					}
					ED_object_base_free_and_unlink(bmain, scene_iter, ob);
				}
			}
		}
		/* end global */
	}
	CTX_DATA_END;

	if (!changed)
		return OPERATOR_CANCELLED;

	/* delete has to handle all open scenes */
	BKE_main_id_tag_listbase(&bmain->scene, LIB_TAG_DOIT, true);
	for (win = wm->windows.first; win; win = win->next) {
		scene = WM_window_get_active_scene(win);

		if (scene->id.tag & LIB_TAG_DOIT) {
			scene->id.tag &= ~LIB_TAG_DOIT;

			DEG_relations_tag_update(bmain);

			DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
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
static void copy_object_set_idnew(bContext *C)
{
	Main *bmain = CTX_data_main(C);

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		BKE_libblock_relink_to_newid(&ob->id);
	}
	CTX_DATA_END;

	BKE_main_id_clear_newpoins(bmain);
}

/********************* Make Duplicates Real ************************/

/**
 * \note regarding hashing dupli-objects when using OB_DUPLICOLLECTION, skip the first member of #DupliObject.persistent_id
 * since its a unique index and we only want to know if the group objects are from the same dupli-group instance.
 */
static unsigned int dupliobject_group_hash(const void *ptr)
{
	const DupliObject *dob = ptr;
	unsigned int hash = BLI_ghashutil_ptrhash(dob->ob);
	unsigned int i;
	for (i = 1; (i < MAX_DUPLI_RECUR) && dob->persistent_id[i] != INT_MAX; i++) {
		hash ^= (dob->persistent_id[i] ^ i);
	}
	return hash;
}

/**
 * \note regarding hashing dupli-objects when NOT using OB_DUPLICOLLECTION, include the first member of #DupliObject.persistent_id
 * since its the index of the vertex/face the object is instantiated on and we want to identify objects on the same vertex/face.
 */
static unsigned int dupliobject_hash(const void *ptr)
{
	const DupliObject *dob = ptr;
	unsigned int hash = BLI_ghashutil_ptrhash(dob->ob);
	hash ^= (dob->persistent_id[0] ^ 0);
	return hash;
}

/* Compare function that matches dupliobject_group_hash */
static bool dupliobject_group_cmp(const void *a_, const void *b_)
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

/* Compare function that matches dupliobject_hash */
static bool dupliobject_cmp(const void *a_, const void *b_)
{
	const DupliObject *a = a_;
	const DupliObject *b = b_;

	if (a->ob != b->ob) {
		return true;
	}

	if (a->persistent_id[0] != b->persistent_id[0]) {
		return true;
	}

	/* matching */
	return false;
}

static void make_object_duplilist_real(bContext *C, Scene *scene, Base *base,
                                       const bool use_base_parent,
                                       const bool use_hierarchy)
{
	Main *bmain = CTX_data_main(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	ListBase *lb_duplis;
	DupliObject *dob;
	GHash *dupli_gh, *parent_gh = NULL;

	if (!(base->object->transflag & OB_DUPLI)) {
		return;
	}

	lb_duplis = object_duplilist(depsgraph, scene, base->object);

	dupli_gh = BLI_ghash_ptr_new(__func__);
	if (use_hierarchy) {
		if (base->object->transflag & OB_DUPLICOLLECTION) {
			parent_gh = BLI_ghash_new(dupliobject_group_hash, dupliobject_group_cmp, __func__);
		}
		else {
			parent_gh = BLI_ghash_new(dupliobject_hash, dupliobject_cmp, __func__);
		}
	}

	for (dob = lb_duplis->first; dob; dob = dob->next) {
		Object *ob_src = dob->ob;
		Object *ob_dst = ID_NEW_SET(dob->ob, BKE_object_copy(bmain, ob_src));
		Base *base_dst;

		/* font duplis can have a totcol without material, we get them from parent
		 * should be implemented better...
		 */
		if (ob_dst->mat == NULL) {
			ob_dst->totcol = 0;
		}

		BKE_collection_object_add_from(bmain, scene, base->object, ob_dst);
		base_dst = BKE_view_layer_base_find(view_layer, ob_dst);
		BLI_assert(base_dst != NULL);

		BKE_scene_object_base_flag_sync_from_base(base_dst);

		/* make sure apply works */
		BKE_animdata_free(&ob_dst->id, true);
		ob_dst->adt = NULL;

		/* Proxies are not to be copied. */
		ob_dst->proxy_from = NULL;
		ob_dst->proxy_group = NULL;
		ob_dst->proxy = NULL;

		ob_dst->parent = NULL;
		BKE_constraints_free(&ob_dst->constraints);
		ob_dst->runtime.curve_cache = NULL;
		ob_dst->transflag &= ~OB_DUPLI;

		copy_m4_m4(ob_dst->obmat, dob->mat);
		BKE_object_apply_mat4(ob_dst, ob_dst->obmat, false, false);

		BLI_ghash_insert(dupli_gh, dob, ob_dst);
		if (parent_gh) {
			void **val;
			/* Due to nature of hash/comparison of this ghash, a lot of duplis may be considered as 'the same',
			 * this avoids trying to insert same key several time and raise asserts in debug builds... */
			if (!BLI_ghash_ensure_p(parent_gh, dob, &val)) {
				*val = ob_dst;
			}
		}
	}

	for (dob = lb_duplis->first; dob; dob = dob->next) {
		Object *ob_src = dob->ob;
		Object *ob_dst = BLI_ghash_lookup(dupli_gh, dob);

		/* Remap new object to itself, and clear again newid pointer of orig object. */
		BKE_libblock_relink_to_newid(&ob_dst->id);

		DEG_id_tag_update(&ob_dst->id, OB_RECALC_DATA);

		if (use_hierarchy) {
			/* original parents */
			Object *ob_src_par = ob_src->parent;
			Object *ob_dst_par = NULL;

			/* find parent that was also made real */
			if (ob_src_par) {
				/* OK to keep most of the members uninitialized,
				 * they won't be read, this is simply for a hash lookup. */
				DupliObject dob_key;
				dob_key.ob = ob_src_par;
				if (base->object->transflag & OB_DUPLICOLLECTION) {
					memcpy(&dob_key.persistent_id[1],
					       &dob->persistent_id[1],
					       sizeof(dob->persistent_id[1]) * (MAX_DUPLI_RECUR - 1));
				}
				else {
					dob_key.persistent_id[0] = dob->persistent_id[0];
				}
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
		}
		else if (use_base_parent) {
			/* since we are ignoring the internal hierarchy - parent all to the
			 * base object */
			ob_dst->parent = base->object;
			ob_dst->partype = PAROBJECT;
		}

		if (ob_dst->parent) {
			/* note, this may be the parent of other objects, but it should
			 * still work out ok */
			BKE_object_apply_mat4(ob_dst, dob->mat, false, true);

			/* to set ob_dst->orig and in case theres any other discrepicies */
			DEG_id_tag_update(&ob_dst->id, OB_RECALC_OB);
		}
	}

	if (base->object->transflag & OB_DUPLICOLLECTION && base->object->dup_group) {
		for (Object *ob = bmain->object.first; ob; ob = ob->id.next) {
			if (ob->proxy_group == base->object) {
				ob->proxy = NULL;
				ob->proxy_from = NULL;
				DEG_id_tag_update(&ob->id, OB_RECALC_OB);
			}
		}
	}

	BLI_ghash_free(dupli_gh, NULL, NULL);
	if (parent_gh) {
		BLI_ghash_free(parent_gh, NULL, NULL);
	}

	free_object_duplilist(lb_duplis);

	BKE_main_id_clear_newpoins(bmain);

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

	DEG_relations_tag_update(bmain);
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

static const EnumPropertyItem convert_target_items[] = {
	{OB_CURVE, "CURVE", ICON_OUTLINER_OB_CURVE, "Curve from Mesh/Text", ""},
	{OB_MESH, "MESH", ICON_OUTLINER_OB_MESH, "Mesh from Curve/Meta/Surf/Text", ""},
	{0, NULL, 0, NULL, NULL}
};

static void convert_ensure_curve_cache(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
	if (ob->runtime.curve_cache == NULL) {
		/* Force creation. This is normally not needed but on operator
		 * redo we might end up with an object which isn't evaluated yet.
		 */
		if (ELEM(ob->type, OB_SURF, OB_CURVE, OB_FONT)) {
			BKE_displist_make_curveTypes(depsgraph, scene, ob, false);
		}
		else if (ob->type == OB_MBALL) {
			BKE_displist_make_mball(depsgraph, scene, ob);
		}
	}
}

static void curvetomesh(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
	convert_ensure_curve_cache(depsgraph, scene, ob);
	BKE_mesh_from_nurbs(bmain, ob); /* also does users */

	if (ob->type == OB_MESH) {
		BKE_object_free_modifiers(ob, 0);
	}
}

static bool convert_poll(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	Base *base_act = CTX_data_active_base(C);
	Object *obact = base_act ? base_act->object : NULL;

	return (!ID_IS_LINKED(scene) && obact && (BKE_object_is_in_editmode(obact) == false) &&
	        (base_act->flag & BASE_SELECTED) && !ID_IS_LINKED(obact));
}

/* Helper for convert_exec */
static Base *duplibase_for_convert(Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base, Object *ob)
{
	Object *obn;
	Base *basen;

	if (ob == NULL) {
		ob = base->object;
	}

	obn = BKE_object_copy(bmain, ob);
	DEG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
	BKE_collection_object_add_from(bmain, scene, ob, obn);

	basen = BKE_view_layer_base_find(view_layer, obn);
	ED_object_base_select(basen, BA_SELECT);
	ED_object_base_select(basen, BA_DESELECT);
	return basen;
}

static int convert_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Base *basen = NULL, *basact = NULL;
	Object *ob1, *obact = CTX_data_active_object(C);
	DerivedMesh *dm;
	Curve *cu;
	Nurb *nu;
	MetaBall *mb;
	Mesh *me;
	const short target = RNA_enum_get(op->ptr, "target");
	bool keep_original = RNA_boolean_get(op->ptr, "keep_original");
	int a, mballConverted = 0;

	/* don't forget multiple users! */

	{
		FOREACH_SCENE_OBJECT_BEGIN(scene, ob)
		{
			ob->flag &= ~OB_DONE;

			/* flag data thats not been edited (only needed for !keep_original) */
			if (ob->data) {
				((ID *)ob->data)->tag |= LIB_TAG_DOIT;
			}

			/* possible metaball basis is not in this scene */
			if (ob->type == OB_MBALL && target == OB_MESH) {
				if (BKE_mball_is_basis(ob) == false) {
					Object *ob_basis;
					ob_basis = BKE_mball_basis_find(scene, ob);
					if (ob_basis) {
						ob_basis->flag &= ~OB_DONE;
					}
				}
			}
		}
		FOREACH_SCENE_OBJECT_END;
	}

	ListBase selected_editable_bases = CTX_data_collection_get(C, "selected_editable_bases");

	/* Ensure we get all meshes calculated with a sufficient data-mask,
	 * needed since re-evaluating single modifiers causes bugs if they depend
	 * on other objects data masks too, see: T50950. */
	{
		for (CollectionPointerLink *link = selected_editable_bases.first; link; link = link->next) {
			Base *base = link->ptr.data;
			Object *ob = base->object;

			/* The way object type conversion works currently (enforcing conversion of *all* objetcs using converted
			 * obdata, even some un-selected/hidden/inother scene ones, sounds totally bad to me.
			 * However, changing this is more design than bugfix, not to mention convoluted code below,
			 * so that will be for later.
			 * But at the very least, do not do that with linked IDs! */
			if ((ID_IS_LINKED(ob) || (ob->data && ID_IS_LINKED(ob->data))) && !keep_original) {
				keep_original = true;
				BKE_reportf(op->reports, RPT_INFO,
				            "Converting some linked object/object data, enforcing 'Keep Original' option to True");
			}

			DEG_id_tag_update(&base->object->id, OB_RECALC_DATA);
		}

		uint64_t customdata_mask_prev = scene->customdata_mask;
		scene->customdata_mask |= CD_MASK_MESH;
		BKE_scene_graph_update_tagged(depsgraph, bmain);
		scene->customdata_mask = customdata_mask_prev;
	}

	for (CollectionPointerLink *link = selected_editable_bases.first; link; link = link->next) {
		Object *newob = NULL;
		Base *base = link->ptr.data;
		Object *ob = base->object;

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
					BKE_object_free_modifiers(ob, 0);  /* after derivedmesh calls! */
				}
				if (ob->type == OB_GPENCIL) {
					BKE_object_free_modifiers(ob, 0);  /* after derivedmesh calls! */
					BKE_object_free_shaderfx(ob, 0);
				}
			}
		}
		else if (ob->type == OB_MESH && target == OB_CURVE) {
			ob->flag |= OB_DONE;

			if (keep_original) {
				basen = duplibase_for_convert(bmain, scene, view_layer, base, NULL);
				newob = basen->object;

				/* decrement original mesh's usage count  */
				me = newob->data;
				id_us_min(&me->id);

				/* make a new copy of the mesh */
				newob->data = BKE_mesh_copy(bmain, me);
			}
			else {
				newob = ob;
			}

			BKE_mesh_to_curve(bmain, depsgraph, scene, newob);

			if (newob->type == OB_CURVE) {
				BKE_object_free_modifiers(newob, 0);   /* after derivedmesh calls! */
				ED_rigidbody_object_remove(bmain, scene, newob);
			}
		}
		else if (ob->type == OB_MESH) {
			ob->flag |= OB_DONE;

			if (keep_original) {
				basen = duplibase_for_convert(bmain, scene, view_layer, base, NULL);
				newob = basen->object;

				/* decrement original mesh's usage count  */
				me = newob->data;
				id_us_min(&me->id);

				/* make a new copy of the mesh */
				newob->data = BKE_mesh_copy(bmain, me);
			}
			else {
				newob = ob;
				DEG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
			}

			/* make new mesh data from the original copy */
			/* note: get the mesh from the original, not from the copy in some
			 * cases this doesnt give correct results (when MDEF is used for eg)
			 */
			dm = mesh_get_derived_final(depsgraph, scene, newob, CD_MASK_MESH);

			DM_to_mesh(dm, newob->data, newob, CD_MASK_MESH, true);

			/* re-tessellation is called by DM_to_mesh */

			BKE_object_free_modifiers(newob, 0);   /* after derivedmesh calls! */
		}
		else if (ob->type == OB_FONT) {
			ob->flag |= OB_DONE;

			if (keep_original) {
				basen = duplibase_for_convert(bmain, scene, view_layer, base, NULL);
				newob = basen->object;

				/* decrement original curve's usage count  */
				id_us_min(&((Curve *)newob->data)->id);

				/* make a new copy of the curve */
				newob->data = BKE_curve_copy(bmain, ob->data);
			}
			else {
				newob = ob;
			}

			cu = newob->data;

			/* TODO(sergey): Ideally DAG will create nurbs list for a curve data
			 *               datablock, but for until we've got granular update
			 *               lets take care by selves.
			 */
			BKE_vfont_to_curve(newob, FO_EDIT);

			newob->type = OB_CURVE;
			cu->type = OB_CURVE;

			if (cu->vfont) {
				id_us_min(&cu->vfont->id);
				cu->vfont = NULL;
			}
			if (cu->vfontb) {
				id_us_min(&cu->vfontb->id);
				cu->vfontb = NULL;
			}
			if (cu->vfonti) {
				id_us_min(&cu->vfonti->id);
				cu->vfonti = NULL;
			}
			if (cu->vfontbi) {
				id_us_min(&cu->vfontbi->id);
				cu->vfontbi = NULL;
			}

			if (!keep_original) {
				/* other users */
				if (cu->id.us > 1) {
					for (ob1 = bmain->object.first; ob1; ob1 = ob1->id.next) {
						if (ob1->data == ob->data) {
							ob1->type = OB_CURVE;
							DEG_id_tag_update(&ob1->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
						}
					}
				}
			}

			for (nu = cu->nurb.first; nu; nu = nu->next)
				nu->charidx = 0;

			cu->flag &= ~CU_3D;
			BKE_curve_curve_dimension_update(cu);

			if (target == OB_MESH) {
				curvetomesh(bmain, depsgraph, scene, newob);

				/* meshes doesn't use displist */
				BKE_object_free_curve_cache(newob);
			}
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
			ob->flag |= OB_DONE;

			if (target == OB_MESH) {
				if (keep_original) {
					basen = duplibase_for_convert(bmain, scene, view_layer, base, NULL);
					newob = basen->object;

					/* decrement original curve's usage count  */
					id_us_min(&((Curve *)newob->data)->id);

					/* make a new copy of the curve */
					newob->data = BKE_curve_copy(bmain, ob->data);
				}
				else {
					newob = ob;
				}

				curvetomesh(bmain, depsgraph, scene, newob);

				/* meshes doesn't use displist */
				BKE_object_free_curve_cache(newob);
			}
		}
		else if (ob->type == OB_MBALL && target == OB_MESH) {
			Object *baseob;

			base->flag &= ~BASE_SELECTED;
			ob->base_flag &= ~BASE_SELECTED;

			baseob = BKE_mball_basis_find(scene, ob);

			if (ob != baseob) {
				/* if motherball is converting it would be marked as done later */
				ob->flag |= OB_DONE;
			}

			if (!(baseob->flag & OB_DONE)) {
				baseob->flag |= OB_DONE;

				basen = duplibase_for_convert(bmain, scene, view_layer, base, baseob);
				newob = basen->object;

				mb = newob->data;
				id_us_min(&mb->id);

				newob->data = BKE_mesh_add(bmain, "Mesh");
				newob->type = OB_MESH;

				me = newob->data;
				me->totcol = mb->totcol;
				if (newob->totcol) {
					me->mat = MEM_dupallocN(mb->mat);
					for (a = 0; a < newob->totcol; a++) id_us_plus((ID *)me->mat[a]);
				}

				convert_ensure_curve_cache(depsgraph, scene, baseob);
				BKE_mesh_from_metaball(&baseob->runtime.curve_cache->disp, newob->data);

				if (obact->type == OB_MBALL) {
					basact = basen;
				}

				mballConverted = 1;
			}
		}
		else {
			continue;
		}

		/* Ensure new object has consistent material data with its new obdata. */
		if (newob) {
			test_object_materials(bmain, newob, newob->data);
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
			DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
			((ID *)ob->data)->tag &= ~LIB_TAG_DOIT; /* flag not to convert this datablock again */
		}
	}
	BLI_freelistN(&selected_editable_bases);

	if (!keep_original) {
		if (mballConverted) {
			FOREACH_SCENE_OBJECT_BEGIN(scene, ob_mball)
			{
				if (ob_mball->type == OB_MBALL) {
					if (ob_mball->flag & OB_DONE) {
						Object *ob_basis = NULL;
						if (BKE_mball_is_basis(ob_mball) ||
						    ((ob_basis = BKE_mball_basis_find(scene, ob_mball)) && (ob_basis->flag & OB_DONE)))
						{
							ED_object_base_free_and_unlink(bmain, scene, ob_mball);
						}
					}
				}
			}
			FOREACH_SCENE_OBJECT_END;
		}
	}

// XXX	ED_object_editmode_enter(C, 0);
// XXX	exit_editmode(C, EM_FREEDATA|EM_WAITCURSOR); /* freedata, but no undo */

	if (basact) {
		/* active base was changed */
		ED_object_base_activate(C, basact);
		BASACT(view_layer) = basact;
	}
	else if (BASACT(view_layer)->object->flag & OB_DONE) {
		WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, BASACT(view_layer)->object);
		WM_event_add_notifier(C, NC_OBJECT | ND_DATA, BASACT(view_layer)->object);
	}

	DEG_relations_tag_update(bmain);
	DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
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
/* Does set ID->newid pointers. */
static Base *object_add_duplicate_internal(Main *bmain, Scene *scene, ViewLayer *view_layer, Object *ob, int dupflag)
{
#define ID_NEW_REMAP_US(a)	if (      (a)->id.newid) { (a) = (void *)(a)->id.newid;       (a)->id.us++; }
#define ID_NEW_REMAP_US2(a)	if (((ID *)a)->newid)    { (a) = ((ID  *)a)->newid;     ((ID *)a)->us++;    }

	Base *base, *basen = NULL;
	Material ***matarar;
	Object *obn;
	ID *id;
	int a, didit;

	if (ob->mode & OB_MODE_POSE) {
		; /* nothing? */
	}
	else {
		obn = ID_NEW_SET(ob, BKE_object_copy(bmain, ob));
		DEG_id_tag_update(&obn->id, OB_RECALC_OB | OB_RECALC_DATA);

		base = BKE_view_layer_base_find(view_layer, ob);
		if ((base != NULL) && (base->flag & BASE_VISIBLE)) {
			BKE_collection_object_add_from(bmain, scene, ob, obn);
		}
		else {
			LayerCollection *layer_collection = BKE_layer_collection_get_active(view_layer);
			BKE_collection_object_add(bmain, layer_collection->collection, obn);
		}
		basen = BKE_view_layer_base_find(view_layer, obn);

		/* 1) duplis should end up in same collection as the original
		 * 2) Rigid Body sim participants MUST always be part of a collection...
		 */
		// XXX: is 2) really a good measure here?
		if (ob->rigidbody_object || ob->rigidbody_constraint) {
			Collection *collection;
			for (collection = bmain->collection.first; collection; collection = collection->id.next) {
				if (BKE_collection_has_object(collection, ob))
					BKE_collection_object_add(bmain, collection, obn);
			}
		}

		/* duplicates using userflags */
		if (dupflag & USER_DUP_ACT) {
			BKE_animdata_copy_id_action(bmain, &obn->id, true);
		}

		if (dupflag & USER_DUP_MAT) {
			for (a = 0; a < obn->totcol; a++) {
				id = (ID *)obn->mat[a];
				if (id) {
					ID_NEW_REMAP_US(obn->mat[a])
					else {
						obn->mat[a] = ID_NEW_SET(obn->mat[a], BKE_material_copy(bmain, obn->mat[a]));
						/* duplicate grease pencil settings */
						if (ob->mat[a]->gp_style) {
							obn->mat[a]->gp_style = MEM_dupallocN(ob->mat[a]->gp_style);
						}
					}
					id_us_min(id);

					if (dupflag & USER_DUP_ACT) {
						BKE_animdata_copy_id_action(bmain, &obn->mat[a]->id, true);
					}
				}
			}
		}
		if (dupflag & USER_DUP_PSYS) {
			ParticleSystem *psys;
			for (psys = obn->particlesystem.first; psys; psys = psys->next) {
				id = (ID *) psys->part;
				if (id) {
					ID_NEW_REMAP_US(psys->part)
					else {
						psys->part = ID_NEW_SET(psys->part, BKE_particlesettings_copy(bmain, psys->part));
					}

					if (dupflag & USER_DUP_ACT) {
						BKE_animdata_copy_id_action(bmain, &psys->part->id, true);
					}

					id_us_min(id);
				}
			}
		}

		id = obn->data;
		didit = 0;

		switch (obn->type) {
			case OB_MESH:
				if (dupflag & USER_DUP_MESH) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_mesh_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_CURVE:
				if (dupflag & USER_DUP_CURVE) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_curve_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_SURF:
				if (dupflag & USER_DUP_SURF) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_curve_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_FONT:
				if (dupflag & USER_DUP_FONT) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_curve_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_MBALL:
				if (dupflag & USER_DUP_MBALL) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_mball_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_LAMP:
				if (dupflag & USER_DUP_LAMP) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_lamp_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_ARMATURE:
				DEG_id_tag_update(&obn->id, OB_RECALC_DATA);
				if (obn->pose)
					BKE_pose_tag_recalc(bmain, obn->pose);
				if (dupflag & USER_DUP_ARM) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_armature_copy(bmain, obn->data));
						BKE_pose_rebuild(bmain, obn, obn->data, true);
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_LATTICE:
				if (dupflag != 0) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_lattice_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_CAMERA:
				if (dupflag != 0) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_camera_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_SPEAKER:
				if (dupflag != 0) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_speaker_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
			case OB_GPENCIL:
				if (dupflag != 0) {
					ID_NEW_REMAP_US2(obn->data)
					else {
						obn->data = ID_NEW_SET(obn->data, BKE_gpencil_copy(bmain, obn->data));
						didit = 1;
					}
					id_us_min(id);
				}
				break;
		}

		/* check if obdata is copied */
		if (didit) {
			Key *key = BKE_key_from_object(obn);

			Key *oldkey = BKE_key_from_object(ob);
			if (oldkey != NULL) {
				ID_NEW_SET(oldkey, key);
			}

			if (dupflag & USER_DUP_ACT) {
				BKE_animdata_copy_id_action(bmain, (ID *)obn->data, true);
				if (key) {
					BKE_animdata_copy_id_action(bmain, (ID *)key, true);
				}
			}

			if (dupflag & USER_DUP_MAT) {
				matarar = give_matarar(obn);
				if (matarar) {
					for (a = 0; a < obn->totcol; a++) {
						id = (ID *)(*matarar)[a];
						if (id) {
							ID_NEW_REMAP_US((*matarar)[a])
							else {
								(*matarar)[a] = ID_NEW_SET((*matarar)[a], BKE_material_copy(bmain, (*matarar)[a]));
							}
							id_us_min(id);
						}
					}
				}
			}
		}
	}
	return basen;

#undef ID_NEW_REMAP_US
#undef ID_NEW_REMAP_US2
}

/* single object duplicate, if dupflag==0, fully linked, else it uses the flags given */
/* leaves selection of base/object unaltered.
 * note: don't call this within a loop since clear_* funcs loop over the entire database.
 * note: caller must do DAG_relations_tag_update(bmain);
 *       this is not done automatic since we may duplicate many objects in a batch */
Base *ED_object_add_duplicate(Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base, int dupflag)
{
	Base *basen;
	Object *ob;

	basen = object_add_duplicate_internal(bmain, scene, view_layer, base->object, dupflag);
	if (basen == NULL) {
		return NULL;
	}

	ob = basen->object;

	/* link own references to the newly duplicated data [#26816] */
	BKE_libblock_relink_to_newid(&ob->id);

	/* DAG_relations_tag_update(bmain); */ /* caller must do */

	if (ob->data != NULL) {
		DEG_id_tag_update_ex(bmain, (ID *)ob->data, DEG_TAG_EDITORS_UPDATE);
	}

	BKE_main_id_clear_newpoins(bmain);

	return basen;
}

/* contextual operator dupli */
static int duplicate_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	const bool linked = RNA_boolean_get(op->ptr, "linked");
	int dupflag = (linked) ? 0 : U.dupflag;

	CTX_DATA_BEGIN (C, Base *, base, selected_bases)
	{
		Base *basen = object_add_duplicate_internal(bmain, scene, view_layer, base->object, dupflag);

		/* note that this is safe to do with this context iterator,
		 * the list is made in advance */
		ED_object_base_select(base, BA_DESELECT);
		ED_object_base_select(basen, BA_SELECT);

		if (basen == NULL) {
			continue;
		}

		/* new object becomes active */
		if (BASACT(view_layer) == base)
			ED_object_base_activate(C, basen);

		if (basen->object->data) {
			DEG_id_tag_update(basen->object->data, 0);
		}
	}
	CTX_DATA_END;

	copy_object_set_idnew(C);

	BKE_main_id_clear_newpoins(bmain);

	DEG_relations_tag_update(bmain);
	DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE | DEG_TAG_SELECT_UPDATE);

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
	prop = RNA_def_enum(ot->srna, "mode", rna_enum_transform_mode_types, TFM_TRANSLATION, "Mode", "");
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* **************** add named object, for dragdrop ************* */

static int add_named_exec(bContext *C, wmOperator *op)
{
	wmWindow *win = CTX_wm_window(C);
	const wmEvent *event = win ? win->eventstate : NULL;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Base *basen;
	Object *ob;
	const bool linked = RNA_boolean_get(op->ptr, "linked");
	int dupflag = (linked) ? 0 : U.dupflag;
	char name[MAX_ID_NAME - 2];

	/* find object, create fake base */
	RNA_string_get(op->ptr, "name", name);
	ob = (Object *)BKE_libblock_find_name(bmain, ID_OB, name);

	if (ob == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Object not found");
		return OPERATOR_CANCELLED;
	}

	/* prepare dupli */
	basen = object_add_duplicate_internal(bmain, scene, view_layer, ob, dupflag);

	if (basen == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Object could not be duplicated");
		return OPERATOR_CANCELLED;
	}

	BKE_scene_object_base_flag_sync_from_object(basen);
	basen->object->restrictflag &= ~OB_RESTRICT_VIEW;

	if (event) {
		ARegion *ar = CTX_wm_region(C);
		const int mval[2] = {event->x - ar->winrct.xmin,
		                     event->y - ar->winrct.ymin};
		ED_object_location_from_view(C, basen->object->loc);
		ED_view3d_cursor3d_position(C, mval, false, basen->object->loc);
	}

	ED_object_base_select(basen, BA_SELECT);
	ED_object_base_activate(C, basen);

	copy_object_set_idnew(C);

	BKE_main_id_clear_newpoins(bmain);

	/* TODO(sergey): Only update relations for the current scene. */
	DEG_relations_tag_update(bmain);

	DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

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

static bool join_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if (!ob || ID_IS_LINKED(ob)) return 0;

	if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_ARMATURE, OB_GPENCIL))
		return ED_operator_screenactive(C);
	else
		return 0;
}

static int join_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);

	if (ob->mode & OB_MODE_EDIT) {
		BKE_report(op->reports, RPT_ERROR, "This data does not support joining in edit mode");
		return OPERATOR_CANCELLED;
	}
	else if (BKE_object_obdata_is_libdata(ob)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot edit external libdata");
		return OPERATOR_CANCELLED;
	}
	else if (ob->type == OB_GPENCIL) {
		bGPdata *gpd = (bGPdata *)ob->data;
		if ((!gpd) || GPENCIL_ANY_MODE(gpd)) {
			BKE_report(op->reports, RPT_ERROR, "This data does not support joining in this mode");
			return OPERATOR_CANCELLED;
		}
	}

	if (ob->type == OB_MESH)
		return join_mesh_exec(C, op);
	else if (ELEM(ob->type, OB_CURVE, OB_SURF))
		return join_curve_exec(C, op);
	else if (ob->type == OB_ARMATURE)
		return join_armature_exec(C, op);
	else if (ob->type == OB_GPENCIL)
		return ED_gpencil_join_objects_exec(C, op);

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

static bool join_shapes_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if (!ob || ID_IS_LINKED(ob)) return 0;

	/* only meshes supported at the moment */
	if (ob->type == OB_MESH)
		return ED_operator_screenactive(C);
	else
		return 0;
}

static int join_shapes_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);

	if (ob->mode & OB_MODE_EDIT) {
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
