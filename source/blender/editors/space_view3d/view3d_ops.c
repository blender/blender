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

#include "BIF_transform.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "view3d_intern.h"


/* ************************** registration **********************************/

void view3d_operatortypes(void)
{
	WM_operatortype_append(VIEW3D_OT_viewrotate);
	WM_operatortype_append(VIEW3D_OT_viewmove);
	WM_operatortype_append(VIEW3D_OT_viewzoom);
	WM_operatortype_append(VIEW3D_OT_viewhome);
	WM_operatortype_append(VIEW3D_OT_viewnumpad);
	WM_operatortype_append(VIEW3D_OT_viewcenter);
	WM_operatortype_append(VIEW3D_OT_select);
	WM_operatortype_append(VIEW3D_OT_borderselect);
	WM_operatortype_append(VIEW3D_OT_clipping);
	WM_operatortype_append(VIEW3D_OT_circle_select);
	WM_operatortype_append(VIEW3D_OT_smoothview);
	WM_operatortype_append(VIEW3D_OT_render_border);
	WM_operatortype_append(VIEW3D_OT_cursor3d);
	WM_operatortype_append(VIEW3D_OT_lasso_select);
	
	transform_operatortypes();
}

void view3d_keymap(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "View3D", SPACE_VIEW3D, 0);
	
	WM_keymap_verify_item(keymap, "VIEW3D_OT_cursor3d", ACTIONMOUSE, KM_PRESS, 0, 0);
	
	WM_keymap_verify_item(keymap, "VIEW3D_OT_viewrotate", MIDDLEMOUSE, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_viewmove", MIDDLEMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_viewzoom", MIDDLEMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "VIEW3D_OT_viewcenter", PADPERIOD, KM_PRESS, 0, 0);
	
	WM_keymap_verify_item(keymap, "VIEW3D_OT_smoothview", TIMER1, KM_ANY, KM_ANY, 0);
	
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewzoom", PADPLUSKEY, KM_PRESS, 0, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewzoom", PADMINUS, KM_PRESS, 0, 0)->ptr, "delta", -1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewzoom", WHEELINMOUSE, KM_PRESS, 0, 0)->ptr, "delta", 1);
	RNA_int_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewzoom", WHEELOUTMOUSE, KM_PRESS, 0, 0)->ptr, "delta", -1);

	RNA_boolean_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewhome", HOMEKEY, KM_PRESS, 0, 0)->ptr, "center", 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewhome", CKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "center", 1);

	/* numpad view hotkeys*/
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD0, KM_PRESS, 0, 0)->ptr, "viewnum", V3D_VIEW_CAMERA);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD1, KM_PRESS, 0, 0)->ptr, "viewnum", V3D_VIEW_FRONT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD2, KM_PRESS, 0, 0)->ptr, "viewnum", V3D_VIEW_STEPDOWN);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD3, KM_PRESS, 0, 0)->ptr, "viewnum", V3D_VIEW_RIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD4, KM_PRESS, 0, 0)->ptr, "viewnum", V3D_VIEW_STEPLEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD5, KM_PRESS, 0, 0)->ptr, "viewnum", V3D_VIEW_PERSPORTHO);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD6, KM_PRESS, 0, 0)->ptr, "viewnum", V3D_VIEW_STEPRIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD7, KM_PRESS, 0, 0)->ptr, "viewnum", V3D_VIEW_TOP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD8, KM_PRESS, 0, 0)->ptr, "viewnum", V3D_VIEW_STEPUP);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD1, KM_PRESS, KM_CTRL, 0)->ptr, "viewnum", V3D_VIEW_BACK);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD3, KM_PRESS, KM_CTRL, 0)->ptr, "viewnum", V3D_VIEW_LEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD7, KM_PRESS, KM_CTRL, 0)->ptr, "viewnum", V3D_VIEW_BOTTOM);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD2, KM_PRESS, KM_CTRL, 0)->ptr, "viewnum", V3D_VIEW_PANDOWN);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD4, KM_PRESS, KM_CTRL, 0)->ptr, "viewnum", V3D_VIEW_PANLEFT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD6, KM_PRESS, KM_CTRL, 0)->ptr, "viewnum", V3D_VIEW_PANRIGHT);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_viewnumpad", PAD8, KM_PRESS, KM_CTRL, 0)->ptr, "viewnum", V3D_VIEW_PANUP);

	/* selection*/
	WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "type", 1);
	WM_keymap_add_item(keymap, "VIEW3D_OT_borderselect", BKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_lasso_select", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(WM_keymap_add_item(keymap, "VIEW3D_OT_lasso_select", LEFTMOUSE, KM_PRESS, KM_SHIFT|KM_CTRL, 0)->ptr, "type", 1);
	WM_keymap_add_item(keymap, "VIEW3D_OT_circle_select", CKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "VIEW3D_OT_clipping", BKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "VIEW3D_OT_render_border", BKEY, KM_PRESS, KM_SHIFT, 0);

	/* TODO - this is just while we have no way to load a text datablock */
	RNA_string_set(WM_keymap_add_item(keymap, "SCRIPT_OT_run_pyfile", PKEY, KM_PRESS, 0, 0)->ptr, "filename", "test.py");

	transform_keymap_for_space(wm, keymap, SPACE_VIEW3D);

	/* generates event, in end to make select work */
	WM_keymap_verify_item(keymap, "WM_OT_tweak_gesture", SELECTMOUSE, KM_PRESS, 0, 0);

}

