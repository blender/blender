/*
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
 */

/** \file
 * \ingroup edinterface
 *
 * PopUp Menu Region
 */

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BLI_ghash.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_report.h"
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
/** \name Utility Functions
 * \{ */

bool ui_but_menu_step_poll(const uiBut *but)
{
  BLI_assert(but->type == UI_BTYPE_MENU);

  /* currently only RNA buttons */
  return ((but->menu_step_func != NULL) ||
          (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_ENUM));
}

int ui_but_menu_step(uiBut *but, int direction)
{
  if (ui_but_menu_step_poll(but)) {
    if (but->menu_step_func) {
      return but->menu_step_func(but->block->evil_C, direction, but->poin);
    }
    else {
      const int curval = RNA_property_enum_get(&but->rnapoin, but->rnaprop);
      return RNA_property_enum_step(
          but->block->evil_C, &but->rnapoin, but->rnaprop, curval, direction);
    }
  }

  printf("%s: cannot cycle button '%s'\n", __func__, but->str);
  return 0;
}

static uint ui_popup_string_hash(const char *str, const bool use_sep)
{
  /* sometimes button contains hotkey, sometimes not, strip for proper compare */
  int hash;
  const char *delimit = use_sep ? strrchr(str, UI_SEP_CHAR) : NULL;

  if (delimit) {
    hash = BLI_ghashutil_strhash_n(str, delimit - str);
  }
  else {
    hash = BLI_ghashutil_strhash(str);
  }

  return hash;
}

uint ui_popup_menu_hash(const char *str)
{
  return BLI_ghashutil_strhash(str);
}

/* but == NULL read, otherwise set */
static uiBut *ui_popup_menu_memory__internal(uiBlock *block, uiBut *but)
{
  static uint mem[256];
  static bool first = true;

  const uint hash = block->puphash;
  const uint hash_mod = hash & 255;

  if (first) {
    /* init */
    memset(mem, -1, sizeof(mem));
    first = 0;
  }

  if (but) {
    /* set */
    mem[hash_mod] = ui_popup_string_hash(but->str, but->flag & UI_BUT_HAS_SEP_CHAR);
    return NULL;
  }
  else {
    /* get */
    for (but = block->buttons.first; but; but = but->next) {
      if (mem[hash_mod] == ui_popup_string_hash(but->str, but->flag & UI_BUT_HAS_SEP_CHAR)) {
        return but;
      }
    }

    return NULL;
  }
}

uiBut *ui_popup_menu_memory_get(uiBlock *block)
{
  return ui_popup_menu_memory__internal(block, NULL);
}

void ui_popup_menu_memory_set(uiBlock *block, uiBut *but)
{
  ui_popup_menu_memory__internal(block, but);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Popup Menu with Callback or String
 * \{ */

struct uiPopupMenu {
  uiBlock *block;
  uiLayout *layout;
  uiBut *but;
  ARegion *butregion;

  int mx, my;
  bool popup, slideout;

  uiMenuCreateFunc menu_func;
  void *menu_arg;
};

static uiBlock *ui_block_func_POPUP(bContext *C, uiPopupBlockHandle *handle, void *arg_pup)
{
  uiBlock *block;
  uiPopupMenu *pup = arg_pup;
  int minwidth, width, height;
  char direction;
  bool flip;

  if (pup->menu_func) {
    pup->block->handle = handle;
    pup->menu_func(C, pup->layout, pup->menu_arg);
    pup->block->handle = NULL;
  }

  /* Find block minimum width. */
  if (uiLayoutGetUnitsX(pup->layout) != 0.0f) {
    /* Use the minimum width from the layout if it's set. */
    minwidth = uiLayoutGetUnitsX(pup->layout) * UI_UNIT_X;
  }
  else if (pup->but) {
    /* minimum width to enforece */
    if (pup->but->drawstr[0]) {
      minwidth = BLI_rctf_size_x(&pup->but->rect);
    }
    else {
      /* For buttons with no text, use the minimum (typically icon only). */
      minwidth = UI_MENU_WIDTH_MIN;
    }
  }
  else {
    minwidth = UI_MENU_WIDTH_MIN;
  }

  /* Find block direction. */
  if (pup->but) {
    if (pup->block->direction != 0) {
      /* allow overriding the direction from menu_func */
      direction = pup->block->direction;
    }
    else {
      direction = UI_DIR_DOWN;
    }
  }
  else {
    direction = UI_DIR_DOWN;
  }

  flip = (direction == UI_DIR_DOWN);

  block = pup->block;

  /* in some cases we create the block before the region,
   * so we set it delayed here if necessary */
  if (BLI_findindex(&handle->region->uiblocks, block) == -1) {
    UI_block_region_set(block, handle->region);
  }

  block->direction = direction;

  UI_block_layout_resolve(block, &width, &height);

  UI_block_flag_enable(block, UI_BLOCK_MOVEMOUSE_QUIT);

  if (pup->popup) {
    uiBut *bt;
    int offset[2];

    uiBut *but_activate = NULL;
    UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_NUMSELECT);
    UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
    UI_block_direction_set(block, direction);

    /* offset the mouse position, possibly based on earlier selection */
    if ((block->flag & UI_BLOCK_POPUP_MEMORY) && (bt = ui_popup_menu_memory_get(block))) {
      /* position mouse on last clicked item, at 0.8*width of the
       * button, so it doesn't overlap the text too much, also note
       * the offset is negative because we are inverse moving the
       * block to be under the mouse */
      offset[0] = -(bt->rect.xmin + 0.8f * BLI_rctf_size_x(&bt->rect));
      offset[1] = -(bt->rect.ymin + 0.5f * UI_UNIT_Y);

      if (ui_but_is_editable(bt)) {
        but_activate = bt;
      }
    }
    else {
      /* position mouse at 0.8*width of the button and below the tile
       * on the first item */
      offset[0] = 0;
      for (bt = block->buttons.first; bt; bt = bt->next) {
        offset[0] = min_ii(offset[0], -(bt->rect.xmin + 0.8f * BLI_rctf_size_x(&bt->rect)));
      }

      offset[1] = 2.1 * UI_UNIT_Y;

      for (bt = block->buttons.first; bt; bt = bt->next) {
        if (ui_but_is_editable(bt)) {
          but_activate = bt;
          break;
        }
      }
    }

    /* in rare cases this is needed since moving the popup
     * to be within the window bounds may move it away from the mouse,
     * This ensures we set an item to be active. */
    if (but_activate) {
      ui_but_activate_over(C, handle->region, but_activate);
    }

    block->minbounds = minwidth;
    UI_block_bounds_set_menu(block, 1, offset);
  }
  else {
    /* for a header menu we set the direction automatic */
    if (!pup->slideout && flip) {
      ScrArea *area = CTX_wm_area(C);
      ARegion *region = CTX_wm_region(C);
      if (area && region) {
        if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
          if (RGN_ALIGN_ENUM_FROM_MASK(ED_area_header_alignment(area)) == RGN_ALIGN_BOTTOM) {
            UI_block_direction_set(block, UI_DIR_UP);
            UI_block_order_flip(block);
          }
        }
        if (region->regiontype == RGN_TYPE_FOOTER) {
          if (RGN_ALIGN_ENUM_FROM_MASK(ED_area_footer_alignment(area)) == RGN_ALIGN_BOTTOM) {
            UI_block_direction_set(block, UI_DIR_UP);
            UI_block_order_flip(block);
          }
        }
      }
    }

    block->minbounds = minwidth;
    UI_block_bounds_set_text(block, 3.0f * UI_UNIT_X);
  }

  /* if menu slides out of other menu, override direction */
  if (pup->slideout) {
    UI_block_direction_set(block, UI_DIR_RIGHT);
  }

  return pup->block;
}

uiPopupBlockHandle *ui_popup_menu_create(
    bContext *C, ARegion *butregion, uiBut *but, uiMenuCreateFunc menu_func, void *arg)
{
  wmWindow *window = CTX_wm_window(C);
  const uiStyle *style = UI_style_get_dpi();
  uiPopupBlockHandle *handle;
  uiPopupMenu *pup;

  pup = MEM_callocN(sizeof(uiPopupMenu), __func__);
  pup->block = UI_block_begin(C, NULL, __func__, UI_EMBOSS_PULLDOWN);
  pup->block->flag |= UI_BLOCK_NUMSELECT; /* default menus to numselect */
  pup->layout = UI_block_layout(
      pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, 200, 0, UI_MENU_PADDING, style);
  pup->slideout = but ? ui_block_is_menu(but->block) : false;
  pup->but = but;
  uiLayoutSetOperatorContext(pup->layout, WM_OP_INVOKE_REGION_WIN);

  if (!but) {
    /* no button to start from, means we are a popup */
    pup->mx = window->eventstate->x;
    pup->my = window->eventstate->y;
    pup->popup = true;
    pup->block->flag |= UI_BLOCK_NO_FLIP;
  }
  /* some enums reversing is strange, currently we have no good way to
   * reverse some enum's but not others, so reverse all so the first menu
   * items are always close to the mouse cursor */
  else {
#if 0
    /* if this is an rna button then we can assume its an enum
     * flipping enums is generally not good since the order can be
     * important [#28786] */
    if (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_ENUM) {
      pup->block->flag |= UI_BLOCK_NO_FLIP;
    }
#endif
    if (but->context) {
      uiLayoutContextCopy(pup->layout, but->context);
    }
  }

  /* menu is created from a callback */
  pup->menu_func = menu_func;
  pup->menu_arg = arg;

  handle = ui_popup_block_create(C, butregion, but, NULL, ui_block_func_POPUP, pup, NULL);

  if (!but) {
    handle->popup = true;

    UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
    WM_event_add_mousemove(window);
  }

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
uiPopupMenu *UI_popup_menu_begin_ex(bContext *C,
                                    const char *title,
                                    const char *block_name,
                                    int icon)
{
  const uiStyle *style = UI_style_get_dpi();
  uiPopupMenu *pup = MEM_callocN(sizeof(uiPopupMenu), "popup menu");
  uiBut *but;

  pup->block = UI_block_begin(C, NULL, block_name, UI_EMBOSS_PULLDOWN);
  pup->block->flag |= UI_BLOCK_POPUP_MEMORY | UI_BLOCK_IS_FLIP;
  pup->block->puphash = ui_popup_menu_hash(title);
  pup->layout = UI_block_layout(
      pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, 200, 0, UI_MENU_PADDING, style);

  /* note, this intentionally differs from the menu & submenu default because many operators
   * use popups like this to select one of their options -
   * where having invoke doesn't make sense */
  uiLayoutSetOperatorContext(pup->layout, WM_OP_EXEC_REGION_WIN);

  /* create in advance so we can let buttons point to retval already */
  pup->block->handle = MEM_callocN(sizeof(uiPopupBlockHandle), "uiPopupBlockHandle");

  /* create title button */
  if (title[0]) {
    char titlestr[256];

    if (icon) {
      BLI_snprintf(titlestr, sizeof(titlestr), " %s", title);
      uiDefIconTextBut(pup->block,
                       UI_BTYPE_LABEL,
                       0,
                       icon,
                       titlestr,
                       0,
                       0,
                       200,
                       UI_UNIT_Y,
                       NULL,
                       0.0,
                       0.0,
                       0,
                       0,
                       "");
    }
    else {
      but = uiDefBut(
          pup->block, UI_BTYPE_LABEL, 0, title, 0, 0, 200, UI_UNIT_Y, NULL, 0.0, 0.0, 0, 0, "");
      but->drawflag = UI_BUT_TEXT_LEFT;
    }

    uiItemS(pup->layout);
  }

  return pup;
}

uiPopupMenu *UI_popup_menu_begin(bContext *C, const char *title, int icon)
{
  return UI_popup_menu_begin_ex(C, title, __func__, icon);
}

/**
 * Setting the button makes the popup open from the button instead of the cursor.
 */
void UI_popup_menu_but_set(uiPopupMenu *pup, struct ARegion *butregion, uiBut *but)
{
  pup->but = but;
  pup->butregion = butregion;
}

/* set the whole structure to work */
void UI_popup_menu_end(bContext *C, uiPopupMenu *pup)
{
  wmWindow *window = CTX_wm_window(C);
  uiPopupBlockHandle *menu;
  uiBut *but = NULL;
  ARegion *butregion = NULL;

  pup->popup = true;
  pup->mx = window->eventstate->x;
  pup->my = window->eventstate->y;

  if (pup->but) {
    but = pup->but;
    butregion = pup->butregion;
  }

  menu = ui_popup_block_create(C, butregion, but, NULL, ui_block_func_POPUP, pup, NULL);
  menu->popup = true;

  UI_popup_handlers_add(C, &window->modalhandlers, menu, 0);
  WM_event_add_mousemove(window);

  MEM_freeN(pup);
}

bool UI_popup_menu_end_or_cancel(bContext *C, uiPopupMenu *pup)
{
  if (!UI_block_is_empty_ex(pup->block, true)) {
    UI_popup_menu_end(C, pup);
    return true;
  }
  else {
    UI_block_layout_resolve(pup->block, NULL, NULL);
    MEM_freeN(pup->block->handle);
    UI_block_free(C, pup->block);
    MEM_freeN(pup);
    return false;
  }
}

uiLayout *UI_popup_menu_layout(uiPopupMenu *pup)
{
  return pup->layout;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Standard Popup Menus
 * \{ */

void UI_popup_menu_reports(bContext *C, ReportList *reports)
{
  Report *report;

  uiPopupMenu *pup = NULL;
  uiLayout *layout;

  if (!CTX_wm_window(C)) {
    return;
  }

  for (report = reports->list.first; report; report = report->next) {
    int icon;
    const char *msg, *msg_next;

    if (report->type < reports->printlevel) {
      continue;
    }

    if (pup == NULL) {
      char title[UI_MAX_DRAW_STR];
      BLI_snprintf(title, sizeof(title), "%s: %s", IFACE_("Report"), report->typestr);
      /* popup_menu stuff does just what we need (but pass meaningful block name) */
      pup = UI_popup_menu_begin_ex(C, title, __func__, ICON_NONE);
      layout = UI_popup_menu_layout(pup);
    }
    else {
      uiItemS(layout);
    }

    /* split each newline into a label */
    msg = report->message;
    icon = UI_icon_from_report_type(report->type);
    do {
      char buf[UI_MAX_DRAW_STR];
      msg_next = strchr(msg, '\n');
      if (msg_next) {
        msg_next++;
        BLI_strncpy(buf, msg, MIN2(sizeof(buf), msg_next - msg));
        msg = buf;
      }
      uiItemL(layout, msg, icon);
      icon = ICON_NONE;
    } while ((msg = msg_next) && *msg);
  }

  if (pup) {
    UI_popup_menu_end(C, pup);
  }
}

int UI_popup_menu_invoke(bContext *C, const char *idname, ReportList *reports)
{
  uiPopupMenu *pup;
  uiLayout *layout;
  MenuType *mt = WM_menutype_find(idname, true);

  if (mt == NULL) {
    BKE_reportf(reports, RPT_ERROR, "Menu \"%s\" not found", idname);
    return OPERATOR_CANCELLED;
  }

  if (WM_menutype_poll(C, mt) == false) {
    /* cancel but allow event to pass through, just like operators do */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  pup = UI_popup_menu_begin(C, IFACE_(mt->label), ICON_NONE);
  layout = UI_popup_menu_layout(pup);

  UI_menutype_draw(C, mt, layout);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Popup Block API
 * \{ */

void UI_popup_block_invoke_ex(
    bContext *C, uiBlockCreateFunc func, void *arg, void (*arg_free)(void *arg), bool can_refresh)
{
  wmWindow *window = CTX_wm_window(C);
  uiPopupBlockHandle *handle;

  handle = ui_popup_block_create(C, NULL, NULL, func, NULL, arg, arg_free);
  handle->popup = true;

  /* It can be useful to disable refresh (even though it will work)
   * as this exists text fields which can be disruptive if refresh isn't needed. */
  handle->can_refresh = can_refresh;

  UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
  UI_block_active_only_flagged_buttons(C, handle->region, handle->region->uiblocks.first);
  WM_event_add_mousemove(window);
}

void UI_popup_block_invoke(bContext *C,
                           uiBlockCreateFunc func,
                           void *arg,
                           void (*arg_free)(void *arg))
{
  UI_popup_block_invoke_ex(C, func, arg, arg_free, true);
}

void UI_popup_block_ex(bContext *C,
                       uiBlockCreateFunc func,
                       uiBlockHandleFunc popup_func,
                       uiBlockCancelFunc cancel_func,
                       void *arg,
                       wmOperator *op)
{
  wmWindow *window = CTX_wm_window(C);
  uiPopupBlockHandle *handle;

  handle = ui_popup_block_create(C, NULL, NULL, func, NULL, arg, NULL);
  handle->popup = true;
  handle->retvalue = 1;
  handle->can_refresh = true;

  handle->popup_op = op;
  handle->popup_arg = arg;
  handle->popup_func = popup_func;
  handle->cancel_func = cancel_func;
  // handle->opcontext = opcontext;

  UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
  UI_block_active_only_flagged_buttons(C, handle->region, handle->region->uiblocks.first);
  WM_event_add_mousemove(window);
}

#if 0 /* UNUSED */
void uiPupBlockOperator(bContext *C, uiBlockCreateFunc func, wmOperator *op, int opcontext)
{
  wmWindow *window = CTX_wm_window(C);
  uiPopupBlockHandle *handle;

  handle = ui_popup_block_create(C, NULL, NULL, func, NULL, op, NULL);
  handle->popup = 1;
  handle->retvalue = 1;
  handle->can_refresh = true;

  handle->popup_arg = op;
  handle->popup_func = operator_cb;
  handle->cancel_func = confirm_cancel_operator;
  handle->opcontext = opcontext;

  UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
  WM_event_add_mousemove(C);
}
#endif

void UI_popup_block_close(bContext *C, wmWindow *win, uiBlock *block)
{
  /* if loading new .blend while popup is open, window will be NULL */
  if (block->handle) {
    if (win) {
      const bScreen *screen = WM_window_get_active_screen(win);

      UI_popup_handlers_remove(&win->modalhandlers, block->handle);
      ui_popup_block_free(C, block->handle);

      /* In the case we have nested popups,
       * closing one may need to redraw another, see: T48874 */
      LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
        ED_region_tag_refresh_ui(region);
      }
    }
  }
}

bool UI_popup_block_name_exists(const bScreen *screen, const char *name)
{
  LISTBASE_FOREACH (const ARegion *, region, &screen->regionbase) {
    LISTBASE_FOREACH (const uiBlock *, block, &region->uiblocks) {
      if (STREQ(block->name, name)) {
        return true;
      }
    }
  }
  return false;
}

/** \} */
