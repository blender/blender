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

/** \file blender/editors/interface/interface_region_popup.c
 *  \ingroup edinterface
 *
 * PopUp Region (Generic)
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "ED_screen.h"

#include "interface_intern.h"
#include "interface_regions_intern.h"

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

/**
 * Translate any popup regions (so we can drag them).
 */
void ui_popup_translate(ARegion *ar, const int mdiff[2])
{
	uiBlock *block;

	BLI_rcti_translate(&ar->winrct, UNPACK2(mdiff));

	ED_region_update_rect(ar);

	ED_region_tag_redraw(ar);

	/* update blocks */
	for (block = ar->uiblocks.first; block; block = block->next) {
		uiSafetyRct *saferct;
		for (saferct = block->saferct.first; saferct; saferct = saferct->next) {
			BLI_rctf_translate(&saferct->parent, UNPACK2(mdiff));
			BLI_rctf_translate(&saferct->safety, UNPACK2(mdiff));
		}
	}
}

/* position block relative to but, result is in window space */
static void ui_popup_block_position(wmWindow *window, ARegion *butregion, uiBut *but, uiBlock *block)
{
	uiPopupBlockHandle *handle = block->handle;

	/* Compute button position in window coordinates using the source
	 * button region/block, to position the popup attached to it. */
	rctf butrct;

	if (!handle->refresh) {
		ui_block_to_window_rctf(butregion, but->block, &butrct, &but->rect);

		/* widget_roundbox_set has this correction too, keep in sync */
		if (but->type != UI_BTYPE_PULLDOWN) {
			if (but->drawflag & UI_BUT_ALIGN_TOP)
				butrct.ymax += U.pixelsize;
			if (but->drawflag & UI_BUT_ALIGN_LEFT)
				butrct.xmin -= U.pixelsize;
		}

		handle->prev_butrct = butrct;
	}
	else {
		/* For refreshes, keep same button position so popup doesn't move. */
		butrct = handle->prev_butrct;
	}

	/* Compute block size in window space, based on buttons contained in it. */
	if (block->rect.xmin == 0.0f && block->rect.xmax == 0.0f) {
		if (block->buttons.first) {
			BLI_rctf_init_minmax(&block->rect);

			for (uiBut *bt = block->buttons.first; bt; bt = bt->next) {
				if (block->content_hints & BLOCK_CONTAINS_SUBMENU_BUT) {
					bt->rect.xmax += UI_MENU_SUBMENU_PADDING;
				}
				BLI_rctf_union(&block->rect, &bt->rect);
			}
		}
		else {
			/* we're nice and allow empty blocks too */
			block->rect.xmin = block->rect.ymin = 0;
			block->rect.xmax = block->rect.ymax = 20;
		}
	}

	ui_block_to_window_rctf(butregion, but->block, &block->rect, &block->rect);

	/* Compute direction relative to button, based on available space. */
	const int size_x = BLI_rctf_size_x(&block->rect) + 0.2f * UI_UNIT_X;  /* 4 for shadow */
	const int size_y = BLI_rctf_size_y(&block->rect) + 0.2f * UI_UNIT_Y;
	const int center_x = (block->direction & UI_DIR_CENTER_X) ? size_x / 2 : 0;
	const int center_y = (block->direction & UI_DIR_CENTER_Y) ? size_y / 2 : 0;

	short dir1 = 0, dir2 = 0;

	if (!handle->refresh) {
		bool left = 0, right = 0, top = 0, down = 0;

		const int win_x = WM_window_pixels_x(window);
		const int win_y = WM_window_pixels_y(window);

		/* Take into account maximum size so we don't have to flip on refresh. */
		const float max_size_x = max_ff(size_x, handle->max_size_x);
		const float max_size_y = max_ff(size_y, handle->max_size_y);

		/* check if there's space at all */
		if (butrct.xmin - max_size_x + center_x > 0.0f) left = 1;
		if (butrct.xmax + max_size_x - center_x < win_x) right = 1;
		if (butrct.ymin - max_size_y + center_y > 0.0f) down = 1;
		if (butrct.ymax + max_size_y - center_y < win_y) top = 1;

		if (top == 0 && down == 0) {
			if (butrct.ymin - max_size_y < win_y - butrct.ymax - max_size_y)
				top = 1;
			else
				down = 1;
		}

		dir1 = (block->direction & UI_DIR_ALL);

		/* Secondary directions. */
		if (dir1 & (UI_DIR_UP | UI_DIR_DOWN)) {
			if      (dir1 & UI_DIR_LEFT)  dir2 = UI_DIR_LEFT;
			else if (dir1 & UI_DIR_RIGHT) dir2 = UI_DIR_RIGHT;
			dir1 &= (UI_DIR_UP | UI_DIR_DOWN);
		}

		if ((dir2 == 0) && (dir1 == UI_DIR_LEFT || dir1 == UI_DIR_RIGHT)) dir2 = UI_DIR_DOWN;
		if ((dir2 == 0) && (dir1 == UI_DIR_UP   || dir1 == UI_DIR_DOWN))  dir2 = UI_DIR_LEFT;

		/* no space at all? don't change */
		if (left || right) {
			if (dir1 == UI_DIR_LEFT  && left == 0)  dir1 = UI_DIR_RIGHT;
			if (dir1 == UI_DIR_RIGHT && right == 0) dir1 = UI_DIR_LEFT;
			/* this is aligning, not append! */
			if (dir2 == UI_DIR_LEFT  && right == 0) dir2 = UI_DIR_RIGHT;
			if (dir2 == UI_DIR_RIGHT && left == 0)  dir2 = UI_DIR_LEFT;
		}
		if (down || top) {
			if (dir1 == UI_DIR_UP   && top == 0)  dir1 = UI_DIR_DOWN;
			if (dir1 == UI_DIR_DOWN && down == 0) dir1 = UI_DIR_UP;
			BLI_assert(dir2 != UI_DIR_UP);
//			if (dir2 == UI_DIR_UP   && top == 0)  dir2 = UI_DIR_DOWN;
			if (dir2 == UI_DIR_DOWN && down == 0) dir2 = UI_DIR_UP;
		}

		handle->prev_dir1 = dir1;
		handle->prev_dir2 = dir2;
	}
	else {
		/* For refreshes, keep same popup direct so popup doesn't move
		 * to a totally different position while editing in it. */
		dir1 = handle->prev_dir1;
		dir2 = handle->prev_dir2;
	}

	/* Compute offset based on direction. */
	int offset_x = 0, offset_y = 0;

	if (dir1 == UI_DIR_LEFT) {
		offset_x = butrct.xmin - block->rect.xmax;
		if (dir2 == UI_DIR_UP) offset_y = butrct.ymin - block->rect.ymin - center_y - MENU_PADDING;
		else                   offset_y = butrct.ymax - block->rect.ymax + center_y + MENU_PADDING;
	}
	else if (dir1 == UI_DIR_RIGHT) {
		offset_x = butrct.xmax - block->rect.xmin;
		if (dir2 == UI_DIR_UP) offset_y = butrct.ymin - block->rect.ymin - center_y - MENU_PADDING;
		else                   offset_y = butrct.ymax - block->rect.ymax + center_y + MENU_PADDING;
	}
	else if (dir1 == UI_DIR_UP) {
		offset_y = butrct.ymax - block->rect.ymin;
		if (dir2 == UI_DIR_RIGHT) offset_x = butrct.xmax - block->rect.xmax + center_x;
		else                      offset_x = butrct.xmin - block->rect.xmin - center_x;
		/* changed direction? */
		if ((dir1 & block->direction) == 0) {
			/* TODO: still do */
			UI_block_order_flip(block);
		}
	}
	else if (dir1 == UI_DIR_DOWN) {
		offset_y = butrct.ymin - block->rect.ymax;
		if (dir2 == UI_DIR_RIGHT) offset_x = butrct.xmax - block->rect.xmax + center_x;
		else                      offset_x = butrct.xmin - block->rect.xmin - center_x;
		/* changed direction? */
		if ((dir1 & block->direction) == 0) {
			/* TODO: still do */
			UI_block_order_flip(block);
		}
	}

	/* Center over popovers for eg. */
	if (block->direction & UI_DIR_CENTER_X) {
		offset_x += BLI_rctf_size_x(&butrct) / ((dir2 == UI_DIR_LEFT) ? 2 : - 2);
	}

	/* Apply offset, buttons in window coords. */
	for (uiBut *bt = block->buttons.first; bt; bt = bt->next) {
		ui_block_to_window_rctf(butregion, but->block, &bt->rect, &bt->rect);

		BLI_rctf_translate(&bt->rect, offset_x, offset_y);

		/* ui_but_update recalculates drawstring size in pixels */
		ui_but_update(bt);
	}

	BLI_rctf_translate(&block->rect, offset_x, offset_y);

	/* Safety calculus. */
	{
		const float midx = BLI_rctf_cent_x(&butrct);
		const float midy = BLI_rctf_cent_y(&butrct);

		/* when you are outside parent button, safety there should be smaller */

		/* parent button to left */
		if (midx < block->rect.xmin) block->safety.xmin = block->rect.xmin - 3;
		else block->safety.xmin = block->rect.xmin - 40;
		/* parent button to right */
		if (midx > block->rect.xmax) block->safety.xmax = block->rect.xmax + 3;
		else block->safety.xmax = block->rect.xmax + 40;

		/* parent button on bottom */
		if (midy < block->rect.ymin) block->safety.ymin = block->rect.ymin - 3;
		else block->safety.ymin = block->rect.ymin - 40;
		/* parent button on top */
		if (midy > block->rect.ymax) block->safety.ymax = block->rect.ymax + 3;
		else block->safety.ymax = block->rect.ymax + 40;

		/* exception for switched pulldowns... */
		if (dir1 && (dir1 & block->direction) == 0) {
			if (dir2 == UI_DIR_RIGHT) block->safety.xmax = block->rect.xmax + 3;
			if (dir2 == UI_DIR_LEFT)  block->safety.xmin = block->rect.xmin - 3;
		}
		block->direction = dir1;
	}

	/* keep a list of these, needed for pulldown menus */
	uiSafetyRct *saferct = MEM_callocN(sizeof(uiSafetyRct), "uiSafetyRct");
	saferct->parent = butrct;
	saferct->safety = block->safety;
	BLI_freelistN(&block->saferct);
	BLI_duplicatelist(&block->saferct, &but->block->saferct);
	BLI_addhead(&block->saferct, saferct);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu Block Creation
 * \{ */

static void ui_block_region_refresh(const bContext *C, ARegion *ar)
{
	ScrArea *ctx_area = CTX_wm_area(C);
	ARegion *ctx_region = CTX_wm_region(C);
	uiBlock *block;

	if (ar->do_draw & RGN_DRAW_REFRESH_UI) {
		ScrArea *handle_ctx_area;
		ARegion *handle_ctx_region;
		uiBlock *block_next;

		ar->do_draw &= ~RGN_DRAW_REFRESH_UI;
		for (block = ar->uiblocks.first; block; block = block_next) {
			block_next = block->next;
			uiPopupBlockHandle *handle = block->handle;

			if (handle->can_refresh) {
				handle_ctx_area = handle->ctx_area;
				handle_ctx_region = handle->ctx_region;

				if (handle_ctx_area) {
					CTX_wm_area_set((bContext *)C, handle_ctx_area);
				}
				if (handle_ctx_region) {
					CTX_wm_region_set((bContext *)C, handle_ctx_region);
				}

				uiBut *but = handle->popup_create_vars.but;
				ARegion *butregion = handle->popup_create_vars.butregion;
				ui_popup_block_refresh((bContext *)C, handle, butregion, but);
			}
		}
	}

	CTX_wm_area_set((bContext *)C, ctx_area);
	CTX_wm_region_set((bContext *)C, ctx_region);
}

static void ui_block_region_draw(const bContext *C, ARegion *ar)
{
	uiBlock *block;

	for (block = ar->uiblocks.first; block; block = block->next)
		UI_block_draw(C, block);
}

/**
 * Use to refresh centered popups on screen resizing (for splash).
 */
static void ui_block_region_popup_window_listener(
        wmWindow *UNUSED(win), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn, const Scene *UNUSED(scene))
{
	switch (wmn->category) {
		case NC_WINDOW:
		{
			switch (wmn->action) {
				case NA_EDITED:
				{
					/* window resize */
					ED_region_tag_refresh_ui(ar);
					break;
				}
			}
			break;
		}
	}
}

static void ui_popup_block_clip(wmWindow *window, uiBlock *block)
{
	uiBut *bt;
	float xofs = 0.0f;
	int width = UI_SCREEN_MARGIN;
	int winx, winy;

	if (block->flag & UI_BLOCK_NO_WIN_CLIP) {
		return;
	}

	winx = WM_window_pixels_x(window);
	winy = WM_window_pixels_y(window);

	/* shift menus to right if outside of view */
	if (block->rect.xmin < width) {
		xofs = (width - block->rect.xmin);
		block->rect.xmin += xofs;
		block->rect.xmax += xofs;
	}
	/* or shift to left if outside of view */
	if (block->rect.xmax > winx - width) {
		xofs = winx - width - block->rect.xmax;
		block->rect.xmin += xofs;
		block->rect.xmax += xofs;
	}

	if (block->rect.ymin < width)
		block->rect.ymin = width;
	if (block->rect.ymax > winy - UI_POPUP_MENU_TOP)
		block->rect.ymax = winy - UI_POPUP_MENU_TOP;

	/* ensure menu items draw inside left/right boundary */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		bt->rect.xmin += xofs;
		bt->rect.xmax += xofs;
	}

}

void ui_popup_block_scrolltest(uiBlock *block)
{
	uiBut *bt;

	block->flag &= ~(UI_BLOCK_CLIPBOTTOM | UI_BLOCK_CLIPTOP);

	for (bt = block->buttons.first; bt; bt = bt->next)
		bt->flag &= ~UI_SCROLLED;

	if (block->buttons.first == block->buttons.last)
		return;

	/* mark buttons that are outside boundary */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		if (bt->rect.ymin < block->rect.ymin) {
			bt->flag |= UI_SCROLLED;
			block->flag |= UI_BLOCK_CLIPBOTTOM;
		}
		if (bt->rect.ymax > block->rect.ymax) {
			bt->flag |= UI_SCROLLED;
			block->flag |= UI_BLOCK_CLIPTOP;
		}
	}

	/* mark buttons overlapping arrows, if we have them */
	for (bt = block->buttons.first; bt; bt = bt->next) {
		if (block->flag & UI_BLOCK_CLIPBOTTOM) {
			if (bt->rect.ymin < block->rect.ymin + UI_MENU_SCROLL_ARROW)
				bt->flag |= UI_SCROLLED;
		}
		if (block->flag & UI_BLOCK_CLIPTOP) {
			if (bt->rect.ymax > block->rect.ymax - UI_MENU_SCROLL_ARROW)
				bt->flag |= UI_SCROLLED;
		}
	}
}

static void ui_popup_block_remove(bContext *C, uiPopupBlockHandle *handle)
{
	wmWindow *win = CTX_wm_window(C);
	bScreen *sc = CTX_wm_screen(C);

	ui_region_temp_remove(C, sc, handle->region);

	/* reset to region cursor (only if there's not another menu open) */
	if (BLI_listbase_is_empty(&sc->regionbase)) {
		ED_region_cursor_set(win, CTX_wm_area(C), CTX_wm_region(C));
		/* in case cursor needs to be changed again */
		WM_event_add_mousemove(C);
	}

	if (handle->scrolltimer)
		WM_event_remove_timer(CTX_wm_manager(C), win, handle->scrolltimer);
}

/**
 * Called for creating new popups and refreshing existing ones.
 */
uiBlock *ui_popup_block_refresh(
        bContext *C, uiPopupBlockHandle *handle,
        ARegion *butregion, uiBut *but)
{
	const int margin = UI_POPUP_MARGIN;
	wmWindow *window = CTX_wm_window(C);
	ARegion *ar = handle->region;

	uiBlockCreateFunc create_func = handle->popup_create_vars.create_func;
	uiBlockHandleCreateFunc handle_create_func = handle->popup_create_vars.handle_create_func;
	void *arg = handle->popup_create_vars.arg;

	uiBlock *block_old = ar->uiblocks.first;
	uiBlock *block;

	handle->refresh = (block_old != NULL);

	BLI_assert(!handle->refresh || handle->can_refresh);

#ifdef DEBUG
	wmEvent *event_back = window->eventstate;
#endif

	/* create ui block */
	if (create_func)
		block = create_func(C, ar, arg);
	else
		block = handle_create_func(C, handle, arg);

	/* callbacks _must_ leave this for us, otherwise we can't call UI_block_update_from_old */
	BLI_assert(!block->endblock);

	/* ensure we don't use mouse coords here! */
#ifdef DEBUG
	window->eventstate = NULL;
#endif

	if (block->handle) {
		memcpy(block->handle, handle, sizeof(uiPopupBlockHandle));
		MEM_freeN(handle);
		handle = block->handle;
	}
	else
		block->handle = handle;

	ar->regiondata = handle;

	/* set UI_BLOCK_NUMSELECT before UI_block_end() so we get alphanumeric keys assigned */
	if (but == NULL) {
		block->flag |= UI_BLOCK_POPUP;
	}

	block->flag |= UI_BLOCK_LOOP;

	/* defer this until blocks are translated (below) */
	block->oldblock = NULL;

	if (!block->endblock) {
		UI_block_end_ex(C, block, handle->popup_create_vars.event_xy, handle->popup_create_vars.event_xy);
	}

	/* if this is being created from a button */
	if (but) {
		block->aspect = but->block->aspect;
		ui_popup_block_position(window, butregion, but, block);
		handle->direction = block->direction;
	}
	else {
		uiSafetyRct *saferct;
		/* keep a list of these, needed for pulldown menus */
		saferct = MEM_callocN(sizeof(uiSafetyRct), "uiSafetyRct");
		saferct->safety = block->safety;
		BLI_addhead(&block->saferct, saferct);
	}

	if (block->flag & UI_BLOCK_RADIAL) {
		int win_width = UI_SCREEN_MARGIN;
		int winx, winy;

		int x_offset = 0, y_offset = 0;

		winx = WM_window_pixels_x(window);
		winy = WM_window_pixels_y(window);

		copy_v2_v2(block->pie_data.pie_center_init, block->pie_data.pie_center_spawned);

		/* only try translation if area is large enough */
		if (BLI_rctf_size_x(&block->rect) < winx - (2.0f * win_width)) {
			if (block->rect.xmin < win_width )   x_offset += win_width - block->rect.xmin;
			if (block->rect.xmax > winx - win_width) x_offset += winx - win_width - block->rect.xmax;
		}

		if (BLI_rctf_size_y(&block->rect) < winy - (2.0f * win_width)) {
			if (block->rect.ymin < win_width )   y_offset += win_width - block->rect.ymin;
			if (block->rect.ymax > winy - win_width) y_offset += winy - win_width - block->rect.ymax;
		}
		/* if we are offsetting set up initial data for timeout functionality */

		if ((x_offset != 0) || (y_offset != 0)) {
			block->pie_data.pie_center_spawned[0] += x_offset;
			block->pie_data.pie_center_spawned[1] += y_offset;

			UI_block_translate(block, x_offset, y_offset);

			if (U.pie_initial_timeout > 0)
				block->pie_data.flags |= UI_PIE_INITIAL_DIRECTION;
		}

		ar->winrct.xmin = 0;
		ar->winrct.xmax = winx;
		ar->winrct.ymin = 0;
		ar->winrct.ymax = winy;

		ui_block_calc_pie_segment(block, block->pie_data.pie_center_init);

		/* lastly set the buttons at the center of the pie menu, ready for animation */
		if (U.pie_animation_timeout > 0) {
			for (uiBut *but_iter = block->buttons.first; but_iter; but_iter = but_iter->next) {
				if (but_iter->pie_dir != UI_RADIAL_NONE) {
					BLI_rctf_recenter(&but_iter->rect, UNPACK2(block->pie_data.pie_center_spawned));
				}
			}
		}
	}
	else {
		/* clip block with window boundary */
		ui_popup_block_clip(window, block);

		/* Avoid menu moving down and losing cursor focus by keeping it at
		 * the same height. */
		if (handle->refresh && handle->prev_block_rect.ymax > block->rect.ymax) {
			float offset = handle->prev_block_rect.ymax - block->rect.ymax;
			UI_block_translate(block, 0, offset);
			block->rect.ymin = handle->prev_block_rect.ymin;
		}

		handle->prev_block_rect = block->rect;

		/* the block and buttons were positioned in window space as in 2.4x, now
		 * these menu blocks are regions so we bring it back to region space.
		 * additionally we add some padding for the menu shadow or rounded menus */
		ar->winrct.xmin = block->rect.xmin - margin;
		ar->winrct.xmax = block->rect.xmax + margin;
		ar->winrct.ymin = block->rect.ymin - margin;
		ar->winrct.ymax = block->rect.ymax + UI_POPUP_MENU_TOP;

		UI_block_translate(block, -ar->winrct.xmin, -ar->winrct.ymin);

		/* apply scroll offset */
		if (handle->scrolloffset != 0.0f) {
			for (uiBut *bt = block->buttons.first; bt; bt = bt->next) {
				bt->rect.ymin += handle->scrolloffset;
				bt->rect.ymax += handle->scrolloffset;
			}
		}
	}

	if (block_old) {
		block->oldblock = block_old;
		UI_block_update_from_old(C, block);
		UI_blocklist_free_inactive(C, &ar->uiblocks);
	}

	/* checks which buttons are visible, sets flags to prevent draw (do after region init) */
	ui_popup_block_scrolltest(block);

	/* adds subwindow */
	ED_region_init(ar);

	/* get winmat now that we actually have the subwindow */
	wmGetProjectionMatrix(block->winmat, &ar->winrct);

	/* notify change and redraw */
	ED_region_tag_redraw(ar);

	ED_region_update_rect(ar);

#ifdef DEBUG
	window->eventstate = event_back;
#endif

	return block;
}

uiPopupBlockHandle *ui_popup_block_create(
        bContext *C, ARegion *butregion, uiBut *but,
        uiBlockCreateFunc create_func, uiBlockHandleCreateFunc handle_create_func,
        void *arg)
{
	wmWindow *window = CTX_wm_window(C);
	uiBut *activebut = UI_context_active_but_get(C);
	static ARegionType type;
	ARegion *ar;
	uiBlock *block;
	uiPopupBlockHandle *handle;

	/* disable tooltips from buttons below */
	if (activebut) {
		UI_but_tooltip_timer_remove(C, activebut);
	}
	/* standard cursor by default */
	WM_cursor_set(window, CURSOR_STD);

	/* create handle */
	handle = MEM_callocN(sizeof(uiPopupBlockHandle), "uiPopupBlockHandle");

	/* store context for operator */
	handle->ctx_area = CTX_wm_area(C);
	handle->ctx_region = CTX_wm_region(C);

	/* store vars to refresh popup (RGN_DRAW_REFRESH_UI) */
	handle->popup_create_vars.create_func = create_func;
	handle->popup_create_vars.handle_create_func = handle_create_func;
	handle->popup_create_vars.arg = arg;
	handle->popup_create_vars.but = but;
	handle->popup_create_vars.butregion = but ? butregion : NULL;
	copy_v2_v2_int(handle->popup_create_vars.event_xy, &window->eventstate->x);

	/* don't allow by default, only if popup type explicitly supports it */
	handle->can_refresh = false;

	/* create area region */
	ar = ui_region_temp_add(CTX_wm_screen(C));
	handle->region = ar;

	memset(&type, 0, sizeof(ARegionType));
	type.draw = ui_block_region_draw;
	type.layout = ui_block_region_refresh;
	type.regionid = RGN_TYPE_TEMPORARY;
	ar->type = &type;

	UI_region_handlers_add(&ar->handlers);

	block = ui_popup_block_refresh(C, handle, butregion, but);
	handle = block->handle;

	/* keep centered on window resizing */
	if (block->bounds_type == UI_BLOCK_BOUNDS_POPUP_CENTER) {
		type.listener = ui_block_region_popup_window_listener;
	}

	return handle;
}

void ui_popup_block_free(bContext *C, uiPopupBlockHandle *handle)
{
	/* If this popup is created from a popover which does NOT have keep-open flag set,
	 * then close the popover too. We could extend this to other popup types too. */
	ARegion *ar = handle->popup_create_vars.butregion;
	if (ar != NULL) {
		for (uiBlock *block = ar->uiblocks.first; block; block = block->next) {
			if (block->handle &&
			    (block->flag & UI_BLOCK_POPOVER) &&
			    (block->flag & UI_BLOCK_KEEP_OPEN) == 0)
			{
				uiPopupBlockHandle *menu = block->handle;
				menu->menuretval = UI_RETURN_OK;
			}
		}
	}

	if (handle->popup_create_vars.free_func) {
		handle->popup_create_vars.free_func(handle, handle->popup_create_vars.arg);
	}

	ui_popup_block_remove(C, handle);

	MEM_freeN(handle);
}

/** \} */
