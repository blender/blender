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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_tooltip.c
 *  \ingroup wm
 *
 * Manages a per-window tool-tip.
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "ED_screen.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

void WM_tooltip_immediate_init(
        bContext *C, wmWindow *win, ARegion *ar,
        wmTooltipInitFn init)
{
	WM_tooltip_timer_clear(C, win);

	bScreen *screen = WM_window_get_active_screen(win);
	if (screen->tool_tip == NULL) {
		screen->tool_tip = MEM_callocN(sizeof(*screen->tool_tip), __func__);
	}
	screen->tool_tip->region_from = ar;
	screen->tool_tip->init = init;
	WM_tooltip_init(C, win);
}

void WM_tooltip_timer_init(
        bContext *C, wmWindow *win, ARegion *ar,
        wmTooltipInitFn init)
{
	WM_tooltip_timer_clear(C, win);

	bScreen *screen = WM_window_get_active_screen(win);
	wmWindowManager *wm = CTX_wm_manager(C);
	if (screen->tool_tip == NULL) {
		screen->tool_tip = MEM_callocN(sizeof(*screen->tool_tip), __func__);
	}
	screen->tool_tip->region_from = ar;
	screen->tool_tip->timer = WM_event_add_timer(
	        wm, win, TIMER, UI_TOOLTIP_DELAY);
	screen->tool_tip->init = init;
}

void WM_tooltip_timer_clear(bContext *C, wmWindow *win)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	bScreen *screen = WM_window_get_active_screen(win);
	if (screen->tool_tip != NULL) {
		if (screen->tool_tip->timer != NULL) {
			WM_event_remove_timer(wm, win, screen->tool_tip->timer);
			screen->tool_tip->timer = NULL;
		}
	}
}

void WM_tooltip_clear(bContext *C, wmWindow *win)
{
	WM_tooltip_timer_clear(C, win);
	bScreen *screen = WM_window_get_active_screen(win);
	if (screen->tool_tip != NULL) {
		if (screen->tool_tip->region) {
			UI_tooltip_free(C, screen, screen->tool_tip->region);
			screen->tool_tip->region = NULL;
		}
		MEM_freeN(screen->tool_tip);
		screen->tool_tip = NULL;
	}
}

void WM_tooltip_init(bContext *C, wmWindow *win)
{
	WM_tooltip_timer_clear(C, win);
	bScreen *screen = WM_window_get_active_screen(win);
	if (screen->tool_tip->region) {
		UI_tooltip_free(C, screen, screen->tool_tip->region);
		screen->tool_tip->region = NULL;
	}
	screen->tool_tip->region = screen->tool_tip->init(
	        C, screen->tool_tip->region_from, &screen->tool_tip->exit_on_event);
	if (screen->tool_tip->region == NULL) {
		WM_tooltip_clear(C, win);
	}
}

void WM_tooltip_refresh(bContext *C, wmWindow *win)
{
	WM_tooltip_timer_clear(C, win);
	bScreen *screen = WM_window_get_active_screen(win);
	if (screen->tool_tip != NULL) {
		if (screen->tool_tip->region) {
			UI_tooltip_free(C, screen, screen->tool_tip->region);
			screen->tool_tip->region = NULL;
		}
		WM_tooltip_init(C, win);
	}
}
