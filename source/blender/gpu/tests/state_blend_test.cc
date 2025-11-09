/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "GPU_batch_utils.hh"
#include "testing/testing.h"

#include "gpu_testing.hh"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_framebuffer.hh"
#include "GPU_state.hh"

#include "BLI_math_vector.hh"

#include "intern/draw_cache.hh"

namespace blender::gpu::tests {

template<GPUBlend blend_type>
void blend_test(float4 source_a, float4 source_b, float4 expected_result)
{
  GPUOffScreen *offscreen = GPU_offscreen_create(1,
                                                 1,
                                                 false,
                                                 TextureFormat::SFLOAT_16_16_16_16,
                                                 GPU_TEXTURE_USAGE_ATTACHMENT |
                                                     GPU_TEXTURE_USAGE_HOST_READ,
                                                 false,
                                                 nullptr);
  BLI_assert(offscreen != nullptr);
  GPU_offscreen_bind(offscreen, false);
  blender::gpu::Texture *color_texture = GPU_offscreen_color_texture(offscreen);
  GPU_texture_clear(color_texture, GPU_DATA_FLOAT, source_a);

  Batch *batch = GPU_batch_preset_quad();

  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_batch_uniform_4fv(batch, "color", source_b);
  GPU_blend(blend_type);

  GPU_batch_draw(batch);
  GPU_offscreen_unbind(offscreen, false);
  GPU_flush();

  float4 read_back;
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
  GPU_offscreen_read_color(offscreen, GPU_DATA_FLOAT, &read_back);
  EXPECT_EQ(read_back, expected_result);

  GPU_offscreen_free(offscreen);

  /* Reset default. */
  GPU_blend(GPU_BLEND_NONE);
}

static void test_blend_none()
{
  blend_test<GPU_BLEND_NONE>(float4(1.0f, 0.0f, 1.0f, 1.0f),
                             float4(0.0f, 1.0f, 0.0f, 0.5f),
                             float4(0.0f, 1.0f, 0.0f, 0.5f));
}
GPU_TEST(blend_none)

static void test_blend_alpha()
{
  blend_test<GPU_BLEND_ALPHA>(float4(1.0f, 0.0f, 1.0f, 1.0f),
                              float4(0.0f, 1.0f, 0.0f, 0.5f),
                              float4(0.5f, 0.5f, 0.5f, 1.0f));
}
GPU_TEST(blend_alpha)

static void test_blend_alpha_premult()
{
  blend_test<GPU_BLEND_ALPHA_PREMULT>(float4(1.0f, 0.0f, 1.0f, 1.0f),
                                      float4(0.0f, 1.0f, 0.0f, 0.5f),
                                      float4(0.5f, 1.0f, 0.5f, 1.0f));
}
GPU_TEST(blend_alpha_premult)

static void test_blend_additive()
{
  blend_test<GPU_BLEND_ADDITIVE>(float4(1.0f, 0.0f, 1.0f, 1.0f),
                                 float4(0.0f, 1.0f, 0.0f, 0.5f),
                                 float4(1.0f, 0.5f, 1.0f, 1.0f));
}
GPU_TEST(blend_additive)

static void test_blend_additive_premult()
{
  blend_test<GPU_BLEND_ADDITIVE_PREMULT>(float4(1.0f, 0.0f, 1.0f, 1.0f),
                                         float4(0.0f, 1.0f, 0.0f, 0.5f),
                                         float4(1.0f, 1.0f, 1.0f, 1.5f));
}
GPU_TEST(blend_additive_premult)

static void test_blend_multiply()
{
  blend_test<GPU_BLEND_MULTIPLY>(float4(1.0f, 0.0f, 1.0f, 1.0f),
                                 float4(0.0f, 1.0f, 0.0f, 0.5f),
                                 float4(0.0f, 0.0f, 0.0f, 0.5f));
}
GPU_TEST(blend_multiply)

static void test_blend_subtract()
{
  blend_test<GPU_BLEND_SUBTRACT>(float4(1.0f, 1.0f, 1.0f, 1.0f),
                                 float4(0.0f, 1.0f, 0.0f, 0.5f),
                                 float4(1.0f, 0.0f, 1.0f, 0.5f));
}
GPU_TEST(blend_subtract)

static void test_blend_invert()
{
  blend_test<GPU_BLEND_INVERT>(float4(1.0f, 1.0f, 1.0f, 1.0f),
                               float4(0.0f, 1.0f, 0.0f, 0.5f),
                               float4(0.0f, 0.0f, 0.0f, 1.0f));
}
GPU_TEST(blend_invert)

static void test_blend_oit()
{
  blend_test<GPU_BLEND_OIT>(float4(1.0f, 1.0f, 1.0f, 1.0f),
                            float4(0.0f, 1.0f, 0.0f, 0.5f),
                            float4(1.0f, 2.0f, 1.0f, 0.5f));
}
GPU_TEST(blend_oit)

static void test_blend_background()
{
  blend_test<GPU_BLEND_BACKGROUND>(float4(1.0f, 1.0f, 1.0f, 1.0f),
                                   float4(0.0f, 1.0f, 0.0f, 0.5f),
                                   float4(0.5f, 0.5f, 0.5f, 0.5f));
}
GPU_TEST(blend_background)

static void test_blend_min()
{
  blend_test<GPU_BLEND_MIN>(float4(1.0f, 2.0f, 3.0f, 4.0f),
                            float4(4.0f, 3.0f, 2.0f, 1.0f),
                            float4(1.0f, 2.0f, 2.0f, 1.0f));
}
GPU_TEST(blend_min)

static void test_blend_max()
{
  blend_test<GPU_BLEND_MAX>(float4(1.0f, 2.0f, 3.0f, 4.0f),
                            float4(4.0f, 3.0f, 2.0f, 1.0f),
                            float4(4.0f, 3.0f, 3.0f, 4.0f));
}
GPU_TEST(blend_max)

}  // namespace blender::gpu::tests
