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
 * \ingroup edtransform
 */

#include "BLI_math.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BKE_context.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "transform.h"
#include "transform_draw_cursors.h" /* Own include. */

enum eArrowDirection {
  UP,
  DOWN,
  LEFT,
  RIGHT,
};

#define ARROW_WIDTH (2.0f * U.pixelsize)
#define DASH_WIDTH (1.0f)
#define DASH_LENGTH (8.0f * DASH_WIDTH * U.pixelsize)

static void drawArrow(const uint pos_id, const enum eArrowDirection dir)
{
  int offset = 5.0f * UI_DPI_FAC;
  int length = (6.0f * UI_DPI_FAC) + (4.0f * U.pixelsize);
  int size = (3.0f * UI_DPI_FAC) + (2.0f * U.pixelsize);

  /* To line up the arrow point nicely, one end has to be extended by half its width. But
   * being on a 45 degree angle, Pythagoras says a movement of sqrt(2)/2 * (line width /2) */
  float adjust = (M_SQRT2 * ARROW_WIDTH / 4.0f);

  if (ELEM(dir, LEFT, DOWN)) {
    offset = -offset;
    length = -length;
    size = -size;
    adjust = -adjust;
  }

  immBegin(GPU_PRIM_LINES, 6);

  if (ELEM(dir, LEFT, RIGHT)) {
    immVertex2f(pos_id, offset, 0);
    immVertex2f(pos_id, offset + length, 0);
    immVertex2f(pos_id, offset + length + adjust, adjust);
    immVertex2f(pos_id, offset + length - size, -size);
    immVertex2f(pos_id, offset + length, 0);
    immVertex2f(pos_id, offset + length - size, size);
  }
  else {
    immVertex2f(pos_id, 0, offset);
    immVertex2f(pos_id, 0, offset + length);
    immVertex2f(pos_id, adjust, offset + length + adjust);
    immVertex2f(pos_id, -size, offset + length - size);
    immVertex2f(pos_id, 0, offset + length);
    immVertex2f(pos_id, size, offset + length - size);
  }

  immEnd();
}

/**
 * Poll callback for cursor drawing:
 * #WM_paint_cursor_activate
 */
bool transform_draw_cursor_poll(bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  return (region && region->regiontype == RGN_TYPE_WINDOW) ? 1 : 0;
}

/**
 * Cursor and help-line drawing, callback for:
 * #WM_paint_cursor_activate
 */
void transform_draw_cursor_draw(bContext *UNUSED(C), int x, int y, void *customdata)
{
  TransInfo *t = (TransInfo *)customdata;

  if (t->helpline == HLP_NONE) {
    return;
  }

  float cent[2];
  const float mval[3] = {x, y, 0.0f};
  float tmval[2] = {
      (float)t->mval[0],
      (float)t->mval[1],
  };

  projectFloatViewEx(t, t->center_global, cent, V3D_PROJ_TEST_CLIP_ZERO);
  /* Offset the values for the area region. */
  const float offset[2] = {
      t->region->winrct.xmin,
      t->region->winrct.ymin,
  };

  for (int i = 0; i < 2; i++) {
    cent[i] += offset[i];
    tmval[i] += offset[i];
  }

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);

  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);
  const uint pos_id = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  /* Dashed lines first. */
  if (ELEM(t->helpline, HLP_SPRING, HLP_ANGLE)) {
    GPU_line_width(DASH_WIDTH);
    immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);
    immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);
    immUniform1i("colors_len", 0); /* "simple" mode */
    immUniformThemeColor3(TH_VIEW_OVERLAY);
    immUniform1f("dash_width", DASH_LENGTH);
    immUniform1f("dash_factor", 0.5f);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2fv(pos_id, cent);
    immVertex2f(pos_id, tmval[0], tmval[1]);
    immEnd();
    immUnbindProgram();
  }

  /* And now, solid lines. */

  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
  immUniformThemeColor3(TH_VIEW_OVERLAY);
  immUniform2fv("viewportSize", &viewport_size[2]);
  immUniform1f("lineWidth", ARROW_WIDTH);

  GPU_matrix_push();
  GPU_matrix_translate_3fv(mval);

  switch (t->helpline) {
    case HLP_SPRING:
      GPU_matrix_rotate_axis(-RAD2DEGF(atan2f(cent[0] - tmval[0], cent[1] - tmval[1])), 'Z');
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      break;
    case HLP_HARROW:
      drawArrow(pos_id, RIGHT);
      drawArrow(pos_id, LEFT);
      break;
    case HLP_VARROW:
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      break;
    case HLP_CARROW: {
      /* Draw arrow based on direction defined by custom-points. */
      const int *data = t->mouse.data;
      const float angle = -atan2f(data[2] - data[0], data[3] - data[1]);
      GPU_matrix_rotate_axis(RAD2DEGF(angle), 'Z');
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      break;
    }
    case HLP_ANGLE: {
      GPU_matrix_push();
      float angle = atan2f(tmval[1] - cent[1], tmval[0] - cent[0]);
      GPU_matrix_translate_3f(cosf(angle), sinf(angle), 0);
      GPU_matrix_rotate_axis(RAD2DEGF(angle), 'Z');
      drawArrow(pos_id, DOWN);
      GPU_matrix_pop();
      GPU_matrix_translate_3f(cosf(angle), sinf(angle), 0);
      GPU_matrix_rotate_axis(RAD2DEGF(angle), 'Z');
      drawArrow(pos_id, UP);
      break;
    }
    case HLP_TRACKBALL: {
      uchar col[3], col2[3];
      UI_GetThemeColor3ubv(TH_GRID, col);
      UI_make_axis_color(col, col2, 'X');
      immUniformColor3ubv(col2);
      drawArrow(pos_id, RIGHT);
      drawArrow(pos_id, LEFT);
      UI_make_axis_color(col, col2, 'Y');
      immUniformColor3ubv(col2);
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      break;
    }
    case HLP_NONE:
      break;
  }

  GPU_matrix_pop();
  immUnbindProgram();
  GPU_line_smooth(false);
  GPU_blend(GPU_BLEND_NONE);
}
