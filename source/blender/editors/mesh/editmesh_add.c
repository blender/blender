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

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_library.h"
#include "BKE_tessmesh.h"


#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_object.h"

#include "mesh_intern.h"

/* ********* add primitive operators ************* */

static void make_prim_init(bContext *C, const char *idname,
                           float *dia, float mat[][4],
                           int *state, const float loc[3], const float rot[3], const unsigned int layer)
{
	Object *obedit = CTX_data_edit_object(C);

	*state = 0;
	if (obedit == NULL || obedit->type != OB_MESH) {
		obedit = ED_object_add_type(C, OB_MESH, loc, rot, FALSE, layer);
		
		rename_id((ID *)obedit, idname);
		rename_id((ID *)obedit->data, idname);

		/* create editmode */
		ED_object_enter_editmode(C, EM_DO_UNDO | EM_IGNORE_LAYER); /* rare cases the active layer is messed up */
		*state = 1;
	}

	*dia = ED_object_new_primitive_matrix(C, obedit, loc, rot, mat);
}

static void make_prim_finish(bContext *C, int *state, int enter_editmode)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);

	/* Primitive has all verts selected, use vert select flush
	 * to push this up to edges & faces. */
	EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

	EDBM_update_generic(C, em, TRUE);

	/* userdef */
	if (*state && !enter_editmode) {
		ED_object_exit_editmode(C, EM_FREEDATA); /* adding EM_DO_UNDO messes up operator redo */
	}
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);
}

static int add_primitive_plane_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state;
	unsigned int layer;
	
	ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, NULL);
	make_prim_init(C, "Plane", &dia, mat, &state, loc, rot, layer);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_op_call_and_selectf(em, op, "vertout",
	                              "create_grid xsegments=%i ysegments=%i size=%f mat=%m4", 1, 1, dia, mat))
	{
		return OPERATOR_CANCELLED;
	}

	make_prim_finish(C, &state, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_plane_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Plane";
	ot->description = "Construct a filled planar mesh with 4 vertices";
	ot->idname = "MESH_OT_primitive_plane_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_plane_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_cube_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state;
	unsigned int layer;
	
	ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, NULL);
	make_prim_init(C, "Cube", &dia, mat, &state, loc, rot, layer);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_op_call_and_selectf(em, op, "vertout", "create_cube mat=%m4 size=%f", mat, dia * 2.0f)) {
		return OPERATOR_CANCELLED;
	}
	
	/* BMESH_TODO make plane side this: M_SQRT2 - plane (diameter of 1.41 makes it unit size) */
	make_prim_finish(C, &state, enter_editmode);

	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cube_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Cube";
	ot->description = "Construct a cube mesh";
	ot->idname = "MESH_OT_primitive_cube_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_cube_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static const EnumPropertyItem fill_type_items[] = {
	{0, "NOTHING", 0, "Nothing", "Don't fill at all"},
	{1, "NGON", 0, "Ngon", "Use ngons"},
	{2, "TRIFAN", 0, "Triangle Fan", "Use triangle fans"},
	{0, NULL, 0, NULL, NULL}};

static int add_primitive_circle_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state, cap_end, cap_tri;
	unsigned int layer;
	
	cap_end = RNA_enum_get(op->ptr, "fill_type");
	cap_tri = (cap_end == 2);
	
	ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, NULL);
	make_prim_init(C, "Circle", &dia, mat, &state, loc, rot, layer);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_op_call_and_selectf(em, op, "vertout",
	                              "create_circle segments=%i diameter=%f cap_ends=%b cap_tris=%b mat=%m4",
	                              RNA_int_get(op->ptr, "vertices"), RNA_float_get(op->ptr, "radius"),
	                              cap_end, cap_tri, mat))
	{
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_circle_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Circle";
	ot->description = "Construct a circle mesh";
	ot->idname = "MESH_OT_primitive_circle_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_circle_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "vertices", 32, 3, INT_MAX, "Vertices", "", 3, 500);
	prop = RNA_def_float(ot->srna, "radius", 1.0f, 0.0, FLT_MAX, "Radius", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	RNA_def_enum(ot->srna, "fill_type", fill_type_items, 0, "Fill Type", "");

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_cylinder_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state, cap_end, cap_tri;
	unsigned int layer;
	
	cap_end = RNA_enum_get(op->ptr, "end_fill_type");
	cap_tri = (cap_end == 2);
	
	ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, NULL);
	make_prim_init(C, "Cylinder", &dia, mat, &state, loc, rot, layer);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_op_call_and_selectf(
	        em, op, "vertout",
	        "create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%b cap_tris=%b depth=%f mat=%m4",
	        RNA_int_get(op->ptr, "vertices"),
	        RNA_float_get(op->ptr, "radius"),
	        RNA_float_get(op->ptr, "radius"),
	        cap_end, cap_tri,
	        RNA_float_get(op->ptr, "depth"), mat))
	{
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);
	
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
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_cylinder_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "vertices", 32, 3, INT_MAX, "Vertices", "", 3, 500);
	prop = RNA_def_float(ot->srna, "radius", 1.0f, 0.0, FLT_MAX, "Radius", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	prop = RNA_def_float(ot->srna, "depth", 2.0f, 0.0, FLT_MAX, "Depth", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);
	RNA_def_enum(ot->srna, "end_fill_type", fill_type_items, 1, "Cap Fill Type", "");

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_cone_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state, cap_end, cap_tri;
	unsigned int layer;
	
	cap_end = RNA_enum_get(op->ptr, "end_fill_type");
	cap_tri = (cap_end == 2);
	
	ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, NULL);
	make_prim_init(C, "Cone", &dia, mat, &state, loc, rot, layer);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_op_call_and_selectf(
	        em, op, "vertout",
	        "create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%b cap_tris=%b depth=%f mat=%m4",
	        RNA_int_get(op->ptr, "vertices"), RNA_float_get(op->ptr, "radius1"),
	        RNA_float_get(op->ptr, "radius2"), cap_end, cap_tri, RNA_float_get(op->ptr, "depth"), mat))
	{
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);

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
	ot->invoke = ED_object_add_generic_invoke;
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

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_grid_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state;
	unsigned int layer;
	
	ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, NULL);
	make_prim_init(C, "Grid", &dia, mat, &state, loc, rot, layer);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_op_call_and_selectf(em, op, "vertout",
	                              "create_grid xsegments=%i ysegments=%i size=%f mat=%m4",
	                              RNA_int_get(op->ptr, "x_subdivisions"),
	                              RNA_int_get(op->ptr, "y_subdivisions"),
	                              RNA_float_get(op->ptr, "size") * dia, mat))
	{
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);
	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_grid_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Grid";
	ot->description = "Construct a grid mesh";
	ot->idname = "MESH_OT_primitive_grid_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_grid_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "x_subdivisions", 10, 3, INT_MAX, "X Subdivisions", "", 3, 1000);
	RNA_def_int(ot->srna, "y_subdivisions", 10, 3, INT_MAX, "Y Subdivisions", "", 3, 1000);
	prop = RNA_def_float(ot->srna, "size", 1.0f, 0.0, FLT_MAX, "Size", "", 0.001, FLT_MAX);
	RNA_def_property_subtype(prop, PROP_DISTANCE);

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_monkey_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state, view_aligned;
	unsigned int layer;
	
	ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, &view_aligned);
	if (!view_aligned)
		rot[0] += (float)M_PI / 2.0f;
	
	make_prim_init(C, "Monkey", &dia, mat, &state, loc, rot, layer);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_op_call_and_selectf(em, op, "vertout", "create_monkey mat=%m4", mat)) {
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);
	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_monkey_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Monkey";
	ot->description = "Construct a Suzanne mesh";
	ot->idname = "MESH_OT_primitive_monkey_add";
	
	/* api callbacks */
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_monkey_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_uvsphere_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state;
	unsigned int layer;
	
	ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, NULL);
	make_prim_init(C, "Sphere", &dia, mat, &state, loc, rot, layer);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_op_call_and_selectf(em, op, "vertout",
	                              "create_uvsphere segments=%i revolutions=%i diameter=%f mat=%m4",
	                              RNA_int_get(op->ptr, "segments"), RNA_int_get(op->ptr, "ring_count"),
	                              RNA_float_get(op->ptr, "size"), mat))
	{
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);

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
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_uvsphere_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "segments", 32, 3, INT_MAX, "Segments", "", 3, 500);
	RNA_def_int(ot->srna, "ring_count", 16, 3, INT_MAX, "Rings", "", 3, 500);
	prop = RNA_def_float(ot->srna, "size", 1.0f, 0.0, FLT_MAX, "Size", "", 0.001, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_icosphere_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state;
	unsigned int layer;
	
	ED_object_add_generic_get_opts(C, op, loc, rot, &enter_editmode, &layer, NULL);
	make_prim_init(C, "Icosphere", &dia, mat, &state, loc, rot, layer);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_op_call_and_selectf(
	        em, op, "vertout",
	        "create_icosphere subdivisions=%i diameter=%f mat=%m4",
	        RNA_int_get(op->ptr, "subdivisions"),
	        RNA_float_get(op->ptr, "size"), mat))
	{
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);

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
	ot->invoke = ED_object_add_generic_invoke;
	ot->exec = add_primitive_icosphere_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "subdivisions", 2, 1, INT_MAX, "Subdivisions", "", 1, 8);
	prop = RNA_def_float(ot->srna, "size", 1.0f, 0.0f, FLT_MAX, "Size", "", 0.001f, 100.00);
	RNA_def_property_subtype(prop, PROP_DISTANCE);

	ED_object_add_generic_props(ot, TRUE);
}
