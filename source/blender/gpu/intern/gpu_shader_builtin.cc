/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_utildefines.h"

#include "GPU_capabilities.h"
#include "GPU_shader.h"

/* Cache of built-in shaders (each is created on first use). */
static GPUShader *builtin_shaders[GPU_SHADER_CFG_LEN][GPU_SHADER_BUILTIN_LEN] = {{nullptr}};

static const char *builtin_shader_create_info_name(eGPUBuiltinShader shader)
{
  switch (shader) {
    case GPU_SHADER_TEXT:
      return "gpu_shader_text";
    case GPU_SHADER_KEYFRAME_SHAPE:
      return "gpu_shader_keyframe_shape";
    case GPU_SHADER_SIMPLE_LIGHTING:
      return "gpu_shader_simple_lighting";
    case GPU_SHADER_3D_IMAGE:
      return "gpu_shader_3D_image";
    case GPU_SHADER_3D_IMAGE_COLOR:
      return "gpu_shader_3D_image_color";
    case GPU_SHADER_2D_CHECKER:
      return "gpu_shader_2D_checker";
    case GPU_SHADER_2D_DIAG_STRIPES:
      return "gpu_shader_2D_diag_stripes";
    case GPU_SHADER_ICON:
      return "gpu_shader_icon";
    case GPU_SHADER_2D_IMAGE_OVERLAYS_MERGE:
      return "gpu_shader_2D_image_overlays_merge";
    case GPU_SHADER_2D_IMAGE_OVERLAYS_STEREO_MERGE:
      return "gpu_shader_2D_image_overlays_stereo_merge";
    case GPU_SHADER_2D_IMAGE_DESATURATE_COLOR:
      return "gpu_shader_2D_image_desaturate_color";
    case GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR:
      return "gpu_shader_2D_image_shuffle_color";
    case GPU_SHADER_2D_IMAGE_RECT_COLOR:
      return "gpu_shader_2D_image_rect_color";
    case GPU_SHADER_ICON_MULTI:
      return "gpu_shader_icon_multi";
    case GPU_SHADER_3D_UNIFORM_COLOR:
      return "gpu_shader_3D_uniform_color";
    case GPU_SHADER_3D_FLAT_COLOR:
      return "gpu_shader_3D_flat_color";
    case GPU_SHADER_3D_SMOOTH_COLOR:
      return "gpu_shader_3D_smooth_color";
    case GPU_SHADER_3D_DEPTH_ONLY:
      return "gpu_shader_3D_depth_only";
    case GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR:
      return "gpu_shader_3D_clipped_uniform_color";
    case GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR:
      return "gpu_shader_3D_polyline_uniform_color";
    case GPU_SHADER_3D_POLYLINE_CLIPPED_UNIFORM_COLOR:
      return "gpu_shader_3D_polyline_uniform_color_clipped";
    case GPU_SHADER_3D_POLYLINE_FLAT_COLOR:
      return "gpu_shader_3D_polyline_flat_color";
    case GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR:
      return "gpu_shader_3D_polyline_smooth_color";
    case GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR:
      return "gpu_shader_3D_line_dashed_uniform_color";
    case GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA:
      return "gpu_shader_2D_point_uniform_size_uniform_color_aa";
    case GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA:
      return "gpu_shader_2D_point_uniform_size_uniform_color_outline_aa";
    case GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR:
      return "gpu_shader_3D_point_varying_size_varying_color";
    case GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA:
      return "gpu_shader_3D_point_uniform_size_uniform_color_aa";
    case GPU_SHADER_2D_AREA_BORDERS:
      return "gpu_shader_2D_area_borders";
    case GPU_SHADER_2D_WIDGET_BASE:
      return "gpu_shader_2D_widget_base";
    case GPU_SHADER_2D_WIDGET_BASE_INST:
      return "gpu_shader_2D_widget_base_inst";
    case GPU_SHADER_2D_WIDGET_SHADOW:
      return "gpu_shader_2D_widget_shadow";
    case GPU_SHADER_2D_NODELINK:
      return "gpu_shader_2D_nodelink";
    case GPU_SHADER_2D_NODELINK_INST:
      return "gpu_shader_2D_nodelink_inst";
    case GPU_SHADER_GPENCIL_STROKE:
      return "gpu_shader_gpencil_stroke";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static const char *builtin_shader_create_info_name_clipped(eGPUBuiltinShader shader)
{
  switch (shader) {
    case GPU_SHADER_3D_UNIFORM_COLOR:
      return "gpu_shader_3D_uniform_color_clipped";
    case GPU_SHADER_3D_FLAT_COLOR:
      return "gpu_shader_3D_flat_color_clipped";
    case GPU_SHADER_3D_SMOOTH_COLOR:
      return "gpu_shader_3D_smooth_color_clipped";
    case GPU_SHADER_3D_DEPTH_ONLY:
      return "gpu_shader_3D_depth_only_clipped";
    case GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR:
      return "gpu_shader_3D_line_dashed_uniform_color_clipped";
    case GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA:
      return "gpu_shader_3D_point_uniform_size_uniform_color_aa_clipped";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

GPUShader *GPU_shader_get_builtin_shader_with_config(eGPUBuiltinShader shader,
                                                     eGPUShaderConfig sh_cfg)
{
  BLI_assert(shader < GPU_SHADER_BUILTIN_LEN);
  BLI_assert(sh_cfg < GPU_SHADER_CFG_LEN);
  GPUShader **sh_p = &builtin_shaders[sh_cfg][shader];

  if (*sh_p == nullptr) {
    if (sh_cfg == GPU_SHADER_CFG_DEFAULT) {
      /* Common case. */
      *sh_p = GPU_shader_create_from_info_name(builtin_shader_create_info_name(shader));
      if (ELEM(shader,
               GPU_SHADER_3D_POLYLINE_CLIPPED_UNIFORM_COLOR,
               GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR,
               GPU_SHADER_3D_POLYLINE_FLAT_COLOR,
               GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR))
      {
        /* Set a default value for `lineSmooth`.
         * Ideally this value should be set by the caller. */
        GPU_shader_bind(*sh_p);
        GPU_shader_uniform_1i(*sh_p, "lineSmooth", 1);
      }
    }
    else if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      /* In rare cases geometry shaders calculate clipping themselves. */
      *sh_p = GPU_shader_create_from_info_name(builtin_shader_create_info_name_clipped(shader));
    }
    else {
      BLI_assert(0);
    }
  }

  return *sh_p;
}

GPUShader *GPU_shader_get_builtin_shader(eGPUBuiltinShader shader)
{
  return GPU_shader_get_builtin_shader_with_config(shader, GPU_SHADER_CFG_DEFAULT);
}

void GPU_shader_free_builtin_shaders(void)
{
  for (int i = 0; i < GPU_SHADER_CFG_LEN; i++) {
    for (int j = 0; j < GPU_SHADER_BUILTIN_LEN; j++) {
      if (builtin_shaders[i][j]) {
        GPU_shader_free(builtin_shaders[i][j]);
        builtin_shaders[i][j] = nullptr;
      }
    }
  }
}
