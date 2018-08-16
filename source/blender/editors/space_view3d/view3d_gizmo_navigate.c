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
#define GIZMO_OFFSET_FAC 1.2f
/* Size of main icon. */
#define GIZMO_SIZE 80
/* Factor for size of smaller button. */
#define GIZMO_MINI_FAC 0.35f
/* How much mini buttons offset from the primary. */
#define GIZMO_MINI_OFFSET_FAC 0.42f


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

/* Vector icons compatible with 'GPU_batch_from_poly_2d_encoded' */
static const uchar shape_camera[] = {
	0xa3, 0x19, 0x78, 0x55, 0x4d, 0x19, 0x4f, 0x0a, 0x7f, 0x00, 0xb0, 0x0a, 0xa9, 0x19,
	0xa9, 0x19, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f, 0x0a, 0x4f, 0x25, 0x25, 0x4f, 0x0a,
	0x4d, 0x19, 0x47, 0x19, 0x65, 0x55, 0x41, 0x55, 0x41, 0x9e, 0x43, 0xa8, 0x38, 0xb3,
	0x34, 0xc3, 0x38, 0xd2, 0x43, 0xdd, 0x53, 0xe1, 0x62, 0xdd, 0x6d, 0xd2, 0x72, 0xc3,
	0x78, 0xc3, 0x7c, 0xd2, 0x87, 0xdd, 0x96, 0xe1, 0xa6, 0xdd, 0xb1, 0xd2, 0xb5, 0xc3,
	0xb1, 0xb3, 0xa6, 0xa8, 0xa9, 0x9e, 0xa9, 0x8c, 0xbb, 0x8c, 0xbb, 0x86, 0xc7, 0x86,
	0xe0, 0x9e, 0xe0, 0x55, 0xc7, 0x6d, 0xbb, 0x6d, 0xbb, 0x67, 0xa9, 0x67, 0xa9, 0x55,
	0x8a, 0x55, 0xa9, 0x19, 0xb0, 0x0a, 0xda, 0x25, 0xf5, 0x4f, 0xff, 0x80, 0xf5, 0xb0,
	0xda, 0xda, 0xb0, 0xf5, 0x80, 0xff, 0x4f, 0xf5, 0x4f, 0xf5, 0x7c, 0xb3, 0x78, 0xc3,
	0x72, 0xc3, 0x6d, 0xb3, 0x62, 0xa8, 0x53, 0xa4, 0x43, 0xa8, 0x41, 0x9e, 0xa9, 0x9e,
	0xa6, 0xa8, 0x96, 0xa4, 0x87, 0xa8, 0x87, 0xa8,
};
static const uchar shape_ortho[] = {
	0x85, 0x15, 0x85, 0x7c, 0xde, 0xb3, 0xde, 0xb8, 0xd9, 0xba, 0x80, 0x85, 0x27, 0xba,
	0x22, 0xb8, 0x22, 0xb3, 0x7b, 0x7c, 0x7b, 0x15, 0x80, 0x12, 0x80, 0x12, 0x1d, 0xba,
	0x80, 0xf2, 0x80, 0xff, 0x4f, 0xf5, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f, 0x0a, 0x4f,
	0x25, 0x25, 0x4f, 0x0a, 0x7f, 0x00, 0x80, 0x0d, 0x1d, 0x45, 0x1d, 0x45, 0xb0, 0x0a,
	0xda, 0x25, 0xf5, 0x4f, 0xff, 0x80, 0xf5, 0xb0, 0xda, 0xda, 0xb0, 0xf5, 0x80, 0xff,
	0x80, 0xf2, 0xe3, 0xba, 0xe3, 0x45, 0x80, 0x0d, 0x7f, 0x00, 0x7f, 0x00,
};
static const uchar shape_pan[] = {
	0xbf, 0x4c, 0xbf, 0x66, 0x99, 0x66, 0x99, 0x40, 0xb2, 0x40, 0x7f, 0x0d, 0x7f, 0x00,
	0xb0, 0x0a, 0xda, 0x25, 0xf5, 0x4f, 0xff, 0x80, 0xf5, 0xb0, 0xda, 0xda, 0xb0, 0xf5,
	0x80, 0xff, 0x80, 0xf2, 0xb3, 0xbf, 0x99, 0xbf, 0x99, 0x99, 0xbf, 0x99, 0xbf, 0xb2,
	0xf2, 0x7f, 0xf2, 0x7f, 0x40, 0xb3, 0x40, 0x99, 0x66, 0x99, 0x66, 0xbf, 0x4d, 0xbf,
	0x80, 0xf2, 0x80, 0xff, 0x4f, 0xf5, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f, 0x0a, 0x4f,
	0x25, 0x25, 0x4f, 0x0a, 0x7f, 0x00, 0x7f, 0x0d, 0x4c, 0x40, 0x66, 0x40, 0x66, 0x66,
	0x40, 0x66, 0x40, 0x4d, 0x0d, 0x80, 0x0d, 0x80,
};
static const uchar shape_persp[] = {
	0xda, 0xda, 0xb0, 0xf5, 0x80, 0xff, 0x4f, 0xf5, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f,
	0x0a, 0x4f, 0x25, 0x25, 0x4f, 0x0a, 0x7f, 0x00, 0x80, 0x07, 0x30, 0x50, 0x18, 0xbd,
	0x80, 0xdb, 0xe8, 0xbd, 0xf5, 0xb0, 0xf5, 0xb0, 0x83, 0x0f, 0x87, 0x7b, 0xe2, 0xb7,
	0xe3, 0xba, 0xe0, 0xbb, 0x80, 0x87, 0x20, 0xbb, 0x1d, 0xba, 0x1d, 0xb7, 0x78, 0x7b,
	0x7d, 0x0f, 0x80, 0x0c, 0x80, 0x0c, 0xd0, 0x50, 0x80, 0x07, 0x7f, 0x00, 0xb0, 0x0a,
	0xda, 0x25, 0xf5, 0x4f, 0xff, 0x80, 0xf5, 0xb0, 0xe8, 0xbd, 0xe8, 0xbd,
};
static const uchar shape_zoom[] = {
	0xad, 0x7f, 0xf1, 0x7f, 0xff, 0x80, 0xf5, 0xb0, 0xda, 0xda, 0xb0, 0xf5, 0x80, 0xff,
	0x4f, 0xf5, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f, 0x0d, 0x7f, 0x52, 0x7f, 0x69, 0xb7,
	0x48, 0xb7, 0x80, 0xd8, 0xb8, 0xb7, 0x96, 0xb7, 0x96, 0xb7, 0x7f, 0x2f, 0x0d, 0x7f,
	0x00, 0x7f, 0x0a, 0x4f, 0x25, 0x25, 0x4f, 0x0a, 0x7f, 0x00, 0xb0, 0x0a, 0xda, 0x25,
	0xf5, 0x4f, 0xff, 0x80, 0xf1, 0x7f, 0xf1, 0x7f,
};


struct NavigateGizmoInfo {
	const char *opname;
	const char *gizmo;
	const unsigned char *shape;
	uint shape_size;
};

#define SHAPE_VARS(shape_id) shape = shape_id, .shape_size = ARRAY_SIZE(shape_id)

static struct NavigateGizmoInfo g_navigate_params[MPR_TOTAL] = {
	{
		.opname = "VIEW3D_OT_move",
		.gizmo = "GIZMO_GT_button_2d",
		.SHAPE_VARS(shape_pan),
	}, {
		.opname = "VIEW3D_OT_rotate",
		.gizmo = "VIEW3D_GT_navigate_rotate",
		.shape = NULL,
		.shape_size = 0,
	}, {
		.opname = "VIEW3D_OT_zoom",
		.gizmo = "GIZMO_GT_button_2d",
		.SHAPE_VARS(shape_zoom),
	}, {
		.opname = "VIEW3D_OT_view_persportho",
		.gizmo = "GIZMO_GT_button_2d",
		.SHAPE_VARS(shape_persp),
	}, {
		.opname = "VIEW3D_OT_view_persportho",
		.gizmo = "GIZMO_GT_button_2d",
		.SHAPE_VARS(shape_ortho),
	}, {
		.opname = "VIEW3D_OT_view_camera",
		.gizmo = "GIZMO_GT_button_2d",
		.SHAPE_VARS(shape_camera),
	},
};

#undef SHAPE_VARS

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
		gz->flag |= WM_GIZMO_GRAB_CURSOR | WM_GIZMO_DRAW_MODAL;

		if (i == MPR_ROTATE) {
			gz->color[3] = 0.0f;
			gz->color_hi[3] = 0.1f;
		}
		else {
			gz->color[3] = 0.2f;
			gz->color_hi[3] = 0.4f;
		}


		/* may be overwritten later */
		gz->scale_basis = (GIZMO_SIZE * GIZMO_MINI_FAC) / 2;
		if (info->shape != NULL) {
			PropertyRNA *prop = RNA_struct_find_property(gz->ptr, "shape");
			RNA_property_string_set_bytes(
			        gz->ptr, prop,
			        (const char *)info->shape, info->shape_size);
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
