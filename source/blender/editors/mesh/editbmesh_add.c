 /* $Id: bmesh_tools.c
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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_key_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_types.h"
#include "RNA_define.h"
#include "RNA_access.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_heap.h"
#include "BLI_array.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_object.h"

#include "UI_interface.h"

#include "mesh_intern.h"
#include "bmesh.h"

#include "editbmesh_bvh.h"


/* uses context to figure out transform for primitive */
/* returns standard diameter */
static float new_primitive_matrix(bContext *C, float *loc, float *rot, float primmat[][4])
{
	Object *obedit= CTX_data_edit_object(C);
	View3D *v3d =CTX_wm_view3d(C);
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
	VECCOPY(primmat[3], loc);
	VECSUB(primmat[3], primmat[3], obedit->obmat[3]);
	invert_m3_m3(imat, mat);
	mul_m3_v3(imat, primmat[3]);
	
	if(v3d) return v3d->grid;
	return 1.0f;
}

/* ********* add primitive operators ************* */

static void make_prim_init(bContext *C, float *dia, float mat[][4], 
						   int *state, float *loc, float *rot)
{
	Object *obedit= CTX_data_edit_object(C);

	*state = 0;
	if(obedit==NULL || obedit->type!=OB_MESH) {
		obedit= ED_object_add_type(C, OB_MESH, loc, rot, FALSE);
		
		/* create editmode */
		ED_object_enter_editmode(C, EM_DO_UNDO|EM_IGNORE_LAYER); /* rare cases the active layer is messed up */
		*state = 1;
	}
	else DAG_id_flush_update(&obedit->id, OB_RECALC_DATA);

	*dia *= new_primitive_matrix(C, loc, rot, mat);
}

static void make_prim_finish(bContext *C, int *state, int enter_editmode)
{
	Object *obedit = CTX_data_edit_object(C);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	/* userdef */
	if (*state && !enter_editmode) {
		ED_object_exit_editmode(C, EM_FREEDATA); /* adding EM_DO_UNDO messes up operator redo */
	}
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, obedit);

}
static int add_primitive_plane_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state;
	
	ED_object_add_generic_get_opts(op, loc, rot, &enter_editmode);
	make_prim_init(C, &dia, mat, &state, loc, rot);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_CallAndSelectOpf(em, op, "vertout", 
			"create_grid xsegments=%i ysegments=%i size=%f mat=%m4", 1, 1, sqrt(2.0), mat)) 
		return OPERATOR_CANCELLED;
	
	/* BMESH_TODO make plane side this: sqrt(2.0f) - plane (diameter of 1.41 makes it unit size) */
	make_prim_finish(C, &state, enter_editmode);

	return OPERATOR_FINISHED;	
}

void MESH_OT_primitive_plane_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Plane";
	ot->description= "Construct a filled planar mesh with 4 vertices.";
	ot->idname= "MESH_OT_primitive_plane_add";
	
	/* api callbacks */
	ot->invoke= ED_object_add_generic_invoke;
	ot->exec= add_primitive_plane_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

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

	ED_object_add_generic_get_opts(op, loc, rot, &enter_editmode);

	make_prim_init(C, &dia, mat, &state, loc, rot);

	obedit= CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_CallAndSelectOpf(em, op, "vertout", "create_cube mat=%m4 size=%f", mat, 2.0f)) 
		return OPERATOR_CANCELLED;
	
	/* BMESH_TODO make plane side this: sqrt(2.0f) - plane (diameter of 1.41 makes it unit size) */
	make_prim_finish(C, &state, enter_editmode);

	return OPERATOR_FINISHED;	
}

void MESH_OT_primitive_cube_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Cube";
	ot->description= "Construct a cube mesh.";
	ot->idname= "MESH_OT_primitive_cube_add";
	
	/* api callbacks */
	ot->invoke= ED_object_add_generic_invoke;
	ot->exec= add_primitive_cube_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_circle_exec(bContext *C, wmOperator *op)
{
#if 0
	int enter_editmode;
	float loc[3], rot[3];
	
	ED_object_add_generic_get_opts(op, loc, rot, &enter_editmode);

	make_prim_ext(C, loc, rot, enter_editmode,
			PRIM_CIRCLE, RNA_int_get(op->ptr, "vertices"), 0, 0,
			RNA_float_get(op->ptr,"radius"), 0.0f, 0,
			RNA_boolean_get(op->ptr, "fill"));
#endif
	return OPERATOR_FINISHED;	
}

void MESH_OT_primitive_circle_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Circle";
	ot->description= "Construct a circle mesh.";
	ot->idname= "MESH_OT_primitive_circle_add";
	
	/* api callbacks */
	ot->invoke= ED_object_add_generic_invoke;
	ot->exec= add_primitive_circle_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "vertices", 32, INT_MIN, INT_MAX, "Vertices", "", 3, 500);
	RNA_def_float(ot->srna, "radius", 1.0f, 0.0, FLT_MAX, "Radius", "", 0.001, 100.00);
	RNA_def_boolean(ot->srna, "fill", 0, "Fill", "");

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_tube_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state;
	
	ED_object_add_generic_get_opts(op, loc, rot, &enter_editmode);
	make_prim_init(C, &dia, mat, &state, loc, rot);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_CallAndSelectOpf(em, op, "vertout", 
			"create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%i depth=%f mat=%m4", 
			RNA_int_get(op->ptr, "vertices"), RNA_float_get(op->ptr, "radius"), 
			RNA_float_get(op->ptr, "radius"), (int)RNA_boolean_get(op->ptr, "cap_end"), RNA_float_get(op->ptr, "depth"), mat))
		return OPERATOR_CANCELLED;
	
	make_prim_finish(C, &state, enter_editmode);
}

void MESH_OT_primitive_tube_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Tube";
	ot->description= "Construct a tube mesh.";
	ot->idname= "MESH_OT_primitive_tube_add";
	
	/* api callbacks */
	ot->invoke= ED_object_add_generic_invoke;
	ot->exec= add_primitive_tube_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "vertices", 32, INT_MIN, INT_MAX, "Vertices", "", 2, 500);
	RNA_def_float(ot->srna, "radius", 1.0f, 0.0, FLT_MAX, "Radius", "", 0.001, 100.00);
	RNA_def_float(ot->srna, "depth", 1.0f, 0.0, FLT_MAX, "Depth", "", 0.001, 100.00);
	RNA_def_boolean(ot->srna, "cap_ends", 1, "Cap Ends", "");

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_cone_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state;
	
	ED_object_add_generic_get_opts(op, loc, rot, &enter_editmode);
	make_prim_init(C, &dia, mat, &state, loc, rot);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_CallAndSelectOpf(em, op, "vertout", 
			"create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%i depth=%f mat=%m4", 
			RNA_int_get(op->ptr, "vertices"), RNA_float_get(op->ptr, "radius"), 
			0.0f, (int)RNA_boolean_get(op->ptr, "cap_end"), RNA_float_get(op->ptr, "depth"), mat))
		return OPERATOR_CANCELLED;
	
	make_prim_finish(C, &state, enter_editmode);

	return OPERATOR_FINISHED;	
}

void MESH_OT_primitive_cone_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Cone";
	ot->description= "Construct a conic mesh (ends filled).";
	ot->idname= "MESH_OT_primitive_cone_add";
	
	/* api callbacks */
	ot->invoke= ED_object_add_generic_invoke;
	ot->exec= add_primitive_cone_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "vertices", 32, INT_MIN, INT_MAX, "Vertices", "", 2, 500);
	RNA_def_float(ot->srna, "radius", 1.0f, 0.0, FLT_MAX, "Radius", "", 0.001, 100.00);
	RNA_def_float(ot->srna, "depth", 1.0f, 0.0, FLT_MAX, "Depth", "", 0.001, 100.00);
	RNA_def_boolean(ot->srna, "cap_end", 0, "Cap End", "");

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
	
	ED_object_add_generic_get_opts(op, loc, rot, &enter_editmode);
	make_prim_init(C, &dia, mat, &state, loc, rot);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_CallAndSelectOpf(em, op, "vertout", 
			"create_grid xsegments=%i ysegments=%i size=%f mat=%m4",
			RNA_int_get(op->ptr, "x_subdivisions"), 
			RNA_int_get(op->ptr, "y_subdivisions"), 
			RNA_float_get(op->ptr, "size"), mat))
	{
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);
	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_grid_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Grid";
	ot->description= "Construct a grid mesh.";
	ot->idname= "MESH_OT_primitive_grid_add";
	
	/* api callbacks */
	ot->invoke= ED_object_add_generic_invoke;
	ot->exec= add_primitive_grid_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "x_subdivisions", 10, INT_MIN, INT_MAX, "X Subdivisions", "", 3, 1000);
	RNA_def_int(ot->srna, "y_subdivisions", 10, INT_MIN, INT_MAX, "Y Subdivisions", "", 3, 1000);
	RNA_def_float(ot->srna, "size", 1.0f, 0.0, FLT_MAX, "Size", "", 0.001, FLT_MAX);

	ED_object_add_generic_props(ot, TRUE);
}

static int add_primitive_monkey_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	Mesh *me;
	BMEditMesh *em;
	float loc[3], rot[3], mat[4][4], dia;
	int enter_editmode;
	int state;
	
	ED_object_add_generic_get_opts(op, loc, rot, &enter_editmode);
	make_prim_init(C, &dia, mat, &state, loc, rot);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_CallAndSelectOpf(em, op, "vertout", "create_monkey mat=%m4", mat)) {
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);
	return OPERATOR_FINISHED;
}

void MESH_OT_primitive_monkey_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Monkey";
	ot->description= "Construct a Suzanne mesh.";
	ot->idname= "MESH_OT_primitive_monkey_add";
	
	/* api callbacks */
	ot->invoke= ED_object_add_generic_invoke;
	ot->exec= add_primitive_monkey_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

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
	
	ED_object_add_generic_get_opts(op, loc, rot, &enter_editmode);
	make_prim_init(C, &dia, mat, &state, loc, rot);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_CallAndSelectOpf(em, op, "vertout", 
			"create_uvsphere segments=%i revolutions=%i diameter=%f mat=%m4", 
			RNA_int_get(op->ptr, "rings"), RNA_int_get(op->ptr, "segments"),
			RNA_float_get(op->ptr,"size"), mat)) 
		return OPERATOR_CANCELLED;
	
	make_prim_finish(C, &state, enter_editmode);

	return OPERATOR_FINISHED;	
}

void MESH_OT_primitive_uv_sphere_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add UV Sphere";
	ot->description= "Construct a UV sphere mesh.";
	ot->idname= "MESH_OT_primitive_uv_sphere_add";
	
	/* api callbacks */
	ot->invoke= ED_object_add_generic_invoke;
	ot->exec= add_primitive_uvsphere_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "segments", 32, INT_MIN, INT_MAX, "Segments", "", 3, 500);
	RNA_def_int(ot->srna, "rings", 24, INT_MIN, INT_MAX, "Rings", "", 3, 500);
	RNA_def_float(ot->srna, "size", 1.0f, 0.0, FLT_MAX, "Size", "", 0.001, 100.00);

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
	
	ED_object_add_generic_get_opts(op, loc, rot, &enter_editmode);
	make_prim_init(C, &dia, mat, &state, loc, rot);

	obedit = CTX_data_edit_object(C);
	me = obedit->data;
	em = me->edit_btmesh;

	if (!EDBM_CallAndSelectOpf(em, op, "vertout", 
			"create_icosphere subdivisions=%i diameter=%f mat=%m4", 
			RNA_int_get(op->ptr, "subdivisions"),
			RNA_float_get(op->ptr, "size"), mat)) {
		return OPERATOR_CANCELLED;
	}
	
	make_prim_finish(C, &state, enter_editmode);

	return OPERATOR_FINISHED;	
}

void MESH_OT_primitive_ico_sphere_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Ico Sphere";
	ot->description= "Construct an Icosphere mesh.";
	ot->idname= "MESH_OT_primitive_ico_sphere_add";
	
	/* api callbacks */
	ot->invoke= ED_object_add_generic_invoke;
	ot->exec= add_primitive_icosphere_exec;
	ot->poll= ED_operator_scene_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_int(ot->srna, "subdivisions", 2, 0, 6, "Subdivisions", "", 0, 8);
	RNA_def_float(ot->srna, "size", 1.0f, 0.0f, FLT_MAX, "Size", "", 0.001f, 100.00);

	ED_object_add_generic_props(ot, TRUE);
}
