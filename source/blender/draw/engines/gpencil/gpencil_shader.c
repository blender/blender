/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. */

/** \file
 * \ingroup draw
 */
#include "DRW_render.h"

#include "gpencil_engine.h"

extern char datatoc_gpencil_common_lib_glsl[];
extern char datatoc_gpencil_frag_glsl[];
extern char datatoc_gpencil_vert_glsl[];
extern char datatoc_gpencil_antialiasing_frag_glsl[];
extern char datatoc_gpencil_antialiasing_vert_glsl[];
extern char datatoc_gpencil_layer_blend_frag_glsl[];
extern char datatoc_gpencil_mask_invert_frag_glsl[];
extern char datatoc_gpencil_depth_merge_frag_glsl[];
extern char datatoc_gpencil_depth_merge_vert_glsl[];
extern char datatoc_gpencil_vfx_frag_glsl[];

extern char datatoc_common_colormanagement_lib_glsl[];
extern char datatoc_common_fullscreen_vert_glsl[];
extern char datatoc_common_view_lib_glsl[];

static struct {
  /* SMAA antialiasing */
  GPUShader *antialiasing_sh[3];
  /* GPencil Object rendering */
  GPUShader *gpencil_sh;
  /* Final Compositing over rendered background. */
  GPUShader *composite_sh;
  /* All layer blend types in one shader! */
  GPUShader *layer_blend_sh;
  /* Merge the final object depth to the depth buffer. */
  GPUShader *depth_merge_sh;
  /* Invert the content of the mask buffer. */
  GPUShader *mask_invert_sh;
  /* Effects. */
  GPUShader *fx_composite_sh;
  GPUShader *fx_colorize_sh;
  GPUShader *fx_blur_sh;
  GPUShader *fx_glow_sh;
  GPUShader *fx_pixel_sh;
  GPUShader *fx_rim_sh;
  GPUShader *fx_shadow_sh;
  GPUShader *fx_transform_sh;
} g_shaders = {{NULL}};

void GPENCIL_shader_free(void)
{
  DRW_SHADER_FREE_SAFE(g_shaders.antialiasing_sh[0]);
  DRW_SHADER_FREE_SAFE(g_shaders.antialiasing_sh[1]);
  DRW_SHADER_FREE_SAFE(g_shaders.antialiasing_sh[2]);
  DRW_SHADER_FREE_SAFE(g_shaders.gpencil_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.composite_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.layer_blend_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.depth_merge_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.mask_invert_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.fx_composite_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.fx_colorize_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.fx_blur_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.fx_glow_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.fx_pixel_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.fx_rim_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.fx_shadow_sh);
  DRW_SHADER_FREE_SAFE(g_shaders.fx_transform_sh);
}

GPUShader *GPENCIL_shader_antialiasing(int stage)
{
  BLI_assert(stage < 3);

  if (!g_shaders.antialiasing_sh[stage]) {
    char stage_info_name[32];
    SNPRINTF(stage_info_name, "gpencil_antialiasing_stage_%d", stage);
    g_shaders.antialiasing_sh[stage] = GPU_shader_create_from_info_name(stage_info_name);
  }
  return g_shaders.antialiasing_sh[stage];
}

GPUShader *GPENCIL_shader_geometry_get(void)
{
  if (!g_shaders.gpencil_sh) {
    g_shaders.gpencil_sh = GPU_shader_create_from_info_name("gpencil_geometry");
  }
  return g_shaders.gpencil_sh;
}

GPUShader *GPENCIL_shader_layer_blend_get(void)
{
  if (!g_shaders.layer_blend_sh) {
    g_shaders.layer_blend_sh = GPU_shader_create_from_info_name("gpencil_layer_blend");
  }
  return g_shaders.layer_blend_sh;
}

GPUShader *GPENCIL_shader_mask_invert_get(void)
{
  if (!g_shaders.mask_invert_sh) {
    g_shaders.mask_invert_sh = GPU_shader_create_from_info_name("gpencil_mask_invert");
  }
  return g_shaders.mask_invert_sh;
}

GPUShader *GPENCIL_shader_depth_merge_get(void)
{
  if (!g_shaders.depth_merge_sh) {
    g_shaders.depth_merge_sh = GPU_shader_create_from_info_name("gpencil_depth_merge");
  }
  return g_shaders.depth_merge_sh;
}

/* ------- FX Shaders --------- */

GPUShader *GPENCIL_shader_fx_blur_get(void)
{
  if (!g_shaders.fx_blur_sh) {
    g_shaders.fx_blur_sh = GPU_shader_create_from_info_name("gpencil_fx_blur");
  }
  return g_shaders.fx_blur_sh;
}

GPUShader *GPENCIL_shader_fx_colorize_get(void)
{
  if (!g_shaders.fx_colorize_sh) {
    g_shaders.fx_colorize_sh = GPU_shader_create_from_info_name("gpencil_fx_colorize");
  }
  return g_shaders.fx_colorize_sh;
}

GPUShader *GPENCIL_shader_fx_composite_get(void)
{
  if (!g_shaders.fx_composite_sh) {
    g_shaders.fx_composite_sh = GPU_shader_create_from_info_name("gpencil_fx_composite");
  }
  return g_shaders.fx_composite_sh;
}

GPUShader *GPENCIL_shader_fx_glow_get(void)
{
  if (!g_shaders.fx_glow_sh) {
    g_shaders.fx_glow_sh = GPU_shader_create_from_info_name("gpencil_fx_glow");
  }
  return g_shaders.fx_glow_sh;
}

GPUShader *GPENCIL_shader_fx_pixelize_get(void)
{
  if (!g_shaders.fx_pixel_sh) {
    g_shaders.fx_pixel_sh = GPU_shader_create_from_info_name("gpencil_fx_pixelize");
  }
  return g_shaders.fx_pixel_sh;
}

GPUShader *GPENCIL_shader_fx_rim_get(void)
{
  if (!g_shaders.fx_rim_sh) {
    g_shaders.fx_rim_sh = GPU_shader_create_from_info_name("gpencil_fx_rim");
  }
  return g_shaders.fx_rim_sh;
}

GPUShader *GPENCIL_shader_fx_shadow_get(void)
{
  if (!g_shaders.fx_shadow_sh) {
    g_shaders.fx_shadow_sh = GPU_shader_create_from_info_name("gpencil_fx_shadow");
  }
  return g_shaders.fx_shadow_sh;
}

GPUShader *GPENCIL_shader_fx_transform_get(void)
{
  if (!g_shaders.fx_transform_sh) {
    g_shaders.fx_transform_sh = GPU_shader_create_from_info_name("gpencil_fx_transform");
  }
  return g_shaders.fx_transform_sh;
}
