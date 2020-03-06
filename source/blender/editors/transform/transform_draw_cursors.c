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

struct ArrowDims {
  int offset;
  int length;
  int size;
};

#define POS_INDEX 0
/* NOTE: this --^ is a bit hackish, but simplifies GPUVertFormat usage among functions
 * private to this file  - merwin
 */

static void drawArrow(enum eArrowDirection dir, const struct ArrowDims *arrow_dims)
{
  int offset = arrow_dims->offset;
  int length = arrow_dims->length;
  int size = arrow_dims->size;

  immBegin(GPU_PRIM_LINES, 6);

  switch (dir) {
    case LEFT:
      offset = -offset;
      length = -length;
      size = -size;
      ATTR_FALLTHROUGH;
    case RIGHT:
      immVertex2f(POS_INDEX, offset, 0);
      immVertex2f(POS_INDEX, offset + length, 0);
      immVertex2f(POS_INDEX, offset + length, 0);
      immVertex2f(POS_INDEX, offset + length - size, -size);
      immVertex2f(POS_INDEX, offset + length, 0);
      immVertex2f(POS_INDEX, offset + length - size, size);
      break;

    case DOWN:
      offset = -offset;
      length = -length;
      size = -size;
      ATTR_FALLTHROUGH;
    case UP:
      immVertex2f(POS_INDEX, 0, offset);
      immVertex2f(POS_INDEX, 0, offset + length);
      immVertex2f(POS_INDEX, 0, offset + length);
      immVertex2f(POS_INDEX, -size, offset + length - size);
      immVertex2f(POS_INDEX, 0, offset + length);
      immVertex2f(POS_INDEX, size, offset + length - size);
      break;
  }

  immEnd();
}

static void drawArrowHead(enum eArrowDirection dir, int size)
{
  immBegin(GPU_PRIM_LINES, 4);

  switch (dir) {
    case LEFT:
      size = -size;
      ATTR_FALLTHROUGH;
    case RIGHT:
      immVertex2f(POS_INDEX, 0, 0);
      immVertex2f(POS_INDEX, -size, -size);
      immVertex2f(POS_INDEX, 0, 0);
      immVertex2f(POS_INDEX, -size, size);
      break;

    case DOWN:
      size = -size;
      ATTR_FALLTHROUGH;
    case UP:
      immVertex2f(POS_INDEX, 0, 0);
      immVertex2f(POS_INDEX, -size, -size);
      immVertex2f(POS_INDEX, 0, 0);
      immVertex2f(POS_INDEX, size, -size);
      break;
  }

  immEnd();
}

static void drawArc(float angle_start, float angle_end, int segments, float size)
{
  float delta = (angle_end - angle_start) / segments;
  float angle;
  int a;

  immBegin(GPU_PRIM_LINE_STRIP, segments + 1);

  for (angle = angle_start, a = 0; a < segments; angle += delta, a++) {
    immVertex2f(POS_INDEX, cosf(angle) * size, sinf(angle) * size);
  }
  immVertex2f(POS_INDEX, cosf(angle_end) * size, sinf(angle_end) * size);

  immEnd();
}

/**
 * Poll callback for cursor drawing:
 * #WM_paint_cursor_activate
 */
bool transform_draw_cursor_poll(bContext *C)
{
  ARegion *region = CTX_wm_region(C);

  if (region && region->regiontype == RGN_TYPE_WINDOW) {
    return 1;
  }
  return 0;
}

/**
 * Cursor and help-line drawing, callback for:
 * #WM_paint_cursor_activate
 */
void transform_draw_cursor_draw(bContext *UNUSED(C), int x, int y, void *customdata)
{
  TransInfo *t = (TransInfo *)customdata;

  if (t->helpline != HLP_NONE) {
    struct ArrowDims arrow_dims = {
        .offset = 5 * UI_DPI_FAC,
        .length = 10 * UI_DPI_FAC,
        .size = 5 * UI_DPI_FAC,
    };

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

    GPU_line_smooth(true);
    GPU_blend(true);

    GPU_matrix_push();

    /* Dashed lines first. */
    if (ELEM(t->helpline, HLP_SPRING, HLP_ANGLE)) {
      const uint shdr_pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

      UNUSED_VARS_NDEBUG(shdr_pos); /* silence warning */
      BLI_assert(shdr_pos == POS_INDEX);

      GPU_line_width(1.0f);

      immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

      float viewport_size[4];
      GPU_viewport_size_get_f(viewport_size);
      immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

      immUniform1i("colors_len", 0); /* "simple" mode */
      immUniformThemeColor3(TH_VIEW_OVERLAY);
      immUniform1f("dash_width", 6.0f * UI_DPI_FAC);
      immUniform1f("dash_factor", 0.5f);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2fv(POS_INDEX, cent);
      immVertex2f(POS_INDEX, tmval[0], tmval[1]);
      immEnd();

      immUnbindProgram();
    }

    /* And now, solid lines. */
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    UNUSED_VARS_NDEBUG(pos); /* silence warning */
    BLI_assert(pos == POS_INDEX);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    switch (t->helpline) {
      case HLP_SPRING:
        immUniformThemeColor3(TH_VIEW_OVERLAY);

        GPU_matrix_translate_3fv(mval);
        GPU_matrix_rotate_axis(-RAD2DEGF(atan2f(cent[0] - tmval[0], cent[1] - tmval[1])), 'Z');

        GPU_line_width(3.0f);
        drawArrow(UP, &arrow_dims);
        drawArrow(DOWN, &arrow_dims);
        break;
      case HLP_HARROW:
        immUniformThemeColor3(TH_VIEW_OVERLAY);
        GPU_matrix_translate_3fv(mval);

        GPU_line_width(3.0f);
        drawArrow(RIGHT, &arrow_dims);
        drawArrow(LEFT, &arrow_dims);
        break;
      case HLP_VARROW:
        immUniformThemeColor3(TH_VIEW_OVERLAY);

        GPU_matrix_translate_3fv(mval);

        GPU_line_width(3.0f);
        drawArrow(UP, &arrow_dims);
        drawArrow(DOWN, &arrow_dims);
        break;
      case HLP_CARROW: {
        /* Draw arrow based on direction defined by custom-points. */
        immUniformThemeColor3(TH_VIEW_OVERLAY);

        GPU_matrix_translate_3fv(mval);

        GPU_line_width(3.0f);

        const int *data = t->mouse.data;
        const float dx = data[2] - data[0], dy = data[3] - data[1];
        const float angle = -atan2f(dx, dy);

        GPU_matrix_push();

        GPU_matrix_rotate_axis(RAD2DEGF(angle), 'Z');

        drawArrow(UP, &arrow_dims);
        drawArrow(DOWN, &arrow_dims);

        GPU_matrix_pop();
        break;
      }
      case HLP_ANGLE: {
        float dx = tmval[0] - cent[0], dy = tmval[1] - cent[1];
        float angle = atan2f(dy, dx);
        float dist = hypotf(dx, dy);
        float delta_angle = min_ff(15.0f / (dist / UI_DPI_FAC), (float)M_PI / 4.0f);
        float spacing_angle = min_ff(5.0f / (dist / UI_DPI_FAC), (float)M_PI / 12.0f);

        immUniformThemeColor3(TH_VIEW_OVERLAY);

        GPU_matrix_translate_3f(cent[0] - tmval[0] + mval[0], cent[1] - tmval[1] + mval[1], 0);

        GPU_line_width(3.0f);
        drawArc(angle - delta_angle, angle - spacing_angle, 10, dist);
        drawArc(angle + spacing_angle, angle + delta_angle, 10, dist);

        GPU_matrix_push();

        GPU_matrix_translate_3f(
            cosf(angle - delta_angle) * dist, sinf(angle - delta_angle) * dist, 0);
        GPU_matrix_rotate_axis(RAD2DEGF(angle - delta_angle), 'Z');

        drawArrowHead(DOWN, arrow_dims.size);

        GPU_matrix_pop();

        GPU_matrix_translate_3f(
            cosf(angle + delta_angle) * dist, sinf(angle + delta_angle) * dist, 0);
        GPU_matrix_rotate_axis(RAD2DEGF(angle + delta_angle), 'Z');

        drawArrowHead(UP, arrow_dims.size);
        break;
      }
      case HLP_TRACKBALL: {
        unsigned char col[3], col2[3];
        UI_GetThemeColor3ubv(TH_GRID, col);

        GPU_matrix_translate_3fv(mval);

        GPU_line_width(3.0f);

        UI_make_axis_color(col, col2, 'X');
        immUniformColor3ubv(col2);

        drawArrow(RIGHT, &arrow_dims);
        drawArrow(LEFT, &arrow_dims);

        UI_make_axis_color(col, col2, 'Y');
        immUniformColor3ubv(col2);

        drawArrow(UP, &arrow_dims);
        drawArrow(DOWN, &arrow_dims);
        break;
      }
    }

    immUnbindProgram();
    GPU_matrix_pop();

    GPU_line_smooth(false);
    GPU_blend(false);
  }
}
