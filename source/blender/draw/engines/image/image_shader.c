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
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BLI_dynstr.h"

#include "GPU_batch.h"

#include "image_engine.h"
#include "image_private.h"

extern char datatoc_common_colormanagement_lib_glsl[];
extern char datatoc_common_globals_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];

extern char datatoc_engine_image_frag_glsl[];
extern char datatoc_engine_image_vert_glsl[];

typedef struct IMAGE_Shaders {
  GPUShader *image_sh[2];
} IMAGE_Shaders;

static struct {
  IMAGE_Shaders shaders;
  DRWShaderLibrary *lib;
} e_data = {{{0}}}; /* Engine data */

void IMAGE_shader_library_ensure(void)
{
  if (e_data.lib == NULL) {
    e_data.lib = DRW_shader_library_create();
    /* NOTE: These need to be ordered by dependencies. */
    DRW_SHADER_LIB_ADD(e_data.lib, common_colormanagement_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_globals_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_view_lib);
  }
}

GPUShader *IMAGE_shader_image_get(bool is_tiled_image)
{
  const int index = is_tiled_image ? 1 : 0;
  IMAGE_Shaders *sh_data = &e_data.shaders;
  if (!sh_data->image_sh[index]) {
    sh_data->image_sh[index] = DRW_shader_create_with_shaderlib(
        datatoc_engine_image_vert_glsl,
        NULL,
        datatoc_engine_image_frag_glsl,
        e_data.lib,
        is_tiled_image ? "#define TILED_IMAGE\n" : NULL);
  }
  return sh_data->image_sh[index];
}

void IMAGE_shader_free(void)
{
  GPUShader **sh_data_as_array = (GPUShader **)&e_data.shaders;
  for (int i = 0; i < (sizeof(IMAGE_Shaders) / sizeof(GPUShader *)); i++) {
    DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
  }

  DRW_SHADER_LIB_FREE_SAFE(e_data.lib);
}
