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
 * Contributor(s): Blender Foundation, Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_ops.c
 *  \ingroup spnode
 */


#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"

#include "BLI_utildefines.h"

#include "ED_node.h"
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
	WM_operatortype_append(NODE_OT_select_same_type);
	WM_operatortype_append(NODE_OT_select_same_type_next);
	WM_operatortype_append(NODE_OT_select_same_type_prev);

	WM_operatortype_append(NODE_OT_view_all);

	WM_operatortype_append(NODE_OT_mute_toggle);
	WM_operatortype_append(NODE_OT_hide_toggle);
	WM_operatortype_append(NODE_OT_preview_toggle);
	WM_operatortype_append(NODE_OT_options_toggle);
	WM_operatortype_append(NODE_OT_hide_socket_toggle);
	WM_operatortype_append(NODE_OT_show_cyclic_dependencies);
	
	WM_operatortype_append(NODE_OT_duplicate);
	WM_operatortype_append(NODE_OT_delete);
	WM_operatortype_append(NODE_OT_delete_reconnect);
	WM_operatortype_append(NODE_OT_resize);
	
	WM_operatortype_append(NODE_OT_link);
	WM_operatortype_append(NODE_OT_link_make);
	WM_operatortype_append(NODE_OT_links_cut);
	WM_operatortype_append(NODE_OT_links_detach);

	WM_operatortype_append(NODE_OT_group_make);
	WM_operatortype_append(NODE_OT_group_ungroup);
	WM_operatortype_append(NODE_OT_group_edit);
	WM_operatortype_append(NODE_OT_group_socket_add);
	WM_operatortype_append(NODE_OT_group_socket_remove);
	WM_operatortype_append(NODE_OT_group_socket_move_up);
	WM_operatortype_append(NODE_OT_group_socket_move_down);
	
	WM_operatortype_append(NODE_OT_link_viewer);
	
	WM_operatortype_append(NODE_OT_read_renderlayers);
	WM_operatortype_append(NODE_OT_read_fullsamplelayers);
	WM_operatortype_append(NODE_OT_render_changed);
	
	WM_operatortype_append(NODE_OT_backimage_move);
	WM_operatortype_append(NODE_OT_backimage_zoom);
	WM_operatortype_append(NODE_OT_backimage_sample);
	
	WM_operatortype_append(NODE_OT_add_file);
	
	WM_operatortype_append(NODE_OT_new_node_tree);
	
	WM_operatortype_append(NODE_OT_output_file_add_socket);
	WM_operatortype_append(NODE_OT_output_file_remove_active_socket);
	WM_operatortype_append(NODE_OT_output_file_move_active_socket);
}

void ED_operatormacros_node(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *mot;
	
	ot = WM_operatortype_append_macro("NODE_OT_duplicate_move", "Duplicate", "Duplicate selected nodes and move them",
	                                  OPTYPE_UNDO|OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "NODE_OT_duplicate");
	WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");

	/* modified operator call for duplicating with input links */
	ot = WM_operatortype_append_macro("NODE_OT_duplicate_move_keep_inputs", "Duplicate",
	                                  "Duplicate selected nodes keeping input links and move them",
	                                  OPTYPE_UNDO|OPTYPE_REGISTER);
	mot = WM_operatortype_macro_define(ot, "NODE_OT_duplicate");
	RNA_boolean_set(mot->ptr, "keep_inputs", TRUE);
	WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");

	ot = WM_operatortype_append_macro("NODE_OT_select_link_viewer", "Link Viewer",
	                                  "Select node and link it to a viewer node", OPTYPE_UNDO);
	WM_operatortype_macro_define(ot, "NODE_OT_select");
	WM_operatortype_macro_define(ot, "NODE_OT_link_viewer");

	ot = WM_operatortype_append_macro("NODE_OT_move_detach_links", "Detach", "Move a node to detach links",
	                                  OPTYPE_UNDO|OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "NODE_OT_links_detach");
	WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");

	ot = WM_operatortype_append_macro("NODE_OT_move_detach_links_release", "Detach", "Move a node to detach links",
	                                  OPTYPE_UNDO|OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "NODE_OT_links_detach");
	mot = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_boolean_set(mot->ptr, "release_confirm", TRUE);
}

void node_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	/* Entire Editor only ----------------- */
	keymap = WM_keymap_find(keyconf, "Node Generic", SPACE_NODE, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_properties", NKEY, KM_PRESS, 0, 0);
	
	/* Main Area only ----------------- */
	keymap = WM_keymap_find(keyconf, "Node Editor", SPACE_NODE, 0);
	
	/* mouse select in nodes used to be both keys, but perhaps this should be reduced? 
	 * NOTE: mouse-clicks on left-mouse will fall through to allow transform-tweak, but also link/resize
	 * NOTE 2: socket select is part of the node select operator, to handle overlapping cases
	 */
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select", ACTIONMOUSE, KM_PRESS, 0, 0);
		RNA_boolean_set(kmi->ptr, "extend", FALSE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
		RNA_boolean_set(kmi->ptr, "extend", FALSE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "extend", TRUE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
		RNA_boolean_set(kmi->ptr, "extend", TRUE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_border", EVT_TWEAK_S, KM_ANY, 0, 0);
		RNA_boolean_set(kmi->ptr, "tweak", TRUE);
	
	/* each of these falls through if not handled... */
	WM_keymap_add_item(keymap, "NODE_OT_link", LEFTMOUSE, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_link", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
		RNA_boolean_set(kmi->ptr, "detach", TRUE);
	WM_keymap_add_item(keymap, "NODE_OT_resize", LEFTMOUSE, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_links_cut", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_link_viewer", LEFTMOUSE, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_backimage_move", MIDDLEMOUSE, KM_PRESS, KM_ALT, 0);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_backimage_zoom", VKEY, KM_PRESS, 0, 0);
		RNA_float_set(kmi->ptr, "factor", 0.83333f);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_backimage_zoom", VKEY, KM_PRESS, KM_ALT, 0);
		RNA_float_set(kmi->ptr, "factor", 1.2f);
	WM_keymap_add_item(keymap, "NODE_OT_backimage_sample", ACTIONMOUSE, KM_PRESS, KM_ALT, 0);

	kmi = WM_keymap_add_item(keymap, "NODE_OT_link_make", FKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "replace", FALSE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_link_make", FKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "replace", TRUE);

	WM_keymap_add_menu(keymap, "NODE_MT_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	/* modified operator call for duplicating with input links */
	WM_keymap_add_item(keymap, "NODE_OT_duplicate_move_keep_inputs", DKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_hide_toggle", HKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_mute_toggle", MKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_preview_toggle", HKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_hide_socket_toggle", HKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_show_cyclic_dependencies", CKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "tweak", FALSE);

	WM_keymap_add_item(keymap, "NODE_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_delete", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_delete_reconnect", XKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "NODE_OT_select_all", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_linked_to", LKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_linked_from", LKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_same_type", GKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_same_type_next", RIGHTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_same_type_prev", LEFTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "NODE_OT_group_make", GKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_group_ungroup", GKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_group_edit", TABKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_read_renderlayers", RKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_read_fullsamplelayers", RKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_render_changed", ZKEY, KM_PRESS, 0, 0);
	
	transform_keymap_for_space(keyconf, keymap, SPACE_NODE);
}
