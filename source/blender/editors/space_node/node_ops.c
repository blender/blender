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
 * Contributor(s): Blender Foundation, Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_node_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_node.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "node_intern.h"

void node_operatortypes(void)
{
	WM_operatortype_append(NODE_OT_properties);
	
	WM_operatortype_append(NODE_OT_select);
	WM_operatortype_append(NODE_OT_select_extend);
	WM_operatortype_append(NODE_OT_select_all);
	WM_operatortype_append(NODE_OT_select_linked_to);
	WM_operatortype_append(NODE_OT_select_linked_from);
	WM_operatortype_append(NODE_OT_visibility_toggle);
	WM_operatortype_append(NODE_OT_view_all);
	WM_operatortype_append(NODE_OT_select_border);
	WM_operatortype_append(NODE_OT_delete);
	WM_operatortype_append(NODE_OT_link);
	WM_operatortype_append(NODE_OT_resize);
	WM_operatortype_append(NODE_OT_links_cut);
	WM_operatortype_append(NODE_OT_duplicate);
	WM_operatortype_append(NODE_OT_group_make);
	WM_operatortype_append(NODE_OT_group_ungroup);
	WM_operatortype_append(NODE_OT_group_edit);
}

void node_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	/* Entire Editor only ----------------- */
	keymap= WM_keymap_find(keyconf, "Node Generic", SPACE_NODE, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_properties", NKEY, KM_PRESS, 0, 0);
	
	/* Main Area only ----------------- */
	keymap= WM_keymap_find(keyconf, "Node", SPACE_NODE, 0);
	
	/* mouse select in nodes used to be both keys, it's UI elements... */
	RNA_enum_set(WM_keymap_add_item(keymap, "NODE_OT_select", ACTIONMOUSE, KM_PRESS, 0, 0)->ptr, "select_type", NODE_SELECT_MOUSE);
	RNA_enum_set(WM_keymap_add_item(keymap, "NODE_OT_select", SELECTMOUSE, KM_PRESS, 0, 0)->ptr, "select_type", NODE_SELECT_MOUSE);
	RNA_enum_set(WM_keymap_add_item(keymap, "NODE_OT_select_extend", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "select_type", NODE_SELECT_MOUSE);
	RNA_enum_set(WM_keymap_add_item(keymap, "NODE_OT_select_extend", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "select_type", NODE_SELECT_MOUSE);
	
	WM_keymap_add_item(keymap, "NODE_OT_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_link", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_resize", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_visibility_toggle", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_links_cut", LEFTMOUSE, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_border", BKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_delete", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "NODE_OT_select_all", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_linked_to", LKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_linked_from", LKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "NODE_OT_group_make", GKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_group_ungroup", GKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_group_edit", TABKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_menu(keymap, "NODE_MT_add", AKEY, KM_PRESS, KM_SHIFT, 0);

	transform_keymap_for_space(keyconf, keymap, SPACE_NODE);
}
