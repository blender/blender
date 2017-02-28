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

#include "BKE_context.h"

#include "BLI_math.h"

#include "ED_screen.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "outliner_intern.h"


enum {
	OUTLINER_ITEM_DRAG_CANCEL,
	OUTLINER_ITEM_DRAG_CONFIRM,
};

typedef struct OutlinerItemDrag {
	TreeElement *dragged_te;
	int init_mouse_xy[2];
} OutlinerItemDrag;

static int outliner_item_drag_drop_poll(bContext *C)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	return ED_operator_outliner_active(C) &&
	       /* Only collection display modes supported for now. Others need more design work */
	       ELEM(soops->outlinevis, SO_ACT_LAYER, SO_COLLECTIONS);
}

static TreeElement *outliner_item_drag_element_find(SpaceOops *soops, ARegion *ar, const wmEvent *event)
{
	const float my = UI_view2d_region_to_view_y(&ar->v2d, event->mval[1]);
	return outliner_find_item_at_y(soops, &soops->tree, my);
}

static OutlinerItemDrag *outliner_item_drag_data_create(TreeElement *dragged_te, const int mouse_xy[2])
{
	OutlinerItemDrag *drag_data = MEM_mallocN(sizeof(*drag_data), __func__);

	drag_data->dragged_te = dragged_te;
	copy_v2_v2_int(drag_data->init_mouse_xy, mouse_xy);

	return drag_data;
}

static void outliner_item_drag_end(OutlinerItemDrag *op_drag_data)
{
	MEM_SAFE_FREE(op_drag_data->dragged_te->drag_data);
	MEM_freeN(op_drag_data);
}

static void outliner_item_drag_handle(OutlinerItemDrag *op_drag_data, ARegion *ar, const wmEvent *event)
{
	TreeElement *dragged_te = op_drag_data->dragged_te;
	const int delta_mouse_y = event->y - op_drag_data->init_mouse_xy[1];
	const int cmp_coord = (int)UI_view2d_region_to_view_y(&ar->v2d, event->mval[1]);

	/* by default we don't change the item position */
	dragged_te->drag_data->insert_te = dragged_te;

	if (delta_mouse_y > 0) {
		for (TreeElement *te = dragged_te->prev; te && (cmp_coord >= (te->ys + (UI_UNIT_Y * 0.5f))); te = te->prev) {
			/* will be NULL if we want to insert as first element */
			dragged_te->drag_data->insert_te = te->prev;
		}
	}
	else {
		for (TreeElement *te = dragged_te->next; te && (cmp_coord <= (te->ys + (UI_UNIT_Y * 0.5f))); te = te->next) {
			dragged_te->drag_data->insert_te = te;
		}
	}
}

static bool outliner_item_drag_drop_apply(const Scene *scene, OutlinerItemDrag *op_drag_data)
{
	TreeElement *dragged_te = op_drag_data->dragged_te;
	TreeElement *insert_after = dragged_te->drag_data->insert_te;

	if (insert_after == dragged_te) {
		/* No need to do anything */
		return false;
	}

	if (dragged_te->reinsert) {
		/* Not sure yet what the best way to handle reordering elements of different types
		 * (and stored in different lists). For collection display mode this is enough. */
		if (!insert_after || (insert_after->reinsert == dragged_te->reinsert)) {
			dragged_te->reinsert(scene, dragged_te, insert_after);
		}
	}

	return true;
}

static int outliner_item_drag_drop_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	OutlinerItemDrag *op_drag_data = op->customdata;
	int retval = OPERATOR_RUNNING_MODAL;
	bool redraw = false;
	bool skip_rebuild = true;

	switch (event->type) {
		case EVT_MODAL_MAP:
			if (event->val == OUTLINER_ITEM_DRAG_CONFIRM) {
				outliner_item_drag_drop_apply(CTX_data_scene(C), op_drag_data);
				skip_rebuild = false;
				retval = OPERATOR_FINISHED;
			}
			else if (event->val == OUTLINER_ITEM_DRAG_CANCEL) {
				retval = OPERATOR_CANCELLED;
			}
			else {
				BLI_assert(0);
			}
			WM_event_add_mousemove(C); /* update highlight */
			outliner_item_drag_end(op_drag_data);
			redraw = true;
			break;
		case MOUSEMOVE:
			outliner_item_drag_handle(op_drag_data, ar, event);
			redraw = true;
			break;
	}

	if (skip_rebuild) {
		soops->storeflag |= SO_TREESTORE_REDRAW; /* only needs to redraw, no rebuild */
	}
	if (redraw) {
		ED_region_tag_redraw(ar);
	}

	return retval;
}

static int outliner_item_drag_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te = outliner_item_drag_element_find(soops, ar, event);

	if (!te) {
		return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
	}


	op->customdata = outliner_item_drag_data_create(te, &event->x);
	te->drag_data = MEM_callocN(sizeof(*te->drag_data), __func__);
	/* by default we don't change the item position */
	te->drag_data->insert_te = te;
	/* unset highlighted tree element, dragged one will be highlighted instead */
	outliner_set_flag(&soops->tree, TSE_HIGHLIGHTED, false);

	soops->storeflag |= SO_TREESTORE_REDRAW; /* only needs to redraw, no rebuild */
	ED_region_tag_redraw(ar);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/**
 * Notes about Outliner Item Drag 'n Drop:
 * Right now only collections display mode is supported. But ideally all/most modes would support this. There are
 * just some open design questions that have to be answered: do we want to allow mixing order of different data types
 * (like render-layers and objects)? Would that be a purely visual change or would that have any other effect? ...
 */
static void OUTLINER_OT_item_drag_drop(wmOperatorType *ot)
{
	ot->name = "Drag and Drop Item";
	ot->idname = "OUTLINER_OT_item_drag_drop";
	ot->description = "Change the hierarchical position of an item by repositioning it using drag and drop";

	ot->invoke = outliner_item_drag_drop_invoke;
	ot->modal = outliner_item_drag_drop_modal;

	ot->poll = outliner_item_drag_drop_poll;

	ot->flag = OPTYPE_UNDO;
}


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
	WM_operatortype_append(OUTLINER_OT_group_operation);
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
	WM_operatortype_append(OUTLINER_OT_collection_operation);

	WM_operatortype_append(OUTLINER_OT_show_one_level);
	WM_operatortype_append(OUTLINER_OT_show_active);
	WM_operatortype_append(OUTLINER_OT_show_hierarchy);
	WM_operatortype_append(OUTLINER_OT_scroll_page);
	
	WM_operatortype_append(OUTLINER_OT_selected_toggle);
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
	WM_operatortype_append(OUTLINER_OT_group_link);

	/* collections */
	WM_operatortype_append(OUTLINER_OT_collections_delete);
	WM_operatortype_append(OUTLINER_OT_collection_select);
	WM_operatortype_append(OUTLINER_OT_collection_link);
	WM_operatortype_append(OUTLINER_OT_collection_unlink);
	WM_operatortype_append(OUTLINER_OT_collection_new);
	WM_operatortype_append(OUTLINER_OT_collection_override_new);
	WM_operatortype_append(OUTLINER_OT_collection_objects_add);
	WM_operatortype_append(OUTLINER_OT_collection_objects_remove);
	WM_operatortype_append(OUTLINER_OT_collection_objects_select);
	WM_operatortype_append(OUTLINER_OT_collection_objects_deselect);
}

static wmKeyMap *outliner_item_drag_drop_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{OUTLINER_ITEM_DRAG_CANCEL,  "CANCEL",  0, "Cancel", ""},
		{OUTLINER_ITEM_DRAG_CONFIRM, "CONFIRM", 0, "Confirm/Drop", ""},
		{0, NULL, 0, NULL, NULL}
	};
	const char *map_name = "Outliner Item Drap 'n Drop Modal Map";

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
	
	WM_keymap_verify_item(keymap, "OUTLINER_OT_selected_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OUTLINER_OT_expanded_toggle", AKEY, KM_PRESS, KM_SHIFT, 0);
	
	/* keying sets - only for databrowse */
	WM_keymap_verify_item(keymap, "OUTLINER_OT_keyingset_add_selected", KKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OUTLINER_OT_keyingset_remove_selected", KKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "ANIM_OT_keyframe_insert", IKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "ANIM_OT_keyframe_delete", IKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_verify_item(keymap, "OUTLINER_OT_drivers_add_selected", DKEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "OUTLINER_OT_drivers_delete_selected", DKEY, KM_PRESS, KM_ALT, 0);

	outliner_item_drag_drop_modal_keymap(keyconf);
}

