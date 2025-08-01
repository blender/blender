/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_shader_builtin.hh"
#include "GPU_state.hh"
#include "gpu_testing.hh"

#include "BLI_math_vector.hh"

namespace blender::gpu::tests {

static constexpr int Size = 4;

static void test_immediate_one_plane()
{
  GPUOffScreen *offscreen = GPU_offscreen_create(Size,
                                                 Size,
                                                 false,
                                                 TextureFormat::SFLOAT_16_16_16_16,
                                                 GPU_TEXTURE_USAGE_ATTACHMENT |
                                                     GPU_TEXTURE_USAGE_HOST_READ,
                                                 false,
                                                 nullptr);
  BLI_assert(offscreen != nullptr);
  GPU_offscreen_bind(offscreen, false);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  float4 color(1.0, 0.5, 0.25, 1.0);
  immUniformColor4fv(color);
  immBegin(GPU_PRIM_TRI_STRIP, 4);
  immVertex3f(pos, -1.0f, 1.0f, 0.0f);
  immVertex3f(pos, 1.0f, 1.0f, 0.0f);
  immVertex3f(pos, -1.0f, -1.0f, 0.0f);
  immVertex3f(pos, 1.0f, -1.0f, 0.0f);
  immEnd();

  GPU_offscreen_unbind(offscreen, false);
  GPU_flush();

  /* Read back data and perform some basic tests. */
  Vector<float4> read_data(Size * Size);
  GPU_offscreen_read_color(offscreen, GPU_DATA_FLOAT, read_data.data());
  for (const float4 &read_color : read_data) {
    EXPECT_EQ(read_color, color);
  }

  GPU_offscreen_free(offscreen);

  immUnbindProgram();
}
GPU_TEST(immediate_one_plane)

/**
 * Draws two planes with two different colors.
 * - Tests that both planes are stored in the same buffer (depends on backend).
 * - Test that data of the first plane isn't overwritten by the second plane.
 *   (push constants, buffer, bind points etc.)
 */
static void test_immediate_two_planes()
{
  GPUOffScreen *offscreen = GPU_offscreen_create(Size,
                                                 Size,
                                                 false,
                                                 TextureFormat::SFLOAT_16_16_16_16,
                                                 GPU_TEXTURE_USAGE_ATTACHMENT |
                                                     GPU_TEXTURE_USAGE_HOST_READ,
                                                 false,
                                                 nullptr);
  BLI_assert(offscreen != nullptr);
  GPU_offscreen_bind(offscreen, false);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  float4 color(1.0, 0.5, 0.25, 1.0);
  immUniformColor4fv(color);
  immBegin(GPU_PRIM_TRI_STRIP, 4);
  immVertex3f(pos, -1.0f, 1.0f, 0.0f);
  immVertex3f(pos, 0.0f, 1.0f, 0.0f);
  immVertex3f(pos, -1.0f, -1.0f, 0.0f);
  immVertex3f(pos, 0.0f, -1.0f, 0.0f);
  immEnd();

  float4 color2(0.25, 0.5, 1.0, 1.0);
  immUniformColor4fv(color2);
  immBegin(GPU_PRIM_TRI_STRIP, 4);
  immVertex3f(pos, 0.0f, 1.0f, 0.0f);
  immVertex3f(pos, 1.0f, 1.0f, 0.0f);
  immVertex3f(pos, 0.0f, -1.0f, 0.0f);
  immVertex3f(pos, 1.0f, -1.0f, 0.0f);
  immEnd();

  GPU_offscreen_unbind(offscreen, false);
  GPU_flush();

  /* Read back data and perform some basic tests.
   * Not performing detailed tests as there might be driver specific limitations. */
  Vector<float4> read_data(Size * Size);
  GPU_offscreen_read_color(offscreen, GPU_DATA_FLOAT, read_data.data());
  int64_t color_num = 0;
  int64_t color2_num = 0;
  for (const float4 &read_color : read_data) {
    if (read_color == color) {
      color_num++;
    }
    else if (read_color == color2) {
      color2_num++;
    }
    else {
      EXPECT_TRUE(read_color == color || read_color == color2);
    }
  }
  EXPECT_TRUE(color_num > 0);
  EXPECT_TRUE(color2_num > 0);

  GPU_offscreen_free(offscreen);

  immUnbindProgram();
}
GPU_TEST(immediate_two_planes)

}  // namespace blender::gpu::tests
