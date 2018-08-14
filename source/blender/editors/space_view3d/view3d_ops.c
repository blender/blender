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


#include "DNA_object_types.h"
#include "DNA_group_types.h"
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
	WM_operatortype_append(VIEW3D_OT_select_border);
	WM_operatortype_append(VIEW3D_OT_clip_border);
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
	WM_operatortype_append(VIEW3D_OT_fly);
	WM_operatortype_append(VIEW3D_OT_walk);
	WM_operatortype_append(VIEW3D_OT_navigate);
	WM_operatortype_append(VIEW3D_OT_ruler);
	WM_operatortype_append(VIEW3D_OT_layers);
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
	WM_operatortype_append(VIEW3D_OT_toggle_matcap_flip);

	WM_operatortype_append(VIEW3D_OT_ruler_add);

	transform_operatortypes();
}

void view3d_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;

	keymap = WM_keymap_find(keyconf, "3D View Generic", SPACE_VIEW3D, 0);

	WM_keymap_add_item(keymap, "VIEW3D_OT_properties", NKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_toolshelf", TKEY, KM_PRESS, 0, 0);

	/* only for region 3D window */
	keymap = WM_keymap_find(keyconf, "3D View", SPACE_VIEW3D, 0);

	WM_keymap_verify_item(keymap, "VIEW3D_OT_cursor3d", ACTIONMOUSE, KM_CLICK, 0, 0);

	WM_keymap_verify_item(keymap, "VIEW3D_OT_rotate", MIDDLEMOUSE, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_move", MIDDLEMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_zoom", MIDDLEMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_dolly", MIDDLEMOUSE, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_selected", PADPERIOD, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "use_all_regions", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "use_all_regions", false);

#ifdef USE_WM_KEYMAP_27X
	WM_keymap_verify_item(keymap, "VIEW3D_OT_view_lock_to_active", PADPERIOD, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_view_lock_clear", PADPERIOD, KM_PRESS, KM_ALT, 0);

	WM_keymap_verify_item(keymap, "VIEW3D_OT_navigate", FKEY, KM_PRESS, KM_SHIFT, 0);
#endif

	WM_keymap_verify_item(keymap, "VIEW3D_OT_smoothview", TIMER1, KM_ANY, KM_ANY, 0);

	WM_keymap_add_item(keymap, "VIEW3D_OT_rotate", MOUSEPAN, 0, 0, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_rotate", MOUSEROTATE, 0, 0, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_move", MOUSEPAN, 0, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", MOUSEZOOM, 0, 0, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", MOUSEPAN, 0, KM_CTRL, 0);

	/*numpad +/-*/
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", PADPLUSKEY, KM_PRESS, 0, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", PADMINUS, KM_PRESS, 0, 0)->ptr, "delta", -1);
	/*ctrl +/-*/
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", EQUALKEY, KM_PRESS, KM_CTRL, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", MINUSKEY, KM_PRESS, KM_CTRL, 0)->ptr, "delta", -1);

	/*wheel mouse forward/back*/
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", WHEELINMOUSE, KM_PRESS, 0, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", WHEELOUTMOUSE, KM_PRESS, 0, 0)->ptr, "delta", -1);

	/* ... and for dolly */
	/*numpad +/-*/
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_dolly", PADPLUSKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_dolly", PADMINUS, KM_PRESS, KM_SHIFT, 0)->ptr, "delta", -1);
	/*ctrl +/-*/
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_dolly", EQUALKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_dolly", MINUSKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0)->ptr, "delta", -1);

#ifdef USE_WM_KEYMAP_27X
	WM_keymap_add_item(keymap, "VIEW3D_OT_zoom_camera_1_to_1", PADENTER, KM_PRESS, KM_SHIFT, 0);
#endif

	WM_keymap_add_item(keymap, "VIEW3D_OT_view_center_camera", HOMEKEY, KM_PRESS, 0, 0); /* only with camera view */
	WM_keymap_add_item(keymap, "VIEW3D_OT_view_center_lock", HOMEKEY, KM_PRESS, 0, 0); /* only with lock view */

#ifdef USE_WM_KEYMAP_27X
	WM_keymap_add_item(keymap, "VIEW3D_OT_view_center_cursor", HOMEKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_view_center_pick", FKEY, KM_PRESS, KM_ALT, 0);
#endif

	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "center", false); /* only without camera view */
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_all", HOMEKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "use_all_regions", true);
	RNA_boolean_set(kmi->ptr, "center", false); /* only without camera view */
#ifdef USE_WM_KEYMAP_27X
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_all", CKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "center", true);
#endif

	WM_keymap_add_menu_pie(keymap, "VIEW3D_MT_view_pie", ACCENTGRAVEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_navigate", ACCENTGRAVEKEY, KM_PRESS, KM_SHIFT, 0);

	/* numpad view hotkeys*/
	WM_keymap_add_item(keymap, "VIEW3D_OT_view_camera", PAD0, KM_PRESS, 0, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD1, KM_PRESS, 0, 0)->ptr, "type", RV3D_VIEW_FRONT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", PAD2, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPDOWN);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD3, KM_PRESS, 0, 0)->ptr, "type", RV3D_VIEW_RIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", PAD4, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPLEFT);
	WM_keymap_add_item(keymap, "VIEW3D_OT_view_persportho", PAD5, KM_PRESS, 0, 0);

	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", PAD6, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPRIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD7, KM_PRESS, 0, 0)->ptr, "type", RV3D_VIEW_TOP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", PAD8, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPUP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD1, KM_PRESS, KM_CTRL, 0)->ptr, "type", RV3D_VIEW_BACK);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD3, KM_PRESS, KM_CTRL, 0)->ptr, "type", RV3D_VIEW_LEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD7, KM_PRESS, KM_CTRL, 0)->ptr, "type", RV3D_VIEW_BOTTOM);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", PAD2, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANDOWN);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", PAD4, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANLEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", PAD6, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANRIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", PAD8, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANUP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_roll", PAD4, KM_PRESS, KM_SHIFT, 0)->ptr, "type", V3D_VIEW_STEPLEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_roll", PAD6, KM_PRESS, KM_SHIFT, 0)->ptr, "type", V3D_VIEW_STEPRIGHT);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", PAD9, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "type", V3D_VIEW_STEPRIGHT);
	RNA_float_set(kmi->ptr, "angle", (float)M_PI);

#ifdef USE_WM_KEYMAP_27X
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", WHEELUPMOUSE, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANRIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", WHEELDOWNMOUSE, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANLEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", WHEELUPMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "type", V3D_VIEW_PANUP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", WHEELDOWNMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "type", V3D_VIEW_PANDOWN);

	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", WHEELUPMOUSE, KM_PRESS, KM_CTRL | KM_ALT, 0)->ptr, "type", V3D_VIEW_STEPLEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", WHEELDOWNMOUSE, KM_PRESS, KM_CTRL | KM_ALT, 0)->ptr, "type", V3D_VIEW_STEPRIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", WHEELUPMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0)->ptr, "type", V3D_VIEW_STEPUP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", WHEELDOWNMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0)->ptr, "type", V3D_VIEW_STEPDOWN);

	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_roll", WHEELUPMOUSE, KM_PRESS, KM_CTRL | KM_SHIFT, 0)->ptr, "type", V3D_VIEW_STEPLEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_roll", WHEELDOWNMOUSE, KM_PRESS, KM_CTRL | KM_SHIFT, 0)->ptr, "type", V3D_VIEW_STEPRIGHT);
#endif

	/* active aligned, replaces '*' key in 2.4x */
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD1, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_FRONT);
	RNA_boolean_set(kmi->ptr, "align_active", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD3, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_RIGHT);
	RNA_boolean_set(kmi->ptr, "align_active", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD7, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_TOP);
	RNA_boolean_set(kmi->ptr, "align_active", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD1, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_BACK);
	RNA_boolean_set(kmi->ptr, "align_active", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD3, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_LEFT);
	RNA_boolean_set(kmi->ptr, "align_active", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", PAD7, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_BOTTOM);
	RNA_boolean_set(kmi->ptr, "align_active", true);

	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", EVT_TWEAK_M, EVT_GESTURE_N, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_TOP);
	RNA_boolean_set(kmi->ptr, "relative", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", EVT_TWEAK_M, EVT_GESTURE_S, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_BOTTOM);
	RNA_boolean_set(kmi->ptr, "relative", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", EVT_TWEAK_M, EVT_GESTURE_E, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_RIGHT);
	RNA_boolean_set(kmi->ptr, "relative", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", EVT_TWEAK_M, EVT_GESTURE_W, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_LEFT);
	RNA_boolean_set(kmi->ptr, "relative", true);

#ifdef WITH_INPUT_NDOF
	/* note: positioned here so keymaps show keyboard keys if assigned */
	/* 3D mouse */
	WM_keymap_add_item(keymap, "VIEW3D_OT_ndof_orbit_zoom", NDOF_MOTION, 0, 0, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_ndof_orbit", NDOF_MOTION, 0, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_ndof_pan", NDOF_MOTION, 0, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_ndof_all", NDOF_MOTION, 0, KM_CTRL | KM_SHIFT, 0);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_selected", NDOF_BUTTON_FIT, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "use_all_regions", false);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_roll", NDOF_BUTTON_ROLL_CCW, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPLEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_roll", NDOF_BUTTON_ROLL_CCW, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPRIGHT);

	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", NDOF_BUTTON_FRONT, KM_PRESS, 0, 0)->ptr, "type", RV3D_VIEW_FRONT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", NDOF_BUTTON_BACK, KM_PRESS, 0, 0)->ptr, "type", RV3D_VIEW_BACK);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", NDOF_BUTTON_LEFT, KM_PRESS, 0, 0)->ptr, "type", RV3D_VIEW_LEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", NDOF_BUTTON_RIGHT, KM_PRESS, 0, 0)->ptr, "type", RV3D_VIEW_RIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", NDOF_BUTTON_TOP, KM_PRESS, 0, 0)->ptr, "type", RV3D_VIEW_TOP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", NDOF_BUTTON_BOTTOM, KM_PRESS, 0, 0)->ptr, "type", RV3D_VIEW_BOTTOM);

	/* 3D mouse align */
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", NDOF_BUTTON_FRONT, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_FRONT);
	RNA_boolean_set(kmi->ptr, "align_active", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", NDOF_BUTTON_RIGHT, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_RIGHT);
	RNA_boolean_set(kmi->ptr, "align_active", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_view_axis", NDOF_BUTTON_TOP, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "type", RV3D_VIEW_TOP);
	RNA_boolean_set(kmi->ptr, "align_active", true);
#endif /* WITH_INPUT_NDOF */

	/* drawtype */
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_toggle_shading", ZKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "type", OB_SOLID);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_toggle_shading", ZKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "type", OB_MATERIAL);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_toggle_shading", ZKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "type", OB_RENDER);

	/* selection*/
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", false);
	RNA_boolean_set(kmi->ptr, "center", false);
	RNA_boolean_set(kmi->ptr, "object", false);
	RNA_boolean_set(kmi->ptr, "enumerate", false);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", true);
	RNA_boolean_set(kmi->ptr, "center", false);
	RNA_boolean_set(kmi->ptr, "object", false);
	RNA_boolean_set(kmi->ptr, "enumerate", false);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", false);
	RNA_boolean_set(kmi->ptr, "center", true);
	RNA_boolean_set(kmi->ptr, "object", true); /* use Ctrl+Select for 2 purposes */
	RNA_boolean_set(kmi->ptr, "enumerate", false);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", false);
	RNA_boolean_set(kmi->ptr, "center", false);
	RNA_boolean_set(kmi->ptr, "object", false);
	RNA_boolean_set(kmi->ptr, "enumerate", true);

	/* selection key-combinations */
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "extend", true);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", true);
	RNA_boolean_set(kmi->ptr, "center", true);
	RNA_boolean_set(kmi->ptr, "object", false);
	RNA_boolean_set(kmi->ptr, "enumerate", false);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", false);
	RNA_boolean_set(kmi->ptr, "center", true);
	RNA_boolean_set(kmi->ptr, "object", false);
	RNA_boolean_set(kmi->ptr, "enumerate", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", true);
	RNA_boolean_set(kmi->ptr, "center", false);
	RNA_boolean_set(kmi->ptr, "object", false);
	RNA_boolean_set(kmi->ptr, "enumerate", true);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT | KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", true);
	RNA_boolean_set(kmi->ptr, "center", true);
	RNA_boolean_set(kmi->ptr, "object", false);
	RNA_boolean_set(kmi->ptr, "enumerate", true);

	WM_keymap_add_item(keymap, "VIEW3D_OT_select_border", BKEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "mode", SEL_OP_ADD);
	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_SHIFT | KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "mode", SEL_OP_SUB);
	WM_keymap_add_item(keymap, "VIEW3D_OT_select_circle", CKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "VIEW3D_OT_clip_border", BKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_zoom_border", BKEY, KM_PRESS, KM_SHIFT, 0);

	kmi = WM_keymap_add_item(keymap, "VIEW3D_OT_render_border", BKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "VIEW3D_OT_clear_render_border", BKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);

	WM_keymap_add_item(keymap, "VIEW3D_OT_camera_to_view", PAD0, KM_PRESS, KM_ALT | KM_CTRL, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_object_as_camera", PAD0, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_menu(keymap, "VIEW3D_MT_snap", SKEY, KM_PRESS, KM_SHIFT, 0);

#ifdef __APPLE__
	WM_keymap_add_item(keymap, "VIEW3D_OT_copybuffer", CKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_pastebuffer", VKEY, KM_PRESS, KM_OSKEY, 0);
#endif
	WM_keymap_add_item(keymap, "VIEW3D_OT_copybuffer", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_pastebuffer", VKEY, KM_PRESS, KM_CTRL, 0);

	/* context ops */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", COMMAKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.transform_pivot_point");
	RNA_string_set(kmi->ptr, "value", "BOUNDING_BOX_CENTER");

	/* 2.4x allowed Comma+Shift too, rather not use both */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", COMMAKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.transform_pivot_point");
	RNA_string_set(kmi->ptr, "value", "MEDIAN_POINT");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", COMMAKEY, KM_PRESS, KM_ALT, 0); /* new in 2.5 */
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.use_transform_pivot_point_align");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", PERIODKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.transform_pivot_point");
	RNA_string_set(kmi->ptr, "value", "CURSOR");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", PERIODKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.transform_pivot_point");
	RNA_string_set(kmi->ptr, "value", "INDIVIDUAL_ORIGINS");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_set_enum", PERIODKEY, KM_PRESS, KM_ALT, 0);
	RNA_string_set(kmi->ptr, "data_path", "tool_settings.transform_pivot_point");
	RNA_string_set(kmi->ptr, "value", "ACTIVE_ELEMENT");

#ifdef USE_WM_KEYMAP_27X
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", SPACEKEY, KM_PRESS, KM_CTRL, 0); /* new in 2.5 */
#else
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", ACCENTGRAVEKEY, KM_PRESS, KM_CTRL, 0);
#endif
	RNA_string_set(kmi->ptr, "data_path", "space_data.show_gizmo_tool");

	transform_keymap_for_space(keyconf, keymap, SPACE_VIEW3D);

	fly_modal_keymap(keyconf);
	walk_modal_keymap(keyconf);
	viewrotate_modal_keymap(keyconf);
	viewmove_modal_keymap(keyconf);
	viewzoom_modal_keymap(keyconf);
	viewdolly_modal_keymap(keyconf);
}
