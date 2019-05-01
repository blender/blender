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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_math.h"

#include "BLI_string.h"
#include "BLI_rect.h"
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

#include "interface_intern.h"
#include "interface_regions_intern.h"
#include "GPU_state.h"

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
  /** use the UI_SEP_CHAR char for splitting shortcuts (good for operators, bad for data) */
  bool use_sep;
  int prv_rows, prv_cols;
} uiSearchboxData;

#define SEARCH_ITEMS 10

/* exported for use by search callbacks */
/* returns zero if nothing to add */
bool UI_search_item_add(uiSearchItems *items, const char *name, void *poin, int iconid)
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

/* ar is the search box itself */
static void ui_searchbox_select(bContext *C, ARegion *ar, uiBut *but, int step)
{
  uiSearchboxData *data = ar->regiondata;

  /* apply step */
  data->active += step;

  if (data->items.totitem == 0) {
    data->active = -1;
  }
  else if (data->active >= data->items.totitem) {
    if (data->items.more) {
      data->items.offset++;
      data->active = data->items.totitem - 1;
      ui_searchbox_update(C, ar, but, false);
    }
    else {
      data->active = data->items.totitem - 1;
    }
  }
  else if (data->active < 0) {
    if (data->items.offset) {
      data->items.offset--;
      data->active = 0;
      ui_searchbox_update(C, ar, but, false);
    }
    else {
      /* only let users step into an 'unset' state for unlink buttons */
      data->active = (but->flag & UI_BUT_VALUE_CLEAR) ? -1 : 0;
    }
  }

  ED_region_tag_redraw(ar);
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

int ui_searchbox_find_index(ARegion *ar, const char *name)
{
  uiSearchboxData *data = ar->regiondata;
  return UI_search_items_find_index(&data->items, name);
}

/* x and y in screencoords */
bool ui_searchbox_inside(ARegion *ar, int x, int y)
{
  uiSearchboxData *data = ar->regiondata;

  return BLI_rcti_isect_pt(&data->bbox, x - ar->winrct.xmin, y - ar->winrct.ymin);
}

/* string validated to be of correct length (but->hardmax) */
bool ui_searchbox_apply(uiBut *but, ARegion *ar)
{
  uiSearchboxData *data = ar->regiondata;

  but->func_arg2 = NULL;

  if (data->active != -1) {
    const char *name = data->items.names[data->active];
    const char *name_sep = data->use_sep ? strrchr(name, UI_SEP_CHAR) : NULL;

    BLI_strncpy(but->editstr, name, name_sep ? (name_sep - name) : data->items.maxstrlen);

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

void ui_searchbox_event(bContext *C, ARegion *ar, uiBut *but, const wmEvent *event)
{
  uiSearchboxData *data = ar->regiondata;
  int type = event->type, val = event->val;

  if (type == MOUSEPAN) {
    ui_pan_to_scroll(event, &type, &val);
  }

  switch (type) {
    case WHEELUPMOUSE:
    case UPARROWKEY:
      ui_searchbox_select(C, ar, but, -1);
      break;
    case WHEELDOWNMOUSE:
    case DOWNARROWKEY:
      ui_searchbox_select(C, ar, but, 1);
      break;
    case MOUSEMOVE:
      if (BLI_rcti_isect_pt(&ar->winrct, event->x, event->y)) {
        rcti rect;
        int a;

        for (a = 0; a < data->items.totitem; a++) {
          ui_searchbox_butrect(&rect, data, a);
          if (BLI_rcti_isect_pt(&rect, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin)) {
            if (data->active != a) {
              data->active = a;
              ui_searchbox_select(C, ar, but, 0);
              break;
            }
          }
        }
      }
      break;
  }
}

/* ar is the search box itself */
void ui_searchbox_update(bContext *C, ARegion *ar, uiBut *but, const bool reset)
{
  uiSearchboxData *data = ar->regiondata;

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
    if (but->search_func && but->func_arg2) {
      data->items.active = but->func_arg2;
      but->search_func(C, but->search_arg, but->editstr, &data->items);
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
  if (but->search_func) {
    but->search_func(C, but->search_arg, but->editstr, &data->items);
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
  ui_searchbox_select(C, ar, but, 0);

  ED_region_tag_redraw(ar);
}

int ui_searchbox_autocomplete(bContext *C, ARegion *ar, uiBut *but, char *str)
{
  uiSearchboxData *data = ar->regiondata;
  int match = AUTOCOMPLETE_NO_MATCH;

  if (str[0]) {
    data->items.autocpl = UI_autocomplete_begin(str, ui_but_string_get_max_length(but));

    but->search_func(C, but->search_arg, but->editstr, &data->items);

    match = UI_autocomplete_end(data->items.autocpl, str);
    data->items.autocpl = NULL;
  }

  return match;
}

static void ui_searchbox_region_draw_cb(const bContext *C, ARegion *ar)
{
  uiSearchboxData *data = ar->regiondata;

  /* pixel space */
  wmOrtho2_region_pixelspace(ar);

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
        /* ensure icon is up-to-date */
        ui_icon_ensure_deferred(C, data->items.icons[a], data->preview);

        ui_searchbox_butrect(&rect, data, a);

        /* widget itself */
        ui_draw_preview_item(&data->fstyle,
                             &rect,
                             data->items.names[a],
                             data->items.icons[a],
                             (a == data->active) ? UI_ACTIVE : 0);
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
      /* draw items */
      for (a = 0; a < data->items.totitem; a++) {
        ui_searchbox_butrect(&rect, data, a);

        /* widget itself */
        ui_draw_menu_item(&data->fstyle,
                          &rect,
                          data->items.names[a],
                          data->items.icons[a],
                          (a == data->active) ? UI_ACTIVE : 0,
                          data->use_sep);
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

static void ui_searchbox_region_free_cb(ARegion *ar)
{
  uiSearchboxData *data = ar->regiondata;
  int a;

  /* free search data */
  for (a = 0; a < data->items.maxitem; a++) {
    MEM_freeN(data->items.names[a]);
  }
  MEM_freeN(data->items.names);
  MEM_freeN(data->items.pointers);
  MEM_freeN(data->items.icons);

  MEM_freeN(data);
  ar->regiondata = NULL;
}

ARegion *ui_searchbox_create_generic(bContext *C, ARegion *butregion, uiBut *but)
{
  wmWindow *win = CTX_wm_window(C);
  uiStyle *style = UI_style_get();
  static ARegionType type;
  ARegion *ar;
  uiSearchboxData *data;
  float aspect = but->block->aspect;
  rctf rect_fl;
  rcti rect_i;
  const int margin = UI_POPUP_MARGIN;
  int winx /*, winy */, ofsx, ofsy;
  int i;

  /* create area region */
  ar = ui_region_temp_add(CTX_wm_screen(C));

  memset(&type, 0, sizeof(ARegionType));
  type.draw = ui_searchbox_region_draw_cb;
  type.free = ui_searchbox_region_free_cb;
  type.regionid = RGN_TYPE_TEMPORARY;
  ar->type = &type;

  /* create searchbox data */
  data = MEM_callocN(sizeof(uiSearchboxData), "uiSearchboxData");

  /* set font, get bb */
  data->fstyle = style->widget; /* copy struct */
  ui_fontscale(&data->fstyle.points, aspect);
  UI_fontstyle_set(&data->fstyle);

  ar->regiondata = data;

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

  /* compute position */
  if (but->block->flag & UI_BLOCK_SEARCH_MENU) {
    const int search_but_h = BLI_rctf_size_y(&but->rect) + 10;
    /* this case is search menu inside other menu */
    /* we copy region size */

    ar->winrct = butregion->winrct;

    /* widget rect, in region coords */
    data->bbox.xmin = margin;
    data->bbox.xmax = BLI_rcti_size_x(&ar->winrct) - margin;
    data->bbox.ymin = margin;
    data->bbox.ymax = BLI_rcti_size_y(&ar->winrct) - margin;

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
    ar->winrct.xmin = rect_i.xmin - margin;
    ar->winrct.xmax = rect_i.xmax + margin;
    ar->winrct.ymin = rect_i.ymin - margin;
    ar->winrct.ymax = rect_i.ymax;
  }

  /* adds subwindow */
  ED_region_init(ar);

  /* notify change and redraw */
  ED_region_tag_redraw(ar);

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
  for (i = 0; i < data->items.maxitem; i++) {
    data->items.names[i] = MEM_callocN(but->hardmax + 1, "search pointers");
  }

  return ar;
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

static void ui_searchbox_region_draw_cb__operator(const bContext *UNUSED(C), ARegion *ar)
{
  uiSearchboxData *data = ar->regiondata;

  /* pixel space */
  wmOrtho2_region_pixelspace(ar);

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
        wmOperatorType *ot = data->items.pointers[a];

        int state = (a == data->active) ? UI_ACTIVE : 0;
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
                          false);
        ui_draw_menu_item(
            &data->fstyle, &rect_post, data->items.names[a], 0, state, data->use_sep);
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
  ARegion *ar;

  UI_but_drawflag_enable(but, UI_BUT_HAS_SHORTCUT);
  ar = ui_searchbox_create_generic(C, butregion, but);

  ar->type->draw = ui_searchbox_region_draw_cb__operator;

  return ar;
}

void ui_searchbox_free(bContext *C, ARegion *ar)
{
  ui_region_temp_remove(C, CTX_wm_screen(C), ar);
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

  but->search_func(but->block->evil_C, but->search_arg, but->drawstr, items);

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
