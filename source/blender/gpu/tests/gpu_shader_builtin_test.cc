/* SPDX-License-Identifier: Apache-2.0 */

#include "gpu_testing.hh"

#include "GPU_shader.h"

namespace blender::gpu::tests {

static void test_compile_builtin_shader(eGPUBuiltinShader shader_type, eGPUShaderConfig sh_cfg)
{
  GPUShader *sh = GPU_shader_get_builtin_shader_with_config(shader_type, sh_cfg);
  EXPECT_NE(sh, nullptr);
}

static void test_compile_builtin_shader(eGPUBuiltinShader shader_type)
{
  test_compile_builtin_shader(shader_type, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(shader_type, GPU_SHADER_CFG_CLIPPED);
}

static void test_shader_builtin()
{
  GPU_shader_free_builtin_shaders();

  test_compile_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
  test_compile_builtin_shader(GPU_SHADER_3D_SMOOTH_COLOR);
  test_compile_builtin_shader(GPU_SHADER_3D_DEPTH_ONLY);
  test_compile_builtin_shader(GPU_SHADER_3D_FLAT_COLOR);
  test_compile_builtin_shader(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
  test_compile_builtin_shader(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

  test_compile_builtin_shader(GPU_SHADER_TEXT, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_KEYFRAME_SHAPE, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_SIMPLE_LIGHTING, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_3D_IMAGE, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_3D_IMAGE_COLOR, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_IMAGE_DESATURATE_COLOR, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_IMAGE_RECT_COLOR, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_IMAGE_MULTI_RECT_COLOR, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_CHECKER, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_DIAG_STRIPES, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_3D_POLYLINE_CLIPPED_UNIFORM_COLOR,
                              GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_3D_POLYLINE_FLAT_COLOR, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_IMAGE_OVERLAYS_MERGE, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_IMAGE_OVERLAYS_STEREO_MERGE, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA,
                              GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA,
                              GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR,
                              GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_GPENCIL_STROKE, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_AREA_BORDERS, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_WIDGET_BASE, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_WIDGET_BASE_INST, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_WIDGET_SHADOW, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_NODELINK, GPU_SHADER_CFG_DEFAULT);
  test_compile_builtin_shader(GPU_SHADER_2D_NODELINK_INST, GPU_SHADER_CFG_DEFAULT);
}

GPU_TEST(shader_builtin)

}  // namespace blender::gpu::tests
