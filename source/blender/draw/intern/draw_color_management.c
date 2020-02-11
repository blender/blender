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
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#include <stdio.h>

#include "draw_manager.h"

#include "DRW_render.h"

#include "GPU_batch.h"
#include "GPU_framebuffer.h"
#include "GPU_matrix.h"
#include "GPU_texture.h"

#include "BKE_colortools.h"

#include "IMB_colormanagement.h"

#include "draw_color_management.h"

/* -------------------------------------------------------------------- */
/** \name Color Management
 * \{ */

/* Draw texture to framebuffer without any color transforms */
void DRW_transform_none(GPUTexture *tex)
{
  drw_state_set(DRW_STATE_WRITE_COLOR);

  GPU_matrix_identity_set();
  GPU_matrix_identity_projection_set();

  /* Draw as texture for final render (without immediate mode). */
  GPUBatch *geom = DRW_cache_fullscreen_quad_get();
  GPU_batch_program_set_builtin(geom, GPU_SHADER_2D_IMAGE_COLOR);
  GPU_batch_uniform_4f(geom, "color", 1.0f, 1.0f, 1.0f, 1.0f);
  GPU_batch_uniform_1i(geom, "image", 0);

  GPU_texture_bind(tex, 0);
  GPU_batch_draw(geom);
  GPU_texture_unbind(tex);
}

/** \} */
