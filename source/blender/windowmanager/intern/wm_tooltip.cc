/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Manages a per-window tool-tip.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"

#include "WM_api.hh"
#include "WM_types.hh"

static double g_tooltip_time_closed;
double WM_tooltip_time_closed()
{
  return g_tooltip_time_closed;
}

void WM_tooltip_immediate_init(
    bContext *C, wmWindow *win, ScrArea *area, ARegion *region, wmTooltipInitFn init)
{
  WM_tooltip_timer_clear(C, win);

  bScreen *screen = WM_window_get_active_screen(win);
  if (screen->tool_tip == nullptr) {
    screen->tool_tip = static_cast<wmTooltipState *>(
        MEM_callocN(sizeof(*screen->tool_tip), __func__));
  }
  screen->tool_tip->area_from = area;
  screen->tool_tip->region_from = region;
  screen->tool_tip->init = init;
  WM_tooltip_init(C, win);
}

void WM_tooltip_timer_init_ex(
    bContext *C, wmWindow *win, ScrArea *area, ARegion *region, wmTooltipInitFn init, double delay)
{
  WM_tooltip_timer_clear(C, win);

  bScreen *screen = WM_window_get_active_screen(win);
  wmWindowManager *wm = CTX_wm_manager(C);
  if (screen->tool_tip == nullptr) {
    screen->tool_tip = static_cast<wmTooltipState *>(
        MEM_callocN(sizeof(*screen->tool_tip), __func__));
  }
  screen->tool_tip->area_from = area;
  screen->tool_tip->region_from = region;
  screen->tool_tip->timer = WM_event_timer_add(wm, win, TIMER, delay);
  screen->tool_tip->init = init;
}

void WM_tooltip_timer_init(
    bContext *C, wmWindow *win, ScrArea *area, ARegion *region, wmTooltipInitFn init)
{
  WM_tooltip_timer_init_ex(C, win, area, region, init, UI_TOOLTIP_DELAY);
}

void WM_tooltip_timer_clear(bContext *C, wmWindow *win)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  bScreen *screen = WM_window_get_active_screen(win);
  if (screen->tool_tip != nullptr) {
    if (screen->tool_tip->timer != nullptr) {
      WM_event_timer_remove(wm, win, screen->tool_tip->timer);
      screen->tool_tip->timer = nullptr;
    }
  }
}

void WM_tooltip_clear(bContext *C, wmWindow *win)
{
  WM_tooltip_timer_clear(C, win);
  bScreen *screen = WM_window_get_active_screen(win);
  if (screen->tool_tip != nullptr) {
    if (screen->tool_tip->region) {
      UI_tooltip_free(C, screen, screen->tool_tip->region);
      screen->tool_tip->region = nullptr;
      g_tooltip_time_closed = BLI_check_seconds_timer();
    }
    MEM_freeN(screen->tool_tip);
    screen->tool_tip = nullptr;
  }
}

void WM_tooltip_init(bContext *C, wmWindow *win)
{
  WM_tooltip_timer_clear(C, win);
  bScreen *screen = WM_window_get_active_screen(win);
  if (screen->tool_tip->region) {
    UI_tooltip_free(C, screen, screen->tool_tip->region);
    screen->tool_tip->region = nullptr;
  }
  const int pass_prev = screen->tool_tip->pass;
  double pass_delay = 0.0;

  {
    ScrArea *area_prev = CTX_wm_area(C);
    ARegion *region_prev = CTX_wm_region(C);
    CTX_wm_area_set(C, screen->tool_tip->area_from);
    CTX_wm_region_set(C, screen->tool_tip->region_from);
    screen->tool_tip->region = screen->tool_tip->init(C,
                                                      screen->tool_tip->region_from,
                                                      &screen->tool_tip->pass,
                                                      &pass_delay,
                                                      &screen->tool_tip->exit_on_event);
    CTX_wm_area_set(C, area_prev);
    CTX_wm_region_set(C, region_prev);
  }

  copy_v2_v2_int(screen->tool_tip->event_xy, win->eventstate->xy);
  if (pass_prev != screen->tool_tip->pass) {
    /* The pass changed, add timer for next pass. */
    wmWindowManager *wm = CTX_wm_manager(C);
    screen->tool_tip->timer = WM_event_timer_add(wm, win, TIMER, pass_delay);
  }
  if (screen->tool_tip->region == nullptr) {
    WM_tooltip_clear(C, win);
  }
}

void WM_tooltip_refresh(bContext *C, wmWindow *win)
{
  WM_tooltip_timer_clear(C, win);
  bScreen *screen = WM_window_get_active_screen(win);
  if (screen->tool_tip != nullptr) {
    if (screen->tool_tip->region) {
      UI_tooltip_free(C, screen, screen->tool_tip->region);
      screen->tool_tip->region = nullptr;
    }
    WM_tooltip_init(C, win);
  }
}
