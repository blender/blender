/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Search Box Region & Interaction
 */

#include "MEM_guardedalloc.h"

#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "UI_interface_icons.hh"
#include "UI_view2d.hh"

#include "BLT_translation.hh"

#include "ED_screen.hh"

#include "BLF_api.hh"

#include "GPU_state.hh"
#include "interface_intern.hh"
#include "interface_regions_intern.hh"

using blender::StringRef;

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
  int *but_flags;
  uint8_t *name_prefix_offsets;

  /** Is there any item with an icon? */
  bool has_icon;

  AutoComplete *autocpl;
  void *active;
};

struct uiSearchboxData {
  rcti bbox;
  uiFontStyle fstyle;
  /** Region zoom level. */
  float zoom;
  uiSearchItems items;
  bool size_set;
  ARegion *butregion;
  uiButSearch *search_but;
  /** index in items array */
  int active;
  /** when menu opened with enough space for this */
  bool noback;
  /** draw thumbnail previews, rather than list */
  bool preview;
  /** Use the #UI_SEP_CHAR char for splitting shortcuts (good for operators, bad for data). */
  bool use_shortcut_sep;
  int prv_rows, prv_cols;
  /**
   * Show the active icon and text after the last instance of this string.
   * Used so we can show leading text to menu items less prominently (not related to 'use_sep').
   */
  const char *sep_string;

  /* Owned by uiButSearch */
  void *search_arg;
  uiButSearchListenFn search_listener;
};

#define SEARCH_ITEMS 10

bool UI_search_item_add(uiSearchItems *items,
                        const StringRef name,
                        void *poin,
                        int iconid,
                        const int but_flag,
                        const uint8_t name_prefix_offset)
{
  /* hijack for autocomplete */
  if (items->autocpl) {
    UI_autocomplete_update_name(items->autocpl, name.drop_prefix(name_prefix_offset));
    return true;
  }

  if (iconid) {
    items->has_icon = true;
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
    name.copy_utf8_truncated(items->names[items->totitem], items->maxstrlen);
  }
  if (items->pointers) {
    items->pointers[items->totitem] = poin;
  }
  if (items->icons) {
    items->icons[items->totitem] = iconid;
  }

  if (name_prefix_offset != 0) {
    /* Lazy initialize, as this isn't used often. */
    if (items->name_prefix_offsets == nullptr) {
      items->name_prefix_offsets = (uint8_t *)MEM_callocN(
          items->maxitem * sizeof(*items->name_prefix_offsets), __func__);
    }
    items->name_prefix_offsets[items->totitem] = name_prefix_offset;
  }

  /* Limit flags that can be set so flags such as 'UI_SELECT' aren't accidentally set
   * which will cause problems, add others as needed. */
  BLI_assert((but_flag &
              ~(UI_BUT_DISABLED | UI_BUT_INACTIVE | UI_BUT_REDALERT | UI_BUT_HAS_SEP_CHAR)) == 0);
  if (items->but_flags) {
    items->but_flags[items->totitem] = but_flag;
  }

  items->totitem++;

  return true;
}

int UI_searchbox_size_y()
{
  return SEARCH_ITEMS * UI_UNIT_Y + 2 * UI_SEARCHBOX_TRIA_H;
}

int UI_searchbox_size_x()
{
  return 12 * UI_UNIT_X;
}

static int ui_searchbox_size_x_from_items(const uiSearchItems &items)
{
  using namespace blender;

  /* Compute the width of each item. */
  Array<int> item_widths(items.totitem);
  threading::parallel_for(item_widths.index_range(), 256, [&](const IndexRange range) {
    for (const int i : range) {
      const blender::StringRefNull name = items.names[i];
      const int icon = items.icons ? items.icons[i] : ICON_NONE;
      const float text_width = BLF_width(BLF_default(), name.c_str(), name.size(), nullptr);
      const float icon_with_padding = icon == ICON_NONE ? 0.0f : UI_ICON_SIZE + UI_UNIT_X;
      const float padding = UI_UNIT_X;
      item_widths[i] = int(text_width + padding + icon_with_padding);
    }
  });

  /* Compute the final width of the search box. */
  int box_width = UI_searchbox_size_x();
  for (const int width : item_widths) {
    box_width = std::max(box_width, width);
  }
  /* Avoid extremely wide boxes. */
  box_width = std::min(box_width, UI_searchbox_size_x() * 5);
  return box_width;
}

int UI_searchbox_size_x_guess(const bContext *C, const uiButSearchUpdateFn update_fn, void *arg)
{
  using namespace blender;

  uiSearchItems items{};
  /* Upper bound on the number of item names that are checked. */
  items.maxitem = 1000;
  items.maxstrlen = 256;

  /* Prepare name buffers. */
  Array<char> names_buffer(items.maxitem * items.maxstrlen);
  Array<char *> names(items.maxitem);
  Array<int> icons(items.maxitem);
  items.names = names.data();
  items.icons = icons.data();
  for (int i : IndexRange(items.maxitem)) {
    names[i] = names_buffer.data() + i * items.maxstrlen;
  }

  /* Gather the items shown in the search box. */
  update_fn(C, arg, "", &items, true);

  /* This is lazy-initialized in #UI_search_item_add. */
  MEM_SAFE_FREE(items.name_prefix_offsets);

  return ui_searchbox_size_x_from_items(items);
}

int UI_search_items_find_index(const uiSearchItems *items, const char *name)
{
  if (items->name_prefix_offsets != nullptr) {
    for (int i = 0; i < items->totitem; i++) {
      if (STREQ(name, items->names[i] + items->name_prefix_offsets[i])) {
        return i;
      }
    }
  }
  else {
    for (int i = 0; i < items->totitem; i++) {
      if (STREQ(name, items->names[i])) {
        return i;
      }
    }
  }
  return -1;
}

/* region is the search box itself */
static void ui_searchbox_select(bContext *C, ARegion *region, uiBut *but, int step)
{
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);

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
  const float tria_h = data->zoom * UI_SEARCHBOX_TRIA_H;

  /* thumbnail preview */
  if (data->preview) {
    const int butw = BLI_rcti_size_x(&data->bbox) / data->prv_cols;
    const int buth = (BLI_rcti_size_y(&data->bbox) - 2.0f * tria_h) / data->prv_rows;
    int row, col;

    *r_rect = data->bbox;

    col = itemnr % data->prv_cols;
    row = itemnr / data->prv_cols;

    r_rect->xmin += col * butw;
    r_rect->xmax = r_rect->xmin + butw;

    r_rect->ymax -= tria_h + row * buth;
    r_rect->ymin = r_rect->ymax - buth;
  }
  /* list view */
  else {
    const float buth = (BLI_rcti_size_y(&data->bbox) - 2.0f * tria_h) / SEARCH_ITEMS;

    *r_rect = data->bbox;

    r_rect->xmin = data->bbox.xmin;
    r_rect->xmax = data->bbox.xmax;

    r_rect->ymax = data->bbox.ymax - tria_h - itemnr * buth;
    r_rect->ymin = r_rect->ymax - buth;
  }
}

int ui_searchbox_find_index(ARegion *region, const char *name)
{
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);
  return UI_search_items_find_index(&data->items, name);
}

bool ui_searchbox_inside(ARegion *region, const int xy[2])
{
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);

  return BLI_rcti_isect_pt(&data->bbox, xy[0] - region->winrct.xmin, xy[1] - region->winrct.ymin);
}

bool ui_searchbox_apply(uiBut *but, ARegion *region)
{
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);
  uiButSearch *search_but = (uiButSearch *)but;

  BLI_assert(but->type == ButType::SearchMenu);

  search_but->item_active = nullptr;

  if (data->active != -1) {
    const char *name = data->items.names[data->active] +
                       /* Never include the prefix in the button. */
                       (data->items.name_prefix_offsets ?
                            data->items.name_prefix_offsets[data->active] :
                            0);

    const char *name_sep = data->use_shortcut_sep ? strrchr(name, UI_SEP_CHAR) : nullptr;

    /* Search button with dynamic string properties may have their own method of applying
     * the search results, so only copy the result if there is a proper space for it. */
    if (but->hardmax != 0) {
      BLI_strncpy(but->editstr, name, name_sep ? (name_sep - name) + 1 : data->items.maxstrlen);
    }

    search_but->item_active = data->items.pointers[data->active];
    MEM_SAFE_FREE(search_but->item_active_str);
    search_but->item_active_str = BLI_strdup(data->items.names[data->active]);

    return true;
  }
  return false;
}

static ARegion *wm_searchbox_tooltip_init(
    bContext *C, ARegion *region, int * /*r_pass*/, double * /*pass_delay*/, bool *r_exit_on_event)
{
  *r_exit_on_event = true;

  LISTBASE_FOREACH (uiBlock *, block, &region->runtime->uiblocks) {
    for (const std::unique_ptr<uiBut> &but : block->buttons) {
      if (but->type != ButType::SearchMenu) {
        continue;
      }

      uiButSearch *search_but = (uiButSearch *)but.get();
      if (!search_but->item_tooltip_fn) {
        continue;
      }

      ARegion *searchbox_region = UI_region_searchbox_region_get(region);
      uiSearchboxData *data = static_cast<uiSearchboxData *>(searchbox_region->regiondata);

      BLI_assert(data->items.pointers[data->active] == search_but->item_active);

      rcti rect;
      ui_searchbox_butrect(&rect, data, data->active);

      return search_but->item_tooltip_fn(
          C, region, &rect, search_but->arg, search_but->item_active);
    }
  }
  return nullptr;
}

bool ui_searchbox_event(
    bContext *C, ARegion *region, uiBut *but, ARegion *butregion, const wmEvent *event)
{
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);
  uiButSearch *search_but = (uiButSearch *)but;
  int type = event->type, val = event->val;
  bool handled = false;
  bool tooltip_timer_started = false;

  BLI_assert(but->type == ButType::SearchMenu);

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
        if (search_but->item_context_menu_fn) {
          if (data->active != -1) {
            /* Check the cursor is over the active element
             * (a little confusing if this isn't the case, although it does work). */
            rcti rect;
            ui_searchbox_butrect(&rect, data, data->active);
            if (BLI_rcti_isect_pt(
                    &rect, event->xy[0] - region->winrct.xmin, event->xy[1] - region->winrct.ymin))
            {

              void *active = data->items.pointers[data->active];
              if (search_but->item_context_menu_fn(C, search_but->arg, active, event)) {
                handled = true;
              }
            }
          }
        }
      }
      break;
    case MOUSEMOVE: {
      /* Ignore the mouse event, in case the search popup is created underneath the cursor.
       * We always want the first result to be selected by default. See: #144168 */
      if (event->xy[0] == event->prev_xy[0] && event->xy[1] == event->prev_xy[1]) {
        ui_searchbox_select(C, region, but, 0);
        handled = true;
        break;
      }

      bool is_inside = false;

      if (BLI_rcti_isect_pt(&region->winrct, event->xy[0], event->xy[1])) {
        rcti rect;
        int a;

        for (a = 0; a < data->items.totitem; a++) {
          ui_searchbox_butrect(&rect, data, a);
          if (BLI_rcti_isect_pt(
                  &rect, event->xy[0] - region->winrct.xmin, event->xy[1] - region->winrct.ymin))
          {
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
            search_but->item_active = data->items.pointers[data->active];
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
static void ui_searchbox_update_fn(bContext *C,
                                   uiButSearch *but,
                                   const char *str,
                                   uiSearchItems *items)
{
  /* While the button is in text editing mode (searchbox open), remove tooltips on every update. */
  if (but->editstr) {
    wmWindow *win = CTX_wm_window(C);
    WM_tooltip_clear(C, win);
  }
  const bool is_first_search = !but->changed;
  but->items_update_fn(C, but->arg, str, items, is_first_search);
}

void ui_searchbox_update(bContext *C, ARegion *region, uiBut *but, const bool reset)
{
  uiButSearch *search_but = (uiButSearch *)but;
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);

  BLI_assert(but->type == ButType::SearchMenu);

  /* reset vars */
  data->items.totitem = 0;
  data->items.more = 0;
  if (!reset) {
    data->items.offset_i = data->items.offset;
  }
  else {
    data->items.offset_i = data->items.offset = 0;
    data->active = -1;

    /* On init, find and center active item. */
    const bool is_first_search = !but->changed;
    if (is_first_search && search_but->items_update_fn && search_but->item_active) {
      data->items.active = search_but->item_active;
      ui_searchbox_update_fn(C, search_but, but->editstr, &data->items);
      data->items.active = nullptr;

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
  if (search_but->items_update_fn) {
    ui_searchbox_update_fn(C, search_but, but->editstr, &data->items);
  }

  /* handle case where editstr is equal to one of items */
  if (reset && data->active == -1) {
    for (int a = 0; a < data->items.totitem; a++) {
      const char *name = data->items.names[a] +
                         /* Never include the prefix in the button. */
                         (data->items.name_prefix_offsets ? data->items.name_prefix_offsets[a] :
                                                            0);
      const char *name_sep = data->use_shortcut_sep ? strrchr(name, UI_SEP_CHAR) : nullptr;
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
  uiButSearch *search_but = (uiButSearch *)but;
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);
  int match = AUTOCOMPLETE_NO_MATCH;

  BLI_assert(but->type == ButType::SearchMenu);

  if (str[0]) {
    int maxncpy = ui_but_string_get_maxncpy(but);
    if (maxncpy == 0) {
      /* The string length is dynamic, just assume a reasonable length. */
      maxncpy = strlen(str) + 1024;
    }
    data->items.autocpl = UI_autocomplete_begin(str, maxncpy);

    ui_searchbox_update_fn(C, search_but, but->editstr, &data->items);

    match = UI_autocomplete_end(data->items.autocpl, str);
    data->items.autocpl = nullptr;
  }

  return match;
}

/**
 * Draws a downwards facing triangle.
 * \param rect: Rectangle under which the triangle icon is drawn. Usually from the last result item
 *              that can be displayed.
 */
static void ui_searchbox_draw_clip_tri_down(rcti *rect, const float zoom)
{
  const float x = BLI_rcti_cent_x(rect) - (0.5f * zoom * UI_ICON_SIZE);
  const float y = rect->ymin - (0.5f * zoom * (UI_SEARCHBOX_TRIA_H - UI_ICON_SIZE) - U.pixelsize) -
                  zoom * UI_ICON_SIZE;
  const float aspect = U.inv_scale_factor / zoom;
  GPU_blend(GPU_BLEND_ALPHA);
  UI_icon_draw_ex(
      x, y, ICON_TRIA_DOWN, aspect, 1.0f, 0.0f, nullptr, false, UI_NO_ICON_OVERLAY_TEXT);
  GPU_blend(GPU_BLEND_NONE);
}

/**
 * Draws an upwards facing triangle.
 * \param rect: Rectangle above which the triangle icon is drawn. Usually from the first result
 *              item that can be displayed.
 */
static void ui_searchbox_draw_clip_tri_up(rcti *rect, const float zoom)
{
  const float x = BLI_rcti_cent_x(rect) - (0.5f * zoom * UI_ICON_SIZE);
  const float y = rect->ymax + (0.5f * zoom * (UI_SEARCHBOX_TRIA_H - UI_ICON_SIZE) - U.pixelsize);
  const float aspect = U.inv_scale_factor / zoom;
  GPU_blend(GPU_BLEND_ALPHA);
  UI_icon_draw_ex(x, y, ICON_TRIA_UP, aspect, 1.0f, 0.0f, nullptr, false, UI_NO_ICON_OVERLAY_TEXT);
  GPU_blend(GPU_BLEND_NONE);
}

static void ui_searchbox_region_draw_fn(const bContext *C, ARegion *region)
{
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);

  /* pixel space */
  wmOrtho2_region_pixelspace(region);

  if (data->noback == false) {
    ui_draw_widget_menu_back(&data->bbox, true);
  }

  /* draw text */
  if (data->items.totitem) {
    rcti rect;

    if (data->preview) {
      /* draw items */
      for (int a = 0; a < data->items.totitem; a++) {
        const int but_flag = ((a == data->active) ? UI_HOVER : 0) | data->items.but_flags[a];

        /* ensure icon is up-to-date */
        ui_icon_ensure_deferred(C, data->items.icons[a], data->preview);

        ui_searchbox_butrect(&rect, data, a);

        /* widget itself */
        ui_draw_preview_item(&data->fstyle,
                             &rect,
                             data->zoom,
                             data->items.names[a],
                             data->items.icons[a],
                             but_flag,
                             UI_STYLE_TEXT_LEFT);
      }

      /* indicate more */
      if (data->items.more || data->items.offset) {
        rcti rect_first_item;
        ui_searchbox_butrect(&rect_first_item, data, 0);
        rcti rect_max_item;
        ui_searchbox_butrect(&rect_max_item, data, data->items.maxitem - 1);

        if (data->items.offset) {
          /* The first item is in the top left corner. Adjust width so the icon is centered. */
          rect_first_item.xmax = rect_max_item.xmax;
          ui_searchbox_draw_clip_tri_up(&rect_first_item, data->zoom);
        }

        if (data->items.more) {
          /* The last item is in the bottom right corner. Adjust width so the icon is centered. */
          rect_max_item.xmin = rect_first_item.xmin;
          ui_searchbox_draw_clip_tri_down(&rect_max_item, data->zoom);
        }
      }
    }
    else {
      const int search_sep_len = data->sep_string ? strlen(data->sep_string) : 0;
      /* draw items */
      for (int a = 0; a < data->items.totitem; a++) {
        const int but_flag = ((a == data->active) ? UI_HOVER : 0) | data->items.but_flags[a];
        const char *name = data->items.names[a];
        int icon = data->items.icons[a];
        char *name_sep_test = nullptr;

        uiMenuItemSeparatorType separator_type = UI_MENU_ITEM_SEPARATOR_NONE;
        if (data->use_shortcut_sep) {
          separator_type = UI_MENU_ITEM_SEPARATOR_SHORTCUT;
        }
        /* Only set for displaying additional hint (e.g. library name of a linked data-block). */
        else if (but_flag & UI_BUT_HAS_SEP_CHAR) {
          separator_type = UI_MENU_ITEM_SEPARATOR_HINT;
        }

        ui_searchbox_butrect(&rect, data, a);

        /* widget itself */
        if ((search_sep_len == 0) ||
            !(name_sep_test = strstr(data->items.names[a], data->sep_string)))
        {
          if (!icon && data->items.has_icon) {
            /* If there is any icon item, make sure all items line up. */
            icon = ICON_BLANK1;
          }

          /* Simple menu item. */
          ui_draw_menu_item(&data->fstyle,
                            &rect,
                            &rect,
                            data->zoom,
                            data->noback,
                            name,
                            icon,
                            but_flag,
                            separator_type,
                            nullptr);
        }
        else {
          /* Split menu item, faded text before the separator. */
          char *name_sep = nullptr;
          do {
            name_sep = name_sep_test;
            name_sep_test = strstr(name_sep + search_sep_len, data->sep_string);
          } while (name_sep_test != nullptr);

          name_sep += search_sep_len;
          const char name_sep_prev = *name_sep;
          *name_sep = '\0';
          int name_width = 0;
          ui_draw_menu_item(&data->fstyle,
                            &rect,
                            &rect,
                            data->zoom,
                            data->noback,
                            name,
                            ICON_NONE,
                            but_flag | UI_BUT_INACTIVE,
                            UI_MENU_ITEM_SEPARATOR_NONE,
                            &name_width);
          *name_sep = name_sep_prev;
          rect.xmin += name_width;
          rect.xmin += UI_UNIT_X / 4;

          if (icon == ICON_BLANK1) {
            icon = ICON_NONE;
          }
          if (icon != ICON_NONE) {
            rect.xmin += UI_UNIT_X / 8;
          }

          /* The previous menu item draws the active selection. */
          ui_draw_menu_item(&data->fstyle,
                            &rect,
                            nullptr,
                            data->zoom,
                            data->noback,
                            name_sep,
                            icon,
                            but_flag,
                            separator_type,
                            nullptr);
        }
      }
      /* indicate more */
      if (data->items.more) {
        ui_searchbox_butrect(&rect, data, data->items.maxitem - 1);
        ui_searchbox_draw_clip_tri_down(&rect, data->zoom);
      }
      if (data->items.offset) {
        ui_searchbox_butrect(&rect, data, 0);
        ui_searchbox_draw_clip_tri_up(&rect, data->zoom);
      }
    }
  }
  else {
    rcti rect;
    ui_searchbox_butrect(&rect, data, 0);
    ui_draw_menu_item(&data->fstyle,
                      &rect,
                      &rect,
                      data->zoom,
                      data->noback,
                      IFACE_("No results found"),
                      0,
                      0,
                      UI_MENU_ITEM_SEPARATOR_NONE,
                      nullptr);
  }
}

static void ui_searchbox_region_free_fn(ARegion *region)
{
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);

  /* free search data */
  for (int a = 0; a < data->items.maxitem; a++) {
    MEM_freeN(data->items.names[a]);
  }
  MEM_freeN(data->items.names);
  MEM_freeN(data->items.pointers);
  MEM_freeN(data->items.icons);
  MEM_freeN(data->items.but_flags);

  if (data->items.name_prefix_offsets != nullptr) {
    MEM_freeN(data->items.name_prefix_offsets);
  }

  MEM_freeN(data);
  region->regiondata = nullptr;
}

static void ui_searchbox_region_listen_fn(const wmRegionListenerParams *params)
{
  uiSearchboxData *data = static_cast<uiSearchboxData *>(params->region->regiondata);
  if (data->search_listener) {
    data->search_listener(params, data->search_arg);
  }
}

static void ui_searchbox_region_layout_fn(const bContext *C, ARegion *region)
{
  uiSearchboxData *data = (uiSearchboxData *)region->regiondata;

  if (data->size_set) {
    /* Already set. */
    return;
  }

  uiButSearch *but = data->search_but;
  ARegion *butregion = data->butregion;
  const int margin = UI_POPUP_MARGIN;
  wmWindow *win = CTX_wm_window(C);

  /* compute position */
  if (but->block->flag & UI_BLOCK_SEARCH_MENU) {
    /* this case is search menu inside other menu */
    /* we copy region size */

    region->winrct = butregion->winrct;

    /* Align menu items with the search button. */
    const float zoom = data->zoom;
    const int padding = zoom * UI_SEARCHBOX_BOUNDS - (data->preview ? 0 : U.pixelsize);
    const int search_but_h = BLI_rctf_size_y(&but->rect) + zoom * UI_SEARCHBOX_BOUNDS;

    /* widget rect, in region coords */
    data->bbox.xmin = margin + padding;
    data->bbox.xmax = BLI_rcti_size_x(&region->winrct) - (margin + padding);
    data->bbox.ymin = margin;
    data->bbox.ymax = BLI_rcti_size_y(&region->winrct) - UI_POPUP_MENU_TOP;

    /* check if button is lower half */
    if (but->rect.ymax < BLI_rctf_cent_y(&but->block->rect)) {
      data->bbox.ymin += search_but_h;
    }
    else {
      data->bbox.ymax -= search_but_h;
    }
  }
  else {
    const int searchbox_width = ui_searchbox_size_x_from_items(data->items);

    rctf rect_fl;
    rect_fl.xmin = but->rect.xmin;
    rect_fl.xmax = but->rect.xmax;
    rect_fl.ymax = but->rect.ymin;
    rect_fl.ymin = rect_fl.ymax - UI_searchbox_size_y();

    const int ofsx = (but->block->panel) ? but->block->panel->ofsx : 0;
    const int ofsy = (but->block->panel) ? but->block->panel->ofsy : 0;

    BLI_rctf_translate(&rect_fl, ofsx, ofsy);

    /* minimal width */
    if (BLI_rctf_size_x(&rect_fl) < searchbox_width) {
      rect_fl.xmax = rect_fl.xmin + searchbox_width;
    }

    /* copy to int, gets projected if possible too */
    rcti rect_i;
    BLI_rcti_rctf_copy(&rect_i, &rect_fl);

    if (butregion->v2d.cur.xmin != butregion->v2d.cur.xmax) {
      UI_view2d_view_to_region_rcti(&butregion->v2d, &rect_fl, &rect_i);
    }

    BLI_rcti_translate(&rect_i, butregion->winrct.xmin, butregion->winrct.ymin);

    int winx = WM_window_native_pixel_x(win);
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

  region->winx = region->winrct.xmax - region->winrct.xmin + 1;
  region->winy = region->winrct.ymax - region->winrct.ymin + 1;

  data->size_set = true;
}

static ARegion *ui_searchbox_create_generic_ex(bContext *C,
                                               ARegion *butregion,
                                               uiButSearch *but,
                                               const bool use_shortcut_sep)
{
  const uiStyle *style = UI_style_get();
  const float aspect = but->block->aspect;

  /* create area region */
  ARegion *region = ui_region_temp_add(CTX_wm_screen(C));

  static ARegionType type;
  memset(&type, 0, sizeof(ARegionType));
  type.layout = ui_searchbox_region_layout_fn;
  type.draw = ui_searchbox_region_draw_fn;
  type.free = ui_searchbox_region_free_fn;
  type.listener = ui_searchbox_region_listen_fn;
  type.regionid = RGN_TYPE_TEMPORARY;
  region->runtime->type = &type;

  /* Create search-box data. */
  uiSearchboxData *data = MEM_callocN<uiSearchboxData>(__func__);
  data->search_arg = but->arg;
  data->search_but = but;
  data->butregion = butregion;
  data->size_set = false;
  data->search_listener = but->listen_fn;
  data->zoom = 1.0f / aspect;

  /* Set font, get the bounding-box. */
  data->fstyle = style->widget; /* copy struct */
  ui_fontscale(&data->fstyle.points, aspect);
  UI_fontstyle_set(&data->fstyle);

  region->regiondata = data;

  /* Special case, hard-coded feature, not draw backdrop when called from menus,
   * assume for design that popup already added it. */
  if (but->block->flag & UI_BLOCK_SEARCH_MENU) {
    data->noback = true;
  }

  if (but->preview_rows > 0 && but->preview_cols > 0) {
    data->preview = true;
    data->prv_rows = but->preview_rows;
    data->prv_cols = but->preview_cols;
  }

  if (but->optype != nullptr || use_shortcut_sep) {
    data->use_shortcut_sep = true;
  }
  data->sep_string = but->item_sep_string;

  /* Adds sub-window. */
  ED_region_floating_init(region);

  /* notify change and redraw */
  ED_region_tag_redraw(region);

  /* prepare search data */
  if (data->preview) {
    data->items.maxitem = data->prv_rows * data->prv_cols;
  }
  else {
    data->items.maxitem = SEARCH_ITEMS;
  }
  /* In case the button's string is dynamic, make sure there are buffers available. */
  data->items.maxstrlen = but->hardmax == 0 ? UI_MAX_NAME_STR : but->hardmax;
  data->items.totitem = 0;
  data->items.names = (char **)MEM_callocN(data->items.maxitem * sizeof(void *), __func__);
  data->items.pointers = (void **)MEM_callocN(data->items.maxitem * sizeof(void *), __func__);
  data->items.icons = MEM_calloc_arrayN<int>(data->items.maxitem, __func__);
  data->items.but_flags = MEM_calloc_arrayN<int>(data->items.maxitem, __func__);
  data->items.name_prefix_offsets = nullptr; /* Lazy initialized as needed. */
  for (int i = 0; i < data->items.maxitem; i++) {
    data->items.names[i] = (char *)MEM_callocN(data->items.maxstrlen + 1, __func__);
  }

  return region;
}

ARegion *ui_searchbox_create_generic(bContext *C, ARegion *butregion, uiButSearch *search_but)
{
  return ui_searchbox_create_generic_ex(C, butregion, search_but, false);
}

/**
 * Similar to Python's `str.title` except...
 *
 * - we know words are upper case and ASCII only.
 * - `_` are replaced by spaces.
 */
static void str_tolower_titlecaps_ascii(char *str, const size_t len)
{
  bool prev_delim = true;

  for (size_t i = 0; (i < len) && str[i]; i++) {
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

static void ui_searchbox_region_draw_cb__operator(const bContext * /*C*/, ARegion *region)
{
  uiSearchboxData *data = static_cast<uiSearchboxData *>(region->regiondata);

  /* pixel space */
  wmOrtho2_region_pixelspace(region);

  if (data->noback == false) {
    ui_draw_widget_menu_back(&data->bbox, true);
  }

  /* draw text */
  if (data->items.totitem) {
    rcti rect;

    /* draw items */
    for (int a = 0; a < data->items.totitem; a++) {
      rcti rect_pre, rect_post;
      ui_searchbox_butrect(&rect, data, a);

      rect_pre = rect;
      rect_post = rect;

      rect_pre.xmax = rect_post.xmin = rect.xmin + ((rect.xmax - rect.xmin) / 4);

      /* widget itself */
      /* NOTE: i18n messages extracting tool does the same, please keep it in sync. */
      {
        const int but_flag = ((a == data->active) ? UI_HOVER : 0) | data->items.but_flags[a];

        wmOperatorType *ot = static_cast<wmOperatorType *>(data->items.pointers[a]);
        char text_pre[128];
        const char *text_pre_p = strstr(ot->idname, "_OT_");
        if (text_pre_p == nullptr) {
          text_pre[0] = '\0';
        }
        else {
          int text_pre_len;
          text_pre_p += 1;
          text_pre_len = BLI_strncpy_utf8_rlen(
              text_pre, ot->idname, min_ii(sizeof(text_pre), text_pre_p - ot->idname));
          text_pre[text_pre_len] = ':';
          text_pre[text_pre_len + 1] = '\0';
          str_tolower_titlecaps_ascii(text_pre, sizeof(text_pre));
        }

        ui_draw_menu_item(&data->fstyle,
                          &rect_pre,
                          &rect,
                          data->zoom,
                          data->noback,
                          CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, text_pre),
                          data->items.icons[a],
                          but_flag,
                          UI_MENU_ITEM_SEPARATOR_NONE,
                          nullptr);
        ui_draw_menu_item(&data->fstyle,
                          &rect_post,
                          nullptr,
                          data->zoom,
                          data->noback,
                          data->items.names[a],
                          0,
                          but_flag,
                          data->use_shortcut_sep ? UI_MENU_ITEM_SEPARATOR_SHORTCUT :
                                                   UI_MENU_ITEM_SEPARATOR_NONE,
                          nullptr);
      }
    }
    /* indicate more */
    if (data->items.more) {
      ui_searchbox_butrect(&rect, data, data->items.maxitem - 1);
      ui_searchbox_draw_clip_tri_down(&rect, data->zoom);
    }
    if (data->items.offset) {
      ui_searchbox_butrect(&rect, data, 0);
      ui_searchbox_draw_clip_tri_up(&rect, data->zoom);
    }
  }
  else {
    rcti rect;
    ui_searchbox_butrect(&rect, data, 0);
    ui_draw_menu_item(&data->fstyle,
                      &rect,
                      &rect,
                      data->zoom,
                      data->noback,
                      IFACE_("No results found"),
                      0,
                      0,
                      UI_MENU_ITEM_SEPARATOR_NONE,
                      nullptr);
  }
}

ARegion *ui_searchbox_create_operator(bContext *C, ARegion *butregion, uiButSearch *search_but)
{
  ARegion *region = ui_searchbox_create_generic_ex(C, butregion, search_but, true);

  region->runtime->type->draw = ui_searchbox_region_draw_cb__operator;

  return region;
}

void ui_searchbox_free(bContext *C, ARegion *region)
{
  ui_region_temp_remove(C, CTX_wm_screen(C), region);
}

static void ui_searchbox_region_draw_cb__menu(const bContext * /*C*/, ARegion * /*region*/)
{
  /* Currently unused. */
}

ARegion *ui_searchbox_create_menu(bContext *C, ARegion *butregion, uiButSearch *search_but)
{
  ARegion *region = ui_searchbox_create_generic_ex(C, butregion, search_but, true);

  if (false) {
    region->runtime->type->draw = ui_searchbox_region_draw_cb__menu;
  }

  return region;
}

void ui_but_search_refresh(uiButSearch *but)
{
  /* possibly very large lists (such as ID datablocks) only
   * only validate string RNA buts (not pointers) */
  if (but->rnaprop && RNA_property_type(but->rnaprop) != PROP_STRING) {
    return;
  }

  uiSearchItems *items = MEM_callocN<uiSearchItems>(__func__);

  /* setup search struct */
  items->maxitem = 10;
  items->maxstrlen = 256;
  items->names = (char **)MEM_callocN(items->maxitem * sizeof(void *), __func__);
  for (int i = 0; i < items->maxitem; i++) {
    items->names[i] = (char *)MEM_callocN(but->hardmax + 1, __func__);
  }

  ui_searchbox_update_fn((bContext *)but->block->evil_C, but, but->drawstr.c_str(), items);

  if (!but->results_are_suggestions) {
    /* Only red-alert when we are sure of it, this can miss cases when >10 matches. */
    if (items->totitem == 0) {
      UI_but_flag_enable(but, UI_BUT_REDALERT);
    }
    else if (items->more == 0) {
      if (UI_search_items_find_index(items, but->drawstr.c_str()) == -1) {
        UI_but_flag_enable(but, UI_BUT_REDALERT);
      }
    }
  }

  for (int i = 0; i < items->maxitem; i++) {
    MEM_freeN(items->names[i]);
  }
  MEM_freeN(items->names);
  MEM_freeN(items);
}

/** \} */
