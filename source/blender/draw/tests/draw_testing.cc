/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "draw_testing.hh"

#include "GPU_shader.hh"

namespace blender::draw {

/* Base class for draw test cases. It will setup and tear down the GPU part around each test. */
#ifdef WITH_OPENGL_BACKEND
void DrawOpenGLTest::SetUp()
{
  GPUOpenGLTest::SetUp();
}
#endif

#ifdef WITH_METAL_BACKEND
void DrawMetalTest::SetUp()
{
  GPUMetalTest::SetUp();
}
#endif

#ifdef WITH_VULKAN_BACKEND
void DrawVulkanTest::SetUp()
{
  GPUVulkanTest::SetUp();
}
#endif

}  // namespace blender::draw
