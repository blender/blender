/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_testing.hh"

#include "GPU_batch.hh"
#include "GPU_context.hh"
#include "GPU_framebuffer.hh"

namespace blender::gpu::tests {

using namespace blender::gpu::shader;

/**
 * Test if all static shaders can be compiled.
 */
static void test_static_shaders()
{
  if (GPU_type_matches_ex(
          GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_OFFICIAL, GPU_BACKEND_OPENGL) &&
      G.debug & G_DEBUG_GPU_FORCE_WORKAROUNDS)
  {
    GTEST_SKIP() << "NVIDIA fails to compile workaround due to reserved names. Gladly it doesn't "
                    "need the workaround.";
  }

  EXPECT_TRUE(gpu_shader_create_info_compile_all(nullptr));
}
GPU_TEST(static_shaders)

static void test_shader_create_info_pipeline()
{
  if (GPU_type_matches_ex(
          GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_OFFICIAL, GPU_BACKEND_OPENGL) &&
      G.debug & G_DEBUG_GPU_FORCE_WORKAROUNDS)
  {
    GTEST_SKIP() << "NVIDIA fails to compile workaround due to reserved names. Gladly it doesn't "
                    "need the workaround.";
  }
  GPU_render_begin();

  ShaderCreateInfo create_info("gpu_framebuffer_layer_viewport_test");
  create_info.vertex_source("gpu_framebuffer_layer_viewport_test.glsl");
  create_info.fragment_source("gpu_framebuffer_layer_viewport_test.glsl");
  create_info.builtins(BuiltinBits::VIEWPORT_INDEX | BuiltinBits::LAYER | BuiltinBits::VERTEX_ID);
  create_info.fragment_out(0, Type::int2_t, "out_value");

  create_info.pipeline_state()
      .state(GPU_WRITE_COLOR,
             GPU_BLEND_NONE,
             GPU_CULL_NONE,
             GPU_DEPTH_NONE,
             GPU_STENCIL_NONE,
             GPU_STENCIL_OP_NONE,
             GPU_VERTEX_LAST)
      .primitive(GPU_PRIM_TRIS)
      .viewports(16)
      .color_format(TextureTargetFormat::SINT_32_32);

  Shader *shader = GPU_shader_create_from_info(
      reinterpret_cast<GPUShaderCreateInfo *>(&create_info));

  EXPECT_TRUE(shader != nullptr);

  /* Setup framebuffer */
  const int2 size(4, 4);
  const int layers = 256;
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *texture = GPU_texture_create_2d_array(
      __func__, UNPACK2(size), layers, 1, TextureFormat::SINT_32_32, usage, nullptr);

  gpu::FrameBuffer *framebuffer = GPU_framebuffer_create(__func__);
  GPU_framebuffer_ensure_config(&framebuffer,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(texture)});
  GPU_framebuffer_bind(framebuffer);

  /* Setup viewports. */
  int viewport_rects[16][4];
  for (int i = 0; i < 16; i++) {
    viewport_rects[i][0] = i % 4;
    viewport_rects[i][1] = i / 4;
    viewport_rects[i][2] = 1;
    viewport_rects[i][3] = 1;
  }
  GPU_framebuffer_multi_viewports_set(framebuffer, viewport_rects);
  int tri_count = size.x * size.y * layers;

  Batch *batch = GPU_batch_create_procedural(GPU_PRIM_TRIS, tri_count * 3);
  GPU_batch_set_shader(batch, shader);
  /* On vulkan this triggers an assert when a new pipeline is created (`G.debug_value == 32`) */
  {
    GPUContext *context = GPU_context_active_get();
    DebugScopePipelineCreation scope(context);
    GPU_batch_draw(batch);
  }
  GPU_flush();

  GPU_render_end();
  GPU_batch_discard(batch);
  GPU_framebuffer_free(framebuffer);
  GPU_texture_free(texture);
  GPU_shader_unbind();
  GPU_SHADER_FREE_SAFE(shader);
}
GPU_TEST(shader_create_info_pipeline)

}  // namespace blender::gpu::tests
