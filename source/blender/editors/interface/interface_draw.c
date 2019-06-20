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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 */

#include <math.h>
#include <string.h>

#include "DNA_color_types.h"
#include "DNA_screen_types.h"
#include "DNA_movieclip_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_node.h"
#include "BKE_tracking.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "BIF_glutil.h"

#include "BLF_api.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "UI_interface.h"

/* own include */
#include "interface_intern.h"

static int roundboxtype = UI_CNR_ALL;

void UI_draw_roundbox_corner_set(int type)
{
  /* Not sure the roundbox function is the best place to change this
   * if this is undone, it's not that big a deal, only makes curves edges
   * square for the  */
  roundboxtype = type;
}

#if 0 /* unused */
int UI_draw_roundbox_corner_get(void)
{
  return roundboxtype;
}
#endif

void UI_draw_roundbox_3ubAlpha(bool filled,
                               float minx,
                               float miny,
                               float maxx,
                               float maxy,
                               float rad,
                               const uchar col[3],
                               uchar alpha)
{
  float colv[4];
  colv[0] = ((float)col[0]) / 255;
  colv[1] = ((float)col[1]) / 255;
  colv[2] = ((float)col[2]) / 255;
  colv[3] = ((float)alpha) / 255;
  UI_draw_roundbox_4fv(filled, minx, miny, maxx, maxy, rad, colv);
}

void UI_draw_roundbox_3fvAlpha(bool filled,
                               float minx,
                               float miny,
                               float maxx,
                               float maxy,
                               float rad,
                               const float col[3],
                               float alpha)
{
  float colv[4];
  colv[0] = col[0];
  colv[1] = col[1];
  colv[2] = col[2];
  colv[3] = alpha;
  UI_draw_roundbox_4fv(filled, minx, miny, maxx, maxy, rad, colv);
}

void UI_draw_roundbox_aa(
    bool filled, float minx, float miny, float maxx, float maxy, float rad, const float color[4])
{
  uiWidgetBaseParameters widget_params = {
      .recti.xmin = minx,
      .recti.ymin = miny,
      .recti.xmax = maxx,
      .recti.ymax = maxy,
      .radi = rad,
      .round_corners[0] = (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 1.0f : 0.0f,
      .round_corners[1] = (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 1.0f : 0.0f,
      .round_corners[2] = (roundboxtype & UI_CNR_TOP_RIGHT) ? 1.0f : 0.0f,
      .round_corners[3] = (roundboxtype & UI_CNR_TOP_LEFT) ? 1.0f : 0.0f,
      .color_inner1[0] = color[0],
      .color_inner2[0] = color[0],
      .color_inner1[1] = color[1],
      .color_inner2[1] = color[1],
      .color_inner1[2] = color[2],
      .color_inner2[2] = color[2],
      .color_inner1[3] = color[3],
      .color_inner2[3] = color[3],
      .alpha_discard = 1.0f,
  };

  GPU_blend(true);

  if (filled) {
    /* plain antialiased filled box */
    widget_params.color_inner1[3] *= 0.125f;
    widget_params.color_inner2[3] *= 0.125f;

    /* WATCH: This is assuming the ModelViewProjectionMatrix is area pixel space.
     * If it has been scaled, then it's no longer valid. */
    GPUBatch *batch = ui_batch_roundbox_get(filled, true);
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_BASE);
    GPU_batch_uniform_4fv_array(batch, "parameters", 11, (float *)&widget_params);
    GPU_batch_draw(batch);
  }
  else {
    /* plain antialiased unfilled box */
    GPU_line_smooth(true);

    GPUBatch *batch = ui_batch_roundbox_get(filled, false);
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_BASE);
    GPU_batch_uniform_4fv_array(batch, "parameters", 11, (float *)&widget_params);
    GPU_batch_draw(batch);

    GPU_line_smooth(false);
  }

  GPU_blend(false);
}

void UI_draw_roundbox_4fv(
    bool filled, float minx, float miny, float maxx, float maxy, float rad, const float col[4])
{
#if 0
  float vec[7][2] = {
      {0.195, 0.02},
      {0.383, 0.067},
      {0.55, 0.169},
      {0.707, 0.293},
      {0.831, 0.45},
      {0.924, 0.617},
      {0.98, 0.805},
  };
  int a;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  /* mult */
  for (a = 0; a < 7; a++) {
    mul_v2_fl(vec[a], rad);
  }

  uint vert_len = 0;
  vert_len += (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 9 : 1;
  vert_len += (roundboxtype & UI_CNR_TOP_RIGHT) ? 9 : 1;
  vert_len += (roundboxtype & UI_CNR_TOP_LEFT) ? 9 : 1;
  vert_len += (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 9 : 1;

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4fv(col);

  immBegin(filled ? GPU_PRIM_TRI_FAN : GPU_PRIM_LINE_LOOP, vert_len);
  /* start with corner right-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
    immVertex2f(pos, maxx - rad, miny);
    for (a = 0; a < 7; a++) {
      immVertex2f(pos, maxx - rad + vec[a][0], miny + vec[a][1]);
    }
    immVertex2f(pos, maxx, miny + rad);
  }
  else {
    immVertex2f(pos, maxx, miny);
  }

  /* corner right-top */
  if (roundboxtype & UI_CNR_TOP_RIGHT) {
    immVertex2f(pos, maxx, maxy - rad);
    for (a = 0; a < 7; a++) {
      immVertex2f(pos, maxx - vec[a][1], maxy - rad + vec[a][0]);
    }
    immVertex2f(pos, maxx - rad, maxy);
  }
  else {
    immVertex2f(pos, maxx, maxy);
  }

  /* corner left-top */
  if (roundboxtype & UI_CNR_TOP_LEFT) {
    immVertex2f(pos, minx + rad, maxy);
    for (a = 0; a < 7; a++) {
      immVertex2f(pos, minx + rad - vec[a][0], maxy - vec[a][1]);
    }
    immVertex2f(pos, minx, maxy - rad);
  }
  else {
    immVertex2f(pos, minx, maxy);
  }

  /* corner left-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
    immVertex2f(pos, minx, miny + rad);
    for (a = 0; a < 7; a++) {
      immVertex2f(pos, minx + vec[a][1], miny + rad - vec[a][0]);
    }
    immVertex2f(pos, minx + rad, miny);
  }
  else {
    immVertex2f(pos, minx, miny);
  }

  immEnd();
  immUnbindProgram();
#endif

  uiWidgetBaseParameters widget_params = {
      .recti.xmin = minx,
      .recti.ymin = miny,
      .recti.xmax = maxx,
      .recti.ymax = maxy,
      .radi = rad,
      .round_corners[0] = (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 1.0f : 0.0f,
      .round_corners[1] = (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 1.0f : 0.0f,
      .round_corners[2] = (roundboxtype & UI_CNR_TOP_RIGHT) ? 1.0f : 0.0f,
      .round_corners[3] = (roundboxtype & UI_CNR_TOP_LEFT) ? 1.0f : 0.0f,
      .color_inner1[0] = col[0],
      .color_inner2[0] = col[0],
      .color_inner1[1] = col[1],
      .color_inner2[1] = col[1],
      .color_inner1[2] = col[2],
      .color_inner2[2] = col[2],
      .color_inner1[3] = col[3],
      .color_inner2[3] = col[3],
      .alpha_discard = 1.0f,
  };

  GPUBatch *batch = ui_batch_roundbox_get(filled, false);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_BASE);
  GPU_batch_uniform_4fv_array(batch, "parameters", 11, (float *)&widget_params);
  GPU_batch_draw(batch);
}

#if 0
static void round_box_shade_col(uint attr,
                                const float col1[3],
                                float const col2[3],
                                const float fac)
{
  float col[4] = {
      fac * col1[0] + (1.0f - fac) * col2[0],
      fac * col1[1] + (1.0f - fac) * col2[1],
      fac * col1[2] + (1.0f - fac) * col2[2],
      1.0f,
  };
  immAttr4fv(attr, col);
}
#endif

/* linear horizontal shade within button or in outline */
/* view2d scrollers use it */
void UI_draw_roundbox_shade_x(bool filled,
                              float minx,
                              float miny,
                              float maxx,
                              float maxy,
                              float rad,
                              float shadetop,
                              float shadedown,
                              const float col[4])
{
#if 0
  float vec[7][2] = {
      {0.195, 0.02},
      {0.383, 0.067},
      {0.55, 0.169},
      {0.707, 0.293},
      {0.831, 0.45},
      {0.924, 0.617},
      {0.98, 0.805},
  };
  const float div = maxy - miny;
  const float idiv = 1.0f / div;
  float coltop[3], coldown[3];
  int vert_count = 0;
  int a;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

  /* mult */
  for (a = 0; a < 7; a++) {
    mul_v2_fl(vec[a], rad);
  }

  /* 'shade' defines strength of shading */
  coltop[0] = min_ff(1.0f, col[0] + shadetop);
  coltop[1] = min_ff(1.0f, col[1] + shadetop);
  coltop[2] = min_ff(1.0f, col[2] + shadetop);
  coldown[0] = max_ff(0.0f, col[0] + shadedown);
  coldown[1] = max_ff(0.0f, col[1] + shadedown);
  coldown[2] = max_ff(0.0f, col[2] + shadedown);

  vert_count += (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 9 : 1;
  vert_count += (roundboxtype & UI_CNR_TOP_RIGHT) ? 9 : 1;
  vert_count += (roundboxtype & UI_CNR_TOP_LEFT) ? 9 : 1;
  vert_count += (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 9 : 1;

  immBegin(filled ? GPU_PRIM_TRI_FAN : GPU_PRIM_LINE_LOOP, vert_count);

  /* start with corner right-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {

    round_box_shade_col(color, coltop, coldown, 0.0);
    immVertex2f(pos, maxx - rad, miny);

    for (a = 0; a < 7; a++) {
      round_box_shade_col(color, coltop, coldown, vec[a][1] * idiv);
      immVertex2f(pos, maxx - rad + vec[a][0], miny + vec[a][1]);
    }

    round_box_shade_col(color, coltop, coldown, rad * idiv);
    immVertex2f(pos, maxx, miny + rad);
  }
  else {
    round_box_shade_col(color, coltop, coldown, 0.0);
    immVertex2f(pos, maxx, miny);
  }

  /* corner right-top */
  if (roundboxtype & UI_CNR_TOP_RIGHT) {

    round_box_shade_col(color, coltop, coldown, (div - rad) * idiv);
    immVertex2f(pos, maxx, maxy - rad);

    for (a = 0; a < 7; a++) {
      round_box_shade_col(color, coltop, coldown, (div - rad + vec[a][1]) * idiv);
      immVertex2f(pos, maxx - vec[a][1], maxy - rad + vec[a][0]);
    }
    round_box_shade_col(color, coltop, coldown, 1.0);
    immVertex2f(pos, maxx - rad, maxy);
  }
  else {
    round_box_shade_col(color, coltop, coldown, 1.0);
    immVertex2f(pos, maxx, maxy);
  }

  /* corner left-top */
  if (roundboxtype & UI_CNR_TOP_LEFT) {

    round_box_shade_col(color, coltop, coldown, 1.0);
    immVertex2f(pos, minx + rad, maxy);

    for (a = 0; a < 7; a++) {
      round_box_shade_col(color, coltop, coldown, (div - vec[a][1]) * idiv);
      immVertex2f(pos, minx + rad - vec[a][0], maxy - vec[a][1]);
    }

    round_box_shade_col(color, coltop, coldown, (div - rad) * idiv);
    immVertex2f(pos, minx, maxy - rad);
  }
  else {
    round_box_shade_col(color, coltop, coldown, 1.0);
    immVertex2f(pos, minx, maxy);
  }

  /* corner left-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_LEFT) {

    round_box_shade_col(color, coltop, coldown, rad * idiv);
    immVertex2f(pos, minx, miny + rad);

    for (a = 0; a < 7; a++) {
      round_box_shade_col(color, coltop, coldown, (rad - vec[a][1]) * idiv);
      immVertex2f(pos, minx + vec[a][1], miny + rad - vec[a][0]);
    }

    round_box_shade_col(color, coltop, coldown, 0.0);
    immVertex2f(pos, minx + rad, miny);
  }
  else {
    round_box_shade_col(color, coltop, coldown, 0.0);
    immVertex2f(pos, minx, miny);
  }

  immEnd();
  immUnbindProgram();
#endif

  uiWidgetBaseParameters widget_params = {
      .recti.xmin = minx,
      .recti.ymin = miny,
      .recti.xmax = maxx,
      .recti.ymax = maxy,
      .radi = rad,
      .round_corners[0] = (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 1.0f : 0.0f,
      .round_corners[1] = (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 1.0f : 0.0f,
      .round_corners[2] = (roundboxtype & UI_CNR_TOP_RIGHT) ? 1.0f : 0.0f,
      .round_corners[3] = (roundboxtype & UI_CNR_TOP_LEFT) ? 1.0f : 0.0f,
      .color_inner1[0] = min_ff(1.0f, col[0] + shadetop),
      .color_inner2[0] = max_ff(0.0f, col[0] + shadedown),
      .color_inner1[1] = min_ff(1.0f, col[1] + shadetop),
      .color_inner2[1] = max_ff(0.0f, col[1] + shadedown),
      .color_inner1[2] = min_ff(1.0f, col[2] + shadetop),
      .color_inner2[2] = max_ff(0.0f, col[2] + shadedown),
      .color_inner1[3] = 1.0f,
      .color_inner2[3] = 1.0f,
      .alpha_discard = 1.0f,
  };

  GPUBatch *batch = ui_batch_roundbox_get(filled, false);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_BASE);
  GPU_batch_uniform_4fv_array(batch, "parameters", 11, (float *)&widget_params);
  GPU_batch_draw(batch);
}

#if 0  /* unused */
/* linear vertical shade within button or in outline */
/* view2d scrollers use it */
void UI_draw_roundbox_shade_y(bool filled,
                              float minx,
                              float miny,
                              float maxx,
                              float maxy,
                              float rad,
                              float shadeleft,
                              float shaderight,
                              const float col[4])
{
  float vec[7][2] = {
      {0.195, 0.02},
      {0.383, 0.067},
      {0.55, 0.169},
      {0.707, 0.293},
      {0.831, 0.45},
      {0.924, 0.617},
      {0.98, 0.805},
  };
  const float div = maxx - minx;
  const float idiv = 1.0f / div;
  float colLeft[3], colRight[3];
  int vert_count = 0;
  int a;

  /* mult */
  for (a = 0; a < 7; a++) {
    mul_v2_fl(vec[a], rad);
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

  /* 'shade' defines strength of shading */
  colLeft[0] = min_ff(1.0f, col[0] + shadeleft);
  colLeft[1] = min_ff(1.0f, col[1] + shadeleft);
  colLeft[2] = min_ff(1.0f, col[2] + shadeleft);
  colRight[0] = max_ff(0.0f, col[0] + shaderight);
  colRight[1] = max_ff(0.0f, col[1] + shaderight);
  colRight[2] = max_ff(0.0f, col[2] + shaderight);

  vert_count += (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 9 : 1;
  vert_count += (roundboxtype & UI_CNR_TOP_RIGHT) ? 9 : 1;
  vert_count += (roundboxtype & UI_CNR_TOP_LEFT) ? 9 : 1;
  vert_count += (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 9 : 1;

  immBegin(filled ? GPU_PRIM_TRI_FAN : GPU_PRIM_LINE_LOOP, vert_count);

  /* start with corner right-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
    round_box_shade_col(color, colLeft, colRight, 0.0);
    immVertex2f(pos, maxx - rad, miny);

    for (a = 0; a < 7; a++) {
      round_box_shade_col(color, colLeft, colRight, vec[a][0] * idiv);
      immVertex2f(pos, maxx - rad + vec[a][0], miny + vec[a][1]);
    }

    round_box_shade_col(color, colLeft, colRight, rad * idiv);
    immVertex2f(pos, maxx, miny + rad);
  }
  else {
    round_box_shade_col(color, colLeft, colRight, 0.0);
    immVertex2f(pos, maxx, miny);
  }

  /* corner right-top */
  if (roundboxtype & UI_CNR_TOP_RIGHT) {
    round_box_shade_col(color, colLeft, colRight, 0.0);
    immVertex2f(pos, maxx, maxy - rad);

    for (a = 0; a < 7; a++) {

      round_box_shade_col(color, colLeft, colRight, (div - rad - vec[a][0]) * idiv);
      immVertex2f(pos, maxx - vec[a][1], maxy - rad + vec[a][0]);
    }
    round_box_shade_col(color, colLeft, colRight, (div - rad) * idiv);
    immVertex2f(pos, maxx - rad, maxy);
  }
  else {
    round_box_shade_col(color, colLeft, colRight, 0.0);
    immVertex2f(pos, maxx, maxy);
  }

  /* corner left-top */
  if (roundboxtype & UI_CNR_TOP_LEFT) {
    round_box_shade_col(color, colLeft, colRight, (div - rad) * idiv);
    immVertex2f(pos, minx + rad, maxy);

    for (a = 0; a < 7; a++) {
      round_box_shade_col(color, colLeft, colRight, (div - rad + vec[a][0]) * idiv);
      immVertex2f(pos, minx + rad - vec[a][0], maxy - vec[a][1]);
    }

    round_box_shade_col(color, colLeft, colRight, 1.0);
    immVertex2f(pos, minx, maxy - rad);
  }
  else {
    round_box_shade_col(color, colLeft, colRight, 1.0);
    immVertex2f(pos, minx, maxy);
  }

  /* corner left-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
    round_box_shade_col(color, colLeft, colRight, 1.0);
    immVertex2f(pos, minx, miny + rad);

    for (a = 0; a < 7; a++) {
      round_box_shade_col(color, colLeft, colRight, (vec[a][0]) * idiv);
      immVertex2f(pos, minx + vec[a][1], miny + rad - vec[a][0]);
    }

    round_box_shade_col(color, colLeft, colRight, 1.0);
    immVertex2f(pos, minx + rad, miny);
  }
  else {
    round_box_shade_col(color, colLeft, colRight, 1.0);
    immVertex2f(pos, minx, miny);
  }

  immEnd();
  immUnbindProgram();
}
#endif /* unused */

void UI_draw_text_underline(int pos_x, int pos_y, int len, int height, const float color[4])
{
  int ofs_y = 4 * U.pixelsize;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4fv(color);

  immRecti(pos, pos_x, pos_y - ofs_y, pos_x + len, pos_y - ofs_y + (height * U.pixelsize));
  immUnbindProgram();
}

/* ************** SPECIAL BUTTON DRAWING FUNCTIONS ************* */

/* based on UI_draw_roundbox_gl_mode,
 * check on making a version which allows us to skip some sides */
void ui_draw_but_TAB_outline(const rcti *rect,
                             float rad,
                             uchar highlight[3],
                             uchar highlight_fade[3])
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
  /* add a 1px offset, looks nicer */
  const int minx = rect->xmin + U.pixelsize, maxx = rect->xmax - U.pixelsize;
  const int miny = rect->ymin + U.pixelsize, maxy = rect->ymax - U.pixelsize;
  int a;
  float vec[4][2] = {
      {0.195, 0.02},
      {0.55, 0.169},
      {0.831, 0.45},
      {0.98, 0.805},
  };

  /* mult */
  for (a = 0; a < 4; a++) {
    mul_v2_fl(vec[a], rad);
  }

  immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);
  immBeginAtMost(GPU_PRIM_LINE_STRIP, 25);

  immAttr3ubv(col, highlight);

  /* start with corner left-top */
  if (roundboxtype & UI_CNR_TOP_LEFT) {
    immVertex2f(pos, minx, maxy - rad);
    for (a = 0; a < 4; a++) {
      immVertex2f(pos, minx + vec[a][1], maxy - rad + vec[a][0]);
    }
    immVertex2f(pos, minx + rad, maxy);
  }
  else {
    immVertex2f(pos, minx, maxy);
  }

  /* corner right-top */
  if (roundboxtype & UI_CNR_TOP_RIGHT) {
    immVertex2f(pos, maxx - rad, maxy);
    for (a = 0; a < 4; a++) {
      immVertex2f(pos, maxx - rad + vec[a][0], maxy - vec[a][1]);
    }
    immVertex2f(pos, maxx, maxy - rad);
  }
  else {
    immVertex2f(pos, maxx, maxy);
  }

  immAttr3ubv(col, highlight_fade);

  /* corner right-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
    immVertex2f(pos, maxx, miny + rad);
    for (a = 0; a < 4; a++) {
      immVertex2f(pos, maxx - vec[a][1], miny + rad - vec[a][0]);
    }
    immVertex2f(pos, maxx - rad, miny);
  }
  else {
    immVertex2f(pos, maxx, miny);
  }

  /* corner left-bottom */
  if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
    immVertex2f(pos, minx + rad, miny);
    for (a = 0; a < 4; a++) {
      immVertex2f(pos, minx + rad - vec[a][0], miny + vec[a][1]);
    }
    immVertex2f(pos, minx, miny + rad);
  }
  else {
    immVertex2f(pos, minx, miny);
  }

  immAttr3ubv(col, highlight);

  /* back to corner left-top */
  immVertex2f(pos, minx, (roundboxtype & UI_CNR_TOP_LEFT) ? (maxy - rad) : maxy);

  immEnd();
  immUnbindProgram();
}

void ui_draw_but_IMAGE(ARegion *UNUSED(ar),
                       uiBut *but,
                       const uiWidgetColors *UNUSED(wcol),
                       const rcti *rect)
{
#ifdef WITH_HEADLESS
  (void)rect;
  (void)but;
#else
  ImBuf *ibuf = (ImBuf *)but->poin;

  if (!ibuf) {
    return;
  }

  float facx = 1.0f;
  float facy = 1.0f;

  int w = BLI_rcti_size_x(rect);
  int h = BLI_rcti_size_y(rect);

  /* scissor doesn't seem to be doing the right thing...? */
#  if 0
  /* prevent drawing outside widget area */
  int scissor[4];
  GPU_scissor_get_i(scissor);
  GPU_scissor(rect->xmin, rect->ymin, w, h);
#  endif

  GPU_blend(true);

  if (w != ibuf->x || h != ibuf->y) {
    facx = (float)w / (float)ibuf->x;
    facy = (float)h / (float)ibuf->y;
  }

  IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_COLOR);
  immDrawPixelsTex(&state,
                   (float)rect->xmin,
                   (float)rect->ymin,
                   ibuf->x,
                   ibuf->y,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   GL_NEAREST,
                   ibuf->rect,
                   facx,
                   facy,
                   NULL);

  GPU_blend(false);

#  if 0
  // restore scissortest
  GPU_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);
#  endif

#endif
}

/**
 * Draw title and text safe areas.
 *
 * \Note This functionn is to be used with the 2D dashed shader enabled.
 *
 * \param pos: is a PRIM_FLOAT, 2, GPU_FETCH_FLOAT vertex attribute.
 * \param line_origin: is a PRIM_FLOAT, 2, GPU_FETCH_FLOAT vertex attribute.
 *
 * The next 4 parameters are the offsets for the view, not the zones.
 */
void UI_draw_safe_areas(uint pos,
                        float x1,
                        float x2,
                        float y1,
                        float y2,
                        const float title_aspect[2],
                        const float action_aspect[2])
{
  const float size_x_half = (x2 - x1) * 0.5f;
  const float size_y_half = (y2 - y1) * 0.5f;

  const float *safe_areas[] = {title_aspect, action_aspect};
  const int safe_len = ARRAY_SIZE(safe_areas);

  for (int i = 0; i < safe_len; i++) {
    if (safe_areas[i][0] || safe_areas[i][1]) {
      float margin_x = safe_areas[i][0] * size_x_half;
      float margin_y = safe_areas[i][1] * size_y_half;

      float minx = x1 + margin_x;
      float miny = y1 + margin_y;
      float maxx = x2 - margin_x;
      float maxy = y2 - margin_y;

      imm_draw_box_wire_2d(pos, minx, miny, maxx, maxy);
    }
  }
}

static void draw_scope_end(const rctf *rect, GLint *scissor)
{
  /* restore scissortest */
  GPU_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);

  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  /* outline */
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  float color[4] = {0.0f, 0.0f, 0.0f, 0.5f};
  UI_draw_roundbox_4fv(
      false, rect->xmin - 1, rect->ymin, rect->xmax + 1, rect->ymax + 1, 3.0f, color);
}

static void histogram_draw_one(float r,
                               float g,
                               float b,
                               float alpha,
                               float x,
                               float y,
                               float w,
                               float h,
                               const float *data,
                               int res,
                               const bool is_line,
                               uint pos_attr)
{
  float color[4] = {r, g, b, alpha};

  /* that can happen */
  if (res == 0) {
    return;
  }

  GPU_line_smooth(true);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);

  immUniformColor4fv(color);

  if (is_line) {
    /* curve outline */
    GPU_line_width(1.5);

    immBegin(GPU_PRIM_LINE_STRIP, res);
    for (int i = 0; i < res; i++) {
      float x2 = x + i * (w / (float)res);
      immVertex2f(pos_attr, x2, y + (data[i] * h));
    }
    immEnd();
  }
  else {
    /* under the curve */
    immBegin(GPU_PRIM_TRI_STRIP, res * 2);
    immVertex2f(pos_attr, x, y);
    immVertex2f(pos_attr, x, y + (data[0] * h));
    for (int i = 1; i < res; i++) {
      float x2 = x + i * (w / (float)res);
      immVertex2f(pos_attr, x2, y + (data[i] * h));
      immVertex2f(pos_attr, x2, y);
    }
    immEnd();

    /* curve outline */
    immUniformColor4f(0.0f, 0.0f, 0.0f, 0.25f);

    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    immBegin(GPU_PRIM_LINE_STRIP, res);
    for (int i = 0; i < res; i++) {
      float x2 = x + i * (w / (float)res);
      immVertex2f(pos_attr, x2, y + (data[i] * h));
    }
    immEnd();
  }

  GPU_line_smooth(false);
}

#define HISTOGRAM_TOT_GRID_LINES 4

void ui_draw_but_HISTOGRAM(ARegion *UNUSED(ar),
                           uiBut *but,
                           const uiWidgetColors *UNUSED(wcol),
                           const rcti *recti)
{
  Histogram *hist = (Histogram *)but->poin;
  int res = hist->x_resolution;
  const bool is_line = (hist->flag & HISTO_FLAG_LINE) != 0;

  rctf rect = {
      .xmin = (float)recti->xmin + 1,
      .xmax = (float)recti->xmax - 1,
      .ymin = (float)recti->ymin + 1,
      .ymax = (float)recti->ymax - 1,
  };

  float w = BLI_rctf_size_x(&rect);
  float h = BLI_rctf_size_y(&rect) * hist->ymax;

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  float color[4];
  UI_GetThemeColor4fv(TH_PREVIEW_BACK, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv(
      true, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f, color);

  /* need scissor test, histogram can draw outside of boundary */
  int scissor[4];
  GPU_scissor_get_i(scissor);
  GPU_scissor((rect.xmin - 1),
              (rect.ymin - 1),
              (rect.xmax + 1) - (rect.xmin - 1),
              (rect.ymax + 1) - (rect.ymin - 1));

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.08f);
  /* draw grid lines here */
  for (int i = 1; i <= HISTOGRAM_TOT_GRID_LINES; i++) {
    const float fac = (float)i / (float)HISTOGRAM_TOT_GRID_LINES;

    /* so we can tell the 1.0 color point */
    if (i == HISTOGRAM_TOT_GRID_LINES) {
      immUniformColor4f(1.0f, 1.0f, 1.0f, 0.5f);
    }

    immBegin(GPU_PRIM_LINES, 4);

    immVertex2f(pos, rect.xmin, rect.ymin + fac * h);
    immVertex2f(pos, rect.xmax, rect.ymin + fac * h);

    immVertex2f(pos, rect.xmin + fac * w, rect.ymin);
    immVertex2f(pos, rect.xmin + fac * w, rect.ymax);

    immEnd();
  }

  if (hist->mode == HISTO_MODE_LUMA) {
    histogram_draw_one(
        1.0, 1.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_luma, res, is_line, pos);
  }
  else if (hist->mode == HISTO_MODE_ALPHA) {
    histogram_draw_one(
        1.0, 1.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_a, res, is_line, pos);
  }
  else {
    if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_R) {
      histogram_draw_one(
          1.0, 0.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_r, res, is_line, pos);
    }
    if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_G) {
      histogram_draw_one(
          0.0, 1.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_g, res, is_line, pos);
    }
    if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_B) {
      histogram_draw_one(
          0.0, 0.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_b, res, is_line, pos);
    }
  }

  immUnbindProgram();

  /* outline */
  draw_scope_end(&rect, scissor);
}

#undef HISTOGRAM_TOT_GRID_LINES

static void waveform_draw_one(float *waveform, int nbr, const float col[3])
{
  GPUVertFormat format = {0};
  uint pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, nbr);

  GPU_vertbuf_attr_fill(vbo, pos_id, waveform);

  /* TODO store the GPUBatch inside the scope */
  GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_UNIFORM_COLOR);
  GPU_batch_uniform_4f(batch, "color", col[0], col[1], col[2], 1.0f);
  GPU_batch_draw(batch);

  GPU_batch_discard(batch);
}

void ui_draw_but_WAVEFORM(ARegion *UNUSED(ar),
                          uiBut *but,
                          const uiWidgetColors *UNUSED(wcol),
                          const rcti *recti)
{
  Scopes *scopes = (Scopes *)but->poin;
  int scissor[4];
  float colors[3][3];
  float colorsycc[3][3] = {{1, 0, 1}, {1, 1, 0}, {0, 1, 1}};
  /* colors  pre multiplied by alpha for speed up */
  float colors_alpha[3][3], colorsycc_alpha[3][3];
  float min, max;

  if (scopes == NULL) {
    return;
  }

  rctf rect = {
      .xmin = (float)recti->xmin + 1,
      .xmax = (float)recti->xmax - 1,
      .ymin = (float)recti->ymin + 1,
      .ymax = (float)recti->ymax - 1,
  };

  if (scopes->wavefrm_yfac < 0.5f) {
    scopes->wavefrm_yfac = 0.98f;
  }
  float w = BLI_rctf_size_x(&rect) - 7;
  float h = BLI_rctf_size_y(&rect) * scopes->wavefrm_yfac;
  float yofs = rect.ymin + (BLI_rctf_size_y(&rect) - h) * 0.5f;
  float w3 = w / 3.0f;

  /* log scale for alpha */
  float alpha = scopes->wavefrm_alpha * scopes->wavefrm_alpha;

  unit_m3(colors);

  for (int c = 0; c < 3; c++) {
    for (int i = 0; i < 3; i++) {
      colors_alpha[c][i] = colors[c][i] * alpha;
      colorsycc_alpha[c][i] = colorsycc[c][i] * alpha;
    }
  }

  /* Flush text cache before changing scissors. */
  BLF_batch_draw_flush();

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  float color[4];
  UI_GetThemeColor4fv(TH_PREVIEW_BACK, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv(
      true, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f, color);

  /* need scissor test, waveform can draw outside of boundary */
  GPU_scissor_get_i(scissor);
  GPU_scissor((rect.xmin - 1),
              (rect.ymin - 1),
              (rect.xmax + 1) - (rect.xmin - 1),
              (rect.ymax + 1) - (rect.ymin - 1));

  /* draw scale numbers first before binding any shader */
  for (int i = 0; i < 6; i++) {
    char str[4];
    BLI_snprintf(str, sizeof(str), "%-3d", i * 20);
    str[3] = '\0';
    BLF_color4f(BLF_default(), 1.0f, 1.0f, 1.0f, 0.08f);
    BLF_draw_default(rect.xmin + 1, yofs - 5 + (i * 0.2f) * h, 0, str, sizeof(str) - 1);
  }

  /* Flush text cache before drawing things on top. */
  BLF_batch_draw_flush();

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.08f);

  /* draw grid lines here */
  immBegin(GPU_PRIM_LINES, 12);

  for (int i = 0; i < 6; i++) {
    immVertex2f(pos, rect.xmin + 22, yofs + (i * 0.2f) * h);
    immVertex2f(pos, rect.xmax + 1, yofs + (i * 0.2f) * h);
  }

  immEnd();

  /* 3 vertical separation */
  if (scopes->wavefrm_mode != SCOPES_WAVEFRM_LUMA) {
    immBegin(GPU_PRIM_LINES, 4);

    for (int i = 1; i < 3; i++) {
      immVertex2f(pos, rect.xmin + i * w3, rect.ymin);
      immVertex2f(pos, rect.xmin + i * w3, rect.ymax);
    }

    immEnd();
  }

  /* separate min max zone on the right */
  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos, rect.xmin + w, rect.ymin);
  immVertex2f(pos, rect.xmin + w, rect.ymax);
  immEnd();

  /* 16-235-240 level in case of ITU-R BT601/709 */
  immUniformColor4f(1.0f, 0.4f, 0.0f, 0.2f);
  if (ELEM(scopes->wavefrm_mode, SCOPES_WAVEFRM_YCC_601, SCOPES_WAVEFRM_YCC_709)) {
    immBegin(GPU_PRIM_LINES, 8);

    immVertex2f(pos, rect.xmin + 22, yofs + h * 16.0f / 255.0f);
    immVertex2f(pos, rect.xmax + 1, yofs + h * 16.0f / 255.0f);

    immVertex2f(pos, rect.xmin + 22, yofs + h * 235.0f / 255.0f);
    immVertex2f(pos, rect.xmin + w3, yofs + h * 235.0f / 255.0f);

    immVertex2f(pos, rect.xmin + 3 * w3, yofs + h * 235.0f / 255.0f);
    immVertex2f(pos, rect.xmax + 1, yofs + h * 235.0f / 255.0f);

    immVertex2f(pos, rect.xmin + w3, yofs + h * 240.0f / 255.0f);
    immVertex2f(pos, rect.xmax + 1, yofs + h * 240.0f / 255.0f);

    immEnd();
  }
  /* 7.5 IRE black point level for NTSC */
  if (scopes->wavefrm_mode == SCOPES_WAVEFRM_LUMA) {
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, rect.xmin, yofs + h * 0.075f);
    immVertex2f(pos, rect.xmax + 1, yofs + h * 0.075f);
    immEnd();
  }

  if (scopes->ok && scopes->waveform_1 != NULL) {
    glBlendFunc(GL_ONE, GL_ONE);
    GPU_point_size(1.0);

    /* LUMA (1 channel) */
    if (scopes->wavefrm_mode == SCOPES_WAVEFRM_LUMA) {
      float col[3] = {alpha, alpha, alpha};

      GPU_matrix_push();
      GPU_matrix_translate_2f(rect.xmin, yofs);
      GPU_matrix_scale_2f(w, h);

      waveform_draw_one(scopes->waveform_1, scopes->waveform_tot, col);

      GPU_matrix_pop();

      /* min max */
      immUniformColor3f(0.5f, 0.5f, 0.5f);
      min = yofs + scopes->minmax[0][0] * h;
      max = yofs + scopes->minmax[0][1] * h;
      CLAMP(min, rect.ymin, rect.ymax);
      CLAMP(max, rect.ymin, rect.ymax);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, rect.xmax - 3, min);
      immVertex2f(pos, rect.xmax - 3, max);
      immEnd();
    }
    /* RGB (3 channel) */
    else if (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB) {
      GPU_matrix_push();
      GPU_matrix_translate_2f(rect.xmin, yofs);
      GPU_matrix_scale_2f(w, h);

      waveform_draw_one(scopes->waveform_1, scopes->waveform_tot, colors_alpha[0]);
      waveform_draw_one(scopes->waveform_2, scopes->waveform_tot, colors_alpha[1]);
      waveform_draw_one(scopes->waveform_3, scopes->waveform_tot, colors_alpha[2]);

      GPU_matrix_pop();
    }
    /* PARADE / YCC (3 channels) */
    else if (ELEM(scopes->wavefrm_mode,
                  SCOPES_WAVEFRM_RGB_PARADE,
                  SCOPES_WAVEFRM_YCC_601,
                  SCOPES_WAVEFRM_YCC_709,
                  SCOPES_WAVEFRM_YCC_JPEG)) {
      int rgb = (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB_PARADE);

      GPU_matrix_push();
      GPU_matrix_translate_2f(rect.xmin, yofs);
      GPU_matrix_scale_2f(w3, h);

      waveform_draw_one(
          scopes->waveform_1, scopes->waveform_tot, (rgb) ? colors_alpha[0] : colorsycc_alpha[0]);

      GPU_matrix_translate_2f(1.0f, 0.0f);
      waveform_draw_one(
          scopes->waveform_2, scopes->waveform_tot, (rgb) ? colors_alpha[1] : colorsycc_alpha[1]);

      GPU_matrix_translate_2f(1.0f, 0.0f);
      waveform_draw_one(
          scopes->waveform_3, scopes->waveform_tot, (rgb) ? colors_alpha[2] : colorsycc_alpha[2]);

      GPU_matrix_pop();
    }

    /* min max */
    if (scopes->wavefrm_mode != SCOPES_WAVEFRM_LUMA) {
      for (int c = 0; c < 3; c++) {
        if (ELEM(scopes->wavefrm_mode, SCOPES_WAVEFRM_RGB_PARADE, SCOPES_WAVEFRM_RGB)) {
          immUniformColor3f(colors[c][0] * 0.75f, colors[c][1] * 0.75f, colors[c][2] * 0.75f);
        }
        else {
          immUniformColor3f(
              colorsycc[c][0] * 0.75f, colorsycc[c][1] * 0.75f, colorsycc[c][2] * 0.75f);
        }
        min = yofs + scopes->minmax[c][0] * h;
        max = yofs + scopes->minmax[c][1] * h;
        CLAMP(min, rect.ymin, rect.ymax);
        CLAMP(max, rect.ymin, rect.ymax);

        immBegin(GPU_PRIM_LINES, 2);
        immVertex2f(pos, rect.xmin + w + 2 + c * 2, min);
        immVertex2f(pos, rect.xmin + w + 2 + c * 2, max);
        immEnd();
      }
    }
  }

  immUnbindProgram();

  /* outline */
  draw_scope_end(&rect, scissor);

  GPU_blend(false);
}

static float polar_to_x(float center, float diam, float ampli, float angle)
{
  return center + diam * ampli * cosf(angle);
}

static float polar_to_y(float center, float diam, float ampli, float angle)
{
  return center + diam * ampli * sinf(angle);
}

static void vectorscope_draw_target(
    uint pos, float centerx, float centery, float diam, const float colf[3])
{
  float y, u, v;
  float tangle = 0.0f, tampli;
  float dangle, dampli, dangle2, dampli2;

  rgb_to_yuv(colf[0], colf[1], colf[2], &y, &u, &v, BLI_YUV_ITU_BT709);

  if (u > 0 && v >= 0) {
    tangle = atanf(v / u);
  }
  else if (u > 0 && v < 0) {
    tangle = atanf(v / u) + 2.0f * (float)M_PI;
  }
  else if (u < 0) {
    tangle = atanf(v / u) + (float)M_PI;
  }
  else if (u == 0 && v > 0.0f) {
    tangle = M_PI_2;
  }
  else if (u == 0 && v < 0.0f) {
    tangle = -M_PI_2;
  }
  tampli = sqrtf(u * u + v * v);

  /* small target vary by 2.5 degree and 2.5 IRE unit */
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.12f);
  dangle = DEG2RADF(2.5f);
  dampli = 2.5f / 200.0f;
  immBegin(GPU_PRIM_LINE_LOOP, 4);
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli, tangle + dangle),
              polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli, tangle + dangle),
              polar_to_y(centery, diam, tampli - dampli, tangle + dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli, tangle - dangle),
              polar_to_y(centery, diam, tampli - dampli, tangle - dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli, tangle - dangle),
              polar_to_y(centery, diam, tampli + dampli, tangle - dangle));
  immEnd();
  /* big target vary by 10 degree and 20% amplitude */
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.12f);
  dangle = DEG2RADF(10.0f);
  dampli = 0.2f * tampli;
  dangle2 = DEG2RADF(5.0f);
  dampli2 = 0.5f * dampli;
  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli - dampli2, tangle + dangle),
              polar_to_y(centery, diam, tampli + dampli - dampli2, tangle + dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli, tangle + dangle),
              polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli, tangle + dangle - dangle2),
              polar_to_y(centery, diam, tampli + dampli, tangle + dangle - dangle2));
  immEnd();
  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli + dampli2, tangle + dangle),
              polar_to_y(centery, diam, tampli - dampli + dampli2, tangle + dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli, tangle + dangle),
              polar_to_y(centery, diam, tampli - dampli, tangle + dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli, tangle + dangle - dangle2),
              polar_to_y(centery, diam, tampli - dampli, tangle + dangle - dangle2));
  immEnd();
  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli + dampli2, tangle - dangle),
              polar_to_y(centery, diam, tampli - dampli + dampli2, tangle - dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli, tangle - dangle),
              polar_to_y(centery, diam, tampli - dampli, tangle - dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli - dampli, tangle - dangle + dangle2),
              polar_to_y(centery, diam, tampli - dampli, tangle - dangle + dangle2));
  immEnd();
  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli - dampli2, tangle - dangle),
              polar_to_y(centery, diam, tampli + dampli - dampli2, tangle - dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli, tangle - dangle),
              polar_to_y(centery, diam, tampli + dampli, tangle - dangle));
  immVertex2f(pos,
              polar_to_x(centerx, diam, tampli + dampli, tangle - dangle + dangle2),
              polar_to_y(centery, diam, tampli + dampli, tangle - dangle + dangle2));
  immEnd();
}

void ui_draw_but_VECTORSCOPE(ARegion *UNUSED(ar),
                             uiBut *but,
                             const uiWidgetColors *UNUSED(wcol),
                             const rcti *recti)
{
  const float skin_rad = DEG2RADF(123.0f); /* angle in radians of the skin tone line */
  Scopes *scopes = (Scopes *)but->poin;

  const float colors[6][3] = {
      {0.75, 0.0, 0.0},
      {0.75, 0.75, 0.0},
      {0.0, 0.75, 0.0},
      {0.0, 0.75, 0.75},
      {0.0, 0.0, 0.75},
      {0.75, 0.0, 0.75},
  };

  rctf rect = {
      .xmin = (float)recti->xmin + 1,
      .xmax = (float)recti->xmax - 1,
      .ymin = (float)recti->ymin + 1,
      .ymax = (float)recti->ymax - 1,
  };

  float w = BLI_rctf_size_x(&rect);
  float h = BLI_rctf_size_y(&rect);
  float centerx = rect.xmin + w * 0.5f;
  float centery = rect.ymin + h * 0.5f;
  float diam = (w < h) ? w : h;

  float alpha = scopes->vecscope_alpha * scopes->vecscope_alpha * scopes->vecscope_alpha;

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  float color[4];
  UI_GetThemeColor4fv(TH_PREVIEW_BACK, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_4fv(
      true, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f, color);

  /* need scissor test, hvectorscope can draw outside of boundary */
  int scissor[4];
  GPU_scissor_get_i(scissor);
  GPU_scissor((rect.xmin - 1),
              (rect.ymin - 1),
              (rect.xmax + 1) - (rect.xmin - 1),
              (rect.ymax + 1) - (rect.ymin - 1));

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.08f);
  /* draw grid elements */
  /* cross */
  immBegin(GPU_PRIM_LINES, 4);

  immVertex2f(pos, centerx - (diam * 0.5f) - 5, centery);
  immVertex2f(pos, centerx + (diam * 0.5f) + 5, centery);

  immVertex2f(pos, centerx, centery - (diam * 0.5f) - 5);
  immVertex2f(pos, centerx, centery + (diam * 0.5f) + 5);

  immEnd();

  /* circles */
  for (int j = 0; j < 5; j++) {
    const int increment = 15;
    immBegin(GPU_PRIM_LINE_LOOP, (int)(360 / increment));
    for (int i = 0; i <= 360 - increment; i += increment) {
      const float a = DEG2RADF((float)i);
      const float r = (j + 1) * 0.1f;
      immVertex2f(pos, polar_to_x(centerx, diam, r, a), polar_to_y(centery, diam, r, a));
    }
    immEnd();
  }
  /* skin tone line */
  immUniformColor4f(1.0f, 0.4f, 0.0f, 0.2f);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(
      pos, polar_to_x(centerx, diam, 0.5f, skin_rad), polar_to_y(centery, diam, 0.5f, skin_rad));
  immVertex2f(
      pos, polar_to_x(centerx, diam, 0.1f, skin_rad), polar_to_y(centery, diam, 0.1f, skin_rad));
  immEnd();

  /* saturation points */
  for (int i = 0; i < 6; i++) {
    vectorscope_draw_target(pos, centerx, centery, diam, colors[i]);
  }

  if (scopes->ok && scopes->vecscope != NULL) {
    /* pixel point cloud */
    float col[3] = {alpha, alpha, alpha};

    glBlendFunc(GL_ONE, GL_ONE);
    GPU_point_size(1.0);

    GPU_matrix_push();
    GPU_matrix_translate_2f(centerx, centery);
    GPU_matrix_scale_1f(diam);

    waveform_draw_one(scopes->vecscope, scopes->waveform_tot, col);

    GPU_matrix_pop();
  }

  immUnbindProgram();

  /* outline */
  draw_scope_end(&rect, scissor);

  GPU_blend(false);
}

static void ui_draw_colorband_handle_tri_hlight(
    uint pos, float x1, float y1, float halfwidth, float height)
{
  GPU_line_smooth(true);

  immBegin(GPU_PRIM_LINE_STRIP, 3);
  immVertex2f(pos, x1 + halfwidth, y1);
  immVertex2f(pos, x1, y1 + height);
  immVertex2f(pos, x1 - halfwidth, y1);
  immEnd();

  GPU_line_smooth(false);
}

static void ui_draw_colorband_handle_tri(
    uint pos, float x1, float y1, float halfwidth, float height, bool fill)
{
  if (fill) {
    GPU_polygon_smooth(true);
  }
  else {
    GPU_line_smooth(true);
  }

  immBegin(fill ? GPU_PRIM_TRIS : GPU_PRIM_LINE_LOOP, 3);
  immVertex2f(pos, x1 + halfwidth, y1);
  immVertex2f(pos, x1, y1 + height);
  immVertex2f(pos, x1 - halfwidth, y1);
  immEnd();

  if (fill) {
    GPU_polygon_smooth(false);
  }
  else {
    GPU_line_smooth(false);
  }
}

static void ui_draw_colorband_handle_box(
    uint pos, float x1, float y1, float x2, float y2, bool fill)
{
  immBegin(fill ? GPU_PRIM_TRI_FAN : GPU_PRIM_LINE_LOOP, 4);
  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x1, y2);
  immVertex2f(pos, x2, y2);
  immVertex2f(pos, x2, y1);
  immEnd();
}

static void ui_draw_colorband_handle(uint shdr_pos,
                                     const rcti *rect,
                                     float x,
                                     const float rgb[3],
                                     struct ColorManagedDisplay *display,
                                     bool active)
{
  const float sizey = BLI_rcti_size_y(rect);
  const float min_width = 3.0f;
  float colf[3] = {UNPACK3(rgb)};

  float half_width = floorf(sizey / 3.5f);
  float height = half_width * 1.4f;

  float y1 = rect->ymin + (sizey * 0.16f);
  float y2 = rect->ymax;

  /* align to pixels */
  x = floorf(x + 0.5f);
  y1 = floorf(y1 + 0.5f);

  if (active || half_width < min_width) {
    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

    float viewport_size[4];
    GPU_viewport_size_get_f(viewport_size);
    immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

    immUniform1i("colors_len", 2); /* "advanced" mode */
    immUniformArray4fv(
        "colors", (float *)(float[][4]){{0.8f, 0.8f, 0.8f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, 2);
    immUniform1f("dash_width", active ? 4.0f : 2.0f);
    immUniform1f("dash_factor", 0.5f);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(shdr_pos, x, y1);
    immVertex2f(shdr_pos, x, y2);
    immEnd();

    immUnbindProgram();

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    /* hide handles when zoomed out too far */
    if (half_width < min_width) {
      return;
    }
  }

  /* shift handle down */
  y1 -= half_width;

  immUniformColor3ub(0, 0, 0);
  ui_draw_colorband_handle_box(
      shdr_pos, x - half_width, y1 - 1, x + half_width, y1 + height, false);

  /* draw all triangles blended */
  GPU_blend(true);

  ui_draw_colorband_handle_tri(shdr_pos, x, y1 + height, half_width, half_width, true);

  if (active) {
    immUniformColor3ub(196, 196, 196);
  }
  else {
    immUniformColor3ub(96, 96, 96);
  }
  ui_draw_colorband_handle_tri(shdr_pos, x, y1 + height, half_width, half_width, true);

  if (active) {
    immUniformColor3ub(255, 255, 255);
  }
  else {
    immUniformColor3ub(128, 128, 128);
  }
  ui_draw_colorband_handle_tri_hlight(
      shdr_pos, x, y1 + height - 1, (half_width - 1), (half_width - 1));

  immUniformColor3ub(0, 0, 0);
  ui_draw_colorband_handle_tri_hlight(shdr_pos, x, y1 + height, half_width, half_width);

  GPU_blend(false);

  immUniformColor3ub(128, 128, 128);
  ui_draw_colorband_handle_box(
      shdr_pos, x - (half_width - 1), y1, x + (half_width - 1), y1 + height, true);

  if (display) {
    IMB_colormanagement_scene_linear_to_display_v3(colf, display);
  }

  immUniformColor3fv(colf);
  ui_draw_colorband_handle_box(
      shdr_pos, x - (half_width - 2), y1 + 1, x + (half_width - 2), y1 + height - 2, true);
}

void ui_draw_but_COLORBAND(uiBut *but, const uiWidgetColors *UNUSED(wcol), const rcti *rect)
{
  struct ColorManagedDisplay *display = ui_block_cm_display_get(but->block);
  uint pos_id, col_id;

  ColorBand *coba = (ColorBand *)(but->editcoba ? but->editcoba : but->poin);
  if (coba == NULL) {
    return;
  }

  float x1 = rect->xmin;
  float sizex = rect->xmax - x1;
  float sizey = BLI_rcti_size_y(rect);
  float sizey_solid = sizey * 0.25f;
  float y1 = rect->ymin;

  GPUVertFormat *format = immVertexFormat();
  pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_CHECKER);

  /* Drawing the checkerboard. */
  const float checker_dark = UI_ALPHA_CHECKER_DARK / 255.0f;
  const float checker_light = UI_ALPHA_CHECKER_LIGHT / 255.0f;
  immUniform4f("color1", checker_dark, checker_dark, checker_dark, 1.0f);
  immUniform4f("color2", checker_light, checker_light, checker_light, 1.0f);
  immUniform1i("size", 8);
  immRectf(pos_id, x1, y1, x1 + sizex, rect->ymax);
  immUnbindProgram();

  /* New format */
  format = immVertexFormat();
  pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  col_id = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

  /* layer: color ramp */
  GPU_blend(true);

  CBData *cbd = coba->data;

  float v1[2], v2[2];
  float colf[4] = {0, 0, 0, 0}; /* initialize in case the colorband isn't valid */

  v1[1] = y1 + sizey_solid;
  v2[1] = rect->ymax;

  immBegin(GPU_PRIM_TRI_STRIP, (sizex + 1) * 2);
  for (int a = 0; a <= sizex; a++) {
    float pos = ((float)a) / sizex;
    BKE_colorband_evaluate(coba, pos, colf);
    if (display) {
      IMB_colormanagement_scene_linear_to_display_v3(colf, display);
    }

    v1[0] = v2[0] = x1 + a;

    immAttr4fv(col_id, colf);
    immVertex2fv(pos_id, v1);
    immVertex2fv(pos_id, v2);
  }
  immEnd();

  /* layer: color ramp without alpha for reference when manipulating ramp properties */
  v1[1] = y1;
  v2[1] = y1 + sizey_solid;

  immBegin(GPU_PRIM_TRI_STRIP, (sizex + 1) * 2);
  for (int a = 0; a <= sizex; a++) {
    float pos = ((float)a) / sizex;
    BKE_colorband_evaluate(coba, pos, colf);
    if (display) {
      IMB_colormanagement_scene_linear_to_display_v3(colf, display);
    }

    v1[0] = v2[0] = x1 + a;

    immAttr4f(col_id, colf[0], colf[1], colf[2], 1.0f);
    immVertex2fv(pos_id, v1);
    immVertex2fv(pos_id, v2);
  }
  immEnd();

  immUnbindProgram();

  GPU_blend(false);

  /* New format */
  format = immVertexFormat();
  pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* layer: box outline */
  immUniformColor4f(0.0f, 0.0f, 0.0f, 1.0f);
  imm_draw_box_wire_2d(pos_id, x1, y1, x1 + sizex, rect->ymax);

  /* layer: box outline */
  GPU_blend(true);
  immUniformColor4f(0.0f, 0.0f, 0.0f, 0.5f);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos_id, x1, y1);
  immVertex2f(pos_id, x1 + sizex, y1);
  immEnd();

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.25f);

  immBegin(GPU_PRIM_LINES, 2);
  immVertex2f(pos_id, x1, y1 - 1);
  immVertex2f(pos_id, x1 + sizex, y1 - 1);
  immEnd();

  GPU_blend(false);

  /* layer: draw handles */
  for (int a = 0; a < coba->tot; a++, cbd++) {
    if (a != coba->cur) {
      float pos = x1 + cbd->pos * (sizex - 1) + 1;
      ui_draw_colorband_handle(pos_id, rect, pos, &cbd->r, display, false);
    }
  }

  /* layer: active handle */
  if (coba->tot != 0) {
    cbd = &coba->data[coba->cur];
    float pos = x1 + cbd->pos * (sizex - 1) + 1;
    ui_draw_colorband_handle(pos_id, rect, pos, &cbd->r, display, true);
  }

  immUnbindProgram();
}

void ui_draw_but_UNITVEC(uiBut *but, const uiWidgetColors *wcol, const rcti *rect)
{
  /* sphere color */
  float diffuse[3] = {1.0f, 1.0f, 1.0f};
  float light[3];
  float size;

  /* backdrop */
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_3ubAlpha(
      true, rect->xmin, rect->ymin, rect->xmax, rect->ymax, 5.0f, (uchar *)wcol->inner, 255);

  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);

  /* setup lights */
  ui_but_v3_get(but, light);

  /* transform to button */
  GPU_matrix_push();

  if (BLI_rcti_size_x(rect) < BLI_rcti_size_y(rect)) {
    size = 0.5f * BLI_rcti_size_x(rect);
  }
  else {
    size = 0.5f * BLI_rcti_size_y(rect);
  }

  GPU_matrix_translate_2f(rect->xmin + 0.5f * BLI_rcti_size_x(rect),
                          rect->ymin + 0.5f * BLI_rcti_size_y(rect));
  GPU_matrix_scale_1f(size);

  GPUBatch *sphere = GPU_batch_preset_sphere(2);
  GPU_batch_program_set_builtin(sphere, GPU_SHADER_SIMPLE_LIGHTING);
  GPU_batch_uniform_4f(sphere, "color", diffuse[0], diffuse[1], diffuse[2], 1.0f);
  GPU_batch_uniform_3fv(sphere, "light", light);
  GPU_batch_draw(sphere);

  /* restore */
  glDisable(GL_CULL_FACE);

  /* AA circle */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor3ubv((uchar *)wcol->inner);

  GPU_blend(true);
  GPU_line_smooth(true);
  imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, 1.0f, 32);
  GPU_blend(false);
  GPU_line_smooth(false);

  /* matrix after circle */
  GPU_matrix_pop();

  immUnbindProgram();
}

static void ui_draw_but_curve_grid(
    uint pos, const rcti *rect, float zoomx, float zoomy, float offsx, float offsy, float step)
{
  float dx = step * zoomx;
  float fx = rect->xmin + zoomx * (-offsx);
  if (fx > rect->xmin) {
    fx -= dx * (floorf(fx - rect->xmin));
  }

  float dy = step * zoomy;
  float fy = rect->ymin + zoomy * (-offsy);
  if (fy > rect->ymin) {
    fy -= dy * (floorf(fy - rect->ymin));
  }

  float line_count = (floorf((rect->xmax - fx) / dx) + 1.0f + floorf((rect->ymax - fy) / dy) +
                      1.0f);

  immBegin(GPU_PRIM_LINES, (int)line_count * 2);
  while (fx <= rect->xmax) {
    immVertex2f(pos, fx, rect->ymin);
    immVertex2f(pos, fx, rect->ymax);
    fx += dx;
  }
  while (fy <= rect->ymax) {
    immVertex2f(pos, rect->xmin, fy);
    immVertex2f(pos, rect->xmax, fy);
    fy += dy;
  }
  immEnd();
}

static void gl_shaded_color_get(const uchar color[3], int shade, uchar r_color[3])
{
  r_color[0] = color[0] - shade > 0 ? color[0] - shade : 0;
  r_color[1] = color[1] - shade > 0 ? color[1] - shade : 0;
  r_color[2] = color[2] - shade > 0 ? color[2] - shade : 0;
}

static void gl_shaded_color_get_fl(const uchar *color, int shade, float r_color[3])
{
  uchar color_shaded[3];
  gl_shaded_color_get(color, shade, color_shaded);
  rgb_uchar_to_float(r_color, color_shaded);
}

static void gl_shaded_color(uchar *color, int shade)
{
  uchar color_shaded[3];
  gl_shaded_color_get(color, shade, color_shaded);
  immUniformColor3ubv(color_shaded);
}

void ui_draw_but_CURVE(ARegion *ar, uiBut *but, const uiWidgetColors *wcol, const rcti *rect)
{
  CurveMapping *cumap;

  if (but->editcumap) {
    cumap = but->editcumap;
  }
  else {
    cumap = (CurveMapping *)but->poin;
  }

  /* calculate offset and zoom */
  float zoomx = (BLI_rcti_size_x(rect) - 2.0f) / BLI_rctf_size_x(&cumap->curr);
  float zoomy = (BLI_rcti_size_y(rect) - 2.0f) / BLI_rctf_size_y(&cumap->curr);
  float offsx = cumap->curr.xmin - (1.0f / zoomx);
  float offsy = cumap->curr.ymin - (1.0f / zoomy);

  /* exit early if too narrow */
  if (zoomx == 0.0f) {
    return;
  }

  CurveMap *cuma = &cumap->cm[cumap->cur];

  /* need scissor test, curve can draw outside of boundary */
  int scissor[4];
  GPU_scissor_get_i(scissor);
  rcti scissor_new = {
      .xmin = rect->xmin,
      .ymin = rect->ymin,
      .xmax = rect->xmax,
      .ymax = rect->ymax,
  };
  rcti scissor_region = {0, ar->winx, 0, ar->winy};
  BLI_rcti_isect(&scissor_new, &scissor_region, &scissor_new);
  GPU_scissor(scissor_new.xmin,
              scissor_new.ymin,
              BLI_rcti_size_x(&scissor_new),
              BLI_rcti_size_y(&scissor_new));

  /* Do this first to not mess imm context */
  if (but->a1 == UI_GRAD_H) {
    /* magic trigger for curve backgrounds */
    float col[3] = {0.0f, 0.0f, 0.0f}; /* dummy arg */

    rcti grid = {
        .xmin = rect->xmin + zoomx * (-offsx),
        .xmax = grid.xmin + zoomx,
        .ymin = rect->ymin + zoomy * (-offsy),
        .ymax = grid.ymin + zoomy,
    };

    ui_draw_gradient(&grid, col, UI_GRAD_H, 1.0f);
  }

  GPU_line_width(1.0f);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* backdrop */
  float color_backdrop[4] = {0, 0, 0, 1};

  if (but->a1 == UI_GRAD_H) {
    /* grid, hsv uses different grid */
    GPU_blend(true);
    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    ARRAY_SET_ITEMS(color_backdrop, 0, 0, 0, 48.0 / 255.0);
    immUniformColor4fv(color_backdrop);
    ui_draw_but_curve_grid(pos, rect, zoomx, zoomy, offsx, offsy, 0.1666666f);
    GPU_blend(false);
  }
  else {
    if (cumap->flag & CUMA_DO_CLIP) {
      gl_shaded_color_get_fl((uchar *)wcol->inner, -20, color_backdrop);
      immUniformColor3fv(color_backdrop);
      immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
      immUniformColor3ubv((uchar *)wcol->inner);
      immRectf(pos,
               rect->xmin + zoomx * (cumap->clipr.xmin - offsx),
               rect->ymin + zoomy * (cumap->clipr.ymin - offsy),
               rect->xmin + zoomx * (cumap->clipr.xmax - offsx),
               rect->ymin + zoomy * (cumap->clipr.ymax - offsy));
    }
    else {
      rgb_uchar_to_float(color_backdrop, (const uchar *)wcol->inner);
      immUniformColor3fv(color_backdrop);
      immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
    }

    /* grid, every 0.25 step */
    gl_shaded_color((uchar *)wcol->inner, -16);
    ui_draw_but_curve_grid(pos, rect, zoomx, zoomy, offsx, offsy, 0.25f);
    /* grid, every 1.0 step */
    gl_shaded_color((uchar *)wcol->inner, -24);
    ui_draw_but_curve_grid(pos, rect, zoomx, zoomy, offsx, offsy, 1.0f);
    /* axes */
    gl_shaded_color((uchar *)wcol->inner, -50);
    immBegin(GPU_PRIM_LINES, 4);
    immVertex2f(pos, rect->xmin, rect->ymin + zoomy * (-offsy));
    immVertex2f(pos, rect->xmax, rect->ymin + zoomy * (-offsy));
    immVertex2f(pos, rect->xmin + zoomx * (-offsx), rect->ymin);
    immVertex2f(pos, rect->xmin + zoomx * (-offsx), rect->ymax);
    immEnd();
  }

  /* cfra option */
  /* XXX 2.48 */
#if 0
  if (cumap->flag & CUMA_DRAW_CFRA) {
    immUniformColor3ub(0x60, 0xc0, 0x40);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, rect->xmin + zoomx * (cumap->sample[0] - offsx), rect->ymin);
    immVertex2f(pos, rect->xmin + zoomx * (cumap->sample[0] - offsx), rect->ymax);
    immEnd();
  }
#endif
  /* sample option */

  if (cumap->flag & CUMA_DRAW_SAMPLE) {
    immBegin(GPU_PRIM_LINES, 2); /* will draw one of the following 3 lines */
    if (but->a1 == UI_GRAD_H) {
      float tsample[3];
      float hsv[3];
      linearrgb_to_srgb_v3_v3(tsample, cumap->sample);
      rgb_to_hsv_v(tsample, hsv);
      immUniformColor3ub(240, 240, 240);

      immVertex2f(pos, rect->xmin + zoomx * (hsv[0] - offsx), rect->ymin);
      immVertex2f(pos, rect->xmin + zoomx * (hsv[0] - offsx), rect->ymax);
    }
    else if (cumap->cur == 3) {
      float lum = IMB_colormanagement_get_luminance(cumap->sample);
      immUniformColor3ub(240, 240, 240);

      immVertex2f(pos, rect->xmin + zoomx * (lum - offsx), rect->ymin);
      immVertex2f(pos, rect->xmin + zoomx * (lum - offsx), rect->ymax);
    }
    else {
      if (cumap->cur == 0) {
        immUniformColor3ub(240, 100, 100);
      }
      else if (cumap->cur == 1) {
        immUniformColor3ub(100, 240, 100);
      }
      else {
        immUniformColor3ub(100, 100, 240);
      }

      immVertex2f(pos, rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymin);
      immVertex2f(pos, rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymax);
    }
    immEnd();
  }
  immUnbindProgram();

  if (cuma->table == NULL) {
    curvemapping_changed(cumap, false);
  }

  CurveMapPoint *cmp = cuma->table;
  rctf line_range;

  /* First curve point. */
  if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
    line_range.xmin = rect->xmin;
    line_range.ymin = rect->ymin + zoomy * (cmp[0].y - offsy);
  }
  else {
    line_range.xmin = rect->xmin + zoomx * (cmp[0].x - offsx + cuma->ext_in[0]);
    line_range.ymin = rect->ymin + zoomy * (cmp[0].y - offsy + cuma->ext_in[1]);
  }
  /* Last curve point. */
  if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
    line_range.xmax = rect->xmax;
    line_range.ymax = rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy);
  }
  else {
    line_range.xmax = rect->xmin + zoomx * (cmp[CM_TABLE].x - offsx - cuma->ext_out[0]);
    line_range.ymax = rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy - cuma->ext_out[1]);
  }

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  GPU_blend(true);

  /* Curve filled. */
  immUniformColor3ubvAlpha((uchar *)wcol->item, 128);
  GPU_polygon_smooth(true);
  immBegin(GPU_PRIM_TRI_STRIP, (CM_TABLE * 2 + 2) + 4);
  immVertex2f(pos, line_range.xmin, rect->ymin);
  immVertex2f(pos, line_range.xmin, line_range.ymin);
  for (int a = 0; a <= CM_TABLE; a++) {
    float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
    float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
    immVertex2f(pos, fx, rect->ymin);
    immVertex2f(pos, fx, fy);
  }
  immVertex2f(pos, line_range.xmax, rect->ymin);
  immVertex2f(pos, line_range.xmax, line_range.ymax);
  immEnd();
  GPU_polygon_smooth(false);

  /* Curve line. */
  GPU_line_width(1.0f);
  immUniformColor3ubvAlpha((uchar *)wcol->item, 255);
  GPU_line_smooth(true);
  immBegin(GPU_PRIM_LINE_STRIP, (CM_TABLE + 1) + 2);
  immVertex2f(pos, line_range.xmin, line_range.ymin);
  for (int a = 0; a <= CM_TABLE; a++) {
    float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
    float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
    immVertex2f(pos, fx, fy);
  }
  immVertex2f(pos, line_range.xmax, line_range.ymax);
  immEnd();

  /* Reset state for fill & line. */
  GPU_line_smooth(false);
  GPU_blend(false);
  immUnbindProgram();

  /* The points, use aspect to make them visible on edges. */
  format = immVertexFormat();
  pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

  /* Calculate vertex colors based on text theme. */
  float color_vert[4], color_vert_select[4];
  UI_GetThemeColor4fv(TH_TEXT_HI, color_vert);
  UI_GetThemeColor4fv(TH_TEXT, color_vert_select);
  if (len_squared_v3v3(color_vert, color_vert_select) < 0.1f) {
    interp_v3_v3v3(color_vert, color_vert_select, color_backdrop, 0.75f);
  }
  if (len_squared_v3(color_vert) > len_squared_v3(color_vert_select)) {
    /* Ensure brightest text color is used for selection. */
    swap_v3_v3(color_vert, color_vert_select);
  }

  cmp = cuma->curve;
  GPU_point_size(max_ff(1.0f, min_ff(UI_DPI_FAC / but->block->aspect * 4.0f, 4.0f)));
  immBegin(GPU_PRIM_POINTS, cuma->totpoint);
  for (int a = 0; a < cuma->totpoint; a++) {
    float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
    float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
    immAttr4fv(col, (cmp[a].flag & CUMA_SELECT) ? color_vert_select : color_vert);
    immVertex2f(pos, fx, fy);
  }
  immEnd();
  immUnbindProgram();

  /* restore scissortest */
  GPU_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);

  /* outline */
  format = immVertexFormat();
  pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  immUniformColor3ubv((uchar *)wcol->outline);
  imm_draw_box_wire_2d(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);

  immUnbindProgram();
}

void ui_draw_but_TRACKPREVIEW(ARegion *UNUSED(ar),
                              uiBut *but,
                              const uiWidgetColors *UNUSED(wcol),
                              const rcti *recti)
{
  bool ok = false;
  MovieClipScopes *scopes = (MovieClipScopes *)but->poin;

  rctf rect = {
      .xmin = (float)recti->xmin + 1,
      .xmax = (float)recti->xmax - 1,
      .ymin = (float)recti->ymin + 1,
      .ymax = (float)recti->ymax - 1,
  };

  int width = BLI_rctf_size_x(&rect) + 1;
  int height = BLI_rctf_size_y(&rect);

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  /* need scissor test, preview image can draw outside of boundary */
  int scissor[4];
  GPU_scissor_get_i(scissor);
  GPU_scissor((rect.xmin - 1),
              (rect.ymin - 1),
              (rect.xmax + 1) - (rect.xmin - 1),
              (rect.ymax + 1) - (rect.ymin - 1));

  if (scopes->track_disabled) {
    float color[4] = {0.7f, 0.3f, 0.3f, 0.3f};
    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_4fv(
        true, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f, color);

    ok = true;
  }
  else if ((scopes->track_search) &&
           ((!scopes->track_preview) ||
            (scopes->track_preview->x != width || scopes->track_preview->y != height))) {
    if (scopes->track_preview) {
      IMB_freeImBuf(scopes->track_preview);
    }

    ImBuf *tmpibuf = BKE_tracking_sample_pattern(scopes->frame_width,
                                                 scopes->frame_height,
                                                 scopes->track_search,
                                                 scopes->track,
                                                 &scopes->undist_marker,
                                                 true,
                                                 scopes->use_track_mask,
                                                 width,
                                                 height,
                                                 scopes->track_pos);
    if (tmpibuf) {
      if (tmpibuf->rect_float) {
        IMB_rect_from_float(tmpibuf);
      }

      if (tmpibuf->rect) {
        scopes->track_preview = tmpibuf;
      }
      else {
        IMB_freeImBuf(tmpibuf);
      }
    }
  }

  if (!ok && scopes->track_preview) {
    GPU_matrix_push();

    /* draw content of pattern area */
    GPU_scissor(rect.xmin, rect.ymin, scissor[2], scissor[3]);

    if (width > 0 && height > 0) {
      ImBuf *drawibuf = scopes->track_preview;
      float col_sel[4], col_outline[4];

      if (scopes->use_track_mask) {
        float color[4] = {0.0f, 0.0f, 0.0f, 0.3f};
        UI_draw_roundbox_corner_set(UI_CNR_ALL);
        UI_draw_roundbox_4fv(
            true, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f, color);
      }

      IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_2D_IMAGE_COLOR);
      immDrawPixelsTex(&state,
                       rect.xmin,
                       rect.ymin + 1,
                       drawibuf->x,
                       drawibuf->y,
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       GL_LINEAR,
                       drawibuf->rect,
                       1.0f,
                       1.0f,
                       NULL);

      /* draw cross for pixel position */
      GPU_matrix_translate_2f(rect.xmin + scopes->track_pos[0], rect.ymin + scopes->track_pos[1]);
      GPU_scissor(rect.xmin, rect.ymin, BLI_rctf_size_x(&rect), BLI_rctf_size_y(&rect));

      GPUVertFormat *format = immVertexFormat();
      uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

      UI_GetThemeColor4fv(TH_SEL_MARKER, col_sel);
      UI_GetThemeColor4fv(TH_MARKER_OUTLINE, col_outline);

      /* Do stipple cross with geometry */
      immBegin(GPU_PRIM_LINES, 7 * 2 * 2);
      float pos_sel[8] = {-10.0f, -7.0f, -4.0f, -1.0f, 2.0f, 5.0f, 8.0f, 11.0f};
      for (int axe = 0; axe < 2; ++axe) {
        for (int i = 0; i < 7; ++i) {
          float x1 = pos_sel[i] * (1 - axe);
          float y1 = pos_sel[i] * axe;
          float x2 = pos_sel[i + 1] * (1 - axe);
          float y2 = pos_sel[i + 1] * axe;

          if (i % 2 == 1) {
            immAttr4fv(col, col_sel);
          }
          else {
            immAttr4fv(col, col_outline);
          }

          immVertex2f(pos, x1, y1);
          immVertex2f(pos, x2, y2);
        }
      }
      immEnd();

      immUnbindProgram();
    }

    GPU_matrix_pop();

    ok = true;
  }

  if (!ok) {
    float color[4] = {0.0f, 0.0f, 0.0f, 0.3f};
    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_4fv(
        true, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f, color);
  }

  /* outline */
  draw_scope_end(&rect, scissor);

  GPU_blend(false);
}

/* ****************************************************** */

/* TODO: high quality UI drop shadows using GLSL shader and single draw call
 * would replace / modify the following 3 functions  - merwin
 */

static void ui_shadowbox(uint pos,
                         uint color,
                         float minx,
                         float miny,
                         float maxx,
                         float maxy,
                         float shadsize,
                         uchar alpha)
{
  /**
   * <pre>
   *          v1-_
   *          |   -_v2
   *          |     |
   *          |     |
   *          |     |
   * v7_______v3____v4
   * \        |     /
   *  \       |   _v5
   *  v8______v6_-
   * </pre>
   */
  const float v1[2] = {maxx, maxy - 0.3f * shadsize};
  const float v2[2] = {maxx + shadsize, maxy - 0.75f * shadsize};
  const float v3[2] = {maxx, miny};
  const float v4[2] = {maxx + shadsize, miny};

  const float v5[2] = {maxx + 0.7f * shadsize, miny - 0.7f * shadsize};

  const float v6[2] = {maxx, miny - shadsize};
  const float v7[2] = {minx + 0.3f * shadsize, miny};
  const float v8[2] = {minx + 0.5f * shadsize, miny - shadsize};

  /* right quad */
  immAttr4ub(color, 0, 0, 0, alpha);
  immVertex2fv(pos, v3);
  immVertex2fv(pos, v1);
  immAttr4ub(color, 0, 0, 0, 0);
  immVertex2fv(pos, v2);

  immVertex2fv(pos, v2);
  immVertex2fv(pos, v4);
  immAttr4ub(color, 0, 0, 0, alpha);
  immVertex2fv(pos, v3);

  /* corner shape */
  /* immAttr4ub(color, 0, 0, 0, alpha); */ /* Not needed, done above in previous tri */
  immVertex2fv(pos, v3);
  immAttr4ub(color, 0, 0, 0, 0);
  immVertex2fv(pos, v4);
  immVertex2fv(pos, v5);

  immVertex2fv(pos, v5);
  immVertex2fv(pos, v6);
  immAttr4ub(color, 0, 0, 0, alpha);
  immVertex2fv(pos, v3);

  /* bottom quad */
  /* immAttr4ub(color, 0, 0, 0, alpha); */ /* Not needed, done above in previous tri */
  immVertex2fv(pos, v3);
  immAttr4ub(color, 0, 0, 0, 0);
  immVertex2fv(pos, v6);
  immVertex2fv(pos, v8);

  immVertex2fv(pos, v8);
  immAttr4ub(color, 0, 0, 0, alpha);
  immVertex2fv(pos, v7);
  immVertex2fv(pos, v3);
}

void UI_draw_box_shadow(uchar alpha, float minx, float miny, float maxx, float maxy)
{
  GPU_blend(true);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

  immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

  immBegin(GPU_PRIM_TRIS, 54);

  /* accumulated outline boxes to make shade not linear, is more pleasant */
  ui_shadowbox(pos, color, minx, miny, maxx, maxy, 11.0, (20 * alpha) >> 8);
  ui_shadowbox(pos, color, minx, miny, maxx, maxy, 7.0, (40 * alpha) >> 8);
  ui_shadowbox(pos, color, minx, miny, maxx, maxy, 5.0, (80 * alpha) >> 8);

  immEnd();

  immUnbindProgram();

  GPU_blend(false);
}

void ui_draw_dropshadow(
    const rctf *rct, float radius, float aspect, float alpha, int UNUSED(select))
{
  float rad;

  if (radius > (BLI_rctf_size_y(rct) - 10.0f) * 0.5f) {
    rad = (BLI_rctf_size_y(rct) - 10.0f) * 0.5f;
  }
  else {
    rad = radius;
  }

  int a, i = 12;
#if 0
  if (select) {
    a = i * aspect; /* same as below */
  }
  else
#endif
  {
    a = i * aspect;
  }

  GPU_blend(true);
  const float dalpha = alpha * 2.0f / 255.0f;
  float calpha = dalpha;
  float visibility = 1.0f;
  for (; i--;) {
    /* alpha ranges from 2 to 20 or so */
#if 0 /* Old Method (pre 2.8) */
    float color[4] = {0.0f, 0.0f, 0.0f, calpha};
    UI_draw_roundbox_4fv(
        true, rct->xmin - a, rct->ymin - a, rct->xmax + a, rct->ymax - 10.0f + a, rad + a, color);
#endif
    /* Compute final visibility to match old method result. */
    /* TODO we could just find a better fit function inside the shader instead of this. */
    visibility = visibility * (1.0f - calpha);
    calpha += dalpha;
  }

  uiWidgetBaseParameters widget_params = {
      .recti.xmin = rct->xmin,
      .recti.ymin = rct->ymin,
      .recti.xmax = rct->xmax,
      .recti.ymax = rct->ymax - 10.0f,
      .rect.xmin = rct->xmin - a,
      .rect.ymin = rct->ymin - a,
      .rect.xmax = rct->xmax + a,
      .rect.ymax = rct->ymax - 10.0f + a,
      .radi = rad,
      .rad = rad + a,
      .round_corners[0] = (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 1.0f : 0.0f,
      .round_corners[1] = (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 1.0f : 0.0f,
      .round_corners[2] = (roundboxtype & UI_CNR_TOP_RIGHT) ? 1.0f : 0.0f,
      .round_corners[3] = (roundboxtype & UI_CNR_TOP_LEFT) ? 1.0f : 0.0f,
      .alpha_discard = 1.0f,
  };

  GPUBatch *batch = ui_batch_roundbox_shadow_get();
  GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_WIDGET_SHADOW);
  GPU_batch_uniform_4fv_array(batch, "parameters", 4, (float *)&widget_params);
  GPU_batch_uniform_1f(batch, "alpha", 1.0f - visibility);
  GPU_batch_draw(batch);

  /* outline emphasis */
  GPU_line_smooth(true);
  float color[4] = {0.0f, 0.0f, 0.0f, 0.4f};
  UI_draw_roundbox_4fv(false,
                       rct->xmin - 0.5f,
                       rct->ymin - 0.5f,
                       rct->xmax + 0.5f,
                       rct->ymax + 0.5f,
                       radius + 0.5f,
                       color);
  GPU_line_smooth(false);

  GPU_blend(false);
}
