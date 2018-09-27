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

/** \file blender/editors/space_view3d/view3d_gizmo_navigate.c
 *  \ingroup spview3d
 */

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"

#include "DNA_object_types.h"

#include "ED_screen.h"
#include "ED_gizmo_library.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name View3D Navigation Gizmo Group
 * \{ */

/* Offset from screen edge. */
#define GIZMO_OFFSET_FAC 1.1f
/* Size of main icon. */
#define GIZMO_SIZE 80
/* Size of the smaller icons. */
#define GIZMO_ICON_SIZE 20
/* How much mini buttons offset from the primary. */
#define GIZMO_MINI_OFFSET_FAC 0.32f


enum {
	MPR_MOVE = 0,
	MPR_ROTATE = 1,
	MPR_ZOOM = 2,

	/* just buttons */
	/* overlaps MPR_ORTHO (switch between) */
	MPR_PERSP = 3,
	MPR_ORTHO = 4,
	MPR_CAMERA = 5,

	MPR_TOTAL = 6,
};

struct NavigateGizmoInfo {
	const char *opname;
	const char *gizmo;
	uint icon;
};

static struct NavigateGizmoInfo g_navigate_params[MPR_TOTAL] = {
	{
		.opname = "VIEW3D_OT_move",
		.gizmo = "GIZMO_GT_button_2d",
		ICON_VIEW_PAN,
	}, {
		.opname = "VIEW3D_OT_rotate",
		.gizmo = "VIEW3D_GT_navigate_rotate",
		0,
	}, {
		.opname = "VIEW3D_OT_zoom",
		.gizmo = "GIZMO_GT_button_2d",
		ICON_VIEW_ZOOM,
	}, {
		.opname = "VIEW3D_OT_view_persportho",
		.gizmo = "GIZMO_GT_button_2d",
		ICON_VIEW_PERSPECTIVE,
	}, {
		.opname = "VIEW3D_OT_view_persportho",
		.gizmo = "GIZMO_GT_button_2d",
		ICON_VIEW_ORTHO,
	}, {
		.opname = "VIEW3D_OT_view_camera",
		.gizmo = "GIZMO_GT_button_2d",
		ICON_VIEW_CAMERA,
	},
};

struct NavigateWidgetGroup {
	wmGizmo *gz_array[MPR_TOTAL];
	/* Store the view state to check for changes. */
	struct {
		rcti rect_visible;
		struct {
			char is_persp;
			char is_camera;
			char viewlock;
		} rv3d;
	} state;
	int region_size[2];
};

static bool WIDGETGROUP_navigate_poll(const bContext *C, wmGizmoGroupType *UNUSED(gzgt))
{
	View3D *v3d = CTX_wm_view3d(C);
	if (((U.uiflag & USER_SHOW_GIZMO_AXIS) == 0) ||
	    (v3d->flag2 & V3D_RENDER_OVERRIDE) ||
	    (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_NAVIGATE)))
	{
		return false;
	}
	return true;

}

static void WIDGETGROUP_navigate_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
	struct NavigateWidgetGroup *navgroup = MEM_callocN(sizeof(struct NavigateWidgetGroup), __func__);

	navgroup->region_size[0] = -1;
	navgroup->region_size[1] = -1;

	wmOperatorType *ot_view_axis = WM_operatortype_find("VIEW3D_OT_view_axis", true);
	wmOperatorType *ot_view_camera = WM_operatortype_find("VIEW3D_OT_view_camera", true);

	for (int i = 0; i < MPR_TOTAL; i++) {
		const struct NavigateGizmoInfo *info = &g_navigate_params[i];
		navgroup->gz_array[i] = WM_gizmo_new(info->gizmo, gzgroup, NULL);
		wmGizmo *gz = navgroup->gz_array[i];
		gz->flag |= WM_GIZMO_MOVE_CURSOR | WM_GIZMO_DRAW_MODAL;

		if (i == MPR_ROTATE) {
			gz->color[3] = 0.0f;
			gz->color_hi[3] = 0.1f;
		}
		else {
			gz->color[3] = 0.2f;
			gz->color_hi[3] = 0.4f;
		}


		/* may be overwritten later */
		gz->scale_basis = (GIZMO_ICON_SIZE) / 2;
		if (info->icon != 0) {
			PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "icon");
			RNA_property_enum_set(gz->ptr, prop, info->icon);
			RNA_enum_set(gz->ptr, "draw_options", ED_GIZMO_BUTTON_SHOW_OUTLINE);
		}

		wmOperatorType *ot = WM_operatortype_find(info->opname, true);
		WM_gizmo_operator_set(gz, 0, ot, NULL);
	}

	{
		wmGizmo *gz = navgroup->gz_array[MPR_CAMERA];
		WM_gizmo_operator_set(gz, 0, ot_view_camera, NULL);
	}

	/* Click only buttons (not modal). */
	{
		int gz_ids[] = {MPR_PERSP, MPR_ORTHO, MPR_CAMERA};
		for (int i = 0; i < ARRAY_SIZE(gz_ids); i++) {
			wmGizmo *gz = navgroup->gz_array[gz_ids[i]];
			RNA_boolean_set(gz->ptr, "show_drag", false);
		}
	}

	/* Modal operators, don't use initial mouse location since we're clicking on a button. */
	{
		int gz_ids[] = {MPR_MOVE, MPR_ROTATE, MPR_ZOOM};
		for (int i = 0; i < ARRAY_SIZE(gz_ids); i++) {
			wmGizmo *gz = navgroup->gz_array[gz_ids[i]];
			wmGizmoOpElem *mpop = WM_gizmo_operator_get(gz, 0);
			RNA_boolean_set(&mpop->ptr, "use_mouse_init", false);
		}
	}

	{
		wmGizmo *gz = navgroup->gz_array[MPR_ROTATE];
		gz->scale_basis = GIZMO_SIZE / 2;
		char mapping[6] = {
			RV3D_VIEW_LEFT,
			RV3D_VIEW_RIGHT,
			RV3D_VIEW_FRONT,
			RV3D_VIEW_BACK,
			RV3D_VIEW_BOTTOM,
			RV3D_VIEW_TOP,
		};

		for (int part_index = 0; part_index < 6; part_index += 1) {
			PointerRNA *ptr = WM_gizmo_operator_set(gz, part_index + 1, ot_view_axis, NULL);
			RNA_enum_set(ptr, "type", mapping[part_index]);
		}

		/* When dragging an axis, use this instead. */
		gz->drag_part = 0;
	}

	gzgroup->customdata = navgroup;
}

static void WIDGETGROUP_navigate_draw_prepare(const bContext *C, wmGizmoGroup *gzgroup)
{
	struct NavigateWidgetGroup *navgroup = gzgroup->customdata;
	ARegion *ar = CTX_wm_region(C);
	const RegionView3D *rv3d = ar->regiondata;

	for (int i = 0; i < 3; i++) {
		copy_v3_v3(navgroup->gz_array[MPR_ROTATE]->matrix_offset[i], rv3d->viewmat[i]);
	}

	rcti rect_visible;
	ED_region_visible_rect(ar, &rect_visible);

	if ((navgroup->state.rect_visible.xmax == rect_visible.xmax) &&
	    (navgroup->state.rect_visible.ymax == rect_visible.ymax) &&
	    (navgroup->state.rv3d.is_persp == rv3d->is_persp) &&
	    (navgroup->state.rv3d.is_camera == (rv3d->persp == RV3D_CAMOB)) &&
	    (navgroup->state.rv3d.viewlock == rv3d->viewlock))
	{
		return;
	}

	navgroup->state.rect_visible = rect_visible;
	navgroup->state.rv3d.is_persp = rv3d->is_persp;
	navgroup->state.rv3d.is_camera = (rv3d->persp == RV3D_CAMOB);
	navgroup->state.rv3d.viewlock = rv3d->viewlock;

	const bool show_rotate = (
	        ((rv3d->viewlock & RV3D_LOCKED) == 0) &&
	        (navgroup->state.rv3d.is_camera == false));
	const bool show_fixed_offset = navgroup->state.rv3d.is_camera;
	const float icon_size = GIZMO_SIZE;
	const float icon_offset = (icon_size * 0.52f) * GIZMO_OFFSET_FAC * UI_DPI_FAC;
	const float icon_offset_mini = icon_size * GIZMO_MINI_OFFSET_FAC * UI_DPI_FAC;
	const float co_rotate[2] = {
		rect_visible.xmax - icon_offset,
		rect_visible.ymax - icon_offset,
	};
	const float co[2] = {
		rect_visible.xmax - ((show_rotate || show_fixed_offset) ? (icon_offset * 2.0f) : (icon_offset_mini * 0.75f)),
		rect_visible.ymax - icon_offset_mini * 0.75f,
	};

	wmGizmo *gz;

	for (uint i = 0; i < ARRAY_SIZE(navgroup->gz_array); i++) {
		gz = navgroup->gz_array[i];
		WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, true);
	}

	/* RV3D_LOCKED or Camera: only show supported buttons. */
	if (show_rotate) {
		gz = navgroup->gz_array[MPR_ROTATE];
		gz->matrix_basis[3][0] = co_rotate[0];
		gz->matrix_basis[3][1] = co_rotate[1];
		WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);
	}

	int icon_mini_slot = 0;

	gz = navgroup->gz_array[MPR_ZOOM];
	gz->matrix_basis[3][0] = co[0] - (icon_offset_mini * icon_mini_slot++);
	gz->matrix_basis[3][1] = co[1];
	WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

	gz = navgroup->gz_array[MPR_MOVE];
	gz->matrix_basis[3][0] = co[0] - (icon_offset_mini * icon_mini_slot++);
	gz->matrix_basis[3][1] = co[1];
	WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

	if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
		gz = navgroup->gz_array[MPR_CAMERA];
		gz->matrix_basis[3][0] = co[0] - (icon_offset_mini * icon_mini_slot++);
		gz->matrix_basis[3][1] = co[1];
		WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);

		if (navgroup->state.rv3d.is_camera == false) {
			gz = navgroup->gz_array[rv3d->is_persp ? MPR_PERSP : MPR_ORTHO];
			gz->matrix_basis[3][0] = co[0] - (icon_offset_mini * icon_mini_slot++);
			gz->matrix_basis[3][1] = co[1];
			WM_gizmo_set_flag(gz, WM_GIZMO_HIDDEN, false);
		}
	}
}

void VIEW3D_GGT_navigate(wmGizmoGroupType *gzgt)
{
	gzgt->name = "View3D Navigate";
	gzgt->idname = "VIEW3D_GGT_navigate";

	gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT |
	              WM_GIZMOGROUPTYPE_SCALE |
	              WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL);

	gzgt->poll = WIDGETGROUP_navigate_poll;
	gzgt->setup = WIDGETGROUP_navigate_setup;
	gzgt->draw_prepare = WIDGETGROUP_navigate_draw_prepare;
}

/** \} */
