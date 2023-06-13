/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "gpu_testing.hh"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector_types.hh"

#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_storage_buffer.h"
#include "GPU_texture.h"

namespace blender::gpu::tests {
static void test_compute_direct()
{
  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    GTEST_SKIP() << "Skipping test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 32;

  /* Build compute shader. */
  GPUShader *shader = GPU_shader_create_from_info_name("gpu_compute_2d_test");
  EXPECT_NE(shader, nullptr);

  /* Create texture to store result and attach to shader. */
  GPUTexture *texture = GPU_texture_create_2d(
      "gpu_shader_compute_2d", SIZE, SIZE, 1, GPU_RGBA32F, GPU_TEXTURE_USAGE_GENERAL, nullptr);
  EXPECT_NE(texture, nullptr);

  GPU_shader_bind(shader);
  GPU_texture_image_bind(texture, GPU_shader_get_sampler_binding(shader, "img_output"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, SIZE, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  float4 *data = static_cast<float4 *>(GPU_texture_read(texture, GPU_DATA_FLOAT, 0));
  const float4 expected_result(1.0f, 0.5f, 0.2f, 1.0f);
  EXPECT_NE(data, nullptr);
  for (int index = 0; index < SIZE * SIZE; index++) {
    EXPECT_EQ(data[index], expected_result);
  }
  MEM_freeN(data);

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_texture_unbind(texture);
  GPU_texture_free(texture);
  GPU_shader_free(shader);
}
GPU_TEST(compute_direct)

static void test_compute_indirect()
{
  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    GTEST_SKIP() << "Skipping test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 32;

  /* Build compute shader. */
  GPUShader *shader = GPU_shader_create_from_info_name("gpu_compute_2d_test");
  EXPECT_NE(shader, nullptr);

  /* Create texture to store result and attach to shader. */
  GPUTexture *texture = GPU_texture_create_2d(
      "gpu_shader_compute_2d", SIZE, SIZE, 1, GPU_RGBA32F, GPU_TEXTURE_USAGE_GENERAL, nullptr);
  EXPECT_NE(texture, nullptr);
  GPU_texture_clear(texture, GPU_DATA_FLOAT, float4(0.0f));

  GPU_shader_bind(shader);
  GPU_texture_image_bind(texture, GPU_shader_get_sampler_binding(shader, "img_output"));

  /* Generate compute tasks. */
  uint4 commands[1] = {
      {SIZE, SIZE, 1, 0},
  };
  GPUStorageBuf *compute_commands = GPU_storagebuf_create_ex(
      sizeof(commands), &commands, GPU_USAGE_STATIC, __func__);

  /* Dispatch compute task. */
  GPU_compute_dispatch_indirect(shader, compute_commands);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  float4 *data = static_cast<float4 *>(GPU_texture_read(texture, GPU_DATA_FLOAT, 0));
  const float4 expected_result(1.0f, 0.5f, 0.2f, 1.0f);
  EXPECT_NE(data, nullptr);
  for (int index = 0; index < SIZE * SIZE; index++) {
    EXPECT_EQ(data[index], expected_result);
  }
  MEM_freeN(data);

  /* Cleanup. */
  GPU_storagebuf_free(compute_commands);
  GPU_shader_unbind();
  GPU_texture_unbind(texture);
  GPU_texture_free(texture);
  GPU_shader_free(shader);
}
GPU_TEST(compute_indirect);

}  // namespace blender::gpu::tests
