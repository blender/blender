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
 */

/** \file
 * \ingroup edinterface
 *
 * Utilities to inspect the interface, extract information.
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "DNA_screen_types.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "interface_intern.h"

#include "WM_api.h"
#include "WM_types.h"

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
               UI_BTYPE_PROGRESS_BAR);
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

/**
 * Can we mouse over the button or is it hidden/disabled/layout.
 * \note ctrl is kind of a hack currently,
 * so that non-embossed UI_BTYPE_TEXT button behaves as a label when ctrl is not pressed.
 */
bool ui_but_is_interactive(const uiBut *but, const bool labeledit)
{
  /* note, UI_BTYPE_LABEL is included for highlights, this allows drags */
  if ((but->type == UI_BTYPE_LABEL) && but->dragpoin == NULL) {
    return false;
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
  if ((but->type == UI_BTYPE_TEXT) && (but->dt == UI_EMBOSS_NONE) && !labeledit) {
    return false;
  }
  if ((but->type == UI_BTYPE_LISTROW) && labeledit) {
    return false;
  }

  return true;
}

/* file selectors are exempt from utf-8 checks */
bool ui_but_is_utf8(const uiBut *but)
{
  if (but->rnaprop) {
    const int subtype = RNA_property_subtype(but->rnaprop);
    return !(ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME, PROP_BYTESTRING));
  }
  else {
    return !(but->flag & UI_BUT_NO_UTF8);
  }
}

#ifdef USE_UI_POPOVER_ONCE
bool ui_but_is_popover_once_compat(const uiBut *but)
{
  return ((but->type == UI_BTYPE_BUT) || ui_but_is_toggle(but));
}
#endif

bool ui_but_has_array_value(const uiBut *but)
{
  return (but->rnapoin.data && but->rnaprop &&
          ELEM(RNA_property_subtype(but->rnaprop),
               PROP_COLOR,
               PROP_TRANSLATION,
               PROP_DIRECTION,
               PROP_VELOCITY,
               PROP_ACCELERATION,
               PROP_MATRIX,
               PROP_EULER,
               PROP_QUATERNION,
               PROP_AXISANGLE,
               PROP_XYZ,
               PROP_XYZ_LENGTH,
               PROP_COLOR_GAMMA,
               PROP_COORDS));
}

bool UI_but_is_tool(const uiBut *but)
{
  /* very evil! */
  if (but->optype != NULL) {
    static wmOperatorType *ot = NULL;
    if (ot == NULL) {
      ot = WM_operatortype_find("WM_OT_tool_set_by_id", false);
    }
    if (but->optype == ot) {
      return true;
    }
  }
  return false;
}

bool UI_but_has_tooltip_label(const uiBut *but)
{
  if ((but->drawstr[0] == '\0') && !ui_block_is_popover(but->block)) {
    return UI_but_is_tool(but);
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Button (#uiBut) Spatial
 * \{ */

void ui_but_pie_dir(RadialDirection dir, float vec[2])
{
  float angle;

  BLI_assert(dir != UI_RADIAL_NONE);

  angle = DEG2RADF((float)ui_radial_dir_to_angle[dir]);
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
  return BLI_rctf_isect(&but->rect, rect, NULL);
}

bool ui_but_contains_point_px(const uiBut *but, const ARegion *ar, int x, int y)
{
  uiBlock *block = but->block;
  if (!ui_region_contains_point_px(ar, x, y)) {
    return false;
  }

  float mx = x, my = y;
  ui_window_to_block_fl(ar, block, &mx, &my);

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

bool ui_but_contains_point_px_icon(const uiBut *but, ARegion *ar, const wmEvent *event)
{
  rcti rect;
  int x = event->x, y = event->y;

  ui_window_to_block(ar, but->block, &x, &y);

  BLI_rcti_rctf_copy(&rect, &but->rect);

  if (but->imb || but->type == UI_BTYPE_COLOR) {
    /* use button size itself */
  }
  else if (but->drawflag & UI_BUT_ICON_LEFT) {
    rect.xmax = rect.xmin + (BLI_rcti_size_y(&rect));
  }
  else {
    int delta = BLI_rcti_size_x(&rect) - BLI_rcti_size_y(&rect);
    rect.xmin += delta / 2;
    rect.xmax -= delta / 2;
  }

  return BLI_rcti_isect_pt(&rect, x, y);
}

/* x and y are only used in case event is NULL... */
uiBut *ui_but_find_mouse_over_ex(ARegion *ar, const int x, const int y, const bool labeledit)
{
  uiBut *butover = NULL;

  if (!ui_region_contains_point_px(ar, x, y)) {
    return NULL;
  }
  for (uiBlock *block = ar->uiblocks.first; block; block = block->next) {
    float mx = x, my = y;
    ui_window_to_block_fl(ar, block, &mx, &my);

    for (uiBut *but = block->buttons.last; but; but = but->prev) {
      if (ui_but_is_interactive(but, labeledit)) {
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

uiBut *ui_but_find_mouse_over(ARegion *ar, const wmEvent *event)
{
  return ui_but_find_mouse_over_ex(ar, event->x, event->y, event->ctrl != 0);
}

uiBut *ui_but_find_rect_over(const struct ARegion *ar, const rcti *rect_px)
{
  if (!ui_region_contains_rect_px(ar, rect_px)) {
    return NULL;
  }

  /* Currently no need to expose this at the moment. */
  bool labeledit = true;
  rctf rect_px_fl;
  BLI_rctf_rcti_copy(&rect_px_fl, rect_px);
  uiBut *butover = NULL;

  for (uiBlock *block = ar->uiblocks.first; block; block = block->next) {
    rctf rect_block;
    ui_window_to_block_rctf(ar, block, &rect_block, &rect_px_fl);

    for (uiBut *but = block->buttons.last; but; but = but->prev) {
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
      if (BLI_rctf_isect(&block->rect, &rect_block, NULL)) {
        break;
      }
    }
  }
  return butover;
}

uiBut *ui_list_find_mouse_over_ex(ARegion *ar, int x, int y)
{
  if (!ui_region_contains_point_px(ar, x, y)) {
    return NULL;
  }
  for (uiBlock *block = ar->uiblocks.first; block; block = block->next) {
    float mx = x, my = y;
    ui_window_to_block_fl(ar, block, &mx, &my);
    for (uiBut *but = block->buttons.last; but; but = but->prev) {
      if (but->type == UI_BTYPE_LISTBOX && ui_but_contains_pt(but, mx, my)) {
        return but;
      }
    }
  }

  return NULL;
}

uiBut *ui_list_find_mouse_over(ARegion *ar, const wmEvent *event)
{
  return ui_list_find_mouse_over_ex(ar, event->x, event->y);
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
  return NULL;
}

uiBut *ui_but_next(uiBut *but)
{
  while (but->next) {
    but = but->next;
    if (ui_but_is_editable(but)) {
      return but;
    }
  }
  return NULL;
}

uiBut *ui_but_first(uiBlock *block)
{
  uiBut *but;

  but = block->buttons.first;
  while (but) {
    if (ui_but_is_editable(but)) {
      return but;
    }
    but = but->next;
  }
  return NULL;
}

uiBut *ui_but_last(uiBlock *block)
{
  uiBut *but;

  but = block->buttons.last;
  while (but) {
    if (ui_but_is_editable(but)) {
      return but;
    }
    but = but->prev;
  }
  return NULL;
}

bool ui_but_is_cursor_warp(const uiBut *but)
{
  if (U.uiflag & USER_CONTINUOUS_MOUSE) {
    if (ELEM(but->type,
             UI_BTYPE_NUM,
             UI_BTYPE_NUM_SLIDER,
             UI_BTYPE_HSVCIRCLE,
             UI_BTYPE_TRACK_PREVIEW,
             UI_BTYPE_HSVCUBE,
             UI_BTYPE_CURVE)) {
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
/** \name Block (#uiBlock) State
 * \{ */

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

bool UI_block_is_empty(const uiBlock *block)
{
  for (const uiBut *but = block->buttons.first; but; but = but->next) {
    if (!ELEM(but->type, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE)) {
      return false;
    }
  }
  return true;
}

bool UI_block_can_add_separator(const uiBlock *block)
{
  if (ui_block_is_menu(block) && !ui_block_is_pie_menu(block)) {
    const uiBut *but = block->buttons.last;
    return (but && !ELEM(but->type, UI_BTYPE_SEPR_LINE, UI_BTYPE_SEPR));
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region (#ARegion) State
 * \{ */

uiBut *ui_region_find_active_but(ARegion *ar)
{
  for (uiBlock *block = ar->uiblocks.first; block; block = block->next) {
    for (uiBut *but = block->buttons.first; but; but = but->next) {
      if (but->active) {
        return but;
      }
    }
  }

  return NULL;
}

uiBut *ui_region_find_first_but_test_flag(ARegion *ar, int flag_include, int flag_exclude)
{
  for (uiBlock *block = ar->uiblocks.first; block; block = block->next) {
    for (uiBut *but = block->buttons.first; but; but = but->next) {
      if (((but->flag & flag_include) == flag_include) && ((but->flag & flag_exclude) == 0)) {
        return but;
      }
    }
  }

  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region (#ARegion) Spatial
 * \{ */

bool ui_region_contains_point_px(const ARegion *ar, int x, int y)
{
  rcti winrct;
  ui_region_winrct_get_no_margin(ar, &winrct);
  if (!BLI_rcti_isect_pt(&winrct, x, y)) {
    return false;
  }

  /* also, check that with view2d, that the mouse is not over the scrollbars
   * NOTE: care is needed here, since the mask rect may include the scrollbars
   * even when they are not visible, so we need to make a copy of the mask to
   * use to check
   */
  if (ar->v2d.mask.xmin != ar->v2d.mask.xmax) {
    const View2D *v2d = &ar->v2d;
    int mx = x, my = y;

    ui_window_to_region(ar, &mx, &my);
    if (!BLI_rcti_isect_pt(&v2d->mask, mx, my) ||
        UI_view2d_mouse_in_scrollers(ar, &ar->v2d, x, y)) {
      return false;
    }
  }

  return true;
}

bool ui_region_contains_rect_px(const ARegion *ar, const rcti *rect_px)
{
  rcti winrct;
  ui_region_winrct_get_no_margin(ar, &winrct);
  if (!BLI_rcti_isect(&winrct, rect_px, NULL)) {
    return false;
  }

  /* See comment in 'ui_region_contains_point_px' */
  if (ar->v2d.mask.xmin != ar->v2d.mask.xmax) {
    const View2D *v2d = &ar->v2d;
    rcti rect_region;
    ui_window_to_region_rcti(ar, &rect_region, rect_px);
    if (!BLI_rcti_isect(&v2d->mask, &rect_region, NULL) ||
        UI_view2d_rect_in_scrollers(ar, &ar->v2d, rect_px)) {
      return false;
    }
  }

  return true;
}

/** \} */
