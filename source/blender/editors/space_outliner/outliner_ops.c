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

#include "DNA_collection_types.h"

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
#include "ED_select_utils.h"

#include "outliner_intern.h"

/* ************************** registration **********************************/

void outliner_operatortypes(void)
{
	WM_operatortype_append(OUTLINER_OT_highlight_update);
	WM_operatortype_append(OUTLINER_OT_item_activate);
	WM_operatortype_append(OUTLINER_OT_select_box);
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

void outliner_keymap(wmKeyConfig *keyconf)
{
	WM_keymap_ensure(keyconf, "Outliner", SPACE_OUTLINER, 0);
}
