/**
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_node.h"

#include "ED_screen.h"
#include "ED_transform.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "node_intern.h"

void node_operatortypes(void)
{
	WM_operatortype_append(NODE_OT_properties);
	
	WM_operatortype_append(NODE_OT_select);
	WM_operatortype_append(NODE_OT_select_all);
	WM_operatortype_append(NODE_OT_select_linked_to);
	WM_operatortype_append(NODE_OT_select_linked_from);
	WM_operatortype_append(NODE_OT_select_border);
	
	WM_operatortype_append(NODE_OT_view_all);
	WM_operatortype_append(NODE_OT_visibility_toggle);
	WM_operatortype_append(NODE_OT_mute);
	WM_operatortype_append(NODE_OT_hide);
	WM_operatortype_append(NODE_OT_show_cyclic_dependencies);
	
	WM_operatortype_append(NODE_OT_duplicate);
	WM_operatortype_append(NODE_OT_delete);
	WM_operatortype_append(NODE_OT_resize);
	
	WM_operatortype_append(NODE_OT_link);
	WM_operatortype_append(NODE_OT_link_make);
	WM_operatortype_append(NODE_OT_links_cut);
	
	WM_operatortype_append(NODE_OT_group_make);
	WM_operatortype_append(NODE_OT_group_ungroup);
	WM_operatortype_append(NODE_OT_group_edit);
	
	WM_operatortype_append(NODE_OT_link_viewer);
	
	WM_operatortype_append(NODE_OT_read_renderlayers);
	WM_operatortype_append(NODE_OT_read_fullsamplelayers);
	
	WM_operatortype_append(NODE_OT_backimage_move);
}

void ED_operatormacros_node(void)
{
	wmOperatorType *ot;
	
	ot= WM_operatortype_append_macro("NODE_OT_duplicate_move", "Duplicate", OPTYPE_UNDO|OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "NODE_OT_duplicate");
	WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");

	ot= WM_operatortype_append_macro("NODE_OT_select_link_viewer", "Link Viewer", OPTYPE_UNDO);
	WM_operatortype_macro_define(ot, "NODE_OT_select");
	WM_operatortype_macro_define(ot, "NODE_OT_link_viewer");
	
}

void node_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	/* Entire Editor only ----------------- */
	keymap= WM_keymap_find(keyconf, "Node Generic", SPACE_NODE, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_properties", NKEY, KM_PRESS, 0, 0);
	
	/* Main Area only ----------------- */
	keymap= WM_keymap_find(keyconf, "Node Editor", SPACE_NODE, 0);
	
	/* mouse select in nodes used to be both keys, but perhaps this should be reduced? 
	 * NOTE: mouse-clicks on left-mouse will fall through to allow transform-tweak, but also link/resize
	 */
	WM_keymap_add_item(keymap, "NODE_OT_select", ACTIONMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	kmi= WM_keymap_add_item(keymap, "NODE_OT_select", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "extend", 1);
	kmi= WM_keymap_add_item(keymap, "NODE_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "extend", 1);
	RNA_boolean_set(WM_keymap_add_item(keymap, "NODE_OT_select_border", EVT_TWEAK_S, KM_ANY, 0, 0)->ptr, "tweak", 1);
	
	/* each of these falls through if not handled... */
	WM_keymap_add_item(keymap, "NODE_OT_link", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_resize", LEFTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_visibility_toggle", LEFTMOUSE, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_links_cut", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_link_viewer", LEFTMOUSE, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_backimage_move", MIDDLEMOUSE, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_link_make", FKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "NODE_OT_link_make", FKEY, KM_PRESS, KM_CTRL, 0)->ptr, "replace", 1);
	
	WM_keymap_add_menu(keymap, "NODE_MT_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_hide", HKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_mute", MKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_show_cyclic_dependencies", CKEY, KM_PRESS, 0, 0);
	
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
	
	WM_keymap_add_item(keymap, "NODE_OT_read_renderlayers", RKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_read_fullsamplelayers", RKEY, KM_PRESS, KM_SHIFT, 0);	
	
	transform_keymap_for_space(keyconf, keymap, SPACE_NODE);
}
