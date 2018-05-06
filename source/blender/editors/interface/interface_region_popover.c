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

#include "ED_screen.h"

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

	uiMenuCreateFunc menu_func;
	void *menu_arg;
};

static void ui_popover_create_block(bContext *C, uiPopover *pup, int opcontext)
{
	uiStyle *style = UI_style_get_dpi();

	pup->block = UI_block_begin(C, NULL, __func__, UI_EMBOSS);
	pup->layout = UI_block_layout(
	        pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0,
	        U.widget_unit * UI_POPOVER_WIDTH_UNITS, 0, MENU_PADDING, style);

	uiLayoutSetOperatorContext(pup->layout, opcontext);

	if (pup->but) {
		if (pup->but->context) {
			uiLayoutContextCopy(pup->layout, pup->but->context);
		}
	}

	pup->block->flag |= UI_BLOCK_NO_FLIP;
}

static uiBlock *ui_block_func_POPOVER(bContext *C, uiPopupBlockHandle *handle, void *arg_pup)
{
	uiPopover *pup = arg_pup;

	/* Create UI block and layout now if it wasn't done between begin/end. */
	if (!pup->layout) {
		ui_popover_create_block(C, pup, WM_OP_INVOKE_REGION_WIN);

		if (pup->menu_func) {
			pup->block->handle = handle;
			pup->menu_func(C, pup->layout, pup->menu_arg);
			pup->block->handle = NULL;
		}

		pup->layout = NULL;
	}

	/* Setup and resolve UI layout for block. */
	uiBlock *block = pup->block;
	int width, height;

	UI_block_region_set(block, handle->region);
	UI_block_layout_resolve(block, &width, &height);
	UI_block_flag_enable(block, UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_KEEP_OPEN | UI_BLOCK_POPOVER);
	UI_block_direction_set(block, UI_DIR_DOWN | UI_DIR_CENTER_X);

	const int block_margin = U.widget_unit / 2;

	if (pup->but) {
		/* For a header menu we set the direction automatic. */
		block->minbounds = BLI_rctf_size_x(&pup->but->rect);
		UI_block_bounds_set_normal(block, block_margin);

		/* If menu slides out of other menu, override direction. */
		bool slideout = false; //ui_block_is_menu(pup->but->block);
		if (slideout)
			UI_block_direction_set(block, UI_DIR_RIGHT);

		/* Store the button location for positioning the popover arrow hint. */
		if (!handle->refresh) {
			float center[2] = {BLI_rctf_cent_x(&pup->but->rect), BLI_rctf_cent_y(&pup->but->rect)};
			ui_block_to_window_fl(handle->ctx_region, pup->but->block, &center[0], &center[1]);
			/* These variables aren't used for popovers, we could add new variables if there is a conflict. */
			handle->prev_mx = block->mx = (int)center[0];
			handle->prev_my = block->my = (int)center[1];
		}
		else {
			block->mx = handle->prev_mx;
			block->my = handle->prev_my;
		}

		/* Prefer popover from header to be positioned into the editor. */
		if (!slideout) {
			ScrArea *sa = CTX_wm_area(C);
			if (sa && ED_area_header_alignment(sa) == RGN_ALIGN_BOTTOM) {
				ARegion *ar = CTX_wm_region(C);
				if (ar && ar->regiontype == RGN_TYPE_HEADER) {
					UI_block_direction_set(block, UI_DIR_UP | UI_DIR_CENTER_X);
				}
			}
		}

		/* Estimated a maximum size so we don't go offscreen for low height
		 * areas near the bottom of the window on refreshes. */
		handle->max_size_y = UI_UNIT_Y * 16.0f;
	}
	else {
		/* Not attached to a button. */
		int offset[2] = {0, 0};  /* Dummy. */
		UI_block_flag_enable(block, UI_BLOCK_LOOP);
		UI_block_direction_set(block, block->direction);
		block->minbounds = UI_MENU_WIDTH_MIN;
		UI_block_bounds_set_popup(block, block_margin, offset[0], offset[1]);
	}

	return block;
}

static void ui_block_free_func_POPOVER(uiPopupBlockHandle *UNUSED(handle), void *arg_pup)
{
	uiPopover *pup = arg_pup;
	MEM_freeN(pup);
}

uiPopupBlockHandle *ui_popover_panel_create(
        bContext *C, ARegion *butregion, uiBut *but,
        uiMenuCreateFunc menu_func, void *arg)
{
	/* Create popover, buttons are created from callback. */
	uiPopover *pup = MEM_callocN(sizeof(uiPopover), __func__);
	pup->but = but;
	pup->menu_func = menu_func;
	pup->menu_arg = arg;

	/* Create popup block. */
	uiPopupBlockHandle *handle;
	handle = ui_popup_block_create(C, butregion, but, NULL, ui_block_func_POPOVER, pup);
	handle->popup_create_vars.free_func = ui_block_free_func_POPOVER;
	handle->can_refresh = true;

	/* Add handlers. If attached to a button, the button will already
	 * add a modal handler and pass on events. */
	if (!but) {
		wmWindow *window = CTX_wm_window(C);
		UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
		WM_event_add_mousemove(C);
		handle->popup = true;
	}

	return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Popup Menu API with begin & end
 * \{ */

/**
 * Only return handler, and set optional title.
 */
uiPopover *UI_popover_begin(bContext *C)
{
	uiPopover *pup = MEM_callocN(sizeof(uiPopover), "popover menu");

	/* Opertor context default same as menus, change if needed. */
	ui_popover_create_block(C, pup, WM_OP_EXEC_REGION_WIN);

	/* create in advance so we can let buttons point to retval already */
	pup->block->handle = MEM_callocN(sizeof(uiPopupBlockHandle), "uiPopupBlockHandle");

	return pup;
}

/* set the whole structure to work */
void UI_popover_end(bContext *C, uiPopover *pup)
{
	/* Create popup block. No refresh support since the buttons were created
	 * between begin/end and we have no callback to recreate them. */
	uiPopupBlockHandle *handle;

	handle = ui_popup_block_create(C, NULL, NULL, NULL, ui_block_func_POPOVER, pup);
	handle->popup_create_vars.free_func = ui_block_free_func_POPOVER;

	/* Add handlers. */
	wmWindow *window = CTX_wm_window(C);
	UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
	WM_event_add_mousemove(C);
	handle->popup = true;
}

uiLayout *UI_popover_layout(uiPopover *pup)
{
	return pup->layout;
}

/** \} */

/* We may want to support this in future */
/* Similar to UI_popup_menu_invoke */
// int UI_popover_panel_invoke(bContext *C, const char *idname, ReportList *reports);
