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
 * Search Box Region & Interaction
 */

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_ID.h"
#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_math.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_view2d.h"

#include "BLT_translation.h"

#include "ED_screen.h"

#include "GPU_state.h"
#include "interface_intern.h"
#include "interface_regions_intern.h"

#define MENU_BORDER (int)(0.3f * U.widget_unit)

/* -------------------------------------------------------------------- */
/** \name Search Box Creation
 * \{ */

struct uiSearchItems {
  int maxitem, totitem, maxstrlen;

  int offset, offset_i; /* offset for inserting in array */
  int more;             /* flag indicating there are more items */

  char **names;
  void **pointers;
  int *icons;
  int *states;

  AutoComplete *autocpl;
  void *active;
};

typedef struct uiSearchboxData {
  rcti bbox;
  uiFontStyle fstyle;
  uiSearchItems items;
  /** index in items array */
  int active;
  /** when menu opened with enough space for this */
  bool noback;
  /** draw thumbnail previews, rather than list */
  bool preview;
  /** Use the #UI_SEP_CHAR char for splitting shortcuts (good for operators, bad for data). */
  bool use_sep;
  int prv_rows, prv_cols;
  /**
   * Show the active icon and text after the last instance of this string.
   * Used so we can show leading text to menu items less prominently (not related to 'use_sep').
   */
  const char *sep_string;
} uiSearchboxData;

#define SEARCH_ITEMS 10

/**
 * Public function exported for functions that use #UI_BTYPE_SEARCH_MENU.
 *
 * \param items: Stores the items.
 * \param name: Text to display for the item.
 * \param poin: Opaque pointer (for use by the caller).
 * \param iconid: The icon, #ICON_NONE for no icon.
 * \param state: The buttons state flag, compatible with #uiBut.flag,
 * typically #UI_BUT_DISABLED / #UI_BUT_INACTIVE.
 * \return false if there is nothing to add.
 */
bool UI_search_item_add(uiSearchItems *items, const char *name, void *poin, int iconid, int state)
{
  /* hijack for autocomplete */
  if (items->autocpl) {
    UI_autocomplete_update_name(items->autocpl, name);
    return true;
  }

  /* hijack for finding active item */
  if (items->active) {
    if (poin == items->active) {
      items->offset_i = items->totitem;
    }
    items->totitem++;
    return true;
  }

  if (items->totitem >= items->maxitem) {
    items->more = 1;
    return false;
  }

  /* skip first items in list */
  if (items->offset_i > 0) {
    items->offset_i--;
    return true;
  }

  if (items->names) {
    BLI_strncpy(items->names[items->totitem], name, items->maxstrlen);
  }
  if (items->pointers) {
    items->pointers[items->totitem] = poin;
  }
  if (items->icons) {
    items->icons[items->totitem] = iconid;
  }

  /* Limit flags that can be set so flags such as 'UI_SELECT' aren't accidentally set
   * which will cause problems, add others as needed. */
  BLI_assert(
      (state & ~(UI_BUT_DISABLED | UI_BUT_INACTIVE | UI_BUT_REDALERT | UI_BUT_HAS_SEP_CHAR)) == 0);
  if (items->states) {
    items->states[items->totitem] = state;
  }

  items->totitem++;

  return true;
}

int UI_searchbox_size_y(void)
{
  return SEARCH_ITEMS * UI_UNIT_Y + 2 * UI_POPUP_MENU_TOP;
}

int UI_searchbox_size_x(void)
{
  return 12 * UI_UNIT_X;
}

int UI_search_items_find_index(uiSearchItems *items, const char *name)
{
  int i;
  for (i = 0; i < items->totitem; i++) {
    if (STREQ(name, items->names[i])) {
      return i;
    }
  }
  return -1;
}

/* region is the search box itself */
static void ui_searchbox_select(bContext *C, ARegion *region, uiBut *but, int step)
{
  uiSearchboxData *data = region->regiondata;

  /* apply step */
  data->active += step;

  if (data->items.totitem == 0) {
    data->active = -1;
  }
  else if (data->active >= data->items.totitem) {
    if (data->items.more) {
      data->items.offset++;
      data->active = data->items.totitem - 1;
      ui_searchbox_update(C, region, but, false);
    }
    else {
      data->active = data->items.totitem - 1;
    }
  }
  else if (data->active < 0) {
    if (data->items.offset) {
      data->items.offset--;
      data->active = 0;
      ui_searchbox_update(C, region, but, false);
    }
    else {
      /* only let users step into an 'unset' state for unlink buttons */
      data->active = (but->flag & UI_BUT_VALUE_CLEAR) ? -1 : 0;
    }
  }

  ED_region_tag_redraw(region);
}

static void ui_searchbox_butrect(rcti *r_rect, uiSearchboxData *data, int itemnr)
{
  /* thumbnail preview */
  if (data->preview) {
    int butw = (BLI_rcti_size_x(&data->bbox) - 2 * MENU_BORDER) / data->prv_cols;
    int buth = (BLI_rcti_size_y(&data->bbox) - 2 * MENU_BORDER) / data->prv_rows;
    int row, col;

    *r_rect = data->bbox;

    col = itemnr % data->prv_cols;
    row = itemnr / data->prv_cols;

    r_rect->xmin += MENU_BORDER + (col * butw);
    r_rect->xmax = r_rect->xmin + butw;

    r_rect->ymax -= MENU_BORDER + (row * buth);
    r_rect->ymin = r_rect->ymax - buth;
  }
  /* list view */
  else {
    int buth = (BLI_rcti_size_y(&data->bbox) - 2 * UI_POPUP_MENU_TOP) / SEARCH_ITEMS;

    *r_rect = data->bbox;
    r_rect->xmin = data->bbox.xmin + 3.0f;
    r_rect->xmax = data->bbox.xmax - 3.0f;

    r_rect->ymax = data->bbox.ymax - UI_POPUP_MENU_TOP - itemnr * buth;
    r_rect->ymin = r_rect->ymax - buth;
  }
}

int ui_searchbox_find_index(ARegion *region, const char *name)
{
  uiSearchboxData *data = region->regiondata;
  return UI_search_items_find_index(&data->items, name);
}

/* x and y in screencoords */
bool ui_searchbox_inside(ARegion *region, int x, int y)
{
  uiSearchboxData *data = region->regiondata;

  return BLI_rcti_isect_pt(&data->bbox, x - region->winrct.xmin, y - region->winrct.ymin);
}

/* string validated to be of correct length (but->hardmax) */
bool ui_searchbox_apply(uiBut *but, ARegion *region)
{
  uiSearchboxData *data = region->regiondata;

  but->func_arg2 = NULL;

  if (data->active != -1) {
    const char *name = data->items.names[data->active];
    const char *name_sep = data->use_sep ? strrchr(name, UI_SEP_CHAR) : NULL;

    BLI_strncpy(but->editstr, name, name_sep ? (name_sep - name) + 1 : data->items.maxstrlen);

    but->func_arg2 = data->items.pointers[data->active];

    return true;
  }
  else if (but->flag & UI_BUT_VALUE_CLEAR) {
    /* It is valid for _VALUE_CLEAR flavor to have no active element
     * (it's a valid way to unlink). */
    but->editstr[0] = '\0';

    return true;
  }
  else {
    return false;
  }
}

static struct ARegion *wm_searchbox_tooltip_init(struct bContext *C,
                                                 struct ARegion *region,
                                                 int *UNUSED(r_pass),
                                                 double *UNUSED(pass_delay),
                                                 bool *r_exit_on_event)
{
  *r_exit_on_event = true;

  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
      if (but->search && but->search->tooltip_fn) {
        return but->search->tooltip_fn(C, region, but->search->arg, but->func_arg2);
      }
    }
  }
  return NULL;
}

bool ui_searchbox_event(
    bContext *C, ARegion *region, uiBut *but, ARegion *butregion, const wmEvent *event)
{
  uiSearchboxData *data = region->regiondata;
  int type = event->type, val = event->val;
  bool handled = false;
  bool tooltip_timer_started = false;

  if (type == MOUSEPAN) {
    ui_pan_to_scroll(event, &type, &val);
  }

  switch (type) {
    case WHEELUPMOUSE:
    case EVT_UPARROWKEY:
      ui_searchbox_select(C, region, but, -1);
      handled = true;
      break;
    case WHEELDOWNMOUSE:
    case EVT_DOWNARROWKEY:
      ui_searchbox_select(C, region, but, 1);
      handled = true;
      break;
    case RIGHTMOUSE:
      if (val) {
        if (but->search->context_menu_fn) {
          if (data->active != -1) {
            /* Check the cursor is over the active element
             * (a little confusing if this isn't the case, although it does work). */
            rcti rect;
            ui_searchbox_butrect(&rect, data, data->active);
            if (BLI_rcti_isect_pt(
                    &rect, event->x - region->winrct.xmin, event->y - region->winrct.ymin)) {

              void *active = data->items.pointers[data->active];
              if (but->search->context_menu_fn(C, but->search->arg, active, event)) {
                handled = true;
              }
            }
          }
        }
      }
      break;
    case MOUSEMOVE: {
      bool is_inside = false;

      if (BLI_rcti_isect_pt(&region->winrct, event->x, event->y)) {
        rcti rect;
        int a;

        for (a = 0; a < data->items.totitem; a++) {
          ui_searchbox_butrect(&rect, data, a);
          if (BLI_rcti_isect_pt(
                  &rect, event->x - region->winrct.xmin, event->y - region->winrct.ymin)) {
            is_inside = true;
            if (data->active != a) {
              data->active = a;
              ui_searchbox_select(C, region, but, 0);
              handled = true;
              break;
            }
          }
        }
      }

      if (U.flag & USER_TOOLTIPS) {
        if (is_inside) {
          if (data->active != -1) {
            ScrArea *area = CTX_wm_area(C);
            but->func_arg2 = data->items.pointers[data->active];
            WM_tooltip_timer_init(C, CTX_wm_window(C), area, butregion, wm_searchbox_tooltip_init);
            tooltip_timer_started = true;
          }
        }
      }

      break;
    }
  }

  if (handled && (tooltip_timer_started == false)) {
    wmWindow *win = CTX_wm_window(C);
    WM_tooltip_clear(C, win);
  }

  return handled;
}

/** Wrap #uiButSearchUpdateFn callback. */
static void ui_searchbox_update_fn(bContext *C, uiBut *but, const char *str, uiSearchItems *items)
{
  wmWindow *win = CTX_wm_window(C);
  WM_tooltip_clear(C, win);
  but->search->update_fn(C, but->search->arg, str, items);
}

/* region is the search box itself */
void ui_searchbox_update(bContext *C, ARegion *region, uiBut *but, const bool reset)
{
  uiSearchboxData *data = region->regiondata;

  /* reset vars */
  data->items.totitem = 0;
  data->items.more = 0;
  if (reset == false) {
    data->items.offset_i = data->items.offset;
  }
  else {
    data->items.offset_i = data->items.offset = 0;
    data->active = -1;

    /* handle active */
    if (but->search->update_fn && but->func_arg2) {
      data->items.active = but->func_arg2;
      ui_searchbox_update_fn(C, but, but->editstr, &data->items);
      data->items.active = NULL;

      /* found active item, calculate real offset by centering it */
      if (data->items.totitem) {
        /* first case, begin of list */
        if (data->items.offset_i < data->items.maxitem) {
          data->active = data->items.offset_i;
          data->items.offset_i = 0;
        }
        else {
          /* second case, end of list */
          if (data->items.totitem - data->items.offset_i <= data->items.maxitem) {
            data->active = data->items.offset_i - data->items.totitem + data->items.maxitem;
            data->items.offset_i = data->items.totitem - data->items.maxitem;
          }
          else {
            /* center active item */
            data->items.offset_i -= data->items.maxitem / 2;
            data->active = data->items.maxitem / 2;
          }
        }
      }
      data->items.offset = data->items.offset_i;
      data->items.totitem = 0;
    }
  }

  /* callback */
  if (but->search->update_fn) {
    ui_searchbox_update_fn(C, but, but->editstr, &data->items);
  }

  /* handle case where editstr is equal to one of items */
  if (reset && data->active == -1) {
    int a;

    for (a = 0; a < data->items.totitem; a++) {
      const char *name = data->items.names[a];
      const char *name_sep = data->use_sep ? strrchr(name, UI_SEP_CHAR) : NULL;
      if (STREQLEN(but->editstr, name, name_sep ? (name_sep - name) : data->items.maxstrlen)) {
        data->active = a;
        break;
      }
    }
    if (data->items.totitem == 1 && but->editstr[0]) {
      data->active = 0;
    }
  }

  /* validate selected item */
  ui_searchbox_select(C, region, but, 0);

  ED_region_tag_redraw(region);
}

int ui_searchbox_autocomplete(bContext *C, ARegion *region, uiBut *but, char *str)
{
  uiSearchboxData *data = region->regiondata;
  int match = AUTOCOMPLETE_NO_MATCH;

  if (str[0]) {
    data->items.autocpl = UI_autocomplete_begin(str, ui_but_string_get_max_length(but));

    ui_searchbox_update_fn(C, but, but->editstr, &data->items);

    match = UI_autocomplete_end(data->items.autocpl, str);
    data->items.autocpl = NULL;
  }

  return match;
}

static void ui_searchbox_region_draw_cb(const bContext *C, ARegion *region)
{
  uiSearchboxData *data = region->regiondata;

  /* pixel space */
  wmOrtho2_region_pixelspace(region);

  if (data->noback == false) {
    ui_draw_widget_menu_back(&data->bbox, true);
  }

  /* draw text */
  if (data->items.totitem) {
    rcti rect;
    int a;

    if (data->preview) {
      /* draw items */
      for (a = 0; a < data->items.totitem; a++) {
        const int state = ((a == data->active) ? UI_ACTIVE : 0) | data->items.states[a];

        /* ensure icon is up-to-date */
        ui_icon_ensure_deferred(C, data->items.icons[a], data->preview);

        ui_searchbox_butrect(&rect, data, a);

        /* widget itself */
        ui_draw_preview_item(
            &data->fstyle, &rect, data->items.names[a], data->items.icons[a], state);
      }

      /* indicate more */
      if (data->items.more) {
        ui_searchbox_butrect(&rect, data, data->items.maxitem - 1);
        GPU_blend(true);
        UI_icon_draw(rect.xmax - 18, rect.ymin - 7, ICON_TRIA_DOWN);
        GPU_blend(false);
      }
      if (data->items.offset) {
        ui_searchbox_butrect(&rect, data, 0);
        GPU_blend(true);
        UI_icon_draw(rect.xmin, rect.ymax - 9, ICON_TRIA_UP);
        GPU_blend(false);
      }
    }
    else {
      const int search_sep_len = data->sep_string ? strlen(data->sep_string) : 0;
      /* draw items */
      for (a = 0; a < data->items.totitem; a++) {
        const int state = ((a == data->active) ? UI_ACTIVE : 0) | data->items.states[a];
        char *name = data->items.names[a];
        int icon = data->items.icons[a];
        char *name_sep_test = NULL;
        const bool use_sep_char = data->use_sep || (state & UI_BUT_HAS_SEP_CHAR);

        ui_searchbox_butrect(&rect, data, a);

        /* widget itself */
        if ((search_sep_len == 0) ||
            !(name_sep_test = strstr(data->items.names[a], data->sep_string))) {

          /* Simple menu item. */
          ui_draw_menu_item(&data->fstyle, &rect, name, icon, state, use_sep_char, NULL);
        }
        else {
          /* Split menu item, faded text before the separator. */
          char *name_sep = NULL;
          do {
            name_sep = name_sep_test;
            name_sep_test = strstr(name_sep + search_sep_len, data->sep_string);
          } while (name_sep_test != NULL);

          name_sep += search_sep_len;
          const char name_sep_prev = *name_sep;
          *name_sep = '\0';
          int name_width = 0;
          ui_draw_menu_item(
              &data->fstyle, &rect, name, 0, state | UI_BUT_INACTIVE, false, &name_width);
          *name_sep = name_sep_prev;
          rect.xmin += name_width;
          rect.xmin += UI_UNIT_X / 4;

          if (icon == ICON_BLANK1) {
            icon = ICON_NONE;
            rect.xmin -= UI_DPI_ICON_SIZE / 4;
          }

          /* The previous menu item draws the active selection. */
          ui_draw_menu_item(
              &data->fstyle, &rect, name_sep, icon, state & ~UI_ACTIVE, use_sep_char, NULL);
        }
      }
      /* indicate more */
      if (data->items.more) {
        ui_searchbox_butrect(&rect, data, data->items.maxitem - 1);
        GPU_blend(true);
        UI_icon_draw((BLI_rcti_size_x(&rect)) / 2, rect.ymin - 9, ICON_TRIA_DOWN);
        GPU_blend(false);
      }
      if (data->items.offset) {
        ui_searchbox_butrect(&rect, data, 0);
        GPU_blend(true);
        UI_icon_draw((BLI_rcti_size_x(&rect)) / 2, rect.ymax - 7, ICON_TRIA_UP);
        GPU_blend(false);
      }
    }
  }
}

static void ui_searchbox_region_free_cb(ARegion *region)
{
  uiSearchboxData *data = region->regiondata;
  int a;

  /* free search data */
  for (a = 0; a < data->items.maxitem; a++) {
    MEM_freeN(data->items.names[a]);
  }
  MEM_freeN(data->items.names);
  MEM_freeN(data->items.pointers);
  MEM_freeN(data->items.icons);
  MEM_freeN(data->items.states);

  MEM_freeN(data);
  region->regiondata = NULL;
}

ARegion *ui_searchbox_create_generic(bContext *C, ARegion *butregion, uiBut *but)
{
  wmWindow *win = CTX_wm_window(C);
  const uiStyle *style = UI_style_get();
  static ARegionType type;
  ARegion *region;
  uiSearchboxData *data;
  float aspect = but->block->aspect;
  rctf rect_fl;
  rcti rect_i;
  const int margin = UI_POPUP_MARGIN;
  int winx /*, winy */, ofsx, ofsy;
  int i;

  /* create area region */
  region = ui_region_temp_add(CTX_wm_screen(C));

  memset(&type, 0, sizeof(ARegionType));
  type.draw = ui_searchbox_region_draw_cb;
  type.free = ui_searchbox_region_free_cb;
  type.regionid = RGN_TYPE_TEMPORARY;
  region->type = &type;

  /* create searchbox data */
  data = MEM_callocN(sizeof(uiSearchboxData), "uiSearchboxData");

  /* set font, get bb */
  data->fstyle = style->widget; /* copy struct */
  ui_fontscale(&data->fstyle.points, aspect);
  UI_fontstyle_set(&data->fstyle);

  region->regiondata = data;

  /* special case, hardcoded feature, not draw backdrop when called from menus,
   * assume for design that popup already added it */
  if (but->block->flag & UI_BLOCK_SEARCH_MENU) {
    data->noback = true;
  }

  if (but->a1 > 0 && but->a2 > 0) {
    data->preview = true;
    data->prv_rows = but->a1;
    data->prv_cols = but->a2;
  }

  /* Only show key shortcuts when needed (checking RNA prop pointer is useless here, a lot of
   * buttons are about data without having that pointer defined, let's rather try with optype!).
   * One can also enforce that behavior by setting
   * UI_BUT_HAS_SHORTCUT drawflag of search button. */
  if (but->optype != NULL || (but->drawflag & UI_BUT_HAS_SHORTCUT) != 0) {
    data->use_sep = true;
  }
  data->sep_string = but->search->sep_string;

  /* compute position */
  if (but->block->flag & UI_BLOCK_SEARCH_MENU) {
    const int search_but_h = BLI_rctf_size_y(&but->rect) + 10;
    /* this case is search menu inside other menu */
    /* we copy region size */

    region->winrct = butregion->winrct;

    /* widget rect, in region coords */
    data->bbox.xmin = margin;
    data->bbox.xmax = BLI_rcti_size_x(&region->winrct) - margin;
    data->bbox.ymin = margin;
    data->bbox.ymax = BLI_rcti_size_y(&region->winrct) - margin;

    /* check if button is lower half */
    if (but->rect.ymax < BLI_rctf_cent_y(&but->block->rect)) {
      data->bbox.ymin += search_but_h;
    }
    else {
      data->bbox.ymax -= search_but_h;
    }
  }
  else {
    const int searchbox_width = UI_searchbox_size_x();

    rect_fl.xmin = but->rect.xmin - 5; /* align text with button */
    rect_fl.xmax = but->rect.xmax + 5; /* symmetrical */
    rect_fl.ymax = but->rect.ymin;
    rect_fl.ymin = rect_fl.ymax - UI_searchbox_size_y();

    ofsx = (but->block->panel) ? but->block->panel->ofsx : 0;
    ofsy = (but->block->panel) ? but->block->panel->ofsy : 0;

    BLI_rctf_translate(&rect_fl, ofsx, ofsy);

    /* minimal width */
    if (BLI_rctf_size_x(&rect_fl) < searchbox_width) {
      rect_fl.xmax = rect_fl.xmin + searchbox_width;
    }

    /* copy to int, gets projected if possible too */
    BLI_rcti_rctf_copy(&rect_i, &rect_fl);

    if (butregion->v2d.cur.xmin != butregion->v2d.cur.xmax) {
      UI_view2d_view_to_region_rcti(&butregion->v2d, &rect_fl, &rect_i);
    }

    BLI_rcti_translate(&rect_i, butregion->winrct.xmin, butregion->winrct.ymin);

    winx = WM_window_pixels_x(win);
    // winy = WM_window_pixels_y(win);  /* UNUSED */
    // wm_window_get_size(win, &winx, &winy);

    if (rect_i.xmax > winx) {
      /* super size */
      if (rect_i.xmax > winx + rect_i.xmin) {
        rect_i.xmax = winx;
        rect_i.xmin = 0;
      }
      else {
        rect_i.xmin -= rect_i.xmax - winx;
        rect_i.xmax = winx;
      }
    }

    if (rect_i.ymin < 0) {
      int newy1 = but->rect.ymax + ofsy;

      if (butregion->v2d.cur.xmin != butregion->v2d.cur.xmax) {
        newy1 = UI_view2d_view_to_region_y(&butregion->v2d, newy1);
      }

      newy1 += butregion->winrct.ymin;

      rect_i.ymax = BLI_rcti_size_y(&rect_i) + newy1;
      rect_i.ymin = newy1;
    }

    /* widget rect, in region coords */
    data->bbox.xmin = margin;
    data->bbox.xmax = BLI_rcti_size_x(&rect_i) + margin;
    data->bbox.ymin = margin;
    data->bbox.ymax = BLI_rcti_size_y(&rect_i) + margin;

    /* region bigger for shadow */
    region->winrct.xmin = rect_i.xmin - margin;
    region->winrct.xmax = rect_i.xmax + margin;
    region->winrct.ymin = rect_i.ymin - margin;
    region->winrct.ymax = rect_i.ymax;
  }

  /* adds subwindow */
  ED_region_floating_initialize(region);

  /* notify change and redraw */
  ED_region_tag_redraw(region);

  /* prepare search data */
  if (data->preview) {
    data->items.maxitem = data->prv_rows * data->prv_cols;
  }
  else {
    data->items.maxitem = SEARCH_ITEMS;
  }
  data->items.maxstrlen = but->hardmax;
  data->items.totitem = 0;
  data->items.names = MEM_callocN(data->items.maxitem * sizeof(void *), "search names");
  data->items.pointers = MEM_callocN(data->items.maxitem * sizeof(void *), "search pointers");
  data->items.icons = MEM_callocN(data->items.maxitem * sizeof(int), "search icons");
  data->items.states = MEM_callocN(data->items.maxitem * sizeof(int), "search flags");
  for (i = 0; i < data->items.maxitem; i++) {
    data->items.names[i] = MEM_callocN(but->hardmax + 1, "search pointers");
  }

  return region;
}

/**
 * Similar to Python's `str.title` except...
 *
 * - we know words are upper case and ascii only.
 * - '_' are replaces by spaces.
 */
static void str_tolower_titlecaps_ascii(char *str, const size_t len)
{
  size_t i;
  bool prev_delim = true;

  for (i = 0; (i < len) && str[i]; i++) {
    if (str[i] >= 'A' && str[i] <= 'Z') {
      if (prev_delim == false) {
        str[i] += 'a' - 'A';
      }
    }
    else if (str[i] == '_') {
      str[i] = ' ';
    }

    prev_delim = ELEM(str[i], ' ') || (str[i] >= '0' && str[i] <= '9');
  }
}

static void ui_searchbox_region_draw_cb__operator(const bContext *UNUSED(C), ARegion *region)
{
  uiSearchboxData *data = region->regiondata;

  /* pixel space */
  wmOrtho2_region_pixelspace(region);

  if (data->noback == false) {
    ui_draw_widget_menu_back(&data->bbox, true);
  }

  /* draw text */
  if (data->items.totitem) {
    rcti rect;
    int a;

    /* draw items */
    for (a = 0; a < data->items.totitem; a++) {
      rcti rect_pre, rect_post;
      ui_searchbox_butrect(&rect, data, a);

      rect_pre = rect;
      rect_post = rect;

      rect_pre.xmax = rect_post.xmin = rect.xmin + ((rect.xmax - rect.xmin) / 4);

      /* widget itself */
      /* NOTE: i18n messages extracting tool does the same, please keep it in sync. */
      {
        const int state = ((a == data->active) ? UI_ACTIVE : 0) | data->items.states[a];

        wmOperatorType *ot = data->items.pointers[a];
        char text_pre[128];
        char *text_pre_p = strstr(ot->idname, "_OT_");
        if (text_pre_p == NULL) {
          text_pre[0] = '\0';
        }
        else {
          int text_pre_len;
          text_pre_p += 1;
          text_pre_len = BLI_strncpy_rlen(
              text_pre, ot->idname, min_ii(sizeof(text_pre), text_pre_p - ot->idname));
          text_pre[text_pre_len] = ':';
          text_pre[text_pre_len + 1] = '\0';
          str_tolower_titlecaps_ascii(text_pre, sizeof(text_pre));
        }

        rect_pre.xmax += 4; /* sneaky, avoid showing ugly margin */
        ui_draw_menu_item(&data->fstyle,
                          &rect_pre,
                          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, text_pre),
                          data->items.icons[a],
                          state,
                          false,
                          NULL);
        ui_draw_menu_item(
            &data->fstyle, &rect_post, data->items.names[a], 0, state, data->use_sep, NULL);
      }
    }
    /* indicate more */
    if (data->items.more) {
      ui_searchbox_butrect(&rect, data, data->items.maxitem - 1);
      GPU_blend(true);
      UI_icon_draw((BLI_rcti_size_x(&rect)) / 2, rect.ymin - 9, ICON_TRIA_DOWN);
      GPU_blend(false);
    }
    if (data->items.offset) {
      ui_searchbox_butrect(&rect, data, 0);
      GPU_blend(true);
      UI_icon_draw((BLI_rcti_size_x(&rect)) / 2, rect.ymax - 7, ICON_TRIA_UP);
      GPU_blend(false);
    }
  }
}

ARegion *ui_searchbox_create_operator(bContext *C, ARegion *butregion, uiBut *but)
{
  ARegion *region;

  UI_but_drawflag_enable(but, UI_BUT_HAS_SHORTCUT);
  region = ui_searchbox_create_generic(C, butregion, but);

  region->type->draw = ui_searchbox_region_draw_cb__operator;

  return region;
}

void ui_searchbox_free(bContext *C, ARegion *region)
{
  ui_region_temp_remove(C, CTX_wm_screen(C), region);
}

static void ui_searchbox_region_draw_cb__menu(const bContext *UNUSED(C), ARegion *UNUSED(region))
{
  /* Currently unused. */
}

ARegion *ui_searchbox_create_menu(bContext *C, ARegion *butregion, uiBut *but)
{
  ARegion *region;

  UI_but_drawflag_enable(but, UI_BUT_HAS_SHORTCUT);
  region = ui_searchbox_create_generic(C, butregion, but);

  if (false) {
    region->type->draw = ui_searchbox_region_draw_cb__menu;
  }

  return region;
}

/* sets red alert if button holds a string it can't find */
/* XXX weak: search_func adds all partial matches... */
void ui_but_search_refresh(uiBut *but)
{
  uiSearchItems *items;
  int x1;

  /* possibly very large lists (such as ID datablocks) only
   * only validate string RNA buts (not pointers) */
  if (but->rnaprop && RNA_property_type(but->rnaprop) != PROP_STRING) {
    return;
  }

  items = MEM_callocN(sizeof(uiSearchItems), "search items");

  /* setup search struct */
  items->maxitem = 10;
  items->maxstrlen = 256;
  items->names = MEM_callocN(items->maxitem * sizeof(void *), "search names");
  for (x1 = 0; x1 < items->maxitem; x1++) {
    items->names[x1] = MEM_callocN(but->hardmax + 1, "search names");
  }

  ui_searchbox_update_fn(but->block->evil_C, but, but->drawstr, items);

  /* only redalert when we are sure of it, this can miss cases when >10 matches */
  if (items->totitem == 0) {
    UI_but_flag_enable(but, UI_BUT_REDALERT);
  }
  else if (items->more == 0) {
    if (UI_search_items_find_index(items, but->drawstr) == -1) {
      UI_but_flag_enable(but, UI_BUT_REDALERT);
    }
  }

  for (x1 = 0; x1 < items->maxitem; x1++) {
    MEM_freeN(items->names[x1]);
  }
  MEM_freeN(items->names);
  MEM_freeN(items);
}

/** \} */
