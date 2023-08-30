/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#include "DNA_node_types.h"

#include "BKE_context.h"

#include "ED_node.hh" /* own include */
#include "ED_screen.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "node_intern.hh" /* own include */

namespace blender::ed::space_node {

void node_operatortypes()
{
  WM_operatortype_append(NODE_OT_select);
  WM_operatortype_append(NODE_OT_select_all);
  WM_operatortype_append(NODE_OT_select_linked_to);
  WM_operatortype_append(NODE_OT_select_linked_from);
  WM_operatortype_append(NODE_OT_select_box);
  WM_operatortype_append(NODE_OT_select_circle);
  WM_operatortype_append(NODE_OT_select_lasso);
  WM_operatortype_append(NODE_OT_select_grouped);
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
  WM_operatortype_append(NODE_OT_deactivate_viewer);

  WM_operatortype_append(NODE_OT_duplicate);
  WM_operatortype_append(NODE_OT_delete);
  WM_operatortype_append(NODE_OT_delete_reconnect);
  WM_operatortype_append(NODE_OT_resize);

  WM_operatortype_append(NODE_OT_link);
  WM_operatortype_append(NODE_OT_link_make);
  WM_operatortype_append(NODE_OT_links_cut);
  WM_operatortype_append(NODE_OT_links_detach);
  WM_operatortype_append(NODE_OT_links_mute);
  WM_operatortype_append(NODE_OT_add_reroute);

  WM_operatortype_append(NODE_OT_group_make);
  WM_operatortype_append(NODE_OT_group_insert);
  WM_operatortype_append(NODE_OT_group_ungroup);
  WM_operatortype_append(NODE_OT_group_separate);
  WM_operatortype_append(NODE_OT_group_edit);

  WM_operatortype_append(NODE_OT_link_viewer);

  WM_operatortype_append(NODE_OT_insert_offset);

  WM_operatortype_append(NODE_OT_read_viewlayers);
  WM_operatortype_append(NODE_OT_render_changed);

  WM_operatortype_append(NODE_OT_backimage_move);
  WM_operatortype_append(NODE_OT_backimage_zoom);
  WM_operatortype_append(NODE_OT_backimage_fit);
  WM_operatortype_append(NODE_OT_backimage_sample);

  WM_operatortype_append(NODE_OT_add_search);
  WM_operatortype_append(NODE_OT_add_group);
  WM_operatortype_append(NODE_OT_add_group_asset);
  WM_operatortype_append(NODE_OT_add_object);
  WM_operatortype_append(NODE_OT_add_collection);
  WM_operatortype_append(NODE_OT_add_file);
  WM_operatortype_append(NODE_OT_add_mask);
  WM_operatortype_append(NODE_OT_add_material);

  WM_operatortype_append(NODE_OT_new_node_tree);

  WM_operatortype_append(NODE_OT_output_file_add_socket);
  WM_operatortype_append(NODE_OT_output_file_remove_active_socket);
  WM_operatortype_append(NODE_OT_output_file_move_active_socket);

  WM_operatortype_append(NODE_OT_parent_set);
  WM_operatortype_append(NODE_OT_join);
  WM_operatortype_append(NODE_OT_attach);
  WM_operatortype_append(NODE_OT_detach);

  WM_operatortype_append(NODE_OT_clipboard_copy);
  WM_operatortype_append(NODE_OT_clipboard_paste);

  WM_operatortype_append(NODE_OT_shader_script_update);

  WM_operatortype_append(NODE_OT_viewer_border);
  WM_operatortype_append(NODE_OT_clear_viewer_border);

  WM_operatortype_append(NODE_OT_switch_view_update);

  WM_operatortype_append(NODE_OT_cryptomatte_layer_add);
  WM_operatortype_append(NODE_OT_cryptomatte_layer_remove);
}

void node_keymap(wmKeyConfig *keyconf)
{
  /* Entire Editor only ----------------- */
  WM_keymap_ensure(keyconf, "Node Generic", SPACE_NODE, 0);

  /* Main Region only ----------------- */
  WM_keymap_ensure(keyconf, "Node Editor", SPACE_NODE, 0);

  node_link_modal_keymap(keyconf);
}

}  // namespace blender::ed::space_node

void ED_operatormacros_node()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *mot;

  ot = WM_operatortype_append_macro("NODE_OT_select_link_viewer",
                                    "Link Viewer",
                                    "Select node and link it to a viewer node",
                                    OPTYPE_UNDO);
  mot = WM_operatortype_macro_define(ot, "NODE_OT_select");
  RNA_boolean_set(mot->ptr, "extend", false);
  RNA_boolean_set(mot->ptr, "socket_select", true);
  RNA_boolean_set(mot->ptr, "clear_viewer", true);
  WM_operatortype_macro_define(ot, "NODE_OT_link_viewer");

  ot = WM_operatortype_append_macro("NODE_OT_translate_attach",
                                    "Move and Attach",
                                    "Move nodes and attach to frame",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  mot = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  WM_operatortype_macro_define(ot, "NODE_OT_attach");

  /* NODE_OT_translate_attach with remove_on_cancel set to true. */
  ot = WM_operatortype_append_macro("NODE_OT_translate_attach_remove_on_cancel",
                                    "Move and Attach",
                                    "Move nodes and attach to frame",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  mot = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(mot->ptr, "remove_on_cancel", true);
  RNA_boolean_set(mot->ptr, "view2d_edge_pan", true);
  WM_operatortype_macro_define(ot, "NODE_OT_attach");

  /* NOTE: Currently not in a default keymap or menu due to messy keymaps
   * and tricky invoke functionality.
   * Kept around in case users want to make own shortcuts.
   */
  ot = WM_operatortype_append_macro("NODE_OT_detach_translate_attach",
                                    "Detach and Move",
                                    "Detach nodes, move and attach to frame",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "NODE_OT_detach");
  mot = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  WM_operatortype_macro_define(ot, "NODE_OT_attach");

  ot = WM_operatortype_append_macro("NODE_OT_duplicate_move",
                                    "Duplicate",
                                    "Duplicate selected nodes and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  mot = WM_operatortype_macro_define(ot, "NODE_OT_duplicate");
  RNA_boolean_set(mot->ptr, "linked", false);
  WM_operatortype_macro_define(ot, "NODE_OT_translate_attach");

  ot = WM_operatortype_append_macro(
      "NODE_OT_duplicate_move_linked",
      "Duplicate Linked",
      "Duplicate selected nodes, but not their node trees, and move them",
      OPTYPE_UNDO | OPTYPE_REGISTER);
  mot = WM_operatortype_macro_define(ot, "NODE_OT_duplicate");
  RNA_boolean_set(mot->ptr, "linked", true);
  WM_operatortype_macro_define(ot, "NODE_OT_translate_attach");

  /* modified operator call for duplicating with input links */
  ot = WM_operatortype_append_macro("NODE_OT_duplicate_move_keep_inputs",
                                    "Duplicate",
                                    "Duplicate selected nodes keeping input links and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  mot = WM_operatortype_macro_define(ot, "NODE_OT_duplicate");
  RNA_boolean_set(mot->ptr, "keep_inputs", true);
  WM_operatortype_macro_define(ot, "NODE_OT_translate_attach");

  ot = WM_operatortype_append_macro("NODE_OT_move_detach_links",
                                    "Detach",
                                    "Move a node to detach links",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "NODE_OT_links_detach");
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");

  ot = WM_operatortype_append_macro("NODE_OT_move_detach_links_release",
                                    "Detach",
                                    "Move a node to detach links",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "NODE_OT_links_detach");
  WM_operatortype_macro_define(ot, "NODE_OT_translate_attach");
}
