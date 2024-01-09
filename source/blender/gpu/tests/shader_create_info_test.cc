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
  EXPECT_TRUE(gpu_shader_create_info_compile(nullptr));
}
GPU_TEST(static_shaders)

}  // namespace blender::gpu::tests
