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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_region_menu_pie.c
 *  \ingroup edinterface
 *
 * Pie Menu Region
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "ED_screen.h"

#include "interface_intern.h"
#include "interface_regions_intern.h"

/* -------------------------------------------------------------------- */
/** \name Pie Menu
 * \{ */

struct uiPieMenu {
	uiBlock *block_radial; /* radial block of the pie menu (more could be added later) */
	uiLayout *layout;
	int mx, my;
};

static uiBlock *ui_block_func_PIE(bContext *UNUSED(C), uiPopupBlockHandle *handle, void *arg_pie)
{
	uiBlock *block;
	uiPieMenu *pie = arg_pie;
	int minwidth, width, height;

	minwidth = 50;
	block = pie->block_radial;

	/* in some cases we create the block before the region,
	 * so we set it delayed here if necessary */
	if (BLI_findindex(&handle->region->uiblocks, block) == -1)
		UI_block_region_set(block, handle->region);

	UI_block_layout_resolve(block, &width, &height);

	UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_NUMSELECT);
	UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

	block->minbounds = minwidth;
	block->bounds = 1;
	block->mx = 0;
	block->my = 0;
	block->bounds_type = UI_BLOCK_BOUNDS_PIE_CENTER;

	block->pie_data.pie_center_spawned[0] = pie->mx;
	block->pie_data.pie_center_spawned[1] = pie->my;

	return pie->block_radial;
}

static float ui_pie_menu_title_width(const char *name, int icon)
{
	const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
	return (UI_fontstyle_string_width(fstyle, name) +
	         (UI_UNIT_X * (1.50f + (icon ? 0.25f : 0.0f))));
}

uiPieMenu *UI_pie_menu_begin(struct bContext *C, const char *title, int icon, const wmEvent *event)
{
	uiStyle *style;
	uiPieMenu *pie;
	short event_type;

	wmWindow *win = CTX_wm_window(C);

	style = UI_style_get_dpi();
	pie = MEM_callocN(sizeof(*pie), "pie menu");

	pie->block_radial = UI_block_begin(C, NULL, __func__, UI_EMBOSS);
	/* may be useful later to allow spawning pies
	 * from old positions */
	/* pie->block_radial->flag |= UI_BLOCK_POPUP_MEMORY; */
	pie->block_radial->puphash = ui_popup_menu_hash(title);
	pie->block_radial->flag |= UI_BLOCK_RADIAL;

	/* if pie is spawned by a left click, release or click event, it is always assumed to be click style */
	if (event->type == LEFTMOUSE || ELEM(event->val, KM_RELEASE, KM_CLICK)) {
		pie->block_radial->pie_data.flags |= UI_PIE_CLICK_STYLE;
		pie->block_radial->pie_data.event = EVENT_NONE;
		win->lock_pie_event = EVENT_NONE;
	}
	else {
		if (win->last_pie_event != EVENT_NONE) {
			/* original pie key has been released, so don't propagate the event */
			if (win->lock_pie_event == EVENT_NONE) {
				event_type = EVENT_NONE;
				pie->block_radial->pie_data.flags |= UI_PIE_CLICK_STYLE;
			}
			else
				event_type = win->last_pie_event;
		}
		else {
			event_type = event->type;
		}

		pie->block_radial->pie_data.event = event_type;
		win->lock_pie_event = event_type;
	}

	pie->layout = UI_block_layout(pie->block_radial, UI_LAYOUT_VERTICAL, UI_LAYOUT_PIEMENU, 0, 0, 200, 0, 0, style);

	/* Open from where we started dragging. */
	if (event->val == KM_CLICK_DRAG) {
		pie->mx = event->prevclickx;
		pie->my = event->prevclicky;
	}
	else {
		pie->mx = event->x;
		pie->my = event->y;
	}

	/* create title button */
	if (title[0]) {
		uiBut *but;
		char titlestr[256];
		int w;
		if (icon) {
			BLI_snprintf(titlestr, sizeof(titlestr), " %s", title);
			w = ui_pie_menu_title_width(titlestr, icon);
			but = uiDefIconTextBut(
			        pie->block_radial, UI_BTYPE_LABEL, 0, icon, titlestr, 0, 0, w, UI_UNIT_Y,
			        NULL, 0.0, 0.0, 0, 0, "");
		}
		else {
			w = ui_pie_menu_title_width(title, 0);
			but = uiDefBut(
			        pie->block_radial, UI_BTYPE_LABEL, 0, title, 0, 0, w, UI_UNIT_Y,
			        NULL, 0.0, 0.0, 0, 0, "");
		}
		/* do not align left */
		but->drawflag &= ~UI_BUT_TEXT_LEFT;
		pie->block_radial->pie_data.title = but->str;
		pie->block_radial->pie_data.icon = icon;
	}

	return pie;
}

void UI_pie_menu_end(bContext *C, uiPieMenu *pie)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *menu;

	menu = ui_popup_block_create(C, NULL, NULL, NULL, ui_block_func_PIE, pie);
	menu->popup = true;
	menu->towardstime = PIL_check_seconds_timer();

	UI_popup_handlers_add(
	        C, &window->modalhandlers,
	        menu, WM_HANDLER_ACCEPT_DBL_CLICK);
	WM_event_add_mousemove(C);

	menu->can_refresh = false;
	MEM_freeN(pie);
}

uiLayout *UI_pie_menu_layout(uiPieMenu *pie)
{
	return pie->layout;
}

int UI_pie_menu_invoke(struct bContext *C, const char *idname, const wmEvent *event)
{
	uiPieMenu *pie;
	uiLayout *layout;
	MenuType *mt = WM_menutype_find(idname, true);

	if (mt == NULL) {
		printf("%s: named menu \"%s\" not found\n", __func__, idname);
		return OPERATOR_CANCELLED;
	}

	if (WM_menutype_poll(C, mt) == false) {
		/* cancel but allow event to pass through, just like operators do */
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}

	pie = UI_pie_menu_begin(C, IFACE_(mt->label), ICON_NONE, event);
	layout = UI_pie_menu_layout(pie);

	UI_menutype_draw(C, mt, layout);

	UI_pie_menu_end(C, pie);

	return OPERATOR_INTERFACE;
}

int UI_pie_menu_invoke_from_operator_enum(
        struct bContext *C, const char *title, const char *opname,
        const char *propname, const wmEvent *event)
{
	uiPieMenu *pie;
	uiLayout *layout;

	pie = UI_pie_menu_begin(C, IFACE_(title), ICON_NONE, event);
	layout = UI_pie_menu_layout(pie);

	layout = uiLayoutRadial(layout);
	uiItemsEnumO(layout, opname, propname);

	UI_pie_menu_end(C, pie);

	return OPERATOR_INTERFACE;
}

int UI_pie_menu_invoke_from_rna_enum(
        struct bContext *C, const char *title, const char *path,
        const wmEvent *event)
{
	PointerRNA ctx_ptr;
	PointerRNA r_ptr;
	PropertyRNA *r_prop;
	uiPieMenu *pie;
	uiLayout *layout;

	RNA_pointer_create(NULL, &RNA_Context, C, &ctx_ptr);

	if (!RNA_path_resolve(&ctx_ptr, path, &r_ptr, &r_prop)) {
		return OPERATOR_CANCELLED;
	}

	/* invalid property, only accept enums */
	if (RNA_property_type(r_prop) != PROP_ENUM) {
		BLI_assert(0);
		return OPERATOR_CANCELLED;
	}

	pie = UI_pie_menu_begin(C, IFACE_(title), ICON_NONE, event);

	layout = UI_pie_menu_layout(pie);

	layout = uiLayoutRadial(layout);
	uiItemFullR(layout, &r_ptr, r_prop, RNA_NO_INDEX, 0, UI_ITEM_R_EXPAND, NULL, 0);

	UI_pie_menu_end(C, pie);

	return OPERATOR_INTERFACE;
}

/** \} */

/* -------------------------------------------------------------------- */
/**
 * \name Pie Menu Levels
 *
 * Pie menus can't contain more than 8 items (yet). When using #uiItemsFullEnumO, a "More" button is created that calls
 * a new pie menu if the enum has too many items. We call this a new "level".
 * Indirect recursion is used, so that a theoretically unlimited number of items is supported.
 *
 * This is a implementation specifically for operator enums, needed since the object mode pie now has more than 8
 * items. Ideally we'd have some way of handling this for all kinds of pie items, but that's tricky.
 *
 * - Julian (Feb 2016)
 *
 * \{ */

typedef struct PieMenuLevelData {
	char title[UI_MAX_NAME_STR]; /* parent pie title, copied for level */
	int icon; /* parent pie icon, copied for level */
	int totitem; /* total count of *remaining* items */

	/* needed for calling uiItemsFullEnumO_array again for new level */
	wmOperatorType *ot;
	const char *propname;
	IDProperty *properties;
	int context, flag;
} PieMenuLevelData;

/**
 * Invokes a new pie menu for a new level.
 */
static void ui_pie_menu_level_invoke(bContext *C, void *argN, void *arg2)
{
	EnumPropertyItem *item_array = (EnumPropertyItem *)argN;
	PieMenuLevelData *lvl = (PieMenuLevelData *)arg2;
	wmWindow *win = CTX_wm_window(C);

	uiPieMenu *pie = UI_pie_menu_begin(C, IFACE_(lvl->title), lvl->icon, win->eventstate);
	uiLayout *layout = UI_pie_menu_layout(pie);

	layout = uiLayoutRadial(layout);

	PointerRNA ptr;

	WM_operator_properties_create_ptr(&ptr, lvl->ot);
	/* so the context is passed to itemf functions (some need it) */
	WM_operator_properties_sanitize(&ptr, false);
	PropertyRNA *prop = RNA_struct_find_property(&ptr, lvl->propname);

	if (prop) {
		uiItemsFullEnumO_items(
		        layout, lvl->ot, ptr, prop, lvl->properties, lvl->context, lvl->flag,
		        item_array, lvl->totitem);
	}
	else {
		RNA_warning("%s.%s not found", RNA_struct_identifier(ptr.type), lvl->propname);
	}

	UI_pie_menu_end(C, pie);
}

/**
 * Set up data for defining a new pie menu level and add button that invokes it.
 */
void ui_pie_menu_level_create(
        uiBlock *block, wmOperatorType *ot, const char *propname, IDProperty *properties,
        const EnumPropertyItem *items, int totitem, int context, int flag)
{
	const int totitem_parent = PIE_MAX_ITEMS - 1;
	const int totitem_remain = totitem - totitem_parent;
	size_t array_size = sizeof(EnumPropertyItem) * totitem_remain;

	/* used as but->func_argN so freeing is handled elsewhere */
	EnumPropertyItem *remaining = MEM_mallocN(array_size + sizeof(EnumPropertyItem), "pie_level_item_array");
	memcpy(remaining, items + totitem_parent, array_size);
	/* a NULL terminating sentinal element is required */
	memset(&remaining[totitem_remain], 0, sizeof(EnumPropertyItem));


	/* yuk, static... issue is we can't reliably free this without doing dangerous changes */
	static PieMenuLevelData lvl;
	BLI_strncpy(lvl.title, block->pie_data.title, UI_MAX_NAME_STR);
	lvl.totitem    = totitem_remain;
	lvl.ot         = ot;
	lvl.propname   = propname;
	lvl.properties = properties;
	lvl.context    = context;
	lvl.flag       = flag;

	/* add a 'more' menu entry */
	uiBut *but = uiDefIconTextBut(block, UI_BTYPE_BUT, 0, ICON_PLUS, "More", 0, 0, UI_UNIT_X * 3, UI_UNIT_Y, NULL,
	                              0.0f, 0.0f, 0.0f, 0.0f, "Show more items of this menu");
	UI_but_funcN_set(but, ui_pie_menu_level_invoke, remaining, &lvl);
}

/** \} */ /* Pie Menu Levels */
