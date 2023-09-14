/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "WM_api.hh"

#include "ED_screen.hh"

#include "outliner_intern.hh"

namespace blender::ed::outliner {
/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void outliner_operatortypes()
{
  WM_operatortype_append(OUTLINER_OT_highlight_update);
  WM_operatortype_append(OUTLINER_OT_item_activate);
  WM_operatortype_append(OUTLINER_OT_select_box);
  WM_operatortype_append(OUTLINER_OT_select_walk);
  WM_operatortype_append(OUTLINER_OT_item_openclose);
  WM_operatortype_append(OUTLINER_OT_item_rename);
  WM_operatortype_append(OUTLINER_OT_item_drag_drop);
  WM_operatortype_append(OUTLINER_OT_operation);
  WM_operatortype_append(OUTLINER_OT_scene_operation);
  WM_operatortype_append(OUTLINER_OT_object_operation);
  WM_operatortype_append(OUTLINER_OT_lib_operation);
  WM_operatortype_append(OUTLINER_OT_lib_relocate);
  WM_operatortype_append(OUTLINER_OT_liboverride_operation);
  WM_operatortype_append(OUTLINER_OT_liboverride_troubleshoot_operation);
  WM_operatortype_append(OUTLINER_OT_id_operation);
  WM_operatortype_append(OUTLINER_OT_id_delete);
  WM_operatortype_append(OUTLINER_OT_id_remap);
  WM_operatortype_append(OUTLINER_OT_id_copy);
  WM_operatortype_append(OUTLINER_OT_id_paste);
  WM_operatortype_append(OUTLINER_OT_data_operation);
  WM_operatortype_append(OUTLINER_OT_animdata_operation);
  WM_operatortype_append(OUTLINER_OT_action_set);
  WM_operatortype_append(OUTLINER_OT_constraint_operation);
  WM_operatortype_append(OUTLINER_OT_modifier_operation);
  WM_operatortype_append(OUTLINER_OT_delete);

  WM_operatortype_append(OUTLINER_OT_show_one_level);
  WM_operatortype_append(OUTLINER_OT_show_active);
  WM_operatortype_append(OUTLINER_OT_show_hierarchy);
  WM_operatortype_append(OUTLINER_OT_scroll_page);

  WM_operatortype_append(OUTLINER_OT_select_all);
  WM_operatortype_append(OUTLINER_OT_expanded_toggle);

  WM_operatortype_append(OUTLINER_OT_keyingset_add_selected);
  WM_operatortype_append(OUTLINER_OT_keyingset_remove_selected);

  WM_operatortype_append(OUTLINER_OT_drivers_add_selected);
  WM_operatortype_append(OUTLINER_OT_drivers_delete_selected);

  WM_operatortype_append(OUTLINER_OT_orphans_purge);

  WM_operatortype_append(OUTLINER_OT_parent_drop);
  WM_operatortype_append(OUTLINER_OT_parent_clear);
  WM_operatortype_append(OUTLINER_OT_scene_drop);
  WM_operatortype_append(OUTLINER_OT_material_drop);
  WM_operatortype_append(OUTLINER_OT_datastack_drop);
  WM_operatortype_append(OUTLINER_OT_collection_drop);

  /* collections */
  WM_operatortype_append(OUTLINER_OT_collection_new);
  WM_operatortype_append(OUTLINER_OT_collection_duplicate_linked);
  WM_operatortype_append(OUTLINER_OT_collection_duplicate);
  WM_operatortype_append(OUTLINER_OT_collection_hierarchy_delete);
  WM_operatortype_append(OUTLINER_OT_collection_objects_select);
  WM_operatortype_append(OUTLINER_OT_collection_objects_deselect);
  WM_operatortype_append(OUTLINER_OT_collection_link);
  WM_operatortype_append(OUTLINER_OT_collection_instance);
  WM_operatortype_append(OUTLINER_OT_collection_exclude_set);
  WM_operatortype_append(OUTLINER_OT_collection_exclude_clear);
  WM_operatortype_append(OUTLINER_OT_collection_holdout_set);
  WM_operatortype_append(OUTLINER_OT_collection_holdout_clear);
  WM_operatortype_append(OUTLINER_OT_collection_indirect_only_set);
  WM_operatortype_append(OUTLINER_OT_collection_indirect_only_clear);

  WM_operatortype_append(OUTLINER_OT_collection_isolate);
  WM_operatortype_append(OUTLINER_OT_collection_disable);
  WM_operatortype_append(OUTLINER_OT_collection_enable);
  WM_operatortype_append(OUTLINER_OT_collection_hide);
  WM_operatortype_append(OUTLINER_OT_collection_show);
  WM_operatortype_append(OUTLINER_OT_collection_disable_render);
  WM_operatortype_append(OUTLINER_OT_collection_enable_render);
  WM_operatortype_append(OUTLINER_OT_collection_hide_inside);
  WM_operatortype_append(OUTLINER_OT_collection_show_inside);
  WM_operatortype_append(OUTLINER_OT_hide);
  WM_operatortype_append(OUTLINER_OT_unhide_all);

  WM_operatortype_append(OUTLINER_OT_collection_color_tag_set);
}

void outliner_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Outliner", SPACE_OUTLINER, RGN_TYPE_WINDOW);
}

/** \} */

}  // namespace blender::ed::outliner
