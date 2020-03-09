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
extern char datatoc_common_smaa_lib_glsl[];
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
  /* general drawing shaders */
  GPUShader *gpencil_fill_sh;
  GPUShader *gpencil_stroke_sh;
  GPUShader *gpencil_point_sh;
  GPUShader *gpencil_edit_point_sh;
  GPUShader *gpencil_line_sh;
  GPUShader *gpencil_drawing_fill_sh;
  GPUShader *gpencil_fullscreen_sh;
  GPUShader *gpencil_simple_fullscreen_sh;
  GPUShader *gpencil_blend_fullscreen_sh;
  GPUShader *gpencil_background_sh;
  GPUShader *gpencil_paper_sh;
} g_shaders = {{NULL}};

void GPENCIL_shader_free(void)
{
  GPUShader **sh_data_as_array = (GPUShader **)&g_shaders;
  for (int i = 0; i < (sizeof(g_shaders) / sizeof(GPUShader *)); i++) {
    DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
  }
}

GPUShader *GPENCIL_shader_antialiasing(int stage)
{
  BLI_assert(stage < 3);

  if (!g_shaders.antialiasing_sh[stage]) {
    char stage_define[32];
    BLI_snprintf(stage_define, sizeof(stage_define), "#define SMAA_STAGE %d\n", stage);

    g_shaders.antialiasing_sh[stage] = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){
                "#define SMAA_INCLUDE_VS 1\n",
                "#define SMAA_INCLUDE_PS 0\n",
                "uniform vec4 viewportMetrics;\n",
                datatoc_common_smaa_lib_glsl,
                datatoc_gpencil_antialiasing_vert_glsl,
                NULL,
            },
        .frag =
            (const char *[]){
                "#define SMAA_INCLUDE_VS 0\n",
                "#define SMAA_INCLUDE_PS 1\n",
                "uniform vec4 viewportMetrics;\n",
                datatoc_common_smaa_lib_glsl,
                datatoc_gpencil_antialiasing_frag_glsl,
                NULL,
            },
        .defs =
            (const char *[]){
                "#define SMAA_GLSL_3\n",
                "#define SMAA_RT_METRICS viewportMetrics\n",
                "#define SMAA_PRESET_HIGH\n",
                "#define SMAA_LUMA_WEIGHT float4(1.0, 1.0, 1.0, 0.0)\n",
                "#define SMAA_NO_DISCARD\n",
                stage_define,
                NULL,
            },
    });
  }
  return g_shaders.antialiasing_sh[stage];
}

GPUShader *GPENCIL_shader_geometry_get(void)
{
  if (!g_shaders.gpencil_sh) {
    g_shaders.gpencil_sh = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){
                datatoc_common_view_lib_glsl,
                datatoc_gpencil_common_lib_glsl,
                datatoc_gpencil_vert_glsl,
                NULL,
            },
        .frag =
            (const char *[]){
                datatoc_common_colormanagement_lib_glsl,
                datatoc_gpencil_common_lib_glsl,
                datatoc_gpencil_frag_glsl,
                NULL,
            },
        .defs =
            (const char *[]){
                "#define GP_MATERIAL_BUFFER_LEN " STRINGIFY(GP_MATERIAL_BUFFER_LEN) "\n",
                "#define GPENCIL_LIGHT_BUFFER_LEN " STRINGIFY(GPENCIL_LIGHT_BUFFER_LEN) "\n",
                "#define UNIFORM_RESOURCE_ID\n",
                NULL,
            },
    });
  }
  return g_shaders.gpencil_sh;
}

GPUShader *GPENCIL_shader_layer_blend_get(void)
{
  if (!g_shaders.layer_blend_sh) {
    g_shaders.layer_blend_sh = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){
                datatoc_common_fullscreen_vert_glsl,
                NULL,
            },
        .frag =
            (const char *[]){
                datatoc_gpencil_common_lib_glsl,
                datatoc_gpencil_layer_blend_frag_glsl,
                NULL,
            },
    });
  }
  return g_shaders.layer_blend_sh;
}

GPUShader *GPENCIL_shader_mask_invert_get(void)
{
  if (!g_shaders.mask_invert_sh) {
    g_shaders.mask_invert_sh = DRW_shader_create_fullscreen(datatoc_gpencil_mask_invert_frag_glsl,
                                                            NULL);
  }
  return g_shaders.mask_invert_sh;
}

GPUShader *GPENCIL_shader_depth_merge_get(void)
{
  if (!g_shaders.depth_merge_sh) {
    g_shaders.depth_merge_sh = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){
                datatoc_common_view_lib_glsl,
                datatoc_gpencil_depth_merge_vert_glsl,
                NULL,
            },
        .frag =
            (const char *[]){
                datatoc_gpencil_depth_merge_frag_glsl,
                NULL,
            },
    });
  }
  return g_shaders.depth_merge_sh;
}

/* ------- FX Shaders --------- */

GPUShader *GPENCIL_shader_fx_blur_get(void)
{
  if (!g_shaders.fx_blur_sh) {
    g_shaders.fx_blur_sh = DRW_shader_create_fullscreen(datatoc_gpencil_vfx_frag_glsl,
                                                        "#define BLUR\n");
  }
  return g_shaders.fx_blur_sh;
}

GPUShader *GPENCIL_shader_fx_colorize_get(void)
{
  if (!g_shaders.fx_colorize_sh) {
    g_shaders.fx_colorize_sh = DRW_shader_create_fullscreen(datatoc_gpencil_vfx_frag_glsl,
                                                            "#define COLORIZE\n");
  }
  return g_shaders.fx_colorize_sh;
}

GPUShader *GPENCIL_shader_fx_composite_get(void)
{
  if (!g_shaders.fx_composite_sh) {
    g_shaders.fx_composite_sh = DRW_shader_create_fullscreen(datatoc_gpencil_vfx_frag_glsl,
                                                             "#define COMPOSITE\n");
  }
  return g_shaders.fx_composite_sh;
}

GPUShader *GPENCIL_shader_fx_glow_get(void)
{
  if (!g_shaders.fx_glow_sh) {
    g_shaders.fx_glow_sh = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){
                datatoc_common_fullscreen_vert_glsl,
                NULL,
            },
        .frag =
            (const char *[]){
                datatoc_gpencil_common_lib_glsl,
                datatoc_gpencil_vfx_frag_glsl,
                NULL,
            },
        .defs =
            (const char *[]){
                "#define GLOW\n",
                NULL,
            },
    });
  }
  return g_shaders.fx_glow_sh;
}

GPUShader *GPENCIL_shader_fx_pixelize_get(void)
{
  if (!g_shaders.fx_pixel_sh) {
    g_shaders.fx_pixel_sh = DRW_shader_create_fullscreen(datatoc_gpencil_vfx_frag_glsl,
                                                         "#define PIXELIZE\n");
  }
  return g_shaders.fx_pixel_sh;
}

GPUShader *GPENCIL_shader_fx_rim_get(void)
{
  if (!g_shaders.fx_rim_sh) {
    g_shaders.fx_rim_sh = GPU_shader_create_from_arrays({
        .vert =
            (const char *[]){
                datatoc_common_fullscreen_vert_glsl,
                NULL,
            },
        .frag =
            (const char *[]){
                datatoc_gpencil_common_lib_glsl,
                datatoc_gpencil_vfx_frag_glsl,
                NULL,
            },
        .defs =
            (const char *[]){
                "#define RIM\n",
                NULL,
            },
    });
  }
  return g_shaders.fx_rim_sh;
}

GPUShader *GPENCIL_shader_fx_shadow_get(void)
{
  if (!g_shaders.fx_shadow_sh) {
    g_shaders.fx_shadow_sh = DRW_shader_create_fullscreen(datatoc_gpencil_vfx_frag_glsl,
                                                          "#define SHADOW\n");
  }
  return g_shaders.fx_shadow_sh;
}

GPUShader *GPENCIL_shader_fx_transform_get(void)
{
  if (!g_shaders.fx_transform_sh) {
    g_shaders.fx_transform_sh = DRW_shader_create_fullscreen(datatoc_gpencil_vfx_frag_glsl,
                                                             "#define TRANSFORM\n");
  }
  return g_shaders.fx_transform_sh;
}
