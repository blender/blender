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

/** \file blender/editors/space_view3d/view3d_manipulator_empty.c
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
#include "ED_manipulator_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */

/** \name Empty Image Manipulators
 * \{ */

struct EmptyImageWidgetGroup {
	wmManipulator *manipulator;
	struct {
		Object *ob;
		float dims[2];
	} state;
};

/* translate callbacks */
static void manipulator_empty_image_prop_matrix_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	float (*matrix)[4] = value_p;
	BLI_assert(mpr_prop->type->array_length == 16);
	struct EmptyImageWidgetGroup *imgroup = mpr_prop->custom_func.user_data;
	const Object *ob = imgroup->state.ob;

	unit_m4(matrix);
	matrix[0][0] = ob->empty_drawsize;
	matrix[1][1] = ob->empty_drawsize;

	float dims[2] = {0.0f, 0.0f};
	RNA_float_get_array(mpr->ptr, "dimensions", dims);
	dims[0] *= ob->empty_drawsize;
	dims[1] *= ob->empty_drawsize;

	matrix[3][0] = (ob->ima_ofs[0] * dims[0]) + (0.5f * dims[0]);
	matrix[3][1] = (ob->ima_ofs[1] * dims[1]) + (0.5f * dims[1]);
}

static void manipulator_empty_image_prop_matrix_set(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	const float (*matrix)[4] = value_p;
	BLI_assert(mpr_prop->type->array_length == 16);
	struct EmptyImageWidgetGroup *imgroup = mpr_prop->custom_func.user_data;
	Object *ob = imgroup->state.ob;

	ob->empty_drawsize = matrix[0][0];

	float dims[2];
	RNA_float_get_array(mpr->ptr, "dimensions", dims);
	dims[0] *= ob->empty_drawsize;
	dims[1] *= ob->empty_drawsize;

	ob->ima_ofs[0] = (matrix[3][0] - (0.5f * dims[0])) / dims[0];
	ob->ima_ofs[1] = (matrix[3][1] - (0.5f * dims[1])) / dims[1];
}

static bool WIDGETGROUP_empty_image_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	Object *ob = CTX_data_active_object(C);

	if (ob && ob->type == OB_EMPTY) {
		return (ob->empty_drawtype == OB_EMPTY_IMAGE);
	}
	return false;
}

static void WIDGETGROUP_empty_image_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	struct EmptyImageWidgetGroup *imgroup = MEM_mallocN(sizeof(struct EmptyImageWidgetGroup), __func__);
	imgroup->manipulator = WM_manipulator_new("MANIPULATOR_WT_cage_2d", mgroup, NULL);
	wmManipulator *mpr = imgroup->manipulator;
	RNA_enum_set(mpr->ptr, "transform",
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE);

	mgroup->customdata = imgroup;

	WM_manipulator_set_flag(mpr, WM_MANIPULATOR_DRAW_HOVER, true);

	UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, mpr->color);
	UI_GetThemeColor3fv(TH_MANIPULATOR_HI, mpr->color_hi);
}

static void WIDGETGROUP_empty_image_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	struct EmptyImageWidgetGroup *imgroup = mgroup->customdata;
	Object *ob = CTX_data_active_object(C);
	wmManipulator *mpr = imgroup->manipulator;

	copy_m4_m4(mpr->matrix_basis, ob->obmat);

	RNA_enum_set(mpr->ptr, "transform",
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE |
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE |
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM);

	imgroup->state.ob = ob;

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
		imgroup->state.dims[0] = size[0] / dims_max;
		imgroup->state.dims[1] = size[1] / dims_max;
	}
	else {
		copy_v2_fl(imgroup->state.dims, 1.0f);
	}
	RNA_float_set_array(mpr->ptr, "dimensions", imgroup->state.dims);

	WM_manipulator_target_property_def_func(
	        mpr, "matrix",
	        &(const struct wmManipulatorPropertyFnParams) {
	            .value_get_fn = manipulator_empty_image_prop_matrix_get,
	            .value_set_fn = manipulator_empty_image_prop_matrix_set,
	            .range_get_fn = NULL,
	            .user_data = imgroup,
	        });
}

void VIEW3D_WGT_empty_image(wmManipulatorGroupType *wgt)
{
	wgt->name = "Area Lamp Widgets";
	wgt->idname = "VIEW3D_WGT_empty_image";

	wgt->flag |= (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	              WM_MANIPULATORGROUPTYPE_3D |
	              WM_MANIPULATORGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_empty_image_poll;
	wgt->setup = WIDGETGROUP_empty_image_setup;
	wgt->refresh = WIDGETGROUP_empty_image_refresh;
}

/** \} */
