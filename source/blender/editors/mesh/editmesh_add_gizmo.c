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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_add_gizmo.c
 *  \ingroup edmesh
 *
 * Creation gizmos.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"

#include "ED_gizmo_library.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"

#include "BLT_translation.h"

#include "mesh_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name Helper Functions
 * \{ */

/**
 * When we place a shape, pick a plane.
 *
 * We may base this choice on context,
 * for now pick the "ground" based on the 3D cursor's dominant plane pointing down relative to the view.
 */
static void calc_initial_placement_point_from_view(
        bContext *C, const float mval[2],
        float r_location[3], float r_rotation[3][3])
{

	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	bool use_mouse_project = true; /* TODO: make optional */

	float cursor_matrix[4][4];
	float orient_matrix[3][3];
	ED_view3d_cursor3d_calc_mat4(scene, v3d, cursor_matrix);

	float dots[3] = {
		dot_v3v3(rv3d->viewinv[2], cursor_matrix[0]),
		dot_v3v3(rv3d->viewinv[2], cursor_matrix[1]),
		dot_v3v3(rv3d->viewinv[2], cursor_matrix[2]),
	};
	const int axis = axis_dominant_v3_single(dots);

	copy_v3_v3(orient_matrix[0], cursor_matrix[(axis + 1) % 3]);
	copy_v3_v3(orient_matrix[1], cursor_matrix[(axis + 2) % 3]);
	copy_v3_v3(orient_matrix[2], cursor_matrix[axis]);

	if (dot_v3v3(rv3d->viewinv[2], orient_matrix[2]) < 0.0f) {
		negate_v3(orient_matrix[2]);
	}
	if (is_negative_m3(orient_matrix)) {
		swap_v3_v3(orient_matrix[0], orient_matrix[1]);
	}

	if (use_mouse_project) {
		float ray_co[3], ray_no[3];
		if (ED_view3d_win_to_ray(
		            CTX_data_depsgraph(C),
		            ar, v3d, mval,
		            ray_co, ray_no, false))
		{
			float plane[4];
			plane_from_point_normal_v3(plane, cursor_matrix[3], orient_matrix[2]);
			float lambda;
			if (isect_ray_plane_v3(ray_co, ray_no, plane, &lambda, true)) {
				madd_v3_v3v3fl(r_location, ray_co, ray_no, lambda);
				copy_m3_m3(r_rotation, orient_matrix);
				return;
			}
		}
	}

	/* fallback */
	copy_v3_v3(r_location, cursor_matrix[3]);
	copy_m3_m3(r_rotation, orient_matrix);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Placement Gizmo
 * \{ */

typedef struct GizmoPlacementGroup {
	struct wmGizmo *cage;
	struct {
		bContext *context;
		wmOperator *op;
		PropertyRNA *prop_matrix;
	} data;
} GizmoPlacementGroup;

/**
 * \warning Calling redo from property updates is not great.
 * This is needed because changing the RNA doesn't cause a redo
 * and we're not using operator UI which does just this.
 */
static void gizmo_placement_exec(GizmoPlacementGroup *man)
{
	wmOperator *op = man->data.op;
	if (op == WM_operator_last_redo((bContext *)man->data.context)) {
		ED_undo_operator_repeat((bContext *)man->data.context, op);
	}
}

static void gizmo_mesh_placement_update_from_op(GizmoPlacementGroup *man)
{
	wmOperator *op = man->data.op;
	UNUSED_VARS(op);
	/* For now don't read back from the operator. */
#if 0
	RNA_property_float_get_array(op->ptr, man->data.prop_matrix, &man->cage->matrix_offset[0][0]);
#endif
}

/* translate callbacks */
static void gizmo_placement_prop_matrix_get(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        void *value_p)
{
	GizmoPlacementGroup *man = gz->parent_gzgroup->customdata;
	wmOperator *op = man->data.op;
	float *value = value_p;
	BLI_assert(gz_prop->type->array_length == 16);
	UNUSED_VARS_NDEBUG(gz_prop);

	if (value_p != man->cage->matrix_offset) {
		mul_m4_m4m4(value_p, man->cage->matrix_basis, man->cage->matrix_offset);
		RNA_property_float_get_array(op->ptr, man->data.prop_matrix, value);
	}
}

static void gizmo_placement_prop_matrix_set(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        const void *value)
{
	GizmoPlacementGroup *man = gz->parent_gzgroup->customdata;
	wmOperator *op = man->data.op;

	BLI_assert(gz_prop->type->array_length == 16);
	UNUSED_VARS_NDEBUG(gz_prop);

	float mat[4][4];
	mul_m4_m4m4(mat, man->cage->matrix_basis, value);

	if (is_negative_m4(mat)) {
		negate_mat3_m4(mat);
	}

	RNA_property_float_set_array(op->ptr, man->data.prop_matrix, &mat[0][0]);

	gizmo_placement_exec(man);
}

static bool gizmo_mesh_placement_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
	wmOperator *op = WM_operator_last_redo(C);
	if (op == NULL || !STREQ(op->type->idname, "MESH_OT_primitive_cube_add_gizmo")) {
		WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
		return false;
	}
	return true;
}

static void gizmo_mesh_placement_modal_from_setup(
        const bContext *C, wmGizmoGroup *gzgroup)
{
	GizmoPlacementGroup *man = gzgroup->customdata;

	/* Initial size. */
	{
		wmGizmo *gz = man->cage;
		zero_m4(gz->matrix_offset);

		/* TODO: support zero scaled matrix in 'GIZMO_GT_cage_3d'. */
		gz->matrix_offset[0][0] = 0.01;
		gz->matrix_offset[1][1] = 0.01;
		gz->matrix_offset[2][2] = 0.01;
		gz->matrix_offset[3][3] = 1.0f;
	}

	/* Start off dragging. */
	{
		wmWindow *win = CTX_wm_window(C);
		ARegion *ar = CTX_wm_region(C);
		wmGizmo *gz = man->cage;

		{
			float mat3[3][3];
			float location[3];
			calc_initial_placement_point_from_view(
			        (bContext *)C, (float[2]){
			            win->eventstate->x - ar->winrct.xmin,
			            win->eventstate->y - ar->winrct.ymin,
			        },
			        location, mat3);
			copy_m4_m3(gz->matrix_basis, mat3);
			copy_v3_v3(gz->matrix_basis[3], location);
		}

		if (1) {
			wmGizmoMap *gzmap = gzgroup->parent_gzmap;
			WM_gizmo_modal_set_from_setup(
			        gzmap, (bContext *)C, man->cage, ED_GIZMO_CAGE3D_PART_SCALE_MAX_X_MAX_Y_MAX_Z, win->eventstate);
		}
	}
}

static void gizmo_mesh_placement_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
	wmOperator *op = WM_operator_last_redo(C);

	if (op == NULL || !STREQ(op->type->idname, "MESH_OT_primitive_cube_add_gizmo")) {
		return;
	}

	struct GizmoPlacementGroup *man = MEM_callocN(sizeof(GizmoPlacementGroup), __func__);
	gzgroup->customdata = man;

	const wmGizmoType *gzt_cage = WM_gizmotype_find("GIZMO_GT_cage_3d", true);

	man->cage = WM_gizmo_new_ptr(gzt_cage, gzgroup, NULL);

	UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, man->cage->color);

	RNA_enum_set(man->cage->ptr, "transform",
	             ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE |
	             ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE |
	             ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE_SIGNED);

	WM_gizmo_set_flag(man->cage, WM_GIZMO_DRAW_VALUE, true);

	man->data.context = (bContext *)C;
	man->data.op = op;
	man->data.prop_matrix = RNA_struct_find_property(op->ptr, "matrix");

	gizmo_mesh_placement_update_from_op(man);

	/* Setup property callbacks */
	{
		WM_gizmo_target_property_def_func(
		        man->cage, "matrix",
		        &(const struct wmGizmoPropertyFnParams) {
		            .value_get_fn = gizmo_placement_prop_matrix_get,
		            .value_set_fn = gizmo_placement_prop_matrix_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });
	}

	gizmo_mesh_placement_modal_from_setup(C, gzgroup);
}

static void gizmo_mesh_placement_draw_prepare(
        const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
	GizmoPlacementGroup *man = gzgroup->customdata;
	if (man->data.op->next) {
		man->data.op = WM_operator_last_redo((bContext *)man->data.context);
	}
	gizmo_mesh_placement_update_from_op(man);
}

static void MESH_GGT_add_bounds(struct wmGizmoGroupType *gzgt)
{
	gzgt->name = "Mesh Add Bounds";
	gzgt->idname = "MESH_GGT_add_bounds";

	gzgt->flag = WM_GIZMOGROUPTYPE_3D;

	gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
	gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

	gzgt->poll = gizmo_mesh_placement_poll;
	gzgt->setup = gizmo_mesh_placement_setup;
	gzgt->draw_prepare = gizmo_mesh_placement_draw_prepare;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Cube Gizmo-Operator
 *
 * For now we use a separate operator to add a cube,
 * we can try to merge then however they are invoked differently
 * and share the same BMesh creation code.
 * \{ */


static int add_primitive_cube_gizmo_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);;
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	float matrix[4][4];

	/* Get the matrix that defines the cube bounds (as set by the gizmo cage). */
	{
		PropertyRNA *prop_matrix = RNA_struct_find_property(op->ptr, "matrix");
		if (RNA_property_is_set(op->ptr, prop_matrix)) {
			RNA_property_float_get_array(op->ptr, prop_matrix, &matrix[0][0]);
			invert_m4_m4(obedit->imat, obedit->obmat);
			mul_m4_m4m4(matrix, obedit->imat, matrix);
		}
		else {
			/* For the first update the widget may not set the matrix. */
			return OPERATOR_FINISHED;
		}
	}

	const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

	if (calc_uvs) {
		ED_mesh_uv_texture_ensure(obedit->data, NULL);
	}

	if (!EDBM_op_call_and_selectf(
	        em, op, "verts.out", false,
	        "create_cube matrix=%m4 size=%f calc_uvs=%b",
	        matrix, 1.0f, calc_uvs))
	{
		return OPERATOR_CANCELLED;
	}

	EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);
	EDBM_update_generic(em, true, true);

	return OPERATOR_FINISHED;
}

static int add_primitive_cube_gizmo_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	View3D *v3d = CTX_wm_view3d(C);

	int ret = add_primitive_cube_gizmo_exec(C, op);
	if (ret & OPERATOR_FINISHED) {
		/* Setup gizmos */
		if (v3d && ((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0)) {
			ARegion *ar = CTX_wm_region(C);
			wmGizmoMap *gzmap = ar->gizmo_map;
			wmGizmoGroupType *gzgt = WM_gizmogrouptype_find("MESH_GGT_add_bounds", false);
			wmGizmoGroup *gzgroup = WM_gizmomap_group_find_ptr(gzmap, gzgt);
			if (gzgroup != NULL) {
				GizmoPlacementGroup *man = gzgroup->customdata;
				man->data.op = op;
				gizmo_mesh_placement_modal_from_setup(C, gzgroup);
			}
			else {
				WM_gizmo_group_type_ensure_ptr(gzgt);
			}
		}
	}

	return ret;
}

void MESH_OT_primitive_cube_add_gizmo(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Cube";
	ot->description = "Construct a cube mesh";
	ot->idname = "MESH_OT_primitive_cube_add_gizmo";

	/* api callbacks */
	ot->invoke = add_primitive_cube_gizmo_invoke;
	ot->exec = add_primitive_cube_gizmo_exec;
	ot->poll = ED_operator_editmesh_view3d;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_mesh_props(ot);
	ED_object_add_generic_props(ot, true);

	/* hidden props */
	PropertyRNA *prop = RNA_def_float_matrix(ot->srna, "matrix", 4, 4, NULL, 0.0f, 0.0f, "Matrix", "", 0.0f, 0.0f);
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	WM_gizmogrouptype_append(MESH_GGT_add_bounds);
}

/** \} */
