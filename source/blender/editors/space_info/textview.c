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
 * \ingroup spinfo
 */

#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "BLF_api.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string_utf8.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "textview.h"

static void console_font_begin(const int font_id, const int lheight)
{
  /* 0.875 is based on: 16 pixels lines get 14 pixel text. */
  BLF_size(font_id, 0.875 * lheight, 72);
}

typedef struct TextViewDrawState {
  int font_id;
  int cwidth;
  int lheight;
  /** Text vertical offset per line. */
  int lofs;
  /** Number of characters that fit into the width of the console (fixed width). */
  int columns;
  const rcti *draw_rect;
  int scroll_ymin, scroll_ymax;
  int *xy;   // [2]
  int *sel;  // [2]
  /* Bottom of view == 0, top of file == combine chars, end of line is lower then start. */
  int *mval_pick_offset;
  const int *mval;  // [2]
  bool do_draw;
} TextViewDrawState;

BLI_INLINE void console_step_sel(TextViewDrawState *tds, const int step)
{
  tds->sel[0] += step;
  tds->sel[1] += step;
}

static void console_draw_sel(const char *str,
                             const int sel[2],
                             const int xy[2],
                             const int str_len_draw,
                             const int cwidth,
                             const int lheight,
                             const unsigned char bg_sel[4])
{
  if (sel[0] <= str_len_draw && sel[1] >= 0) {
    const int sta = BLI_str_utf8_offset_to_column(str, max_ii(sel[0], 0));
    const int end = BLI_str_utf8_offset_to_column(str, min_ii(sel[1], str_len_draw));

    GPU_blend(true);
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    immUniformColor4ubv(bg_sel);
    immRecti(pos, xy[0] + (cwidth * sta), xy[1] - 2 + lheight, xy[0] + (cwidth * end), xy[1] - 2);

    immUnbindProgram();

    GPU_blend(false);
  }
}

/**
 * \warning Allocated memory for 'offsets' must be freed by caller.
 * \return The length in bytes.
 */
static int console_wrap_offsets(const char *str, int len, int width, int *lines, int **offsets)
{
  int i, end; /* Offset as unicode code-point. */
  int j;      /* Offset as bytes. */

  *lines = 1;

  *offsets = MEM_callocN(
      sizeof(**offsets) *
          (len * BLI_UTF8_WIDTH_MAX / MAX2(1, width - (BLI_UTF8_WIDTH_MAX - 1)) + 1),
      "console_wrap_offsets");
  (*offsets)[0] = 0;

  for (i = 0, end = width, j = 0; j < len && str[j]; j += BLI_str_utf8_size_safe(str + j)) {
    int columns = BLI_str_utf8_char_width_safe(str + j);

    if (i + columns > end) {
      (*offsets)[*lines] = j;
      (*lines)++;

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
static bool console_draw_string(TextViewDrawState *tds,
                                const char *str,
                                int str_len,
                                const unsigned char fg[3],
                                const unsigned char bg[3],
                                const unsigned char bg_sel[4])
{
  int tot_lines; /* Total number of lines for wrapping. */
  int *offsets;  /* Offsets of line beginnings for wrapping. */
  int y_next;

  str_len = console_wrap_offsets(str, str_len, tds->columns, &tot_lines, &offsets);
  y_next = tds->xy[1] + tds->lheight * tot_lines;

  /* Just advance the height. */
  if (tds->do_draw == false) {
    if (tds->mval_pick_offset && tds->mval[1] != INT_MAX && tds->xy[1] <= tds->mval[1]) {
      if (y_next >= tds->mval[1]) {
        int ofs = 0;

        /* Wrap. */
        if (tot_lines > 1) {
          int iofs = (int)((float)(y_next - tds->mval[1]) / tds->lheight);
          ofs += offsets[MIN2(iofs, tot_lines - 1)];
        }

        /* Last part. */
        ofs += BLI_str_utf8_offset_from_column(str + ofs,
                                               (int)floor((float)tds->mval[0] / tds->cwidth));

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
  else if (y_next < tds->scroll_ymin) {
    /* Have not reached the drawable area so don't break. */
    tds->xy[1] = y_next;

    /* Adjust selection even if not drawing. */
    if (tds->sel[0] != tds->sel[1]) {
      console_step_sel(tds, -(str_len + 1));
    }

    MEM_freeN(offsets);
    return true;
  }

  /* Check if we need to wrap lines. */
  if (tot_lines > 1) {
    const int initial_offset = offsets[tot_lines - 1];
    size_t len = str_len - initial_offset;
    const char *s = str + initial_offset;
    int i;

    int sel_orig[2];
    copy_v2_v2_int(sel_orig, tds->sel);

    /* Invert and swap for wrapping. */
    tds->sel[0] = str_len - sel_orig[1];
    tds->sel[1] = str_len - sel_orig[0];

    if (bg) {
      GPUVertFormat *format = immVertexFormat();
      uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

      immUniformColor3ubv(bg);
      immRecti(
          pos, 0, tds->xy[1], tds->draw_rect->xmax, (tds->xy[1] + (tds->lheight * tot_lines)));

      immUnbindProgram();
    }

    /* Last part needs no clipping. */
    BLF_position(tds->font_id, tds->xy[0], tds->lofs + tds->xy[1], 0);
    BLF_color3ubv(tds->font_id, fg);
    BLF_draw_mono(tds->font_id, s, len, tds->cwidth);

    if (tds->sel[0] != tds->sel[1]) {
      console_step_sel(tds, -initial_offset);
      /* BLF_color3ub(tds->font_id, 255, 0, 0); // debug */
      console_draw_sel(s, tds->sel, tds->xy, len, tds->cwidth, tds->lheight, bg_sel);
    }

    tds->xy[1] += tds->lheight;

    for (i = tot_lines - 1; i > 0; i--) {
      len = offsets[i] - offsets[i - 1];
      s = str + offsets[i - 1];

      BLF_position(tds->font_id, tds->xy[0], tds->lofs + tds->xy[1], 0);
      BLF_draw_mono(tds->font_id, s, len, tds->cwidth);

      if (tds->sel[0] != tds->sel[1]) {
        console_step_sel(tds, len);
        /* BLF_color3ub(tds->font_id, 0, 255, 0); // debug */
        console_draw_sel(s, tds->sel, tds->xy, len, tds->cwidth, tds->lheight, bg_sel);
      }

      tds->xy[1] += tds->lheight;

      /* Check if were out of view bounds. */
      if (tds->xy[1] > tds->scroll_ymax) {
        MEM_freeN(offsets);
        return false;
      }
    }

    copy_v2_v2_int(tds->sel, sel_orig);
    console_step_sel(tds, -(str_len + 1));
  }
  else {
    /* Simple, no wrap. */

    if (bg) {
      GPUVertFormat *format = immVertexFormat();
      uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

      immUniformColor3ubv(bg);
      immRecti(pos, 0, tds->xy[1], tds->draw_rect->xmax, tds->xy[1] + tds->lheight);

      immUnbindProgram();
    }

    BLF_color3ubv(tds->font_id, fg);
    BLF_position(tds->font_id, tds->xy[0], tds->lofs + tds->xy[1], 0);
    BLF_draw_mono(tds->font_id, str, str_len, tds->cwidth);

    if (tds->sel[0] != tds->sel[1]) {
      int isel[2];

      isel[0] = str_len - tds->sel[1];
      isel[1] = str_len - tds->sel[0];

      /* BLF_color3ub(tds->font_id, 255, 255, 0); // debug */
      console_draw_sel(str, isel, tds->xy, str_len, tds->cwidth, tds->lheight, bg_sel);
      console_step_sel(tds, -(str_len + 1));
    }

    tds->xy[1] += tds->lheight;

    if (tds->xy[1] > tds->scroll_ymax) {
      MEM_freeN(offsets);
      return false;
    }
  }

  MEM_freeN(offsets);
  return true;
}

/**
 * \param r_mval_pick_item: The resulting item clicked on using \a mval_init.
 * Set from the void pointer which holds the current iterator.
 * It's type depends on the data being iterated over.
 * \param r_mval_pick_offset: The offset in bytes of the \a mval_init.
 * Use for selection.
 */
int textview_draw(TextViewContext *tvc,
                  const bool do_draw,
                  const int mval_init[2],
                  void **r_mval_pick_item,
                  int *r_mval_pick_offset)
{
  TextViewDrawState tds = {0};

  int x_orig = tvc->draw_rect.xmin, y_orig = tvc->draw_rect.ymin + tvc->lheight / 6;
  int xy[2];
  /* Disable selection by. */
  int sel[2] = {-1, -1};
  unsigned char fg[3], bg[3];
  const int font_id = blf_mono_font;

  console_font_begin(font_id, tvc->lheight);

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

  if (r_mval_pick_offset != NULL) {
    *r_mval_pick_offset = 0;
  }

  /* Constants for the text-view context. */
  tds.font_id = font_id;
  tds.cwidth = (int)BLF_fixed_width(font_id);
  BLI_assert(tds.cwidth > 0);
  tds.lheight = tvc->lheight;
  tds.lofs = -BLF_descender(font_id);
  /* Note, scroll bar must be already subtracted. */
  tds.columns = (tvc->draw_rect.xmax - tvc->draw_rect.xmin) / tds.cwidth;
  /* Avoid divide by zero on small windows. */
  if (tds.columns < 1) {
    tds.columns = 1;
  }
  tds.draw_rect = &tvc->draw_rect;
  tds.scroll_ymin = tvc->scroll_ymin;
  tds.scroll_ymax = tvc->scroll_ymax;
  tds.xy = xy;
  tds.sel = sel;
  tds.mval_pick_offset = r_mval_pick_offset;
  tds.mval = mval;
  tds.do_draw = do_draw;

  /* Shouldnt be needed. */
  tvc->cwidth = tds.cwidth;
  tvc->columns = tds.columns;
  tvc->iter_index = 0;

  if (tvc->sel_start != tvc->sel_end) {
    sel[0] = tvc->sel_start;
    sel[1] = tvc->sel_end;
  }

  if (tvc->begin(tvc)) {
    unsigned char bg_sel[4] = {0};

    if (do_draw && tvc->const_colors) {
      tvc->const_colors(tvc, bg_sel);
    }

    do {
      const char *ext_line;
      int ext_len;
      int color_flag = 0;

      const int y_prev = xy[1];

      if (do_draw) {
        color_flag = tvc->line_color(tvc, fg, bg);
      }

      tvc->line_get(tvc, &ext_line, &ext_len);

      if (!console_draw_string(&tds,
                               ext_line,
                               ext_len,
                               (color_flag & TVC_LINE_FG) ? fg : NULL,
                               (color_flag & TVC_LINE_BG) ? bg : NULL,
                               bg_sel)) {
        /* When drawing, if we pass v2d->cur.ymax, then quit. */
        if (do_draw) {
          /* Past the y limits. */
          break;
        }
      }

      if ((mval[1] != INT_MAX) && (mval[1] >= y_prev && mval[1] <= xy[1])) {
        *r_mval_pick_item = (void *)tvc->iter;
        break;
      }

      tvc->iter_index++;

    } while (tvc->step(tvc));
  }

  tvc->end(tvc);

  xy[1] += tvc->lheight * 2;

  return xy[1] - y_orig;
}
