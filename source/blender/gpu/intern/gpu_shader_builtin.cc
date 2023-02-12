/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "BLI_utildefines.h"

#include "GPU_shader.h"

/* Cache of built-in shaders (each is created on first use). */
static GPUShader *builtin_shaders[GPU_SHADER_CFG_LEN][GPU_SHADER_BUILTIN_LEN] = {{nullptr}};

typedef struct {
  const char *name;
  const char *create_info;
  /** Optional. */
  const char *clipped_create_info;
} GPUShaderStages;

static const GPUShaderStages builtin_shader_stages[GPU_SHADER_BUILTIN_LEN] = {
    [GPU_SHADER_TEXT] =
        {
            .name = "GPU_SHADER_TEXT",
            .create_info = "gpu_shader_text",
        },
    [GPU_SHADER_KEYFRAME_SHAPE] =
        {
            .name = "GPU_SHADER_KEYFRAME_SHAPE",
            .create_info = "gpu_shader_keyframe_shape",
        },
    [GPU_SHADER_SIMPLE_LIGHTING] =
        {
            .name = "GPU_SHADER_SIMPLE_LIGHTING",
            .create_info = "gpu_shader_simple_lighting",
        },
    [GPU_SHADER_3D_IMAGE] =
        {
            .name = "GPU_SHADER_3D_IMAGE",
            .create_info = "gpu_shader_3D_image",
        },
    [GPU_SHADER_3D_IMAGE_COLOR] =
        {
            .name = "GPU_SHADER_3D_IMAGE_COLOR",
            .create_info = "gpu_shader_3D_image_color",
        },
    [GPU_SHADER_2D_CHECKER] =
        {
            .name = "GPU_SHADER_2D_CHECKER",
            .create_info = "gpu_shader_2D_checker",
        },

    [GPU_SHADER_2D_DIAG_STRIPES] =
        {
            .name = "GPU_SHADER_2D_DIAG_STRIPES",
            .create_info = "gpu_shader_2D_diag_stripes",
        },

    [GPU_SHADER_ICON] =
        {
            .name = "GPU_SHADER_ICON",
            .create_info = "gpu_shader_icon",
        },
    [GPU_SHADER_2D_IMAGE_OVERLAYS_MERGE] =
        {
            .name = "GPU_SHADER_2D_IMAGE_OVERLAYS_MERGE",
            .create_info = "gpu_shader_2D_image_overlays_merge",
        },
    [GPU_SHADER_2D_IMAGE_OVERLAYS_STEREO_MERGE] =
        {
            .name = "GPU_SHADER_2D_IMAGE_OVERLAYS_STEREO_MERGE",
            .create_info = "gpu_shader_2D_image_overlays_stereo_merge",
        },
    [GPU_SHADER_2D_IMAGE_DESATURATE_COLOR] =
        {
            .name = "GPU_SHADER_2D_IMAGE_DESATURATE_COLOR",
            .create_info = "gpu_shader_2D_image_desaturate_color",
        },
    [GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR] =
        {
            .name = "GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR",
            .create_info = "gpu_shader_2D_image_shuffle_color",
        },
    [GPU_SHADER_2D_IMAGE_RECT_COLOR] =
        {
            .name = "GPU_SHADER_2D_IMAGE_RECT_COLOR",
            .create_info = "gpu_shader_2D_image_rect_color",
        },
    [GPU_SHADER_2D_IMAGE_MULTI_RECT_COLOR] =
        {
            .name = "GPU_SHADER_2D_IMAGE_MULTI_RECT_COLOR",
            .create_info = "gpu_shader_2D_image_multi_rect_color",
        },

    [GPU_SHADER_3D_UNIFORM_COLOR] =
        {
            .name = "GPU_SHADER_3D_UNIFORM_COLOR",
            .create_info = "gpu_shader_3D_uniform_color",
            .clipped_create_info = "gpu_shader_3D_uniform_color_clipped",
        },
    [GPU_SHADER_3D_FLAT_COLOR] =
        {
            .name = "GPU_SHADER_3D_FLAT_COLOR",
            .create_info = "gpu_shader_3D_flat_color",
            .clipped_create_info = "gpu_shader_3D_flat_color_clipped",
        },
    [GPU_SHADER_3D_SMOOTH_COLOR] =
        {
            .name = "GPU_SHADER_3D_SMOOTH_COLOR",
            .create_info = "gpu_shader_3D_smooth_color",
            .clipped_create_info = "gpu_shader_3D_smooth_color_clipped",
        },
    [GPU_SHADER_3D_DEPTH_ONLY] =
        {
            .name = "GPU_SHADER_3D_DEPTH_ONLY",
            .create_info = "gpu_shader_3D_depth_only",
            .clipped_create_info = "gpu_shader_3D_depth_only_clipped",
        },
    [GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR] =
        {
            .name = "GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR",
            .create_info = "gpu_shader_3D_clipped_uniform_color",
        },

    [GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR] =
        {
            .name = "GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR",
            .create_info = "gpu_shader_3D_polyline_uniform_color",
        },
    [GPU_SHADER_3D_POLYLINE_CLIPPED_UNIFORM_COLOR] =
        {
            .name = "GPU_SHADER_3D_POLYLINE_CLIPPED_UNIFORM_COLOR",
            .create_info = "gpu_shader_3D_polyline_uniform_color_clipped",
        },
    [GPU_SHADER_3D_POLYLINE_FLAT_COLOR] =
        {
            .name = "GPU_SHADER_3D_POLYLINE_FLAT_COLOR",
            .create_info = "gpu_shader_3D_polyline_flat_color",
        },
    [GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR] =
        {
            .name = "GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR",
            .create_info = "gpu_shader_3D_polyline_smooth_color",
        },

    [GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR] =
        {
            .name = "GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR",
            .create_info = "gpu_shader_3D_line_dashed_uniform_color",
            .clipped_create_info = "gpu_shader_3D_line_dashed_uniform_color_clipped",
        },

    [GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA] =
        {
            .name = "GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA",
            .create_info = "gpu_shader_2D_point_uniform_size_uniform_color_aa",
        },
    [GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA] =
        {
            .name = "GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA",
            .create_info = "gpu_shader_2D_point_uniform_size_uniform_color_outline_aa",
        },
    [GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR] =
        {
            .name = "GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR",
            .create_info = "gpu_shader_3D_point_fixed_size_varying_color",
        },
    [GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR] =
        {
            .name = "GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR",
            .create_info = "gpu_shader_3D_point_varying_size_varying_color",
        },
    [GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA] =
        {
            .name = "GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA",
            .create_info = "gpu_shader_3D_point_uniform_size_uniform_color_aa",
            .clipped_create_info = "gpu_shader_3D_point_uniform_size_uniform_color_aa_clipped",
        },

    [GPU_SHADER_2D_AREA_BORDERS] =
        {
            .name = "GPU_SHADER_2D_AREA_BORDERS",
            .create_info = "gpu_shader_2D_area_borders",
        },
    [GPU_SHADER_2D_WIDGET_BASE] =
        {
            .name = "GPU_SHADER_2D_WIDGET_BASE",
            .create_info = "gpu_shader_2D_widget_base",
        },
    [GPU_SHADER_2D_WIDGET_BASE_INST] =
        {
            .name = "GPU_SHADER_2D_WIDGET_BASE_INST",
            .create_info = "gpu_shader_2D_widget_base_inst",
        },
    [GPU_SHADER_2D_WIDGET_SHADOW] =
        {
            .name = "GPU_SHADER_2D_WIDGET_SHADOW",
            .create_info = "gpu_shader_2D_widget_shadow",
        },
    [GPU_SHADER_2D_NODELINK] =
        {
            .name = "GPU_SHADER_2D_NODELINK",
            .create_info = "gpu_shader_2D_nodelink",
        },

    [GPU_SHADER_2D_NODELINK_INST] =
        {
            .name = "GPU_SHADER_2D_NODELINK_INST",
            .create_info = "gpu_shader_2D_nodelink_inst",
        },

    [GPU_SHADER_GPENCIL_STROKE] =
        {
            .name = "GPU_SHADER_GPENCIL_STROKE",
            .create_info = "gpu_shader_gpencil_stroke",
        },
};

GPUShader *GPU_shader_get_builtin_shader_with_config(eGPUBuiltinShader shader,
                                                     eGPUShaderConfig sh_cfg)
{
  BLI_assert(shader < GPU_SHADER_BUILTIN_LEN);
  BLI_assert(sh_cfg < GPU_SHADER_CFG_LEN);
  GPUShader **sh_p = &builtin_shaders[sh_cfg][shader];

  if (*sh_p == nullptr) {
    const GPUShaderStages *stages = &builtin_shader_stages[shader];

    /* common case */
    if (sh_cfg == GPU_SHADER_CFG_DEFAULT) {
      *sh_p = GPU_shader_create_from_info_name(stages->create_info);
      if (ELEM(shader,
               GPU_SHADER_3D_POLYLINE_CLIPPED_UNIFORM_COLOR,
               GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR,
               GPU_SHADER_3D_POLYLINE_FLAT_COLOR,
               GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR)) {
        /* Set a default value for `lineSmooth`.
         * Ideally this value should be set by the caller. */
        GPU_shader_bind(*sh_p);
        GPU_shader_uniform_1i(*sh_p, "lineSmooth", 1);
      }
    }
    else if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      /* In rare cases geometry shaders calculate clipping themselves. */
      if (stages->clipped_create_info != nullptr) {
        *sh_p = GPU_shader_create_from_info_name(stages->clipped_create_info);
      }
      else {
        BLI_assert_msg(0, "Shader doesn't support clip mode.");
      }
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
