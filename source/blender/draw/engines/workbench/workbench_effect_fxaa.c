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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */
#include "workbench_private.h"

/* *********** STATIC *********** */
static struct {
  struct GPUShader *effect_fxaa_sh;
} e_data = {NULL};

/* Shaders */
extern char datatoc_common_fxaa_lib_glsl[];
extern char datatoc_common_fullscreen_vert_glsl[];
extern char datatoc_workbench_effect_fxaa_frag_glsl[];

/* *********** Functions *********** */
void workbench_fxaa_engine_init(void)
{
  if (e_data.effect_fxaa_sh == NULL) {
    e_data.effect_fxaa_sh = DRW_shader_create_with_lib(datatoc_common_fullscreen_vert_glsl,
                                                       NULL,
                                                       datatoc_workbench_effect_fxaa_frag_glsl,
                                                       datatoc_common_fxaa_lib_glsl,
                                                       NULL);
  }
}

DRWPass *workbench_fxaa_create_pass(GPUTexture **color_buffer_tx)
{
  DRWPass *pass = DRW_pass_create("Effect FXAA", DRW_STATE_WRITE_COLOR);
  DRWShadingGroup *grp = DRW_shgroup_create(e_data.effect_fxaa_sh, pass);
  DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", color_buffer_tx);
  DRW_shgroup_uniform_vec2(grp, "invertedViewportSize", DRW_viewport_invert_size_get(), 1);
  DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
  return pass;
}

void workbench_fxaa_engine_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.effect_fxaa_sh);
}
