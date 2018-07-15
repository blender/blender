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

/** \file blender/editors/space_view3d/view3d_gizmo_empty.c
 *  \ingroup spview3d
 */


#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_image.h"

#include "DNA_object_types.h"
#include "DNA_lamp_types.h"

#include "ED_screen.h"
#include "ED_gizmo_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */

/** \name Empty Image Gizmos
 * \{ */

struct EmptyImageWidgetGroup {
	wmGizmo *gizmo;
	struct {
		Object *ob;
		float dims[2];
	} state;
};

/* translate callbacks */
static void gizmo_empty_image_prop_matrix_get(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        void *value_p)
{
	float (*matrix)[4] = value_p;
	BLI_assert(gz_prop->type->array_length == 16);
	struct EmptyImageWidgetGroup *igzgroup = gz_prop->custom_func.user_data;
	const Object *ob = igzgroup->state.ob;

	unit_m4(matrix);
	matrix[0][0] = ob->empty_drawsize;
	matrix[1][1] = ob->empty_drawsize;

	float dims[2] = {0.0f, 0.0f};
	RNA_float_get_array(gz->ptr, "dimensions", dims);
	dims[0] *= ob->empty_drawsize;
	dims[1] *= ob->empty_drawsize;

	matrix[3][0] = (ob->ima_ofs[0] * dims[0]) + (0.5f * dims[0]);
	matrix[3][1] = (ob->ima_ofs[1] * dims[1]) + (0.5f * dims[1]);
}

static void gizmo_empty_image_prop_matrix_set(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        const void *value_p)
{
	const float (*matrix)[4] = value_p;
	BLI_assert(gz_prop->type->array_length == 16);
	struct EmptyImageWidgetGroup *igzgroup = gz_prop->custom_func.user_data;
	Object *ob = igzgroup->state.ob;

	ob->empty_drawsize = matrix[0][0];

	float dims[2];
	RNA_float_get_array(gz->ptr, "dimensions", dims);
	dims[0] *= ob->empty_drawsize;
	dims[1] *= ob->empty_drawsize;

	ob->ima_ofs[0] = (matrix[3][0] - (0.5f * dims[0])) / dims[0];
	ob->ima_ofs[1] = (matrix[3][1] - (0.5f * dims[1])) / dims[1];
}

static bool WIDGETGROUP_empty_image_poll(const bContext *C, wmGizmoGroupType *UNUSED(gzgt))
{
	View3D *v3d = CTX_wm_view3d(C);

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) ||
	    (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)))
	{
		return false;
	}

	Object *ob = CTX_data_active_object(C);

	if (ob && ob->type == OB_EMPTY) {
		return (ob->empty_drawtype == OB_EMPTY_IMAGE);
	}
	return false;
}

static void WIDGETGROUP_empty_image_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
	struct EmptyImageWidgetGroup *igzgroup = MEM_mallocN(sizeof(struct EmptyImageWidgetGroup), __func__);
	igzgroup->gizmo = WM_gizmo_new("GIZMO_GT_cage_2d", gzgroup, NULL);
	wmGizmo *gz = igzgroup->gizmo;
	RNA_enum_set(gz->ptr, "transform",
	             ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE);

	gzgroup->customdata = igzgroup;

	WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_HOVER, true);

	UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
	UI_GetThemeColor3fv(TH_GIZMO_HI, gz->color_hi);
}

static void WIDGETGROUP_empty_image_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
	struct EmptyImageWidgetGroup *igzgroup = gzgroup->customdata;
	Object *ob = CTX_data_active_object(C);
	wmGizmo *gz = igzgroup->gizmo;

	copy_m4_m4(gz->matrix_basis, ob->obmat);

	RNA_enum_set(gz->ptr, "transform",
	             ED_GIZMO_CAGE2D_XFORM_FLAG_TRANSLATE |
	             ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE |
	             ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE_UNIFORM);

	igzgroup->state.ob = ob;

	/* Use dimensions for aspect. */
	if (ob->data != NULL) {
		const Image *image = ob->data;
		ImageUser iuser = *ob->iuser;
		float size[2];
		BKE_image_get_size_fl(ob->data, &iuser, size);

		/* Get the image aspect even if the buffer is invalid */
		if (image->aspx > image->aspy) {
			size[1] *= image->aspy / image->aspx;
		}
		else if (image->aspx < image->aspy) {
			size[0] *= image->aspx / image->aspy;
		}

		const float dims_max = max_ff(size[0], size[1]);
		igzgroup->state.dims[0] = size[0] / dims_max;
		igzgroup->state.dims[1] = size[1] / dims_max;
	}
	else {
		copy_v2_fl(igzgroup->state.dims, 1.0f);
	}
	RNA_float_set_array(gz->ptr, "dimensions", igzgroup->state.dims);

	WM_gizmo_target_property_def_func(
	        gz, "matrix",
	        &(const struct wmGizmoPropertyFnParams) {
	            .value_get_fn = gizmo_empty_image_prop_matrix_get,
	            .value_set_fn = gizmo_empty_image_prop_matrix_set,
	            .range_get_fn = NULL,
	            .user_data = igzgroup,
	        });
}

void VIEW3D_GGT_empty_image(wmGizmoGroupType *gzgt)
{
	gzgt->name = "Area Light Widgets";
	gzgt->idname = "VIEW3D_GGT_empty_image";

	gzgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT |
	              WM_GIZMOGROUPTYPE_3D |
	              WM_GIZMOGROUPTYPE_DEPTH_3D);

	gzgt->poll = WIDGETGROUP_empty_image_poll;
	gzgt->setup = WIDGETGROUP_empty_image_setup;
	gzgt->refresh = WIDGETGROUP_empty_image_refresh;
}

/** \} */
