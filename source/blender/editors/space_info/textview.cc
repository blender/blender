/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spinfo
 */

#include "MEM_guardedalloc.h"

#include "BLF_api.h"

#include "BLI_math_color.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "DNA_userdef_types.h" /* For 'UI_SCALE_FAC' */

#include "UI_interface.hh"
#include "UI_interface_icons.hh"

#include "textview.hh"

static void textview_font_begin(const int font_id, const int lheight)
{
  /* Font size in relation to line height. */
  BLF_size(font_id, 0.8f * lheight);
}

struct TextViewDrawState {
  int font_id;
  int cwidth;
  int lheight;
  /** Text vertical offset per line. */
  int lofs;
  int row_vpadding;
  /** Number of characters that fit into the width of the console (fixed width). */
  int columns;
  /** For drawing text. */
  const rcti *draw_rect;
  /** For drawing backgrounds colors which may extend beyond text. */
  const rcti *draw_rect_outer;
  int scroll_ymin, scroll_ymax;
  int *xy;   // [2]
  int *sel;  // [2]
  /* Bottom of view == 0, top of file == combine chars, end of line is lower than start. */
  int *mval_pick_offset;
  const int *mval;  // [2]
  bool do_draw;
};

BLI_INLINE void textview_step_sel(TextViewDrawState *tds, const int step)
{
  tds->sel[0] += step;
  tds->sel[1] += step;
}

static void textview_draw_sel(const char *str,
                              const int xy[2],
                              const int str_len_draw,
                              TextViewDrawState *tds,
                              const uchar bg_sel[4])
{
  const int sel[2] = {tds->sel[0], tds->sel[1]};
  const int cwidth = tds->cwidth;
  const int lheight = tds->lheight;

  if (sel[0] <= str_len_draw && sel[1] >= 0) {
    const int sta = BLI_str_utf8_offset_to_column(str, max_ii(sel[0], 0));
    const int end = BLI_str_utf8_offset_to_column(str, min_ii(sel[1], str_len_draw));

    GPU_blend(GPU_BLEND_ALPHA);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    immUniformColor4ubv(bg_sel);
    immRecti(pos, xy[0] + (cwidth * sta), xy[1] + lheight, xy[0] + (cwidth * end), xy[1]);

    immUnbindProgram();

    GPU_blend(GPU_BLEND_NONE);
  }
}

/**
 * \warning Allocated memory for 'r_offsets' must be freed by caller.
 * \return The length in bytes.
 */
static int textview_wrap_offsets(
    const char *str, const int str_len, const int width, int *r_lines, int **r_offsets)
{
  int i, end; /* Offset as unicode code-point. */
  int j;      /* Offset as bytes. */

  *r_lines = 1;

  *r_offsets = static_cast<int *>(MEM_callocN(
      sizeof(**r_offsets) *
          (str_len * BLI_UTF8_WIDTH_MAX / MAX2(1, width - (BLI_UTF8_WIDTH_MAX - 1)) + 1),
      __func__));
  (*r_offsets)[0] = 0;

  for (i = 0, end = width, j = 0; j < str_len && str[j]; j += BLI_str_utf8_size_safe(str + j)) {
    int columns = BLI_str_utf8_char_width_safe(str + j);

    if (i + columns > end) {
      (*r_offsets)[*r_lines] = j;
      (*r_lines)++;

      end = i + width;
    }
    i += columns;
  }
  return j;
}

/**
 * return false if the last line is off the screen
 * should be able to use this for any string type.
 */
static bool textview_draw_string(TextViewDrawState *tds,
                                 const char *str,
                                 int str_len,
                                 const uchar fg[4],
                                 const uchar bg[4],
                                 int icon,
                                 const uchar icon_fg[4],
                                 const uchar icon_bg[4],
                                 const uchar bg_sel[4])
{
  int tot_lines; /* Total number of lines for wrapping. */
  int *offsets;  /* Offsets of line beginnings for wrapping. */

  str_len = textview_wrap_offsets(str, str_len, tds->columns, &tot_lines, &offsets);

  int line_height = (tot_lines * tds->lheight) + (tds->row_vpadding * 2);
  int line_bottom = tds->xy[1];
  int line_top = line_bottom + line_height;

  int y_next = line_top;

  /* Just advance the height. */
  if (tds->do_draw == false) {
    if (tds->mval_pick_offset && tds->mval[1] != INT_MAX && line_bottom <= tds->mval[1]) {
      if (y_next >= tds->mval[1]) {
        int ofs = 0;

        /* Wrap. */
        if (tot_lines > 1) {
          int iofs = int(float(y_next - tds->mval[1]) / tds->lheight);
          ofs += offsets[MIN2(iofs, tot_lines - 1)];
        }

        /* Last part. */
        ofs += BLI_str_utf8_offset_from_column(str + ofs,
                                               int(floor(float(tds->mval[0]) / tds->cwidth)));

        CLAMP(ofs, 0, str_len);
        *tds->mval_pick_offset += str_len - ofs;
      }
      else {
        *tds->mval_pick_offset += str_len + 1;
      }
    }

    tds->xy[1] = y_next;
    MEM_freeN(offsets);
    return true;
  }
  if (y_next < tds->scroll_ymin) {
    /* Have not reached the drawable area so don't break. */
    tds->xy[1] = y_next;

    /* Adjust selection even if not drawing. */
    if (tds->sel[0] != tds->sel[1]) {
      textview_step_sel(tds, -(str_len + 1));
    }

    MEM_freeN(offsets);
    return true;
  }

  size_t len;
  const char *s;
  int i;

  int sel_orig[2];
  copy_v2_v2_int(sel_orig, tds->sel);

  /* Invert and swap for wrapping. */
  tds->sel[0] = str_len - sel_orig[1];
  tds->sel[1] = str_len - sel_orig[0];

  if (bg) {
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4ubv(bg);
    immRecti(pos, tds->draw_rect_outer->xmin, line_bottom, tds->draw_rect_outer->xmax, line_top);
    immUnbindProgram();
  }

  if (icon_bg) {
    float col[4];
    int bg_size = UI_ICON_SIZE * 1.2;
    float vpadding = (tds->lheight + (tds->row_vpadding * 2) - bg_size) / 2;
    float hpadding = tds->draw_rect->xmin - (bg_size * 1.2f);

    rgba_uchar_to_float(col, icon_bg);
    UI_draw_roundbox_corner_set(UI_CNR_ALL);

    rctf roundbox_rect;
    roundbox_rect.xmin = hpadding;
    roundbox_rect.xmax = bg_size + hpadding;
    roundbox_rect.ymin = line_top - bg_size - vpadding;
    roundbox_rect.ymax = line_top - vpadding;

    UI_draw_roundbox_4fv(&roundbox_rect, true, 4 * UI_SCALE_FAC, col);
  }

  if (icon) {
    int vpadding = (tds->lheight + (tds->row_vpadding * 2) - UI_ICON_SIZE) / 2;
    int hpadding = tds->draw_rect->xmin - (UI_ICON_SIZE * 1.3f);

    GPU_blend(GPU_BLEND_ALPHA);
    UI_icon_draw_ex(hpadding,
                    line_top - UI_ICON_SIZE - vpadding,
                    icon,
                    (16 / UI_ICON_SIZE),
                    1.0f,
                    0.0f,
                    icon_fg,
                    false,
                    UI_NO_ICON_OVERLAY_TEXT);
    GPU_blend(GPU_BLEND_NONE);
  }

  tds->xy[1] += tds->row_vpadding;

  /* Last part needs no clipping. */
  const int final_offset = offsets[tot_lines - 1];
  len = str_len - final_offset;
  s = str + final_offset;

  if (tds->sel[0] != tds->sel[1]) {
    textview_step_sel(tds, -final_offset);
    const int pos[2] = {tds->xy[0], line_bottom};
    textview_draw_sel(s, pos, len, tds, bg_sel);
  }

  BLF_position(tds->font_id, tds->xy[0], tds->lofs + line_bottom + tds->row_vpadding, 0);
  BLF_color4ubv(tds->font_id, fg);
  BLF_draw_mono(tds->font_id, s, len, tds->cwidth);

  tds->xy[1] += tds->lheight;

  BLF_color4ubv(tds->font_id, fg);

  for (i = tot_lines - 1; i > 0; i--) {
    len = offsets[i] - offsets[i - 1];
    s = str + offsets[i - 1];

    if (tds->sel[0] != tds->sel[1]) {
      textview_step_sel(tds, len);
      textview_draw_sel(s, tds->xy, len, tds, bg_sel);
    }

    BLF_position(tds->font_id, tds->xy[0], tds->lofs + tds->xy[1], 0);
    BLF_draw_mono(tds->font_id, s, len, tds->cwidth);

    tds->xy[1] += tds->lheight;

    /* Check if we're out of view bounds. */
    if (tds->xy[1] > tds->scroll_ymax) {
      MEM_freeN(offsets);
      return false;
    }
  }

  tds->xy[1] = y_next;

  copy_v2_v2_int(tds->sel, sel_orig);
  textview_step_sel(tds, -(str_len + 1));

  MEM_freeN(offsets);
  return true;
}

int textview_draw(TextViewContext *tvc,
                  const bool do_draw,
                  const int mval_init[2],
                  void **r_mval_pick_item,
                  int *r_mval_pick_offset)
{
  TextViewDrawState tds = {0};

  const int x_orig = tvc->draw_rect.xmin, y_orig = tvc->draw_rect.ymin;
  int xy[2];
  /* Disable selection by. */
  int sel[2] = {-1, -1};
  uchar fg[4], bg[4], icon_fg[4], icon_bg[4];
  int icon = 0;
  const int font_id = blf_mono_font;

  textview_font_begin(font_id, tvc->lheight);

  xy[0] = x_orig;
  xy[1] = y_orig;

  /* Offset and clamp the results,
   * clamping so moving the cursor out of the bounds doesn't wrap onto the other lines. */
  const int mval[2] = {
      (mval_init[0] == INT_MAX) ?
          INT_MAX :
          CLAMPIS(mval_init[0], tvc->draw_rect.xmin, tvc->draw_rect.xmax) - tvc->draw_rect.xmin,
      (mval_init[1] == INT_MAX) ?
          INT_MAX :
          CLAMPIS(mval_init[1], tvc->draw_rect.ymin, tvc->draw_rect.ymax) + tvc->scroll_ymin,
  };

  if (r_mval_pick_offset != nullptr) {
    *r_mval_pick_offset = 0;
  }

  /* Constants for the text-view context. */
  tds.font_id = font_id;
  tds.cwidth = int(BLF_fixed_width(font_id));
  BLI_assert(tds.cwidth > 0);
  tds.lheight = tvc->lheight;
  tds.row_vpadding = tvc->row_vpadding;
  tds.lofs = -BLF_descender(font_id);
  /* NOTE: scroll bar must be already subtracted. */
  tds.columns = (tvc->draw_rect.xmax - tvc->draw_rect.xmin) / tds.cwidth;
  /* Avoid divide by zero on small windows. */
  if (tds.columns < 1) {
    tds.columns = 1;
  }
  tds.draw_rect = &tvc->draw_rect;
  tds.draw_rect_outer = &tvc->draw_rect_outer;
  tds.scroll_ymin = tvc->scroll_ymin;
  tds.scroll_ymax = tvc->scroll_ymax;
  tds.xy = xy;
  tds.sel = sel;
  tds.mval_pick_offset = r_mval_pick_offset;
  tds.mval = mval;
  tds.do_draw = do_draw;

  if (tvc->sel_start != tvc->sel_end) {
    sel[0] = tvc->sel_start;
    sel[1] = tvc->sel_end;
  }

  if (tvc->begin(tvc)) {
    uchar bg_sel[4] = {0};

    if (do_draw && tvc->const_colors) {
      tvc->const_colors(tvc, bg_sel);
    }

    int iter_index = 0;
    do {
      const char *ext_line;
      int ext_len;
      int data_flag = 0;

      const int y_prev = xy[1];

      if (do_draw) {
        data_flag = tvc->line_data(tvc, fg, bg, &icon, icon_fg, icon_bg);
      }

      tvc->line_get(tvc, &ext_line, &ext_len);

      const bool is_out_of_view_y = !textview_draw_string(
          &tds,
          ext_line,
          ext_len,
          (data_flag & TVC_LINE_FG) ? fg : nullptr,
          (data_flag & TVC_LINE_BG) ? bg : nullptr,
          (data_flag & TVC_LINE_ICON) ? icon : 0,
          (data_flag & TVC_LINE_ICON_FG) ? icon_fg : nullptr,
          (data_flag & TVC_LINE_ICON_BG) ? icon_bg : nullptr,
          bg_sel);

      if (do_draw) {
        /* We always want the cursor to draw. */
        if (tvc->draw_cursor && iter_index == 0) {
          tvc->draw_cursor(tvc, tds.cwidth, tds.columns);
        }

        /* When drawing, if we pass v2d->cur.ymax, then quit. */
        if (is_out_of_view_y) {
          break;
        }
      }

      if ((mval[1] != INT_MAX) && (mval[1] >= y_prev && mval[1] <= xy[1])) {
        *r_mval_pick_item = (void *)tvc->iter;
        break;
      }

      iter_index++;

    } while (tvc->step(tvc));
  }

  tvc->end(tvc);

  /* Sanity checks (bugs here can be tricky to track down). */
  BLI_assert(tds.lheight == tvc->lheight);
  BLI_assert(tds.row_vpadding == tvc->row_vpadding);
  BLI_assert(tds.do_draw == do_draw);

  xy[1] += tvc->lheight * 2;

  return xy[1] - y_orig;
}
