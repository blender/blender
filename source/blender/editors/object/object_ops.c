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
#include "ED_object.h"

#include "object_intern.h"


/* ************************** registration **********************************/


void ED_operatortypes_object(void)
{
	WM_operatortype_append(OBJECT_OT_editmode_toggle);
	WM_operatortype_append(OBJECT_OT_make_parent);
	WM_operatortype_append(OBJECT_OT_clear_parent);
	WM_operatortype_append(OBJECT_OT_make_track);
	WM_operatortype_append(OBJECT_OT_clear_track);
	WM_operatortype_append(OBJECT_OT_select_invert);
	WM_operatortype_append(OBJECT_OT_select_random);
	WM_operatortype_append(OBJECT_OT_de_select_all);
	WM_operatortype_append(OBJECT_OT_select_by_type);
	WM_operatortype_append(OBJECT_OT_select_by_layer);
	WM_operatortype_append(OBJECT_OT_select_linked);
	WM_operatortype_append(OBJECT_OT_clear_location);
	WM_operatortype_append(OBJECT_OT_clear_rotation);
	WM_operatortype_append(OBJECT_OT_clear_scale);
	WM_operatortype_append(OBJECT_OT_clear_origin);
	WM_operatortype_append(OBJECT_OT_clear_restrictview);
	WM_operatortype_append(OBJECT_OT_set_restrictview);
	WM_operatortype_append(OBJECT_OT_set_slowparent);
	WM_operatortype_append(OBJECT_OT_clear_slowparent);
	WM_operatortype_append(OBJECT_OT_set_center);
	WM_operatortype_append(OBJECT_OT_make_dupli_real);
	WM_operatortype_append(OBJECT_OT_add_duplicate);
	WM_operatortype_append(GROUP_OT_group_create);
	WM_operatortype_append(GROUP_OT_group_remove);
	WM_operatortype_append(GROUP_OT_objects_add_active);
	WM_operatortype_append(GROUP_OT_objects_remove_active);

	WM_operatortype_append(OBJECT_OT_delete);
	WM_operatortype_append(OBJECT_OT_mesh_add);
	WM_operatortype_append(OBJECT_OT_curve_add);
	WM_operatortype_append(OBJECT_OT_text_add);
	WM_operatortype_append(OBJECT_OT_surface_add);
	WM_operatortype_append(OBJECT_OT_armature_add);
	WM_operatortype_append(OBJECT_OT_object_add);
	WM_operatortype_append(OBJECT_OT_primitive_add);
}

void ED_keymap_object(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "Object Non-modal", 0, 0);
	
	/* Note: this keymap works disregarding mode */
	WM_keymap_add_item(keymap, "OBJECT_OT_editmode_toggle", TABKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_set_center", CKEY, KM_PRESS, KM_ALT|KM_CTRL, 0);

	/* Note: this keymap gets disabled in non-objectmode,  */
	keymap= WM_keymap_listbase(wm, "Object Mode", 0, 0);
	
	WM_keymap_add_item(keymap, "OBJECT_OT_de_select_all", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_select_invert", IKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_select_random", PADASTERKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_select_by_type", PADASTERKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_select_by_layer", PADASTERKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_select_linked", LKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_verify_item(keymap, "OBJECT_OT_make_parent", PKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_clear_parent", PKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_make_track", TKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_clear_track", TKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "OBJECT_OT_clear_location", GKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_clear_rotation", RKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_clear_scale", SKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_clear_origin", OKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "OBJECT_OT_clear_restrictview", HKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_set_restrictview", HKEY, KM_PRESS, 0, 0);
	
	WM_keymap_verify_item(keymap, "OBJECT_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_primitive_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_verify_item(keymap, "OBJECT_OT_add_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	
	// XXX this should probably be in screen instead... here for testing purposes in the meantime... - Aligorith
	WM_keymap_verify_item(keymap, "ANIM_OT_insert_keyframe_old", IKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "ANIM_OT_delete_keyframe_old", IKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "GROUP_OT_group_create", GKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "GROUP_OT_group_remove", GKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_verify_item(keymap, "GROUP_OT_objects_add_active", GKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "GROUP_OT_objects_remove_active", GKEY, KM_PRESS, KM_SHIFT|KM_ALT, 0);

}

