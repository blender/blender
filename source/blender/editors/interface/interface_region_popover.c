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

/** \file blender/editors/interface/interface_region_popover.c
 *  \ingroup edinterface
 *
 * Pop-Over Region
 *
 * \note This is very close to 'interface_region_menu_popup.c'
 *
 * We could even merge them, however menu logic is already over-loaded.
 * PopOver's have the following differences.
 *
 * - UI is not constrained to a list.
 * - Pressing a button won't close the pop-over.
 * - Different draw style (to show this is has different behavior from a menu).
 * - #PanelType are used instead of #MenuType.
 * - No menu flipping support.
 * - No moving the menu to fit the mouse cursor.
 * - No key accelerators to access menu items
 *   (if we add support they would work differently).
 * - No arrow key navigation.
 * - No menu memory.
 * - No title.
 */

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_listbase.h"

#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"


#include "UI_interface.h"

#include "interface_intern.h"
#include "interface_regions_intern.h"

/* -------------------------------------------------------------------- */
/** \name Popup Menu with Callback or String
 * \{ */

struct uiPopover {
	uiBlock *block;
	uiLayout *layout;
	uiBut *but;
	ARegion *butregion;

	int mx, my;
	bool popover, slideout;

	uiMenuCreateFunc menu_func;
	void *menu_arg;
};

static uiBlock *ui_block_func_POPOVER(bContext *C, uiPopupBlockHandle *handle, void *arg_pup)
{
	uiBlock *block;
	uiPopover *pup = arg_pup;
	int offset[2], minwidth, width, height;

	if (pup->menu_func) {
		pup->block->handle = handle;
		pup->menu_func(C, pup->layout, pup->menu_arg);
		pup->block->handle = NULL;
	}

	if (pup->but) {
		/* minimum width to enforece */
		minwidth = BLI_rctf_size_x(&pup->but->rect);
	}
	else {
		minwidth = 50;
	}

	block = pup->block;

	/* in some cases we create the block before the region,
	 * so we set it delayed here if necessary */
	if (BLI_findindex(&handle->region->uiblocks, block) == -1)
		UI_block_region_set(block, handle->region);

	UI_block_layout_resolve(block, &width, &height);

	UI_block_flag_enable(block, UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_KEEP_OPEN | UI_BLOCK_POPOVER);

	UI_block_direction_set(block, UI_DIR_DOWN | UI_DIR_CENTER_X);

	const int block_margin = U.widget_unit / 2;

	if (pup->popover) {
		UI_block_flag_enable(block, UI_BLOCK_LOOP);
		UI_block_direction_set(block, block->direction);
		block->minbounds = minwidth;
		UI_block_bounds_set_popup(block, block_margin, offset[0], offset[1]);
	}
	else {
		/* for a header menu we set the direction automatic */
		block->minbounds = minwidth;
		UI_block_bounds_set_normal(block, block_margin);
	}

	/* if menu slides out of other menu, override direction */
	if (pup->slideout)
		UI_block_direction_set(block, UI_DIR_RIGHT);

	return pup->block;
}

uiPopupBlockHandle *ui_popover_panel_create(
        bContext *C, ARegion *butregion, uiBut *but,
        uiMenuCreateFunc menu_func, void *arg)
{
	wmWindow *window = CTX_wm_window(C);
	uiStyle *style = UI_style_get_dpi();
	uiPopupBlockHandle *handle;
	uiPopover *pup;

	pup = MEM_callocN(sizeof(uiPopover), __func__);
	pup->block = UI_block_begin(C, NULL, __func__, UI_EMBOSS);
	UI_block_emboss_set(pup->block, UI_EMBOSS);
	pup->layout = UI_block_layout(
	        pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0,
	        U.widget_unit * UI_POPOVER_WIDTH_UNITS, 0, MENU_PADDING, style);
	pup->slideout = false; // but ? ui_block_is_menu(but->block) : false;
	pup->but = but;
	uiLayoutSetOperatorContext(pup->layout, WM_OP_INVOKE_REGION_WIN);

	if (!but) {
		/* no button to start from, means we are a popover */
		pup->mx = window->eventstate->x;
		pup->my = window->eventstate->y;
		pup->popover = true;
		pup->block->flag |= UI_BLOCK_NO_FLIP;
	}
	/* some enums reversing is strange, currently we have no good way to
	 * reverse some enum's but not others, so reverse all so the first menu
	 * items are always close to the mouse cursor */
	else {
		if (but->context)
			uiLayoutContextCopy(pup->layout, but->context);
	}

	/* menu is created from a callback */
	pup->menu_func = menu_func;
	pup->menu_arg = arg;

	handle = ui_popup_block_create(C, butregion, but, NULL, ui_block_func_POPOVER, pup);

	if (!but) {
		handle->popup = true;

		UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
		WM_event_add_mousemove(C);
	}

	handle->can_refresh = false;
	MEM_freeN(pup);

	return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Popup Menu API with begin & end
 * \{ */

/**
 * Only return handler, and set optional title.
 * \param block_name: Assigned to uiBlock.name (useful info for debugging).
 */
uiPopover *UI_popover_begin_ex(bContext *C, const char *block_name)
{
	uiStyle *style = UI_style_get_dpi();
	uiPopover *pup = MEM_callocN(sizeof(uiPopover), "popover menu");

	pup->block = UI_block_begin(C, NULL, block_name, UI_EMBOSS);
	pup->layout = UI_block_layout(
	        pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0,
	        U.widget_unit * UI_POPOVER_WIDTH_UNITS, 0, MENU_PADDING, style);

	/* Copied from menus, change if needed. */
	uiLayoutSetOperatorContext(pup->layout, WM_OP_EXEC_REGION_WIN);

	/* create in advance so we can let buttons point to retval already */
	pup->block->handle = MEM_callocN(sizeof(uiPopupBlockHandle), "uiPopupBlockHandle");

	return pup;
}

uiPopover *UI_popover_begin(bContext *C)
{
	return UI_popover_begin_ex(C, __func__);
}

/**
 * Setting the button makes the popover open from the button instead of the cursor.
 */
#if 0
void UI_popover_panel_but_set(uiPopover *pup, struct ARegion *butregion, uiBut *but)
{
	pup->but = but;
	pup->butregion = butregion;
}
#endif

/* set the whole structure to work */
void UI_popover_end(bContext *C, uiPopover *pup)
{
	wmWindow *window = CTX_wm_window(C);
	uiPopupBlockHandle *menu;
	uiBut *but = NULL;
	ARegion *butregion = NULL;

	pup->popover = true;
	pup->mx = window->eventstate->x;
	pup->my = window->eventstate->y;

	if (pup->but) {
		but = pup->but;
		butregion = pup->butregion;
	}

	menu = ui_popup_block_create(C, butregion, but, NULL, ui_block_func_POPOVER, pup);
	menu->popup = true;

	UI_popup_handlers_add(C, &window->modalhandlers, menu, 0);
	WM_event_add_mousemove(C);

	menu->can_refresh = false;
	MEM_freeN(pup);
}

uiLayout *UI_popover_layout(uiPopover *pup)
{
	return pup->layout;
}

/** \} */

/* We may want to support this in future */
/* Similar to UI_popup_menu_invoke */
// int UI_popover_panel_invoke(bContext *C, const char *idname, ReportList *reports);
