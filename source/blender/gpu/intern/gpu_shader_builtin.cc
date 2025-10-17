/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"
#include "BLI_utildefines.h"

#include "GPU_capabilities.hh"
#include "GPU_shader.hh"

#include "gpu_shader_private.hh"

struct BuiltinShader : blender::gpu::StaticShader {
  /* WORKAROUND: This is needed for the polyline workaround default initialization. */
  bool init = false;

  BuiltinShader(std::string info_name) : blender::gpu::StaticShader(info_name) {}
};

/* Cache of built-in shaders (each is created on first use). */
static BuiltinShader *builtin_shaders[GPU_SHADER_CFG_LEN][GPU_SHADER_BUILTIN_LEN] = {{nullptr}};

static const char *builtin_shader_create_info_name(GPUBuiltinShader shader)
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
    case GPU_SHADER_3D_IMAGE_SCENE_LINEAR_TO_REC709_SRGB:
      return "gpu_shader_3D_image_scene_linear";
    case GPU_SHADER_3D_IMAGE_COLOR:
      return "gpu_shader_3D_image_color";
    case GPU_SHADER_3D_IMAGE_COLOR_SCENE_LINEAR_TO_REC709_SRGB:
      return "gpu_shader_3D_image_color_scene_linear";
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
    case GPU_SHADER_3D_POINT_FLAT_COLOR:
      return "gpu_shader_3D_point_flat_color";
    case GPU_SHADER_3D_POINT_UNIFORM_COLOR:
      return "gpu_shader_3D_point_uniform_color";
    case GPU_SHADER_2D_AREA_BORDERS:
      return "gpu_shader_2D_area_borders";
    case GPU_SHADER_2D_WIDGET_BASE:
      return "gpu_shader_2D_widget_base";
    case GPU_SHADER_2D_WIDGET_BASE_INST:
      return "gpu_shader_2D_widget_base_inst";
    case GPU_SHADER_2D_WIDGET_SHADOW:
      return "gpu_shader_2D_widget_shadow";
    case GPU_SHADER_2D_NODE_SOCKET:
      return "gpu_shader_2D_node_socket";
    case GPU_SHADER_2D_NODE_SOCKET_INST:
      return "gpu_shader_2D_node_socket_inst";
    case GPU_SHADER_2D_NODELINK:
      return "gpu_shader_2D_nodelink";
    case GPU_SHADER_GPENCIL_STROKE:
      return "gpu_shader_gpencil_stroke";
    case GPU_SHADER_SEQUENCER_STRIPS:
      return "gpu_shader_sequencer_strips";
    case GPU_SHADER_SEQUENCER_THUMBS:
      return "gpu_shader_sequencer_thumbs";
    case GPU_SHADER_SEQUENCER_SCOPE_RASTER:
      return "gpu_shader_sequencer_scope_raster";
    case GPU_SHADER_SEQUENCER_SCOPE_RESOLVE:
      return "gpu_shader_sequencer_scope_resolve";
    case GPU_SHADER_SEQUENCER_ZEBRA:
      return "gpu_shader_sequencer_zebra";
    case GPU_SHADER_INDEXBUF_POINTS:
      return "gpu_shader_index_2d_array_points";
    case GPU_SHADER_INDEXBUF_LINES:
      return "gpu_shader_index_2d_array_lines";
    case GPU_SHADER_INDEXBUF_TRIS:
      return "gpu_shader_index_2d_array_tris";
    case GPU_SHADER_XR_RAYCAST:
      return "gpu_shader_xr_raycast";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static const char *builtin_shader_create_info_name_clipped(GPUBuiltinShader shader)
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
    case GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR:
      return "gpu_shader_3D_polyline_uniform_color_clipped";
    default:
      BLI_assert_msg(false, "Clipped shader configuration not available.");
      return "";
  }
}

blender::gpu::Shader *GPU_shader_get_builtin_shader_with_config(GPUBuiltinShader shader,
                                                                GPUShaderConfig sh_cfg)
{
  BLI_assert(shader < GPU_SHADER_BUILTIN_LEN);

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif
  BuiltinShader **sh_p = &builtin_shaders[sh_cfg][shader];
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

  if (*sh_p == nullptr) {
    if (sh_cfg == GPU_SHADER_CFG_DEFAULT) {
      /* Common case. */
      const char *info_name = builtin_shader_create_info_name(shader);
      *sh_p = MEM_new<BuiltinShader>(__func__, info_name);
    }
    else if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      /* In rare cases geometry shaders calculate clipping themselves. */
      const char *info_name_clipped = builtin_shader_create_info_name_clipped(shader);
      if (!blender::StringRefNull(info_name_clipped).is_empty()) {
        *sh_p = MEM_new<BuiltinShader>(__func__, info_name_clipped);
      }
    }
    else {
      BLI_assert(0);
    }
  }

  if ((*sh_p)->init == false) {
    (*sh_p)->init = true;
    if (ELEM(shader,
             GPU_SHADER_3D_POLYLINE_CLIPPED_UNIFORM_COLOR,
             GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR,
             GPU_SHADER_3D_POLYLINE_FLAT_COLOR,
             GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR))
    {
      blender::gpu::Shader *sh = (*sh_p)->get();
      /* Set a default value for `lineSmooth`.
       * Ideally this value should be set by the caller. */
      GPU_shader_bind(sh);
      GPU_shader_uniform_1i(sh, "lineSmooth", 1);
      /* WORKAROUND: See is_polyline declaration. */
      sh->is_polyline = true;
    }
  }

  return (*sh_p)->get();
}

static void gpu_shader_warm_builtin_shader_async(GPUBuiltinShader shader, GPUShaderConfig sh_cfg)
{
  BLI_assert(shader < GPU_SHADER_BUILTIN_LEN);

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif
  BuiltinShader **sh_p = &builtin_shaders[sh_cfg][shader];
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

  if (*sh_p == nullptr) {
    if (sh_cfg == GPU_SHADER_CFG_DEFAULT) {
      /* Common case. */
      const char *info_name = builtin_shader_create_info_name(shader);
      *sh_p = MEM_new<BuiltinShader>(__func__, info_name);
    }
    else if (sh_cfg == GPU_SHADER_CFG_CLIPPED) {
      /* In rare cases geometry shaders calculate clipping themselves. */
      const char *info_name_clipped = builtin_shader_create_info_name_clipped(shader);
      if (!blender::StringRefNull(info_name_clipped).is_empty()) {
        *sh_p = MEM_new<BuiltinShader>(__func__, info_name_clipped);
      }
    }
    else {
      BLI_assert(0);
    }
  }
  (*sh_p)->ensure_compile_async();
}

blender::gpu::Shader *GPU_shader_get_builtin_shader(GPUBuiltinShader shader)
{
  return GPU_shader_get_builtin_shader_with_config(shader, GPU_SHADER_CFG_DEFAULT);
}

void GPU_shader_builtin_warm_up()
{
  if ((G.debug & G_DEBUG_GPU) && (GPU_backend_get_type() == GPU_BACKEND_OPENGL)) {
    /* On some system (Mesa OpenGL), doing this warm up seems to breaks something related to debug
     * hooks and makes the Blender application hang. */
    return;
  }

  if (GPU_use_subprocess_compilation() && (GPU_backend_get_type() == GPU_BACKEND_OPENGL)) {
    /* The overhead of creating the subprocesses at this exact moment can create bubbles during the
     * startup process. It is usually fast enough on OpenGL that we can skip it. */
    return;
  }
  /* Ordered by first usage in default startup screen.
   * Adding more to this list will delay the scheduling of engine shaders and increase time to
   * first pixel. */
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_TEXT, GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_2D_WIDGET_BASE, GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_3D_UNIFORM_COLOR, GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR,
                                       GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_3D_IMAGE_COLOR, GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_2D_NODE_SOCKET, GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_2D_WIDGET_BASE_INST, GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR,
                                       GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_2D_IMAGE_DESATURATE_COLOR,
                                       GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR,
                                       GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_2D_WIDGET_SHADOW, GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_2D_DIAG_STRIPES, GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_2D_IMAGE_RECT_COLOR, GPU_SHADER_CFG_DEFAULT);
  gpu_shader_warm_builtin_shader_async(GPU_SHADER_2D_AREA_BORDERS, GPU_SHADER_CFG_DEFAULT);
}

void GPU_shader_free_builtin_shaders()
{
  /* Make sure non is bound before deleting. */
  GPU_shader_unbind();
  for (int i = 0; i < GPU_SHADER_CFG_LEN; i++) {
    for (int j = 0; j < GPU_SHADER_BUILTIN_LEN; j++) {
      MEM_SAFE_DELETE(builtin_shaders[i][j]);
    }
  }
}
