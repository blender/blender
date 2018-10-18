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

/** \file blender/editors/mesh/editmesh_extrude_gizmo.c
 *  \ingroup edmesh
 */

#include "BLI_math.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_global.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "WM_toolsystem.h"

#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"
#include "ED_gizmo_library.h"
#include "ED_gizmo_utils.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "mesh_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name Extrude Gizmo
 * \{ */

enum {
	EXTRUDE_AXIS_NORMAL = 0,
	EXTRUDE_AXIS_XYZ = 1,
};

static const float extrude_button_scale = 0.15f;
static const float extrude_button_offset_scale = 1.5f;
static const float extrude_arrow_scale = 1.0f;
static const float extrude_arrow_xyz_axis_scale = 1.0f;
static const float extrude_arrow_normal_axis_scale = 1.0f;

static const uchar shape_plus[] = {
	0x5f, 0xfb, 0x40, 0xee, 0x25, 0xda, 0x11, 0xbf, 0x4, 0xa0, 0x0, 0x80, 0x4, 0x5f, 0x11,
	0x40, 0x25, 0x25, 0x40, 0x11, 0x5f, 0x4, 0x7f, 0x0, 0xa0, 0x4, 0xbf, 0x11, 0xda, 0x25,
	0xee, 0x40, 0xfb, 0x5f, 0xff, 0x7f, 0xfb, 0xa0, 0xee, 0xbf, 0xda, 0xda, 0xbf, 0xee,
	0xa0, 0xfb, 0x80, 0xff, 0x6e, 0xd7, 0x92, 0xd7, 0x92, 0x90, 0xd8, 0x90, 0xd8, 0x6d,
	0x92, 0x6d, 0x92, 0x27, 0x6e, 0x27, 0x6e, 0x6d, 0x28, 0x6d, 0x28, 0x90, 0x6e,
	0x90, 0x6e, 0xd7, 0x80, 0xff, 0x5f, 0xfb, 0x5f, 0xfb,
};

typedef struct GizmoExtrudeGroup {

	/* XYZ & normal. */
	struct wmGizmo *invoke_xyz_no[4];
	struct wmGizmo *adjust_xyz_no[5];

	struct {
		float normal_mat3[3][3];  /* use Z axis for normal. */
		int orientation_type;
	} data;

	wmOperatorType *ot_extrude;
	PropertyRNA *gzgt_axis_type_prop;
} GizmoExtrudeGroup;

static void gizmo_mesh_extrude_orientation_matrix_set(
        struct GizmoExtrudeGroup *ggd, const float mat[3][3])
{
	for (int i = 0; i < 3; i++) {
		/* Set orientation without location. */
		for (int j = 0; j < 3; j++) {
			copy_v3_v3(ggd->adjust_xyz_no[i]->matrix_basis[j], mat[j]);
		}
		/* nop when (i == 2). */
		swap_v3_v3(ggd->adjust_xyz_no[i]->matrix_basis[i], ggd->adjust_xyz_no[i]->matrix_basis[2]);
		/* Orient to normal gives generally less awkward results. */
		if (ggd->data.orientation_type != V3D_MANIP_NORMAL) {
			if (dot_v3v3(ggd->adjust_xyz_no[i]->matrix_basis[2], ggd->data.normal_mat3[2]) < 0.0f) {
				negate_v3(ggd->adjust_xyz_no[i]->matrix_basis[2]);
			}
		}
		mul_v3_v3fl(
		        ggd->invoke_xyz_no[i]->matrix_offset[3],
		        ggd->adjust_xyz_no[i]->matrix_basis[2],
		        (extrude_arrow_xyz_axis_scale * extrude_button_offset_scale) / extrude_button_scale);
	}
}

static void gizmo_mesh_extrude_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
	struct GizmoExtrudeGroup *ggd = MEM_callocN(sizeof(GizmoExtrudeGroup), __func__);
	gzgroup->customdata = ggd;

	const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
	const wmGizmoType *gzt_move = WM_gizmotype_find("GIZMO_GT_button_2d", true);

	for (int i = 0; i < 4; i++) {
		ggd->adjust_xyz_no[i] = WM_gizmo_new_ptr(gzt_arrow, gzgroup, NULL);
		ggd->invoke_xyz_no[i] = WM_gizmo_new_ptr(gzt_move, gzgroup, NULL);
		ggd->invoke_xyz_no[i]->flag |= WM_GIZMO_DRAW_OFFSET_SCALE;
	}

	{
		PropertyRNA *prop = RNA_struct_find_property(ggd->invoke_xyz_no[3]->ptr, "shape");
		for (int i = 0; i < 4; i++) {
			RNA_property_string_set_bytes(
			        ggd->invoke_xyz_no[i]->ptr, prop,
			        (const char *)shape_plus, ARRAY_SIZE(shape_plus));
		}
	}

	{
		ggd->ot_extrude = WM_operatortype_find("MESH_OT_extrude_context_move", true);
		ggd->gzgt_axis_type_prop = RNA_struct_type_find_property(gzgroup->type->srna, "axis_type");
	}

	for (int i = 0; i < 3; i++) {
		UI_GetThemeColor3fv(TH_AXIS_X + i, ggd->invoke_xyz_no[i]->color);
		UI_GetThemeColor3fv(TH_AXIS_X + i, ggd->adjust_xyz_no[i]->color);
	}
	UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, ggd->invoke_xyz_no[3]->color);
	UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, ggd->adjust_xyz_no[3]->color);

	for (int i = 0; i < 4; i++) {
		WM_gizmo_set_scale(ggd->invoke_xyz_no[i], extrude_button_scale);
		WM_gizmo_set_scale(ggd->adjust_xyz_no[i], extrude_arrow_scale);
	}
	WM_gizmo_set_scale(ggd->adjust_xyz_no[3], extrude_arrow_normal_axis_scale);

	for (int i = 0; i < 4; i++) {
	}

	for (int i = 0; i < 4; i++) {
		WM_gizmo_set_flag(ggd->adjust_xyz_no[i], WM_GIZMO_DRAW_VALUE, true);
	}

	/* XYZ & normal axis extrude. */
	for (int i = 0; i < 4; i++) {
		PointerRNA *ptr = WM_gizmo_operator_set(ggd->invoke_xyz_no[i], 0, ggd->ot_extrude, NULL);
		{
			bool constraint[3] = {0, 0, 0};
			constraint[MIN2(i, 2)] = 1;
			PointerRNA macroptr = RNA_pointer_get(ptr, "TRANSFORM_OT_translate");
			RNA_boolean_set(&macroptr, "release_confirm", true);
			RNA_boolean_set_array(&macroptr, "constraint_axis", constraint);
		}
	}

	/* Adjust extrude. */
	for (int i = 0; i < 4; i++) {
		PointerRNA *ptr = WM_gizmo_operator_set(ggd->adjust_xyz_no[i], 0, ggd->ot_extrude, NULL);
		{
			bool constraint[3] = {0, 0, 0};
			constraint[MIN2(i, 2)] = 1;
			PointerRNA macroptr = RNA_pointer_get(ptr, "TRANSFORM_OT_translate");
			RNA_boolean_set(&macroptr, "release_confirm", true);
			RNA_boolean_set_array(&macroptr, "constraint_axis", constraint);
		}
		wmGizmoOpElem *gzop = WM_gizmo_operator_get(ggd->adjust_xyz_no[i], 0);
		gzop->is_redo = true;
	}
}

static void gizmo_mesh_extrude_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
	GizmoExtrudeGroup *ggd = gzgroup->customdata;

	for (int i = 0; i < 4; i++) {
		WM_gizmo_set_flag(ggd->invoke_xyz_no[i], WM_GIZMO_HIDDEN, true);
		WM_gizmo_set_flag(ggd->adjust_xyz_no[i], WM_GIZMO_HIDDEN, true);
	}

	if (G.moving) {
		return;
	}

	Scene *scene = CTX_data_scene(C);

	int axis_type;
	{
		PointerRNA ptr;
		bToolRef *tref = WM_toolsystem_ref_from_context((bContext *)C);
		WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgroup->type, &ptr);
		axis_type = RNA_property_enum_get(&ptr, ggd->gzgt_axis_type_prop);
	}

	ggd->data.orientation_type = scene->orientation_type;
	const bool use_normal = (
	        (ggd->data.orientation_type != V3D_MANIP_NORMAL) ||
	        (axis_type == EXTRUDE_AXIS_NORMAL));
	const int axis_len_used = use_normal ? 4 : 3;

	struct TransformBounds tbounds;

	if (use_normal) {
		struct TransformBounds tbounds_normal;
		if (!ED_transform_calc_gizmo_stats(
		            C, &(struct TransformCalcParams){
		                .orientation_type = V3D_MANIP_NORMAL + 1,
		            }, &tbounds_normal))
		{
			unit_m3(tbounds_normal.axis);
		}
		copy_m3_m3(ggd->data.normal_mat3, tbounds_normal.axis);
	}

	/* TODO(campbell): run second since this modifies the 3D view, it should not. */
	if (!ED_transform_calc_gizmo_stats(
	            C, &(struct TransformCalcParams){
	                .orientation_type = ggd->data.orientation_type + 1,
	            }, &tbounds))
	{
		return;
	}

	/* Main axis is normal. */
	if (!use_normal) {
		copy_m3_m3(ggd->data.normal_mat3, tbounds.axis);
	}

	/* Offset the add icon. */
	mul_v3_v3fl(
	        ggd->invoke_xyz_no[3]->matrix_offset[3],
	        ggd->data.normal_mat3[2],
	        (extrude_arrow_normal_axis_scale * extrude_button_offset_scale) / extrude_button_scale);

	/* Needed for normal orientation. */
	gizmo_mesh_extrude_orientation_matrix_set(ggd, tbounds.axis);
	if (use_normal) {
		copy_m4_m3(ggd->adjust_xyz_no[3]->matrix_basis, ggd->data.normal_mat3);
	}

	/* Location. */
	for (int i = 0; i < axis_len_used; i++) {
		WM_gizmo_set_matrix_location(ggd->invoke_xyz_no[i], tbounds.center);
		WM_gizmo_set_matrix_location(ggd->adjust_xyz_no[i], tbounds.center);
	}

	/* Adjust current operator. */
	/* Don't use 'WM_operator_last_redo' because selection actions will be ignored. */
	wmOperator *op = CTX_wm_manager(C)->operators.last;
	bool has_redo = (op && op->type == ggd->ot_extrude);

	/* Un-hide. */
	for (int i = 0; i < axis_len_used; i++) {
		WM_gizmo_set_flag(ggd->invoke_xyz_no[i], WM_GIZMO_HIDDEN, false);
		WM_gizmo_set_flag(ggd->adjust_xyz_no[i], WM_GIZMO_HIDDEN, !has_redo);
	}

	/* Operator properties. */
	if (use_normal) {
		wmGizmoOpElem *gzop = WM_gizmo_operator_get(ggd->invoke_xyz_no[3], 0);
		PointerRNA macroptr = RNA_pointer_get(&gzop->ptr, "TRANSFORM_OT_translate");
		RNA_enum_set(&macroptr, "constraint_orientation", V3D_MANIP_NORMAL);
	}

	/* Redo with current settings. */
	if (has_redo) {
		wmOperator *op_transform = op->macro.last;
		float value[4];
		RNA_float_get_array(op_transform->ptr, "value", value);
		bool constraint_axis[3];
		RNA_boolean_get_array(op_transform->ptr, "constraint_axis", constraint_axis);
		int orientation_type = RNA_enum_get(op_transform->ptr, "constraint_orientation");

		/* We could also access this from 'ot->last_properties' */
		for (int i = 0; i < 4; i++) {
			if ((i != 3) ?
			    (orientation_type == ggd->data.orientation_type && constraint_axis[i]) :
			    (orientation_type == V3D_MANIP_NORMAL && constraint_axis[2]))
			{
				wmGizmoOpElem *gzop = WM_gizmo_operator_get(ggd->adjust_xyz_no[i], 0);

				PointerRNA macroptr = RNA_pointer_get(&gzop->ptr, "TRANSFORM_OT_translate");

				RNA_float_set_array(&macroptr, "value", value);
				RNA_boolean_set_array(&macroptr, "constraint_axis", constraint_axis);
				RNA_enum_set(&macroptr, "constraint_orientation", orientation_type);
			}
			else {
				/* TODO(campbell): ideally we could adjust all,
				 * this is complicated by how operator redo and the transform macro works. */
				WM_gizmo_set_flag(ggd->adjust_xyz_no[i], WM_GIZMO_HIDDEN, true);
			}
		}
	}

	for (int i = 0; i < 4; i++) {
		RNA_enum_set(
		        ggd->invoke_xyz_no[i]->ptr,
		        "draw_options",
		        (ggd->adjust_xyz_no[i]->flag & WM_GIZMO_HIDDEN) ?
		        ED_GIZMO_BUTTON_SHOW_HELPLINE : 0);
	}

	/* TODO: skip calculating axis which wont be used (above). */
	switch (axis_type) {
		case EXTRUDE_AXIS_NORMAL:
			for (int i = 0; i < 3; i++) {
				WM_gizmo_set_flag(ggd->invoke_xyz_no[i], WM_GIZMO_HIDDEN, true);
			}
			break;
		case EXTRUDE_AXIS_XYZ:
			WM_gizmo_set_flag(ggd->invoke_xyz_no[3], WM_GIZMO_HIDDEN, true);
			break;
	}
}

static void gizmo_mesh_extrude_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
	GizmoExtrudeGroup *ggd = gzgroup->customdata;
	switch (ggd->data.orientation_type) {
		case V3D_MANIP_VIEW:
		{
			RegionView3D *rv3d = CTX_wm_region_view3d(C);
			float mat[3][3];
			copy_m3_m4(mat, rv3d->viewinv);
			normalize_m3(mat);
			gizmo_mesh_extrude_orientation_matrix_set(ggd, mat);
			break;
		}
	}

	/* Basic ordering for drawing only. */
	{
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		LISTBASE_FOREACH (wmGizmo *, gz, &gzgroup->gizmos) {
			gz->temp.f = dot_v3v3(rv3d->viewinv[2], gz->matrix_offset[3]);
		}
		BLI_listbase_sort(&gzgroup->gizmos, WM_gizmo_cmp_temp_fl_reverse);
	}
}

static void gizmo_mesh_extrude_message_subscribe(
        const bContext *C, wmGizmoGroup *gzgroup, struct wmMsgBus *mbus)
{
	GizmoExtrudeGroup *ggd = gzgroup->customdata;
	ARegion *ar = CTX_wm_region(C);

	/* Subscribe to view properties */
	wmMsgSubscribeValue msg_sub_value_gz_tag_refresh = {
		.owner = ar,
		.user_data = gzgroup->parent_gzmap,
		.notify = WM_gizmo_do_msg_notify_tag_refresh,
	};

	{
		WM_msg_subscribe_rna_anon_prop(mbus, Scene, transform_orientation, &msg_sub_value_gz_tag_refresh);
	}


	WM_msg_subscribe_rna_params(
	        mbus,
	        &(const wmMsgParams_RNA){
	            .ptr = (PointerRNA){.type = gzgroup->type->srna},
	            .prop = ggd->gzgt_axis_type_prop,
	        },
	        &msg_sub_value_gz_tag_refresh, __func__);
}

void MESH_GGT_extrude(struct wmGizmoGroupType *gzgt)
{
	gzgt->name = "Mesh Extrude";
	gzgt->idname = "MESH_GGT_extrude";

	gzgt->flag = WM_GIZMOGROUPTYPE_3D;

	gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
	gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

	gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
	gzgt->setup = gizmo_mesh_extrude_setup;
	gzgt->refresh = gizmo_mesh_extrude_refresh;
	gzgt->draw_prepare = gizmo_mesh_extrude_draw_prepare;
	gzgt->message_subscribe = gizmo_mesh_extrude_message_subscribe;

	static const EnumPropertyItem axis_type_items[] = {
		{EXTRUDE_AXIS_NORMAL, "NORMAL", 0, "Normal", "Only show normal axis"},
		{EXTRUDE_AXIS_XYZ, "XYZ", 0, "XYZ", "Follow scene orientation"},
		{0, NULL, 0, NULL, NULL}
	};
	RNA_def_enum(gzgt->srna, "axis_type", axis_type_items, 0, "Axis Type", "");
}

/** \} */
