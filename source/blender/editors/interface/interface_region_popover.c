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
#include "BLI_math_vector.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_report.h"

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

  /* Needed for keymap removal. */
  wmWindow *window;
  wmKeyMap *keymap;
  struct wmEventHandler_Keymap *keymap_handler;

  uiMenuCreateFunc menu_func;
  void *menu_arg;

  /* Size in pixels (ui scale applied). */
  int ui_size_x;

#ifdef USE_UI_POPOVER_ONCE
  bool is_once;
#endif
};

static void ui_popover_create_block(bContext *C, uiPopover *pup, int opcontext)
{
  BLI_assert(pup->ui_size_x != 0);

  uiStyle *style = UI_style_get_dpi();

  pup->block = UI_block_begin(C, NULL, __func__, UI_EMBOSS);
  UI_block_flag_enable(pup->block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_POPOVER);
#ifdef USE_UI_POPOVER_ONCE
  if (pup->is_once) {
    UI_block_flag_enable(pup->block, UI_BLOCK_POPOVER_ONCE);
  }
#endif

  pup->layout = UI_block_layout(pup->block,
                                UI_LAYOUT_VERTICAL,
                                UI_LAYOUT_PANEL,
                                0,
                                0,
                                pup->ui_size_x,
                                0,
                                UI_MENU_PADDING,
                                style);

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
  UI_block_direction_set(block, UI_DIR_DOWN | UI_DIR_CENTER_X);

  const int block_margin = U.widget_unit / 2;

  if (pup->but) {
    /* For a header menu we set the direction automatic. */
    block->minbounds = BLI_rctf_size_x(&pup->but->rect);
    UI_block_bounds_set_normal(block, block_margin);

    /* If menu slides out of other menu, override direction. */
    bool slideout = ui_block_is_menu(pup->but->block);
    if (slideout) {
      UI_block_direction_set(block, UI_DIR_RIGHT);
    }

    /* Store the button location for positioning the popover arrow hint. */
    if (!handle->refresh) {
      float center[2] = {BLI_rctf_cent_x(&pup->but->rect), BLI_rctf_cent_y(&pup->but->rect)};
      ui_block_to_window_fl(handle->ctx_region, pup->but->block, &center[0], &center[1]);
      /* These variables aren't used for popovers,
       * we could add new variables if there is a conflict. */
      block->bounds_offset[0] = (int)center[0];
      block->bounds_offset[1] = (int)center[1];
      copy_v2_v2_int(handle->prev_bounds_offset, block->bounds_offset);
    }
    else {
      copy_v2_v2_int(block->bounds_offset, handle->prev_bounds_offset);
    }

    if (!slideout) {
      ScrArea *sa = CTX_wm_area(C);
      ARegion *ar = CTX_wm_region(C);

      if (ar && ar->panels.first) {
        /* For regions with panels, prefer to open to top so we can
         * see the values of the buttons below changing. */
        UI_block_direction_set(block, UI_DIR_UP | UI_DIR_CENTER_X);
      }
      /* Prefer popover from header to be positioned into the editor. */
      else if (sa && ar) {
        if (ELEM(ar->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
          if (ED_area_header_alignment(sa) == RGN_ALIGN_BOTTOM) {
            UI_block_direction_set(block, UI_DIR_UP | UI_DIR_CENTER_X);
          }
        }
        if (ar->regiontype == RGN_TYPE_FOOTER) {
          if (ED_area_footer_alignment(sa) == RGN_ALIGN_BOTTOM) {
            UI_block_direction_set(block, UI_DIR_UP | UI_DIR_CENTER_X);
          }
        }
      }
    }

    /* Estimated a maximum size so we don't go offscreen for low height
     * areas near the bottom of the window on refreshes. */
    handle->max_size_y = UI_UNIT_Y * 16.0f;
  }
  else {
    /* Not attached to a button. */
    int bounds_offset[2] = {0, 0};
    UI_block_flag_enable(block, UI_BLOCK_LOOP);
    UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
    UI_block_direction_set(block, block->direction);
    block->minbounds = UI_MENU_WIDTH_MIN;

    if (!handle->refresh) {
      uiBut *but = NULL;
      uiBut *but_first = NULL;
      for (but = block->buttons.first; but; but = but->next) {
        if ((but_first == NULL) && ui_but_is_editable(but)) {
          but_first = but;
        }
        if (but->flag & (UI_SELECT | UI_SELECT_DRAW)) {
          break;
        }
      }

      if (but) {
        bounds_offset[0] = -(but->rect.xmin + 0.8f * BLI_rctf_size_x(&but->rect));
        bounds_offset[1] = -BLI_rctf_cent_y(&but->rect);
      }
      else {
        bounds_offset[0] = -(pup->ui_size_x / 2);
        bounds_offset[1] = but_first ? -BLI_rctf_cent_y(&but_first->rect) : (UI_UNIT_Y / 2);
      }
      copy_v2_v2_int(handle->prev_bounds_offset, bounds_offset);
    }
    else {
      copy_v2_v2_int(bounds_offset, handle->prev_bounds_offset);
    }

    UI_block_bounds_set_popup(block, block_margin, bounds_offset);
  }

  return block;
}

static void ui_block_free_func_POPOVER(void *arg_pup)
{
  uiPopover *pup = arg_pup;
  if (pup->keymap != NULL) {
    wmWindow *window = pup->window;
    WM_event_remove_keymap_handler(&window->modalhandlers, pup->keymap);
  }
  MEM_freeN(pup);
}

uiPopupBlockHandle *ui_popover_panel_create(
    bContext *C, ARegion *butregion, uiBut *but, uiMenuCreateFunc menu_func, void *arg)
{
  wmWindow *window = CTX_wm_window(C);

  /* Create popover, buttons are created from callback. */
  uiPopover *pup = MEM_callocN(sizeof(uiPopover), __func__);
  pup->but = but;

  /* FIXME: maybe one day we want non panel popovers? */
  {
    int ui_units_x = ((PanelType *)arg)->ui_units_x;
    pup->ui_size_x = U.widget_unit * (ui_units_x ? ui_units_x : UI_POPOVER_WIDTH_UNITS);
  }

  pup->menu_func = menu_func;
  pup->menu_arg = arg;

#ifdef USE_UI_POPOVER_ONCE
  {
    /* Ideally this would be passed in. */
    const wmEvent *event = window->eventstate;
    pup->is_once = (event->type == LEFTMOUSE) && (event->val == KM_PRESS);
  }
#endif

  /* Create popup block. */
  uiPopupBlockHandle *handle;
  handle = ui_popup_block_create(
      C, butregion, but, NULL, ui_block_func_POPOVER, pup, ui_block_free_func_POPOVER);
  handle->can_refresh = true;

  /* Add handlers. If attached to a button, the button will already
   * add a modal handler and pass on events. */
  if (!but) {
    UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
    WM_event_add_mousemove(C);
    handle->popup = true;
  }

  return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Standard Popover Panels
 * \{ */

int UI_popover_panel_invoke(bContext *C, const char *idname, bool keep_open, ReportList *reports)
{
  uiLayout *layout;
  PanelType *pt = WM_paneltype_find(idname, true);
  if (pt == NULL) {
    BKE_reportf(reports, RPT_ERROR, "Panel \"%s\" not found", idname);
    return OPERATOR_CANCELLED;
  }

  if (pt->poll && (pt->poll(C, pt) == false)) {
    /* cancel but allow event to pass through, just like operators do */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  uiBlock *block = NULL;
  if (keep_open) {
    uiPopupBlockHandle *handle = ui_popover_panel_create(
        C, NULL, NULL, ui_item_paneltype_func, pt);
    uiPopover *pup = handle->popup_create_vars.arg;
    block = pup->block;
  }
  else {
    uiPopover *pup = UI_popover_begin(C, U.widget_unit * pt->ui_units_x);
    layout = UI_popover_layout(pup);
    UI_paneltype_draw(C, pt, layout);
    UI_popover_end(C, pup, NULL);
    block = pup->block;
  }

  if (block) {
    UI_block_active_only_flagged_buttons(C, CTX_wm_region(C), block);
  }
  return OPERATOR_INTERFACE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Popup Menu API with begin & end
 * \{ */

/**
 * Only return handler, and set optional title.
 */
uiPopover *UI_popover_begin(bContext *C, int ui_size_x)
{
  uiPopover *pup = MEM_callocN(sizeof(uiPopover), "popover menu");
  if (ui_size_x == 0) {
    ui_size_x = U.widget_unit * UI_POPOVER_WIDTH_UNITS;
  }
  pup->ui_size_x = ui_size_x;

  /* Opertor context default same as menus, change if needed. */
  ui_popover_create_block(C, pup, WM_OP_EXEC_REGION_WIN);

  /* create in advance so we can let buttons point to retval already */
  pup->block->handle = MEM_callocN(sizeof(uiPopupBlockHandle), "uiPopupBlockHandle");

  return pup;
}

static void popover_keymap_fn(wmKeyMap *UNUSED(keymap), wmKeyMapItem *UNUSED(kmi), void *user_data)
{
  uiPopover *pup = user_data;
  pup->block->handle->menuretval = UI_RETURN_OK;
}

/* set the whole structure to work */
void UI_popover_end(bContext *C, uiPopover *pup, wmKeyMap *keymap)
{
  wmWindow *window = CTX_wm_window(C);
  /* Create popup block. No refresh support since the buttons were created
   * between begin/end and we have no callback to recreate them. */
  uiPopupBlockHandle *handle;

  if (keymap) {
    /* Add so we get keymaps shown in the buttons. */
    UI_block_flag_enable(pup->block, UI_BLOCK_SHOW_SHORTCUT_ALWAYS);
    pup->keymap = keymap;
    pup->keymap_handler = WM_event_add_keymap_handler_priority(&window->modalhandlers, keymap, 0);
    WM_event_set_keymap_handler_post_callback(pup->keymap_handler, popover_keymap_fn, pup);
  }

  handle = ui_popup_block_create(
      C, NULL, NULL, NULL, ui_block_func_POPOVER, pup, ui_block_free_func_POPOVER);

  /* Add handlers. */
  UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
  WM_event_add_mousemove(C);
  handle->popup = true;

  /* Re-add so it gets priority. */
  if (keymap) {
    BLI_remlink(&window->modalhandlers, pup->keymap_handler);
    BLI_addhead(&window->modalhandlers, pup->keymap_handler);
  }

  pup->window = window;

  /* TODO(campbell): we may want to make this configurable.
   * The begin/end stype of calling popups doesn't allow to 'can_refresh' to be set.
   * For now close this style of popvers when accessed. */
  UI_block_flag_disable(pup->block, UI_BLOCK_KEEP_OPEN);

  /* panels are created flipped (from event handling pov) */
  pup->block->flag ^= UI_BLOCK_IS_FLIP;
}

uiLayout *UI_popover_layout(uiPopover *pup)
{
  return pup->layout;
}

#ifdef USE_UI_POPOVER_ONCE
void UI_popover_once_clear(uiPopover *pup)
{
  pup->is_once = false;
}
#endif

/** \} */
