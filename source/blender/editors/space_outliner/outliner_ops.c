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

/** \file blender/editors/space_outliner/outliner_ops.c
 *  \ingroup spoutliner
 */

#include "DNA_space_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "BLI_utildefines.h"

#include "outliner_intern.h"


/* ************************** registration **********************************/

void outliner_operatortypes(void)
{
	WM_operatortype_append(OUTLINER_OT_item_activate);
	WM_operatortype_append(OUTLINER_OT_select_border);
	WM_operatortype_append(OUTLINER_OT_item_openclose);
	WM_operatortype_append(OUTLINER_OT_item_rename);
	WM_operatortype_append(OUTLINER_OT_operation);
	WM_operatortype_append(OUTLINER_OT_object_operation);
	WM_operatortype_append(OUTLINER_OT_group_operation);
	WM_operatortype_append(OUTLINER_OT_id_operation);
	WM_operatortype_append(OUTLINER_OT_data_operation);
	WM_operatortype_append(OUTLINER_OT_animdata_operation);
	WM_operatortype_append(OUTLINER_OT_action_set);

	WM_operatortype_append(OUTLINER_OT_show_one_level);
	WM_operatortype_append(OUTLINER_OT_show_active);
	WM_operatortype_append(OUTLINER_OT_show_hierarchy);
	WM_operatortype_append(OUTLINER_OT_scroll_page);
	
	WM_operatortype_append(OUTLINER_OT_selected_toggle);
	WM_operatortype_append(OUTLINER_OT_expanded_toggle);
	
	WM_operatortype_append(OUTLINER_OT_renderability_toggle);
	WM_operatortype_append(OUTLINER_OT_selectability_toggle);
	WM_operatortype_append(OUTLINER_OT_visibility_toggle);
	
	WM_operatortype_append(OUTLINER_OT_keyingset_add_selected);
	WM_operatortype_append(OUTLINER_OT_keyingset_remove_selected);
	
	WM_operatortype_append(OUTLINER_OT_drivers_add_selected);
	WM_operatortype_append(OUTLINER_OT_drivers_delete_selected);

	WM_operatortype_append(OUTLINER_OT_parent_drop);
	WM_operatortype_append(OUTLINER_OT_parent_clear);
	WM_operatortype_append(OUTLINER_OT_scene_drop);
}

void outliner_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Outliner", SPACE_OUTLINER, 0);
	wmKeyMapItem *kmi;
	
	WM_keymap_add_item(keymap, "OUTLINER_OT_item_rename", LEFTMOUSE, KM_DBL_CLICK, 0, 0);

	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_activate", LEFTMOUSE, KM_CLICK, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_activate", LEFTMOUSE, KM_CLICK, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);

	WM_keymap_add_item(keymap, "OUTLINER_OT_select_border", BKEY, KM_PRESS, 0, 0);
	
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_openclose", RETKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "all", FALSE);
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_openclose", RETKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "all", TRUE);
	
	WM_keymap_add_item(keymap, "OUTLINER_OT_item_rename", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "OUTLINER_OT_operation", RIGHTMOUSE, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "OUTLINER_OT_show_hierarchy", HOMEKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "OUTLINER_OT_show_active", PERIODKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OUTLINER_OT_show_active", PADPERIOD, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "OUTLINER_OT_scroll_page", PAGEDOWNKEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_scroll_page", PAGEUPKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "up", TRUE);
	
	WM_keymap_add_item(keymap, "OUTLINER_OT_show_one_level", PADPLUSKEY, KM_PRESS, 0, 0); /* open */
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_show_one_level", PADMINUS, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "open", FALSE); /* close */
	
	WM_keymap_verify_item(keymap, "OUTLINER_OT_selected_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OUTLINER_OT_expanded_toggle", AKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_verify_item(keymap, "OUTLINER_OT_renderability_toggle", RKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OUTLINER_OT_selectability_toggle", SKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OUTLINER_OT_visibility_toggle", VKEY, KM_PRESS, 0, 0);
	
	
	/* keying sets - only for databrowse */
	WM_keymap_verify_item(keymap, "OUTLINER_OT_keyingset_add_selected", KKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OUTLINER_OT_keyingset_remove_selected", KKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "ANIM_OT_keyframe_insert", IKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "ANIM_OT_keyframe_delete", IKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "OUTLINER_OT_drivers_add_selected", DKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OUTLINER_OT_drivers_delete_selected", DKEY, KM_PRESS, KM_ALT, 0);
}

