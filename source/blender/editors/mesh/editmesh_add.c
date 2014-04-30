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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_add.c
 *  \ingroup edmesh
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"


#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_editmesh.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_object.h"

#include "mesh_intern.h"  /* own include */

/* ********* add primitive operators ************* */

static Object *make_prim_init(bContext *C, const char *idname,
                              float *dia, float mat[4][4],
                              bool *was_editmode, const float loc[3], const float rot[3], const unsigned int layer)
{
	Object *obedit = CTX_data_edit_object(C);

	*was_editmode = false;
	if (obedit == NULL || obedit->type != OB_MESH) {
		obedit = ED_object_add_type(C, OB_MESH, loc, rot, false, layer);

		rename_id((ID *)obedit, idname);
		rename_id((ID *)obedit->data, idname);

		/* create editmode */
		ED_object_editmode_enter(C, EM_DO_UNDO | EM_IGNORE_LAYER); /* rare cases the active layer is messed up */
		*was_editmode = true;
	}

	*dia = ED_object_new_primitive_matrix(C, obedit, loc, rot, mat, false);

	return obedit;
}

static void make_prim_finish(bContext *C, Object *obedit, bool was_editmode, int enter_editmode)
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	const bool exit_editmode = ((was_editmode == true) && (enter_editmode == false));

	/* Primitive has all verts selected, use vert select flush
	 * to push this up to edges & faces. */
	EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

	/* only recalc editmode tessface if we are staying in editmode */
	EDBM_update_generic(em, !exit_editmode, true);

	/* userdef */
	if (exit_editmode) {
		ED_object_editmode_exit(C, EM_FREEDATA); /* adding EM_DO_UNDO messes up operator redo */
	}
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);
}

static int add_primitive_plane_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	bool was_editmode;
	unsigned int layer;

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	obedit = make_prim_init(C, CTX_DATA_(BLF_I18NCONTEXT_ID_MESH, "Plane"), &dia, mat, &was_editmode, loc, rot, layer);
	em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out", false,
	        "create_grid x_segments=%i y_segments=%i size=%f matrix=%m4",
	        1, 1, RNA_float_get(op->ptr, "radius"), mat))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, was_editmode, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_plane_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Plane";
	ot->description = "Construct a filled planar mesh with 4 vertices";
	ot->idname = "MESH_OT_primitive_plane_add";

	/* api callbacks */
	ot->exec = add_primitive_plane_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_cube_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	bool was_editmode;
	unsigned int layer;

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	obedit = make_prim_init(C, CTX_DATA_(BLF_I18NCONTEXT_ID_MESH, "Cube"), &dia, mat, &was_editmode, loc, rot, layer);
	em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out", false,
	        "create_cube matrix=%m4 size=%f",
	        mat, RNA_float_get(op->ptr, "radius") * 2.0f))
	{
		return OPERATOR_CANCELLED;
	}

	/* BMESH_TODO make plane side this: M_SQRT2 - plane (diameter of 1.41 makes it unit size) */
	make_prim_finish(C, obedit, was_editmode, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cube_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Cube";
	ot->description = "Construct a cube mesh";
	ot->idname = "MESH_OT_primitive_cube_add";

	/* api callbacks */
	ot->exec = add_primitive_cube_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static const EnumPropertyItem fill_type_items[] = {
	{0, "NOTHING", 0, "Nothing", "Don't fill at all"},
	{1, "NGON", 0, "Ngon", "Use ngons"},
	{2, "TRIFAN", 0, "Triangle Fan", "Use triangle fans"},
	{0, NULL, 0, NULL, NULL}};

static int add_primitive_circle_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	int cap_end, cap_tri;
	unsigned int layer;
	bool was_editmode;

	cap_end = RNA_enum_get(op->ptr, "fill_type");
	cap_tri = (cap_end == 2);

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	obedit = make_prim_init(C, CTX_DATA_(BLF_I18NCONTEXT_ID_MESH, "Circle"), &dia, mat, &was_editmode, loc, rot, layer);
	em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out", false,
	        "create_circle segments=%i diameter=%f cap_ends=%b cap_tris=%b matrix=%m4",
	        RNA_int_get(op->ptr, "vertices"), RNA_float_get(op->ptr, "radius"),
	        cap_end, cap_tri, mat))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, was_editmode, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_circle_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Circle";
	ot->description = "Construct a circle mesh";
	ot->idname = "MESH_OT_primitive_circle_add";

	/* api callbacks */
	ot->exec = add_primitive_circle_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "vertices", 32, 3, INT_MAX, "Vertices", "", 3, 500);
	ED_object_add_unit_props(ot);
	RNA_def_enum(ot->srna, "fill_type", fill_type_items, 0, "Fill Type", "");

	ED_object_add_generic_props(ot, true);
}

static int add_primitive_cylinder_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	unsigned int layer;
	bool was_editmode;
	const int end_fill_type = RNA_enum_get(op->ptr, "end_fill_type");
	const bool cap_end = (end_fill_type != 0);
	const bool cap_tri = (end_fill_type == 2);

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	obedit = make_prim_init(C, CTX_DATA_(BLF_I18NCONTEXT_ID_MESH, "Cylinder"), &dia, mat, &was_editmode, loc, rot, layer);
	em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out", false,
	        "create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%b cap_tris=%b depth=%f matrix=%m4",
	        RNA_int_get(op->ptr, "vertices"),
	        RNA_float_get(op->ptr, "radius"),
	        RNA_float_get(op->ptr, "radius"),
	        cap_end, cap_tri,
	        RNA_float_get(op->ptr, "depth"), mat))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, was_editmode, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cylinder_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Cylinder";
	ot->description = "Construct a cylinder mesh";
	ot->idname = "MESH_OT_primitive_cylinder_add";

	/* api callbacks */
	ot->exec = add_primitive_cylinder_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "vertices", 32, 3, INT_MAX, "Vertices", "", 3, 500);
	ED_object_add_unit_props(ot);
	prop = RNA_def_float(ot->srna, "depth", 2.0f, 0.0, FLT_MAX, "Depth", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	RNA_def_enum(ot->srna, "end_fill_type", fill_type_items, 1, "Cap Fill Type", "");

	ED_object_add_generic_props(ot, true);
}

static int add_primitive_cone_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	unsigned int layer;
	bool was_editmode;
	const int end_fill_type = RNA_enum_get(op->ptr, "end_fill_type");
	const bool cap_end = (end_fill_type != 0);
	const bool cap_tri = (end_fill_type == 2);

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	obedit = make_prim_init(C, CTX_DATA_(BLF_I18NCONTEXT_ID_MESH, "Cone"), &dia, mat, &was_editmode, loc, rot, layer);
	em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out", false,
	        "create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%b cap_tris=%b depth=%f matrix=%m4",
	        RNA_int_get(op->ptr, "vertices"), RNA_float_get(op->ptr, "radius1"),
	        RNA_float_get(op->ptr, "radius2"), cap_end, cap_tri, RNA_float_get(op->ptr, "depth"), mat))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, was_editmode, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cone_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Cone";
	ot->description = "Construct a conic mesh";
	ot->idname = "MESH_OT_primitive_cone_add";

	/* api callbacks */
	ot->exec = add_primitive_cone_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "vertices", 32, 3, INT_MAX, "Vertices", "", 3, 500);
	prop = RNA_def_float(ot->srna, "radius1", 1.0f, 0.0, FLT_MAX, "Radius 1", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	prop = RNA_def_float(ot->srna, "radius2", 0.0f, 0.0, FLT_MAX, "Radius 2", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	prop = RNA_def_float(ot->srna, "depth", 2.0f, 0.0, FLT_MAX, "Depth", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	RNA_def_enum(ot->srna, "end_fill_type", fill_type_items, 1, "Base Fill Type", "");

	ED_object_add_generic_props(ot, true);
}

static int add_primitive_grid_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	bool was_editmode;
	unsigned int layer;

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	obedit = make_prim_init(C, CTX_DATA_(BLF_I18NCONTEXT_ID_MESH, "Grid"), &dia, mat, &was_editmode, loc, rot, layer);
	em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out", false,
	        "create_grid x_segments=%i y_segments=%i size=%f matrix=%m4",
	        RNA_int_get(op->ptr, "x_subdivisions"),
	        RNA_int_get(op->ptr, "y_subdivisions"),
	        RNA_float_get(op->ptr, "radius"), mat))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, was_editmode, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_grid_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Grid";
	ot->description = "Construct a grid mesh";
	ot->idname = "MESH_OT_primitive_grid_add";

	/* api callbacks */
	ot->exec = add_primitive_grid_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "x_subdivisions", 10, 2, INT_MAX, "X Subdivisions", "", 2, 1000);
	RNA_def_int(ot->srna, "y_subdivisions", 10, 2, INT_MAX, "Y Subdivisions", "", 2, 1000);
	ED_object_add_unit_props(ot);

	ED_object_add_generic_props(ot, true);
}

static int add_primitive_monkey_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	unsigned int layer;
	bool was_editmode;

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Y', loc, rot, &enter_editmode, &layer, NULL);

	obedit = make_prim_init(C, CTX_DATA_(BLF_I18NCONTEXT_ID_MESH, "Suzanne"), &dia, mat, &was_editmode, loc, rot, layer);
	dia = RNA_float_get(op->ptr, "radius");
	mat[0][0] *= dia;
	mat[1][1] *= dia;
	mat[2][2] *= dia;

	em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out",  false,
	        "create_monkey matrix=%m4", mat))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, was_editmode, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_monkey_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Monkey";
	ot->description = "Construct a Suzanne mesh";
	ot->idname = "MESH_OT_primitive_monkey_add";

	/* api callbacks */
	ot->exec = add_primitive_monkey_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ED_object_add_unit_props(ot);
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, true);
}

static int add_primitive_uvsphere_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	bool was_editmode;
	unsigned int layer;

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	obedit = make_prim_init(C, CTX_DATA_(BLF_I18NCONTEXT_ID_MESH, "Sphere"), &dia, mat, &was_editmode, loc, rot, layer);
	em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out", false,
	        "create_uvsphere u_segments=%i v_segments=%i diameter=%f matrix=%m4",
	        RNA_int_get(op->ptr, "segments"), RNA_int_get(op->ptr, "ring_count"),
	        RNA_float_get(op->ptr, "size"), mat))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, was_editmode, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_uv_sphere_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add UV Sphere";
	ot->description = "Construct a UV sphere mesh";
	ot->idname = "MESH_OT_primitive_uv_sphere_add";

	/* api callbacks */
	ot->exec = add_primitive_uvsphere_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "segments", 32, 3, INT_MAX, "Segments", "", 3, 500);
	RNA_def_int(ot->srna, "ring_count", 16, 3, INT_MAX, "Rings", "", 3, 500);
	prop = RNA_def_float(ot->srna, "size", 1.0f, 0.0, FLT_MAX, "Size", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);

	ED_object_add_generic_props(ot, true);
}

static int add_primitive_icosphere_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	bool was_editmode;
	unsigned int layer;

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	obedit = make_prim_init(C, CTX_DATA_(BLF_I18NCONTEXT_ID_MESH, "Icosphere"), &dia, mat, &was_editmode, loc, rot, layer);
	em = BKE_editmesh_from_object(obedit);

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out", false,
	        "create_icosphere subdivisions=%i diameter=%f matrix=%m4",
	        RNA_int_get(op->ptr, "subdivisions"),
	        RNA_float_get(op->ptr, "size"), mat))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, obedit, was_editmode, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_ico_sphere_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Ico Sphere";
	ot->description = "Construct an Icosphere mesh";
	ot->idname = "MESH_OT_primitive_ico_sphere_add";

	/* api callbacks */
	ot->exec = add_primitive_icosphere_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "subdivisions", 2, 1, INT_MAX, "Subdivisions", "", 1, 8);
	prop = RNA_def_float(ot->srna, "size", 1.0f, 0.0f, FLT_MAX, "Size", "", 0.001f, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);

	ED_object_add_generic_props(ot, true);
}
