/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup eduv
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "ED_uvedit.h"

/* ------------------------- */

void ED_image_draw_cursor(ARegion *region, const float cursor[2])
{
  float zoom[2], x_fac, y_fac;

  UI_view2d_scale_get_inverse(&region->v2d, &zoom[0], &zoom[1]);

  mul_v2_fl(zoom, 256.0f * UI_SCALE_FAC);
  x_fac = zoom[0];
  y_fac = zoom[1];

  GPU_line_width(1.0f);

  GPU_matrix_translate_2fv(cursor);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniform4f("color", 1.0f, 0.0f, 0.0f, 1.0f);
  immUniform4f("color2", 1.0f, 1.0f, 1.0f, 1.0f);
  immUniform1f("dash_width", 8.0f);
  immUniform1f("udash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 8);

  immVertex2f(shdr_pos, -0.05f * x_fac, 0.0f);
  immVertex2f(shdr_pos, 0.0f, 0.05f * y_fac);

  immVertex2f(shdr_pos, 0.0f, 0.05f * y_fac);
  immVertex2f(shdr_pos, 0.05f * x_fac, 0.0f);

  immVertex2f(shdr_pos, 0.05f * x_fac, 0.0f);
  immVertex2f(shdr_pos, 0.0f, -0.05f * y_fac);

  immVertex2f(shdr_pos, 0.0f, -0.05f * y_fac);
  immVertex2f(shdr_pos, -0.05f * x_fac, 0.0f);

  immEnd();

  immUniform4f("color", 1.0f, 1.0f, 1.0f, 1.0f);
  immUniform4f("color2", 0.0f, 0.0f, 0.0f, 1.0f);
  immUniform1f("dash_width", 2.0f);
  immUniform1f("udash_factor", 0.5f);

  immBegin(GPU_PRIM_LINES, 8);

  immVertex2f(shdr_pos, -0.020f * x_fac, 0.0f);
  immVertex2f(shdr_pos, -0.1f * x_fac, 0.0f);

  immVertex2f(shdr_pos, 0.1f * x_fac, 0.0f);
  immVertex2f(shdr_pos, 0.020f * x_fac, 0.0f);

  immVertex2f(shdr_pos, 0.0f, -0.020f * y_fac);
  immVertex2f(shdr_pos, 0.0f, -0.1f * y_fac);

  immVertex2f(shdr_pos, 0.0f, 0.1f * y_fac);
  immVertex2f(shdr_pos, 0.0f, 0.020f * y_fac);

  immEnd();

  immUnbindProgram();

  GPU_matrix_translate_2f(-cursor[0], -cursor[1]);
}
