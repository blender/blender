/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * PopUp Region (Generic)
 */

#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "ED_screen.hh"

#include "interface_intern.hh"
#include "interface_regions_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

void ui_popup_translate(ARegion *region, const int mdiff[2])
{
  BLI_rcti_translate(&region->winrct, UNPACK2(mdiff));

  ED_region_update_rect(region);

  ED_region_tag_redraw(region);

  /* update blocks */
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    uiPopupBlockHandle *handle = block->handle;
    /* Make empty, will be initialized on next use, see #60608. */
    BLI_rctf_init(&handle->prev_block_rect, 0, 0, 0, 0);

    LISTBASE_FOREACH (uiSafetyRct *, saferct, &block->saferct) {
      BLI_rctf_translate(&saferct->parent, UNPACK2(mdiff));
      BLI_rctf_translate(&saferct->safety, UNPACK2(mdiff));
    }
  }
}

/* position block relative to but, result is in window space */
static void ui_popup_block_position(wmWindow *window,
                                    ARegion *butregion,
                                    uiBut *but,
                                    uiBlock *block)
{
  uiPopupBlockHandle *handle = block->handle;

  /* Compute button position in window coordinates using the source
   * button region/block, to position the popup attached to it. */
  rctf butrct;
  if (!handle->refresh) {
    ui_block_to_window_rctf(butregion, but->block, &butrct, &but->rect);

    /* widget_roundbox_set has this correction too, keep in sync */
    if (but->type != UI_BTYPE_PULLDOWN) {
      if (but->drawflag & UI_BUT_ALIGN_TOP) {
        butrct.ymax += U.pixelsize;
      }
      if (but->drawflag & UI_BUT_ALIGN_LEFT) {
        butrct.xmin -= U.pixelsize;
      }
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

      LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
        if (block->content_hints & UI_BLOCK_CONTAINS_SUBMENU_BUT) {
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
  const int size_x = BLI_rctf_size_x(&block->rect) + 0.2f * UI_UNIT_X; /* 4 for shadow */
  const int size_y = BLI_rctf_size_y(&block->rect) + 0.2f * UI_UNIT_Y;
  const int center_x = (block->direction & UI_DIR_CENTER_X) ? size_x / 2 : 0;
  const int center_y = (block->direction & UI_DIR_CENTER_Y) ? size_y / 2 : 0;

  const int win_x = WM_window_pixels_x(window);
  const int win_y = WM_window_pixels_y(window);

  /* Take into account maximum size so we don't have to flip on refresh. */
  const float max_size_x = max_ff(size_x, handle->max_size_x);
  const float max_size_y = max_ff(size_y, handle->max_size_y);

  short dir1 = 0, dir2 = 0;

  if (!handle->refresh) {
    bool left = false, right = false, top = false, down = false;

    /* check if there's space at all */
    if (butrct.xmin - max_size_x + center_x > 0.0f) {
      left = true;
    }
    if (butrct.xmax + max_size_x - center_x < win_x) {
      right = true;
    }
    if (butrct.ymin - max_size_y + center_y > 0.0f) {
      down = true;
    }
    if (butrct.ymax + max_size_y - center_y < win_y) {
      top = true;
    }

    if (top == 0 && down == 0) {
      if (butrct.ymin - max_size_y < win_y - butrct.ymax - max_size_y) {
        top = true;
      }
      else {
        down = true;
      }
    }

    dir1 = (block->direction & UI_DIR_ALL);

    /* Secondary directions. */
    if (dir1 & (UI_DIR_UP | UI_DIR_DOWN)) {
      if (dir1 & UI_DIR_LEFT) {
        dir2 = UI_DIR_LEFT;
      }
      else if (dir1 & UI_DIR_RIGHT) {
        dir2 = UI_DIR_RIGHT;
      }
      dir1 &= (UI_DIR_UP | UI_DIR_DOWN);
    }

    if ((dir2 == 0) && ELEM(dir1, UI_DIR_LEFT, UI_DIR_RIGHT)) {
      dir2 = UI_DIR_DOWN;
    }
    if ((dir2 == 0) && ELEM(dir1, UI_DIR_UP, UI_DIR_DOWN)) {
      dir2 = UI_DIR_LEFT;
    }

    /* no space at all? don't change */
    if (left || right) {
      if (dir1 == UI_DIR_LEFT && left == 0) {
        dir1 = UI_DIR_RIGHT;
      }
      if (dir1 == UI_DIR_RIGHT && right == 0) {
        dir1 = UI_DIR_LEFT;
      }
      /* this is aligning, not append! */
      if (dir2 == UI_DIR_LEFT && right == 0) {
        dir2 = UI_DIR_RIGHT;
      }
      if (dir2 == UI_DIR_RIGHT && left == 0) {
        dir2 = UI_DIR_LEFT;
      }
    }
    if (down || top) {
      if (dir1 == UI_DIR_UP && top == 0) {
        dir1 = UI_DIR_DOWN;
      }
      if (dir1 == UI_DIR_DOWN && down == 0) {
        dir1 = UI_DIR_UP;
      }
      BLI_assert(dir2 != UI_DIR_UP);
      //          if (dir2 == UI_DIR_UP   && top == 0)  { dir2 = UI_DIR_DOWN; }
      if (dir2 == UI_DIR_DOWN && down == 0) {
        dir2 = UI_DIR_UP;
      }
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
  float offset_x = 0, offset_y = 0;

  /* Ensure buttons don't come between the parent button and the popup, see: #63566. */
  const float offset_overlap = max_ff(U.pixelsize, 1.0f);

  if (dir1 == UI_DIR_LEFT) {
    offset_x = (butrct.xmin - block->rect.xmax) + offset_overlap;
    if (dir2 == UI_DIR_UP) {
      offset_y = butrct.ymin - block->rect.ymin - center_y - UI_MENU_PADDING;
    }
    else {
      offset_y = butrct.ymax - block->rect.ymax + center_y + UI_MENU_PADDING;
    }
  }
  else if (dir1 == UI_DIR_RIGHT) {
    offset_x = (butrct.xmax - block->rect.xmin) - offset_overlap;
    if (dir2 == UI_DIR_UP) {
      offset_y = butrct.ymin - block->rect.ymin - center_y - UI_MENU_PADDING;
    }
    else {
      offset_y = butrct.ymax - block->rect.ymax + center_y + UI_MENU_PADDING;
    }
  }
  else if (dir1 == UI_DIR_UP) {
    offset_y = (butrct.ymax - block->rect.ymin) - offset_overlap;

    if (but->type == UI_BTYPE_COLOR && block->rect.ymax + offset_y > win_y - UI_POPUP_MENU_TOP) {
      /* Shift this down, aligning the top edge close to the window top. */
      offset_y = win_y - block->rect.ymax - UI_POPUP_MENU_TOP;
      /* All four corners should be rounded since this no longer button-aligned. */
      block->direction = UI_DIR_CENTER_Y;
      dir1 = UI_DIR_CENTER_Y;
    }

    if (dir2 == UI_DIR_RIGHT) {
      offset_x = butrct.xmax - block->rect.xmax + center_x;
    }
    else {
      offset_x = butrct.xmin - block->rect.xmin - center_x;
    }
    /* changed direction? */
    if ((dir1 & block->direction) == 0) {
      /* TODO: still do */
      UI_block_order_flip(block);
    }
  }
  else if (dir1 == UI_DIR_DOWN) {
    offset_y = (butrct.ymin - block->rect.ymax) + offset_overlap;

    if (but->type == UI_BTYPE_COLOR && block->rect.ymin + offset_y < UI_SCREEN_MARGIN) {
      /* Shift this up, aligning the bottom edge close to the window bottom. */
      offset_y = -block->rect.ymin + UI_SCREEN_MARGIN;
      /* All four corners should be rounded since this no longer button-aligned. */
      block->direction = UI_DIR_CENTER_Y;
      dir1 = UI_DIR_CENTER_Y;
    }

    if (dir2 == UI_DIR_RIGHT) {
      offset_x = butrct.xmax - block->rect.xmax + center_x;
    }
    else {
      offset_x = butrct.xmin - block->rect.xmin - center_x;
    }
    /* changed direction? */
    if ((dir1 & block->direction) == 0) {
      /* TODO: still do */
      UI_block_order_flip(block);
    }
  }

  /* Center over popovers for eg. */
  if (block->direction & UI_DIR_CENTER_X) {
    offset_x += BLI_rctf_size_x(&butrct) / ((dir2 == UI_DIR_LEFT) ? 2 : -2);
  }

  /* Apply offset, buttons in window coords. */
  LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
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

    const int s1 = 40 * UI_SCALE_FAC;
    const int s2 = 3 * UI_SCALE_FAC;

    /* parent button to left */
    if (midx < block->rect.xmin) {
      block->safety.xmin = block->rect.xmin - s2;
    }
    else {
      block->safety.xmin = block->rect.xmin - s1;
    }
    /* parent button to right */
    if (midx > block->rect.xmax) {
      block->safety.xmax = block->rect.xmax + s2;
    }
    else {
      block->safety.xmax = block->rect.xmax + s1;
    }

    /* parent button on bottom */
    if (midy < block->rect.ymin) {
      block->safety.ymin = block->rect.ymin - s2;
    }
    else {
      block->safety.ymin = block->rect.ymin - s1;
    }
    /* parent button on top */
    if (midy > block->rect.ymax) {
      block->safety.ymax = block->rect.ymax + s2;
    }
    else {
      block->safety.ymax = block->rect.ymax + s1;
    }

    /* Exception for switched pull-downs. */
    if (dir1 && (dir1 & block->direction) == 0) {
      if (dir2 == UI_DIR_RIGHT) {
        block->safety.xmax = block->rect.xmax + s2;
      }
      if (dir2 == UI_DIR_LEFT) {
        block->safety.xmin = block->rect.xmin - s2;
      }
    }
    block->direction = dir1;
  }

  /* Keep a list of these, needed for pull-down menus. */
  uiSafetyRct *saferct = MEM_cnew<uiSafetyRct>(__func__);
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

static void ui_block_region_refresh(const bContext *C, ARegion *region)
{
  ScrArea *ctx_area = CTX_wm_area(C);
  ARegion *ctx_region = CTX_wm_region(C);

  if (region->do_draw & RGN_REFRESH_UI) {
    ScrArea *handle_ctx_area;
    ARegion *handle_ctx_region;

    region->do_draw &= ~RGN_REFRESH_UI;
    LISTBASE_FOREACH_MUTABLE (uiBlock *, block, &region->uiblocks) {
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

static void ui_block_region_draw(const bContext *C, ARegion *region)
{
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    UI_block_draw(C, block);
  }
}

/**
 * Use to refresh centered popups on screen resizing (for splash).
 */
static void ui_block_region_popup_window_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_WINDOW: {
      switch (wmn->action) {
        case NA_EDITED: {
          /* window resize */
          ED_region_tag_refresh_ui(region);
          break;
        }
      }
      break;
    }
  }
}

static void ui_popup_block_clip(wmWindow *window, uiBlock *block)
{
  const float xmin_orig = block->rect.xmin;
  const int margin = UI_SCREEN_MARGIN;

  if (block->flag & UI_BLOCK_NO_WIN_CLIP) {
    return;
  }

  const int winx = WM_window_pixels_x(window);
  const int winy = WM_window_pixels_y(window);

  /* shift to left if outside of view */
  if (block->rect.xmax > winx - margin) {
    const float xofs = winx - margin - block->rect.xmax;
    block->rect.xmin += xofs;
    block->rect.xmax += xofs;
  }
  /* shift menus to right if outside of view */
  if (block->rect.xmin < margin) {
    const float xofs = (margin - block->rect.xmin);
    block->rect.xmin += xofs;
    block->rect.xmax += xofs;
  }

  if (block->rect.ymin < margin) {
    block->rect.ymin = margin;
  }
  if (block->rect.ymax > winy - UI_POPUP_MENU_TOP) {
    block->rect.ymax = winy - UI_POPUP_MENU_TOP;
  }

  /* ensure menu items draw inside left/right boundary */
  const float xofs = block->rect.xmin - xmin_orig;
  LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
    bt->rect.xmin += xofs;
    bt->rect.xmax += xofs;
  }
}

void ui_popup_block_scrolltest(uiBlock *block)
{
  block->flag &= ~(UI_BLOCK_CLIPBOTTOM | UI_BLOCK_CLIPTOP);

  LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
    bt->flag &= ~UI_SCROLLED;
  }

  if (block->buttons.first == block->buttons.last) {
    return;
  }

  /* mark buttons that are outside boundary */
  LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
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
  LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
    if (block->flag & UI_BLOCK_CLIPBOTTOM) {
      if (bt->rect.ymin < block->rect.ymin + UI_MENU_SCROLL_ARROW) {
        bt->flag |= UI_SCROLLED;
      }
    }
    if (block->flag & UI_BLOCK_CLIPTOP) {
      if (bt->rect.ymax > block->rect.ymax - UI_MENU_SCROLL_ARROW) {
        bt->flag |= UI_SCROLLED;
      }
    }
  }
}

static void ui_popup_block_remove(bContext *C, uiPopupBlockHandle *handle)
{
  wmWindow *ctx_win = CTX_wm_window(C);
  ScrArea *ctx_area = CTX_wm_area(C);
  ARegion *ctx_region = CTX_wm_region(C);

  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = ctx_win;
  bScreen *screen = CTX_wm_screen(C);

  /* There may actually be a different window active than the one showing the popup, so lookup real
   * one. */
  if (BLI_findindex(&screen->regionbase, handle->region) == -1) {
    LISTBASE_FOREACH (wmWindow *, win_iter, &wm->windows) {
      screen = WM_window_get_active_screen(win_iter);
      if (BLI_findindex(&screen->regionbase, handle->region) != -1) {
        win = win_iter;
        break;
      }
    }
  }

  BLI_assert(win && screen);

  CTX_wm_window_set(C, win);
  ui_region_temp_remove(C, screen, handle->region);

  /* Reset context (area and region were nullptr'ed when changing context window). */
  CTX_wm_window_set(C, ctx_win);
  CTX_wm_area_set(C, ctx_area);
  CTX_wm_region_set(C, ctx_region);

  /* reset to region cursor (only if there's not another menu open) */
  if (BLI_listbase_is_empty(&screen->regionbase)) {
    win->tag_cursor_refresh = true;
  }

  if (handle->scrolltimer) {
    WM_event_timer_remove(wm, win, handle->scrolltimer);
  }
}

uiBlock *ui_popup_block_refresh(bContext *C,
                                uiPopupBlockHandle *handle,
                                ARegion *butregion,
                                uiBut *but)
{
  const int margin = UI_POPUP_MARGIN;
  wmWindow *window = CTX_wm_window(C);
  ARegion *region = handle->region;

  const uiBlockCreateFunc create_func = handle->popup_create_vars.create_func;
  const uiBlockHandleCreateFunc handle_create_func = handle->popup_create_vars.handle_create_func;
  void *arg = handle->popup_create_vars.arg;

  uiBlock *block_old = static_cast<uiBlock *>(region->uiblocks.first);

  handle->refresh = (block_old != nullptr);

  BLI_assert(!handle->refresh || handle->can_refresh);

#ifdef DEBUG
  wmEvent *event_back = window->eventstate;
  wmEvent *event_last_back = window->event_last_handled;
#endif

  /* create ui block */
  uiBlock *block;
  if (create_func) {
    block = create_func(C, region, arg);
  }
  else {
    block = handle_create_func(C, handle, arg);
  }

  /* callbacks _must_ leave this for us, otherwise we can't call UI_block_update_from_old */
  BLI_assert(!block->endblock);

  /* ensure we don't use mouse coords here! */
#ifdef DEBUG
  window->eventstate = nullptr;
#endif

  if (block->handle) {
    memcpy(block->handle, handle, sizeof(uiPopupBlockHandle));
    MEM_freeN(handle);
    handle = block->handle;
  }
  else {
    block->handle = handle;
  }

  region->regiondata = handle;

  /* set UI_BLOCK_NUMSELECT before UI_block_end() so we get alphanumeric keys assigned */
  if (but == nullptr) {
    block->flag |= UI_BLOCK_POPUP;
  }

  block->flag |= UI_BLOCK_LOOP;
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  /* defer this until blocks are translated (below) */
  block->oldblock = nullptr;

  if (!block->endblock) {
    UI_block_end_ex(
        C, block, handle->popup_create_vars.event_xy, handle->popup_create_vars.event_xy);
  }

  /* if this is being created from a button */
  if (but) {
    block->aspect = but->block->aspect;
    ui_popup_block_position(window, butregion, but, block);
    handle->direction = block->direction;
  }
  else {
    /* Keep a list of these, needed for pull-down menus. */
    uiSafetyRct *saferct = MEM_cnew<uiSafetyRct>(__func__);
    saferct->safety = block->safety;
    BLI_addhead(&block->saferct, saferct);
  }

  if (block->flag & UI_BLOCK_RADIAL) {
    const int win_width = UI_SCREEN_MARGIN;

    const int winx = WM_window_pixels_x(window);
    const int winy = WM_window_pixels_y(window);

    copy_v2_v2(block->pie_data.pie_center_init, block->pie_data.pie_center_spawned);

    /* only try translation if area is large enough */
    int x_offset = 0;
    if (BLI_rctf_size_x(&block->rect) < winx - (2.0f * win_width)) {
      if (block->rect.xmin < win_width) {
        x_offset += win_width - block->rect.xmin;
      }
      if (block->rect.xmax > winx - win_width) {
        x_offset += winx - win_width - block->rect.xmax;
      }
    }

    int y_offset = 0;
    if (BLI_rctf_size_y(&block->rect) < winy - (2.0f * win_width)) {
      if (block->rect.ymin < win_width) {
        y_offset += win_width - block->rect.ymin;
      }
      if (block->rect.ymax > winy - win_width) {
        y_offset += winy - win_width - block->rect.ymax;
      }
    }
    /* if we are offsetting set up initial data for timeout functionality */

    if ((x_offset != 0) || (y_offset != 0)) {
      block->pie_data.pie_center_spawned[0] += x_offset;
      block->pie_data.pie_center_spawned[1] += y_offset;

      UI_block_translate(block, x_offset, y_offset);

      if (U.pie_initial_timeout > 0) {
        block->pie_data.flags |= UI_PIE_INITIAL_DIRECTION;
      }
    }

    region->winrct.xmin = 0;
    region->winrct.xmax = winx;
    region->winrct.ymin = 0;
    region->winrct.ymax = winy;

    ui_block_calc_pie_segment(block, block->pie_data.pie_center_init);

    /* lastly set the buttons at the center of the pie menu, ready for animation */
    if (U.pie_animation_timeout > 0) {
      LISTBASE_FOREACH (uiBut *, but_iter, &block->buttons) {
        if (but_iter->pie_dir != UI_RADIAL_NONE) {
          BLI_rctf_recenter(&but_iter->rect, UNPACK2(block->pie_data.pie_center_spawned));
        }
      }
    }
  }
  else {
    /* Add an offset to draw the popover arrow. */
    if ((block->flag & UI_BLOCK_POPOVER) && ELEM(block->direction, UI_DIR_UP, UI_DIR_DOWN)) {
      /* Keep sync with 'ui_draw_popover_back_impl'. */
      const float unit_size = U.widget_unit / block->aspect;
      const float unit_half = unit_size * (block->direction == UI_DIR_DOWN ? 0.5 : -0.5);

      UI_block_translate(block, 0, -unit_half);
    }

    /* clip block with window boundary */
    ui_popup_block_clip(window, block);

    /* Avoid menu moving down and losing cursor focus by keeping it at
     * the same height. */
    if (handle->refresh && handle->prev_block_rect.ymax > block->rect.ymax) {
      if (block->bounds_type != UI_BLOCK_BOUNDS_POPUP_CENTER) {
        const float offset = handle->prev_block_rect.ymax - block->rect.ymax;
        UI_block_translate(block, 0, offset);
        block->rect.ymin = handle->prev_block_rect.ymin;
      }
    }

    handle->prev_block_rect = block->rect;

    /* the block and buttons were positioned in window space as in 2.4x, now
     * these menu blocks are regions so we bring it back to region space.
     * additionally we add some padding for the menu shadow or rounded menus */
    region->winrct.xmin = block->rect.xmin - margin;
    region->winrct.xmax = block->rect.xmax + margin;
    region->winrct.ymin = block->rect.ymin - margin;
    region->winrct.ymax = block->rect.ymax + UI_POPUP_MENU_TOP;

    UI_block_translate(block, -region->winrct.xmin, -region->winrct.ymin);

    /* apply scroll offset */
    if (handle->scrolloffset != 0.0f) {
      LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
        bt->rect.ymin += handle->scrolloffset;
        bt->rect.ymax += handle->scrolloffset;
      }
    }
  }

  if (block_old) {
    block->oldblock = block_old;
    UI_block_update_from_old(C, block);
    UI_blocklist_free_inactive(C, region);
  }

  /* checks which buttons are visible, sets flags to prevent draw (do after region init) */
  ui_popup_block_scrolltest(block);

  /* Adds sub-window. */
  ED_region_floating_init(region);

  /* Get `winmat` now that we actually have the sub-window. */
  wmGetProjectionMatrix(block->winmat, &region->winrct);

  /* notify change and redraw */
  ED_region_tag_redraw(region);

  ED_region_update_rect(region);

#ifdef DEBUG
  window->eventstate = event_back;
  window->event_last_handled = event_last_back;
#endif

  return block;
}

uiPopupBlockHandle *ui_popup_block_create(bContext *C,
                                          ARegion *butregion,
                                          uiBut *but,
                                          uiBlockCreateFunc create_func,
                                          uiBlockHandleCreateFunc handle_create_func,
                                          void *arg,
                                          uiFreeArgFunc arg_free)
{
  wmWindow *window = CTX_wm_window(C);
  uiBut *activebut = UI_context_active_but_get(C);

  /* disable tooltips from buttons below */
  if (activebut) {
    UI_but_tooltip_timer_remove(C, activebut);
  }
  /* standard cursor by default */
  WM_cursor_set(window, WM_CURSOR_DEFAULT);

  /* create handle */
  uiPopupBlockHandle *handle = MEM_cnew<uiPopupBlockHandle>(__func__);

  /* store context for operator */
  handle->ctx_area = CTX_wm_area(C);
  handle->ctx_region = CTX_wm_region(C);

  /* store vars to refresh popup (RGN_REFRESH_UI) */
  handle->popup_create_vars.create_func = create_func;
  handle->popup_create_vars.handle_create_func = handle_create_func;
  handle->popup_create_vars.arg = arg;
  handle->popup_create_vars.arg_free = arg_free;
  handle->popup_create_vars.but = but;
  handle->popup_create_vars.butregion = but ? butregion : nullptr;
  copy_v2_v2_int(handle->popup_create_vars.event_xy, window->eventstate->xy);

  /* don't allow by default, only if popup type explicitly supports it */
  handle->can_refresh = false;

  /* create area region */
  ARegion *region = ui_region_temp_add(CTX_wm_screen(C));
  handle->region = region;

  static ARegionType type;
  memset(&type, 0, sizeof(ARegionType));
  type.draw = ui_block_region_draw;
  type.layout = ui_block_region_refresh;
  type.regionid = RGN_TYPE_TEMPORARY;
  region->type = &type;

  UI_region_handlers_add(&region->handlers);

  uiBlock *block = ui_popup_block_refresh(C, handle, butregion, but);
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
  ARegion *region = handle->popup_create_vars.butregion;
  if (region != nullptr) {
    LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
      if (block->handle && (block->flag & UI_BLOCK_POPOVER) &&
          (block->flag & UI_BLOCK_KEEP_OPEN) == 0) {
        uiPopupBlockHandle *menu = block->handle;
        menu->menuretval = UI_RETURN_OK;
      }
    }
  }

  if (handle->popup_create_vars.arg_free) {
    handle->popup_create_vars.arg_free(handle->popup_create_vars.arg);
  }

  ui_popup_block_remove(C, handle);

  MEM_freeN(handle);
}

/** \} */
