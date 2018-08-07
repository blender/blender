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

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_group_types.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_main.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "outliner_intern.h"

/* ************************** registration **********************************/

void outliner_operatortypes(void)
{
	WM_operatortype_append(OUTLINER_OT_highlight_update);
	WM_operatortype_append(OUTLINER_OT_item_activate);
	WM_operatortype_append(OUTLINER_OT_select_border);
	WM_operatortype_append(OUTLINER_OT_item_openclose);
	WM_operatortype_append(OUTLINER_OT_item_rename);
	WM_operatortype_append(OUTLINER_OT_item_drag_drop);
	WM_operatortype_append(OUTLINER_OT_operation);
	WM_operatortype_append(OUTLINER_OT_scene_operation);
	WM_operatortype_append(OUTLINER_OT_object_operation);
	WM_operatortype_append(OUTLINER_OT_lib_operation);
	WM_operatortype_append(OUTLINER_OT_lib_relocate);
	WM_operatortype_append(OUTLINER_OT_id_operation);
	WM_operatortype_append(OUTLINER_OT_id_delete);
	WM_operatortype_append(OUTLINER_OT_id_remap);
	WM_operatortype_append(OUTLINER_OT_data_operation);
	WM_operatortype_append(OUTLINER_OT_animdata_operation);
	WM_operatortype_append(OUTLINER_OT_action_set);
	WM_operatortype_append(OUTLINER_OT_constraint_operation);
	WM_operatortype_append(OUTLINER_OT_modifier_operation);

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
	WM_operatortype_append(OUTLINER_OT_collection_drop);

	/* collections */
	WM_operatortype_append(OUTLINER_OT_collection_new);
	WM_operatortype_append(OUTLINER_OT_collection_duplicate);
	WM_operatortype_append(OUTLINER_OT_collection_delete);
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
}

static wmKeyMap *outliner_item_drag_drop_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{OUTLINER_ITEM_DRAG_CANCEL,  "CANCEL",  0, "Cancel", ""},
		{OUTLINER_ITEM_DRAG_CONFIRM, "CONFIRM", 0, "Confirm/Drop", ""},
		{0, NULL, 0, NULL, NULL}
	};
	const char *map_name = "Outliner Item Drag & Drop Modal Map";

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, map_name);

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items)
		return NULL;

	keymap = WM_modalkeymap_add(keyconf, map_name, modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, OUTLINER_ITEM_DRAG_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, OUTLINER_ITEM_DRAG_CANCEL);

	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, OUTLINER_ITEM_DRAG_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_RELEASE, KM_ANY, 0, OUTLINER_ITEM_DRAG_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_RELEASE, KM_ANY, 0, OUTLINER_ITEM_DRAG_CONFIRM);

	WM_modalkeymap_assign(keymap, "OUTLINER_OT_item_drag_drop");

	return keymap;
}

void outliner_keymap(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Outliner", SPACE_OUTLINER, 0);
	wmKeyMapItem *kmi;

	WM_keymap_add_item(keymap, "OUTLINER_OT_highlight_update", MOUSEMOVE, KM_ANY, KM_ANY, 0);

	WM_keymap_add_item(keymap, "OUTLINER_OT_item_rename", LEFTMOUSE, KM_DBL_CLICK, 0, 0);

	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_activate", LEFTMOUSE, KM_CLICK, 0, 0);
	RNA_boolean_set(kmi->ptr, "recursive", false);
	RNA_boolean_set(kmi->ptr, "extend", false);

	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_activate", LEFTMOUSE, KM_CLICK, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "recursive", false);
	RNA_boolean_set(kmi->ptr, "extend", true);

	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_activate", LEFTMOUSE, KM_CLICK, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "recursive", true);
	RNA_boolean_set(kmi->ptr, "extend", false);

	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_activate", LEFTMOUSE, KM_CLICK, KM_CTRL | KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "recursive", true);
	RNA_boolean_set(kmi->ptr, "extend", true);


	WM_keymap_add_item(keymap, "OUTLINER_OT_select_border", BKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_openclose", RETKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "all", false);
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_item_openclose", RETKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "all", true);

	WM_keymap_add_item(keymap, "OUTLINER_OT_item_rename", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "OUTLINER_OT_operation", RIGHTMOUSE, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "OUTLINER_OT_item_drag_drop", EVT_TWEAK_L, KM_ANY, 0, 0);

	WM_keymap_add_item(keymap, "OUTLINER_OT_show_hierarchy", HOMEKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "OUTLINER_OT_show_active", PERIODKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OUTLINER_OT_show_active", PADPERIOD, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_scroll_page", PAGEDOWNKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "up", false);
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_scroll_page", PAGEUPKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "up", true);

	WM_keymap_add_item(keymap, "OUTLINER_OT_show_one_level", PADPLUSKEY, KM_PRESS, 0, 0); /* open */
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_show_one_level", PADMINUS, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "open", false); /* close */

	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_SELECT);
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_select_all", AKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_DESELECT);
	kmi = WM_keymap_add_item(keymap, "OUTLINER_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	WM_keymap_add_item(keymap, "OUTLINER_OT_expanded_toggle", AKEY, KM_PRESS, KM_SHIFT, 0);

	/* keying sets - only for databrowse */
	WM_keymap_add_item(keymap, "OUTLINER_OT_keyingset_add_selected", KKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OUTLINER_OT_keyingset_remove_selected", KKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "ANIM_OT_keyframe_insert", IKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "ANIM_OT_keyframe_delete", IKEY, KM_PRESS, KM_ALT, 0);

	/* Note: was D, Alt-D, keep these free for duplicate. */
	WM_keymap_add_item(keymap, "OUTLINER_OT_drivers_add_selected", DKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "OUTLINER_OT_drivers_delete_selected", DKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);

	WM_keymap_add_item(keymap, "OUTLINER_OT_collection_new", CKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "OUTLINER_OT_collection_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OUTLINER_OT_collection_delete", DELKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "OBJECT_OT_move_to_collection", MKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OBJECT_OT_link_to_collection", MKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "OUTLINER_OT_collection_exclude_set", EKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "OUTLINER_OT_collection_exclude_clear", EKEY, KM_PRESS, KM_ALT, 0);

	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_hide_view_clear", HKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "select", false);
	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_hide_view_set", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", false);
	kmi = WM_keymap_add_item(keymap, "OBJECT_OT_hide_view_set", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", true);

	outliner_item_drag_drop_modal_keymap(keyconf);
}
