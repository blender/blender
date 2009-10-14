/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_transform.h"

#include "view3d_intern.h"


/* ************************** registration **********************************/

void view3d_operatortypes(void)
{
	WM_operatortype_append(VIEW3D_OT_rotate);
	WM_operatortype_append(VIEW3D_OT_move);
	WM_operatortype_append(VIEW3D_OT_zoom);
	WM_operatortype_append(VIEW3D_OT_view_all);
	WM_operatortype_append(VIEW3D_OT_viewnumpad);
	WM_operatortype_append(VIEW3D_OT_view_orbit);
	WM_operatortype_append(VIEW3D_OT_view_pan);
	WM_operatortype_append(VIEW3D_OT_view_persportho);
	WM_operatortype_append(VIEW3D_OT_view_center);
	WM_operatortype_append(VIEW3D_OT_select);
	WM_operatortype_append(VIEW3D_OT_select_border);
	WM_operatortype_append(VIEW3D_OT_clip_border);
	WM_operatortype_append(VIEW3D_OT_select_circle);
	WM_operatortype_append(VIEW3D_OT_smoothview);
	WM_operatortype_append(VIEW3D_OT_render_border);
	WM_operatortype_append(VIEW3D_OT_zoom_border);
	WM_operatortype_append(VIEW3D_OT_manipulator);
	WM_operatortype_append(VIEW3D_OT_cursor3d);
	WM_operatortype_append(VIEW3D_OT_select_lasso);
	WM_operatortype_append(VIEW3D_OT_setcameratoview);
	WM_operatortype_append(VIEW3D_OT_setobjectascamera);
	WM_operatortype_append(VIEW3D_OT_localview);
	WM_operatortype_append(VIEW3D_OT_game_start);
	WM_operatortype_append(VIEW3D_OT_fly);
	WM_operatortype_append(VIEW3D_OT_layers);
	
	WM_operatortype_append(VIEW3D_OT_properties);
	WM_operatortype_append(VIEW3D_OT_toolbar);
	
	WM_operatortype_append(VIEW3D_OT_snap_selected_to_grid);
	WM_operatortype_append(VIEW3D_OT_snap_selected_to_cursor);
	WM_operatortype_append(VIEW3D_OT_snap_selected_to_center);
	WM_operatortype_append(VIEW3D_OT_snap_cursor_to_grid);
	WM_operatortype_append(VIEW3D_OT_snap_cursor_to_selected);
	WM_operatortype_append(VIEW3D_OT_snap_cursor_to_active);
	WM_operatortype_append(VIEW3D_OT_snap_menu);
		
	transform_operatortypes();
}

void view3d_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *km;
	
	keymap= WM_keymap_find(keyconf, "View3D Generic", SPACE_VIEW3D, 0);
	
	WM_keymap_add_item(keymap, "VIEW3D_OT_properties", NKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_toolbar", TKEY, KM_PRESS, 0, 0);
	
	/* only for region 3D window */
	keymap= WM_keymap_find(keyconf, "View3D", SPACE_VIEW3D, 0);
	
	WM_keymap_verify_item(keymap, "VIEW3D_OT_manipulator", LEFTMOUSE, KM_PRESS, 0, 0); /* manipulator always on left mouse, not on action mouse*/
	
	WM_keymap_verify_item(keymap, "VIEW3D_OT_cursor3d", ACTIONMOUSE, KM_PRESS, 0, 0);
	
	WM_keymap_verify_item(keymap, "VIEW3D_OT_rotate", MIDDLEMOUSE, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_move", MIDDLEMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_zoom", MIDDLEMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_view_center", PADPERIOD, KM_PRESS, 0, 0);
	
	WM_keymap_verify_item(keymap, "VIEW3D_OT_fly", FKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_verify_item(keymap, "VIEW3D_OT_smoothview", TIMER1, KM_ANY, KM_ANY, 0);
	
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", PADPLUSKEY, KM_PRESS, 0, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", PADMINUS, KM_PRESS, 0, 0)->ptr, "delta", -1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", WHEELINMOUSE, KM_PRESS, 0, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_zoom", WHEELOUTMOUSE, KM_PRESS, 0, 0)->ptr, "delta", -1);

	RNA_boolean_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_all", HOMEKEY, KM_PRESS, 0, 0)->ptr, "center", 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_all", CKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "center", 1);

	/* numpad view hotkeys*/
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD0, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_CAMERA);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD1, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_FRONT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", PAD2, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPDOWN);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD3, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_RIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", PAD4, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPLEFT);
	WM_keymap_add_item(keymap, "VIEW3D_OT_view_persportho", PAD5, KM_PRESS, 0, 0);
	
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", PAD6, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPRIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD7, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_TOP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_orbit", PAD8, KM_PRESS, 0, 0)->ptr, "type", V3D_VIEW_STEPUP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD1, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_BACK);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD3, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_LEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD7, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_BOTTOM);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", PAD2, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANDOWN);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", PAD4, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANLEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", PAD6, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANRIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_view_pan", PAD8, KM_PRESS, KM_CTRL, 0)->ptr, "type", V3D_VIEW_PANUP);

	WM_keymap_add_item(keymap, "VIEW3D_OT_localview", PADSLASHKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "VIEW3D_OT_game_start", PKEY, KM_PRESS, 0, 0);
	
	/* layers, shift + alt are properties set in invoke() */
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", ACCENTGRAVEKEY, KM_PRESS, 0, 0)->ptr, "nr", 0);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", ONEKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", TWOKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 2);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", THREEKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 3);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", FOURKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 4);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", FIVEKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 5);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", SIXKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 6);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", SEVENKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 7);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", EIGHTKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 8);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", NINEKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 9);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_layers", ZEROKEY, KM_PRESS, KM_ANY, 0)->ptr, "nr", 10);
	
	/* drawtype */

	km = WM_keymap_add_item(keymap, "WM_OT_context_toggle_values", ZKEY, KM_PRESS, 0, 0);
	RNA_string_set(km->ptr, "path", "space_data.viewport_shading");
	RNA_string_set(km->ptr, "value_1", "'SOLID'");
	RNA_string_set(km->ptr, "value_2", "'WIREFRAME'");

	km = WM_keymap_add_item(keymap, "WM_OT_context_toggle_values", ZKEY, KM_PRESS, KM_ALT, 0);
	RNA_string_set(km->ptr, "path", "space_data.viewport_shading");
	RNA_string_set(km->ptr, "value_1", "'TEXTURED'");
	RNA_string_set(km->ptr, "value_2", "'SOLID'");

	km = WM_keymap_add_item(keymap, "WM_OT_context_toggle_values", ZKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_string_set(km->ptr, "path", "space_data.viewport_shading");
	RNA_string_set(km->ptr, "value_1", "'SHADED'");
	RNA_string_set(km->ptr, "value_2", "'WIREFRAME'");

	/* selection*/
	WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", TRUE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL, 0)->ptr, "center", TRUE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_ALT, 0)->ptr, "enumerate", TRUE);

	/* selection key-combinations */
	km = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	RNA_boolean_set(km->ptr, "center", TRUE);
	RNA_boolean_set(km->ptr, "extend", TRUE);
	km = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_CTRL|KM_ALT, 0);
	RNA_boolean_set(km->ptr, "center", TRUE);
	RNA_boolean_set(km->ptr, "enumerate", TRUE);
	km = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_ALT, 0);
	RNA_boolean_set(km->ptr, "extend", TRUE);
	RNA_boolean_set(km->ptr, "enumerate", TRUE);
	km = WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_CTRL|KM_ALT, 0);
	RNA_boolean_set(km->ptr, "center", TRUE);
	RNA_boolean_set(km->ptr, "extend", TRUE);
	RNA_boolean_set(km->ptr, "enumerate", TRUE);

	WM_keymap_add_item(keymap, "VIEW3D_OT_select_border", BKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "VIEW3D_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_SHIFT|KM_CTRL, 0)->ptr, "deselect", 1);
	WM_keymap_add_item(keymap, "VIEW3D_OT_select_circle", CKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "VIEW3D_OT_clip_border", BKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_zoom_border", BKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_render_border", BKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "VIEW3D_OT_camera_to_view", PAD0, KM_PRESS, KM_ALT|KM_CTRL, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_object_as_camera", PAD0, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "VIEW3D_OT_snap_menu", SKEY, KM_PRESS, KM_SHIFT, 0);

	/* context ops */
	km = WM_keymap_add_item(keymap, "WM_OT_context_set", COMMAKEY, KM_PRESS, 0, 0);
	RNA_string_set(km->ptr, "path", "space_data.pivot_point");
	RNA_string_set(km->ptr, "value", "'BOUNDING_BOX_CENTER'");

	km = WM_keymap_add_item(keymap, "WM_OT_context_set", COMMAKEY, KM_PRESS, KM_CTRL, 0); /* 2.4x allowed Comma+Shift too, rather not use both */
	RNA_string_set(km->ptr, "path", "space_data.pivot_point");
	RNA_string_set(km->ptr, "value", "'MEDIAN_POINT'");

	km = WM_keymap_add_item(keymap, "WM_OT_context_toggle", COMMAKEY, KM_PRESS, KM_ALT, 0); /* new in 2.5 */
	RNA_string_set(km->ptr, "path", "space_data.pivot_point_align");

	km = WM_keymap_add_item(keymap, "WM_OT_context_set", PERIODKEY, KM_PRESS, 0, 0);
	RNA_string_set(km->ptr, "path", "space_data.pivot_point");
	RNA_string_set(km->ptr, "value", "'CURSOR'");

	km = WM_keymap_add_item(keymap, "WM_OT_context_set", PERIODKEY, KM_PRESS, KM_CTRL, 0);
	RNA_string_set(km->ptr, "path", "space_data.pivot_point");
	RNA_string_set(km->ptr, "value", "'INDIVIDUAL_CENTERS'");

	km = WM_keymap_add_item(keymap, "WM_OT_context_set", PERIODKEY, KM_PRESS, KM_ALT, 0);
	RNA_string_set(km->ptr, "path", "space_data.pivot_point");
	RNA_string_set(km->ptr, "value", "'ACTIVE_ELEMENT'");


	transform_keymap_for_space(keyconf, keymap, SPACE_VIEW3D);

	fly_modal_keymap(keyconf);
	viewrotate_modal_keymap(keyconf);
	viewmove_modal_keymap(keyconf);
	viewzoom_modal_keymap(keyconf);
}

