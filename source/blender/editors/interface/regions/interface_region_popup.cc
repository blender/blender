/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * PopUp Region (Generic)
 */

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLF_api.hh"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"
#include "BLI_vector_set.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"

#include "interface_intern.hh"
#include "interface_regions_intern.hh"

namespace blender::ui {

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

void popup_translate(ARegion *region, const int mdiff[2])
{
  BLI_rcti_translate(&region->winrct, UNPACK2(mdiff));

  ED_region_update_rect(region);

  ED_region_tag_redraw(region);

  /* update blocks */
  for (Block &block : region->runtime->uiblocks) {
    PopupBlockHandle *handle = block.handle;
    /* Make empty, will be initialized on next use, see #60608. */
    BLI_rctf_init(&handle->prev_block_rect, 0, 0, 0, 0);

    for (SafetyRect &saferct : block.saferct) {
      BLI_rctf_translate(&saferct.parent, UNPACK2(mdiff));
      BLI_rctf_translate(&saferct.safety, UNPACK2(mdiff));
    }
  }
}

/* position block relative to but, result is in window space */
static void ui_popup_block_position(wmWindow *window,
                                    ARegion *butregion,
                                    Button *but,
                                    Block *block)
{
  PopupBlockHandle *handle = block->handle;

  /* Compute button position in window coordinates using the source
   * button region/block, to position the popup attached to it. */
  rctf butrct;
  if (!handle->refresh) {
    block_to_window_rctf(butregion, but->block, &butrct, &but->rect);

    /* widget_roundbox_set has this correction too, keep in sync */
    if (but->type != ButtonType::Pulldown) {
      if (but->drawflag & BUT_ALIGN_TOP) {
        butrct.ymax += U.pixelsize;
      }
      if (but->drawflag & BUT_ALIGN_LEFT) {
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
    if (!block->buttons.is_empty()) {
      BLI_rctf_init_minmax(&block->rect);

      for (const std::unique_ptr<Button> &bt : block->buttons) {
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

  /* Trim the popup and its contents to the width of the button if the size difference
   * is small. This avoids cases where the rounded corner clips underneath the button. */
  const int delta = BLI_rctf_size_x(&block->rect) - BLI_rctf_size_x(&butrct);
  const float max_radius = (0.5f * U.widget_unit);

  if (delta >= 0 && delta < max_radius) {
    for (const std::unique_ptr<Button> &bt : block->buttons) {
      /* Only trim the right most buttons in multi-column popovers. */
      if (bt->rect.xmax == block->rect.xmax) {
        bt->rect.xmax -= delta;
      }
    }
    block->rect.xmax -= delta;
  }

  block_to_window_rctf(butregion, but->block, &block->rect, &block->rect);

  /* `block->rect` is already scaled with `butregion->winrct`,
   * apply this scale to layout panels too. */
  if (Panel *panel = block->panel) {
    for (LayoutPanelBody &body : panel->runtime->layout_panels.bodies) {
      body.start_y /= block->aspect;
      body.end_y /= block->aspect;
    }
    for (LayoutPanelHeader &header : panel->runtime->layout_panels.headers) {
      header.start_y /= block->aspect;
      header.end_y /= block->aspect;
    }
  }

  /* Compute direction relative to button, based on available space. */
  const int size_x = BLI_rctf_size_x(&block->rect) + 0.2f * UI_UNIT_X; /* 4 for shadow */
  const int size_y = BLI_rctf_size_y(&block->rect) + 0.2f * UI_UNIT_Y;
  const int center_x = (block->direction & UI_DIR_CENTER_X) ? size_x / 2 : 0;
  const int center_y = (block->direction & UI_DIR_CENTER_Y) ? size_y / 2 : 0;

  const int2 win_size = WM_window_native_pixel_size(window);

  /* Take into account maximum size so we don't have to flip on refresh. */
  const float2 max_size = {
      max_ff(size_x, handle->max_size_x),
      max_ff(size_y, handle->max_size_y),
  };

  short dir1 = 0, dir2 = 0;

  if (!handle->refresh) {
    bool left = false, right = false, top = false, down = false;

    /* check if there's space at all */
    if (butrct.xmin - max_size[0] + center_x > 0.0f) {
      left = true;
    }
    if (butrct.xmax + max_size[0] - center_x < win_size[0]) {
      right = true;
    }
    if (butrct.ymin - max_size[1] + center_y > 0.0f) {
      down = true;
    }
    if (butrct.ymax + max_size[1] - center_y < win_size[1]) {
      top = true;
    }

    if (top == 0 && down == 0) {
      if (butrct.ymin - max_size[1] < win_size[1] - butrct.ymax - max_size[1]) {
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

    if (but->type == ButtonType::Color &&
        block->rect.ymax + offset_y > win_size[1] - UI_POPUP_MENU_TOP)
    {
      /* Shift this down, aligning the top edge close to the window top. */
      offset_y = win_size[1] - block->rect.ymax - UI_POPUP_MENU_TOP;
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
  }
  else if (dir1 == UI_DIR_DOWN) {
    offset_y = (butrct.ymin - block->rect.ymax) + offset_overlap;

    if (but->type == ButtonType::Color && block->rect.ymin + offset_y < UI_SCREEN_MARGIN) {
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
  }

  /* Center over popovers for eg. */
  if (block->direction & UI_DIR_CENTER_X) {
    offset_x += BLI_rctf_size_x(&butrct) / ((dir2 == UI_DIR_LEFT) ? 2 : -2);
  }

  /* Apply offset, buttons in window coords. */
  for (const std::unique_ptr<Button> &bt : block->buttons) {
    block_to_window_rctf(butregion, but->block, &bt->rect, &bt->rect);

    BLI_rctf_translate(&bt->rect, offset_x, offset_y);

    /* ui_but_update recalculates drawstring size in pixels */
    button_update(bt.get());
  }

  BLI_rctf_translate(&block->rect, offset_x, offset_y);

  /* Safety calculus. */
  {
    const float midx = BLI_rctf_cent_x(&butrct);
    const float midy = BLI_rctf_cent_y(&butrct);

    /* when you are outside parent button, safety there should be smaller */

    const int s1 = (U.flag & USER_MENU_CLOSE_LEAVE) ? 40 * UI_SCALE_FAC : win_size[0];
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

    const bool fully_aligned_with_button = BLI_rctf_size_x(&block->rect) <=
                                           BLI_rctf_size_x(&butrct) + 1;
    const bool off_screen_left = (block->rect.xmin < 0);
    const bool off_screen_right = (block->rect.xmax > win_size[0]);

    if (fully_aligned_with_button) {
      /* Popup is neither left or right from the button. */
      dir2 &= ~(UI_DIR_LEFT | UI_DIR_RIGHT);
    }
    else if (off_screen_left || off_screen_right) {
      /* Popup is both left and right from the button. */
      dir2 |= (UI_DIR_LEFT | UI_DIR_RIGHT);
    }

    /* Popovers don't need secondary direction. Pull-downs to
     * the left or right are currently not supported. */
    const bool no_2nd_dir = (but->type == ButtonType::Popover ||
                             button_menu_draw_as_popover(but) ||
                             dir1 & (UI_DIR_RIGHT | UI_DIR_LEFT));
    block->direction = no_2nd_dir ? dir1 : (dir1 | dir2);
  }

  /* Keep a list of these, needed for pull-down menus. */
  SafetyRect *saferct = MEM_new_for_free<SafetyRect>(__func__);
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

static void block_region_refresh(const bContext *C, ARegion *region)
{
  BLI_assert(region->regiontype == RGN_TYPE_TEMPORARY);

  ScrArea *ctx_area = CTX_wm_area(C);
  ARegion *ctx_region = CTX_wm_region(C);

  if (region->runtime->do_draw & RGN_REFRESH_UI) {
    ScrArea *handle_ctx_area;
    ARegion *handle_ctx_region;

    region->runtime->do_draw &= ~RGN_REFRESH_UI;
    for (Block &block : region->runtime->uiblocks.items_mutable()) {
      PopupBlockHandle *handle = block.handle;

      if (handle->can_refresh) {
        handle_ctx_area = handle->ctx_area;
        handle_ctx_region = handle->ctx_region;

        if (handle_ctx_area) {
          CTX_wm_area_set(const_cast<bContext *>(C), handle_ctx_area);
        }
        if (handle_ctx_region) {
          CTX_wm_region_set(const_cast<bContext *>(C), handle_ctx_region);
        }

        Button *but = handle->popup_create_vars.but;
        ARegion *butregion = handle->popup_create_vars.butregion;
        popup_block_refresh(const_cast<bContext *>(C), handle, butregion, but);
      }
    }
  }

  CTX_wm_area_set(const_cast<bContext *>(C), ctx_area);
  CTX_wm_region_set(const_cast<bContext *>(C), ctx_region);
}

static void block_region_draw(const bContext *C, ARegion *region)
{
  for (Block &block : region->runtime->uiblocks) {
    block_draw(C, &block);
  }
}

/**
 * Use to refresh centered popups on screen resizing (for splash).
 */
static void block_region_popup_window_listener(const wmRegionListenerParams *params)
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

static void ui_popup_block_clip(wmWindow *window, Block *block)
{
  const float xmin_orig = block->rect.xmin;
  const int margin = UI_SCREEN_MARGIN;

  if (block->flag & BLOCK_NO_WIN_CLIP) {
    return;
  }

  const int2 win_size = WM_window_native_pixel_size(window);

  /* shift to left if outside of view */
  if (block->rect.xmax > win_size[0] - margin) {
    const float xofs = win_size[0] - margin - block->rect.xmax;
    block->rect.xmin += xofs;
    block->rect.xmax += xofs;
  }
  /* shift menus to right if outside of view */
  if (block->rect.xmin < margin) {
    const float xofs = (margin - block->rect.xmin);
    block->rect.xmin += xofs;
    block->rect.xmax += xofs;
  }

  block->rect.ymin = std::max<float>(block->rect.ymin, margin);
  block->rect.ymax = std::min<float>(block->rect.ymax, win_size[1] - UI_POPUP_MENU_TOP);

  /* ensure menu items draw inside left/right boundary */
  const float xofs = block->rect.xmin - xmin_orig;
  for (const std::unique_ptr<Button> &bt : block->buttons) {
    bt->rect.xmin += xofs;
    bt->rect.xmax += xofs;
  }
}

void popup_block_scrolltest(Block *block)
{
  block->flag &= ~(BLOCK_CLIPBOTTOM | BLOCK_CLIPTOP);

  for (const std::unique_ptr<Button> &bt : block->buttons) {
    bt->flag &= ~UI_SCROLLED;
  }

  if (block->buttons.size() < 2) {
    return;
  }

  /* mark buttons that are outside boundary */
  for (const std::unique_ptr<Button> &bt : block->buttons) {
    if (bt->rect.ymax < block->rect.ymin) {
      bt->flag |= UI_SCROLLED;
    }
    if (bt->rect.ymin > block->rect.ymax) {
      bt->flag |= UI_SCROLLED;
    }
    if (bt->rect.ymin < block->rect.ymin) {
      block->flag |= BLOCK_CLIPBOTTOM;
    }
    if (bt->rect.ymax > block->rect.ymax) {
      block->flag |= BLOCK_CLIPTOP;
    }
  }

  /* mark buttons overlapping arrows, if we have them */
  for (const std::unique_ptr<Button> &bt : block->buttons) {
    if (block->flag & BLOCK_CLIPBOTTOM) {
      if (bt->rect.ymax < block->rect.ymin + UI_MENU_SCROLL_MOUSE) {
        bt->flag |= UI_SCROLLED;
      }
    }
    if (block->flag & BLOCK_CLIPTOP) {
      if (bt->rect.ymin > block->rect.ymax - UI_MENU_SCROLL_MOUSE) {
        bt->flag |= UI_SCROLLED;
      }
    }
  }
}

static void ui_popup_block_remove(bContext *C, PopupBlockHandle *handle)
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
    for (wmWindow &win_iter : wm->windows) {
      screen = WM_window_get_active_screen(&win_iter);
      if (BLI_findindex(&screen->regionbase, handle->region) != -1) {
        win = &win_iter;
        break;
      }
    }
  }

  BLI_assert(win && screen);

  CTX_wm_window_set(C, win);
  region_temp_remove(C, screen, handle->region);

  /* Reset context (area and region were null'ed when changing context window). */
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

void layout_panel_popup_scroll_apply(Panel *panel, const float dy)
{
  if (!panel || dy == 0.0f) {
    return;
  }
  for (LayoutPanelBody &body : panel->runtime->layout_panels.bodies) {
    body.start_y += dy;
    body.end_y += dy;
  }
  for (LayoutPanelHeader &headcer : panel->runtime->layout_panels.headers) {
    headcer.start_y += dy;
    headcer.end_y += dy;
  }
}

/**
 * Persistent storage of open-close-state of layout panels in popups.
 *
 * Usually this state is stored in each region's panels, however since these regions are
 * temporally allocated this state is lost when the popup is closed and the region is freed.
 * See #152631.
 */
struct PopupLayoutPanelStates {
  /** #PanelType::idname or #OperatorType::idname. */
  std::string idname;
  ListBaseT<LayoutPanelState> states = {};

  PopupLayoutPanelStates(StringRef idname) : idname{idname} {}

  ~PopupLayoutPanelStates()
  {
    for (LayoutPanelState &state : states.items_mutable()) {
      BLI_remlink(&states, &state);
      MEM_freeN(state.idname);
      MEM_freeN(&state);
    }
  }
};

struct PopupLayoutPanelStatesIDNameGetter {
  StringRef operator()(const std::unique_ptr<PopupLayoutPanelStates> &value) const
  {
    return StringRef(value->idname);
  }
};

ListBaseT<LayoutPanelState> &popup_persistent_layout_panel_states(StringRef idname)
{
  static CustomIDVectorSet<std::unique_ptr<PopupLayoutPanelStates>,
                           PopupLayoutPanelStatesIDNameGetter>
      popup_states;
  if (!popup_states.contains_as(idname)) {
    popup_states.add_new(std::make_unique<PopupLayoutPanelStates>(idname));
  }
  return popup_states.lookup_key_as(idname)->states;
}

void popup_dummy_panel_set(ARegion *region, Block *block, StringRef idname)
{
  Panel *&panel = region->runtime->popup_block_panel;
  if (!panel) {
    /* Dummy popup panel type. */
    static PanelType panel_type = []() {
      PanelType type{};
      type.flag = PANEL_TYPE_NO_HEADER;
      return type;
    }();
    panel = BKE_panel_new(&panel_type);
  }
  panel->runtime->layout_panels.clear();
  panel->runtime->popup_layout_panel_states = &popup_persistent_layout_panel_states(idname);
  block->panel = panel;
  panel->runtime->block = block;
}

Block *popup_block_refresh(bContext *C, PopupBlockHandle *handle, ARegion *butregion, Button *but)
{
  const int margin = UI_POPUP_MARGIN;
  wmWindow *window = CTX_wm_window(C);
  ARegion *region = handle->region;

  const BlockCreateFunc create_func = handle->popup_create_vars.create_func;
  const BlockHandleCreateFunc handle_create_func = handle->popup_create_vars.handle_create_func;
  void *arg = handle->popup_create_vars.arg;

  Block *block_old = static_cast<Block *>(region->runtime->uiblocks.first);

  handle->refresh = (block_old != nullptr);

  BLI_assert(!handle->refresh || handle->can_refresh);

#ifndef NDEBUG
  wmEvent *event_back = window->runtime->eventstate;
  wmEvent *event_last_back = window->runtime->event_last_handled;
#endif

  /* create ui block */
  Block *block;
  if (create_func) {
    block = create_func(C, region, arg);
  }
  else {
    block = handle_create_func(C, handle, arg);
  }

  /* Don't create accelerator keys if the parent menu does not have them. */
  if (but && but->block->flag & BLOCK_NO_ACCELERATOR_KEYS) {
    block->flag |= BLOCK_NO_ACCELERATOR_KEYS;
  }

  /* callbacks _must_ leave this for us, otherwise we can't call block_update_from_old */
  BLI_assert(!block->endblock);

  /* Ensure we don't use mouse coords here.
   *
   * NOTE(@ideasman42): Important because failing to do will cause glitches refreshing the popup.
   *
   * - Many popups use #wmEvent::xy to position them.
   * - Refreshing a pop-up must only ever change it's contents. Consider that refreshing
   *   might be used to show a menu item as grayed out, or change a text label,
   *   we *never* want the popup to move based on the cursor location while refreshing.
   * - The location of the cursor at the time of creation is stored in:
   *   `handle->popup_create_vars.event_xy` which must be used instead.
   *
   * Since it's difficult to control logic which is called indirectly here,
   * clear the `eventstate` entirely to ensure it's never used when refreshing a popup. */
#ifndef NDEBUG
  window->runtime->eventstate = nullptr;
#endif

  if (block->handle) {
    memcpy(block->handle, handle, sizeof(PopupBlockHandle));
    MEM_delete(handle);
    handle = block->handle;
  }
  else {
    block->handle = handle;
  }

  region->regiondata = handle;

  /* set BLOCK_NUMSELECT before block_end() so we get alphanumeric keys assigned */
  if (but == nullptr) {
    block->flag |= BLOCK_POPUP;
  }

  block->flag |= BLOCK_LOOP;
  block_theme_style_set(block, BLOCK_THEME_STYLE_POPUP);

  /* defer this until blocks are translated (below) */
  block->oldblock = nullptr;

  if (!block->endblock) {
    block_end_ex(C,
                 CTX_data_main(C),
                 window,
                 CTX_data_scene(C),
                 region,
                 CTX_data_depsgraph_pointer(C),
                 block,
                 handle->popup_create_vars.event_xy,
                 handle->popup_create_vars.event_xy);
  }

  /* if this is being created from a button */
  if (but) {
    block->aspect = but->block->aspect;
    ui_popup_block_position(window, butregion, but, block);
    handle->direction = block->direction;
  }
  else {
    /* Keep a list of these, needed for pull-down menus. */
    SafetyRect *saferct = MEM_new_for_free<SafetyRect>(__func__);
    saferct->safety = block->safety;
    BLI_addhead(&block->saferct, saferct);
  }

  if (block->flag & BLOCK_PIE_MENU) {
    const int win_width = UI_SCREEN_MARGIN;

    const int2 win_size = WM_window_native_pixel_size(window);

    copy_v2_v2(block->pie_data.pie_center_init, block->pie_data.pie_center_spawned);

    /* only try translation if area is large enough */
    int x_offset = 0;
    if (BLI_rctf_size_x(&block->rect) < win_size[0] - (2.0f * win_width)) {
      if (block->rect.xmin < win_width) {
        x_offset += win_width - block->rect.xmin;
      }
      if (block->rect.xmax > win_size[0] - win_width) {
        x_offset += win_size[0] - win_width - block->rect.xmax;
      }
    }

    int y_offset = 0;
    if (BLI_rctf_size_y(&block->rect) < win_size[1] - (2.0f * win_width)) {
      if (block->rect.ymin < win_width) {
        y_offset += win_width - block->rect.ymin;
      }
      if (block->rect.ymax > win_size[1] - win_width) {
        y_offset += win_size[1] - win_width - block->rect.ymax;
      }
    }
    /* if we are offsetting set up initial data for timeout functionality */

    if ((x_offset != 0) || (y_offset != 0)) {
      block->pie_data.pie_center_spawned[0] += x_offset;
      block->pie_data.pie_center_spawned[1] += y_offset;

      block_translate(block, x_offset, y_offset);

      if (U.pie_initial_timeout > 0) {
        block->pie_data.flags |= PIE_INITIAL_DIRECTION;
      }
    }

    region->winrct.xmin = 0;
    region->winrct.xmax = win_size[0];
    region->winrct.ymin = 0;
    region->winrct.ymax = win_size[1];

    block_calc_pie_segment(block, block->pie_data.pie_center_init);

    /* lastly set the buttons at the center of the pie menu, ready for animation */
    if (U.pie_animation_timeout > 0) {
      for (const std::unique_ptr<Button> &but_iter : block->buttons) {
        if (but_iter->pie_dir != UI_RADIAL_NONE) {
          BLI_rctf_recenter(&but_iter->rect, UNPACK2(block->pie_data.pie_center_spawned));
        }
      }
    }
  }
  else {
    /* Add an offset to draw the popover arrow. */
    if ((block->flag & BLOCK_POPOVER) && ELEM(block->direction, UI_DIR_UP, UI_DIR_DOWN)) {
      /* Keep sync with 'ui_draw_popover_back_impl'. */
      const float unit_size = U.widget_unit / block->aspect;
      const float unit_half = unit_size * (block->direction == UI_DIR_DOWN ? 0.5 : -0.5);

      block_translate(block, 0, -unit_half);
    }

    /* clip block with window boundary */
    ui_popup_block_clip(window, block);

    /* Avoid menu moving down and losing cursor focus by keeping it at the same height when the
     * popup is displaced down by at least one window unit. */
    if (handle->refresh && (handle->prev_block_rect.ymax - block->rect.ymax) > 1.0f) {
      if (block->bounds_type != BLOCK_BOUNDS_POPUP_CENTER) {
        const float offset = handle->prev_block_rect.ymax - block->rect.ymax;
        block_translate(block, 0, offset);
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

    block_translate(block, -region->winrct.xmin, -region->winrct.ymin);
    /* Popups can change size, fix scroll offset if a panel was closed. */
    float ymin = FLT_MAX;
    float ymax = -FLT_MAX;
    for (const std::unique_ptr<Button> &bt : block->buttons) {
      ymin = min_ff(ymin, bt->rect.ymin);
      ymax = max_ff(ymax, bt->rect.ymax);
    }
    const int scroll_pad = block_is_menu(block) ? UI_MENU_SCROLL_PAD : UI_UNIT_Y * 0.5f;
    const float scroll_min = std::min(block->rect.ymax - ymax - scroll_pad, 0.0f);
    const float scroll_max = std::max(block->rect.ymin - ymin + scroll_pad, 0.0f);
    handle->scrolloffset = std::clamp(handle->scrolloffset, scroll_min, scroll_max);
    /* apply scroll offset */
    if (handle->scrolloffset != 0.0f) {
      for (const std::unique_ptr<Button> &bt : block->buttons) {
        bt->rect.ymin += handle->scrolloffset;
        bt->rect.ymax += handle->scrolloffset;
      }
    }
    /* Layout panels are relative to `block->rect.ymax`. Rather than a
     * scroll, this is a offset applied due to the overflow at the top. */
    layout_panel_popup_scroll_apply(block->panel, -scroll_min);
  }
  /* Apply popup scroll offset to layout panels. */
  layout_panel_popup_scroll_apply(block->panel, handle->scrolloffset);

  if (block_old) {
    block->oldblock = block_old;
    block_update_from_old(C, block);
    blocklist_free_inactive(C, region);
  }

  /* checks which buttons are visible, sets flags to prevent draw (do after region init) */
  popup_block_scrolltest(block);

  /* Adds sub-window. */
  ED_region_floating_init(region);

  /* Get `winmat` now that we actually have the sub-window. */
  wmGetProjectionMatrix(block->winmat, &region->winrct);

  /* notify change and redraw */
  ED_region_tag_redraw(region);

  ED_region_update_rect(region);

#ifndef NDEBUG
  window->runtime->eventstate = event_back;
  window->runtime->event_last_handled = event_last_back;
#endif

  return block;
}

PopupBlockHandle *popup_block_create(bContext *C,
                                     ARegion *butregion,
                                     Button *but,
                                     BlockCreateFunc create_func,
                                     BlockHandleCreateFunc handle_create_func,
                                     void *arg,
                                     FreeArgFunc arg_free,
                                     const bool can_refresh)
{
  wmWindow *window = CTX_wm_window(C);
  Button *activebut = context_active_but_get(C);

  /* disable tooltips from buttons below */
  if (activebut) {
    button_tooltip_timer_remove(C, activebut);
  }
  /* standard cursor by default */
  WM_cursor_set(window, WM_CURSOR_DEFAULT);

  /* create handle */
  PopupBlockHandle *handle = MEM_new<PopupBlockHandle>(__func__);

  /* store context for operator */
  handle->ctx_area = CTX_wm_area(C);
  handle->ctx_region = CTX_wm_region(C);
  handle->can_refresh = can_refresh;

  /* store vars to refresh popup (RGN_REFRESH_UI) */
  handle->popup_create_vars.create_func = create_func;
  handle->popup_create_vars.handle_create_func = handle_create_func;
  handle->popup_create_vars.arg = arg;
  handle->popup_create_vars.arg_free = arg_free;
  handle->popup_create_vars.but = but;
  handle->popup_create_vars.butregion = but ? butregion : nullptr;
  copy_v2_v2_int(handle->popup_create_vars.event_xy, window->runtime->eventstate->xy);

  /* create area region */
  ARegion *region = region_temp_add(CTX_wm_screen(C));
  handle->region = region;

  static ARegionType type;
  memset(&type, 0, sizeof(ARegionType));
  type.draw = block_region_draw;
  type.layout = block_region_refresh;
  type.regionid = RGN_TYPE_TEMPORARY;
  region->runtime->type = &type;

  region_handlers_add(&region->runtime->handlers);

  /* Note that this will be set in the code-path that typically calls refreshing
   * (that loops over #Screen::regionbase and refreshes regions tagged with #RGN_REFRESH_UI).
   * Whereas this only runs on initial creation.
   * Set the region here so drawing logic can rely on it being set.
   * Note that restoring the previous value may not be needed, it just avoids potential
   * problems caused by popups manipulating the context which created them.
   *
   * The check for `can_refresh` exists because the context when refreshing sets the "region_popup"
   * so failing to do so here would cause callbacks draw function to have a different context
   * the first time it's called. Setting this in every context causes button context menus to
   * fail because setting the "region_popup" causes poll functions to reference the popup region
   * instead of the region where the button was created, see #121728.
   *
   * NOTE(@ideasman42): the logic for which popups run with their region set to
   * #bContext::wm::region_popup could be adjusted, making this context member depend on
   * the ability to refresh seems somewhat arbitrary although it does make *some* sense
   * because accessing the region later (to tag for refreshing for example)
   * only makes sense if that region supports refreshing. */
  ARegion *region_popup_prev = nullptr;
  if (can_refresh) {
    region_popup_prev = CTX_wm_region_popup(C);
    CTX_wm_region_popup_set(C, region);
  }

  Block *block = popup_block_refresh(C, handle, butregion, but);
  handle = block->handle;

  /* Wait with tooltips until the mouse is moved, button handling will re-enable them on the first
   * actual mouse move. */
  block->tooltipdisabled = true;

  if (can_refresh) {
    CTX_wm_region_popup_set(C, region_popup_prev);
  }

  /* keep centered on window resizing */
  if (block->bounds_type == BLOCK_BOUNDS_POPUP_CENTER) {
    type.listener = block_region_popup_window_listener;
  }

  return handle;
}

void popup_block_free(bContext *C, PopupBlockHandle *handle)
{
  bool is_submenu = false;

  /* If this popup is created from a popover which does NOT have keep-open flag set,
   * then close the popover too. We could extend this to other popup types too. */
  ARegion *region = handle->popup_create_vars.butregion;
  if (region != nullptr) {
    for (Block &block : region->runtime->uiblocks) {
      if (block.handle && (block.flag & BLOCK_POPOVER) && (block.flag & BLOCK_KEEP_OPEN) == 0) {
        PopupBlockHandle *menu = block.handle;
        menu->menuretval = RETURN_OK;
      }

      if (block_is_menu(&block)) {
        is_submenu = true;
      }
    }
  }

  /* Clear the status bar text that is set when opening a menu. */
  if (!is_submenu) {
    ED_workspace_status_text(C, nullptr);
  }

  if (handle->popup_create_vars.arg_free) {
    handle->popup_create_vars.arg_free(handle->popup_create_vars.arg);
  }

  if (handle->region->runtime->popup_block_panel) {
    BKE_panel_free(handle->region->runtime->popup_block_panel);
  }

  ui_popup_block_remove(C, handle);

  MEM_delete(handle);
}

struct AlertData {
  AlertIcon icon;
  std::string title;
  std::string message;
  bool compact;
  bool okay_button;
  bool mouse_move_quit;
};

static void ui_alert_ok_cb(bContext *C, void *arg1, void *arg2)
{
  AlertData *data = static_cast<AlertData *>(arg1);
  MEM_delete(data);
  Block *block = static_cast<Block *>(arg2);
  popup_menu_retval_set(block, RETURN_OK, true);
  wmWindow *win = CTX_wm_window(C);
  popup_block_close(C, win, block);
}

static void ui_alert_ok(bContext * /*C*/, void *arg, int /*retval*/)
{
  AlertData *data = static_cast<AlertData *>(arg);
  MEM_delete(data);
}

static void ui_alert_cancel(bContext * /*C*/, void *user_data)
{
  AlertData *data = static_cast<AlertData *>(user_data);
  MEM_delete(data);
}

static Block *ui_alert_create(bContext *C, ARegion *region, void *user_data)
{
  AlertData *data = static_cast<AlertData *>(user_data);

  const uiStyle *style = style_get_dpi();
  const short icon_size = (data->compact ? 32 : 40) * UI_SCALE_FAC;
  const int max_width = int((data->compact ? 250.0f : 350.0f) * UI_SCALE_FAC);
  const int min_width = int(120.0f * UI_SCALE_FAC);

  Block *block = block_begin(C, region, __func__, EmbossType::Emboss);
  block_theme_style_set(block, BLOCK_THEME_STYLE_POPUP);
  block_flag_disable(block, BLOCK_LOOP);
  block_emboss_set(block, EmbossType::Emboss);
  popup_dummy_panel_set(region, block, data->title);

  block_flag_enable(block, BLOCK_KEEP_OPEN | BLOCK_NUMSELECT);
  if (data->mouse_move_quit) {
    block_flag_enable(block, BLOCK_MOVEMOUSE_QUIT);
  }

  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;

  fontstyle_set(&style->widget);
  /* Width based on the text lengths. */
  int text_width = BLF_width(style->widget.uifont_id, data->title.c_str(), data->title.size());

  Vector<StringRef> messages = BLF_string_wrap(
      fstyle->uifont_id, data->message, max_width, BLFWrapMode::Typographical);

  for (auto &st_ref : messages) {
    const std::string &st = st_ref;
    text_width = std::max(text_width,
                          int(BLF_width(style->widget.uifont_id, st.c_str(), st.size())));
  }

  int dialog_width = std::max(text_width + int(style->columnspace * 2.5), min_width);

  Layout &layout = *uiItemsAlertBox(block, style, dialog_width + icon_size, data->icon, icon_size);

  Layout &content = layout.column(false);
  content.scale_y_set(0.75f);

  /* Title. */
  uiItemL_ex(&content, data->title, ICON_NONE, true, false);

  content.separator(1.0f);

  /* Message lines. */
  for (auto &st : messages) {
    content.label(st, ICON_NONE);
  }

  if (data->okay_button) {

    layout.separator(2.0f);

    /* Clear so the OK button is left alone. */
    block_func_set(block, nullptr, nullptr, nullptr);

    const float pad = std::max((1.0f - ((200.0f * UI_SCALE_FAC) / float(text_width))) / 2.0f,
                               0.01f);
    Layout &split = layout.split(pad, true);
    split.column(true);
    Layout &buttons = split.split(1.0f - (pad * 2.0f), true);
    buttons.scale_y_set(1.2f);

    Block *buttons_block = layout.block();
    Button *okay_but = uiDefBut(
        buttons_block, ButtonType::But, "OK", 0, 0, 0, UI_UNIT_Y, nullptr, 0, 0, "");
    button_func_set(okay_but, ui_alert_ok_cb, user_data, block);
    button_flag_enable(okay_but, BUT_ACTIVE_DEFAULT);
  }

  const int padding = (data->compact ? 10 : 14) * UI_SCALE_FAC;

  if (data->mouse_move_quit) {
    const float button_center_x = -0.5f;
    const float button_center_y = data->okay_button ? 4.0f : 2.0f;
    const int bounds_offset[2] = {int(button_center_x * layout.width()),
                                  int(button_center_y * UI_UNIT_X)};
    block_bounds_set_popup(block, padding, bounds_offset);
  }
  else {
    block_bounds_set_centered(block, padding);
  }

  return block;
}

void alert(bContext *C,
           const StringRef title,
           const StringRef message,
           const AlertIcon icon,
           const bool compact)
{
  AlertData *data = MEM_new<AlertData>(__func__);
  data->title = title;
  data->message = message;
  data->icon = icon;
  data->compact = compact;
  data->okay_button = true;
  data->mouse_move_quit = compact;

  popup_block_ex(C, ui_alert_create, ui_alert_ok, ui_alert_cancel, data, nullptr);
}

/** \} */

}  // namespace blender::ui
