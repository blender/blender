/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "gpu_shader_create_info_private.hh"
#include "gpu_testing.hh"

namespace blender::gpu::tests {

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

  EXPECT_TRUE(gpu_shader_create_info_compile(nullptr));
}
GPU_TEST(static_shaders)

}  // namespace blender::gpu::tests
