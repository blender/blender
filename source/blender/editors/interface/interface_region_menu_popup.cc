/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * PopUp Menu Region
 */

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"

#include "BLT_translation.h"

#include "ED_screen.hh"

#include "interface_intern.hh"
#include "interface_regions_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

bool ui_but_menu_step_poll(const uiBut *but)
{
  BLI_assert(but->type == UI_BTYPE_MENU);

  /* currently only RNA buttons */
  return ((but->menu_step_func != nullptr) ||
          (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_ENUM));
}

int ui_but_menu_step(uiBut *but, int direction)
{
  if (ui_but_menu_step_poll(but)) {
    if (but->menu_step_func) {
      return but->menu_step_func(
          static_cast<bContext *>(but->block->evil_C), direction, but->poin);
    }

    const int curval = RNA_property_enum_get(&but->rnapoin, but->rnaprop);
    return RNA_property_enum_step(static_cast<bContext *>(but->block->evil_C),
                                  &but->rnapoin,
                                  but->rnaprop,
                                  curval,
                                  direction);
  }

  printf("%s: cannot cycle button '%s'\n", __func__, but->str);
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Popup Menu Memory
 *
 * Support menu-memory, a feature that positions the cursor
 * over the previously used menu item.
 *
 * \note This is stored for each unique menu title.
 * \{ */

static uint ui_popup_string_hash(const char *str, const bool use_sep)
{
  /* sometimes button contains hotkey, sometimes not, strip for proper compare */
  int hash;
  const char *delimit = use_sep ? strrchr(str, UI_SEP_CHAR) : nullptr;

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

/* but == nullptr read, otherwise set */
static uiBut *ui_popup_menu_memory__internal(uiBlock *block, uiBut *but)
{
  static uint mem[256];
  static bool first = true;

  const uint hash = block->puphash;
  const uint hash_mod = hash & 255;

  if (first) {
    /* init */
    memset(mem, -1, sizeof(mem));
    first = false;
  }

  if (but) {
    /* set */
    mem[hash_mod] = ui_popup_string_hash(but->str, but->flag & UI_BUT_HAS_SEP_CHAR);
    return nullptr;
  }

  /* get */
  LISTBASE_FOREACH (uiBut *, but_iter, &block->buttons) {
    /* Prevent labels (typically headings), from being returned in the case the text
     * happens to matches one of the menu items.
     * Skip separators too as checking them is redundant. */
    if (ELEM(but_iter->type, UI_BTYPE_LABEL, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE)) {
      continue;
    }
    if (mem[hash_mod] == ui_popup_string_hash(but_iter->str, but_iter->flag & UI_BUT_HAS_SEP_CHAR))
    {
      return but_iter;
    }
  }

  return nullptr;
}

uiBut *ui_popup_menu_memory_get(uiBlock *block)
{
  return ui_popup_menu_memory__internal(block, nullptr);
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

  /* Menu hash is created from this, to keep a memory of recently opened menus. */
  const char *title;

  int mx, my;
  bool popup, slideout;

  std::function<void(bContext *C, uiLayout *layout)> menu_func;
};

/**
 * \param title: Optional. If set, it will be used to store recently opened menus so they can be
 *               opened with the mouse over the last chosen entry again.
 */
static void ui_popup_menu_create_block(bContext *C,
                                       uiPopupMenu *pup,
                                       const char *title,
                                       const char *block_name)
{
  const uiStyle *style = UI_style_get_dpi();

  pup->block = UI_block_begin(C, nullptr, block_name, UI_EMBOSS_PULLDOWN);
  if (!pup->but) {
    pup->block->flag |= UI_BLOCK_NO_FLIP;
  }
  /* A title is only provided when a Menu has a label, this is not always the case, see e.g.
   * `VIEW3D_MT_edit_mesh_context_menu` -- this specifies its own label inside the draw function
   * depending on vertex/edge/face mode. We still want to flag the uiBlock (but only insert into
   * the `puphash` if we have a title provided). Choosing an entry in a menu will still handle
   * `puphash` later (see `button_activate_exit`) though multiple menus without a label might fight
   * for the same storage of the menu memory. Using idname instead (or in combination with the
   * label) for the hash could be looked at to solve this. */
  pup->block->flag |= UI_BLOCK_POPUP_MEMORY;
  if (title && title[0]) {
    pup->block->puphash = ui_popup_menu_hash(title);
  }
  pup->layout = UI_block_layout(
      pup->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, 200, 0, UI_MENU_PADDING, style);

  /* NOTE: this intentionally differs from the menu & sub-menu default because many operators
   * use popups like this to select one of their options -
   * where having invoke doesn't make sense.
   * When the menu was opened from a button, use invoke still for compatibility. This used to be
   * the default and changing now could cause issues. */
  const wmOperatorCallContext opcontext = pup->but ? WM_OP_INVOKE_REGION_WIN :
                                                     WM_OP_EXEC_REGION_WIN;

  uiLayoutSetOperatorContext(pup->layout, opcontext);

  if (pup->but) {
    if (pup->but->context) {
      uiLayoutContextCopy(pup->layout, pup->but->context);
    }
  }
}

static uiBlock *ui_block_func_POPUP(bContext *C, uiPopupBlockHandle *handle, void *arg_pup)
{
  uiPopupMenu *pup = static_cast<uiPopupMenu *>(arg_pup);

  int minwidth = 0;

  if (!pup->layout) {
    ui_popup_menu_create_block(C, pup, pup->title, __func__);

    if (pup->menu_func) {
      pup->block->handle = handle;
      pup->menu_func(C, pup->layout);
      pup->block->handle = nullptr;
    }

    if (uiLayoutGetUnitsX(pup->layout) != 0.0f) {
      /* Use the minimum width from the layout if it's set. */
      minwidth = uiLayoutGetUnitsX(pup->layout) * UI_UNIT_X;
    }

    pup->layout = nullptr;
  }

  /* Find block minimum width. */
  if (minwidth) {
    /* Skip. */
  }
  else if (pup->but) {
    /* Minimum width to enforce. */
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
  char direction;
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

  bool flip = (direction == UI_DIR_DOWN);

  uiBlock *block = pup->block;

  /* in some cases we create the block before the region,
   * so we set it delayed here if necessary */
  if (BLI_findindex(&handle->region->uiblocks, block) == -1) {
    UI_block_region_set(block, handle->region);
  }

  block->direction = direction;

  int width, height;
  UI_block_layout_resolve(block, &width, &height);

  UI_block_flag_enable(block, UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_NUMSELECT);

  if (pup->popup) {
    int offset[2] = {0, 0};

    uiBut *but_activate = nullptr;
    UI_block_flag_enable(block, UI_BLOCK_LOOP);
    UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
    UI_block_direction_set(block, direction);

    /* offset the mouse position, possibly based on earlier selection */
    if (!handle->refresh) {
      uiBut *bt;
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
        LISTBASE_FOREACH (uiBut *, but_iter, &block->buttons) {
          offset[0] = min_ii(offset[0],
                             -(but_iter->rect.xmin + 0.8f * BLI_rctf_size_x(&but_iter->rect)));
        }

        offset[1] = 2.1 * UI_UNIT_Y;

        LISTBASE_FOREACH (uiBut *, but_iter, &block->buttons) {
          if (ui_but_is_editable(but_iter)) {
            but_activate = but_iter;
            break;
          }
        }
      }
      copy_v2_v2_int(handle->prev_bounds_offset, offset);
    }
    else {
      copy_v2_v2_int(offset, handle->prev_bounds_offset);
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
      ARegion *region = CTX_wm_region(C);
      if (region) {
        if (RGN_TYPE_IS_HEADER_ANY(region->regiontype)) {
          if (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_BOTTOM) {
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

static void ui_block_free_func_POPUP(void *arg_pup)
{
  uiPopupMenu *pup = static_cast<uiPopupMenu *>(arg_pup);
  MEM_delete(pup);
}

static uiPopupBlockHandle *ui_popup_menu_create(
    bContext *C,
    ARegion *butregion,
    uiBut *but,
    const char *title,
    std::function<void(bContext *, uiLayout *)> menu_func)
{
  wmWindow *window = CTX_wm_window(C);

  uiPopupMenu *pup = MEM_new<uiPopupMenu>(__func__);
  pup->title = title;
  /* menu is created from a callback */
  pup->menu_func = menu_func;
  if (but) {
    pup->slideout = ui_block_is_menu(but->block);
    pup->but = but;
  }

  if (!but) {
    /* no button to start from, means we are a popup */
    pup->mx = window->eventstate->xy[0];
    pup->my = window->eventstate->xy[1];
    pup->popup = true;
  }
  /* some enums reversing is strange, currently we have no good way to
   * reverse some enum's but not others, so reverse all so the first menu
   * items are always close to the mouse cursor */
  else {
#if 0
    /* if this is an rna button then we can assume its an enum
     * flipping enums is generally not good since the order can be
     * important #28786. */
    if (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_ENUM) {
      pup->block->flag |= UI_BLOCK_NO_FLIP;
    }
#endif
  }

  uiPopupBlockHandle *handle = ui_popup_block_create(
      C, butregion, but, nullptr, ui_block_func_POPUP, pup, ui_block_free_func_POPUP);

  if (!but) {
    handle->popup = true;

    UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
    WM_event_add_mousemove(window);
  }

  return handle;
}

uiPopupBlockHandle *ui_popup_menu_create(
    bContext *C, ARegion *butregion, uiBut *but, uiMenuCreateFunc menu_func, void *arg)
{
  return ui_popup_menu_create(
      C, butregion, but, nullptr, [menu_func, arg](bContext *C, uiLayout *layout) {
        menu_func(C, layout, arg);
      });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Popup Menu API with begin & end
 * \{ */

static void create_title_button(uiLayout *layout, const char *title, int icon)
{
  uiBlock *block = uiLayoutGetBlock(layout);
  char titlestr[256];

  if (icon) {
    SNPRINTF(titlestr, " %s", title);
    uiDefIconTextBut(block,
                     UI_BTYPE_LABEL,
                     0,
                     icon,
                     titlestr,
                     0,
                     0,
                     200,
                     UI_UNIT_Y,
                     nullptr,
                     0.0,
                     0.0,
                     0,
                     0,
                     "");
  }
  else {
    uiBut *but = uiDefBut(
        block, UI_BTYPE_LABEL, 0, title, 0, 0, 200, UI_UNIT_Y, nullptr, 0.0, 0.0, 0, 0, "");
    but->drawflag = UI_BUT_TEXT_LEFT;
  }

  uiItemS(layout);
}

uiPopupMenu *UI_popup_menu_begin_ex(bContext *C,
                                    const char *title,
                                    const char *block_name,
                                    int icon)
{
  uiPopupMenu *pup = MEM_new<uiPopupMenu>(__func__);

  pup->title = title;

  ui_popup_menu_create_block(C, pup, title, block_name);
  /* Further buttons will be laid out top to bottom by default. */
  pup->block->flag |= UI_BLOCK_IS_FLIP;

  /* create in advance so we can let buttons point to retval already */
  pup->block->handle = MEM_cnew<uiPopupBlockHandle>(__func__);

  if (title[0]) {
    create_title_button(pup->layout, title, icon);
  }

  return pup;
}

uiPopupMenu *UI_popup_menu_begin(bContext *C, const char *title, int icon)
{
  return UI_popup_menu_begin_ex(C, title, __func__, icon);
}

void UI_popup_menu_but_set(uiPopupMenu *pup, ARegion *butregion, uiBut *but)
{
  pup->but = but;
  pup->butregion = butregion;
}

void UI_popup_menu_end(bContext *C, uiPopupMenu *pup)
{
  wmWindow *window = CTX_wm_window(C);

  pup->popup = true;
  pup->mx = window->eventstate->xy[0];
  pup->my = window->eventstate->xy[1];

  uiBut *but = nullptr;
  ARegion *butregion = nullptr;
  if (pup->but) {
    but = pup->but;
    butregion = pup->butregion;
  }

  uiPopupBlockHandle *menu = ui_popup_block_create(
      C, butregion, but, nullptr, ui_block_func_POPUP, pup, nullptr);
  menu->popup = true;

  UI_popup_handlers_add(C, &window->modalhandlers, menu, 0);
  WM_event_add_mousemove(window);

  MEM_delete(pup);
}

bool UI_popup_menu_end_or_cancel(bContext *C, uiPopupMenu *pup)
{
  if (!UI_block_is_empty_ex(pup->block, true)) {
    UI_popup_menu_end(C, pup);
    return true;
  }
  UI_block_layout_resolve(pup->block, nullptr, nullptr);
  MEM_freeN(pup->block->handle);
  UI_block_free(C, pup->block);
  MEM_delete(pup);
  return false;
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
  uiPopupMenu *pup = nullptr;
  uiLayout *layout;

  if (!CTX_wm_window(C)) {
    return;
  }

  LISTBASE_FOREACH (Report *, report, &reports->list) {
    int icon;
    const char *msg, *msg_next;

    if (report->type < reports->printlevel) {
      continue;
    }

    if (pup == nullptr) {
      char title[UI_MAX_DRAW_STR];
      SNPRINTF(title, "%s: %s", IFACE_("Report"), report->typestr);
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

static void ui_popup_menu_create_from_menutype(bContext *C,
                                               MenuType *mt,
                                               const char *title,
                                               const int icon)
{
  uiPopupBlockHandle *handle = ui_popup_menu_create(
      C, nullptr, nullptr, title, [mt, title, icon](bContext *C, uiLayout *layout) -> void {
        if (title && title[0]) {
          create_title_button(layout, title, icon);
        }
        ui_item_menutype_func(C, layout, mt);
      });

  handle->can_refresh = true;
}

int UI_popup_menu_invoke(bContext *C, const char *idname, ReportList *reports)
{
  MenuType *mt = WM_menutype_find(idname, true);

  if (mt == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "Menu \"%s\" not found", idname);
    return OPERATOR_CANCELLED;
  }

  if (WM_menutype_poll(C, mt) == false) {
    /* cancel but allow event to pass through, just like operators do */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }
  /* For now always recreate menus on redraw that were invoked with this function. Maybe we want to
   * make that optional somehow. */
  const bool allow_refresh = true;

  const char *title = CTX_IFACE_(mt->translation_context, mt->label);
  if (allow_refresh) {
    ui_popup_menu_create_from_menutype(C, mt, title, ICON_NONE);
  }
  else {
    /* If no refresh is needed, create the block directly. */
    uiPopupMenu *pup = UI_popup_menu_begin(C, title, ICON_NONE);
    uiLayout *layout = UI_popup_menu_layout(pup);
    UI_menutype_draw(C, mt, layout);
    UI_popup_menu_end(C, pup);
  }

  return OPERATOR_INTERFACE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Popup Block API
 * \{ */

void UI_popup_block_invoke_ex(
    bContext *C, uiBlockCreateFunc func, void *arg, uiFreeArgFunc arg_free, bool can_refresh)
{
  wmWindow *window = CTX_wm_window(C);

  uiPopupBlockHandle *handle = ui_popup_block_create(
      C, nullptr, nullptr, func, nullptr, arg, arg_free);
  handle->popup = true;

  /* It can be useful to disable refresh (even though it will work)
   * as this exists text fields which can be disruptive if refresh isn't needed. */
  handle->can_refresh = can_refresh;

  UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
  UI_block_active_only_flagged_buttons(
      C, handle->region, static_cast<uiBlock *>(handle->region->uiblocks.first));
  WM_event_add_mousemove(window);
}

void UI_popup_block_invoke(bContext *C, uiBlockCreateFunc func, void *arg, uiFreeArgFunc arg_free)
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

  uiPopupBlockHandle *handle = ui_popup_block_create(
      C, nullptr, nullptr, func, nullptr, arg, nullptr);
  handle->popup = true;
  handle->retvalue = 1;
  handle->can_refresh = true;

  handle->popup_op = op;
  handle->popup_arg = arg;
  handle->popup_func = popup_func;
  handle->cancel_func = cancel_func;
  // handle->opcontext = opcontext;

  UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
  UI_block_active_only_flagged_buttons(
      C, handle->region, static_cast<uiBlock *>(handle->region->uiblocks.first));
  WM_event_add_mousemove(window);
}

#if 0 /* UNUSED */
void uiPupBlockOperator(bContext *C,
                        uiBlockCreateFunc func,
                        wmOperator *op,
                        wmOperatorCallContext opcontext)
{
  wmWindow *window = CTX_wm_window(C);
  uiPopupBlockHandle *handle;

  handle = ui_popup_block_create(C, nullptr, nullptr, func, nullptr, op, nullptr);
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
  /* if loading new .blend while popup is open, window will be nullptr */
  if (block->handle) {
    if (win) {
      const bScreen *screen = WM_window_get_active_screen(win);

      UI_popup_handlers_remove(&win->modalhandlers, block->handle);
      ui_popup_block_free(C, block->handle);

      /* In the case we have nested popups,
       * closing one may need to redraw another, see: #48874 */
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
