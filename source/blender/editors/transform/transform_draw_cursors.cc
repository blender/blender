/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "BLI_math_rotation.h"

#include "BKE_context.hh"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "UI_resources.hh"

#include "transform.hh"
#include "transform_draw_cursors.hh" /* Own include. */

using namespace blender;

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
  int offset = 5.0f * UI_SCALE_FAC;
  int length = (6.0f * UI_SCALE_FAC) + (4.0f * U.pixelsize);
  int size = (3.0f * UI_SCALE_FAC) + (2.0f * U.pixelsize);

  /* To line up the arrow point nicely, one end has to be extended by half its width. But
   * being on a 45 degree angle, Pythagoras says a movement of `sqrt(2) / 2 * (line width / 2)`. */
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

bool transform_draw_cursor_poll(bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  return (region && ELEM(region->regiontype, RGN_TYPE_WINDOW, RGN_TYPE_PREVIEW)) ? true : false;
}

void transform_draw_cursor_draw(bContext *C, int x, int y, void *customdata)
{
  TransInfo *t = (TransInfo *)customdata;

  if (t->helpline == HLP_NONE) {
    return;
  }

  /* Offset the values for the area region. */
  const float2 offset = {
      float(t->region->winrct.xmin),
      float(t->region->winrct.ymin),
  };

  float2 cent;
  float2 tmval = t->mval;

  projectFloatViewEx(t, t->center_global, cent, V3D_PROJ_TEST_CLIP_ZERO);

  cent += offset;
  tmval += offset;

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);

  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  float fg_color[4];
  float bg_color[4];
  if (v3d && scene) {
    /* Use overlay colors for 3D Viewport. */
    ED_view3d_text_colors_get(scene, v3d, fg_color, bg_color);
  }
  else {
    /* Otherwise editor foreground and background colors. */
    UI_GetThemeColor3fv(TH_TEXT_HI, fg_color);
    UI_GetThemeColor3fv(TH_BACK, bg_color);
  }
  fg_color[3] = 1.0f;
  bg_color[3] = 0.5f;

  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);
  const uint pos_id = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  /* Dashed lines first. */
  if (ELEM(t->helpline, HLP_SPRING, HLP_ANGLE, HLP_ERROR_DASH)) {
    GPU_line_width(DASH_WIDTH);
    immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
    immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);
    immUniform1i("colors_len", 0); /* "simple" mode. */
    immUniform1f("dash_width", DASH_LENGTH);
    immUniform1f("udash_factor", 0.5f);

    /* Draw in background color first. */
    immUniformColor4fv(bg_color);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2fv(pos_id, cent);
    immVertex2f(pos_id, tmval[0], tmval[1]);
    immEnd();

    /* Then foreground over top, shifted slightly. */
    immUniformColor4fv(fg_color);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos_id, cent[0] - U.pixelsize, cent[1] + U.pixelsize);
    immVertex2f(pos_id, tmval[0] - U.pixelsize, tmval[1] + U.pixelsize);
    immEnd();

    immUnbindProgram();
  }

  /* And now, solid lines. */

  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
  immUniform2fv("viewportSize", &viewport_size[2]);

  /* First pass is background color and wider lines. */
  immUniformColor4fv(bg_color);
  immUniform1f("lineWidth", ARROW_WIDTH * 2.0f);

  GPU_matrix_push();
  GPU_matrix_translate_3f(float(x), float(y), 0.0f);

  switch (t->helpline) {
    case HLP_SPRING:
      GPU_matrix_rotate_axis(-RAD2DEGF(atan2f(cent[0] - tmval[0], cent[1] - tmval[1])), 'Z');
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      immUniformColor4fv(fg_color);
      immUniform1f("lineWidth", ARROW_WIDTH);
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      break;
    case HLP_HARROW:
      drawArrow(pos_id, RIGHT);
      drawArrow(pos_id, LEFT);
      immUniform1f("lineWidth", ARROW_WIDTH);
      immUniformColor4fv(fg_color);
      drawArrow(pos_id, RIGHT);
      drawArrow(pos_id, LEFT);
      break;
    case HLP_VARROW:
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      immUniform1f("lineWidth", ARROW_WIDTH);
      immUniformColor4fv(fg_color);
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      break;
    case HLP_CARROW: {
      /* Draw arrow based on direction defined by custom-points. */
      const int *data = static_cast<const int *>(t->mouse.data);
      const float angle = -atan2f(data[2] - data[0], data[3] - data[1]);
      GPU_matrix_rotate_axis(RAD2DEGF(angle), 'Z');
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      immUniform1f("lineWidth", ARROW_WIDTH);
      immUniformColor4fv(fg_color);
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      break;
    }
    case HLP_ANGLE: {
      GPU_matrix_push();
      float angle = atan2f(tmval[1] - cent[1], tmval[0] - cent[0]);
      GPU_matrix_translate_3f(cosf(angle), sinf(angle), 0);
      GPU_matrix_rotate_axis(RAD2DEGF(angle), 'Z');

      immUniform1f("lineWidth", ARROW_WIDTH * 2.0f);
      drawArrow(pos_id, DOWN);
      immUniformColor4fv(fg_color);
      immUniform1f("lineWidth", ARROW_WIDTH);
      drawArrow(pos_id, DOWN);

      GPU_matrix_pop();
      GPU_matrix_translate_3f(cosf(angle), sinf(angle), 0);
      GPU_matrix_rotate_axis(RAD2DEGF(angle), 'Z');
      immUniformColor4fv(bg_color);
      immUniform1f("lineWidth", ARROW_WIDTH * 2.0f);
      drawArrow(pos_id, UP);
      immUniformColor4fv(fg_color);
      immUniform1f("lineWidth", ARROW_WIDTH);
      drawArrow(pos_id, UP);
      break;
    }
    case HLP_TRACKBALL: {
      immUniformColor4fv(bg_color);
      GPU_matrix_translate_3f(U.pixelsize, -U.pixelsize, 0.0f);
      drawArrow(pos_id, RIGHT);
      drawArrow(pos_id, LEFT);
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      GPU_matrix_translate_3f(-U.pixelsize, U.pixelsize, 0.0f);

      immUniform1f("lineWidth", ARROW_WIDTH);
      uchar col[3], col2[3];
      UI_GetThemeColor3ubv(TH_GRID, col);
      UI_make_axis_color(col, 'X', col2);
      immUniformColor3ubv(col2);
      drawArrow(pos_id, RIGHT);
      drawArrow(pos_id, LEFT);
      UI_make_axis_color(col, 'Y', col2);
      immUniformColor3ubv(col2);
      drawArrow(pos_id, UP);
      drawArrow(pos_id, DOWN);
      break;
    }
    case HLP_ERROR:
    case HLP_ERROR_DASH:
    case HLP_NONE:
      break;
  }

  GPU_matrix_pop();
  immUnbindProgram();
  GPU_line_smooth(false);
  GPU_blend(GPU_BLEND_NONE);
}
