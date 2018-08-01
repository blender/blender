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

typedef struct OutlinerDragDropTooltip {
	TreeElement *te;
	void *handle;
} OutlinerDragDropTooltip;

enum {
	OUTLINER_ITEM_DRAG_CANCEL,
	OUTLINER_ITEM_DRAG_CONFIRM,
};

static bool outliner_item_drag_drop_poll(bContext *C)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	return ED_operator_outliner_active(C) &&
	       /* Only collection display modes supported for now. Others need more design work */
	       ELEM(soops->outlinevis, SO_VIEW_LAYER, SO_LIBRARIES);
}

static TreeElement *outliner_item_drag_element_find(SpaceOops *soops, ARegion *ar, const wmEvent *event)
{
	/* note: using EVT_TWEAK_ events to trigger dragging is fine,
	 * it sends coordinates from where dragging was started */
	const float my = UI_view2d_region_to_view_y(&ar->v2d, event->mval[1]);
	return outliner_find_item_at_y(soops, &soops->tree, my);
}

static void outliner_item_drag_end(wmWindow *win, OutlinerDragDropTooltip *data)
{
	MEM_SAFE_FREE(data->te->drag_data);

	if (data->handle) {
		WM_draw_cb_exit(win, data->handle);
	}

	MEM_SAFE_FREE(data);
}

static void outliner_item_drag_get_insert_data(
        const SpaceOops *soops, ARegion *ar, const wmEvent *event, TreeElement *te_dragged,
        TreeElement **r_te_insert_handle, TreeElementInsertType *r_insert_type)
{
	TreeElement *te_hovered;
	float view_mval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);
	te_hovered = outliner_find_item_at_y(soops, &soops->tree, view_mval[1]);

	if (te_hovered) {
		/* mouse hovers an element (ignoring x-axis), now find out how to insert the dragged item exactly */

		if (te_hovered == te_dragged) {
			*r_te_insert_handle = te_dragged;
		}
		else if (te_hovered != te_dragged) {
			const float margin = UI_UNIT_Y * (1.0f / 4);

			*r_te_insert_handle = te_hovered;
			if (view_mval[1] < (te_hovered->ys + margin)) {
				if (TSELEM_OPEN(TREESTORE(te_hovered), soops)) {
					/* inserting after a open item means we insert into it, but as first child */
					if (BLI_listbase_is_empty(&te_hovered->subtree)) {
						*r_insert_type = TE_INSERT_INTO;
					}
					else {
						*r_insert_type = TE_INSERT_BEFORE;
						*r_te_insert_handle = te_hovered->subtree.first;
					}
				}
				else {
					*r_insert_type = TE_INSERT_AFTER;
				}
			}
			else if (view_mval[1] > (te_hovered->ys + (3 * margin))) {
				*r_insert_type = TE_INSERT_BEFORE;
			}
			else {
				*r_insert_type = TE_INSERT_INTO;
			}
		}
	}
	else {
		/* mouse doesn't hover any item (ignoring x-axis), so it's either above list bounds or below. */

		TreeElement *first = soops->tree.first;
		TreeElement *last = soops->tree.last;

		if (view_mval[1] < last->ys) {
			*r_te_insert_handle = last;
			*r_insert_type = TE_INSERT_AFTER;
		}
		else if (view_mval[1] > (first->ys + UI_UNIT_Y)) {
			*r_te_insert_handle = first;
			*r_insert_type = TE_INSERT_BEFORE;
		}
		else {
			BLI_assert(0);
		}
	}
}

static void outliner_item_drag_handle(
        SpaceOops *soops, ARegion *ar, const wmEvent *event, TreeElement *te_dragged)
{
	TreeElement *te_insert_handle;
	TreeElementInsertType insert_type;

	outliner_item_drag_get_insert_data(soops, ar, event, te_dragged, &te_insert_handle, &insert_type);

	if (!te_dragged->reinsert_poll &&
	    /* there is no reinsert_poll, so we do some generic checks (same types and reinsert callback is available) */
	    (TREESTORE(te_dragged)->type == TREESTORE(te_insert_handle)->type) &&
	    te_dragged->reinsert)
	{
		/* pass */
	}
	else if (te_dragged == te_insert_handle) {
		/* nothing will happen anyway, no need to do poll check */
	}
	else if (!te_dragged->reinsert_poll ||
	         !te_dragged->reinsert_poll(te_dragged, &te_insert_handle, &insert_type))
	{
		te_insert_handle = NULL;
	}
	te_dragged->drag_data->insert_type = insert_type;
	te_dragged->drag_data->insert_handle = te_insert_handle;
}

/**
 * Returns true if it is a collection and empty.
 */
static bool is_empty_collection(TreeElement *te)
{
	Collection *collection = outliner_collection_from_tree_element(te);

	if (!collection) {
		return false;
	}

	return BLI_listbase_is_empty(&collection->gobject) &&
	       BLI_listbase_is_empty(&collection->children);
}

static bool outliner_item_drag_drop_apply(
        Main *bmain,
        Scene *scene,
        SpaceOops *soops,
        OutlinerDragDropTooltip *data,
        const wmEvent *event)
{
	TreeElement *dragged_te = data->te;
	TreeElement *insert_handle = dragged_te->drag_data->insert_handle;
	TreeElementInsertType insert_type = dragged_te->drag_data->insert_type;

	if ((insert_handle == dragged_te) || !insert_handle) {
		/* No need to do anything */
	}
	else if (dragged_te->reinsert) {
		BLI_assert(!dragged_te->reinsert_poll || dragged_te->reinsert_poll(dragged_te, &insert_handle,
		                                                                   &insert_type));
		/* call of assert above should not have changed insert_handle and insert_type at this point */
		BLI_assert(dragged_te->drag_data->insert_handle == insert_handle &&
		           dragged_te->drag_data->insert_type == insert_type);

		/* If the collection was just created and you moved objects/collections inside it,
		 * it is strange to have it closed and we not see the newly dragged elements. */
		const bool should_open_collection = (insert_type == TE_INSERT_INTO) && is_empty_collection(insert_handle);

		dragged_te->reinsert(bmain, scene, soops, dragged_te, insert_handle, insert_type, event);

		if (should_open_collection && !is_empty_collection(insert_handle)) {
			TREESTORE(insert_handle)->flag &= ~TSE_CLOSED;
		}
		return true;
	}

	return false;
}

static int outliner_item_drag_drop_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	OutlinerDragDropTooltip *data = op->customdata;
	TreeElement *te_dragged = data->te;
	int retval = OPERATOR_RUNNING_MODAL;
	bool redraw = false;
	bool skip_rebuild = true;

	switch (event->type) {
		case EVT_MODAL_MAP:
			if (event->val == OUTLINER_ITEM_DRAG_CONFIRM) {
				if (outliner_item_drag_drop_apply(bmain, scene, soops, data, event)) {
					skip_rebuild = false;
				}
				retval = OPERATOR_FINISHED;
			}
			else if (event->val == OUTLINER_ITEM_DRAG_CANCEL) {
				retval = OPERATOR_CANCELLED;
			}
			else {
				BLI_assert(0);
			}
			WM_event_add_mousemove(C); /* update highlight */
			outliner_item_drag_end(CTX_wm_window(C), data);
			redraw = true;
			break;
		case MOUSEMOVE:
			outliner_item_drag_handle(soops, ar, event, te_dragged);
			redraw = true;
			break;
	}

	if (redraw) {
		if (skip_rebuild) {
			ED_region_tag_redraw_no_rebuild(ar);
		}
		else {
			ED_region_tag_redraw(ar);
		}
	}

	return retval;
}

static const char *outliner_drag_drop_tooltip_get(
        const TreeElement *te_float)
{
	const char *name = NULL;

	const TreeElement *te_insert = te_float->drag_data->insert_handle;
	if (te_float && outliner_is_collection_tree_element(te_float)) {
		if (te_insert == NULL) {
			name = TIP_("Move collection");
		}
		else {
			switch (te_float->drag_data->insert_type) {
				case TE_INSERT_BEFORE:
					if (te_insert->prev && outliner_is_collection_tree_element(te_insert->prev)) {
						name = TIP_("Move between collections");
					}
					else {
						name = TIP_("Move before collection");
					}
					break;
				case TE_INSERT_AFTER:
					if (te_insert->next && outliner_is_collection_tree_element(te_insert->next)) {
						name = TIP_("Move between collections");
					}
					else {
						name = TIP_("Move after collection");
					}
					break;
				case TE_INSERT_INTO:
					name = TIP_("Move inside collection");
					break;
			}
		}
	}
	else if ((TREESTORE(te_float)->type == 0) && (te_float->idcode == ID_OB)) {
		name = TIP_("Move to collection (Ctrl to link)");
	}

	return name;
}

static void outliner_drag_drop_tooltip_cb(const wmWindow *win, void *vdata)
{
	OutlinerDragDropTooltip *data = vdata;
	const char *tooltip;

	int cursorx, cursory;
	int x, y;

	tooltip = outliner_drag_drop_tooltip_get(data->te);
	if (tooltip == NULL) {
		return;
	}

	cursorx = win->eventstate->x;
	cursory = win->eventstate->y;

	x = cursorx + U.widget_unit;
	y = cursory - U.widget_unit;

	/* Drawing. */
	const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;

	const float col_fg[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	const float col_bg[4] = {0.0f, 0.0f, 0.0f, 0.2f};

	GPU_blend(true);
	UI_fontstyle_draw_simple_backdrop(fstyle, x, y, tooltip, col_fg, col_bg);
	GPU_blend(false);
}

static int outliner_item_drag_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te_dragged = outliner_item_drag_element_find(soops, ar, event);

	if (!te_dragged) {
		return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
	}

	OutlinerDragDropTooltip *data = MEM_mallocN(sizeof(OutlinerDragDropTooltip), __func__);
	data->te = te_dragged;

	op->customdata = data;
	te_dragged->drag_data = MEM_callocN(sizeof(*te_dragged->drag_data), __func__);
	/* by default we don't change the item position */
	te_dragged->drag_data->insert_handle = te_dragged;
	/* unset highlighted tree element, dragged one will be highlighted instead */
	outliner_flag_set(&soops->tree, TSE_HIGHLIGHTED, false);

	ED_region_tag_redraw_no_rebuild(ar);

	WM_event_add_modal_handler(C, op);

	data->handle = WM_draw_cb_activate(CTX_wm_window(C), outliner_drag_drop_tooltip_cb, data);

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
	ot->name = "Drag and Drop";
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
