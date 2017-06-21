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


static bool WIDGETGROUP_node_transform_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	SpaceNode *snode = CTX_wm_space_node(C);

	if ((snode->flag & SNODE_BACKDRAW) == 0) {
		return false;
	}

	if (snode && snode->edittree && snode->edittree->type == NTREE_COMPOSIT) {
		bNode *node = nodeGetActive(snode->edittree);

		if (node && node->type == CMP_NODE_VIEWER) {
			return true;
		}
	}

	return false;
}

static void WIDGETGROUP_node_transform_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = MEM_mallocN(sizeof(wmManipulatorWrapper), __func__);

	wwrapper->manipulator = WM_manipulator_new("MANIPULATOR_WT_cage_2d", mgroup, "backdrop_cage", NULL);

	RNA_enum_set(wwrapper->manipulator->ptr, "transform",
	             ED_MANIPULATOR_RECT_TRANSFORM_FLAG_TRANSLATE | ED_MANIPULATOR_RECT_TRANSFORM_FLAG_SCALE_UNIFORM);

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
		PointerRNA nodeptr;
		RNA_pointer_create(snode->id, &RNA_SpaceNodeEditor, snode, &nodeptr);
		WM_manipulator_target_property_def_rna(cage, "offset", &nodeptr, "backdrop_offset", -1);
		WM_manipulator_target_property_def_rna(cage, "scale", &nodeptr, "backdrop_zoom", -1);
	}
	else {
		WM_manipulator_set_flag(cage, WM_MANIPULATOR_HIDDEN, true);
	}

	BKE_image_release_ibuf(ima, ibuf, lock);
}

void NODE_WGT_backdrop_transform(wmManipulatorGroupType *wgt)
{
	wgt->name = "Backdrop Transform Widgets";
	wgt->idname = "NODE_WGT_backdrop_transform";

	wgt->flag |= WM_MANIPULATORGROUPTYPE_PERSISTENT;

	wgt->poll = WIDGETGROUP_node_transform_poll;
	wgt->setup = WIDGETGROUP_node_transform_setup;
	wgt->refresh = WIDGETGROUP_node_transform_refresh;
}
