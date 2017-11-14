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

/** \file blender/editors/space_node/node_manipulators.c
 *  \ingroup spnode
 */

#include <math.h>

#include "BLI_utildefines.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_image.h"

#include "ED_screen.h"
#include "ED_manipulator_library.h"

#include "IMB_imbuf_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "node_intern.h"


/* -------------------------------------------------------------------- */

/** \name Local Utilities
 * \{ */

static void node_manipulator_calc_matrix_space(
        const SpaceNode *snode, const ARegion *ar, float matrix_space[4][4])
{
	unit_m4(matrix_space);
	mul_v3_fl(matrix_space[0], snode->zoom);
	mul_v3_fl(matrix_space[1], snode->zoom);
	matrix_space[3][0] = (ar->winx / 2) + snode->xof;
	matrix_space[3][1] = (ar->winy / 2) + snode->yof;
}

static void node_manipulator_calc_matrix_space_with_image_dims(
        const SpaceNode *snode, const ARegion *ar, const float image_dims[2], float matrix_space[4][4])
{
	unit_m4(matrix_space);
	mul_v3_fl(matrix_space[0], snode->zoom * image_dims[0]);
	mul_v3_fl(matrix_space[1], snode->zoom * image_dims[1]);
	matrix_space[3][0] = ((ar->winx / 2) + snode->xof) - ((image_dims[0] / 2.0f) * snode->zoom);
	matrix_space[3][1] = ((ar->winy / 2) + snode->yof) - ((image_dims[1] / 2.0f) * snode->zoom);
}

/** \} */



/* -------------------------------------------------------------------- */

/** \name Backdrop Manipulator
 * \{ */

static void manipulator_node_backdrop_prop_matrix_get(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	float (*matrix)[4] = value_p;
	BLI_assert(mpr_prop->type->array_length == 16);
	const SpaceNode *snode = mpr_prop->custom_func.user_data;
	matrix[0][0] = snode->zoom;
	matrix[1][1] = snode->zoom;
	matrix[3][0] = snode->xof;
	matrix[3][1] = snode->yof;
}

static void manipulator_node_backdrop_prop_matrix_set(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	const float (*matrix)[4] = value_p;
	BLI_assert(mpr_prop->type->array_length == 16);
	SpaceNode *snode = mpr_prop->custom_func.user_data;
	snode->zoom = matrix[0][0];
	snode->zoom = matrix[1][1];
	snode->xof  = matrix[3][0];
	snode->yof  = matrix[3][1];
}

static bool WIDGETGROUP_node_transform_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	SpaceNode *snode = CTX_wm_space_node(C);

	if ((snode->flag & SNODE_BACKDRAW) == 0) {
		return false;
	}

	if (snode && snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
		bNode *node = nodeGetActive(snode->edittree);

		if (node && ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
			return true;
		}
	}

	return false;
}

static void WIDGETGROUP_node_transform_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = MEM_mallocN(sizeof(wmManipulatorWrapper), __func__);

	wwrapper->manipulator = WM_manipulator_new("MANIPULATOR_WT_cage_2d", mgroup, NULL);

	RNA_enum_set(wwrapper->manipulator->ptr, "transform",
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE | ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM);

	mgroup->customdata = wwrapper;
}

static void WIDGETGROUP_node_transform_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	wmManipulator *cage = ((wmManipulatorWrapper *)mgroup->customdata)->manipulator;
	const ARegion *ar = CTX_wm_region(C);
	/* center is always at the origin */
	const float origin[3] = {ar->winx / 2, ar->winy / 2};

	void *lock;
	Image *ima = BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);

	if (ibuf) {
		const float dims[2] = {
			(ibuf->x > 0) ? ibuf->x : 64.0f,
			(ibuf->y > 0) ? ibuf->y : 64.0f,
		};

		RNA_float_set_array(cage->ptr, "dimensions", dims);
		WM_manipulator_set_matrix_location(cage, origin);
		WM_manipulator_set_flag(cage, WM_MANIPULATOR_HIDDEN, false);

		/* need to set property here for undo. TODO would prefer to do this in _init */
		SpaceNode *snode = CTX_wm_space_node(C);
#if 0
		PointerRNA nodeptr;
		RNA_pointer_create(snode->id, &RNA_SpaceNodeEditor, snode, &nodeptr);
		WM_manipulator_target_property_def_rna(cage, "offset", &nodeptr, "backdrop_offset", -1);
		WM_manipulator_target_property_def_rna(cage, "scale", &nodeptr, "backdrop_zoom", -1);
#endif

		WM_manipulator_target_property_def_func(
		        cage, "matrix",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_node_backdrop_prop_matrix_get,
		            .value_set_fn = manipulator_node_backdrop_prop_matrix_set,
		            .range_get_fn = NULL,
		            .user_data = snode,
		        });
	}
	else {
		WM_manipulator_set_flag(cage, WM_MANIPULATOR_HIDDEN, true);
	}

	BKE_image_release_ibuf(ima, ibuf, lock);
}

void NODE_WGT_backdrop_transform(wmManipulatorGroupType *wgt)
{
	wgt->name = "Backdrop Transform Widget";
	wgt->idname = "NODE_WGT_backdrop_transform";

	wgt->flag |= WM_MANIPULATORGROUPTYPE_PERSISTENT;

	wgt->poll = WIDGETGROUP_node_transform_poll;
	wgt->setup = WIDGETGROUP_node_transform_setup;
	wgt->refresh = WIDGETGROUP_node_transform_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Crop Manipulator
 * \{ */

struct NodeCropWidgetGroup {
	wmManipulator *border;

	struct {
		float dims[2];
	} state;

	struct {
		PointerRNA ptr;
		PropertyRNA *prop;
		bContext *context;
	} update_data;
};

static void manipulator_node_crop_update(struct NodeCropWidgetGroup *crop_group)
{
	RNA_property_update(crop_group->update_data.context, &crop_group->update_data.ptr, crop_group->update_data.prop);
}

static void two_xy_to_rect(const NodeTwoXYs *nxy, rctf *rect, const float dims[2], bool is_relative)
{
	if (is_relative) {
		rect->xmin = nxy->fac_x1;
		rect->xmax = nxy->fac_x2;
		rect->ymin = nxy->fac_y1;
		rect->ymax = nxy->fac_y2;
	}
	else {
		rect->xmin = nxy->x1 / dims[0];
		rect->xmax = nxy->x2 / dims[0];
		rect->ymin = nxy->y1 / dims[1];
		rect->ymax = nxy->y2 / dims[1];
	}
}

static void two_xy_from_rect(NodeTwoXYs *nxy, const rctf *rect, const float dims[2], bool is_relative)
{
	if (is_relative) {
		nxy->fac_x1 = rect->xmin;
		nxy->fac_x2 = rect->xmax;
		nxy->fac_y1 = rect->ymin;
		nxy->fac_y2 = rect->ymax;
	}
	else {
		nxy->x1 = rect->xmin * dims[0];
		nxy->x2 = rect->xmax * dims[0];
		nxy->y1 = rect->ymin * dims[1];
		nxy->y2 = rect->ymax * dims[1];
	}
}

/* scale callbacks */
static void manipulator_node_crop_prop_matrix_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	float (*matrix)[4] = value_p;
	BLI_assert(mpr_prop->type->array_length == 16);
	struct NodeCropWidgetGroup *crop_group = mpr->parent_mgroup->customdata;
	const float *dims = crop_group->state.dims;
	const bNode *node = mpr_prop->custom_func.user_data;
	const NodeTwoXYs *nxy = node->storage;
	bool is_relative = (bool)node->custom2;
	rctf rct;
	two_xy_to_rect(nxy, &rct, dims, is_relative);
	matrix[0][0] = BLI_rctf_size_x(&rct);
	matrix[1][1] = BLI_rctf_size_y(&rct);
	matrix[3][0] = (BLI_rctf_cent_x(&rct) - 0.5f) * dims[0];
	matrix[3][1] = (BLI_rctf_cent_y(&rct) - 0.5f) * dims[1];
}

static void manipulator_node_crop_prop_matrix_set(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	const float (*matrix)[4] = value_p;
	BLI_assert(mpr_prop->type->array_length == 16);
	struct NodeCropWidgetGroup *crop_group = mpr->parent_mgroup->customdata;
	const float *dims = crop_group->state.dims;
	bNode *node = mpr_prop->custom_func.user_data;
	NodeTwoXYs *nxy = node->storage;
	bool is_relative = (bool)node->custom2;
	rctf rct;
	two_xy_to_rect(nxy, &rct, dims, is_relative);
	BLI_rctf_resize(&rct, matrix[0][0], matrix[1][1]);
	BLI_rctf_recenter(&rct, (matrix[3][0] / dims[0]) + 0.5f, (matrix[3][1] / dims[1]) + 0.5f);
	BLI_rctf_isect(&(rctf){.xmin = 0, .ymin = 0, .xmax = 1, .ymax = 1}, &rct, &rct);
	two_xy_from_rect(nxy, &rct, dims, is_relative);
	manipulator_node_crop_update(crop_group);
}

static bool WIDGETGROUP_node_crop_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	SpaceNode *snode = CTX_wm_space_node(C);

	if ((snode->flag & SNODE_BACKDRAW) == 0) {
		return false;
	}

	if (snode && snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
		bNode *node = nodeGetActive(snode->edittree);

		if (node && ELEM(node->type, CMP_NODE_CROP)) {
			/* ignore 'use_crop_size', we can't usefully edit the crop in this case. */
			if ((node->custom1 & (0 << 1)) == 0) {
				return true;
			}
		}
	}

	return false;
}

static void WIDGETGROUP_node_crop_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	struct NodeCropWidgetGroup *crop_group = MEM_mallocN(sizeof(struct NodeCropWidgetGroup), __func__);

	crop_group->border = WM_manipulator_new("MANIPULATOR_WT_cage_2d", mgroup, NULL);

	RNA_enum_set(crop_group->border->ptr, "transform",
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE | ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE);

	mgroup->customdata = crop_group;
}

static void WIDGETGROUP_node_crop_draw_prepare(const bContext *C, wmManipulatorGroup *mgroup)
{
	ARegion *ar = CTX_wm_region(C);
	wmManipulator *mpr = mgroup->manipulators.first;

	SpaceNode *snode = CTX_wm_space_node(C);

	node_manipulator_calc_matrix_space(snode, ar, mpr->matrix_space);
}

static void WIDGETGROUP_node_crop_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	struct NodeCropWidgetGroup *crop_group = mgroup->customdata;
	wmManipulator *mpr = crop_group->border;

	void *lock;
	Image *ima = BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);

	if (ibuf) {
		crop_group->state.dims[0] = (ibuf->x > 0) ? ibuf->x : 64.0f;
		crop_group->state.dims[1] = (ibuf->y > 0) ? ibuf->y : 64.0f;

		RNA_float_set_array(mpr->ptr, "dimensions", crop_group->state.dims);
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_HIDDEN, false);

		SpaceNode *snode = CTX_wm_space_node(C);
		bNode *node = nodeGetActive(snode->edittree);

		crop_group->update_data.context = (bContext *)C;
		RNA_pointer_create((ID *)snode->edittree, &RNA_CompositorNodeCrop, node, &crop_group->update_data.ptr);
		crop_group->update_data.prop = RNA_struct_find_property(&crop_group->update_data.ptr, "relative");

		WM_manipulator_target_property_def_func(
		        mpr, "matrix",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_node_crop_prop_matrix_get,
		            .value_set_fn = manipulator_node_crop_prop_matrix_set,
		            .range_get_fn = NULL,
		            .user_data = node,
		        });
	}
	else {
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_HIDDEN, true);
	}

	BKE_image_release_ibuf(ima, ibuf, lock);
}

void NODE_WGT_backdrop_crop(wmManipulatorGroupType *wgt)
{
	wgt->name = "Backdrop Crop Widget";
	wgt->idname = "NODE_WGT_backdrop_crop";

	wgt->flag |= WM_MANIPULATORGROUPTYPE_PERSISTENT;

	wgt->poll = WIDGETGROUP_node_crop_poll;
	wgt->setup = WIDGETGROUP_node_crop_setup;
	wgt->draw_prepare = WIDGETGROUP_node_crop_draw_prepare;
	wgt->refresh = WIDGETGROUP_node_crop_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Sun Beams
 * \{ */

struct NodeSunBeamsWidgetGroup {
	wmManipulator *manipulator;

	struct {
		float dims[2];
	} state;
};

static bool WIDGETGROUP_node_sbeam_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	SpaceNode *snode = CTX_wm_space_node(C);

	if ((snode->flag & SNODE_BACKDRAW) == 0) {
		return false;
	}

	if (snode && snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
		bNode *node = nodeGetActive(snode->edittree);

		if (node && ELEM(node->type, CMP_NODE_SUNBEAMS)) {
			return true;
		}
	}

	return false;
}

static void WIDGETGROUP_node_sbeam_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	struct NodeSunBeamsWidgetGroup *sbeam_group = MEM_mallocN(sizeof(struct NodeSunBeamsWidgetGroup), __func__);

	sbeam_group->manipulator = WM_manipulator_new("MANIPULATOR_WT_grab_3d", mgroup, NULL);
	wmManipulator *mpr = sbeam_group->manipulator;

	RNA_enum_set(mpr->ptr, "draw_style",  ED_MANIPULATOR_GRAB_STYLE_CROSS_2D);

	mpr->scale_basis = 0.05f;

	mgroup->customdata = sbeam_group;
}

static void WIDGETGROUP_node_sbeam_draw_prepare(const bContext *C, wmManipulatorGroup *mgroup)
{
	struct NodeSunBeamsWidgetGroup *sbeam_group = mgroup->customdata;
	ARegion *ar = CTX_wm_region(C);
	wmManipulator *mpr = mgroup->manipulators.first;

	SpaceNode *snode = CTX_wm_space_node(C);

	node_manipulator_calc_matrix_space_with_image_dims(snode, ar, sbeam_group->state.dims, mpr->matrix_space);
}

static void WIDGETGROUP_node_sbeam_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	struct NodeSunBeamsWidgetGroup *sbeam_group = mgroup->customdata;
	wmManipulator *mpr = sbeam_group->manipulator;

	void *lock;
	Image *ima = BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);

	if (ibuf) {
		sbeam_group->state.dims[0] = (ibuf->x > 0) ? ibuf->x : 64.0f;
		sbeam_group->state.dims[1] = (ibuf->y > 0) ? ibuf->y : 64.0f;

		SpaceNode *snode = CTX_wm_space_node(C);
		bNode *node = nodeGetActive(snode->edittree);

		/* need to set property here for undo. TODO would prefer to do this in _init */
		PointerRNA nodeptr;
		RNA_pointer_create((ID *)snode->edittree, &RNA_CompositorNodeSunBeams, node, &nodeptr);
		WM_manipulator_target_property_def_rna(mpr, "offset", &nodeptr, "source", -1);

		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_DRAW_MODAL, true);
	}
	else {
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_HIDDEN, true);
	}

	BKE_image_release_ibuf(ima, ibuf, lock);
}

void NODE_WGT_backdrop_sun_beams(wmManipulatorGroupType *wgt)
{
	wgt->name = "Sun Beams Widget";
	wgt->idname = "NODE_WGT_sbeam";

	wgt->flag |= WM_MANIPULATORGROUPTYPE_PERSISTENT;

	wgt->poll = WIDGETGROUP_node_sbeam_poll;
	wgt->setup = WIDGETGROUP_node_sbeam_setup;
	wgt->draw_prepare = WIDGETGROUP_node_sbeam_draw_prepare;
	wgt->refresh = WIDGETGROUP_node_sbeam_refresh;
}

/** \} */



/* -------------------------------------------------------------------- */

/** \name Corner Pin
 * \{ */

struct NodeCornerPinWidgetGroup {
	wmManipulator *manipulators[4];

	struct {
		float dims[2];
	} state;
};

static bool WIDGETGROUP_node_corner_pin_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	SpaceNode *snode = CTX_wm_space_node(C);

	if ((snode->flag & SNODE_BACKDRAW) == 0) {
		return false;
	}

	if (snode && snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
		bNode *node = nodeGetActive(snode->edittree);

		if (node && ELEM(node->type, CMP_NODE_CORNERPIN)) {
			return true;
		}
	}

	return false;
}

static void WIDGETGROUP_node_corner_pin_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	struct NodeCornerPinWidgetGroup *cpin_group = MEM_mallocN(sizeof(struct NodeCornerPinWidgetGroup), __func__);
	const wmManipulatorType *wt_grab_3d = WM_manipulatortype_find("MANIPULATOR_WT_grab_3d", false);

	for (int i = 0; i < 4; i++) {
		cpin_group->manipulators[i] = WM_manipulator_new_ptr(wt_grab_3d, mgroup, NULL);
		wmManipulator *mpr = cpin_group->manipulators[i];

		RNA_enum_set(mpr->ptr, "draw_style",  ED_MANIPULATOR_GRAB_STYLE_CROSS_2D);

		mpr->scale_basis = 0.01f;
	}

	mgroup->customdata = cpin_group;
}

static void WIDGETGROUP_node_corner_pin_draw_prepare(const bContext *C, wmManipulatorGroup *mgroup)
{
	struct NodeCornerPinWidgetGroup *cpin_group = mgroup->customdata;
	ARegion *ar = CTX_wm_region(C);

	SpaceNode *snode = CTX_wm_space_node(C);

	float matrix_space[4][4];
	node_manipulator_calc_matrix_space_with_image_dims(snode, ar, cpin_group->state.dims, matrix_space);

	for (int i = 0; i < 4; i++) {
		wmManipulator *mpr = cpin_group->manipulators[i];
		copy_m4_m4(mpr->matrix_space, matrix_space);
	}
}

static void WIDGETGROUP_node_corner_pin_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	struct NodeCornerPinWidgetGroup *cpin_group = mgroup->customdata;

	void *lock;
	Image *ima = BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock);

	if (ibuf) {
		cpin_group->state.dims[0] = (ibuf->x > 0) ? ibuf->x : 64.0f;
		cpin_group->state.dims[1] = (ibuf->y > 0) ? ibuf->y : 64.0f;

		SpaceNode *snode = CTX_wm_space_node(C);
		bNode *node = nodeGetActive(snode->edittree);

		/* need to set property here for undo. TODO would prefer to do this in _init */
		int i = 0;
		for (bNodeSocket *sock = node->inputs.first; sock && i < 4; sock = sock->next) {
			if (sock->type == SOCK_VECTOR) {
				wmManipulator *mpr = cpin_group->manipulators[i++];

				PointerRNA sockptr;
				RNA_pointer_create((ID *)snode->edittree, &RNA_NodeSocket, sock, &sockptr);
				WM_manipulator_target_property_def_rna(mpr, "offset", &sockptr, "default_value", -1);

				WM_manipulator_set_flag(mpr, WM_MANIPULATOR_DRAW_MODAL, true);
			}
		}
	}
	else {
		for (int i = 0; i < 4; i++) {
			wmManipulator *mpr = cpin_group->manipulators[i];
			WM_manipulator_set_flag(mpr, WM_MANIPULATOR_HIDDEN, true);
		}
	}

	BKE_image_release_ibuf(ima, ibuf, lock);
}

void NODE_WGT_backdrop_corner_pin(wmManipulatorGroupType *wgt)
{
	wgt->name = "Corner Pin Widget";
	wgt->idname = "NODE_WGT_backdrop_corner_pin";

	wgt->flag |= WM_MANIPULATORGROUPTYPE_PERSISTENT;

	wgt->poll = WIDGETGROUP_node_corner_pin_poll;
	wgt->setup = WIDGETGROUP_node_corner_pin_setup;
	wgt->draw_prepare = WIDGETGROUP_node_corner_pin_draw_prepare;
	wgt->refresh = WIDGETGROUP_node_corner_pin_refresh;
}

/** \} */
