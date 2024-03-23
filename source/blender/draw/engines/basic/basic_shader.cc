/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.hh"

#include "GPU_shader.hh"

#include "basic_private.h"

extern "C" char datatoc_basic_depth_frag_glsl[];
extern "C" char datatoc_basic_depth_vert_glsl[];
extern "C" char datatoc_basic_conservative_depth_geom_glsl[];

extern "C" char datatoc_common_view_lib_glsl[];
extern "C" char datatoc_common_pointcloud_lib_glsl[];

/* Shaders */

struct BASIC_Shaders {
  /* Depth Pre Pass */
  GPUShader *depth;
  GPUShader *pointcloud_depth;
  GPUShader *curves_depth;
  GPUShader *depth_conservative;
  GPUShader *pointcloud_depth_conservative;
};

static struct {
  BASIC_Shaders sh_data[GPU_SHADER_CFG_LEN];
} e_data = {{{nullptr}}}; /* Engine data */

GPUShader *BASIC_shaders_depth_sh_get(eGPUShaderConfig config)
{
  BASIC_Shaders *sh_data = &e_data.sh_data[config];
  if (sh_data->depth == nullptr) {
    sh_data->depth = GPU_shader_create_from_info_name(
        config == GPU_SHADER_CFG_CLIPPED ? "basic_depth_mesh_clipped" : "basic_depth_mesh");
  }
  return sh_data->depth;
}

GPUShader *BASIC_shaders_pointcloud_depth_sh_get(eGPUShaderConfig config)
{
  BASIC_Shaders *sh_data = &e_data.sh_data[config];
  if (sh_data->pointcloud_depth == nullptr) {
    sh_data->pointcloud_depth = GPU_shader_create_from_info_name(
        config == GPU_SHADER_CFG_CLIPPED ? "basic_depth_pointcloud_clipped" :
                                           "basic_depth_pointcloud");
  }
  return sh_data->pointcloud_depth;
}

GPUShader *BASIC_shaders_curves_depth_sh_get(eGPUShaderConfig config)
{
  BASIC_Shaders *sh_data = &e_data.sh_data[config];
  if (sh_data->curves_depth == nullptr) {
    sh_data->curves_depth = GPU_shader_create_from_info_name(
        config == GPU_SHADER_CFG_CLIPPED ? "basic_depth_curves_clipped" : "basic_depth_curves");
  }
  return sh_data->curves_depth;
}

GPUShader *BASIC_shaders_depth_conservative_sh_get(eGPUShaderConfig config)
{
  BASIC_Shaders *sh_data = &e_data.sh_data[config];
  if (sh_data->depth_conservative == nullptr) {
    sh_data->depth_conservative = GPU_shader_create_from_info_name(
        config == GPU_SHADER_CFG_CLIPPED ? "basic_depth_mesh_conservative_clipped" :
                                           "basic_depth_mesh_conservative");
  }
  return sh_data->depth_conservative;
}

GPUShader *BASIC_shaders_pointcloud_depth_conservative_sh_get(eGPUShaderConfig config)
{
  BASIC_Shaders *sh_data = &e_data.sh_data[config];
  if (sh_data->pointcloud_depth_conservative == nullptr) {
    sh_data->pointcloud_depth_conservative = GPU_shader_create_from_info_name(
        config == GPU_SHADER_CFG_CLIPPED ? "basic_depth_pointcloud_conservative_clipped" :
                                           "basic_depth_pointcloud_conservative");
  }
  return sh_data->pointcloud_depth_conservative;
}

void BASIC_shaders_free()
{
  for (int i = 0; i < GPU_SHADER_CFG_LEN; i++) {
    GPUShader **sh_data_as_array = (GPUShader **)&e_data.sh_data[i];
    for (int j = 0; j < (sizeof(BASIC_Shaders) / sizeof(GPUShader *)); j++) {
      DRW_SHADER_FREE_SAFE(sh_data_as_array[j]);
    }
  }
}
