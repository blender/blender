/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Utilities to inspect the interface, extract information.
 */

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_screen_types.h"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "RNA_access.h"

#include "interface_intern.hh"

#include "WM_api.hh"
#include "WM_types.hh"

/* -------------------------------------------------------------------- */
/** \name Button (#uiBut) State
 * \{ */

bool ui_but_is_editable(const uiBut *but)
{
  return !ELEM(but->type,
               UI_BTYPE_LABEL,
               UI_BTYPE_SEPR,
               UI_BTYPE_SEPR_LINE,
               UI_BTYPE_ROUNDBOX,
               UI_BTYPE_LISTBOX,
               UI_BTYPE_PROGRESS);
}

bool ui_but_is_editable_as_text(const uiBut *but)
{
  return ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER, UI_BTYPE_SEARCH_MENU);
}

bool ui_but_is_toggle(const uiBut *but)
{
  return ELEM(but->type,
              UI_BTYPE_BUT_TOGGLE,
              UI_BTYPE_TOGGLE,
              UI_BTYPE_ICON_TOGGLE,
              UI_BTYPE_ICON_TOGGLE_N,
              UI_BTYPE_TOGGLE_N,
              UI_BTYPE_CHECKBOX,
              UI_BTYPE_CHECKBOX_N,
              UI_BTYPE_ROW);
}

bool ui_but_is_interactive_ex(const uiBut *but, const bool labeledit, const bool for_tooltip)
{
  /* NOTE: #UI_BTYPE_LABEL is included for highlights, this allows drags. */
  if (but->type == UI_BTYPE_LABEL) {
    if (for_tooltip) {
      /* It's important labels are considered interactive for the purpose of showing tooltip. */
      if (!ui_but_drag_is_draggable(but) && but->tip_func == nullptr) {
        return false;
      }
    }
    else {
      if (!ui_but_drag_is_draggable(but)) {
        return false;
      }
    }
  }

  if (ELEM(but->type, UI_BTYPE_ROUNDBOX, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE, UI_BTYPE_LISTBOX)) {
    return false;
  }
  if (but->flag & UI_HIDDEN) {
    return false;
  }
  if (but->flag & UI_SCROLLED) {
    return false;
  }
  if ((but->type == UI_BTYPE_TEXT) &&
      ELEM(but->emboss, UI_EMBOSS_NONE, UI_EMBOSS_NONE_OR_STATUS) && !labeledit)
  {
    return false;
  }
  if ((but->type == UI_BTYPE_LISTROW) && labeledit) {
    return false;
  }
  if (but->type == UI_BTYPE_VIEW_ITEM) {
    const uiButViewItem *but_item = static_cast<const uiButViewItem *>(but);
    return UI_view_item_is_interactive(but_item->view_item);
  }

  return true;
}

bool ui_but_is_interactive(const uiBut *but, const bool labeledit)
{
  return ui_but_is_interactive_ex(but, labeledit, false);
}

bool UI_but_is_utf8(const uiBut *but)
{
  if (but->rnaprop) {
    const int subtype = RNA_property_subtype(but->rnaprop);
    return !ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME, PROP_BYTESTRING);
  }
  return !(but->flag & UI_BUT_NO_UTF8);
}

#ifdef USE_UI_POPOVER_ONCE
bool ui_but_is_popover_once_compat(const uiBut *but)
{
  return (ELEM(but->type, UI_BTYPE_BUT, UI_BTYPE_DECORATOR) || ui_but_is_toggle(but));
}
#endif

bool ui_but_has_array_value(const uiBut *but)
{
  return (but->rnapoin.data && but->rnaprop && RNA_property_array_check(but->rnaprop));
}

static wmOperatorType *g_ot_tool_set_by_id = nullptr;
bool UI_but_is_tool(const uiBut *but)
{
  /* very evil! */
  if (but->optype != nullptr) {
    if (g_ot_tool_set_by_id == nullptr) {
      g_ot_tool_set_by_id = WM_operatortype_find("WM_OT_tool_set_by_id", false);
    }
    if (but->optype == g_ot_tool_set_by_id) {
      return true;
    }
  }
  return false;
}

bool UI_but_has_tooltip_label(const uiBut *but)
{
  return (but->drawflag & UI_BUT_HAS_TOOLTIP_LABEL) != 0;
}

int ui_but_icon(const uiBut *but)
{
  if (!(but->flag & UI_HAS_ICON)) {
    return ICON_NONE;
  }

  /* Consecutive icons can be toggle between. */
  if (but->drawflag & UI_BUT_ICON_REVERSE) {
    return but->icon - but->iconadd;
  }
  return but->icon + but->iconadd;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button (#uiBut) Spatial
 * \{ */

void ui_but_pie_dir(RadialDirection dir, float vec[2])
{
  float angle;

  BLI_assert(dir != UI_RADIAL_NONE);

  angle = DEG2RADF(float(ui_radial_dir_to_angle[dir]));
  vec[0] = cosf(angle);
  vec[1] = sinf(angle);
}

static bool ui_but_isect_pie_seg(const uiBlock *block, const uiBut *but)
{
  const float angle_range = (block->pie_data.flags & UI_PIE_DEGREES_RANGE_LARGE) ? M_PI_4 :
                                                                                   M_PI_4 / 2.0;
  float vec[2];

  if (block->pie_data.flags & UI_PIE_INVALID_DIR) {
    return false;
  }

  ui_but_pie_dir(but->pie_dir, vec);

  if (saacos(dot_v2v2(vec, block->pie_data.pie_dir)) < angle_range) {
    return true;
  }

  return false;
}

bool ui_but_contains_pt(const uiBut *but, float mx, float my)
{
  return BLI_rctf_isect_pt(&but->rect, mx, my);
}

bool ui_but_contains_rect(const uiBut *but, const rctf *rect)
{
  return BLI_rctf_isect(&but->rect, rect, nullptr);
}

bool ui_but_contains_point_px(const uiBut *but, const ARegion *region, const int xy[2])
{
  uiBlock *block = but->block;
  if (!ui_region_contains_point_px(region, xy)) {
    return false;
  }

  float mx = xy[0], my = xy[1];
  ui_window_to_block_fl(region, block, &mx, &my);

  if (but->pie_dir != UI_RADIAL_NONE) {
    if (!ui_but_isect_pie_seg(block, but)) {
      return false;
    }
  }
  else if (!ui_but_contains_pt(but, mx, my)) {
    return false;
  }

  return true;
}

bool ui_but_contains_point_px_icon(const uiBut *but, ARegion *region, const wmEvent *event)
{
  rcti rect;
  int x = event->xy[0], y = event->xy[1];

  ui_window_to_block(region, but->block, &x, &y);

  BLI_rcti_rctf_copy(&rect, &but->rect);

  if (but->dragflag & UI_BUT_DRAG_FULL_BUT) {
    /* use button size itself */
  }
  else if (but->drawflag & UI_BUT_ICON_LEFT) {
    rect.xmax = rect.xmin + BLI_rcti_size_y(&rect);
  }
  else {
    const int delta = BLI_rcti_size_x(&rect) - BLI_rcti_size_y(&rect);
    rect.xmin += delta / 2;
    rect.xmax -= delta / 2;
  }

  return BLI_rcti_isect_pt(&rect, x, y);
}

static uiBut *ui_but_find(const ARegion *region,
                          const uiButFindPollFn find_poll,
                          const void *find_custom_data)
{
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    LISTBASE_FOREACH_BACKWARD (uiBut *, but, &block->buttons) {
      if (find_poll && find_poll(but, find_custom_data) == false) {
        continue;
      }
      return but;
    }
  }

  return nullptr;
}

uiBut *ui_but_find_mouse_over_ex(const ARegion *region,
                                 const int xy[2],
                                 const bool labeledit,
                                 const bool for_tooltip,
                                 const uiButFindPollFn find_poll,
                                 const void *find_custom_data)
{
  uiBut *butover = nullptr;

  if (!ui_region_contains_point_px(region, xy)) {
    return nullptr;
  }
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    float mx = xy[0], my = xy[1];
    ui_window_to_block_fl(region, block, &mx, &my);

    LISTBASE_FOREACH_BACKWARD (uiBut *, but, &block->buttons) {
      if (find_poll && find_poll(but, find_custom_data) == false) {
        continue;
      }
      if (ui_but_is_interactive_ex(but, labeledit, for_tooltip)) {
        if (but->pie_dir != UI_RADIAL_NONE) {
          if (ui_but_isect_pie_seg(block, but)) {
            butover = but;
            break;
          }
        }
        else if (ui_but_contains_pt(but, mx, my)) {
          butover = but;
          break;
        }
      }
    }

    /* CLIP_EVENTS prevents the event from reaching other blocks */
    if (block->flag & UI_BLOCK_CLIP_EVENTS) {
      /* check if mouse is inside block */
      if (BLI_rctf_isect_pt(&block->rect, mx, my)) {
        break;
      }
    }
  }

  return butover;
}

uiBut *ui_but_find_mouse_over(const ARegion *region, const wmEvent *event)
{
  return ui_but_find_mouse_over_ex(
      region, event->xy, event->modifier & KM_CTRL, false, nullptr, nullptr);
}

uiBut *ui_but_find_rect_over(const ARegion *region, const rcti *rect_px)
{
  if (!ui_region_contains_rect_px(region, rect_px)) {
    return nullptr;
  }

  /* Currently no need to expose this at the moment. */
  const bool labeledit = true;
  rctf rect_px_fl;
  BLI_rctf_rcti_copy(&rect_px_fl, rect_px);
  uiBut *butover = nullptr;

  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    rctf rect_block;
    ui_window_to_block_rctf(region, block, &rect_block, &rect_px_fl);

    LISTBASE_FOREACH_BACKWARD (uiBut *, but, &block->buttons) {
      if (ui_but_is_interactive(but, labeledit)) {
        /* No pie menu support. */
        BLI_assert(but->pie_dir == UI_RADIAL_NONE);
        if (ui_but_contains_rect(but, &rect_block)) {
          butover = but;
          break;
        }
      }
    }

    /* CLIP_EVENTS prevents the event from reaching other blocks */
    if (block->flag & UI_BLOCK_CLIP_EVENTS) {
      /* check if mouse is inside block */
      if (BLI_rctf_isect(&block->rect, &rect_block, nullptr)) {
        break;
      }
    }
  }
  return butover;
}

uiBut *ui_list_find_mouse_over_ex(const ARegion *region, const int xy[2])
{
  if (!ui_region_contains_point_px(region, xy)) {
    return nullptr;
  }
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    float mx = xy[0], my = xy[1];
    ui_window_to_block_fl(region, block, &mx, &my);
    LISTBASE_FOREACH_BACKWARD (uiBut *, but, &block->buttons) {
      if (but->type == UI_BTYPE_LISTBOX && ui_but_contains_pt(but, mx, my)) {
        return but;
      }
    }
  }

  return nullptr;
}

uiBut *ui_list_find_mouse_over(const ARegion *region, const wmEvent *event)
{
  if (event == nullptr) {
    /* If there is no info about the mouse, just act as if there is nothing underneath it. */
    return nullptr;
  }
  return ui_list_find_mouse_over_ex(region, event->xy);
}

uiList *UI_list_find_mouse_over(const ARegion *region, const wmEvent *event)
{
  uiBut *list_but = ui_list_find_mouse_over(region, event);
  if (!list_but) {
    return nullptr;
  }

  return static_cast<uiList *>(list_but->custom_data);
}

static bool ui_list_contains_row(const uiBut *listbox_but, const uiBut *listrow_but)
{
  BLI_assert(listbox_but->type == UI_BTYPE_LISTBOX);
  BLI_assert(listrow_but->type == UI_BTYPE_LISTROW);
  /* The list box and its rows have the same RNA data (active data pointer/prop). */
  return ui_but_rna_equals(listbox_but, listrow_but);
}

static bool ui_but_is_listbox_with_row(const uiBut *but, const void *customdata)
{
  const uiBut *row_but = static_cast<const uiBut *>(customdata);
  return (but->type == UI_BTYPE_LISTBOX) && ui_list_contains_row(but, row_but);
}

uiBut *ui_list_find_from_row(const ARegion *region, const uiBut *row_but)
{
  return ui_but_find(region, ui_but_is_listbox_with_row, row_but);
}

static bool ui_but_is_listrow(const uiBut *but, const void * /*customdata*/)
{
  return but->type == UI_BTYPE_LISTROW;
}

uiBut *ui_list_row_find_mouse_over(const ARegion *region, const int xy[2])
{
  return ui_but_find_mouse_over_ex(region, xy, false, false, ui_but_is_listrow, nullptr);
}

struct ListRowFindIndexData {
  int index;
  uiBut *listbox;
};

static bool ui_but_is_listrow_at_index(const uiBut *but, const void *customdata)
{
  const ListRowFindIndexData *find_data = static_cast<const ListRowFindIndexData *>(customdata);

  return ui_but_is_listrow(but, nullptr) && ui_list_contains_row(find_data->listbox, but) &&
         (but->hardmax == find_data->index);
}

uiBut *ui_list_row_find_index(const ARegion *region, const int index, uiBut *listbox)
{
  BLI_assert(listbox->type == UI_BTYPE_LISTBOX);
  ListRowFindIndexData data = {};
  data.index = index;
  data.listbox = listbox;
  return ui_but_find(region, ui_but_is_listrow_at_index, &data);
}

static bool ui_but_is_view_item_fn(const uiBut *but, const void * /*customdata*/)
{
  return but->type == UI_BTYPE_VIEW_ITEM;
}

uiBut *ui_view_item_find_mouse_over(const ARegion *region, const int xy[2])
{
  return ui_but_find_mouse_over_ex(region, xy, false, false, ui_but_is_view_item_fn, nullptr);
}

static bool ui_but_is_active_view_item(const uiBut *but, const void * /*customdata*/)
{
  if (but->type != UI_BTYPE_VIEW_ITEM) {
    return false;
  }

  const uiButViewItem *view_item_but = (const uiButViewItem *)but;
  return UI_view_item_is_active(view_item_but->view_item);
}

uiBut *ui_view_item_find_active(const ARegion *region)
{
  return ui_but_find(region, ui_but_is_active_view_item, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button (#uiBut) Relations
 * \{ */

uiBut *ui_but_prev(uiBut *but)
{
  while (but->prev) {
    but = but->prev;
    if (ui_but_is_editable(but)) {
      return but;
    }
  }
  return nullptr;
}

uiBut *ui_but_next(uiBut *but)
{
  while (but->next) {
    but = but->next;
    if (ui_but_is_editable(but)) {
      return but;
    }
  }
  return nullptr;
}

uiBut *ui_but_first(uiBlock *block)
{
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    if (ui_but_is_editable(but)) {
      return but;
    }
  }
  return nullptr;
}

uiBut *ui_but_last(uiBlock *block)
{
  uiBut *but = static_cast<uiBut *>(block->buttons.last);
  while (but) {
    if (ui_but_is_editable(but)) {
      return but;
    }
    but = but->prev;
  }
  return nullptr;
}

bool ui_but_is_cursor_warp(const uiBut *but)
{
  if (U.uiflag & USER_CONTINUOUS_MOUSE) {
    if (ELEM(but->type,
             UI_BTYPE_NUM,
             UI_BTYPE_NUM_SLIDER,
             UI_BTYPE_TRACK_PREVIEW,
             UI_BTYPE_HSVCUBE,
             UI_BTYPE_HSVCIRCLE,
             UI_BTYPE_CURVE,
             UI_BTYPE_CURVEPROFILE))
    {
      return true;
    }
  }

  return false;
}

bool ui_but_contains_password(const uiBut *but)
{
  return but->rnaprop && (RNA_property_subtype(but->rnaprop) == PROP_PASSWORD);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button (#uiBut) Text
 * \{ */

size_t ui_but_drawstr_len_without_sep_char(const uiBut *but)
{
  if (but->flag & UI_BUT_HAS_SEP_CHAR) {
    const char *str_sep = strrchr(but->drawstr, UI_SEP_CHAR);
    if (str_sep != nullptr) {
      return (str_sep - but->drawstr);
    }
  }
  return strlen(but->drawstr);
}

size_t ui_but_drawstr_without_sep_char(const uiBut *but, char *str, size_t str_maxncpy)
{
  size_t str_len_clip = ui_but_drawstr_len_without_sep_char(but);
  return BLI_strncpy_rlen(str, but->drawstr, min_zz(str_len_clip + 1, str_maxncpy));
}

size_t ui_but_tip_len_only_first_line(const uiBut *but)
{
  if (but->tip == nullptr) {
    return 0;
  }
  const char *str_sep = BLI_strchr_or_end(but->tip, '\n');
  return (str_sep - but->tip);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Block (#uiBlock) State
 * \{ */

uiBut *ui_block_active_but_get(const uiBlock *block)
{
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    if (but->active) {
      return but;
    }
  }

  return nullptr;
}

bool ui_block_is_menu(const uiBlock *block)
{
  return (((block->flag & UI_BLOCK_LOOP) != 0) &&
          /* non-menu popups use keep-open, so check this is off */
          ((block->flag & UI_BLOCK_KEEP_OPEN) == 0));
}

bool ui_block_is_popover(const uiBlock *block)
{
  return (block->flag & UI_BLOCK_POPOVER) != 0;
}

bool ui_block_is_pie_menu(const uiBlock *block)
{
  return ((block->flag & UI_BLOCK_RADIAL) != 0);
}

bool ui_block_is_popup_any(const uiBlock *block)
{
  return (ui_block_is_menu(block) || ui_block_is_popover(block) || ui_block_is_pie_menu(block));
}

static const uiBut *ui_but_next_non_separator(const uiBut *but)
{
  for (; but; but = but->next) {
    if (!ELEM(but->type, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE)) {
      return but;
    }
  }
  return nullptr;
}

bool UI_block_is_empty_ex(const uiBlock *block, const bool skip_title)
{
  const uiBut *but = static_cast<const uiBut *>(block->buttons.first);
  if (skip_title) {
    /* Skip the first label, since popups often have a title,
     * we may want to consider the block empty in this case. */
    but = ui_but_next_non_separator(but);
    if (but && but->type == UI_BTYPE_LABEL) {
      but = but->next;
    }
  }
  return (ui_but_next_non_separator(but) == nullptr);
}

bool UI_block_is_empty(const uiBlock *block)
{
  return UI_block_is_empty_ex(block, false);
}

bool UI_block_can_add_separator(const uiBlock *block)
{
  if (ui_block_is_menu(block) && !ui_block_is_pie_menu(block)) {
    const uiBut *but = static_cast<const uiBut *>(block->buttons.last);
    return (but && !ELEM(but->type, UI_BTYPE_SEPR_LINE, UI_BTYPE_SEPR));
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Block (#uiBlock) Spatial
 * \{ */

uiBlock *ui_block_find_mouse_over_ex(const ARegion *region, const int xy[2], bool only_clip)
{
  if (!ui_region_contains_point_px(region, xy)) {
    return nullptr;
  }
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    if (only_clip) {
      if ((block->flag & UI_BLOCK_CLIP_EVENTS) == 0) {
        continue;
      }
    }
    float mx = xy[0], my = xy[1];
    ui_window_to_block_fl(region, block, &mx, &my);
    if (BLI_rctf_isect_pt(&block->rect, mx, my)) {
      return block;
    }
  }
  return nullptr;
}

uiBlock *ui_block_find_mouse_over(const ARegion *region, const wmEvent *event, bool only_clip)
{
  return ui_block_find_mouse_over_ex(region, event->xy, only_clip);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region (#ARegion) State
 * \{ */

uiBut *ui_region_find_active_but(ARegion *region)
{
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    uiBut *but = ui_block_active_but_get(block);
    if (but) {
      return but;
    }
  }

  return nullptr;
}

uiBut *ui_region_find_first_but_test_flag(ARegion *region, int flag_include, int flag_exclude)
{
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
      if (((but->flag & flag_include) == flag_include) && ((but->flag & flag_exclude) == 0)) {
        return but;
      }
    }
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region (#ARegion) Spatial
 * \{ */

bool ui_region_contains_point_px(const ARegion *region, const int xy[2])
{
  rcti winrct;
  ui_region_winrct_get_no_margin(region, &winrct);
  if (!BLI_rcti_isect_pt_v(&winrct, xy)) {
    return false;
  }

  /* also, check that with view2d, that the mouse is not over the scroll-bars
   * NOTE: care is needed here, since the mask rect may include the scroll-bars
   * even when they are not visible, so we need to make a copy of the mask to
   * use to check
   */
  if (region->v2d.mask.xmin != region->v2d.mask.xmax) {
    const View2D *v2d = &region->v2d;
    int mx = xy[0], my = xy[1];

    ui_window_to_region(region, &mx, &my);
    if (!BLI_rcti_isect_pt(&v2d->mask, mx, my) ||
        UI_view2d_mouse_in_scrollers(region, &region->v2d, xy))
    {
      return false;
    }
  }

  return true;
}

bool ui_region_contains_rect_px(const ARegion *region, const rcti *rect_px)
{
  rcti winrct;
  ui_region_winrct_get_no_margin(region, &winrct);
  if (!BLI_rcti_isect(&winrct, rect_px, nullptr)) {
    return false;
  }

  /* See comment in 'ui_region_contains_point_px' */
  if (region->v2d.mask.xmin != region->v2d.mask.xmax) {
    const View2D *v2d = &region->v2d;
    rcti rect_region;
    ui_window_to_region_rcti(region, &rect_region, rect_px);
    if (!BLI_rcti_isect(&v2d->mask, &rect_region, nullptr) ||
        UI_view2d_rect_in_scrollers(region, &region->v2d, rect_px))
    {
      return false;
    }
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen (#bScreen) Spatial
 * \{ */

ARegion *ui_screen_region_find_mouse_over_ex(bScreen *screen, const int xy[2])
{
  LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
    rcti winrct;

    ui_region_winrct_get_no_margin(region, &winrct);

    if (BLI_rcti_isect_pt_v(&winrct, xy)) {
      return region;
    }
  }
  return nullptr;
}

ARegion *ui_screen_region_find_mouse_over(bScreen *screen, const wmEvent *event)
{
  return ui_screen_region_find_mouse_over_ex(screen, event->xy);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Manage Internal State
 * \{ */

void ui_interface_tag_script_reload_queries()
{
  g_ot_tool_set_by_id = nullptr;
}

/** \} */
