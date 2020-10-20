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

  mul_v2_fl(zoom, 256.0f * UI_DPI_FAC);
  x_fac = zoom[0];
  y_fac = zoom[1];

  GPU_line_width(1.0f);

  GPU_matrix_translate_2fv(cursor);

  const uint shdr_pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_DPI_FAC, viewport_size[3] / UI_DPI_FAC);

  immUniform1i("colors_len", 2); /* "advanced" mode */
  immUniformArray4fv(
      "colors", (float *)(float[][4]){{1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, 2);
  immUniform1f("dash_width", 8.0f);
  immUniform1f("dash_factor", 0.5f);

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

  immUniformArray4fv(
      "colors", (float *)(float[][4]){{1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}, 2);
  immUniform1f("dash_width", 2.0f);
  immUniform1f("dash_factor", 0.5f);

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
