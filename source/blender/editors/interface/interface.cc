/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cctype>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstddef> /* `offsetof()` */
#include <cstring>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_workspace_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BLI_utildefines.h"

#include "BLO_readfile.h"

#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_idprop.h"
#include "BKE_main.hh"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.hh"
#include "BKE_unit.hh"

#include "ED_asset.hh"

#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BLF_api.hh"
#include "BLT_translation.h"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_string_search.hh"
#include "UI_view2d.hh"

#include "IMB_imbuf.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern_run.h"
#endif

#include "ED_numinput.hh"
#include "ED_screen.hh"

#include "IMB_colormanagement.hh"

#include "DEG_depsgraph_query.hh"

#include "interface_intern.hh"

using blender::StringRef;
using blender::Vector;

/* prototypes. */
static void ui_def_but_rna__menu(bContext *C, uiLayout *layout, void *but_p);
static void ui_def_but_rna__panel_type(bContext * /*C*/, uiLayout *layout, void *but_p);
static void ui_def_but_rna__menu_type(bContext * /*C*/, uiLayout *layout, void *but_p);

/* avoid unneeded calls to ui_but_value_get */
#define UI_BUT_VALUE_UNSET DBL_MAX
#define UI_GET_BUT_VALUE_INIT(_but, _value) \
  if (_value == DBL_MAX) { \
    (_value) = ui_but_value_get(_but); \
  } \
  ((void)0)

#define B_NOP -1

/**
 * a full doc with API notes can be found in 'blender/doc/guides/interface_API.txt'
 *
 * `uiBlahBlah()`   external function.
 * `ui_blah_blah()` internal function.
 */

static void ui_but_free(const bContext *C, uiBut *but);

static bool ui_but_is_unit_radians_ex(UnitSettings *unit, const int unit_type)
{
  return (unit->system_rotation == USER_UNIT_ROT_RADIANS && unit_type == PROP_UNIT_ROTATION);
}

static bool ui_but_is_unit_radians(const uiBut *but)
{
  UnitSettings *unit = but->block->unit;
  const int unit_type = UI_but_unit_type_get(but);

  return ui_but_is_unit_radians_ex(unit, unit_type);
}

/* ************* window matrix ************** */

void ui_block_to_region_fl(const ARegion *region, const uiBlock *block, float *r_x, float *r_y)
{
  const int getsizex = BLI_rcti_size_x(&region->winrct) + 1;
  const int getsizey = BLI_rcti_size_y(&region->winrct) + 1;

  float gx = *r_x;
  float gy = *r_y;

  if (block->panel) {
    gx += block->panel->ofsx;
    gy += block->panel->ofsy;
  }

  *r_x = float(getsizex) * (0.5f + 0.5f * (gx * block->winmat[0][0] + gy * block->winmat[1][0] +
                                           block->winmat[3][0]));
  *r_y = float(getsizey) * (0.5f + 0.5f * (gx * block->winmat[0][1] + gy * block->winmat[1][1] +
                                           block->winmat[3][1]));
}

void ui_block_to_window_fl(const ARegion *region, const uiBlock *block, float *r_x, float *r_y)
{
  ui_block_to_region_fl(region, block, r_x, r_y);
  *r_x += region->winrct.xmin;
  *r_y += region->winrct.ymin;
}

void ui_block_to_window(const ARegion *region, const uiBlock *block, int *r_x, int *r_y)
{
  float fx = *r_x;
  float fy = *r_y;

  ui_block_to_window_fl(region, block, &fx, &fy);

  *r_x = int(lround(fx));
  *r_y = int(lround(fy));
}

void ui_block_to_region_rctf(const ARegion *region,
                             const uiBlock *block,
                             rctf *rct_dst,
                             const rctf *rct_src)
{
  *rct_dst = *rct_src;
  ui_block_to_region_fl(region, block, &rct_dst->xmin, &rct_dst->ymin);
  ui_block_to_region_fl(region, block, &rct_dst->xmax, &rct_dst->ymax);
}

void ui_block_to_window_rctf(const ARegion *region,
                             const uiBlock *block,
                             rctf *rct_dst,
                             const rctf *rct_src)
{
  *rct_dst = *rct_src;
  ui_block_to_window_fl(region, block, &rct_dst->xmin, &rct_dst->ymin);
  ui_block_to_window_fl(region, block, &rct_dst->xmax, &rct_dst->ymax);
}

float ui_block_to_window_scale(const ARegion *region, const uiBlock *block)
{
  /* We could have function for this to avoid dummy arg. */
  float min_y = 0, max_y = 1;
  float dummy_x = 0.0f;
  ui_block_to_window_fl(region, block, &dummy_x, &min_y);
  dummy_x = 0.0f;
  ui_block_to_window_fl(region, block, &dummy_x, &max_y);
  return max_y - min_y;
}

void ui_window_to_block_fl(const ARegion *region, const uiBlock *block, float *r_x, float *r_y)
{
  const int getsizex = BLI_rcti_size_x(&region->winrct) + 1;
  const int getsizey = BLI_rcti_size_y(&region->winrct) + 1;
  const int sx = region->winrct.xmin;
  const int sy = region->winrct.ymin;

  const float a = 0.5f * float(getsizex) * block->winmat[0][0];
  const float b = 0.5f * float(getsizex) * block->winmat[1][0];
  const float c = 0.5f * float(getsizex) * (1.0f + block->winmat[3][0]);

  const float d = 0.5f * float(getsizey) * block->winmat[0][1];
  const float e = 0.5f * float(getsizey) * block->winmat[1][1];
  const float f = 0.5f * float(getsizey) * (1.0f + block->winmat[3][1]);

  const float px = *r_x - sx;
  const float py = *r_y - sy;

  *r_y = (a * (py - f) + d * (c - px)) / (a * e - d * b);
  *r_x = (px - b * (*r_y) - c) / a;

  if (block->panel) {
    *r_x -= block->panel->ofsx;
    *r_y -= block->panel->ofsy;
  }
}

void ui_window_to_block_rctf(const ARegion *region,
                             const uiBlock *block,
                             rctf *rct_dst,
                             const rctf *rct_src)
{
  *rct_dst = *rct_src;
  ui_window_to_block_fl(region, block, &rct_dst->xmin, &rct_dst->ymin);
  ui_window_to_block_fl(region, block, &rct_dst->xmax, &rct_dst->ymax);
}

void ui_window_to_block(const ARegion *region, const uiBlock *block, int *r_x, int *r_y)
{
  float fx = *r_x;
  float fy = *r_y;

  ui_window_to_block_fl(region, block, &fx, &fy);

  *r_x = int(lround(fx));
  *r_y = int(lround(fy));
}

void ui_window_to_region(const ARegion *region, int *r_x, int *r_y)
{
  *r_x -= region->winrct.xmin;
  *r_y -= region->winrct.ymin;
}

void ui_window_to_region_rcti(const ARegion *region, rcti *rect_dst, const rcti *rct_src)
{
  rect_dst->xmin = rct_src->xmin - region->winrct.xmin;
  rect_dst->xmax = rct_src->xmax - region->winrct.xmin;
  rect_dst->ymin = rct_src->ymin - region->winrct.ymin;
  rect_dst->ymax = rct_src->ymax - region->winrct.ymin;
}

void ui_window_to_region_rctf(const ARegion *region, rctf *rect_dst, const rctf *rct_src)
{
  rect_dst->xmin = rct_src->xmin - region->winrct.xmin;
  rect_dst->xmax = rct_src->xmax - region->winrct.xmin;
  rect_dst->ymin = rct_src->ymin - region->winrct.ymin;
  rect_dst->ymax = rct_src->ymax - region->winrct.ymin;
}

void ui_region_to_window(const ARegion *region, int *r_x, int *r_y)
{
  *r_x += region->winrct.xmin;
  *r_y += region->winrct.ymin;
}

static void ui_update_flexible_spacing(const ARegion *region, uiBlock *block)
{
  int sepr_flex_len = 0;
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    if (but->type == UI_BTYPE_SEPR_SPACER) {
      sepr_flex_len++;
    }
  }

  if (sepr_flex_len == 0) {
    return;
  }

  rcti rect;
  ui_but_to_pixelrect(&rect, region, block, static_cast<const uiBut *>(block->buttons.last));
  const float buttons_width = float(rect.xmax) + UI_HEADER_OFFSET;
  const float region_width = float(region->sizex) * UI_SCALE_FAC;

  if (region_width <= buttons_width) {
    return;
  }

  /* We could get rid of this loop if we agree on a max number of spacer */
  Vector<int, 8> spacers_pos;
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    if (but->type == UI_BTYPE_SEPR_SPACER) {
      ui_but_to_pixelrect(&rect, region, block, but);
      spacers_pos.append(rect.xmax + UI_HEADER_OFFSET);
    }
  }

  const float view_scale_x = UI_view2d_scale_get_x(&region->v2d);
  const float segment_width = region_width / float(sepr_flex_len);
  float offset = 0, remaining_space = region_width - buttons_width;
  int i = 0;
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    BLI_rctf_translate(&but->rect, offset / view_scale_x, 0);
    if (but->type == UI_BTYPE_SEPR_SPACER) {
      /* How much the next block overlap with the current segment */
      int overlap = ((i == sepr_flex_len - 1) ? buttons_width - spacers_pos[i] :
                                                (spacers_pos[i + 1] - spacers_pos[i]) / 2);
      const int segment_end = segment_width * (i + 1);
      const int spacer_end = segment_end - overlap;
      const int spacer_sta = spacers_pos[i] + offset;
      if (spacer_end > spacer_sta) {
        const float step = min_ff(remaining_space, spacer_end - spacer_sta);
        remaining_space -= step;
        offset += step;
      }
      i++;
    }
  }
  ui_block_bounds_calc(block);
}

static void ui_update_window_matrix(const wmWindow *window, const ARegion *region, uiBlock *block)
{
  /* window matrix and aspect */
  if (region && region->visible) {
    /* Get projection matrix which includes View2D translation and zoom. */
    GPU_matrix_projection_get(block->winmat);
    block->aspect = 2.0f / fabsf(region->winx * block->winmat[0][0]);
  }
  else {
    /* No sub-window created yet, for menus for example, so we use the main
     * window instead, since buttons are created there anyway. */
    const int width = WM_window_pixels_x(window);
    const int height = WM_window_pixels_y(window);
    const rcti winrct = {0, width - 1, 0, height - 1};

    wmGetProjectionMatrix(block->winmat, &winrct);
    block->aspect = 2.0f / fabsf(width * block->winmat[0][0]);
  }
}

void ui_region_winrct_get_no_margin(const ARegion *region, rcti *r_rect)
{
  uiBlock *block = static_cast<uiBlock *>(region->uiblocks.first);
  if (block && (block->flag & UI_BLOCK_LOOP) && (block->flag & UI_BLOCK_RADIAL) == 0) {
    BLI_rcti_rctf_copy_floor(r_rect, &block->rect);
    BLI_rcti_translate(r_rect, region->winrct.xmin, region->winrct.ymin);
  }
  else {
    *r_rect = region->winrct;
  }
}

/* ******************* block calc ************************* */

void UI_block_translate(uiBlock *block, int x, int y)
{
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    BLI_rctf_translate(&but->rect, x, y);
  }

  BLI_rctf_translate(&block->rect, x, y);
}

static bool ui_but_is_row_alignment_group(const uiBut *left, const uiBut *right)
{
  const bool is_same_align_group = (left->alignnr && (left->alignnr == right->alignnr));
  return is_same_align_group && (left->rect.xmin < right->rect.xmin);
}

static void ui_block_bounds_calc_text(uiBlock *block, float offset)
{
  const uiStyle *style = UI_style_get();
  uiBut *col_bt;
  int i = 0, j, x1addval = offset;

  UI_fontstyle_set(&style->widget);

  uiBut *init_col_bt = static_cast<uiBut *>(block->buttons.first);
  LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
    if (!ELEM(bt->type, UI_BTYPE_SEPR, UI_BTYPE_SEPR_LINE, UI_BTYPE_SEPR_SPACER)) {
      j = BLF_width(style->widget.uifont_id, bt->drawstr.c_str(), bt->drawstr.size());

      if (j > i) {
        i = j;
      }
    }

    /* Skip all buttons that are in a horizontal alignment group.
     * We don't want to split them apart (but still check the row's width and apply current
     * offsets). */
    if (bt->next && ui_but_is_row_alignment_group(bt, bt->next)) {
      int width = 0;
      const int alignnr = bt->alignnr;
      for (col_bt = bt; col_bt && col_bt->alignnr == alignnr; col_bt = col_bt->next) {
        width += BLI_rctf_size_x(&col_bt->rect);
        col_bt->rect.xmin += x1addval;
        col_bt->rect.xmax += x1addval;
      }
      if (width > i) {
        i = width;
      }
      /* Give the following code the last button in the alignment group, there might have to be a
       * split immediately after. */
      bt = col_bt ? col_bt->prev : nullptr;
    }

    if (bt && bt->next && bt->rect.xmin < bt->next->rect.xmin) {
      /* End of this column, and it's not the last one. */
      for (col_bt = init_col_bt; col_bt->prev != bt; col_bt = col_bt->next) {
        col_bt->rect.xmin = x1addval;
        col_bt->rect.xmax = x1addval + i + block->bounds;

        ui_but_update(col_bt); /* clips text again */
      }

      /* And we prepare next column. */
      x1addval += i + block->bounds;
      i = 0;
      init_col_bt = col_bt;
    }
  }

  /* Last column. */
  for (col_bt = init_col_bt; col_bt; col_bt = col_bt->next) {
    /* Recognize a horizontally arranged alignment group and skip its items. */
    if (col_bt->next && ui_but_is_row_alignment_group(col_bt, col_bt->next)) {
      const int alignnr = col_bt->alignnr;
      for (; col_bt && col_bt->alignnr == alignnr; col_bt = col_bt->next) {
        /* pass */
      }
    }
    if (!col_bt) {
      break;
    }

    col_bt->rect.xmin = x1addval;
    col_bt->rect.xmax = max_ff(x1addval + i + block->bounds, offset + block->minbounds);

    ui_but_update(col_bt); /* clips text again */
  }
}

void ui_block_bounds_calc(uiBlock *block)
{
  if (BLI_listbase_is_empty(&block->buttons)) {
    if (block->panel) {
      block->rect.xmin = 0.0;
      block->rect.xmax = block->panel->sizex;
      block->rect.ymin = 0.0;
      block->rect.ymax = block->panel->sizey;
    }
  }
  else {

    BLI_rctf_init_minmax(&block->rect);

    LISTBASE_FOREACH (uiBut *, bt, &block->buttons) {
      BLI_rctf_union(&block->rect, &bt->rect);
    }

    block->rect.xmin -= block->bounds;
    block->rect.ymin -= block->bounds;
    block->rect.xmax += block->bounds;
    block->rect.ymax += block->bounds;
  }

  block->rect.xmax = block->rect.xmin + max_ff(BLI_rctf_size_x(&block->rect), block->minbounds);

  /* hardcoded exception... but that one is annoying with larger safety */
  uiBut *bt = static_cast<uiBut *>(block->buttons.first);
  const int xof = ((bt && STRPREFIX(bt->str.c_str(), "ERROR")) ? 10 : 40) * UI_SCALE_FAC;

  block->safety.xmin = block->rect.xmin - xof;
  block->safety.ymin = block->rect.ymin - xof;
  block->safety.xmax = block->rect.xmax + xof;
  block->safety.ymax = block->rect.ymax + xof;
}

static void ui_block_bounds_calc_centered(wmWindow *window, uiBlock *block)
{
  /* NOTE: this is used for the splash where window bounds event has not been
   * updated by ghost, get the window bounds from ghost directly */

  const int xmax = WM_window_pixels_x(window);
  const int ymax = WM_window_pixels_y(window);

  ui_block_bounds_calc(block);

  const int width = BLI_rctf_size_x(&block->rect);
  const int height = BLI_rctf_size_y(&block->rect);

  const int startx = (xmax * 0.5f) - (width * 0.5f);
  const int starty = (ymax * 0.5f) - (height * 0.5f);

  UI_block_translate(block, startx - block->rect.xmin, starty - block->rect.ymin);

  /* now recompute bounds and safety */
  ui_block_bounds_calc(block);
}

static void ui_block_bounds_calc_centered_pie(uiBlock *block)
{
  const int xy[2] = {
      int(block->pie_data.pie_center_spawned[0]),
      int(block->pie_data.pie_center_spawned[1]),
  };

  UI_block_translate(block, xy[0], xy[1]);

  /* now recompute bounds and safety */
  ui_block_bounds_calc(block);
}

static void ui_block_bounds_calc_popup(
    wmWindow *window, uiBlock *block, eBlockBoundsCalc bounds_calc, const int xy[2], int r_xy[2])
{
  const int oldbounds = block->bounds;

  /* compute mouse position with user defined offset */
  ui_block_bounds_calc(block);

  const int xmax = WM_window_pixels_x(window);
  const int ymax = WM_window_pixels_y(window);

  int oldwidth = BLI_rctf_size_x(&block->rect);
  int oldheight = BLI_rctf_size_y(&block->rect);

  /* first we ensure wide enough text bounds */
  if (bounds_calc == UI_BLOCK_BOUNDS_POPUP_MENU) {
    if (block->flag & UI_BLOCK_LOOP) {
      block->bounds = 2.5f * UI_UNIT_X;
      ui_block_bounds_calc_text(block, block->rect.xmin);
    }
  }

  /* next we recompute bounds */
  block->bounds = oldbounds;
  ui_block_bounds_calc(block);

  /* and we adjust the position to fit within window */
  const int width = BLI_rctf_size_x(&block->rect);
  const int height = BLI_rctf_size_y(&block->rect);

  /* avoid divide by zero below, caused by calling with no UI, but better not crash */
  oldwidth = oldwidth > 0 ? oldwidth : std::max(1, width);
  oldheight = oldheight > 0 ? oldheight : std::max(1, height);

  /* offset block based on mouse position, user offset is scaled
   * along in case we resized the block in ui_block_bounds_calc_text */
  rcti rect;
  const int raw_x = rect.xmin = xy[0] + block->rect.xmin +
                                (block->bounds_offset[0] * width) / oldwidth;
  int raw_y = rect.ymin = xy[1] + block->rect.ymin +
                          (block->bounds_offset[1] * height) / oldheight;
  rect.xmax = rect.xmin + width;
  rect.ymax = rect.ymin + height;

  rcti rect_bounds;
  const int margin = UI_SCREEN_MARGIN;
  rect_bounds.xmin = margin;
  rect_bounds.ymin = margin;
  rect_bounds.xmax = xmax - margin;
  rect_bounds.ymax = ymax - UI_POPUP_MENU_TOP;

  int ofs_dummy[2];
  BLI_rcti_clamp(&rect, &rect_bounds, ofs_dummy);
  UI_block_translate(block, rect.xmin - block->rect.xmin, rect.ymin - block->rect.ymin);

  /* now recompute bounds and safety */
  ui_block_bounds_calc(block);

  /* If given, adjust input coordinates such that they would generate real final popup position.
   * Needed to handle correctly floating panels once they have been dragged around,
   * see #52999. */
  if (r_xy) {
    r_xy[0] = xy[0] + block->rect.xmin - raw_x;
    r_xy[1] = xy[1] + block->rect.ymin - raw_y;
  }
}

void UI_block_bounds_set_normal(uiBlock *block, int addval)
{
  if (block == nullptr) {
    return;
  }

  block->bounds = addval;
  block->bounds_type = UI_BLOCK_BOUNDS;
}

void UI_block_bounds_set_text(uiBlock *block, int addval)
{
  block->bounds = addval;
  block->bounds_type = UI_BLOCK_BOUNDS_TEXT;
}

void UI_block_bounds_set_popup(uiBlock *block, int addval, const int bounds_offset[2])
{
  block->bounds = addval;
  block->bounds_type = UI_BLOCK_BOUNDS_POPUP_MOUSE;
  if (bounds_offset != nullptr) {
    block->bounds_offset[0] = bounds_offset[0];
    block->bounds_offset[1] = bounds_offset[1];
  }
  else {
    block->bounds_offset[0] = 0;
    block->bounds_offset[1] = 0;
  }
}

void UI_block_bounds_set_menu(uiBlock *block, int addval, const int bounds_offset[2])
{
  block->bounds = addval;
  block->bounds_type = UI_BLOCK_BOUNDS_POPUP_MENU;
  if (bounds_offset != nullptr) {
    copy_v2_v2_int(block->bounds_offset, bounds_offset);
  }
  else {
    zero_v2_int(block->bounds_offset);
  }
}

void UI_block_bounds_set_centered(uiBlock *block, int addval)
{
  block->bounds = addval;
  block->bounds_type = UI_BLOCK_BOUNDS_POPUP_CENTER;
}

void UI_block_bounds_set_explicit(uiBlock *block, int minx, int miny, int maxx, int maxy)
{
  block->rect.xmin = minx;
  block->rect.ymin = miny;
  block->rect.xmax = maxx;
  block->rect.ymax = maxy;
  block->bounds_type = UI_BLOCK_BOUNDS_NONE;
}

static float ui_but_get_float_precision(uiBut *but)
{
  if (but->type == UI_BTYPE_NUM) {
    return ((uiButNumber *)but)->precision;
  }
  if (but->type == UI_BTYPE_NUM_SLIDER) {
    return ((uiButNumberSlider *)but)->precision;
  }

  return but->a2;
}

static float ui_but_get_float_step_size(uiBut *but)
{
  if (but->type == UI_BTYPE_NUM) {
    return ((uiButNumber *)but)->step_size;
  }
  if (but->type == UI_BTYPE_NUM_SLIDER) {
    return ((uiButNumberSlider *)but)->step_size;
  }

  return but->a1;
}

static bool ui_but_hide_fraction(uiBut *but, double value)
{
  /* Hide the fraction if both the value and the step are exact integers. */
  if (floor(value) == value) {
    const float step = ui_but_get_float_step_size(but) * UI_PRECISION_FLOAT_SCALE;

    if (floorf(step) == step) {
      /* Don't hide if it has any unit except frame count. */
      switch (UI_but_unit_type_get(but)) {
        case PROP_UNIT_NONE:
        case PROP_UNIT_TIME:
          return true;

        default:
          return false;
      }
    }
  }

  return false;
}

static int ui_but_calc_float_precision(uiBut *but, double value)
{
  if (ui_but_hide_fraction(but, value)) {
    return 0;
  }

  int prec = int(ui_but_get_float_precision(but));

  /* first check for various special cases:
   * * If button is radians, we want additional precision (see #39861).
   * * If prec is not set, we fallback to a simple default */
  if (ui_but_is_unit_radians(but) && prec < 5) {
    prec = 5;
  }
  else if (prec == -1) {
    prec = (but->hardmax < 10.001f) ? 3 : 2;
  }
  else {
    CLAMP(prec, 0, UI_PRECISION_FLOAT_MAX);
  }

  return UI_calc_float_precision(prec, value);
}

/* ************** BLOCK ENDING FUNCTION ************* */

bool ui_but_rna_equals(const uiBut *a, const uiBut *b)
{
  return ui_but_rna_equals_ex(a, &b->rnapoin, b->rnaprop, b->rnaindex);
}

bool ui_but_rna_equals_ex(const uiBut *but,
                          const PointerRNA *ptr,
                          const PropertyRNA *prop,
                          int index)
{
  if (but->rnapoin.data != ptr->data) {
    return false;
  }
  if (but->rnaprop != prop || but->rnaindex != index) {
    return false;
  }

  return true;
}

/* NOTE: if `but->poin` is allocated memory for every `uiDefBut*`, things fail. */
static bool ui_but_equals_old(const uiBut *but, const uiBut *oldbut)
{
  if (but->identity_cmp_func) {
    /* If the buttons have own identity comparator callbacks (and they match), use this to
     * determine equality. */
    if (but->identity_cmp_func && (but->type == oldbut->type) &&
        (but->identity_cmp_func == oldbut->identity_cmp_func))
    {
      /* Test if the comparison is symmetrical (if a == b then b == a), may help catch some issues.
       */
      BLI_assert(but->identity_cmp_func(but, oldbut) == but->identity_cmp_func(oldbut, but));

      return but->identity_cmp_func(but, oldbut);
    }
  }

  /* various properties are being compared here, hopefully sufficient
   * to catch all cases, but it is simple to add more checks later */
  if (but->retval != oldbut->retval) {
    return false;
  }
  if (!ui_but_rna_equals(but, oldbut)) {
    return false;
  }
  if (but->func != oldbut->func) {
    return false;
  }
  /* Compares the contained function pointers. Buttons with different apply functions can be
   * considered to do different things, and as such do not equal each other. */
  if (but->apply_func.target<void(bContext &)>() != oldbut->apply_func.target<void(bContext &)>())
  {
    return false;
  }
  if (but->funcN != oldbut->funcN) {
    return false;
  }
  if (!ELEM(oldbut->func_arg1, oldbut, but->func_arg1)) {
    return false;
  }
  if (!ELEM(oldbut->func_arg2, oldbut, but->func_arg2)) {
    return false;
  }
  if (!but->funcN && ((but->poin != oldbut->poin && (uiBut *)oldbut->poin != oldbut) ||
                      (but->pointype != oldbut->pointype)))
  {
    return false;
  }
  if (but->optype != oldbut->optype) {
    return false;
  }
  if (but->dragtype != oldbut->dragtype) {
    return false;
  }

  if ((but->type == UI_BTYPE_VIEW_ITEM) && (oldbut->type == UI_BTYPE_VIEW_ITEM)) {
    uiButViewItem *but_item = (uiButViewItem *)but;
    uiButViewItem *oldbut_item = (uiButViewItem *)oldbut;
    if (!but_item->view_item || !oldbut_item->view_item ||
        !UI_view_item_matches(but_item->view_item, oldbut_item->view_item))
    {
      return false;
    }
  }

  return true;
}

uiBut *ui_but_find_old(uiBlock *block_old, const uiBut *but_new)
{
  LISTBASE_FOREACH (uiBut *, but, &block_old->buttons) {
    if (ui_but_equals_old(but_new, but)) {
      return but;
    }
  }
  return nullptr;
}

uiBut *ui_but_find_new(uiBlock *block_new, const uiBut *but_old)
{
  LISTBASE_FOREACH (uiBut *, but, &block_new->buttons) {
    if (ui_but_equals_old(but, but_old)) {
      return but;
    }
  }
  return nullptr;
}

static bool ui_but_extra_icons_equals_old(const uiButExtraOpIcon *new_extra_icon,
                                          const uiButExtraOpIcon *old_extra_icon)
{
  return (new_extra_icon->optype_params->optype == old_extra_icon->optype_params->optype) &&
         (new_extra_icon->icon == old_extra_icon->icon);
}

static uiButExtraOpIcon *ui_but_extra_icon_find_old(const uiButExtraOpIcon *new_extra_icon,
                                                    const uiBut *old_but)
{
  LISTBASE_FOREACH (uiButExtraOpIcon *, op_icon, &old_but->extra_op_icons) {
    if (ui_but_extra_icons_equals_old(new_extra_icon, op_icon)) {
      return op_icon;
    }
  }
  return nullptr;
}

static void ui_but_extra_icons_update_from_old_but(const uiBut *new_but, const uiBut *old_but)
{
  /* Specifically for keeping some state info for the active button. */
  BLI_assert(old_but->active);

  LISTBASE_FOREACH (uiButExtraOpIcon *, new_extra_icon, &new_but->extra_op_icons) {
    uiButExtraOpIcon *old_extra_icon = ui_but_extra_icon_find_old(new_extra_icon, old_but);
    /* Keep the highlighting state, and let handling update it later. */
    if (old_extra_icon) {
      new_extra_icon->highlighted = old_extra_icon->highlighted;
    }
  }
}

/**
 * Update pointers and other information in the old active button based on new information in the
 * corresponding new button from the current layout pass.
 *
 * \param oldbut: The button from the last layout pass that will be moved to the new block.
 * \param but: The newly added button with much of the up to date information, to be feed later.
 *
 * \note #uiBut has ownership of many of its pointers. When the button is freed all these
 * pointers are freed as well, so ownership has to be moved out of \a but in order to free it.
 */
static void ui_but_update_old_active_from_new(uiBut *oldbut, uiBut *but)
{
  BLI_assert(oldbut->active);

  /* flags from the buttons we want to refresh, may want to add more here... */
  const int flag_copy = UI_BUT_REDALERT | UI_HAS_ICON | UI_SELECT_DRAW;
  const int drawflag_copy = UI_BUT_HAS_TOOLTIP_LABEL;

  /* still stuff needs to be copied */
  oldbut->rect = but->rect;
  oldbut->context = but->context; /* set by Layout */

  /* drawing */
  oldbut->icon = but->icon;
  oldbut->iconadd = but->iconadd;
  oldbut->alignnr = but->alignnr;

  /* typically the same pointers, but not on undo/redo */
  /* XXX some menu buttons store button itself in but->poin. Ugly */
  if (oldbut->poin != (char *)oldbut) {
    std::swap(oldbut->poin, but->poin);
    std::swap(oldbut->func_argN, but->func_argN);
  }

  /* Move tooltip from new to old. */
  std::swap(oldbut->tip_func, but->tip_func);
  std::swap(oldbut->tip_arg, but->tip_arg);
  std::swap(oldbut->tip_arg_free, but->tip_arg_free);
  std::swap(oldbut->tip_label_func, but->tip_label_func);

  oldbut->flag = (oldbut->flag & ~flag_copy) | (but->flag & flag_copy);
  oldbut->drawflag = (oldbut->drawflag & ~drawflag_copy) | (but->drawflag & drawflag_copy);

  ui_but_extra_icons_update_from_old_but(but, oldbut);
  std::swap(but->extra_op_icons, oldbut->extra_op_icons);

  if (oldbut->type == UI_BTYPE_SEARCH_MENU) {
    uiButSearch *search_oldbut = (uiButSearch *)oldbut, *search_but = (uiButSearch *)but;

    std::swap(search_oldbut->arg_free_fn, search_but->arg_free_fn);
    std::swap(search_oldbut->arg, search_but->arg);
  }

  /* copy hardmin for list rows to prevent 'sticking' highlight to mouse position
   * when scrolling without moving mouse (see #28432) */
  if (ELEM(oldbut->type, UI_BTYPE_ROW, UI_BTYPE_LISTROW)) {
    oldbut->hardmax = but->hardmax;
  }

  switch (oldbut->type) {
    case UI_BTYPE_PROGRESS: {
      uiButProgress *progress_oldbut = (uiButProgress *)oldbut;
      uiButProgress *progress_but = (uiButProgress *)but;
      progress_oldbut->progress_factor = progress_but->progress_factor;
      break;
    }
    case UI_BTYPE_VIEW_ITEM: {
      uiButViewItem *view_item_oldbut = (uiButViewItem *)oldbut;
      uiButViewItem *view_item_newbut = (uiButViewItem *)but;
      ui_view_item_swap_button_pointers(view_item_newbut->view_item, view_item_oldbut->view_item);
      std::swap(view_item_newbut->view_item, view_item_oldbut->view_item);
      break;
    }
    default:
      break;
  }

  /* move/copy string from the new button to the old */
  /* needed for alt+mouse wheel over enums */
  std::swap(but->str, oldbut->str);

  if (but->dragpoin) {
    std::swap(but->dragpoin, oldbut->dragpoin);
  }
  if (but->imb) {
    std::swap(but->imb, oldbut->imb);
  }

  /* NOTE: if layout hasn't been applied yet, it uses old button pointers... */
}

/**
 * \return true when \a but_p is set (only done for active buttons).
 */
static bool ui_but_update_from_old_block(const bContext *C,
                                         uiBlock *block,
                                         uiBut **but_p,
                                         uiBut **but_old_p)
{
  uiBlock *oldblock = block->oldblock;
  uiBut *but = *but_p;

#if 0
  /* Simple method - search every time. Keep this for easy testing of the "fast path." */
  uiBut *oldbut = ui_but_find_old(oldblock, but);
  UNUSED_VARS(but_old_p);
#else
  BLI_assert(*but_old_p == nullptr || BLI_findindex(&oldblock->buttons, *but_old_p) != -1);

  /* As long as old and new buttons are aligned, avoid loop-in-loop (calling #ui_but_find_old). */
  uiBut *oldbut;
  if (LIKELY(*but_old_p && ui_but_equals_old(but, *but_old_p))) {
    oldbut = *but_old_p;
  }
  else {
    /* Fallback to block search. */
    oldbut = ui_but_find_old(oldblock, but);
  }
  (*but_old_p) = oldbut ? oldbut->next : nullptr;
#endif

  bool found_active = false;

  if (!oldbut) {
    return false;
  }

  if (oldbut->active) {
    /* Move button over from oldblock to new block. */
    BLI_remlink(&oldblock->buttons, oldbut);
    BLI_insertlinkafter(&block->buttons, but, oldbut);
    /* Add the old button to the button groups in the new block. */
    ui_button_group_replace_but_ptr(block, but, oldbut);
    oldbut->block = block;
    *but_p = oldbut;

    ui_but_update_old_active_from_new(oldbut, but);

    if (!BLI_listbase_is_empty(&block->butstore)) {
      UI_butstore_register_update(block, oldbut, but);
    }

    BLI_remlink(&block->buttons, but);
    ui_but_free(C, but);

    found_active = true;
  }
  else {
    int flag_copy = UI_BUT_DRAG_MULTI;

    /* Stupid special case: The active button may be inside (as in, overlapped on top) a row
     * button which we also want to keep highlighted then. */
    if (ELEM(but->type, UI_BTYPE_VIEW_ITEM, UI_BTYPE_LISTROW)) {
      flag_copy |= UI_HOVER;
    }

    but->flag = (but->flag & ~flag_copy) | (oldbut->flag & flag_copy);

    /* ensures one button can get activated, and in case the buttons
     * draw are the same this gives O(1) lookup for each button */
    BLI_remlink(&oldblock->buttons, oldbut);
    ui_but_free(C, oldbut);
  }

  return found_active;
}

bool UI_but_active_only_ex(
    const bContext *C, ARegion *region, uiBlock *block, uiBut *but, const bool remove_on_failure)
{
  bool activate = false, found = false, isactive = false;

  uiBlock *oldblock = block->oldblock;
  if (!oldblock) {
    activate = true;
  }
  else {
    uiBut *oldbut = ui_but_find_old(oldblock, but);
    if (oldbut) {
      found = true;

      if (oldbut->active) {
        isactive = true;
      }
    }
  }
  if ((activate == true) || (found == false)) {
    /* There might still be another active button. */
    uiBut *old_active = ui_region_find_active_but(region);
    if (old_active) {
      ui_but_active_free(C, old_active);
    }

    ui_but_activate_event((bContext *)C, region, but);
  }
  else if ((found == true) && (isactive == false)) {
    if (remove_on_failure) {
      BLI_remlink(&block->buttons, but);
      if (but->layout) {
        ui_layout_remove_but(but->layout, but);
      }
      ui_but_free(C, but);
    }
    return false;
  }

  return true;
}

bool UI_but_active_only(const bContext *C, ARegion *region, uiBlock *block, uiBut *but)
{
  return UI_but_active_only_ex(C, region, block, but, true);
}

bool UI_block_active_only_flagged_buttons(const bContext *C, ARegion *region, uiBlock *block)
{
  /* Running this command before end-block has run, means buttons that open menus
   * won't have those menus correctly positioned, see #83539. */
  BLI_assert(block->endblock);

  bool done = false;
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    if (but->flag & UI_BUT_ACTIVATE_ON_INIT) {
      but->flag &= ~UI_BUT_ACTIVATE_ON_INIT;
      if (ui_but_is_editable(but)) {
        if (UI_but_active_only_ex(C, region, block, but, false)) {
          done = true;
          break;
        }
      }
    }
  }

  if (done) {
    /* Run this in a second pass since it's possible activating the button
     * removes the buttons being looped over. */
    LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
      but->flag &= ~UI_BUT_ACTIVATE_ON_INIT;
    }
  }

  return done;
}

void UI_but_execute(const bContext *C, ARegion *region, uiBut *but)
{
  void *active_back;
  ui_but_execute_begin((bContext *)C, region, but, &active_back);
  /* Value is applied in begin. No further action required. */
  ui_but_execute_end((bContext *)C, region, but, active_back);
}

/* use to check if we need to disable undo, but don't make any changes
 * returns false if undo needs to be disabled. */
static bool ui_but_is_rna_undo(const uiBut *but)
{
  if (but->rnapoin.owner_id) {
    /* avoid undo push for buttons who's ID are screen or wm level
     * we could disable undo for buttons with no ID too but may have
     * unforeseen consequences, so best check for ID's we _know_ are not
     * handled by undo - campbell */
    ID *id = but->rnapoin.owner_id;
    if (ID_CHECK_UNDO(id) == false) {
      return false;
    }
  }
  if (but->rnapoin.type && !RNA_struct_undo_check(but->rnapoin.type)) {
    return false;
  }

  return true;
}

/* assigns automatic keybindings to menu items for fast access
 * (underline key in menu) */
static void ui_menu_block_set_keyaccels(uiBlock *block)
{
  uint menu_key_mask = 0;
  int tot_missing = 0;

  /* only do it before bounding */
  if (block->rect.xmin != block->rect.xmax) {
    return;
  }

  for (int pass = 0; pass < 2; pass++) {
    /* 2 Passes: One for first letter only, second for any letter if the first pass fails.
     * Run first pass on all buttons so first word chars always get first priority. */

    LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
      if (!ELEM(but->type,
                UI_BTYPE_BUT,
                UI_BTYPE_BUT_MENU,
                UI_BTYPE_MENU,
                UI_BTYPE_BLOCK,
                UI_BTYPE_PULLDOWN,
                /* For PIE-menus. */
                UI_BTYPE_ROW) ||
          (but->flag & UI_HIDDEN))
      {
        continue;
      }

      if (but->menu_key != '\0') {
        continue;
      }

      if (but->str.empty()) {
        continue;
      }

      const char *str_pt = but->str.c_str();
      uchar menu_key;
      do {
        menu_key = tolower(*str_pt);
        if ((menu_key >= 'a' && menu_key <= 'z') && !(menu_key_mask & 1 << (menu_key - 'a'))) {
          menu_key_mask |= 1 << (menu_key - 'a');
          break;
        }

        if (pass == 0) {
          /* Skip to next delimiter on first pass (be picky) */
          while (isalpha(*str_pt)) {
            str_pt++;
          }

          if (*str_pt) {
            str_pt++;
          }
        }
        else {
          /* just step over every char second pass and find first usable key */
          str_pt++;
        }
      } while (*str_pt);

      if (*str_pt) {
        but->menu_key = menu_key;
      }
      else {
        /* run second pass */
        tot_missing++;
      }

      /* if all keys have been used just exit, unlikely */
      if (menu_key_mask == (1 << 26) - 1) {
        return;
      }
    }

    /* check if second pass is needed */
    if (!tot_missing) {
      break;
    }
  }
}

void ui_but_add_shortcut(uiBut *but, const char *shortcut_str, const bool do_strip)
{
  if (do_strip && (but->flag & UI_BUT_HAS_SEP_CHAR)) {
    const size_t sep_index = but->str.find_first_of(UI_SEP_CHAR);
    if (sep_index != std::string::npos) {
      but->str = but->str.substr(0, sep_index);
    }
    but->flag &= ~UI_BUT_HAS_SEP_CHAR;
  }

  /* without this, just allow stripping of the shortcut */
  if (shortcut_str == nullptr) {
    return;
  }

  but->str = fmt::format("{}" UI_SEP_CHAR_S "{}", but->str, shortcut_str);
  but->flag |= UI_BUT_HAS_SEP_CHAR;
  ui_but_update(but);
}

/* -------------------------------------------------------------------- */
/** \name Find Key Shortcut for Button
 *
 * - #ui_but_event_operator_string (and helpers)
 * - #ui_but_event_property_operator_string
 * \{ */

static bool ui_but_event_operator_string_from_operator(const bContext *C,
                                                       wmOperatorCallParams *op_call_params,
                                                       char *buf,
                                                       const size_t buf_maxncpy)
{
  BLI_assert(op_call_params->optype != nullptr);
  bool found = false;
  IDProperty *prop = reinterpret_cast<IDProperty *>(op_call_params->opptr) ?
                         static_cast<IDProperty *>(op_call_params->opptr->data) :
                         nullptr;

  if (WM_key_event_operator_string(C,
                                   op_call_params->optype->idname,
                                   op_call_params->opcontext,
                                   prop,
                                   true,
                                   buf,
                                   buf_maxncpy))
  {
    found = true;
  }
  return found;
}

static bool ui_but_event_operator_string_from_menu(const bContext *C,
                                                   uiBut *but,
                                                   char *buf,
                                                   const size_t buf_maxncpy)
{
  MenuType *mt = UI_but_menutype_get(but);
  BLI_assert(mt != nullptr);

  bool found = false;

  /* annoying, create a property */
  const IDPropertyTemplate val = {0};
  IDProperty *prop_menu = IDP_New(IDP_GROUP, &val, __func__); /* Dummy, name is unimportant. */
  IDP_AddToGroup(prop_menu, IDP_NewStringMaxSize(mt->idname, sizeof(mt->idname), "name"));

  if (WM_key_event_operator_string(
          C, "WM_OT_call_menu", WM_OP_INVOKE_REGION_WIN, prop_menu, true, buf, buf_maxncpy))
  {
    found = true;
  }

  IDP_FreeProperty(prop_menu);
  return found;
}

static bool ui_but_event_operator_string_from_panel(const bContext *C,
                                                    uiBut *but,
                                                    char *buf,
                                                    const size_t buf_maxncpy)
{
  /** Nearly exact copy of #ui_but_event_operator_string_from_menu */
  PanelType *pt = UI_but_paneltype_get(but);
  BLI_assert(pt != nullptr);

  bool found = false;

  /* annoying, create a property */
  const IDPropertyTemplate group_val = {0};
  IDProperty *prop_panel = IDP_New(
      IDP_GROUP, &group_val, __func__); /* Dummy, name is unimportant. */
  IDP_AddToGroup(prop_panel, IDP_NewStringMaxSize(pt->idname, sizeof(pt->idname), "name"));
  IDPropertyTemplate space_type_val = {0};
  space_type_val.i = pt->space_type;
  IDP_AddToGroup(prop_panel, IDP_New(IDP_INT, &space_type_val, "space_type"));
  IDPropertyTemplate region_type_val = {0};
  region_type_val.i = pt->region_type;
  IDP_AddToGroup(prop_panel, IDP_New(IDP_INT, &region_type_val, "region_type"));

  for (int i = 0; i < 2; i++) {
    /* FIXME(@ideasman42): We can't reasonably search all configurations - long term. */
    IDPropertyTemplate val = {0};
    val.i = i;

    IDP_ReplaceInGroup(prop_panel, IDP_New(IDP_INT, &val, "keep_open"));
    if (WM_key_event_operator_string(
            C, "WM_OT_call_panel", WM_OP_INVOKE_REGION_WIN, prop_panel, true, buf, buf_maxncpy))
    {
      found = true;
      break;
    }
  }

  IDP_FreeProperty(prop_panel);
  return found;
}

static bool ui_but_event_operator_string(const bContext *C,
                                         uiBut *but,
                                         char *buf,
                                         const size_t buf_maxncpy)
{
  bool found = false;

  if (but->optype != nullptr) {
    wmOperatorCallParams params = {};
    params.optype = but->optype;
    params.opptr = but->opptr;
    params.opcontext = but->opcontext;
    found = ui_but_event_operator_string_from_operator(C, &params, buf, buf_maxncpy);
  }
  else if (UI_but_menutype_get(but) != nullptr) {
    found = ui_but_event_operator_string_from_menu(C, but, buf, buf_maxncpy);
  }
  else if (UI_but_paneltype_get(but) != nullptr) {
    found = ui_but_event_operator_string_from_panel(C, but, buf, buf_maxncpy);
  }

  return found;
}

static bool ui_but_extra_icon_event_operator_string(const bContext *C,
                                                    const uiButExtraOpIcon *extra_icon,
                                                    char *buf,
                                                    const size_t buf_maxncpy)
{
  wmOperatorType *extra_icon_optype = UI_but_extra_operator_icon_optype_get(extra_icon);

  if (extra_icon_optype) {
    return ui_but_event_operator_string_from_operator(
        C, extra_icon->optype_params, buf, buf_maxncpy);
  }

  return false;
}

static bool ui_but_event_property_operator_string(const bContext *C,
                                                  uiBut *but,
                                                  char *buf,
                                                  const size_t buf_maxncpy)
{
  using namespace blender;
  /* Context toggle operator names to check. */

  /* NOTE(@ideasman42): This function could use a refactor to generalize button type to operator
   * relationship as well as which operators use properties. */
  const char *ctx_toggle_opnames[] = {
      "WM_OT_context_toggle",
      "WM_OT_context_toggle_enum",
      "WM_OT_context_cycle_int",
      "WM_OT_context_cycle_enum",
      "WM_OT_context_cycle_array",
      "WM_OT_context_menu_enum",
      nullptr,
  };

  const char *ctx_enum_opnames[] = {
      "WM_OT_context_set_enum",
      nullptr,
  };

  const char *ctx_enum_opnames_for_Area_ui_type[] = {
      "SCREEN_OT_space_type_set_or_cycle",
      nullptr,
  };

  const char **opnames = ctx_toggle_opnames;
  int opnames_len = ARRAY_SIZE(ctx_toggle_opnames);

  int prop_enum_value = -1;
  bool prop_enum_value_ok = false;
  bool prop_enum_value_is_int = false;
  const char *prop_enum_value_id = "value";
  PointerRNA *ptr = &but->rnapoin;
  PropertyRNA *prop = but->rnaprop;
  int prop_index = but->rnaindex;
  if ((but->type == UI_BTYPE_BUT_MENU) && (but->block->handle != nullptr)) {
    uiBut *but_parent = but->block->handle->popup_create_vars.but;
    if ((but->type == UI_BTYPE_BUT_MENU) && (but_parent && but_parent->rnaprop) &&
        (RNA_property_type(but_parent->rnaprop) == PROP_ENUM) &&
        ELEM(but_parent->menu_create_func,
             ui_def_but_rna__menu,
             ui_def_but_rna__panel_type,
             ui_def_but_rna__menu_type))
    {
      prop_enum_value = int(but->hardmin);
      ptr = &but_parent->rnapoin;
      prop = but_parent->rnaprop;
      prop_enum_value_ok = true;

      opnames = ctx_enum_opnames;
      opnames_len = ARRAY_SIZE(ctx_enum_opnames);
    }
  }
  /* Don't use the button again. */
  but = nullptr;

  if (prop == nullptr) {
    return false;
  }

  /* This version is only for finding hotkeys for properties.
   * These are set via a data-path which is appended to the context,
   * manipulated using operators (see #ctx_toggle_opnames). */

  if (ptr->owner_id) {
    ID *id = ptr->owner_id;

    if (GS(id->name) == ID_SCR) {
      if (RNA_struct_is_a(ptr->type, &RNA_Area)) {
        /* data should be directly on here... */
        const char *prop_id = RNA_property_identifier(prop);
        /* Hack since keys access 'type', UI shows 'ui_type'. */
        if (STREQ(prop_id, "ui_type")) {
          prop_id = "type";
          prop_enum_value >>= 16;
          prop = RNA_struct_find_property(ptr, prop_id);
          prop_index = -1;

          opnames = ctx_enum_opnames_for_Area_ui_type;
          opnames_len = ARRAY_SIZE(ctx_enum_opnames_for_Area_ui_type);
          prop_enum_value_id = "space_type";
          prop_enum_value_is_int = true;
        }
      }
    }
  }

  /* There may be multiple data-paths to the same properties,
   * support different variations so key bindings are properly detected no matter which are used.
   */
  Vector<std::string, 2> data_path_variations;

  {
    std::optional<std::string> data_path = WM_context_path_resolve_property_full(
        C, ptr, prop, prop_index);

    /* Always iterate once, even if data-path isn't set. */
    data_path_variations.append(data_path.has_value() ? data_path.value() : "");

    if (data_path.has_value()) {
      StringRef data_path_ref = StringRef(data_path.value());
      if (data_path_ref.startswith("scene.tool_settings.")) {
        data_path_variations.append(data_path_ref.drop_known_prefix("scene."));
      }
    }
  }

  /* We have a data-path! */
  bool found = false;

  for (int data_path_index = 0; data_path_index < data_path_variations.size() && (found == false);
       data_path_index++)
  {
    const StringRefNull data_path = data_path_variations[data_path_index];
    if (!data_path.is_empty() || (prop_enum_value_ok && prop_enum_value_id)) {
      /* Create a property to host the "data_path" property we're sending to the operators. */
      IDProperty *prop_path;

      const IDPropertyTemplate group_val = {0};
      prop_path = IDP_New(IDP_GROUP, &group_val, __func__);
      if (!data_path.is_empty()) {
        IDP_AddToGroup(prop_path, IDP_NewString(data_path.c_str(), "data_path"));
      }
      if (prop_enum_value_ok) {
        const EnumPropertyItem *item;
        bool free;
        RNA_property_enum_items((bContext *)C, ptr, prop, &item, nullptr, &free);
        const int index = RNA_enum_from_value(item, prop_enum_value);
        if (index != -1) {
          IDProperty *prop_value;
          if (prop_enum_value_is_int) {
            const int value = item[index].value;
            IDPropertyTemplate val = {};
            val.i = value;
            prop_value = IDP_New(IDP_INT, &val, prop_enum_value_id);
          }
          else {
            const char *id = item[index].identifier;
            prop_value = IDP_NewString(id, prop_enum_value_id);
          }
          IDP_AddToGroup(prop_path, prop_value);
        }
        else {
          opnames_len = 0; /* Do nothing. */
        }
        if (free) {
          MEM_freeN((void *)item);
        }
      }

      /* check each until one works... */

      for (int i = 0; (i < opnames_len) && (opnames[i]); i++) {
        if (WM_key_event_operator_string(
                C, opnames[i], WM_OP_INVOKE_REGION_WIN, prop_path, false, buf, buf_maxncpy))
        {
          found = true;
          break;
        }
      }
      /* cleanup */
      IDP_FreeProperty(prop_path);
    }
  }

  return found;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pie Menu Direction
 *
 * This goes in a seemingly weird pattern:
 *
 * <pre>
 *     4
 *  5     6
 * 1       2
 *  7     8
 *     3
 * </pre>
 *
 * but it's actually quite logical. It's designed to be 'upwards compatible'
 * for muscle memory so that the menu item locations are fixed and don't move
 * as new items are added to the menu later on. It also optimizes efficiency -
 * a radial menu is best kept symmetrical, with as large an angle between
 * items as possible, so that the gestural mouse movements can be fast and inexact.
 *
 * It starts off with two opposite sides for the first two items
 * then joined by the one below for the third (this way, even with three items,
 * the menu seems to still be 'in order' reading left to right). Then the fourth is
 * added to complete the compass directions. From here, it's just a matter of
 * subdividing the rest of the angles for the last 4 items.
 *
 * --Matt 07/2006
 * \{ */

const char ui_radial_dir_order[8] = {
    UI_RADIAL_W,
    UI_RADIAL_E,
    UI_RADIAL_S,
    UI_RADIAL_N,
    UI_RADIAL_NW,
    UI_RADIAL_NE,
    UI_RADIAL_SW,
    UI_RADIAL_SE,
};

const char ui_radial_dir_to_numpad[8] = {8, 9, 6, 3, 2, 1, 4, 7};
const short ui_radial_dir_to_angle[8] = {90, 45, 0, 315, 270, 225, 180, 135};

static void ui_but_pie_direction_string(uiBut *but, char *buf, int size)
{
  BLI_assert(but->pie_dir < ARRAY_SIZE(ui_radial_dir_to_numpad));
  BLI_snprintf(buf, size, "%d", ui_radial_dir_to_numpad[but->pie_dir]);
}

/** \} */

static void ui_menu_block_set_keymaps(const bContext *C, uiBlock *block)
{
  char buf[128];

  BLI_assert(block->flag & (UI_BLOCK_LOOP | UI_BLOCK_SHOW_SHORTCUT_ALWAYS));

  /* only do it before bounding */
  if (block->rect.xmin != block->rect.xmax) {
    return;
  }
  if (block->name == "splash") {
    return;
  }

  if (block->flag & UI_BLOCK_RADIAL) {
    LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
      if (but->pie_dir != UI_RADIAL_NONE) {
        ui_but_pie_direction_string(but, buf, sizeof(buf));
        ui_but_add_shortcut(but, buf, false);
      }
    }
  }
  else {
    LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
      if (block->flag & UI_BLOCK_SHOW_SHORTCUT_ALWAYS) {
        /* Skip icon-only buttons (as used in the toolbar). */
        if (but->drawstr[0] == '\0') {
          continue;
        }
        if (((block->flag & UI_BLOCK_POPOVER) == 0) && UI_but_is_tool(but)) {
          /* For non-popovers, shown in shortcut only
           * (has special shortcut handling code). */
          continue;
        }
      }
      else if (but->emboss != UI_EMBOSS_PULLDOWN) {
        continue;
      }

      if (ui_but_event_operator_string(C, but, buf, sizeof(buf))) {
        ui_but_add_shortcut(but, buf, false);
      }
      else if (ui_but_event_property_operator_string(C, but, buf, sizeof(buf))) {
        ui_but_add_shortcut(but, buf, false);
      }
    }
  }
}

void ui_but_override_flag(Main *bmain, uiBut *but)
{
  const uint override_status = RNA_property_override_library_status(
      bmain, &but->rnapoin, but->rnaprop, but->rnaindex);

  if (override_status & RNA_OVERRIDE_STATUS_OVERRIDDEN) {
    but->flag |= UI_BUT_OVERRIDDEN;
  }
  else {
    but->flag &= ~UI_BUT_OVERRIDDEN;
  }
}

/* -------------------------------------------------------------------- */
/** \name Button Extra Operator Icons
 *
 * Extra icons are shown on the right hand side of buttons. They can be clicked to invoke custom
 * operators.
 * There are some predefined here, which get added to buttons automatically based on button data
 * (type, flags, state, etc).
 * \{ */

/**
 * Predefined types for generic extra operator icons (uiButExtraOpIcon).
 */
enum PredefinedExtraOpIconType {
  PREDEFINED_EXTRA_OP_ICON_NONE = 1,
  PREDEFINED_EXTRA_OP_ICON_CLEAR,
  PREDEFINED_EXTRA_OP_ICON_EYEDROPPER,
};

static PointerRNA *ui_but_extra_operator_icon_add_ptr(uiBut *but,
                                                      wmOperatorType *optype,
                                                      wmOperatorCallContext opcontext,
                                                      int icon)
{
  uiButExtraOpIcon *extra_op_icon = MEM_new<uiButExtraOpIcon>(__func__);

  extra_op_icon->icon = icon;
  extra_op_icon->optype_params = MEM_cnew<wmOperatorCallParams>(__func__);
  extra_op_icon->optype_params->optype = optype;
  extra_op_icon->optype_params->opptr = MEM_cnew<PointerRNA>(__func__);
  WM_operator_properties_create_ptr(extra_op_icon->optype_params->opptr,
                                    extra_op_icon->optype_params->optype);
  extra_op_icon->optype_params->opcontext = opcontext;
  extra_op_icon->highlighted = false;
  extra_op_icon->disabled = false;

  BLI_addtail(&but->extra_op_icons, extra_op_icon);

  return extra_op_icon->optype_params->opptr;
}

static void ui_but_extra_operator_icon_free(uiButExtraOpIcon *extra_icon)
{
  WM_operator_properties_free(extra_icon->optype_params->opptr);
  MEM_freeN(extra_icon->optype_params->opptr);
  MEM_freeN(extra_icon->optype_params);
  MEM_freeN(extra_icon);
}

void ui_but_extra_operator_icons_free(uiBut *but)
{
  LISTBASE_FOREACH_MUTABLE (uiButExtraOpIcon *, op_icon, &but->extra_op_icons) {
    ui_but_extra_operator_icon_free(op_icon);
  }
  BLI_listbase_clear(&but->extra_op_icons);
}

PointerRNA *UI_but_extra_operator_icon_add(uiBut *but,
                                           const char *opname,
                                           wmOperatorCallContext opcontext,
                                           int icon)
{
  wmOperatorType *optype = WM_operatortype_find(opname, false);

  if (optype) {
    return ui_but_extra_operator_icon_add_ptr(but, optype, opcontext, icon);
  }

  return nullptr;
}

wmOperatorType *UI_but_extra_operator_icon_optype_get(const uiButExtraOpIcon *extra_icon)
{
  return extra_icon ? extra_icon->optype_params->optype : nullptr;
}

PointerRNA *UI_but_extra_operator_icon_opptr_get(const uiButExtraOpIcon *extra_icon)
{
  return extra_icon->optype_params->opptr;
}

static bool ui_but_icon_extra_is_visible_text_clear(const uiBut *but)
{
  BLI_assert(but->type == UI_BTYPE_TEXT);
  return ((but->flag & UI_BUT_VALUE_CLEAR) && but->drawstr[0]);
}

static bool ui_but_icon_extra_is_visible_search_unlink(const uiBut *but)
{
  BLI_assert(ELEM(but->type, UI_BTYPE_SEARCH_MENU));
  return ((but->editstr == nullptr) && (but->drawstr[0] != '\0') &&
          (but->flag & UI_BUT_VALUE_CLEAR));
}

static bool ui_but_icon_extra_is_visible_search_eyedropper(uiBut *but)
{
  BLI_assert(but->type == UI_BTYPE_SEARCH_MENU && (but->flag & UI_BUT_VALUE_CLEAR));

  if (but->rnaprop == nullptr) {
    return false;
  }

  StructRNA *type = RNA_property_pointer_type(&but->rnapoin, but->rnaprop);
  const short idcode = RNA_type_to_ID_code(type);

  return ((but->editstr == nullptr) && (idcode == ID_OB || OB_DATA_SUPPORT_ID(idcode)));
}

static PredefinedExtraOpIconType ui_but_icon_extra_get(uiBut *but)
{
  switch (but->type) {
    case UI_BTYPE_TEXT:
      if (ui_but_icon_extra_is_visible_text_clear(but)) {
        return PREDEFINED_EXTRA_OP_ICON_CLEAR;
      }
      break;
    case UI_BTYPE_SEARCH_MENU:
      if ((but->flag & UI_BUT_VALUE_CLEAR) == 0) {
        /* pass */
      }
      else if (ui_but_icon_extra_is_visible_search_unlink(but)) {
        return PREDEFINED_EXTRA_OP_ICON_CLEAR;
      }
      else if (ui_but_icon_extra_is_visible_search_eyedropper(but)) {
        return PREDEFINED_EXTRA_OP_ICON_EYEDROPPER;
      }
      break;
    default:
      break;
  }

  return PREDEFINED_EXTRA_OP_ICON_NONE;
}

/**
 * While some extra operator icons have to be set explicitly upon button creating, this code adds
 * some generic ones based on button data. Currently these are mutually exclusive, so there's only
 * ever one predefined extra icon.
 */
static void ui_but_predefined_extra_operator_icons_add(uiBut *but)
{
  const PredefinedExtraOpIconType extra_icon = ui_but_icon_extra_get(but);
  wmOperatorType *optype = nullptr;
  BIFIconID icon = ICON_NONE;

  switch (extra_icon) {
    case PREDEFINED_EXTRA_OP_ICON_EYEDROPPER: {
      static wmOperatorType *id_eyedropper_ot = nullptr;
      if (!id_eyedropper_ot) {
        id_eyedropper_ot = WM_operatortype_find("UI_OT_eyedropper_id", false);
      }
      BLI_assert(id_eyedropper_ot);

      optype = id_eyedropper_ot;
      icon = ICON_EYEDROPPER;

      break;
    }
    case PREDEFINED_EXTRA_OP_ICON_CLEAR: {
      static wmOperatorType *clear_ot = nullptr;
      if (!clear_ot) {
        clear_ot = WM_operatortype_find("UI_OT_button_string_clear", false);
      }
      BLI_assert(clear_ot);

      optype = clear_ot;
      icon = ICON_PANEL_CLOSE;

      break;
    }
    default:
      break;
  }

  if (optype) {
    LISTBASE_FOREACH (uiButExtraOpIcon *, op_icon, &but->extra_op_icons) {
      if ((op_icon->optype_params->optype == optype) && (op_icon->icon == icon)) {
        /* Don't add the same operator icon twice (happens if button is kept alive while active).
         */
        return;
      }
    }
    ui_but_extra_operator_icon_add_ptr(but, optype, WM_OP_INVOKE_DEFAULT, icon);
  }
}

/** \} */

void UI_block_update_from_old(const bContext *C, uiBlock *block)
{
  if (!block->oldblock) {
    return;
  }

  uiBut *but_old = static_cast<uiBut *>(block->oldblock->buttons.first);

  if (BLI_listbase_is_empty(&block->oldblock->butstore) == false) {
    UI_butstore_update(block);
  }

  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    if (ui_but_update_from_old_block(C, block, &but, &but_old)) {
      ui_but_update(but);

      /* redraw dynamic tooltip if we have one open */
      if (but->tip_func) {
        UI_but_tooltip_refresh((bContext *)C, but);
      }
    }
  }

  block->auto_open = block->oldblock->auto_open;
  block->auto_open_last = block->oldblock->auto_open_last;
  block->tooltipdisabled = block->oldblock->tooltipdisabled;
  BLI_movelisttolist(&block->color_pickers.list, &block->oldblock->color_pickers.list);

  block->oldblock = nullptr;
}

#ifndef NDEBUG
/**
 * Extra sanity checks for invariants (debug builds only).
 */
static void ui_but_validate(const uiBut *but)
{
  /* Number buttons must have a click-step,
   * assert instead of correcting the value to ensure the caller knows what they're doing. */
  if (but->type == UI_BTYPE_NUM) {
    uiButNumber *number_but = (uiButNumber *)but;

    if (ELEM(but->pointype, UI_BUT_POIN_CHAR, UI_BUT_POIN_SHORT, UI_BUT_POIN_INT)) {
      BLI_assert(int(number_but->step_size) > 0);
    }
  }
}
#endif

bool ui_but_context_poll_operator_ex(bContext *C,
                                     const uiBut *but,
                                     const wmOperatorCallParams *optype_params)
{
  bool result;
  int old_but_flag = 0;

  if (but) {
    old_but_flag = but->flag;

    /* Temporarily make this button override the active one, in case the poll acts on the active
     * button. */
    const_cast<uiBut *>(but)->flag |= UI_BUT_ACTIVE_OVERRIDE;

    if (but->context) {
      CTX_store_set(C, but->context);
    }
  }

  result = WM_operator_poll_context(C, optype_params->optype, optype_params->opcontext);

  if (but) {
    BLI_assert_msg((but->flag & ~UI_BUT_ACTIVE_OVERRIDE) ==
                       (old_but_flag & ~UI_BUT_ACTIVE_OVERRIDE),
                   "Operator polls shouldn't change button flags");

    const_cast<uiBut *>(but)->flag = old_but_flag;

    if (but->context) {
      CTX_store_set(C, nullptr);
    }
  }

  return result;
}

bool ui_but_context_poll_operator(bContext *C, wmOperatorType *ot, const uiBut *but)
{
  const wmOperatorCallContext opcontext = but ? but->opcontext : WM_OP_INVOKE_DEFAULT;
  wmOperatorCallParams params = {};
  params.optype = ot;
  params.opcontext = opcontext;
  return ui_but_context_poll_operator_ex(C, but, &params);
}

void UI_block_end_ex(const bContext *C, uiBlock *block, const int xy[2], int r_xy[2])
{
  wmWindow *window = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  BLI_assert(block->active);

  /* Extend button data. This needs to be done before the block updating. */
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    ui_but_predefined_extra_operator_icons_add(but);
  }

  UI_block_update_from_old(C, block);

  /* inherit flags from 'old' buttons that was drawn here previous, based
   * on matching buttons, we need this to make button event handling non
   * blocking, while still allowing buttons to be remade each redraw as it
   * is expected by blender code */
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    /* temp? Proper check for graying out */
    if (but->optype) {
      wmOperatorType *ot = but->optype;

      if (ot == nullptr || !ui_but_context_poll_operator((bContext *)C, ot, but)) {
        but->flag |= UI_BUT_DISABLED;
      }
    }

    LISTBASE_FOREACH (uiButExtraOpIcon *, op_icon, &but->extra_op_icons) {
      if (!ui_but_context_poll_operator_ex((bContext *)C, but, op_icon->optype_params)) {
        op_icon->disabled = true;
      }
    }

    const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
        depsgraph, (scene) ? scene->r.cfra : 0.0f);
    ui_but_anim_flag(but, &anim_eval_context);
    ui_but_override_flag(CTX_data_main(C), but);
    if (UI_but_is_decorator(but)) {
      ui_but_anim_decorate_update_from_flag((uiButDecorator *)but);
    }

#ifndef NDEBUG
    ui_but_validate(but);
#endif
  }

  /* handle pending stuff */
  if (block->layouts.first) {
    UI_block_layout_resolve(block, nullptr, nullptr);
  }
  ui_block_align_calc(block, CTX_wm_region(C));
  if ((block->flag & UI_BLOCK_LOOP) && (block->flag & UI_BLOCK_NUMSELECT) &&
      (block->flag & UI_BLOCK_NO_ACCELERATOR_KEYS) == 0)
  {
    ui_menu_block_set_keyaccels(block); /* could use a different flag to check */
  }

  if (block->flag & (UI_BLOCK_LOOP | UI_BLOCK_SHOW_SHORTCUT_ALWAYS)) {
    ui_menu_block_set_keymaps(C, block);
  }

  /* after keymaps! */
  switch (block->bounds_type) {
    case UI_BLOCK_BOUNDS_NONE:
      break;
    case UI_BLOCK_BOUNDS:
      ui_block_bounds_calc(block);
      break;
    case UI_BLOCK_BOUNDS_TEXT:
      ui_block_bounds_calc_text(block, 0.0f);
      break;
    case UI_BLOCK_BOUNDS_POPUP_CENTER:
      ui_block_bounds_calc_centered(window, block);
      break;
    case UI_BLOCK_BOUNDS_PIE_CENTER:
      ui_block_bounds_calc_centered_pie(block);
      break;

      /* fallback */
    case UI_BLOCK_BOUNDS_POPUP_MOUSE:
    case UI_BLOCK_BOUNDS_POPUP_MENU:
      ui_block_bounds_calc_popup(window, block, block->bounds_type, xy, r_xy);
      break;
  }

  /* Update bounds of all views in this block. If this block is a panel, this will be done later in
   * #UI_panels_end(), because buttons are offset there. */
  if (!block->panel) {
    ui_block_views_bounds_calc(block);
  }

  if (block->rect.xmin == 0.0f && block->rect.xmax == 0.0f) {
    UI_block_bounds_set_normal(block, 0);
  }
  if (block->flag & UI_BUT_ALIGN) {
    UI_block_align_end(block);
  }

  ui_update_flexible_spacing(region, block);

  block->endblock = true;
}

void UI_block_end(const bContext *C, uiBlock *block)
{
  wmWindow *window = CTX_wm_window(C);

  UI_block_end_ex(C, block, window->eventstate->xy, nullptr);
}

/* ************** BLOCK DRAWING FUNCTION ************* */

void ui_fontscale(float *points, float aspect)
{
  *points /= aspect;
}

void ui_but_to_pixelrect(rcti *rect, const ARegion *region, const uiBlock *block, const uiBut *but)
{
  *rect = ui_to_pixelrect(region, block, (but) ? &but->rect : &block->rect);
}

rcti ui_to_pixelrect(const ARegion *region, const uiBlock *block, const rctf *src_rect)
{
  rctf rectf;
  ui_block_to_window_rctf(region, block, &rectf, src_rect);
  rcti recti;
  BLI_rcti_rctf_copy_round(&recti, &rectf);
  BLI_rcti_translate(&recti, -region->winrct.xmin, -region->winrct.ymin);
  return recti;
}

static bool ui_but_pixelrect_in_view(const ARegion *region, const rcti *rect)
{
  rcti rect_winspace = *rect;
  BLI_rcti_translate(&rect_winspace, region->winrct.xmin, region->winrct.ymin);
  return BLI_rcti_isect(&region->winrct, &rect_winspace, nullptr);
}

void UI_block_draw(const bContext *C, uiBlock *block)
{
  uiStyle style = *UI_style_get_dpi(); /* XXX pass on as arg */

  /* get menu region or area region */
  ARegion *region = CTX_wm_menu(C);
  if (!region) {
    region = CTX_wm_region(C);
  }

  if (!block->endblock) {
    UI_block_end(C, block);
  }

  /* we set this only once */
  GPU_blend(GPU_BLEND_ALPHA);

  /* scale fonts */
  ui_fontscale(&style.paneltitle.points, block->aspect);
  ui_fontscale(&style.grouplabel.points, block->aspect);
  ui_fontscale(&style.widgetlabel.points, block->aspect);
  ui_fontscale(&style.widget.points, block->aspect);

  /* scale block min/max to rect */
  rcti rect;
  ui_but_to_pixelrect(&rect, region, block, nullptr);

  /* pixel space for AA widgets */
  GPU_matrix_push_projection();
  GPU_matrix_push();
  GPU_matrix_identity_set();

  wmOrtho2_region_pixelspace(region);

  /* back */
  if (block->flag & UI_BLOCK_RADIAL) {
    ui_draw_pie_center(block);
  }
  else if (block->flag & UI_BLOCK_POPOVER) {
    ui_draw_popover_back(region, &style, block, &rect);
  }
  else if (block->flag & UI_BLOCK_LOOP) {
    ui_draw_menu_back(&style, block, &rect);
  }
  else if (block->panel) {
    ui_draw_aligned_panel(region,
                          &style,
                          block,
                          &rect,
                          UI_panel_category_is_visible(region),
                          UI_panel_should_show_background(region, block->panel->type),
                          region->flag & RGN_FLAG_SEARCH_FILTER_ACTIVE);
  }

  BLF_batch_draw_begin();
  UI_icon_draw_cache_begin();
  UI_widgetbase_draw_cache_begin();

  /* widgets */
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    if (but->flag & (UI_HIDDEN | UI_SCROLLED)) {
      continue;
    }

    ui_but_to_pixelrect(&rect, region, block, but);
    /* Optimization: Don't draw buttons that are not visible (outside view bounds). */
    if (!ui_but_pixelrect_in_view(region, &rect)) {
      continue;
    }

    /* XXX: figure out why invalid coordinates happen when closing render window */
    /* and material preview is redrawn in main window (temp fix for bug #23848) */
    if (rect.xmin < rect.xmax && rect.ymin < rect.ymax) {
      ui_draw_but(C, region, &style, but, &rect);
    }
  }

  UI_widgetbase_draw_cache_end();
  UI_icon_draw_cache_end();
  BLF_batch_draw_end();

  ui_block_views_draw_overlays(region, block);

  /* restore matrix */
  GPU_matrix_pop_projection();
  GPU_matrix_pop();
}

static void ui_block_message_subscribe(ARegion *region, wmMsgBus *mbus, uiBlock *block)
{
  uiBut *but_prev = nullptr;
  /* possibly we should keep the region this block is contained in? */
  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    if (but->rnapoin.type && but->rnaprop) {
      /* quick check to avoid adding buttons representing a vector, multiple times. */
      if ((but_prev && (but_prev->rnaprop == but->rnaprop) &&
           (but_prev->rnapoin.type == but->rnapoin.type) &&
           (but_prev->rnapoin.data == but->rnapoin.data) &&
           (but_prev->rnapoin.owner_id == but->rnapoin.owner_id)) == false)
      {
        /* TODO: could make this into utility function. */
        wmMsgSubscribeValue value = {};
        value.owner = region;
        value.user_data = region;
        value.notify = ED_region_do_msg_notify_tag_redraw;
        WM_msg_subscribe_rna(mbus, &but->rnapoin, but->rnaprop, &value, __func__);
        but_prev = but;
      }
    }
  }
}

void UI_region_message_subscribe(ARegion *region, wmMsgBus *mbus)
{
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    ui_block_message_subscribe(region, mbus, block);
  }
}

/* ************* EVENTS ************* */

int ui_but_is_pushed_ex(uiBut *but, double *value)
{
  int is_push = 0;
  if (but->pushed_state_func) {
    return but->pushed_state_func(*but);
  }

  if (but->bit) {
    const bool state = !ELEM(
        but->type, UI_BTYPE_TOGGLE_N, UI_BTYPE_ICON_TOGGLE_N, UI_BTYPE_CHECKBOX_N);
    int lvalue;
    UI_GET_BUT_VALUE_INIT(but, *value);
    lvalue = int(*value);
    if (UI_BITBUT_TEST(lvalue, (but->bitnr))) {
      is_push = state;
    }
    else {
      is_push = !state;
    }
  }
  else {
    switch (but->type) {
      case UI_BTYPE_BUT:
      case UI_BTYPE_HOTKEY_EVENT:
      case UI_BTYPE_KEY_EVENT:
      case UI_BTYPE_COLOR:
      case UI_BTYPE_DECORATOR:
        is_push = -1;
        break;
      case UI_BTYPE_BUT_TOGGLE:
      case UI_BTYPE_TOGGLE:
      case UI_BTYPE_ICON_TOGGLE:
      case UI_BTYPE_CHECKBOX:
        UI_GET_BUT_VALUE_INIT(but, *value);
        if (*value != double(but->hardmin)) {
          is_push = true;
        }
        break;
      case UI_BTYPE_ICON_TOGGLE_N:
      case UI_BTYPE_TOGGLE_N:
      case UI_BTYPE_CHECKBOX_N:
        UI_GET_BUT_VALUE_INIT(but, *value);
        if (*value == 0.0) {
          is_push = true;
        }
        break;
      case UI_BTYPE_ROW:
      case UI_BTYPE_LISTROW:
      case UI_BTYPE_TAB:
        if ((but->type == UI_BTYPE_TAB) && but->rnaprop && but->custom_data) {
          /* uiBut.custom_data points to data this tab represents (e.g. workspace).
           * uiBut.rnapoin/prop store an active value (e.g. active workspace). */
          if (RNA_property_type(but->rnaprop) == PROP_POINTER) {
            const PointerRNA active_ptr = RNA_property_pointer_get(&but->rnapoin, but->rnaprop);
            if (active_ptr.data == but->custom_data) {
              is_push = true;
            }
          }
          break;
        }
        else if (but->optype) {
          break;
        }

        UI_GET_BUT_VALUE_INIT(but, *value);
        /* support for rna enum buts */
        if (but->rnaprop && (RNA_property_flag(but->rnaprop) & PROP_ENUM_FLAG)) {
          if (int(*value) & int(but->hardmax)) {
            is_push = true;
          }
        }
        else {
          if (*value == double(but->hardmax)) {
            is_push = true;
          }
        }
        break;
      case UI_BTYPE_VIEW_ITEM: {
        const uiButViewItem *view_item_but = (const uiButViewItem *)but;

        is_push = -1;
        if (view_item_but->view_item) {
          is_push = UI_view_item_is_active(view_item_but->view_item);
        }
        break;
      }
      default:
        is_push = -1;
        break;
    }
  }

  if ((but->drawflag & UI_BUT_CHECKBOX_INVERT) && (is_push != -1)) {
    is_push = !bool(is_push);
  }
  return is_push;
}
int ui_but_is_pushed(uiBut *but)
{
  double value = UI_BUT_VALUE_UNSET;
  return ui_but_is_pushed_ex(but, &value);
}

static void ui_but_update_select_flag(uiBut *but, double *value)
{
  switch (ui_but_is_pushed_ex(but, value)) {
    case true:
      but->flag |= UI_SELECT;
      break;
    case false:
      but->flag &= ~UI_SELECT;
      break;
  }
}

/* ************************************************ */

void UI_block_lock_set(uiBlock *block, bool val, const char *lockstr)
{
  if (val) {
    block->lock = val;
    block->lockstr = lockstr;
  }
}

void UI_block_lock_clear(uiBlock *block)
{
  block->lock = false;
  block->lockstr = nullptr;
}

/* *********************** data get/set ***********************
 * this either works with the pointed to data, or can work with
 * an edit override pointer while dragging for example */

void ui_but_v3_get(uiBut *but, float vec[3])
{
  if (but->editvec) {
    copy_v3_v3(vec, but->editvec);
  }

  if (but->rnaprop) {
    PropertyRNA *prop = but->rnaprop;

    zero_v3(vec);

    if (RNA_property_type(prop) == PROP_FLOAT) {
      int tot = RNA_property_array_length(&but->rnapoin, prop);
      BLI_assert(tot > 0);
      if (tot == 3) {
        RNA_property_float_get_array(&but->rnapoin, prop, vec);
      }
      else {
        tot = min_ii(tot, 3);
        for (int a = 0; a < tot; a++) {
          vec[a] = RNA_property_float_get_index(&but->rnapoin, prop, a);
        }
      }
    }
  }
  else if (but->pointype == UI_BUT_POIN_CHAR) {
    const char *cp = (char *)but->poin;
    vec[0] = float(cp[0]) / 255.0f;
    vec[1] = float(cp[1]) / 255.0f;
    vec[2] = float(cp[2]) / 255.0f;
  }
  else if (but->pointype == UI_BUT_POIN_FLOAT) {
    const float *fp = (float *)but->poin;
    copy_v3_v3(vec, fp);
  }
  else {
    if (but->editvec == nullptr) {
      fprintf(stderr, "%s: can't get color, should never happen\n", __func__);
      zero_v3(vec);
    }
  }

  if (but->type == UI_BTYPE_UNITVEC) {
    normalize_v3(vec);
  }
}

void ui_but_v3_set(uiBut *but, const float vec[3])
{
  if (but->editvec) {
    copy_v3_v3(but->editvec, vec);
  }

  if (but->rnaprop) {
    PropertyRNA *prop = but->rnaprop;

    if (RNA_property_type(prop) == PROP_FLOAT) {
      int tot;
      int a;

      tot = RNA_property_array_length(&but->rnapoin, prop);
      BLI_assert(tot > 0);
      if (tot == 3) {
        RNA_property_float_set_array(&but->rnapoin, prop, vec);
      }
      else {
        tot = min_ii(tot, 3);
        for (a = 0; a < tot; a++) {
          RNA_property_float_set_index(&but->rnapoin, prop, a, vec[a]);
        }
      }
    }
  }
  else if (but->pointype == UI_BUT_POIN_CHAR) {
    char *cp = (char *)but->poin;
    cp[0] = char(lround(vec[0] * 255.0f));
    cp[1] = char(lround(vec[1] * 255.0f));
    cp[2] = char(lround(vec[2] * 255.0f));
  }
  else if (but->pointype == UI_BUT_POIN_FLOAT) {
    float *fp = (float *)but->poin;
    copy_v3_v3(fp, vec);
  }
}

bool ui_but_is_float(const uiBut *but)
{
  if (but->pointype == UI_BUT_POIN_FLOAT && but->poin) {
    return true;
  }

  if (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_FLOAT) {
    return true;
  }

  return false;
}

PropertyScaleType ui_but_scale_type(const uiBut *but)
{
  if (but->rnaprop) {
    return RNA_property_ui_scale(but->rnaprop);
  }
  return PROP_SCALE_LINEAR;
}

bool ui_but_is_bool(const uiBut *but)
{
  if (ELEM(but->type,
           UI_BTYPE_TOGGLE,
           UI_BTYPE_TOGGLE_N,
           UI_BTYPE_ICON_TOGGLE,
           UI_BTYPE_ICON_TOGGLE_N,
           UI_BTYPE_CHECKBOX,
           UI_BTYPE_CHECKBOX_N,
           UI_BTYPE_BUT_TOGGLE,
           UI_BTYPE_TAB))
  {
    return true;
  }

  if (but->rnaprop && RNA_property_type(but->rnaprop) == PROP_BOOLEAN) {
    return true;
  }

  if ((but->rnaprop && RNA_property_type(but->rnaprop) == PROP_ENUM) &&
      (but->type == UI_BTYPE_ROW))
  {
    return true;
  }

  return false;
}

bool ui_but_is_unit(const uiBut *but)
{
  UnitSettings *unit = but->block->unit;
  const int unit_type = UI_but_unit_type_get(but);

  if (unit_type == PROP_UNIT_NONE) {
    return false;
  }

#if 1 /* removed so angle buttons get correct snapping */
  if (ui_but_is_unit_radians_ex(unit, unit_type)) {
    return false;
  }
#endif

  /* for now disable time unit conversion */
  if (unit_type == PROP_UNIT_TIME) {
    return false;
  }

  if (unit->system == USER_UNIT_NONE) {
    /* These types have units irrespective of scene units. */
    if (!ELEM(unit_type, PROP_UNIT_ROTATION, PROP_UNIT_TIME_ABSOLUTE)) {
      return false;
    }
  }

  return true;
}

bool ui_but_is_compatible(const uiBut *but_a, const uiBut *but_b)
{
  if (but_a->type != but_b->type) {
    return false;
  }
  if (but_a->pointype != but_b->pointype) {
    return false;
  }

  if (but_a->rnaprop) {
    /* skip 'rnapoin.data', 'rnapoin.owner_id'
     * allow different data to have the same props edited at once */
    if (but_a->rnapoin.type != but_b->rnapoin.type) {
      return false;
    }
    if (RNA_property_type(but_a->rnaprop) != RNA_property_type(but_b->rnaprop)) {
      return false;
    }
    if (RNA_property_subtype(but_a->rnaprop) != RNA_property_subtype(but_b->rnaprop)) {
      return false;
    }
  }

  return true;
}

bool ui_but_is_rna_valid(uiBut *but)
{
  if (but->rnaprop == nullptr || RNA_struct_contains_property(&but->rnapoin, but->rnaprop)) {
    return true;
  }
  printf("property removed %s: %p\n", but->drawstr.c_str(), but->rnaprop);
  return false;
}

bool ui_but_supports_cycling(const uiBut *but)
{
  return (ELEM(but->type, UI_BTYPE_ROW, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER, UI_BTYPE_LISTBOX) ||
          (but->type == UI_BTYPE_MENU && ui_but_menu_step_poll(but)) ||
          (but->type == UI_BTYPE_COLOR && ((uiButColor *)but)->is_pallete_color) ||
          (but->menu_step_func != nullptr));
}

double ui_but_value_get(uiBut *but)
{
  double value = 0.0;

  if (but->editval) {
    return *(but->editval);
  }
  if (but->poin == nullptr && but->rnapoin.data == nullptr) {
    return 0.0;
  }

  if (but->rnaprop) {
    PropertyRNA *prop = but->rnaprop;

    BLI_assert(but->rnaindex != -1);

    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN:
        if (RNA_property_array_check(prop)) {
          value = RNA_property_boolean_get_index(&but->rnapoin, prop, but->rnaindex);
        }
        else {
          value = RNA_property_boolean_get(&but->rnapoin, prop);
        }
        break;
      case PROP_INT:
        if (RNA_property_array_check(prop)) {
          value = RNA_property_int_get_index(&but->rnapoin, prop, but->rnaindex);
        }
        else {
          value = RNA_property_int_get(&but->rnapoin, prop);
        }
        break;
      case PROP_FLOAT:
        if (RNA_property_array_check(prop)) {
          value = RNA_property_float_get_index(&but->rnapoin, prop, but->rnaindex);
        }
        else {
          value = RNA_property_float_get(&but->rnapoin, prop);
        }
        break;
      case PROP_ENUM:
        value = RNA_property_enum_get(&but->rnapoin, prop);
        break;
      default:
        value = 0.0;
        break;
    }
  }
  else if (but->pointype == UI_BUT_POIN_CHAR) {
    value = *(char *)but->poin;
  }
  else if (but->pointype == UI_BUT_POIN_SHORT) {
    value = *(short *)but->poin;
  }
  else if (but->pointype == UI_BUT_POIN_INT) {
    value = *(int *)but->poin;
  }
  else if (but->pointype == UI_BUT_POIN_FLOAT) {
    value = *(float *)but->poin;
  }

  return value;
}

void ui_but_value_set(uiBut *but, double value)
{
  /* Value is a HSV value: convert to RGB. */
  if (but->rnaprop) {
    PropertyRNA *prop = but->rnaprop;

    if (RNA_property_editable(&but->rnapoin, prop)) {
      switch (RNA_property_type(prop)) {
        case PROP_BOOLEAN:
          if (RNA_property_array_check(prop)) {
            RNA_property_boolean_set_index(&but->rnapoin, prop, but->rnaindex, value);
          }
          else {
            RNA_property_boolean_set(&but->rnapoin, prop, value);
          }
          break;
        case PROP_INT:
          if (RNA_property_array_check(prop)) {
            RNA_property_int_set_index(&but->rnapoin, prop, but->rnaindex, int(value));
          }
          else {
            RNA_property_int_set(&but->rnapoin, prop, int(value));
          }
          break;
        case PROP_FLOAT:
          if (RNA_property_array_check(prop)) {
            RNA_property_float_set_index(&but->rnapoin, prop, but->rnaindex, value);
          }
          else {
            RNA_property_float_set(&but->rnapoin, prop, value);
          }
          break;
        case PROP_ENUM:
          if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
            int ivalue = int(value);
            /* toggle for enum/flag buttons */
            ivalue ^= RNA_property_enum_get(&but->rnapoin, prop);
            RNA_property_enum_set(&but->rnapoin, prop, ivalue);
          }
          else {
            RNA_property_enum_set(&but->rnapoin, prop, value);
          }
          break;
        default:
          break;
      }
    }

    /* we can't be sure what RNA set functions actually do,
     * so leave this unset */
    value = UI_BUT_VALUE_UNSET;
  }
  else if (but->pointype == 0) {
    /* pass */
  }
  else {
    /* first do rounding */
    if (but->pointype == UI_BUT_POIN_CHAR) {
      value = round_db_to_uchar_clamp(value);
    }
    else if (but->pointype == UI_BUT_POIN_SHORT) {
      value = round_db_to_short_clamp(value);
    }
    else if (but->pointype == UI_BUT_POIN_INT) {
      value = round_db_to_int_clamp(value);
    }
    else if (but->pointype == UI_BUT_POIN_FLOAT) {
      float fval = float(value);
      if (fval >= -0.00001f && fval <= 0.00001f) {
        /* prevent negative zero */
        fval = 0.0f;
      }
      value = fval;
    }

    /* then set value with possible edit override */
    if (but->editval) {
      value = *but->editval = value;
    }
    else if (but->pointype == UI_BUT_POIN_CHAR) {
      value = *((char *)but->poin) = char(value);
    }
    else if (but->pointype == UI_BUT_POIN_SHORT) {
      value = *((short *)but->poin) = short(value);
    }
    else if (but->pointype == UI_BUT_POIN_INT) {
      value = *((int *)but->poin) = int(value);
    }
    else if (but->pointype == UI_BUT_POIN_FLOAT) {
      value = *((float *)but->poin) = float(value);
    }
  }

  ui_but_update_select_flag(but, &value);
}

int ui_but_string_get_maxncpy(uiBut *but)
{
  if (ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU)) {
    return but->hardmax;
  }
  return UI_MAX_DRAW_STR;
}

uiBut *ui_but_drag_multi_edit_get(uiBut *but)
{
  uiBut *return_but = nullptr;

  BLI_assert(but->flag & UI_BUT_DRAG_MULTI);

  LISTBASE_FOREACH (uiBut *, but_iter, &but->block->buttons) {
    if (but_iter->editstr) {
      return_but = but_iter;
      break;
    }
  }

  return return_but;
}

static double ui_get_but_scale_unit(uiBut *but, double value)
{
  UnitSettings *unit = but->block->unit;
  const int unit_type = UI_but_unit_type_get(but);

  /* Time unit is a bit special, not handled by BKE_scene_unit_scale() for now. */
  if (unit_type == PROP_UNIT_TIME) { /* WARNING: using evil_C :| */
    Scene *scene = CTX_data_scene(static_cast<const bContext *>(but->block->evil_C));
    return FRA2TIME(value);
  }
  return BKE_scene_unit_scale(unit, RNA_SUBTYPE_UNIT_VALUE(unit_type), value);
}

void ui_but_convert_to_unit_alt_name(uiBut *but, char *str, size_t str_maxncpy)
{
  if (!ui_but_is_unit(but)) {
    return;
  }

  UnitSettings *unit = but->block->unit;
  const int unit_type = UI_but_unit_type_get(but);
  char *orig_str;

  orig_str = BLI_strdup(str);

  BKE_unit_name_to_alt(
      str, str_maxncpy, orig_str, unit->system, RNA_SUBTYPE_UNIT_VALUE(unit_type));

  MEM_freeN(orig_str);
}

/**
 * \param float_precision: Override the button precision.
 */
static void ui_get_but_string_unit(
    uiBut *but, char *str, int str_maxncpy, double value, bool pad, int float_precision)
{
  UnitSettings *unit = but->block->unit;
  const int unit_type = UI_but_unit_type_get(but);
  int precision;

  if (unit->scale_length < 0.0001f) {
    unit->scale_length = 1.0f; /* XXX do_versions */
  }

  /* Use precision override? */
  if (float_precision == -1) {
    /* Sanity checks */
    precision = int(ui_but_get_float_precision(but));
    if (precision > UI_PRECISION_FLOAT_MAX) {
      precision = UI_PRECISION_FLOAT_MAX;
    }
    else if (precision == -1) {
      precision = 2;
    }
  }
  else {
    precision = float_precision;
  }

  BKE_unit_value_as_string(str,
                           str_maxncpy,
                           ui_get_but_scale_unit(but, value),
                           precision,
                           RNA_SUBTYPE_UNIT_VALUE(unit_type),
                           unit,
                           pad);
}

static float ui_get_but_step_unit(uiBut *but, float step_default)
{
  const int unit_type = RNA_SUBTYPE_UNIT_VALUE(UI_but_unit_type_get(but));
  const double step_orig = step_default * UI_PRECISION_FLOAT_SCALE;
  /* Scaling up 'step_origg ' here is a bit arbitrary,
   * its just giving better scales from user POV */
  const double scale_step = ui_get_but_scale_unit(but, step_orig * 10);
  const double step = BKE_unit_closest_scalar(scale_step, but->block->unit->system, unit_type);

  /* -1 is an error value */
  if (step == -1.0f) {
    return step_default;
  }

  const double scale_unit = ui_get_but_scale_unit(but, 1.0);
  const double step_unit = BKE_unit_closest_scalar(
      scale_unit, but->block->unit->system, unit_type);
  double step_final;

  BLI_assert(step > 0.0);

  step_final = (step / scale_unit) / double(UI_PRECISION_FLOAT_SCALE);

  if (step == step_unit) {
    /* Logic here is to scale by the original 'step_orig'
     * only when the unit step matches the scaled step.
     *
     * This is needed for units that don't have a wide range of scales (degrees for eg.).
     * Without this we can't select between a single degree, or a 10th of a degree.
     */
    step_final *= step_orig;
  }

  return float(step_final);
}

void ui_but_string_get_ex(uiBut *but,
                          char *str,
                          const size_t str_maxncpy,
                          const int float_precision,
                          const bool use_exp_float,
                          bool *r_use_exp_float)
{
  if (r_use_exp_float) {
    *r_use_exp_float = false;
  }

  if (but->rnaprop && ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU, UI_BTYPE_TAB)) {
    const PropertyType type = RNA_property_type(but->rnaprop);

    int buf_len;
    const char *buf = nullptr;
    if ((but->type == UI_BTYPE_TAB) && (but->custom_data)) {
      StructRNA *ptr_type = RNA_property_pointer_type(&but->rnapoin, but->rnaprop);

      /* uiBut.custom_data points to data this tab represents (e.g. workspace).
       * uiBut.rnapoin/prop store an active value (e.g. active workspace). */
      PointerRNA ptr = RNA_pointer_create(but->rnapoin.owner_id, ptr_type, but->custom_data);
      buf = RNA_struct_name_get_alloc(&ptr, str, str_maxncpy, &buf_len);
    }
    else if (type == PROP_STRING) {
      /* RNA string */
      buf = RNA_property_string_get_alloc(&but->rnapoin, but->rnaprop, str, str_maxncpy, &buf_len);
    }
    else if (type == PROP_ENUM) {
      /* RNA enum */
      const int value = RNA_property_enum_get(&but->rnapoin, but->rnaprop);
      if (RNA_property_enum_name(static_cast<bContext *>(but->block->evil_C),
                                 &but->rnapoin,
                                 but->rnaprop,
                                 value,
                                 &buf))
      {
        BLI_strncpy(str, buf, str_maxncpy);
        buf = str;
      }
    }
    else if (type == PROP_POINTER) {
      /* RNA pointer */
      PointerRNA ptr = RNA_property_pointer_get(&but->rnapoin, but->rnaprop);
      buf = RNA_struct_name_get_alloc(&ptr, str, str_maxncpy, &buf_len);
    }
    else {
      BLI_assert(0);
    }

    if (buf == nullptr) {
      str[0] = '\0';
    }
    else if (buf != str) {
      BLI_assert(str_maxncpy <= buf_len + 1);
      /* string was too long, we have to truncate */
      if (UI_but_is_utf8(but)) {
        BLI_strncpy_utf8(str, buf, str_maxncpy);
      }
      else {
        BLI_strncpy(str, buf, str_maxncpy);
      }
      MEM_freeN((void *)buf);
    }
  }
  else if (ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU)) {
    /* string */
    BLI_strncpy(str, but->poin, str_maxncpy);
    return;
  }
  else if (ui_but_anim_expression_get(but, str, str_maxncpy)) {
    /* driver expression */
  }
  else {
    /* number editing */
    const double value = ui_but_value_get(but);

    PropertySubType subtype = PROP_NONE;
    if (but->rnaprop) {
      subtype = RNA_property_subtype(but->rnaprop);
    }

    if (ui_but_is_float(but)) {
      int prec = float_precision;

      if (float_precision == -1) {
        prec = ui_but_calc_float_precision(but, value);
      }
      else if (!use_exp_float && ui_but_hide_fraction(but, value)) {
        prec = 0;
      }

      if (ui_but_is_unit(but)) {
        ui_get_but_string_unit(but, str, str_maxncpy, value, false, prec);
      }
      else if (subtype == PROP_FACTOR) {
        if (U.factor_display_type == USER_FACTOR_AS_FACTOR) {
          BLI_snprintf(str, str_maxncpy, "%.*f", prec, value);
        }
        else {
          BLI_snprintf(str, str_maxncpy, "%.*f", std::max(0, prec - 2), value * 100);
        }
      }
      else {
        const int int_digits_num = integer_digits_f(value);
        if (use_exp_float) {
          if (int_digits_num < -6 || int_digits_num > 12) {
            BLI_snprintf(str, str_maxncpy, "%.*g", prec, value);
            if (r_use_exp_float) {
              *r_use_exp_float = true;
            }
          }
          else {
            prec -= int_digits_num;
            CLAMP(prec, 0, UI_PRECISION_FLOAT_MAX);
            BLI_snprintf(str, str_maxncpy, "%.*f", prec, value);
          }
        }
        else {
          prec -= int_digits_num;
          CLAMP(prec, 0, UI_PRECISION_FLOAT_MAX);
          BLI_snprintf(str, str_maxncpy, "%.*f", prec, value);
        }
      }
    }
    else {
      BLI_snprintf(str, str_maxncpy, "%d", int(value));
    }
  }
}
void ui_but_string_get(uiBut *but, char *str, const size_t str_maxncpy)
{
  ui_but_string_get_ex(but, str, str_maxncpy, -1, false, nullptr);
}

char *ui_but_string_get_dynamic(uiBut *but, int *r_str_size)
{
  char *str = nullptr;
  *r_str_size = 1;

  if (but->rnaprop && ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU)) {
    const PropertyType type = RNA_property_type(but->rnaprop);

    if (type == PROP_STRING) {
      /* RNA string */
      str = RNA_property_string_get_alloc(&but->rnapoin, but->rnaprop, nullptr, 0, r_str_size);
      (*r_str_size) += 1;
    }
    else if (type == PROP_ENUM) {
      /* RNA enum */
      const int value = RNA_property_enum_get(&but->rnapoin, but->rnaprop);
      const char *value_id;
      if (!RNA_property_enum_name(static_cast<bContext *>(but->block->evil_C),
                                  &but->rnapoin,
                                  but->rnaprop,
                                  value,
                                  &value_id))
      {
        value_id = "";
      }

      *r_str_size = strlen(value_id) + 1;
      str = BLI_strdupn(value_id, *r_str_size);
    }
    else if (type == PROP_POINTER) {
      /* RNA pointer */
      PointerRNA ptr = RNA_property_pointer_get(&but->rnapoin, but->rnaprop);
      str = RNA_struct_name_get_alloc(&ptr, nullptr, 0, r_str_size);
      (*r_str_size) += 1;
    }
    else {
      BLI_assert(0);
    }
  }
  else {
    BLI_assert(0);
  }

  if (UNLIKELY(str == nullptr)) {
    /* should never happen, paranoid check */
    *r_str_size = 1;
    str = BLI_strdup("");
    BLI_assert(0);
  }

  return str;
}

/**
 * Report a generic error prefix when evaluating a string with #BPY_run_string_as_number
 * as the Python error on its own doesn't provide enough context.
 */
#define UI_NUMBER_EVAL_ERROR_PREFIX RPT_("Error evaluating number, see Info editor for details")

static bool ui_number_from_string_units(
    bContext *C, const char *str, const int unit_type, const UnitSettings *unit, double *r_value)
{
  char *error = nullptr;
  const bool ok = user_string_to_number(C, str, unit, unit_type, r_value, true, &error);
  if (error) {
    ReportList *reports = CTX_wm_reports(C);
    BKE_reportf(reports, RPT_ERROR, "%s: %s", UI_NUMBER_EVAL_ERROR_PREFIX, error);
    MEM_freeN(error);
  }
  return ok;
}

static bool ui_number_from_string_units_with_but(bContext *C,
                                                 const char *str,
                                                 const uiBut *but,
                                                 double *r_value)
{
  const int unit_type = RNA_SUBTYPE_UNIT_VALUE(UI_but_unit_type_get(but));
  const UnitSettings *unit = but->block->unit;
  return ui_number_from_string_units(C, str, unit_type, unit, r_value);
}

static bool ui_number_from_string(bContext *C, const char *str, double *r_value)
{
  bool ok;
#ifdef WITH_PYTHON
  BPy_RunErrInfo err_info = {};
  err_info.reports = CTX_wm_reports(C);
  err_info.report_prefix = UI_NUMBER_EVAL_ERROR_PREFIX;
  ok = BPY_run_string_as_number(C, nullptr, str, &err_info, r_value);
#else
  UNUSED_VARS(C);
  *r_value = atof(str);
  ok = true;
#endif
  return ok;
}

static bool ui_number_from_string_factor(bContext *C, const char *str, double *r_value)
{
  const int len = strlen(str);
  if (BLI_strn_endswith(str, "%", len)) {
    char *str_new = BLI_strdupn(str, len - 1);
    const bool success = ui_number_from_string(C, str_new, r_value);
    MEM_freeN(str_new);
    *r_value /= 100.0;
    return success;
  }
  if (!ui_number_from_string(C, str, r_value)) {
    return false;
  }
  if (U.factor_display_type == USER_FACTOR_AS_PERCENTAGE) {
    *r_value /= 100.0;
  }
  return true;
}

static bool ui_number_from_string_percentage(bContext *C, const char *str, double *r_value)
{
  const int len = strlen(str);
  if (BLI_strn_endswith(str, "%", len)) {
    char *str_new = BLI_strdupn(str, len - 1);
    const bool success = ui_number_from_string(C, str_new, r_value);
    MEM_freeN(str_new);
    return success;
  }
  return ui_number_from_string(C, str, r_value);
}

bool ui_but_string_eval_number(bContext *C, const uiBut *but, const char *str, double *r_value)
{
  if (str[0] == '\0') {
    *r_value = 0.0;
    return true;
  }

  PropertySubType subtype = PROP_NONE;
  if (but->rnaprop) {
    subtype = RNA_property_subtype(but->rnaprop);
  }

  if (ui_but_is_float(but)) {
    if (ui_but_is_unit(but)) {
      return ui_number_from_string_units_with_but(C, str, but, r_value);
    }
    if (subtype == PROP_FACTOR) {
      return ui_number_from_string_factor(C, str, r_value);
    }
    if (subtype == PROP_PERCENTAGE) {
      return ui_number_from_string_percentage(C, str, r_value);
    }
    return ui_number_from_string(C, str, r_value);
  }
  return ui_number_from_string(C, str, r_value);
}

bool ui_but_string_set(bContext *C, uiBut *but, const char *str)
{
  if (but->rnaprop && but->rnapoin.data && ELEM(but->type, UI_BTYPE_TEXT, UI_BTYPE_SEARCH_MENU)) {
    if (RNA_property_editable(&but->rnapoin, but->rnaprop)) {
      const PropertyType type = RNA_property_type(but->rnaprop);

      if (type == PROP_STRING) {
        /* RNA string */
        RNA_property_string_set(&but->rnapoin, but->rnaprop, str);
        return true;
      }

      if (type == PROP_POINTER) {
        if (str[0] == '\0') {
          RNA_property_pointer_set(&but->rnapoin, but->rnaprop, PointerRNA_NULL, nullptr);
          return true;
        }

        uiButSearch *search_but = (but->type == UI_BTYPE_SEARCH_MENU) ? (uiButSearch *)but :
                                                                        nullptr;
        /* RNA pointer */
        PointerRNA rptr;

        /* This is kind of hackish, in theory think we could only ever use the second member of
         * this if/else, since #ui_searchbox_apply() is supposed to always set that pointer when
         * we are storing pointers... But keeping str search first for now,
         * to try to break as little as possible existing code. All this is band-aids anyway.
         * Fact remains, using `editstr` as main 'reference' over whole search button thingy
         * is utterly weak and should be redesigned IMHO, but that's not a simple task. */
        if (search_but && search_but->rnasearchprop &&
            RNA_property_collection_lookup_string(
                &search_but->rnasearchpoin, search_but->rnasearchprop, str, &rptr))
        {
          RNA_property_pointer_set(&but->rnapoin, but->rnaprop, rptr, nullptr);
        }
        else if (search_but->item_active != nullptr) {
          rptr = RNA_pointer_create(nullptr,
                                    RNA_property_pointer_type(&but->rnapoin, but->rnaprop),
                                    search_but->item_active);
          RNA_property_pointer_set(&but->rnapoin, but->rnaprop, rptr, nullptr);
        }

        return true;
      }

      if (type == PROP_ENUM) {
        int value;
        if (RNA_property_enum_value(static_cast<bContext *>(but->block->evil_C),
                                    &but->rnapoin,
                                    but->rnaprop,
                                    str,
                                    &value))
        {
          RNA_property_enum_set(&but->rnapoin, but->rnaprop, value);
          return true;
        }
        return false;
      }
      BLI_assert(0);
    }
  }
  else if (but->type == UI_BTYPE_TAB) {
    if (but->rnaprop && but->custom_data) {
      StructRNA *ptr_type = RNA_property_pointer_type(&but->rnapoin, but->rnaprop);
      PropertyRNA *prop;

      /* uiBut.custom_data points to data this tab represents (e.g. workspace).
       * uiBut.rnapoin/prop store an active value (e.g. active workspace). */
      PointerRNA ptr = RNA_pointer_create(but->rnapoin.owner_id, ptr_type, but->custom_data);
      prop = RNA_struct_name_property(ptr_type);
      if (RNA_property_editable(&ptr, prop)) {
        RNA_property_string_set(&ptr, prop, str);
      }
    }
  }
  else if (but->type == UI_BTYPE_TEXT) {
    /* string */
    if (!but->poin) {
      str = "";
    }
    else if (UI_but_is_utf8(but)) {
      BLI_strncpy_utf8(but->poin, str, but->hardmax);
    }
    else {
      BLI_strncpy(but->poin, str, but->hardmax);
    }

    return true;
  }
  else if (but->type == UI_BTYPE_SEARCH_MENU) {
    /* string */
    BLI_strncpy(but->poin, str, but->hardmax);
    return true;
  }
  else if (ui_but_anim_expression_set(but, str)) {
    /* driver expression */
    return true;
  }
  else if (str[0] == '#') {
    /* Shortcut to create new driver expression (versus immediate Python-execution). */
    return ui_but_anim_expression_create(but, str + 1);
  }
  else {
    /* number editing */
    double value;

    if (ui_but_string_eval_number(C, but, str, &value) == false) {
      WM_report_banner_show(CTX_wm_manager(C), CTX_wm_window(C));
      return false;
    }

    if (!ui_but_is_float(but)) {
      value = floor(value + 0.5);
    }

    /* not that we use hard limits here */
    if (value < double(but->hardmin)) {
      value = but->hardmin;
    }
    if (value > double(but->hardmax)) {
      value = but->hardmax;
    }

    ui_but_value_set(but, value);
    return true;
  }

  return false;
}

static double soft_range_round_up(double value, double max)
{
  /* round up to .., 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, ..
   * checking for 0.0 prevents floating point exceptions */
  const double newmax = (value != 0.0) ? pow(10.0, ceil(log(value) / M_LN10)) : 0.0;

  if (newmax * 0.2 >= max && newmax * 0.2 >= value) {
    return newmax * 0.2;
  }
  if (newmax * 0.5 >= max && newmax * 0.5 >= value) {
    return newmax * 0.5;
  }
  return newmax;
}

static double soft_range_round_down(double value, double max)
{
  /* round down to .., 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, ..
   * checking for 0.0 prevents floating point exceptions */
  const double newmax = (value != 0.0) ? pow(10.0, floor(log(value) / M_LN10)) : 0.0;

  if (newmax * 5.0 <= max && newmax * 5.0 <= value) {
    return newmax * 5.0;
  }
  if (newmax * 2.0 <= max && newmax * 2.0 <= value) {
    return newmax * 2.0;
  }
  return newmax;
}

void ui_but_range_set_hard(uiBut *but)
{
  if (but->rnaprop == nullptr) {
    return;
  }

  const PropertyType type = RNA_property_type(but->rnaprop);

  if (type == PROP_INT) {
    int imin, imax;
    RNA_property_int_range(&but->rnapoin, but->rnaprop, &imin, &imax);
    but->hardmin = imin;
    but->hardmax = imax;
  }
  else if (type == PROP_FLOAT) {
    float fmin, fmax;
    RNA_property_float_range(&but->rnapoin, but->rnaprop, &fmin, &fmax);
    but->hardmin = fmin;
    but->hardmax = fmax;
  }
}

void ui_but_range_set_soft(uiBut *but)
{
  /* This could be split up into functions which handle arrays and not. */

  /* Ideally we would not limit this, but practically it's more than
   * enough. Worst case is very long vectors won't use a smart soft-range,
   * which isn't so bad. */

  if (but->rnaprop) {
    const PropertyType type = RNA_property_type(but->rnaprop);
    const PropertySubType subtype = RNA_property_subtype(but->rnaprop);
    double softmin, softmax;
    // double step, precision; /* UNUSED. */
    double value_min;
    double value_max;

    /* clamp button range to something reasonable in case
     * we get -inf/inf from RNA properties */
    if (type == PROP_INT) {
      const bool is_array = RNA_property_array_check(but->rnaprop);
      int imin, imax, istep;

      RNA_property_int_ui_range(&but->rnapoin, but->rnaprop, &imin, &imax, &istep);
      softmin = (imin == INT_MIN) ? -1e4 : imin;
      softmax = (imin == INT_MAX) ? 1e4 : imax;
      // step = istep;  /* UNUSED */
      // precision = 1; /* UNUSED */

      if (is_array) {
        int value_range[2];
        RNA_property_int_get_array_range(&but->rnapoin, but->rnaprop, value_range);
        value_min = double(value_range[0]);
        value_max = double(value_range[1]);
      }
      else {
        value_min = value_max = ui_but_value_get(but);
      }
    }
    else if (type == PROP_FLOAT) {
      const bool is_array = RNA_property_array_check(but->rnaprop);
      float fmin, fmax, fstep, fprecision;

      RNA_property_float_ui_range(&but->rnapoin, but->rnaprop, &fmin, &fmax, &fstep, &fprecision);
      softmin = (fmin == -FLT_MAX) ? float(-1e4) : fmin;
      softmax = (fmax == FLT_MAX) ? float(1e4) : fmax;
      // step = fstep;           /* UNUSED */
      // precision = fprecision; /* UNUSED */

      /* Use shared min/max for array values, except for color alpha. */
      if (is_array && !(subtype == PROP_COLOR && but->rnaindex == 3)) {
        float value_range[2];
        RNA_property_float_get_array_range(&but->rnapoin, but->rnaprop, value_range);
        value_min = double(value_range[0]);
        value_max = double(value_range[1]);
      }
      else {
        value_min = value_max = ui_but_value_get(but);
      }
    }
    else {
      return;
    }

    /* if the value goes out of the soft/max range, adapt the range */
    if (value_min + 1e-10 < softmin) {
      if (value_min < 0.0) {
        softmin = -soft_range_round_up(-value_min, -softmin);
      }
      else {
        softmin = soft_range_round_down(value_min, softmin);
      }

      if (softmin < double(but->hardmin)) {
        softmin = double(but->hardmin);
      }
    }
    if (value_max - 1e-10 > softmax) {
      if (value_max < 0.0) {
        softmax = -soft_range_round_down(-value_max, -softmax);
      }
      else {
        softmax = soft_range_round_up(value_max, softmax);
      }

      if (softmax > double(but->hardmax)) {
        softmax = but->hardmax;
      }
    }

    but->softmin = softmin;
    but->softmax = softmax;
  }
  else if (but->poin && (but->pointype & UI_BUT_POIN_TYPES)) {
    float value = ui_but_value_get(but);
    if (isfinite(value)) {
      CLAMP(value, but->hardmin, but->hardmax);
      but->softmin = min_ff(but->softmin, value);
      but->softmax = max_ff(but->softmax, value);
    }
  }
}

/* ******************* Free ********************/

/**
 * Free data specific to a certain button type.
 * For now just do in a switch-case, we could instead have a callback stored in #uiBut and set that
 * in #ui_but_alloc_info().
 */
static void ui_but_free_type_specific(uiBut *but)
{
  switch (but->type) {
    case UI_BTYPE_SEARCH_MENU: {
      uiButSearch *search_but = (uiButSearch *)but;
      MEM_SAFE_FREE(search_but->item_active_str);

      if (search_but->arg_free_fn) {
        search_but->arg_free_fn(search_but->arg);
        search_but->arg = nullptr;
      }
      break;
    }
    default:
      break;
  }
}

/* can be called with C==nullptr */
static void ui_but_free(const bContext *C, uiBut *but)
{
  if (but->opptr) {
    WM_operator_properties_free(but->opptr);
    MEM_freeN(but->opptr);
  }

  if (but->func_argN) {
    MEM_freeN(but->func_argN);
  }

  if (but->tip_arg_free) {
    but->tip_arg_free(but->tip_arg);
  }

  if (but->hold_argN) {
    MEM_freeN(but->hold_argN);
  }

  if (but->placeholder) {
    MEM_freeN(but->placeholder);
  }

  ui_but_free_type_specific(but);

  if (but->active) {
    /* XXX solve later, buttons should be free-able without context ideally,
     * however they may have open tooltips or popup windows, which need to
     * be closed using a context pointer */
    if (C) {
      ui_but_active_free(C, but);
    }
    else {
      if (but->active) {
        MEM_freeN(but->active);
      }
    }
  }

  if ((but->type == UI_BTYPE_IMAGE) && but->poin) {
    IMB_freeImBuf((ImBuf *)but->poin);
  }

  ui_but_drag_free(but);
  ui_but_extra_operator_icons_free(but);

  BLI_assert(UI_butstore_is_registered(but->block, but) == false);

  MEM_delete(but);
}

void UI_block_free(const bContext *C, uiBlock *block)
{
  UI_butstore_clear(block);

  while (uiBut *but = static_cast<uiBut *>(BLI_pophead(&block->buttons))) {
    ui_but_free(C, but);
  }

  if (block->unit) {
    MEM_freeN(block->unit);
  }

  if (block->func_argN) {
    MEM_freeN(block->func_argN);
  }

  BLI_freelistN(&block->saferct);
  BLI_freelistN(&block->color_pickers.list);
  BLI_freelistN(&block->dynamic_listeners);

  ui_block_free_views(block);

  MEM_delete(block);
}

void UI_block_listen(const uiBlock *block, const wmRegionListenerParams *listener_params)
{
  /* Note that #uiBlock.active shouldn't be checked here, since notifier listening happens before
   * drawing, so there are no active blocks at this point. */

  LISTBASE_FOREACH (uiBlockDynamicListener *, listener, &block->dynamic_listeners) {
    listener->listener_func(listener_params);
  }

  ui_block_views_listen(block, listener_params);
}

void UI_blocklist_update_window_matrix(const bContext *C, const ListBase *lb)
{
  ARegion *region = CTX_wm_region(C);
  wmWindow *window = CTX_wm_window(C);

  LISTBASE_FOREACH (uiBlock *, block, lb) {
    if (block->active) {
      ui_update_window_matrix(window, region, block);
    }
  }
}

void UI_blocklist_update_view_for_buttons(const bContext *C, const ListBase *lb)
{
  LISTBASE_FOREACH (uiBlock *, block, lb) {
    if (block->active) {
      ui_but_update_view_for_active(C, block);
    }
  }
}

void UI_blocklist_draw(const bContext *C, const ListBase *lb)
{
  LISTBASE_FOREACH (uiBlock *, block, lb) {
    if (block->active) {
      UI_block_draw(C, block);
    }
  }
}

void UI_blocklist_free(const bContext *C, ARegion *region)
{
  ListBase *lb = &region->uiblocks;
  while (uiBlock *block = static_cast<uiBlock *>(BLI_pophead(lb))) {
    UI_block_free(C, block);
  }
  if (region->runtime.block_name_map != nullptr) {
    BLI_ghash_free(region->runtime.block_name_map, nullptr, nullptr);
    region->runtime.block_name_map = nullptr;
  }
}

void UI_blocklist_free_inactive(const bContext *C, ARegion *region)
{
  ListBase *lb = &region->uiblocks;

  LISTBASE_FOREACH_MUTABLE (uiBlock *, block, lb) {
    if (!block->handle) {
      if (block->active) {
        block->active = false;
      }
      else {
        if (region->runtime.block_name_map != nullptr) {
          uiBlock *b = static_cast<uiBlock *>(
              BLI_ghash_lookup(region->runtime.block_name_map, block->name.c_str()));
          if (b == block) {
            BLI_ghash_remove(region->runtime.block_name_map, b->name.c_str(), nullptr, nullptr);
          }
        }
        BLI_remlink(lb, block);
        UI_block_free(C, block);
      }
    }
  }
}

void UI_block_region_set(uiBlock *block, ARegion *region)
{
  ListBase *lb = &region->uiblocks;
  uiBlock *oldblock = nullptr;

  /* each listbase only has one block with this name, free block
   * if is already there so it can be rebuilt from scratch */
  if (lb) {
    if (region->runtime.block_name_map == nullptr) {
      region->runtime.block_name_map = BLI_ghash_str_new(__func__);
    }
    oldblock = (uiBlock *)BLI_ghash_lookup(region->runtime.block_name_map, block->name.c_str());

    if (oldblock) {
      oldblock->active = false;
      oldblock->panel = nullptr;
      oldblock->handle = nullptr;
    }

    /* at the beginning of the list! for dynamical menus/blocks */
    BLI_addhead(lb, block);
    BLI_ghash_reinsert(region->runtime.block_name_map,
                       const_cast<char *>(block->name.c_str()),
                       block,
                       nullptr,
                       nullptr);
  }

  block->oldblock = oldblock;
}

uiBlock *UI_block_begin(const bContext *C, ARegion *region, std::string name, eUIEmbossType emboss)
{
  wmWindow *window = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);

  uiBlock *block = MEM_new<uiBlock>(__func__);
  block->active = true;
  block->emboss = emboss;
  block->evil_C = (void *)C; /* XXX */

  if (scene) {
    /* store display device name, don't lookup for transformations yet
     * block could be used for non-color displays where looking up for transformation
     * would slow down redraw, so only lookup for actual transform when it's indeed
     * needed
     */
    STRNCPY(block->display_device, scene->display_settings.display_device);

    /* copy to avoid crash when scene gets deleted with ui still open */
    block->unit = MEM_new<UnitSettings>(__func__);
    memcpy(block->unit, &scene->unit, sizeof(scene->unit));
  }
  else {
    STRNCPY(block->display_device, IMB_colormanagement_display_get_default_name());
  }

  block->name = std::move(name);

  if (region) {
    UI_block_region_set(block, region);
  }

  /* Set window matrix and aspect for region and OpenGL state. */
  ui_update_window_matrix(window, region, block);

  /* Tag as popup menu if not created within a region. */
  if (!(region && region->visible)) {
    block->auto_open = true;
    block->flag |= UI_BLOCK_LOOP;
  }

  return block;
}

void ui_block_add_dynamic_listener(uiBlock *block,
                                   void (*listener_func)(const wmRegionListenerParams *params))
{
  uiBlockDynamicListener *listener = static_cast<uiBlockDynamicListener *>(
      MEM_mallocN(sizeof(*listener), __func__));
  listener->listener_func = listener_func;
  BLI_addtail(&block->dynamic_listeners, listener);
}

eUIEmbossType UI_block_emboss_get(uiBlock *block)
{
  return block->emboss;
}

void UI_block_emboss_set(uiBlock *block, eUIEmbossType emboss)
{
  block->emboss = emboss;
}

void UI_block_theme_style_set(uiBlock *block, char theme_style)
{
  block->theme_style = theme_style;
}

bool UI_block_is_search_only(const uiBlock *block)
{
  return block->flag & UI_BLOCK_SEARCH_ONLY;
}

void UI_block_set_search_only(uiBlock *block, bool search_only)
{
  SET_FLAG_FROM_TEST(block->flag, search_only, UI_BLOCK_SEARCH_ONLY);
}

static void ui_but_build_drawstr_float(uiBut *but, double value)
{
  PropertySubType subtype = PROP_NONE;
  if (but->rnaprop) {
    subtype = RNA_property_subtype(but->rnaprop);
  }

  /* Change negative zero to regular zero, without altering anything else. */
  value += +0.0f;

  if (value == double(FLT_MAX)) {
    but->drawstr = but->str + "inf";
  }
  else if (value == double(-FLT_MAX)) {
    but->drawstr = but->str + "-inf";
  }
  else if (subtype == PROP_PERCENTAGE) {
    const int prec = ui_but_calc_float_precision(but, value);
    but->drawstr = fmt::format("{}{:.{}f}%", but->str, value, prec);
  }
  else if (subtype == PROP_PIXEL) {
    const int prec = ui_but_calc_float_precision(but, value);
    but->drawstr = fmt::format("{}{:.{}f} px", but->str, value, prec);
  }
  else if (subtype == PROP_FACTOR) {
    const int precision = ui_but_calc_float_precision(but, value);

    if (U.factor_display_type == USER_FACTOR_AS_FACTOR) {
      but->drawstr = fmt::format("{}{:.{}f}", but->str, value, precision);
    }
    else {
      but->drawstr = fmt::format("{}{:.{}f}", but->str, value * 100, std::max(0, precision - 2));
    }
  }
  else if (ui_but_is_unit(but)) {
    char new_str[UI_MAX_DRAW_STR];
    ui_get_but_string_unit(but, new_str, sizeof(new_str), value, true, -1);
    but->drawstr = but->str + new_str;
  }
  else {
    const int prec = ui_but_calc_float_precision(but, value);
    but->drawstr = fmt::format("{}{:.{}f}", but->str, value, prec);
  }
}

static void ui_but_build_drawstr_int(uiBut *but, int value)
{
  PropertySubType subtype = PROP_NONE;
  if (but->rnaprop) {
    subtype = RNA_property_subtype(but->rnaprop);
  }

  but->drawstr = but->str + std::to_string(value);

  if (subtype == PROP_PERCENTAGE) {
    but->drawstr += "%";
  }
  else if (subtype == PROP_PIXEL) {
    but->drawstr += " px";
  }
}

/**
 * \param but: Button to update.
 * \param validate: When set, this function may change the button value.
 * Otherwise treat the button value as read-only.
 */
static void ui_but_update_ex(uiBut *but, const bool validate)
{
  /* if something changed in the button */
  double value = UI_BUT_VALUE_UNSET;

  ui_but_update_select_flag(but, &value);

  /* only update soft range while not editing */
  if (!ui_but_is_editing(but)) {
    if ((but->rnaprop != nullptr) || (but->poin && (but->pointype & UI_BUT_POIN_TYPES))) {
      ui_but_range_set_soft(but);
    }
  }

  /* test for min and max, icon sliders, etc */
  switch (but->type) {
    case UI_BTYPE_NUM:
    case UI_BTYPE_SCROLL:
    case UI_BTYPE_NUM_SLIDER:
      if (validate) {
        UI_GET_BUT_VALUE_INIT(but, value);
        if (value < double(but->hardmin)) {
          ui_but_value_set(but, but->hardmin);
        }
        else if (value > double(but->hardmax)) {
          ui_but_value_set(but, but->hardmax);
        }

        /* max must never be smaller than min! Both being equal is allowed though */
        BLI_assert(but->softmin <= but->softmax && but->hardmin <= but->hardmax);
      }
      break;

    case UI_BTYPE_ICON_TOGGLE:
    case UI_BTYPE_ICON_TOGGLE_N:
      if ((but->rnaprop == nullptr) || (RNA_property_flag(but->rnaprop) & PROP_ICONS_CONSECUTIVE))
      {
        if (but->rnaprop && RNA_property_flag(but->rnaprop) & PROP_ICONS_REVERSE) {
          but->drawflag |= UI_BUT_ICON_REVERSE;
        }

        but->iconadd = (but->flag & UI_SELECT) ? 1 : 0;
      }
      break;

      /* quiet warnings for unhandled types */
    default:
      break;
  }

  switch (but->type) {
    case UI_BTYPE_MENU:
      if (BLI_rctf_size_x(&but->rect) >= (UI_UNIT_X * 2)) {
        /* only needed for menus in popup blocks that don't recreate buttons on redraw */
        if (but->block->flag & UI_BLOCK_LOOP) {
          if (but->rnaprop && (RNA_property_type(but->rnaprop) == PROP_ENUM)) {
            const int value_enum = RNA_property_enum_get(&but->rnapoin, but->rnaprop);

            EnumPropertyItem item;
            if (RNA_property_enum_item_from_value_gettexted(
                    static_cast<bContext *>(but->block->evil_C),
                    &but->rnapoin,
                    but->rnaprop,
                    value_enum,
                    &item))
            {
              but->str = item.name;
              but->icon = item.icon;
            }
          }
        }
        but->drawstr = but->str;
      }
      break;

    case UI_BTYPE_NUM:
    case UI_BTYPE_NUM_SLIDER:
      if (but->editstr) {
        break;
      }
      UI_GET_BUT_VALUE_INIT(but, value);
      if (ui_but_is_float(but)) {
        ui_but_build_drawstr_float(but, value);
      }
      else {
        ui_but_build_drawstr_int(but, int(value));
      }
      break;

    case UI_BTYPE_LABEL:
      if (ui_but_is_float(but)) {
        UI_GET_BUT_VALUE_INIT(but, value);
        const int prec = ui_but_calc_float_precision(but, value);
        but->drawstr = fmt::format("{}{:.{}f}", but->str, value, prec);
      }
      else {
        but->drawstr = but->str;
      }

      break;

    case UI_BTYPE_TEXT:
    case UI_BTYPE_SEARCH_MENU:
      if (!but->editstr) {
        char str[UI_MAX_DRAW_STR];
        ui_but_string_get(but, str, UI_MAX_DRAW_STR);
        but->drawstr = fmt::format("{}{}", but->str, str);
      }
      break;

    case UI_BTYPE_KEY_EVENT: {
      const char *str;
      if (but->flag & UI_SELECT) {
        str = IFACE_("Press a key");
      }
      else {
        UI_GET_BUT_VALUE_INIT(but, value);
        str = WM_key_event_string(short(value), false);
      }
      but->drawstr = fmt::format("{}{}", but->str, str);
      break;
    }
    case UI_BTYPE_HOTKEY_EVENT:
      if (but->flag & UI_SELECT) {
        const uiButHotkeyEvent *hotkey_but = (uiButHotkeyEvent *)but;

        if (hotkey_but->modifier_key) {
          /* Rely on #KM_NOTHING being zero for `type`, `val` ... etc. */
          wmKeyMapItem kmi_dummy = {nullptr};
          kmi_dummy.shift = (hotkey_but->modifier_key & KM_SHIFT) ? KM_PRESS : KM_NOTHING;
          kmi_dummy.ctrl = (hotkey_but->modifier_key & KM_CTRL) ? KM_PRESS : KM_NOTHING;
          kmi_dummy.alt = (hotkey_but->modifier_key & KM_ALT) ? KM_PRESS : KM_NOTHING;
          kmi_dummy.oskey = (hotkey_but->modifier_key & KM_OSKEY) ? KM_PRESS : KM_NOTHING;

          char kmi_str[128];
          WM_keymap_item_to_string(&kmi_dummy, true, kmi_str, sizeof(kmi_str));
          but->drawstr = kmi_str;
        }
        else {
          but->drawstr = IFACE_("Press a key");
        }
      }
      else {
        but->drawstr = but->str;
      }

      break;

    case UI_BTYPE_HSVCUBE:
    case UI_BTYPE_HSVCIRCLE:
      break;
    default:
      but->drawstr = but->str;
      break;
  }

  /* if we are doing text editing, this will override the drawstr */
  if (but->editstr) {
    but->drawstr.clear();
  }

  /* text clipping moved to widget drawing code itself */
}

void ui_but_update(uiBut *but)
{
  ui_but_update_ex(but, false);
}

void ui_but_update_edited(uiBut *but)
{
  ui_but_update_ex(but, true);
}

void UI_block_align_begin(uiBlock *block)
{
  /* if other align was active, end it */
  if (block->flag & UI_BUT_ALIGN) {
    UI_block_align_end(block);
  }

  block->flag |= UI_BUT_ALIGN_DOWN;
  block->alignnr++;

  /* Buttons declared after this call will get this `alignnr`. */ /* XXX flag? */
}

void UI_block_align_end(uiBlock *block)
{
  block->flag &= ~UI_BUT_ALIGN; /* all 4 flags */
}

ColorManagedDisplay *ui_block_cm_display_get(uiBlock *block)
{
  return IMB_colormanagement_display_get_named(block->display_device);
}

void ui_block_cm_to_display_space_v3(uiBlock *block, float pixel[3])
{
  ColorManagedDisplay *display = ui_block_cm_display_get(block);

  IMB_colormanagement_scene_linear_to_display_v3(pixel, display);
}

/**
 * Factory function: Allocate button and set #uiBut.type.
 */
static uiBut *ui_but_new(const eButType type)
{
  uiBut *but = nullptr;

  switch (type) {
    case UI_BTYPE_NUM:
      but = MEM_new<uiButNumber>("uiButNumber");
      break;
    case UI_BTYPE_NUM_SLIDER:
      but = MEM_new<uiButNumberSlider>("uiButNumber");
      break;
    case UI_BTYPE_COLOR:
      but = MEM_new<uiButColor>("uiButColor");
      break;
    case UI_BTYPE_DECORATOR:
      but = MEM_new<uiButDecorator>("uiButDecorator");
      break;
    case UI_BTYPE_TAB:
      but = MEM_new<uiButTab>("uiButTab");
      break;
    case UI_BTYPE_SEARCH_MENU:
      but = MEM_new<uiButSearch>("uiButSearch");
      break;
    case UI_BTYPE_PROGRESS:
      but = MEM_new<uiButProgress>("uiButProgress");
      break;
    case UI_BTYPE_HSVCUBE:
      but = MEM_new<uiButHSVCube>("uiButHSVCube");
      break;
    case UI_BTYPE_COLORBAND:
      but = MEM_new<uiButColorBand>("uiButColorBand");
      break;
    case UI_BTYPE_CURVE:
      but = MEM_new<uiButCurveMapping>("uiButCurveMapping");
      break;
    case UI_BTYPE_CURVEPROFILE:
      but = MEM_new<uiButCurveProfile>("uiButCurveProfile");
      break;
    case UI_BTYPE_HOTKEY_EVENT:
      but = MEM_new<uiButHotkeyEvent>("uiButHotkeyEvent");
      break;
    case UI_BTYPE_VIEW_ITEM:
      but = MEM_new<uiButViewItem>("uiButViewItem");
      break;
    default:
      but = MEM_new<uiBut>("uiBut");
      break;
  }

  but->type = type;
  return but;
}

uiBut *ui_but_change_type(uiBut *but, eButType new_type)
{
  if (but->type == new_type) {
    /* Nothing to do. */
    return but;
  }

  uiBut *insert_after_but = but->prev;

  /* Remove old button address */
  BLI_remlink(&but->block->buttons, but);

  const uiBut *old_but_ptr = but;
  /* Button may have pointer to a member within itself, this will have to be updated. */
  const bool has_poin_ptr_to_self = but->poin == (char *)but;

  /* Copy construct button with the new type. */
  but = ui_but_new(new_type);
  *but = *old_but_ptr;
  /* We didn't mean to override this :) */
  but->type = new_type;
  if (has_poin_ptr_to_self) {
    but->poin = (char *)but;
  }

  BLI_insertlinkafter(&but->block->buttons, insert_after_but, but);

  if (but->layout) {
    const bool found_layout = ui_layout_replace_but_ptr(but->layout, old_but_ptr, but);
    BLI_assert(found_layout);
    UNUSED_VARS_NDEBUG(found_layout);
    ui_button_group_replace_but_ptr(uiLayoutGetBlock(but->layout), old_but_ptr, but);
  }
#ifdef WITH_PYTHON
  if (UI_editsource_enable_check()) {
    UI_editsource_but_replace(old_but_ptr, but);
  }
#endif

  MEM_delete(old_but_ptr);

  return but;
}

/**
 * \param x, y: The lower left hand corner of the button (X axis)
 * \param width, height: The size of the button.
 *
 * for float buttons:
 * \param a1: Click Step (how much to change the value each click)
 * \param a2: Number of decimal point values to display. 0 defaults to 3 (0.000)
 * 1,2,3, and a maximum of 4, all greater values will be clamped to 4.
 */
static uiBut *ui_def_but(uiBlock *block,
                         int type,
                         int retval,
                         const StringRef str,
                         int x,
                         int y,
                         short width,
                         short height,
                         void *poin,
                         float min,
                         float max,
                         float a1,
                         float a2,
                         const char *tip)
{
  BLI_assert(width >= 0 && height >= 0);

  /* we could do some more error checks here */
  if ((type & BUTTYPE) == UI_BTYPE_LABEL) {
    BLI_assert((poin != nullptr || min != 0.0f || max != 0.0f || (a1 == 0.0f && a2 != 0.0f) ||
                (a1 != 0.0f && a1 != 1.0f)) == false);
  }

  if (type & UI_BUT_POIN_TYPES) { /* a pointer is required */
    if (poin == nullptr) {
      BLI_assert(0);
      return nullptr;
    }
  }

  uiBut *but = ui_but_new((eButType)(type & BUTTYPE));

  but->pointype = (eButPointerType)(type & UI_BUT_POIN_TYPES);
  but->bit = type & UI_BUT_POIN_BIT;
  but->bitnr = type & 31;

  but->retval = retval;

  but->str = str;

  but->rect.xmin = x;
  but->rect.ymin = y;
  but->rect.xmax = but->rect.xmin + width;
  but->rect.ymax = but->rect.ymin + height;

  but->poin = (char *)poin;
  but->hardmin = but->softmin = min;
  but->hardmax = but->softmax = max;
  but->a1 = a1;
  but->a2 = a2;
  but->tip = tip;

  but->disabled_info = block->lockstr;
  but->emboss = block->emboss;

  but->block = block; /* pointer back, used for front-buffer status, and picker. */

  if ((block->flag & UI_BUT_ALIGN) && ui_but_can_align(but)) {
    but->alignnr = block->alignnr;
  }

  but->func = block->func;
  but->func_arg1 = block->func_arg1;
  but->func_arg2 = block->func_arg2;

  but->funcN = block->funcN;
  if (block->func_argN) {
    but->func_argN = MEM_dupallocN(block->func_argN);
  }

  but->pos = -1; /* cursor invisible */

  if (ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER)) { /* add a space to name */
    if (!but->str.empty() && but->str.size() < UI_MAX_NAME_STR - 2) {
      if (but->str[but->str.size() - 1] != ' ') {
        but->str += ' ';
      }
    }
  }

  if (block->flag & UI_BLOCK_RADIAL) {
    but->drawflag |= UI_BUT_TEXT_LEFT;
    if (!but->str.empty()) {
      but->drawflag |= UI_BUT_ICON_LEFT;
    }
  }
  else if (((block->flag & UI_BLOCK_LOOP) && !ui_block_is_popover(block) &&
            !(block->flag & UI_BLOCK_QUICK_SETUP)) ||
           ELEM(but->type,
                UI_BTYPE_MENU,
                UI_BTYPE_TEXT,
                UI_BTYPE_LABEL,
                UI_BTYPE_BLOCK,
                UI_BTYPE_BUT_MENU,
                UI_BTYPE_SEARCH_MENU,
                UI_BTYPE_POPOVER))
  {
    but->drawflag |= (UI_BUT_TEXT_LEFT | UI_BUT_ICON_LEFT);
  }
#ifdef USE_NUMBUTS_LR_ALIGN
  else if (ELEM(but->type, UI_BTYPE_NUM, UI_BTYPE_NUM_SLIDER)) {
    if (!but->str.empty()) {
      but->drawflag |= UI_BUT_TEXT_LEFT;
    }
  }
#endif

  but->drawflag |= (block->flag & UI_BUT_ALIGN);

  if (block->lock == true) {
    but->flag |= UI_BUT_DISABLED;
  }

  /* keep track of UI_interface.hh */
  if (ELEM(but->type,
           UI_BTYPE_BLOCK,
           UI_BTYPE_BUT,
           UI_BTYPE_DECORATOR,
           UI_BTYPE_LABEL,
           UI_BTYPE_PULLDOWN,
           UI_BTYPE_ROUNDBOX,
           UI_BTYPE_LISTBOX,
           UI_BTYPE_BUT_MENU,
           UI_BTYPE_SCROLL,
           UI_BTYPE_GRIP,
           UI_BTYPE_SEPR,
           UI_BTYPE_SEPR_LINE,
           UI_BTYPE_SEPR_SPACER) ||
      (but->type >= UI_BTYPE_SEARCH_MENU))
  {
    /* pass */
  }
  else {
    but->flag |= UI_BUT_UNDO;
  }

  if (ELEM(but->type, UI_BTYPE_COLOR)) {
    but->dragflag |= UI_BUT_DRAG_FULL_BUT;
  }

  BLI_addtail(&block->buttons, but);

  if (block->curlayout) {
    ui_layout_add_but(block->curlayout, but);
  }

#ifdef WITH_PYTHON
  /* If the 'UI_OT_editsource' is running, extract the source info from the button. */
  if (UI_editsource_enable_check()) {
    UI_editsource_active_but_test(but);
  }
#endif

  return but;
}

void ui_def_but_icon(uiBut *but, const int icon, const int flag)
{
  if (icon) {
    ui_icon_ensure_deferred(static_cast<const bContext *>(but->block->evil_C),
                            icon,
                            (flag & UI_BUT_ICON_PREVIEW) != 0);
  }
  but->icon = icon;
  but->flag |= flag;

  if (!but->str.empty()) {
    but->drawflag |= UI_BUT_ICON_LEFT;
  }
}

void ui_def_but_icon_clear(uiBut *but)
{
  but->icon = ICON_NONE;
  but->flag &= ~UI_HAS_ICON;
  but->drawflag &= ~UI_BUT_ICON_LEFT;
}

static void ui_def_but_rna__menu(bContext *C, uiLayout *layout, void *but_p)
{
  uiBlock *block = uiLayoutGetBlock(layout);
  uiPopupBlockHandle *handle = block->handle;
  uiBut *but = (uiBut *)but_p;
  const int current_value = RNA_property_enum_get(&but->rnapoin, but->rnaprop);

  /* see comment in ui_item_enum_expand, re: `uiname`. */
  const EnumPropertyItem *item_array;

  UI_block_flag_enable(block, UI_BLOCK_MOVEMOUSE_QUIT);

  bool free;
  RNA_property_enum_items_gettexted(static_cast<bContext *>(block->evil_C),
                                    &but->rnapoin,
                                    but->rnaprop,
                                    &item_array,
                                    nullptr,
                                    &free);

  /* We don't want nested rows, cols in menus. */
  UI_block_layout_set_current(block, layout);

  int totitems = 0;
  int categories = 0;
  bool has_item_with_icon = false;
  int columns = 1;
  int rows = 0;

  const wmWindow *win = CTX_wm_window(C);
  const int row_height = int(float(UI_UNIT_Y) / but->block->aspect);
  /* Calculate max_rows from how many rows can fit in this window. */
  const int max_rows = (win->sizey - (4 * row_height)) / row_height;
  float text_width = 0.0f;

  BLF_size(BLF_default(), UI_style_get()->widgetlabel.points * UI_SCALE_FAC);
  int col_rows = 0;
  float col_width = 0.0f;

  for (const EnumPropertyItem *item = item_array; item->identifier; item++, totitems++) {
    col_rows++;
    if (col_rows > 1 && (col_rows > max_rows || (!item->identifier[0] && item->name))) {
      columns++;
      text_width += col_width;
      col_width = 0;
      col_rows = 0;
    }
    if (!item->identifier[0] && item->name) {
      categories++;
      /* The category name adds to the column length. */
      col_rows++;
    }
    if (item->icon) {
      has_item_with_icon = true;
    }
    if (item->name && item->name[0]) {
      float item_width = BLF_width(BLF_default(), item->name, BLF_DRAW_STR_DUMMY_MAX);
      col_width = std::max(col_width, item_width + (100.0f * UI_SCALE_FAC));
    }
    rows = std::max(rows, col_rows);
  }
  text_width += col_width;
  text_width /= but->block->aspect;

  /* Wrap long single-column lists. */
  if (categories == 0) {
    columns = std::max((totitems + 20) / 20, 1);
    if (columns > 8) {
      columns = (totitems + 25) / 25;
    }
    rows = std::max(totitems / columns, 1);
    while (rows * columns < totitems) {
      rows++;
    }
  }

  /* If the estimated width is greater than available size, collapse to one column. */
  if (columns > 1 && text_width > WM_window_pixels_x(win)) {
    columns = 1;
    rows = totitems;
  }

  const char *title = RNA_property_ui_name(but->rnaprop);

  /* Is there a non-blank label before this button on the same row? */
  const bool prior_label = but->prev && but->prev->type == UI_BTYPE_LABEL && but->prev->str[0] &&
                           but->prev->alignnr == but->alignnr;

  if (title && title[0] && (categories == 0) && (!but->str[0] || !prior_label)) {
    /* Show title when no categories and calling button has no text or prior label. */
    uiDefBut(block,
             UI_BTYPE_LABEL,
             0,
             title,
             0,
             0,
             UI_UNIT_X * 5,
             UI_UNIT_Y,
             nullptr,
             0.0,
             0.0,
             0,
             0,
             "");
    uiItemS(layout);
  }

  /* NOTE: `item_array[...]` is reversed on access. */

  /* create items */
  uiLayout *split = uiLayoutSplit(layout, 0.0f, false);

  bool new_column;

  int column_end = 0;
  uiLayout *column = nullptr;
  for (int a = 0; a < totitems; a++) {
    new_column = (a == column_end);
    if (new_column) {
      /* start new column, and find out where it ends in advance, so we
       * can flip the order of items properly per column */
      column_end = totitems;

      for (int b = a + 1; b < totitems; b++) {
        const EnumPropertyItem *item = &item_array[b];

        /* new column on N rows or on separation label */
        if (((b - a) % rows == 0) || (columns > 1 && !item->identifier[0] && item->name)) {
          column_end = b;
          break;
        }
      }

      column = uiLayoutColumn(split, false);
    }

    const EnumPropertyItem *item = &item_array[a];

    if (new_column && (categories > 0) && (columns > 1) && item->identifier[0]) {
      uiItemL(column, "", ICON_NONE);
      uiItemS(column);
    }

    if (!item->identifier[0]) {
      if (item->name || columns > 1) {
        if (item->icon) {
          uiItemL(column, item->name, item->icon);
        }
        else if (item->name) {
          /* Do not use uiItemL here, as our root layout is a menu one,
           * it will add a fake blank icon! */
          uiDefBut(block,
                   UI_BTYPE_LABEL,
                   0,
                   item->name,
                   0,
                   0,
                   UI_UNIT_X * 5,
                   UI_UNIT_Y,
                   nullptr,
                   0.0,
                   0.0,
                   0,
                   0,
                   "");
        }
      }
      uiItemS(column);
    }
    else {
      int icon = item->icon;
      /* Use blank icon if there is none for this item (but for some other one) to make sure labels
       * align. */
      if (icon == ICON_NONE && has_item_with_icon) {
        icon = ICON_BLANK1;
      }

      uiBut *item_but;
      if (icon) {
        item_but = uiDefIconTextButI(block,
                                     UI_BTYPE_BUT_MENU,
                                     B_NOP,
                                     icon,
                                     item->name,
                                     0,
                                     0,
                                     UI_UNIT_X * 5,
                                     UI_UNIT_Y,
                                     &handle->retvalue,
                                     item->value,
                                     0.0,
                                     0,
                                     -1,
                                     item->description);
      }
      else {
        item_but = uiDefButI(block,
                             UI_BTYPE_BUT_MENU,
                             B_NOP,
                             item->name,
                             0,
                             0,
                             UI_UNIT_X * 5,
                             UI_UNIT_X,
                             &handle->retvalue,
                             item->value,
                             0.0,
                             0,
                             -1,
                             item->description);
      }
      if (item->value == current_value) {
        item_but->flag |= UI_SELECT_DRAW;
      }
    }
  }

  UI_block_layout_set_current(block, layout);

  if (free) {
    MEM_freeN((void *)item_array);
  }
}

static void ui_def_but_rna__panel_type(bContext *C, uiLayout *layout, void *arg)
{
  PanelType *panel_type = static_cast<PanelType *>(arg);
  if (panel_type) {
    ui_item_paneltype_func(C, layout, panel_type);
  }
  else {
    uiItemL(layout, RPT_("Missing Panel"), ICON_NONE);
  }
}

void ui_but_rna_menu_convert_to_panel_type(uiBut *but, const char *panel_type)
{
  BLI_assert(ELEM(but->type, UI_BTYPE_MENU, UI_BTYPE_COLOR));
  //  BLI_assert(but->menu_create_func == ui_def_but_rna__menu);
  //  BLI_assert((void *)but->poin == but);
  but->menu_create_func = ui_def_but_rna__panel_type;
  but->func_argN = BLI_strdup(panel_type);
}

bool ui_but_menu_draw_as_popover(const uiBut *but)
{
  return (but->menu_create_func == ui_def_but_rna__panel_type);
}

static void ui_def_but_rna__menu_type(bContext *C, uiLayout *layout, void *but_p)
{
  uiBut *but = static_cast<uiBut *>(but_p);
  const char *menu_type = static_cast<const char *>(but->func_argN);
  MenuType *mt = WM_menutype_find(menu_type, true);
  if (mt) {
    ui_item_menutype_func(C, layout, mt);
  }
  else {
    char msg[256];
    SNPRINTF(msg, RPT_("Missing Menu: %s"), menu_type);
    uiItemL(layout, msg, ICON_NONE);
  }
}

void ui_but_rna_menu_convert_to_menu_type(uiBut *but, const char *menu_type)
{
  BLI_assert(but->type == UI_BTYPE_MENU);
  BLI_assert(but->menu_create_func == ui_def_but_rna__menu);
  BLI_assert((void *)but->poin == but);
  but->menu_create_func = ui_def_but_rna__menu_type;
  but->func_argN = BLI_strdup(menu_type);
}

static void ui_but_submenu_enable(uiBlock *block, uiBut *but)
{
  but->flag |= UI_BUT_ICON_SUBMENU;
  block->content_hints |= UI_BLOCK_CONTAINS_SUBMENU_BUT;
}

/**
 * ui_def_but_rna_propname and ui_def_but_rna
 * both take the same args except for propname vs prop, this is done so we can
 * avoid an extra lookup on 'prop' when its already available.
 *
 * When this kind of change won't disrupt branches, best look into making more
 * of our UI functions take prop rather than propname.
 */
static uiBut *ui_def_but_rna(uiBlock *block,
                             int type,
                             int retval,
                             const char *str,
                             int x,
                             int y,
                             short width,
                             short height,
                             PointerRNA *ptr,
                             PropertyRNA *prop,
                             int index,
                             float min,
                             float max,
                             const char *tip)
{
  const PropertyType proptype = RNA_property_type(prop);
  int icon = 0;
  uiMenuCreateFunc func = nullptr;

  if (ELEM(type, UI_BTYPE_COLOR, UI_BTYPE_HSVCIRCLE, UI_BTYPE_HSVCUBE)) {
    BLI_assert(index == -1);
  }

  /* use rna values if parameters are not specified */
  if ((proptype == PROP_ENUM) && ELEM(type, UI_BTYPE_MENU, UI_BTYPE_ROW, UI_BTYPE_LISTROW)) {
    bool free;
    const EnumPropertyItem *item;
    RNA_property_enum_items(
        static_cast<bContext *>(block->evil_C), ptr, prop, &item, nullptr, &free);

    int value;
    /* UI_BTYPE_MENU is handled a little differently here */
    if (type == UI_BTYPE_MENU) {
      value = RNA_property_enum_get(ptr, prop);
    }
    else {
      value = int(max);
    }

    const int i = RNA_enum_from_value(item, value);
    if (i != -1) {

      if (!str) {
        str = item[i].name;
#ifdef WITH_INTERNATIONAL
        str = CTX_IFACE_(RNA_property_translation_context(prop), str);
#endif
      }

      icon = item[i].icon;
    }
    else {
      if (!str) {
        if (type == UI_BTYPE_MENU) {
          str = "";
        }
        else {
          str = RNA_property_ui_name(prop);
        }
      }
    }

    if (type == UI_BTYPE_MENU) {
      func = ui_def_but_rna__menu;
    }

    if (free) {
      MEM_freeN((void *)item);
    }
  }
  else {
    if (!str) {
      str = RNA_property_ui_name(prop);
    }
    icon = RNA_property_ui_icon(prop);
  }

  if (!tip && proptype != PROP_ENUM) {
    tip = RNA_property_ui_description(prop);
  }

  float step = -1.0f;
  float precision = -1.0f;
  if (proptype == PROP_INT) {
    int hardmin, hardmax, softmin, softmax, int_step;

    RNA_property_int_range(ptr, prop, &hardmin, &hardmax);
    RNA_property_int_ui_range(ptr, prop, &softmin, &softmax, &int_step);

    if (!ELEM(type, UI_BTYPE_ROW, UI_BTYPE_LISTROW) && min == max) {
      min = hardmin;
      max = hardmax;
    }
    step = int_step;
    precision = 0;
  }
  else if (proptype == PROP_FLOAT) {
    float hardmin, hardmax, softmin, softmax;

    RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
    RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);

    if (!ELEM(type, UI_BTYPE_ROW, UI_BTYPE_LISTROW) && min == max) {
      min = hardmin;
      max = hardmax;
    }
  }
  else if (proptype == PROP_STRING) {
    min = 0;
    max = RNA_property_string_maxlength(prop);
    /* NOTE: 'max' may be zero (code for dynamically resized array). */
  }

  /* now create button */
  uiBut *but = ui_def_but(
      block, type, retval, str, x, y, width, height, nullptr, min, max, step, precision, tip);

  if (but->type == UI_BTYPE_NUM) {
    /* Set default values, can be overridden later. */
    UI_but_number_step_size_set(but, step);
    UI_but_number_precision_set(but, precision);
  }
  else if (but->type == UI_BTYPE_NUM_SLIDER) {
    /* Set default values, can be overridden later. */
    UI_but_number_slider_step_size_set(but, step);
    UI_but_number_slider_precision_set(but, precision);
  }

  but->rnapoin = *ptr;
  but->rnaprop = prop;

  if (RNA_property_array_check(but->rnaprop)) {
    but->rnaindex = index;
  }
  else {
    but->rnaindex = 0;
  }

  if (icon) {
    ui_def_but_icon(but, icon, UI_HAS_ICON);
  }

  if (type == UI_BTYPE_MENU) {
    if (but->emboss == UI_EMBOSS_PULLDOWN) {
      ui_but_submenu_enable(block, but);
    }
  }
  else if (type == UI_BTYPE_SEARCH_MENU) {
    if (proptype == PROP_POINTER) {
      /* Search buttons normally don't get undo, see: #54580. */
      but->flag |= UI_BUT_UNDO;
    }
  }

  const char *info;
  if (but->rnapoin.data && !RNA_property_editable_info(&but->rnapoin, prop, &info)) {
    UI_but_disable(but, info);
  }

  if (proptype == PROP_POINTER) {
    /* If the button shows an ID, automatically set it as focused in context so operators can
     * access it. */
    const PointerRNA pptr = RNA_property_pointer_get(ptr, prop);
    if (pptr.data && RNA_struct_is_ID(pptr.type)) {
      but->context = CTX_store_add(block->contexts, "id", &pptr);
    }
  }

  if (but->flag & UI_BUT_UNDO && (ui_but_is_rna_undo(but) == false)) {
    but->flag &= ~UI_BUT_UNDO;
  }

  /* If this button uses units, calculate the step from this */
  if ((proptype == PROP_FLOAT) && ui_but_is_unit(but)) {
    if (type == UI_BTYPE_NUM) {
      uiButNumber *number_but = (uiButNumber *)but;
      number_but->step_size = ui_get_but_step_unit(but, number_but->step_size);
    }
    if (type == UI_BTYPE_NUM_SLIDER) {
      uiButNumberSlider *number_but = (uiButNumberSlider *)but;
      number_but->step_size = ui_get_but_step_unit(but, number_but->step_size);
    }
    else {
      but->a1 = ui_get_but_step_unit(but, but->a1);
    }
  }

  if (func) {
    but->menu_create_func = func;
    but->poin = (char *)but;
  }

  return but;
}

static uiBut *ui_def_but_rna_propname(uiBlock *block,
                                      int type,
                                      int retval,
                                      const char *str,
                                      int x,
                                      int y,
                                      short width,
                                      short height,
                                      PointerRNA *ptr,
                                      const char *propname,
                                      int index,
                                      float min,
                                      float max,
                                      const char *tip)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);

  uiBut *but;
  if (prop) {
    but = ui_def_but_rna(
        block, type, retval, str, x, y, width, height, ptr, prop, index, min, max, tip);
  }
  else {
    but = ui_def_but(
        block, type, retval, propname, x, y, width, height, nullptr, min, max, -1.0f, -1.0f, tip);

    UI_but_disable(but, N_("Unknown Property"));
  }

  return but;
}

static uiBut *ui_def_but_operator_ptr(uiBlock *block,
                                      int type,
                                      wmOperatorType *ot,
                                      wmOperatorCallContext opcontext,
                                      const StringRef str,
                                      int x,
                                      int y,
                                      short width,
                                      short height,
                                      const char *tip)
{
  if ((!tip || tip[0] == '\0') && ot && ot->srna && !ot->get_description) {
    tip = RNA_struct_ui_description(ot->srna);
  }

  uiBut *but = ui_def_but(block, type, -1, str, x, y, width, height, nullptr, 0, 0, 0, 0, tip);
  but->optype = ot;
  but->opcontext = opcontext;
  but->flag &= ~UI_BUT_UNDO; /* no need for ui_but_is_rna_undo(), we never need undo here */

  /* Enable quick tooltip label if this is a tool button without a label. */
  if (str.is_empty() && !ui_block_is_popover(block) && UI_but_is_tool(but)) {
    UI_but_drawflag_enable(but, UI_BUT_HAS_TOOLTIP_LABEL);
  }

  if (!ot) {
    UI_but_disable(but, "");
  }

  return but;
}

uiBut *uiDefBut(uiBlock *block,
                int type,
                int retval,
                const StringRef str,
                int x,
                int y,
                short width,
                short height,
                void *poin,
                float min,
                float max,
                float a1,
                float a2,
                const char *tip)
{
  uiBut *but = ui_def_but(
      block, type, retval, str, x, y, width, height, poin, min, max, a1, a2, tip);

  ui_but_update(but);

  return but;
}

uiBut *uiDefButImage(
    uiBlock *block, void *imbuf, int x, int y, short width, short height, const uchar color[4])
{
  uiBut *but = ui_def_but(
      block, UI_BTYPE_IMAGE, 0, "", x, y, width, height, imbuf, 0, 0, 0, 0, "");
  if (color) {
    copy_v4_v4_uchar(but->col, color);
  }
  else {
    but->col[0] = 255;
    but->col[1] = 255;
    but->col[2] = 255;
    but->col[3] = 255;
  }
  ui_but_update(but);
  return but;
}

uiBut *uiDefButAlert(uiBlock *block, int icon, int x, int y, short width, short height)
{
  ImBuf *ibuf = UI_icon_alert_imbuf_get((eAlertIcon)icon);
  if (ibuf) {
    bTheme *btheme = UI_GetTheme();
    return uiDefButImage(block, ibuf, x, y, width, height, btheme->tui.wcol_menu_back.text);
  }
  return nullptr;
}

/**
 * if \a _x_ is a power of two (only one bit) return the power,
 * otherwise return -1.
 *
 * for powers of two:
 * \code{.c}
 *     ((1 << findBitIndex(x)) == x);
 * \endcode
 */
static int findBitIndex(uint x)
{
  if (!x || !is_power_of_2_i(x)) { /* is_power_of_2_i(x) strips lowest bit */
    return -1;
  }
  int idx = 0;

  if (x & 0xFFFF0000) {
    idx += 16;
    x >>= 16;
  }
  if (x & 0xFF00) {
    idx += 8;
    x >>= 8;
  }
  if (x & 0xF0) {
    idx += 4;
    x >>= 4;
  }
  if (x & 0xC) {
    idx += 2;
    x >>= 2;
  }
  if (x & 0x2) {
    idx += 1;
  }

  return idx;
}

/* Auto-complete helper functions. */
struct AutoComplete {
  size_t maxncpy;
  int matches;
  char *truncate;
  const char *startname;
};

AutoComplete *UI_autocomplete_begin(const char *startname, size_t maxncpy)
{
  AutoComplete *autocpl;

  autocpl = MEM_cnew<AutoComplete>(__func__);
  autocpl->maxncpy = maxncpy;
  autocpl->matches = 0;
  autocpl->truncate = static_cast<char *>(MEM_callocN(sizeof(char) * maxncpy, __func__));
  autocpl->startname = startname;

  return autocpl;
}

void UI_autocomplete_update_name(AutoComplete *autocpl, const char *name)
{
  char *truncate = autocpl->truncate;
  const char *startname = autocpl->startname;
  int match_index = 0;
  for (int a = 0; a < autocpl->maxncpy - 1; a++) {
    if (startname[a] == 0 || startname[a] != name[a]) {
      match_index = a;
      break;
    }
  }

  /* found a match */
  if (startname[match_index] == 0) {
    autocpl->matches++;
    /* first match */
    if (truncate[0] == 0) {
      BLI_strncpy(truncate, name, autocpl->maxncpy);
    }
    else {
      /* remove from truncate what is not in bone->name */
      for (int a = 0; a < autocpl->maxncpy - 1; a++) {
        if (name[a] == 0) {
          truncate[a] = 0;
          break;
        }
        if (truncate[a] != name[a]) {
          truncate[a] = 0;
        }
      }
    }
  }
}

int UI_autocomplete_end(AutoComplete *autocpl, char *autoname)
{
  int match = AUTOCOMPLETE_NO_MATCH;
  if (autocpl->truncate[0]) {
    if (autocpl->matches == 1) {
      match = AUTOCOMPLETE_FULL_MATCH;
    }
    else {
      match = AUTOCOMPLETE_PARTIAL_MATCH;
    }
    BLI_strncpy(autoname, autocpl->truncate, autocpl->maxncpy);
  }
  else {
    if (autoname != autocpl->startname) { /* don't copy a string over itself */
      BLI_strncpy(autoname, autocpl->startname, autocpl->maxncpy);
    }
  }

  MEM_freeN(autocpl->truncate);
  MEM_freeN(autocpl);
  return match;
}

#define PREVIEW_TILE_PAD (0.15f * UI_UNIT_X)

int UI_preview_tile_size_x(const int size_px)
{
  const float pad = PREVIEW_TILE_PAD;
  return round_fl_to_int((size_px / 20.0f) * UI_UNIT_X + 2.0f * pad);
}

int UI_preview_tile_size_y(const int size_px)
{
  const float font_height = UI_UNIT_Y;
  /* Add some extra padding to make things less tight vertically. */
  const float pad = PREVIEW_TILE_PAD;

  return round_fl_to_int(UI_preview_tile_size_y_no_label(size_px) + font_height + pad);
}

int UI_preview_tile_size_y_no_label(const int size_px)
{
  const float pad = PREVIEW_TILE_PAD;
  return round_fl_to_int((size_px / 20.0f) * UI_UNIT_Y + 2.0f * pad);
}

#undef PREVIEW_TILE_PAD

static void ui_but_update_and_icon_set(uiBut *but, int icon)
{
  if (icon) {
    ui_def_but_icon(but, icon, UI_HAS_ICON);
  }

  ui_but_update(but);
}

static uiBut *uiDefButBit(uiBlock *block,
                          int type,
                          int bit,
                          int retval,
                          const StringRef str,
                          int x,
                          int y,
                          short width,
                          short height,
                          void *poin,
                          float min,
                          float max,
                          float a1,
                          float a2,
                          const char *tip)
{
  const int bitIdx = findBitIndex(bit);
  if (bitIdx == -1) {
    return nullptr;
  }
  return uiDefBut(block,
                  type | UI_BUT_POIN_BIT | bitIdx,
                  retval,
                  str,
                  x,
                  y,
                  width,
                  height,
                  poin,
                  min,
                  max,
                  a1,
                  a2,
                  tip);
}
uiBut *uiDefButF(uiBlock *block,
                 int type,
                 int retval,
                 const StringRef str,
                 int x,
                 int y,
                 short width,
                 short height,
                 float *poin,
                 float min,
                 float max,
                 const char *tip)
{
  return uiDefBut(block,
                  type | UI_BUT_POIN_FLOAT,
                  retval,
                  str,
                  x,
                  y,
                  width,
                  height,
                  (void *)poin,
                  min,
                  max,
                  0.0f,
                  0.0f,
                  tip);
}
uiBut *uiDefButI(uiBlock *block,
                 int type,
                 int retval,
                 const StringRef str,
                 int x,
                 int y,
                 short width,
                 short height,
                 int *poin,
                 float min,
                 float max,
                 float a1,
                 float a2,
                 const char *tip)
{
  return uiDefBut(block,
                  type | UI_BUT_POIN_INT,
                  retval,
                  str,
                  x,
                  y,
                  width,
                  height,
                  (void *)poin,
                  min,
                  max,
                  a1,
                  a2,
                  tip);
}
uiBut *uiDefButBitI(uiBlock *block,
                    int type,
                    int bit,
                    int retval,
                    const StringRef str,
                    int x,
                    int y,
                    short width,
                    short height,
                    int *poin,
                    float min,
                    float max,
                    const char *tip)
{
  return uiDefButBit(block,
                     type | UI_BUT_POIN_INT,
                     bit,
                     retval,
                     str,
                     x,
                     y,
                     width,
                     height,
                     (void *)poin,
                     min,
                     max,
                     0.0f,
                     0.0f,
                     tip);
}
uiBut *uiDefButS(uiBlock *block,
                 int type,
                 int retval,
                 const StringRef str,
                 int x,
                 int y,
                 short width,
                 short height,
                 short *poin,
                 float min,
                 float max,
                 float a1,
                 float a2,
                 const char *tip)
{
  return uiDefBut(block,
                  type | UI_BUT_POIN_SHORT,
                  retval,
                  str,
                  x,
                  y,
                  width,
                  height,
                  (void *)poin,
                  min,
                  max,
                  a1,
                  a2,
                  tip);
}
uiBut *uiDefButBitS(uiBlock *block,
                    int type,
                    int bit,
                    int retval,
                    const StringRef str,
                    int x,
                    int y,
                    short width,
                    short height,
                    short *poin,
                    float min,
                    float max,
                    const char *tip)
{
  return uiDefButBit(block,
                     type | UI_BUT_POIN_SHORT,
                     bit,
                     retval,
                     str,
                     x,
                     y,
                     width,
                     height,
                     (void *)poin,
                     min,
                     max,
                     0.0f,
                     0.0f,
                     tip);
}
uiBut *uiDefButC(uiBlock *block,
                 int type,
                 int retval,
                 const StringRef str,
                 int x,
                 int y,
                 short width,
                 short height,
                 char *poin,
                 float min,
                 float max,
                 const char *tip)
{
  return uiDefBut(block,
                  type | UI_BUT_POIN_CHAR,
                  retval,
                  str,
                  x,
                  y,
                  width,
                  height,
                  (void *)poin,
                  min,
                  max,
                  0.0f,
                  0.0f,
                  tip);
}
uiBut *uiDefButBitC(uiBlock *block,
                    int type,
                    int bit,
                    int retval,
                    const StringRef str,
                    int x,
                    int y,
                    short width,
                    short height,
                    char *poin,
                    float min,
                    float max,
                    const char *tip)
{
  return uiDefButBit(block,
                     type | UI_BUT_POIN_CHAR,
                     bit,
                     retval,
                     str,
                     x,
                     y,
                     width,
                     height,
                     (void *)poin,
                     min,
                     max,
                     0,
                     0,
                     tip);
}
uiBut *uiDefButR(uiBlock *block,
                 int type,
                 int retval,
                 const char *str,
                 int x,
                 int y,
                 short width,
                 short height,
                 PointerRNA *ptr,
                 const char *propname,
                 int index,
                 float min,
                 float max,
                 const char *tip)
{
  uiBut *but = ui_def_but_rna_propname(
      block, type, retval, str, x, y, width, height, ptr, propname, index, min, max, tip);
  ui_but_update(but);
  return but;
}
uiBut *uiDefButR_prop(uiBlock *block,
                      int type,
                      int retval,
                      const char *str,
                      int x,
                      int y,
                      short width,
                      short height,
                      PointerRNA *ptr,
                      PropertyRNA *prop,
                      int index,
                      float min,
                      float max,
                      const char *tip)
{
  uiBut *but = ui_def_but_rna(
      block, type, retval, str, x, y, width, height, ptr, prop, index, min, max, tip);
  ui_but_update(but);
  return but;
}

uiBut *uiDefButO_ptr(uiBlock *block,
                     int type,
                     wmOperatorType *ot,
                     wmOperatorCallContext opcontext,
                     const StringRef str,
                     int x,
                     int y,
                     short width,
                     short height,
                     const char *tip)
{
  uiBut *but = ui_def_but_operator_ptr(block, type, ot, opcontext, str, x, y, width, height, tip);
  ui_but_update(but);
  return but;
}
uiBut *uiDefButO(uiBlock *block,
                 int type,
                 const char *opname,
                 wmOperatorCallContext opcontext,
                 const char *str,
                 int x,
                 int y,
                 short width,
                 short height,
                 const char *tip)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false);
  if (str == nullptr && ot == nullptr) {
    str = opname;
  }
  return uiDefButO_ptr(block, type, ot, opcontext, str, x, y, width, height, tip);
}

uiBut *uiDefIconBut(uiBlock *block,
                    int type,
                    int retval,
                    int icon,
                    int x,
                    int y,
                    short width,
                    short height,
                    void *poin,
                    float min,
                    float max,
                    float a1,
                    float a2,
                    const char *tip)
{
  uiBut *but = ui_def_but(
      block, type, retval, "", x, y, width, height, poin, min, max, a1, a2, tip);
  ui_but_update_and_icon_set(but, icon);
  return but;
}
static uiBut *uiDefIconButBit(uiBlock *block,
                              int type,
                              int bit,
                              int retval,
                              int icon,
                              int x,
                              int y,
                              short width,
                              short height,
                              void *poin,
                              float min,
                              float max,
                              float a1,
                              float a2,
                              const char *tip)
{
  const int bitIdx = findBitIndex(bit);
  if (bitIdx == -1) {
    return nullptr;
  }
  return uiDefIconBut(block,
                      type | UI_BUT_POIN_BIT | bitIdx,
                      retval,
                      icon,
                      x,
                      y,
                      width,
                      height,
                      poin,
                      min,
                      max,
                      a1,
                      a2,
                      tip);
}

uiBut *uiDefIconButI(uiBlock *block,
                     int type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     int *poin,
                     float min,
                     float max,
                     float a1,
                     float a2,
                     const char *tip)
{
  return uiDefIconBut(block,
                      type | UI_BUT_POIN_INT,
                      retval,
                      icon,
                      x,
                      y,
                      width,
                      height,
                      (void *)poin,
                      min,
                      max,
                      a1,
                      a2,
                      tip);
}
uiBut *uiDefIconButBitI(uiBlock *block,
                        int type,
                        int bit,
                        int retval,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        int *poin,
                        float min,
                        float max,
                        float a1,
                        float a2,
                        const char *tip)
{
  return uiDefIconButBit(block,
                         type | UI_BUT_POIN_INT,
                         bit,
                         retval,
                         icon,
                         x,
                         y,
                         width,
                         height,
                         (void *)poin,
                         min,
                         max,
                         a1,
                         a2,
                         tip);
}
uiBut *uiDefIconButS(uiBlock *block,
                     int type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     short *poin,
                     float min,
                     float max,
                     float a1,
                     float a2,
                     const char *tip)
{
  return uiDefIconBut(block,
                      type | UI_BUT_POIN_SHORT,
                      retval,
                      icon,
                      x,
                      y,
                      width,
                      height,
                      (void *)poin,
                      min,
                      max,
                      a1,
                      a2,
                      tip);
}
uiBut *uiDefIconButBitS(uiBlock *block,
                        int type,
                        int bit,
                        int retval,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        short *poin,
                        float min,
                        float max,
                        float a1,
                        float a2,
                        const char *tip)
{
  return uiDefIconButBit(block,
                         type | UI_BUT_POIN_SHORT,
                         bit,
                         retval,
                         icon,
                         x,
                         y,
                         width,
                         height,
                         (void *)poin,
                         min,
                         max,
                         a1,
                         a2,
                         tip);
}
uiBut *uiDefIconButBitC(uiBlock *block,
                        int type,
                        int bit,
                        int retval,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        char *poin,
                        float min,
                        float max,
                        float a1,
                        float a2,
                        const char *tip)
{
  return uiDefIconButBit(block,
                         type | UI_BUT_POIN_CHAR,
                         bit,
                         retval,
                         icon,
                         x,
                         y,
                         width,
                         height,
                         (void *)poin,
                         min,
                         max,
                         a1,
                         a2,
                         tip);
}
uiBut *uiDefIconButR(uiBlock *block,
                     int type,
                     int retval,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     PointerRNA *ptr,
                     const char *propname,
                     int index,
                     float min,
                     float max,
                     const char *tip)
{
  uiBut *but = ui_def_but_rna_propname(
      block, type, retval, "", x, y, width, height, ptr, propname, index, min, max, tip);
  ui_but_update_and_icon_set(but, icon);
  return but;
}
uiBut *uiDefIconButR_prop(uiBlock *block,
                          int type,
                          int retval,
                          int icon,
                          int x,
                          int y,
                          short width,
                          short height,
                          PointerRNA *ptr,
                          PropertyRNA *prop,
                          int index,
                          float min,
                          float max,
                          const char *tip)
{
  uiBut *but = ui_def_but_rna(
      block, type, retval, "", x, y, width, height, ptr, prop, index, min, max, tip);
  ui_but_update_and_icon_set(but, icon);
  return but;
}

uiBut *uiDefIconButO_ptr(uiBlock *block,
                         int type,
                         wmOperatorType *ot,
                         wmOperatorCallContext opcontext,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         const char *tip)
{
  uiBut *but = ui_def_but_operator_ptr(block, type, ot, opcontext, "", x, y, width, height, tip);
  ui_but_update_and_icon_set(but, icon);
  return but;
}
uiBut *uiDefIconButO(uiBlock *block,
                     int type,
                     const char *opname,
                     wmOperatorCallContext opcontext,
                     int icon,
                     int x,
                     int y,
                     short width,
                     short height,
                     const char *tip)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false);
  return uiDefIconButO_ptr(block, type, ot, opcontext, icon, x, y, width, height, tip);
}

uiBut *uiDefIconTextBut(uiBlock *block,
                        int type,
                        int retval,
                        int icon,
                        const StringRef str,
                        int x,
                        int y,
                        short width,
                        short height,
                        void *poin,
                        float min,
                        float max,
                        float a1,
                        float a2,
                        const char *tip)
{
  uiBut *but = ui_def_but(
      block, type, retval, str, x, y, width, height, poin, min, max, a1, a2, tip);
  ui_but_update_and_icon_set(but, icon);
  but->drawflag |= UI_BUT_ICON_LEFT;
  return but;
}
uiBut *uiDefIconTextButF(uiBlock *block,
                         int type,
                         int retval,
                         int icon,
                         const StringRef str,
                         int x,
                         int y,
                         short width,
                         short height,
                         float *poin,
                         float min,
                         float max,
                         float a1,
                         float a2,
                         const char *tip)
{
  return uiDefIconTextBut(block,
                          type | UI_BUT_POIN_FLOAT,
                          retval,
                          icon,
                          str,
                          x,
                          y,
                          width,
                          height,
                          (void *)poin,
                          min,
                          max,
                          a1,
                          a2,
                          tip);
}
uiBut *uiDefIconTextButI(uiBlock *block,
                         int type,
                         int retval,
                         int icon,
                         const StringRef str,
                         int x,
                         int y,
                         short width,
                         short height,
                         int *poin,
                         float min,
                         float max,
                         float a1,
                         float a2,
                         const char *tip)
{
  return uiDefIconTextBut(block,
                          type | UI_BUT_POIN_INT,
                          retval,
                          icon,
                          str,
                          x,
                          y,
                          width,
                          height,
                          (void *)poin,
                          min,
                          max,
                          a1,
                          a2,
                          tip);
}
uiBut *uiDefIconTextButR(uiBlock *block,
                         int type,
                         int retval,
                         int icon,
                         const char *str,
                         int x,
                         int y,
                         short width,
                         short height,
                         PointerRNA *ptr,
                         const char *propname,
                         int index,
                         float min,
                         float max,
                         const char *tip)
{
  uiBut *but = ui_def_but_rna_propname(
      block, type, retval, str, x, y, width, height, ptr, propname, index, min, max, tip);
  ui_but_update_and_icon_set(but, icon);
  but->drawflag |= UI_BUT_ICON_LEFT;
  return but;
}
uiBut *uiDefIconTextButR_prop(uiBlock *block,
                              int type,
                              int retval,
                              int icon,
                              const char *str,
                              int x,
                              int y,
                              short width,
                              short height,
                              PointerRNA *ptr,
                              PropertyRNA *prop,
                              int index,
                              float min,
                              float max,
                              const char *tip)
{
  uiBut *but = ui_def_but_rna(
      block, type, retval, str, x, y, width, height, ptr, prop, index, min, max, tip);
  ui_but_update_and_icon_set(but, icon);
  but->drawflag |= UI_BUT_ICON_LEFT;
  return but;
}
uiBut *uiDefIconTextButO_ptr(uiBlock *block,
                             int type,
                             wmOperatorType *ot,
                             wmOperatorCallContext opcontext,
                             int icon,
                             const StringRef str,
                             int x,
                             int y,
                             short width,
                             short height,
                             const char *tip)
{
  uiBut *but = ui_def_but_operator_ptr(block, type, ot, opcontext, str, x, y, width, height, tip);
  ui_but_update_and_icon_set(but, icon);
  but->drawflag |= UI_BUT_ICON_LEFT;
  return but;
}
uiBut *uiDefIconTextButO(uiBlock *block,
                         int type,
                         const char *opname,
                         wmOperatorCallContext opcontext,
                         int icon,
                         const StringRef str,
                         int x,
                         int y,
                         short width,
                         short height,
                         const char *tip)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false);
  if (str.is_empty()) {
    return uiDefIconButO_ptr(block, type, ot, opcontext, icon, x, y, width, height, tip);
  }
  return uiDefIconTextButO_ptr(block, type, ot, opcontext, icon, str, x, y, width, height, tip);
}

/* END Button containing both string label and icon */

/* cruft to make uiBlock and uiBut private */

int UI_blocklist_min_y_get(ListBase *lb)
{
  int min = 0;

  LISTBASE_FOREACH (uiBlock *, block, lb) {
    if (block == lb->first || block->rect.ymin < min) {
      min = block->rect.ymin;
    }
  }

  return min;
}

void UI_block_direction_set(uiBlock *block, char direction)
{
  block->direction = direction;
}

void UI_block_flag_enable(uiBlock *block, int flag)
{
  block->flag |= flag;
}

void UI_block_flag_disable(uiBlock *block, int flag)
{
  block->flag &= ~flag;
}

void UI_but_flag_enable(uiBut *but, int flag)
{
  but->flag |= flag;
}

void UI_but_flag2_enable(uiBut *but, int flag)
{
  but->flag2 |= flag;
}

void UI_but_flag_disable(uiBut *but, int flag)
{
  but->flag &= ~flag;
}

bool UI_but_flag_is_set(uiBut *but, int flag)
{
  return (but->flag & flag) != 0;
}

void UI_but_drawflag_enable(uiBut *but, int flag)
{
  but->drawflag |= flag;
}

void UI_but_drawflag_disable(uiBut *but, int flag)
{
  but->drawflag &= ~flag;
}

void UI_but_dragflag_enable(uiBut *but, int flag)
{
  but->dragflag |= flag;
}

void UI_but_dragflag_disable(uiBut *but, int flag)
{
  but->dragflag &= ~flag;
}

void UI_but_disable(uiBut *but, const char *disabled_hint)
{
  UI_but_flag_enable(but, UI_BUT_DISABLED);

  /* Only one disabled hint at a time currently. Don't override the previous one here. */
  if (but->disabled_info && but->disabled_info[0]) {
    return;
  }

  but->disabled_info = disabled_hint;
}

void UI_but_placeholder_set(uiBut *but, const char *placeholder_text)
{
  MEM_SAFE_FREE(but->placeholder);
  but->placeholder = BLI_strdup_null(placeholder_text);
}

const char *ui_but_placeholder_get(uiBut *but)
{
  const char *placeholder = (but->placeholder) ? but->placeholder : nullptr;

  if (!placeholder && but->rnaprop) {
    if (but->type == UI_BTYPE_SEARCH_MENU) {
      StructRNA *type = RNA_property_pointer_type(&but->rnapoin, but->rnaprop);
      const short idcode = RNA_type_to_ID_code(type);
      if (idcode != 0) {
        RNA_enum_name(rna_enum_id_type_items, idcode, &placeholder);
        placeholder = CTX_IFACE_(BLT_I18NCONTEXT_ID_ID, placeholder);
      }
      else if (type && !STREQ(RNA_struct_identifier(type), "UnknownType")) {
        placeholder = RNA_struct_ui_name(type);
      }
    }
    else if (but->type == UI_BTYPE_TEXT && but->icon == ICON_VIEWZOOM) {
      placeholder = CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "Search");
    }
  }

  return placeholder;
}

void UI_but_type_set_menu_from_pulldown(uiBut *but)
{
  BLI_assert(but->type == UI_BTYPE_PULLDOWN);
  but->type = UI_BTYPE_MENU;
  UI_but_drawflag_disable(but, UI_BUT_TEXT_RIGHT);
  UI_but_drawflag_enable(but, UI_BUT_TEXT_LEFT);
}

int UI_but_return_value_get(uiBut *but)
{
  return but->retval;
}

PointerRNA *UI_but_operator_ptr_ensure(uiBut *but)
{
  if (but->optype && !but->opptr) {
    but->opptr = MEM_cnew<PointerRNA>(__func__);
    WM_operator_properties_create_ptr(but->opptr, but->optype);
  }

  return but->opptr;
}

void UI_but_context_ptr_set(uiBlock *block, uiBut *but, const char *name, const PointerRNA *ptr)
{
  bContextStore *ctx = CTX_store_add(block->contexts, name, ptr);
  ctx->used = true;
  but->context = ctx;
}

const PointerRNA *UI_but_context_ptr_get(const uiBut *but, const char *name, const StructRNA *type)
{
  return CTX_store_ptr_lookup(but->context, name, type);
}

const bContextStore *UI_but_context_get(const uiBut *but)
{
  return but->context;
}

void UI_but_unit_type_set(uiBut *but, const int unit_type)
{
  but->unit_type = uchar(RNA_SUBTYPE_UNIT_VALUE(unit_type));
}

int UI_but_unit_type_get(const uiBut *but)
{
  const int ownUnit = int(but->unit_type);

  /* own unit define always takes precedence over RNA provided, allowing for overriding
   * default value provided in RNA in a few special cases (i.e. Active Keyframe in Graph Edit)
   */
  /* XXX: this doesn't allow clearing unit completely, though the same could be said for icons */
  if ((ownUnit != 0) || (but->rnaprop == nullptr)) {
    return ownUnit << 16;
  }
  return RNA_SUBTYPE_UNIT(RNA_property_subtype(but->rnaprop));
}

void UI_block_func_handle_set(uiBlock *block, uiBlockHandleFunc func, void *arg)
{
  block->handle_func = func;
  block->handle_func_arg = arg;
}

void UI_block_func_butmenu_set(uiBlock *block, uiMenuHandleFunc func, void *arg)
{
  block->butm_func = func;
  block->butm_func_arg = arg;
}

void UI_block_func_set(uiBlock *block, uiButHandleFunc func, void *arg1, void *arg2)
{
  block->func = func;
  block->func_arg1 = arg1;
  block->func_arg2 = arg2;
}

void UI_block_funcN_set(uiBlock *block, uiButHandleNFunc funcN, void *argN, void *arg2)
{
  if (block->func_argN) {
    MEM_freeN(block->func_argN);
  }

  block->funcN = funcN;
  block->func_argN = argN;
  block->func_arg2 = arg2;
}

void UI_but_func_rename_set(uiBut *but, uiButHandleRenameFunc func, void *arg1)
{
  but->rename_func = func;
  but->rename_arg1 = arg1;
}

void UI_but_func_drawextra_set(
    uiBlock *block,
    void (*func)(const bContext *C, void *idv, void *arg1, void *arg2, rcti *rect),
    void *arg1,
    void *arg2)
{
  block->drawextra = func;
  block->drawextra_arg1 = arg1;
  block->drawextra_arg2 = arg2;
}

void UI_but_func_set(uiBut *but, uiButHandleFunc func, void *arg1, void *arg2)
{
  but->func = func;
  but->func_arg1 = arg1;
  but->func_arg2 = arg2;
}

void UI_but_func_set(uiBut *but, std::function<void(bContext &)> func)
{
  but->apply_func = std::move(func);
}

void UI_but_funcN_set(uiBut *but, uiButHandleNFunc funcN, void *argN, void *arg2)
{
  if (but->func_argN) {
    MEM_freeN(but->func_argN);
  }

  but->funcN = funcN;
  but->func_argN = argN;
  but->func_arg2 = arg2;
}

void UI_but_func_complete_set(uiBut *but, uiButCompleteFunc func, void *arg)
{
  but->autocomplete_func = func;
  but->autofunc_arg = arg;
}

void UI_but_func_menu_step_set(uiBut *but, uiMenuStepFunc func)
{
  but->menu_step_func = func;
}

void UI_but_func_tooltip_label_set(uiBut *but, std::function<std::string(const uiBut *but)> func)
{
  but->tip_label_func = std::move(func);
  UI_but_drawflag_enable(but, UI_BUT_HAS_TOOLTIP_LABEL);
}

void UI_but_func_tooltip_set(uiBut *but, uiButToolTipFunc func, void *arg, uiFreeArgFunc free_arg)
{
  but->tip_func = func;
  if (but->tip_arg_free) {
    but->tip_arg_free(but->tip_arg);
  }
  but->tip_arg = arg;
  but->tip_arg_free = free_arg;
}

void UI_but_func_tooltip_custom_set(uiBut *but,
                                    uiButToolTipCustomFunc func,
                                    void *arg,
                                    uiFreeArgFunc free_arg)
{
  but->tip_custom_func = func;
  if (but->tip_arg_free) {
    but->tip_arg_free(but->tip_arg);
  }
  but->tip_arg = arg;
  but->tip_arg_free = free_arg;
}

void UI_but_func_pushed_state_set(uiBut *but, std::function<bool(const uiBut &)> func)
{
  but->pushed_state_func = func;
  ui_but_update(but);
}

uiBut *uiDefBlockBut(uiBlock *block,
                     uiBlockCreateFunc func,
                     void *arg,
                     const StringRef str,
                     int x,
                     int y,
                     short width,
                     short height,
                     const char *tip)
{
  uiBut *but = ui_def_but(
      block, UI_BTYPE_BLOCK, 0, str, x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);
  but->block_create_func = func;
  ui_but_update(but);
  return but;
}

uiBut *uiDefBlockButN(uiBlock *block,
                      uiBlockCreateFunc func,
                      void *argN,
                      const StringRef str,
                      int x,
                      int y,
                      short width,
                      short height,
                      const char *tip)
{
  uiBut *but = ui_def_but(
      block, UI_BTYPE_BLOCK, 0, str, x, y, width, height, nullptr, 0.0, 0.0, 0.0, 0.0, tip);
  but->block_create_func = func;
  if (but->func_argN) {
    MEM_freeN(but->func_argN);
  }
  but->func_argN = argN;
  ui_but_update(but);
  return but;
}

uiBut *uiDefMenuBut(uiBlock *block,
                    uiMenuCreateFunc func,
                    void *arg,
                    const StringRef str,
                    int x,
                    int y,
                    short width,
                    short height,
                    const char *tip)
{
  uiBut *but = ui_def_but(
      block, UI_BTYPE_PULLDOWN, 0, str, x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);
  but->menu_create_func = func;
  ui_but_update(but);
  return but;
}

uiBut *uiDefIconTextMenuBut(uiBlock *block,
                            uiMenuCreateFunc func,
                            void *arg,
                            int icon,
                            const StringRef str,
                            int x,
                            int y,
                            short width,
                            short height,
                            const char *tip)
{
  uiBut *but = ui_def_but(
      block, UI_BTYPE_PULLDOWN, 0, str, x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);

  ui_def_but_icon(but, icon, UI_HAS_ICON);

  but->drawflag |= UI_BUT_ICON_LEFT;
  ui_but_submenu_enable(block, but);

  but->menu_create_func = func;
  ui_but_update(but);

  return but;
}

uiBut *uiDefIconMenuBut(uiBlock *block,
                        uiMenuCreateFunc func,
                        void *arg,
                        int icon,
                        int x,
                        int y,
                        short width,
                        short height,
                        const char *tip)
{
  uiBut *but = ui_def_but(
      block, UI_BTYPE_PULLDOWN, 0, "", x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);

  ui_def_but_icon(but, icon, UI_HAS_ICON);
  but->drawflag &= ~UI_BUT_ICON_LEFT;

  but->menu_create_func = func;
  ui_but_update(but);

  return but;
}

uiBut *uiDefIconBlockBut(uiBlock *block,
                         uiBlockCreateFunc func,
                         void *arg,
                         int retval,
                         int icon,
                         int x,
                         int y,
                         short width,
                         short height,
                         const char *tip)
{
  uiBut *but = ui_def_but(
      block, UI_BTYPE_BLOCK, retval, "", x, y, width, height, arg, 0.0, 0.0, 0.0, 0.0, tip);

  ui_def_but_icon(but, icon, UI_HAS_ICON);

  but->drawflag |= UI_BUT_ICON_LEFT;

  but->block_create_func = func;
  ui_but_update(but);

  return but;
}

uiBut *uiDefSearchBut(uiBlock *block,
                      void *arg,
                      int retval,
                      int icon,
                      int maxncpy,
                      int x,
                      int y,
                      short width,
                      short height,
                      float a1,
                      float a2,
                      const char *tip)
{
  uiBut *but = ui_def_but(block,
                          UI_BTYPE_SEARCH_MENU,
                          retval,
                          "",
                          x,
                          y,
                          width,
                          height,
                          arg,
                          0.0,
                          maxncpy,
                          a1,
                          a2,
                          tip);

  ui_def_but_icon(but, icon, UI_HAS_ICON);

  but->drawflag |= UI_BUT_ICON_LEFT | UI_BUT_TEXT_LEFT;

  ui_but_update(but);

  return but;
}

void UI_but_func_search_set(uiBut *but,
                            uiButSearchCreateFn search_create_fn,
                            uiButSearchUpdateFn search_update_fn,
                            void *arg,
                            const bool free_arg,
                            uiFreeArgFunc search_arg_free_fn,
                            uiButHandleFunc search_exec_fn,
                            void *active)
{
  uiButSearch *search_but = (uiButSearch *)but;

  BLI_assert(but->type == UI_BTYPE_SEARCH_MENU);

  /* needed since callers don't have access to internal functions
   * (as an alternative we could expose it) */
  if (search_create_fn == nullptr) {
    search_create_fn = ui_searchbox_create_generic;
  }

  if (search_but->arg_free_fn != nullptr) {
    search_but->arg_free_fn(search_but->arg);
    search_but->arg = nullptr;
  }

  search_but->popup_create_fn = search_create_fn;
  search_but->items_update_fn = search_update_fn;
  search_but->item_active = active;

  search_but->arg = arg;
  search_but->arg_free_fn = search_arg_free_fn;

  if (search_exec_fn) {
#ifndef NDEBUG
    if (but->func) {
      /* watch this, can be cause of much confusion, see: #47691 */
      printf("%s: warning, overwriting button callback with search function callback!\n",
             __func__);
    }
#endif
    /* Handling will pass the active item as arg2 later, so keep it nullptr here. */
    if (free_arg) {
      UI_but_funcN_set(but, search_exec_fn, search_but->arg, nullptr);
    }
    else {
      UI_but_func_set(but, search_exec_fn, search_but->arg, nullptr);
    }
  }

  /* search buttons show red-alert if item doesn't exist, not for menus. Don't do this for
   * buttons where any result is valid anyway, since any string will be valid anyway. */
  if (0 == (but->block->flag & UI_BLOCK_LOOP) && !search_but->results_are_suggestions) {
    /* skip empty buttons, not all buttons need input, we only show invalid */
    if (!but->drawstr.empty()) {
      ui_but_search_refresh(search_but);
    }
  }
}

void UI_but_func_search_set_context_menu(uiBut *but, uiButSearchContextMenuFn context_menu_fn)
{
  uiButSearch *but_search = (uiButSearch *)but;
  BLI_assert(but->type == UI_BTYPE_SEARCH_MENU);

  but_search->item_context_menu_fn = context_menu_fn;
}

void UI_but_func_search_set_sep_string(uiBut *but, const char *search_sep_string)
{
  uiButSearch *but_search = (uiButSearch *)but;
  BLI_assert(but->type == UI_BTYPE_SEARCH_MENU);

  but_search->item_sep_string = search_sep_string;
}

void UI_but_func_search_set_tooltip(uiBut *but, uiButSearchTooltipFn tooltip_fn)
{
  uiButSearch *but_search = (uiButSearch *)but;
  BLI_assert(but->type == UI_BTYPE_SEARCH_MENU);

  but_search->item_tooltip_fn = tooltip_fn;
}

void UI_but_func_search_set_listen(uiBut *but, uiButSearchListenFn listen_fn)
{
  uiButSearch *but_search = (uiButSearch *)but;
  BLI_assert(but->type == UI_BTYPE_SEARCH_MENU);
  but_search->listen_fn = listen_fn;
}

void UI_but_func_search_set_results_are_suggestions(uiBut *but, const bool value)
{
  uiButSearch *but_search = (uiButSearch *)but;
  BLI_assert(but->type == UI_BTYPE_SEARCH_MENU);

  but_search->results_are_suggestions = value;
}

/* Callbacks for operator search button. */
static void operator_enum_search_update_fn(
    const bContext *C, void *but, const char *str, uiSearchItems *items, const bool /*is_first*/)
{
  wmOperatorType *ot = ((uiBut *)but)->optype;
  PropertyRNA *prop = ot->prop;

  if (prop == nullptr) {
    printf("%s: %s has no enum property set\n", __func__, ot->idname);
  }
  else if (RNA_property_type(prop) != PROP_ENUM) {
    printf("%s: %s \"%s\" is not an enum property\n",
           __func__,
           ot->idname,
           RNA_property_identifier(prop));
  }
  else {
    /* Will create it if needed! */
    PointerRNA *ptr = UI_but_operator_ptr_ensure(static_cast<uiBut *>(but));

    bool do_free;
    const EnumPropertyItem *all_items;
    RNA_property_enum_items_gettexted((bContext *)C, ptr, prop, &all_items, nullptr, &do_free);

    blender::ui::string_search::StringSearch<const EnumPropertyItem> search;

    for (const EnumPropertyItem *item = all_items; item->identifier; item++) {
      search.add(item->name, item);
    }

    const blender::Vector<const EnumPropertyItem *> filtered_items = search.query(str);
    for (const EnumPropertyItem *item : filtered_items) {
      /* NOTE: need to give the index rather than the
       * identifier because the enum can be freed */
      if (!UI_search_item_add(items, item->name, POINTER_FROM_INT(item->value), item->icon, 0, 0))
      {
        break;
      }
    }

    if (do_free) {
      MEM_freeN((void *)all_items);
    }
  }
}

static void operator_enum_search_exec_fn(bContext * /*C*/, void *but, void *arg2)
{
  wmOperatorType *ot = ((uiBut *)but)->optype;
  /* Will create it if needed! */
  PointerRNA *opptr = UI_but_operator_ptr_ensure(static_cast<uiBut *>(but));

  if (ot) {
    if (ot->prop) {
      RNA_property_enum_set(opptr, ot->prop, POINTER_AS_INT(arg2));
      /* We do not call op from here, will be called by button code.
       * ui_apply_but_funcs_after() (in `interface_handlers.cc`)
       * called this func before checking operators,
       * because one of its parameters is the button itself! */
    }
    else {
      printf("%s: op->prop for '%s' is nullptr\n", __func__, ot->idname);
    }
  }
}

uiBut *uiDefSearchButO_ptr(uiBlock *block,
                           wmOperatorType *ot,
                           IDProperty *properties,
                           void *arg,
                           int retval,
                           int icon,
                           int maxncpy,
                           int x,
                           int y,
                           short width,
                           short height,
                           float a1,
                           float a2,
                           const char *tip)
{
  uiBut *but = uiDefSearchBut(block, arg, retval, icon, maxncpy, x, y, width, height, a1, a2, tip);
  UI_but_func_search_set(but,
                         ui_searchbox_create_generic,
                         operator_enum_search_update_fn,
                         but,
                         false,
                         nullptr,
                         operator_enum_search_exec_fn,
                         nullptr);

  but->optype = ot;
  but->opcontext = WM_OP_EXEC_DEFAULT;

  if (properties) {
    PointerRNA *ptr = UI_but_operator_ptr_ensure(but);
    /* Copy id-properties. */
    ptr->data = IDP_CopyProperty(properties);
  }

  return but;
}

void UI_but_hint_drawstr_set(uiBut *but, const char *string)
{
  ui_but_add_shortcut(but, string, false);
}

void UI_but_icon_indicator_number_set(uiBut *but, const int indicator_number)
{
  UI_icon_text_overlay_init_from_count(&but->icon_overlay_text, indicator_number);
}

void UI_but_node_link_set(uiBut *but, bNodeSocket *socket, const float draw_color[4])
{
  but->flag |= UI_BUT_NODE_LINK;
  but->custom_data = socket;
  rgba_float_to_uchar(but->col, draw_color);
}

void UI_but_number_step_size_set(uiBut *but, float step_size)
{
  uiButNumber *but_number = (uiButNumber *)but;
  BLI_assert(but->type == UI_BTYPE_NUM);

  but_number->step_size = step_size;
  BLI_assert(step_size > 0);
}

void UI_but_number_precision_set(uiBut *but, float precision)
{
  uiButNumber *but_number = (uiButNumber *)but;
  BLI_assert(but->type == UI_BTYPE_NUM);

  but_number->precision = precision;
  /* -1 is a valid value, UI code figures out an appropriate precision then. */
  BLI_assert(precision > -2);
}

void UI_but_number_slider_step_size_set(uiBut *but, float step_size)
{
  uiButNumberSlider *but_number = (uiButNumberSlider *)but;
  BLI_assert(but->type == UI_BTYPE_NUM_SLIDER);

  but_number->step_size = step_size;
  BLI_assert(step_size > 0);
}

void UI_but_number_slider_precision_set(uiBut *but, float precision)
{
  uiButNumberSlider *but_number = (uiButNumberSlider *)but;
  BLI_assert(but->type == UI_BTYPE_NUM_SLIDER);

  but_number->precision = precision;
  /* -1 is a valid value, UI code figures out an appropriate precision then. */
  BLI_assert(precision > -2);
}

void UI_but_focus_on_enter_event(wmWindow *win, uiBut *but)
{
  wmEvent event;
  wm_event_init_from_window(win, &event);

  event.type = EVT_BUT_OPEN;
  event.val = KM_PRESS;
  event.flag = static_cast<eWM_EventFlag>(0);
  event.customdata = but;
  event.customdata_free = false;

  wm_event_add(win, &event);
}

void UI_but_func_hold_set(uiBut *but, uiButHandleHoldFunc func, void *argN)
{
  but->hold_func = func;
  but->hold_argN = argN;
}

std::optional<EnumPropertyItem> UI_but_rna_enum_item_get(bContext &C, uiBut &but)
{
  PointerRNA *ptr = nullptr;
  PropertyRNA *prop = nullptr;
  int value = 0;
  if (but.rnaprop && RNA_property_type(but.rnaprop) == PROP_ENUM) {
    ptr = &but.rnapoin;
    prop = but.rnaprop;
    value = ELEM(but.type, UI_BTYPE_ROW, UI_BTYPE_TAB) ? int(but.hardmax) :
                                                         int(ui_but_value_get(&but));
  }
  else if (but.optype) {
    wmOperatorType *ot = but.optype;

    /* So the context is passed to `itemf` functions. */
    PointerRNA *opptr = UI_but_operator_ptr_ensure(&but);
    WM_operator_properties_sanitize(opptr, false);

    /* If the default property of the operator is an enum and is set, fetch the tooltip of the
     * selected value so that "Snap" and "Mirror" operator menus in the Animation Editors will
     * show tooltips for the different operations instead of the meaningless generic tooltip. */
    if (ot->prop && RNA_property_type(ot->prop) == PROP_ENUM) {
      if (RNA_struct_contains_property(opptr, ot->prop)) {
        ptr = opptr;
        prop = ot->prop;
        value = RNA_property_enum_get(opptr, ot->prop);
      }
    }
  }

  if (!ptr || !prop) {
    return std::nullopt;
  }

  EnumPropertyItem item;
  if (!RNA_property_enum_item_from_value_gettexted(&C, ptr, prop, value, &item)) {
    return std::nullopt;
  }

  return item;
}

std::string UI_but_string_get_rna_property_identifier(const uiBut &but)
{
  if (!but.rnaprop) {
    return {};
  }
  return RNA_property_identifier(but.rnaprop);
}

std::string UI_but_string_get_rna_struct_identifier(const uiBut &but)
{
  if (but.rnaprop && but.rnapoin.data) {
    return RNA_struct_identifier(but.rnapoin.type);
  }
  if (but.optype) {
    return but.optype->idname;
  }
  if (ELEM(but.type, UI_BTYPE_MENU, UI_BTYPE_PULLDOWN)) {
    if (MenuType *mt = UI_but_menutype_get(&but)) {
      return mt->idname;
    }
  }
  if (but.type == UI_BTYPE_POPOVER) {
    if (PanelType *pt = UI_but_paneltype_get(&but)) {
      return pt->idname;
    }
  }
  return {};
}

std::string UI_but_string_get_label(uiBut &but)
{
  if (!but.str.empty()) {
    size_t str_len = but.str.size();
    if (but.flag & UI_BUT_HAS_SEP_CHAR) {
      const size_t sep_index = but.str.find_first_of(UI_SEP_CHAR);
      if (sep_index != std::string::npos) {
        str_len = sep_index;
      }
    }
    return but.str.substr(0, str_len);
  }
  return UI_but_string_get_rna_label(but);
}

std::string UI_but_string_get_tooltip_label(const uiBut &but)
{
  if (!but.tip_label_func) {
    return {};
  }
  return but.tip_label_func(&but);
}

std::string UI_but_string_get_rna_label(uiBut &but)
{
  if (but.rnaprop) {
    return RNA_property_ui_name(but.rnaprop);
  }
  if (but.optype) {
    PointerRNA *opptr = UI_but_operator_ptr_ensure(&but);
    return WM_operatortype_name(but.optype, opptr).c_str();
  }
  if (ELEM(but.type, UI_BTYPE_MENU, UI_BTYPE_PULLDOWN, UI_BTYPE_POPOVER)) {
    if (MenuType *mt = UI_but_menutype_get(&but)) {
      return CTX_TIP_(mt->translation_context, mt->label);
    }

    if (wmOperatorType *ot = UI_but_operatortype_get_from_enum_menu(&but, nullptr)) {
      return WM_operatortype_name(ot, nullptr).c_str();
    }

    if (PanelType *pt = UI_but_paneltype_get(&but)) {
      return CTX_TIP_(pt->translation_context, pt->label);
    }
  }
  return {};
}

std::string UI_but_string_get_rna_label_context(const uiBut &but)
{
  if (but.rnaprop) {
    return RNA_property_translation_context(but.rnaprop);
  }
  if (but.optype) {
    return RNA_struct_translation_context(but.optype->srna);
  }
  if (ELEM(but.type, UI_BTYPE_MENU, UI_BTYPE_PULLDOWN)) {
    if (MenuType *mt = UI_but_menutype_get(&but)) {
      return RNA_struct_translation_context(mt->rna_ext.srna);
    }
  }
  return BLT_I18NCONTEXT_DEFAULT_BPYRNA;
}

std::string UI_but_string_get_tooltip(bContext &C, uiBut &but)
{
  if (but.tip_func) {
    return but.tip_func(&C, but.tip_arg, but.tip);
  }
  if (but.tip && but.tip[0]) {
    return but.tip;
  }
  return UI_but_string_get_rna_tooltip(C, but);
}

std::string UI_but_string_get_rna_tooltip(bContext &C, uiBut &but)
{
  if (but.rnaprop) {
    const char *t = RNA_property_ui_description(but.rnaprop);
    if (t && t[0]) {
      return t;
    }
  }
  else if (but.optype) {
    PointerRNA *opptr = UI_but_operator_ptr_ensure(&but);
    const bContextStore *previous_ctx = CTX_store_get(&C);
    CTX_store_set(&C, but.context);
    std::string tmp = WM_operatortype_description(&C, but.optype, opptr).c_str();
    CTX_store_set(&C, previous_ctx);
    return tmp;
  }
  if (ELEM(but.type, UI_BTYPE_MENU, UI_BTYPE_PULLDOWN, UI_BTYPE_POPOVER)) {
    if (MenuType *mt = UI_but_menutype_get(&but)) {
      /* Not all menus are from Python. */
      if (mt->rna_ext.srna) {
        const char *t = RNA_struct_ui_description(mt->rna_ext.srna);
        if (t && t[0]) {
          return t;
        }
      }
    }

    if (wmOperatorType *ot = UI_but_operatortype_get_from_enum_menu(&but, nullptr)) {
      return WM_operatortype_description(&C, ot, nullptr).c_str();
    }
  }

  return {};
}

std::string UI_but_string_get_operator_keymap(bContext &C, uiBut &but)
{
  char buf[128];
  if (!ui_but_event_operator_string(&C, &but, buf, sizeof(buf))) {
    return {};
  }
  return buf;
}

std::string UI_but_string_get_property_keymap(bContext &C, uiBut &but)
{
  char buf[128];
  if (!ui_but_event_property_operator_string(&C, &but, buf, sizeof(buf))) {
    return {};
  }
  return buf;
}

std::string UI_but_extra_icon_string_get_label(const uiButExtraOpIcon &extra_icon)
{
  wmOperatorType *optype = UI_but_extra_operator_icon_optype_get(&extra_icon);
  PointerRNA *opptr = UI_but_extra_operator_icon_opptr_get(&extra_icon);
  return WM_operatortype_name(optype, opptr);
}

std::string UI_but_extra_icon_string_get_tooltip(bContext &C, const uiButExtraOpIcon &extra_icon)
{
  wmOperatorType *optype = UI_but_extra_operator_icon_optype_get(&extra_icon);
  PointerRNA *opptr = UI_but_extra_operator_icon_opptr_get(&extra_icon);
  return WM_operatortype_description(&C, optype, opptr);
}

std::string UI_but_extra_icon_string_get_operator_keymap(const bContext &C,
                                                         const uiButExtraOpIcon &extra_icon)
{
  char buf[128];
  if (!ui_but_extra_icon_event_operator_string(&C, &extra_icon, buf, sizeof(buf))) {
    return {};
  }
  return buf;
}

/* Program Init/Exit */

void UI_init()
{
  ui_resources_init();
}

void UI_init_userdef()
{
  /* Initialize UI variables from values set in the preferences. */
  uiStyleInit();
}

void UI_reinit_font()
{
  uiStyleInit();
}

void UI_exit()
{
  ui_resources_free();
  ui_but_clipboard_free();
}

void UI_interface_tag_script_reload()
{
  ui_interface_tag_script_reload_queries();
}
