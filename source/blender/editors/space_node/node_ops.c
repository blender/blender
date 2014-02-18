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

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "ED_node.h"  /* own include */
#include "ED_screen.h"
#include "ED_transform.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "node_intern.h"  /* own include */

void node_operatortypes(void)
{
	WM_operatortype_append(NODE_OT_properties);
	WM_operatortype_append(NODE_OT_toolbar);
	
	WM_operatortype_append(NODE_OT_select);
	WM_operatortype_append(NODE_OT_select_all);
	WM_operatortype_append(NODE_OT_select_linked_to);
	WM_operatortype_append(NODE_OT_select_linked_from);
	WM_operatortype_append(NODE_OT_select_border);
	WM_operatortype_append(NODE_OT_select_circle);
	WM_operatortype_append(NODE_OT_select_lasso);
	WM_operatortype_append(NODE_OT_select_same_type);
	WM_operatortype_append(NODE_OT_select_same_type_step);
	
	WM_operatortype_append(NODE_OT_find_node);
	
	WM_operatortype_append(NODE_OT_view_all);
	WM_operatortype_append(NODE_OT_view_selected);

	WM_operatortype_append(NODE_OT_mute_toggle);
	WM_operatortype_append(NODE_OT_hide_toggle);
	WM_operatortype_append(NODE_OT_preview_toggle);
	WM_operatortype_append(NODE_OT_options_toggle);
	WM_operatortype_append(NODE_OT_hide_socket_toggle);
	WM_operatortype_append(NODE_OT_node_copy_color);
	
	WM_operatortype_append(NODE_OT_duplicate);
	WM_operatortype_append(NODE_OT_delete);
	WM_operatortype_append(NODE_OT_delete_reconnect);
	WM_operatortype_append(NODE_OT_resize);
	
	WM_operatortype_append(NODE_OT_link);
	WM_operatortype_append(NODE_OT_link_make);
	WM_operatortype_append(NODE_OT_links_cut);
	WM_operatortype_append(NODE_OT_links_detach);
	WM_operatortype_append(NODE_OT_add_reroute);

	WM_operatortype_append(NODE_OT_group_make);
	WM_operatortype_append(NODE_OT_group_insert);
	WM_operatortype_append(NODE_OT_group_ungroup);
	WM_operatortype_append(NODE_OT_group_separate);
	WM_operatortype_append(NODE_OT_group_edit);
	
	WM_operatortype_append(NODE_OT_link_viewer);
	
	WM_operatortype_append(NODE_OT_read_renderlayers);
	WM_operatortype_append(NODE_OT_read_fullsamplelayers);
	WM_operatortype_append(NODE_OT_render_changed);
	
	WM_operatortype_append(NODE_OT_backimage_move);
	WM_operatortype_append(NODE_OT_backimage_zoom);
	WM_operatortype_append(NODE_OT_backimage_fit);
	WM_operatortype_append(NODE_OT_backimage_sample);
	
	WM_operatortype_append(NODE_OT_add_file);
	WM_operatortype_append(NODE_OT_add_mask);
	
	WM_operatortype_append(NODE_OT_new_node_tree);
	
	WM_operatortype_append(NODE_OT_output_file_add_socket);
	WM_operatortype_append(NODE_OT_output_file_remove_active_socket);
	WM_operatortype_append(NODE_OT_output_file_move_active_socket);
	
	WM_operatortype_append(NODE_OT_parent_set);
	WM_operatortype_append(NODE_OT_parent_clear);
	WM_operatortype_append(NODE_OT_join);
	WM_operatortype_append(NODE_OT_attach);
	WM_operatortype_append(NODE_OT_detach);
	
	WM_operatortype_append(NODE_OT_clipboard_copy);
	WM_operatortype_append(NODE_OT_clipboard_paste);
	
	WM_operatortype_append(NODE_OT_shader_script_update);

	WM_operatortype_append(NODE_OT_viewer_border);

	WM_operatortype_append(NODE_OT_tree_socket_add);
	WM_operatortype_append(NODE_OT_tree_socket_remove);
	WM_operatortype_append(NODE_OT_tree_socket_move);
}

void ED_operatormacros_node(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *mot;
	
	ot = WM_operatortype_append_macro("NODE_OT_select_link_viewer", "Link Viewer",
	                                  "Select node and link it to a viewer node",
	                                  OPTYPE_UNDO);
	WM_operatortype_macro_define(ot, "NODE_OT_select");
	WM_operatortype_macro_define(ot, "NODE_OT_link_viewer");

	ot = WM_operatortype_append_macro("NODE_OT_translate_attach", "Move and Attach",
	                                  "Move nodes and attach to frame",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	mot = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_boolean_set(mot->ptr, "release_confirm", TRUE);
	WM_operatortype_macro_define(ot, "NODE_OT_attach");

	ot = WM_operatortype_append_macro("NODE_OT_detach_translate_attach", "Detach and Move",
	                                  "Detach nodes, move and attach to frame",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "NODE_OT_detach");
	mot = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_boolean_set(mot->ptr, "release_confirm", TRUE);
	WM_operatortype_macro_define(ot, "NODE_OT_attach");

	ot = WM_operatortype_append_macro("NODE_OT_duplicate_move", "Duplicate",
	                                  "Duplicate selected nodes and move them",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "NODE_OT_duplicate");
	WM_operatortype_macro_define(ot, "NODE_OT_translate_attach");

	/* modified operator call for duplicating with input links */
	ot = WM_operatortype_append_macro("NODE_OT_duplicate_move_keep_inputs", "Duplicate",
	                                  "Duplicate selected nodes keeping input links and move them",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	mot = WM_operatortype_macro_define(ot, "NODE_OT_duplicate");
	RNA_boolean_set(mot->ptr, "keep_inputs", TRUE);
	WM_operatortype_macro_define(ot, "NODE_OT_translate_attach");

	ot = WM_operatortype_append_macro("NODE_OT_move_detach_links", "Detach", "Move a node to detach links",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "NODE_OT_links_detach");
	WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");

	ot = WM_operatortype_append_macro("NODE_OT_move_detach_links_release", "Detach", "Move a node to detach links",
	                                  OPTYPE_UNDO | OPTYPE_REGISTER);
	WM_operatortype_macro_define(ot, "NODE_OT_links_detach");
	WM_operatortype_macro_define(ot, "NODE_OT_translate_attach");
}

/* helper function for repetitive select operator keymap */
static void node_select_keymap(wmKeyMap *keymap, int extend)
{
	/* modifier combinations */
	const int mod_single[] = { 0, KM_CTRL, KM_ALT, KM_CTRL | KM_ALT,
	                           -1 /* terminator */
	};
	const int mod_extend[] = { KM_SHIFT, KM_SHIFT | KM_CTRL,
	                           KM_SHIFT | KM_ALT, KM_SHIFT | KM_CTRL | KM_ALT,
	                           -1 /* terminator */
	};
	const int *mod = (extend ? mod_extend : mod_single);
	wmKeyMapItem *kmi;
	int i;
	
	for (i = 0; mod[i] >= 0; ++i) {
		kmi = WM_keymap_add_item(keymap, "NODE_OT_select", ACTIONMOUSE, KM_PRESS, mod[i], 0);
		RNA_boolean_set(kmi->ptr, "extend", extend);
		kmi = WM_keymap_add_item(keymap, "NODE_OT_select", SELECTMOUSE, KM_PRESS, mod[i], 0);
		RNA_boolean_set(kmi->ptr, "extend", extend);
	}
}

void node_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	
	/* Entire Editor only ----------------- */
	keymap = WM_keymap_find(keyconf, "Node Generic", SPACE_NODE, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_properties", NKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_toolbar", TKEY, KM_PRESS, 0, 0);
	
	/* Main Area only ----------------- */
	keymap = WM_keymap_find(keyconf, "Node Editor", SPACE_NODE, 0);
	
	/* mouse select in nodes used to be both keys, but perhaps this should be reduced? 
	 * NOTE: mouse-clicks on left-mouse will fall through to allow transform-tweak, but also link/resize
	 * NOTE 2: socket select is part of the node select operator, to handle overlapping cases
	 * NOTE 3: select op is registered for various combinations of modifier key, so the specialized
	 *         grab operators (unlink, attach, etc.) can work easily on single nodes.
	 */
	node_select_keymap(keymap, FALSE);
	node_select_keymap(keymap, TRUE);
	
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_border", EVT_TWEAK_S, KM_ANY, 0, 0);
	RNA_boolean_set(kmi->ptr, "tweak", TRUE);
	
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", FALSE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_SHIFT | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", TRUE);

	WM_keymap_add_item(keymap, "NODE_OT_select_circle", CKEY, KM_PRESS, 0, 0);

	/* each of these falls through if not handled... */
	kmi = WM_keymap_add_item(keymap, "NODE_OT_link", LEFTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "detach", FALSE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_link", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "detach", TRUE);
	
	WM_keymap_add_item(keymap, "NODE_OT_resize", LEFTMOUSE, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_add_reroute", LEFTMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_links_cut", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_link_viewer", LEFTMOUSE, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_backimage_move", MIDDLEMOUSE, KM_PRESS, KM_ALT, 0);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_backimage_zoom", VKEY, KM_PRESS, 0, 0);
	RNA_float_set(kmi->ptr, "factor", 0.83333f);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_backimage_zoom", VKEY, KM_PRESS, KM_ALT, 0);
	RNA_float_set(kmi->ptr, "factor", 1.2f);
	WM_keymap_add_item(keymap, "NODE_OT_backimage_fit", HOMEKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_backimage_sample", ACTIONMOUSE, KM_PRESS, KM_ALT, 0);

	kmi = WM_keymap_add_item(keymap, "NODE_OT_link_make", FKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "replace", FALSE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_link_make", FKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "replace", TRUE);

	WM_keymap_add_menu(keymap, "NODE_MT_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_duplicate_move", DKEY, KM_PRESS, KM_SHIFT, 0);
	/* modified operator call for duplicating with input links */
	WM_keymap_add_item(keymap, "NODE_OT_duplicate_move_keep_inputs", DKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_parent_set", PKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_parent_clear", PKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_join", JKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_hide_toggle", HKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_mute_toggle", MKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_preview_toggle", HKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_hide_socket_toggle", HKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_view_all", NDOF_BUTTON_FIT, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "tweak", FALSE);

	WM_keymap_add_item(keymap, "NODE_OT_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_delete", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_delete_reconnect", XKEY, KM_PRESS, KM_CTRL, 0);

	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	WM_keymap_add_item(keymap, "NODE_OT_select_linked_to", LKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_linked_from", LKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "NODE_OT_select_same_type", GKEY, KM_PRESS, KM_SHIFT, 0);

	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_same_type_step", RIGHTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "prev", FALSE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_select_same_type_step", LEFTBRACKETKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "prev", TRUE);
	
	WM_keymap_add_item(keymap, "NODE_OT_find_node", FKEY, KM_PRESS, KM_CTRL, 0);
	
	/* node group operators */
	WM_keymap_add_item(keymap, "NODE_OT_group_make", GKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_group_ungroup", GKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_group_separate", PKEY, KM_PRESS, 0, 0);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_group_edit", TABKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "exit", FALSE);
	kmi = WM_keymap_add_item(keymap, "NODE_OT_group_edit", TABKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "exit", TRUE);

	WM_keymap_add_item(keymap, "NODE_OT_read_renderlayers", RKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_read_fullsamplelayers", RKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "NODE_OT_render_changed", ZKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "NODE_OT_clipboard_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "NODE_OT_clipboard_paste", VKEY, KM_PRESS, KM_CTRL, 0);
#ifdef __APPLE__
	WM_keymap_add_item(keymap, "NODE_OT_clipboard_copy", CKEY, KM_PRESS, KM_OSKEY, 0);
	WM_keymap_add_item(keymap, "NODE_OT_clipboard_paste", VKEY, KM_PRESS, KM_OSKEY, 0);
#endif
	WM_keymap_add_item(keymap, "NODE_OT_viewer_border", BKEY, KM_PRESS, KM_CTRL, 0);

	transform_keymap_for_space(keyconf, keymap, SPACE_NODE);
}
