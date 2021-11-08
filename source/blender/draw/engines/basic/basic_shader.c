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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "GPU_shader.h"

#include "basic_private.h"

extern char datatoc_depth_frag_glsl[];
extern char datatoc_depth_vert_glsl[];
extern char datatoc_conservative_depth_geom_glsl[];

extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_pointcloud_lib_glsl[];

/* Shaders */

typedef struct BASIC_Shaders {
  /* Depth Pre Pass */
  struct GPUShader *depth;
  struct GPUShader *pointcloud_depth;
  struct GPUShader *depth_conservative;
  struct GPUShader *pointcloud_depth_conservative;
} BASIC_Shaders;

static struct {
  BASIC_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{NULL}}}; /* Engine data */

static GPUShader *BASIC_shader_create_depth_sh(const GPUShaderConfigData *sh_cfg)
{
  return GPU_shader_create_from_arrays({
      .vert = (const char *[]){sh_cfg->lib,
                               datatoc_common_view_lib_glsl,
                               datatoc_depth_vert_glsl,
                               NULL},
      .frag = (const char *[]){datatoc_depth_frag_glsl, NULL},
      .defs = (const char *[]){sh_cfg->def, NULL},
  });
}

static GPUShader *BASIC_shader_create_pointcloud_depth_sh(const GPUShaderConfigData *sh_cfg)
{
  return GPU_shader_create_from_arrays({
      .vert = (const char *[]){sh_cfg->lib,
                               datatoc_common_view_lib_glsl,
                               datatoc_common_pointcloud_lib_glsl,
                               datatoc_depth_vert_glsl,
                               NULL},
      .frag = (const char *[]){datatoc_depth_frag_glsl, NULL},
      .defs = (const char *[]){sh_cfg->def,
                               "#define POINTCLOUD\n",
                               "#define INSTANCED_ATTR\n",
                               "#define UNIFORM_RESOURCE_ID\n",
                               NULL},
  });
}

static GPUShader *BASIC_shader_create_depth_conservative_sh(const GPUShaderConfigData *sh_cfg)
{
  return GPU_shader_create_from_arrays({
      .vert = (const char *[]){sh_cfg->lib,
                               datatoc_common_view_lib_glsl,
                               datatoc_depth_vert_glsl,
                               NULL},
      .geom = (const char *[]){sh_cfg->lib,
                               datatoc_common_view_lib_glsl,
                               datatoc_conservative_depth_geom_glsl,
                               NULL},
      .frag = (const char *[]){datatoc_depth_frag_glsl, NULL},
      .defs = (const char *[]){sh_cfg->def, "#define CONSERVATIVE_RASTER\n", NULL},
  });
}

static GPUShader *BASIC_shader_create_pointcloud_depth_conservative_sh(
    const GPUShaderConfigData *sh_cfg)
{
  return GPU_shader_create_from_arrays({
      .vert = (const char *[]){sh_cfg->lib,
                               datatoc_common_view_lib_glsl,
                               datatoc_common_pointcloud_lib_glsl,
                               datatoc_depth_vert_glsl,
                               NULL},
      .geom = (const char *[]){sh_cfg->lib,
                               datatoc_common_view_lib_glsl,
                               datatoc_conservative_depth_geom_glsl,
                               NULL},
      .frag = (const char *[]){datatoc_depth_frag_glsl, NULL},
      .defs = (const char *[]){sh_cfg->def,
                               "#define CONSERVATIVE_RASTER\n",
                               "#define POINTCLOUD\n",
                               "#define INSTANCED_ATTR\n",
                               "#define UNIFORM_RESOURCE_ID\n",
                               NULL},
  });
}

GPUShader *BASIC_shaders_depth_sh_get(eGPUShaderConfig config)
{
  BASIC_Shaders *sh_data = &e_data.sh_data[config];
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[config];
  if (sh_data->depth == NULL) {
    sh_data->depth = BASIC_shader_create_depth_sh(sh_cfg);
  }
  return sh_data->depth;
}

GPUShader *BASIC_shaders_pointcloud_depth_sh_get(eGPUShaderConfig config)
{
  BASIC_Shaders *sh_data = &e_data.sh_data[config];
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[config];
  if (sh_data->pointcloud_depth == NULL) {
    sh_data->pointcloud_depth = BASIC_shader_create_pointcloud_depth_sh(sh_cfg);
  }
  return sh_data->pointcloud_depth;
}

GPUShader *BASIC_shaders_depth_conservative_sh_get(eGPUShaderConfig config)
{
  BASIC_Shaders *sh_data = &e_data.sh_data[config];
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[config];
  if (sh_data->depth_conservative == NULL) {
    sh_data->depth_conservative = BASIC_shader_create_depth_conservative_sh(sh_cfg);
  }
  return sh_data->depth_conservative;
}

GPUShader *BASIC_shaders_pointcloud_depth_conservative_sh_get(eGPUShaderConfig config)
{
  BASIC_Shaders *sh_data = &e_data.sh_data[config];
  const GPUShaderConfigData *sh_cfg = &GPU_shader_cfg_data[config];
  if (sh_data->pointcloud_depth_conservative == NULL) {
    sh_data->pointcloud_depth_conservative = BASIC_shader_create_pointcloud_depth_conservative_sh(
        sh_cfg);
  }
  return sh_data->pointcloud_depth_conservative;
}

void BASIC_shaders_free(void)
{
  for (int i = 0; i < GPU_SHADER_CFG_LEN; i++) {
    GPUShader **sh_data_as_array = (GPUShader **)&e_data.sh_data[i];
    for (int j = 0; j < (sizeof(BASIC_Shaders) / sizeof(GPUShader *)); j++) {
      DRW_SHADER_FREE_SAFE(sh_data_as_array[j]);
    }
  }
}
