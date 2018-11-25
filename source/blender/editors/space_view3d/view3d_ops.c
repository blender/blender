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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_ops.c
 *  \ingroup spview3d
 */


#include <stdlib.h>
#include <math.h>


#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_blender_copybuffer.h"
#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_transform.h"

#include "view3d_intern.h"

#ifdef WIN32
#  include "BLI_math_base.h" /* M_PI */
#endif

/* ************************** copy paste ***************************** */

static int view3d_copybuffer_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	char str[FILE_MAX];

	BKE_copybuffer_begin(bmain);

	/* context, selection, could be generalized */
	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		BKE_copybuffer_tag_ID(&ob->id);
	}
	CTX_DATA_END;

	for (Collection *collection = bmain->collection.first; collection; collection = collection->id.next) {
		for (CollectionObject *cob = collection->gobject.first; cob; cob = cob->next) {
			Object *object = cob->ob;

			if (object && (object->id.tag & LIB_TAG_DOIT)) {
				BKE_copybuffer_tag_ID(&collection->id);
				/* don't expand out to all other objects */
				collection->id.tag &= ~LIB_TAG_NEED_EXPAND;
				break;
			}
		}
	}

	BLI_make_file_string("/", str, BKE_tempdir_base(), "copybuffer.blend");
	BKE_copybuffer_save(bmain, str, op->reports);

	BKE_report(op->reports, RPT_INFO, "Copied selected objects to buffer");

	return OPERATOR_FINISHED;
}

static void VIEW3D_OT_copybuffer(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Copy Selection to Buffer";
	ot->idname = "VIEW3D_OT_copybuffer";
	ot->description = "Selected objects are saved in a temp file";

	/* api callbacks */
	ot->exec = view3d_copybuffer_exec;
	ot->poll = ED_operator_scene;
}

static int view3d_pastebuffer_exec(bContext *C, wmOperator *op)
{
	char str[FILE_MAX];
	short flag = 0;

	if (RNA_boolean_get(op->ptr, "autoselect"))
		flag |= FILE_AUTOSELECT;
	if (RNA_boolean_get(op->ptr, "active_collection"))
		flag |= FILE_ACTIVE_COLLECTION;

	BLI_make_file_string("/", str, BKE_tempdir_base(), "copybuffer.blend");
	if (BKE_copybuffer_paste(C, str, flag, op->reports)) {
		WM_event_add_notifier(C, NC_WINDOW, NULL);

		BKE_report(op->reports, RPT_INFO, "Objects pasted from buffer");

		return OPERATOR_FINISHED;
	}

	BKE_report(op->reports, RPT_INFO, "No buffer to paste from");

	return OPERATOR_CANCELLED;
}

static void VIEW3D_OT_pastebuffer(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Paste Selection from Buffer";
	ot->idname = "VIEW3D_OT_pastebuffer";
	ot->description = "Contents of copy buffer gets pasted";

	/* api callbacks */
	ot->exec = view3d_pastebuffer_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "autoselect", true, "Select", "Select pasted objects");
	RNA_def_boolean(ot->srna, "active_collection", true, "Active Collection", "Put pasted objects on the active collection");
}

/* ************************** registration **********************************/

void view3d_operatortypes(void)
{
	WM_operatortype_append(VIEW3D_OT_rotate);
	WM_operatortype_append(VIEW3D_OT_move);
	WM_operatortype_append(VIEW3D_OT_zoom);
	WM_operatortype_append(VIEW3D_OT_zoom_camera_1_to_1);
	WM_operatortype_append(VIEW3D_OT_dolly);
#ifdef WITH_INPUT_NDOF
	WM_operatortype_append(VIEW3D_OT_ndof_orbit_zoom);
	WM_operatortype_append(VIEW3D_OT_ndof_orbit);
	WM_operatortype_append(VIEW3D_OT_ndof_pan);
	WM_operatortype_append(VIEW3D_OT_ndof_all);
#endif /* WITH_INPUT_NDOF */
	WM_operatortype_append(VIEW3D_OT_view_all);
	WM_operatortype_append(VIEW3D_OT_view_axis);
	WM_operatortype_append(VIEW3D_OT_view_camera);
	WM_operatortype_append(VIEW3D_OT_view_orbit);
	WM_operatortype_append(VIEW3D_OT_view_roll);
	WM_operatortype_append(VIEW3D_OT_view_pan);
	WM_operatortype_append(VIEW3D_OT_view_persportho);
	WM_operatortype_append(VIEW3D_OT_background_image_add);
	WM_operatortype_append(VIEW3D_OT_background_image_remove);
	WM_operatortype_append(VIEW3D_OT_view_selected);
	WM_operatortype_append(VIEW3D_OT_view_lock_clear);
	WM_operatortype_append(VIEW3D_OT_view_lock_to_active);
	WM_operatortype_append(VIEW3D_OT_view_center_cursor);
	WM_operatortype_append(VIEW3D_OT_view_center_pick);
	WM_operatortype_append(VIEW3D_OT_view_center_camera);
	WM_operatortype_append(VIEW3D_OT_view_center_lock);
	WM_operatortype_append(VIEW3D_OT_select);
	WM_operatortype_append(VIEW3D_OT_select_box);
	// WM_operatortype_append(VIEW3D_OT_clip_border);
	WM_operatortype_append(VIEW3D_OT_select_circle);
	WM_operatortype_append(VIEW3D_OT_smoothview);
	WM_operatortype_append(VIEW3D_OT_render_border);
	WM_operatortype_append(VIEW3D_OT_clear_render_border);
	WM_operatortype_append(VIEW3D_OT_zoom_border);
	WM_operatortype_append(VIEW3D_OT_cursor3d);
	WM_operatortype_append(VIEW3D_OT_select_lasso);
	WM_operatortype_append(VIEW3D_OT_select_menu);
	WM_operatortype_append(VIEW3D_OT_camera_to_view);
	WM_operatortype_append(VIEW3D_OT_camera_to_view_selected);
	WM_operatortype_append(VIEW3D_OT_object_as_camera);
	WM_operatortype_append(VIEW3D_OT_localview);
	WM_operatortype_append(VIEW3D_OT_fly);
	WM_operatortype_append(VIEW3D_OT_walk);
	WM_operatortype_append(VIEW3D_OT_navigate);
	WM_operatortype_append(VIEW3D_OT_ruler);
	WM_operatortype_append(VIEW3D_OT_copybuffer);
	WM_operatortype_append(VIEW3D_OT_pastebuffer);

	WM_operatortype_append(VIEW3D_OT_properties);
	WM_operatortype_append(VIEW3D_OT_object_mode_pie_or_toggle);
	WM_operatortype_append(VIEW3D_OT_toolshelf);

	WM_operatortype_append(VIEW3D_OT_snap_selected_to_grid);
	WM_operatortype_append(VIEW3D_OT_snap_selected_to_cursor);
	WM_operatortype_append(VIEW3D_OT_snap_selected_to_active);
	WM_operatortype_append(VIEW3D_OT_snap_cursor_to_grid);
	WM_operatortype_append(VIEW3D_OT_snap_cursor_to_center);
	WM_operatortype_append(VIEW3D_OT_snap_cursor_to_selected);
	WM_operatortype_append(VIEW3D_OT_snap_cursor_to_active);

	WM_operatortype_append(VIEW3D_OT_toggle_shading);
	WM_operatortype_append(VIEW3D_OT_toggle_xray);
	WM_operatortype_append(VIEW3D_OT_toggle_matcap_flip);

	WM_operatortype_append(VIEW3D_OT_ruler_add);

	transform_operatortypes();
}

void view3d_keymap(wmKeyConfig *keyconf)
{
	WM_keymap_ensure(keyconf, "3D View Generic", SPACE_VIEW3D, 0);

	/* only for region 3D window */
	WM_keymap_ensure(keyconf, "3D View", SPACE_VIEW3D, 0);

	fly_modal_keymap(keyconf);
	walk_modal_keymap(keyconf);
	viewrotate_modal_keymap(keyconf);
	viewmove_modal_keymap(keyconf);
	viewzoom_modal_keymap(keyconf);
	viewdolly_modal_keymap(keyconf);
}
