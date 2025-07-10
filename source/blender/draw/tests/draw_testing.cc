/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "draw_testing.hh"

#include "DRW_engine.hh"
#include "GPU_shader.hh"

namespace blender::draw {

/* Base class for draw test cases. It will setup and tear down the GPU part around each test. */
#ifdef WITH_OPENGL_BACKEND
void DrawOpenGLTest::SetUp()
{
  GPUOpenGLTest::SetUp();
  DRW_mutexes_init();
}

void DrawOpenGLTest::TearDown()
{
  DRW_mutexes_exit();
  GPUOpenGLTest::TearDown();
}
#endif

#ifdef WITH_METAL_BACKEND
void DrawMetalTest::SetUp()
{
  GPUMetalTest::SetUp();
  DRW_mutexes_init();
}

void DrawMetalTest::TearDown()
{
  DRW_mutexes_exit();
  GPUMetalTest::TearDown();
}
#endif

#ifdef WITH_VULKAN_BACKEND
void DrawVulkanTest::SetUp()
{
  GPUVulkanTest::SetUp();
  DRW_mutexes_init();
}

void DrawVulkanTest::TearDown()
{
  DRW_mutexes_exit();
  GPUVulkanTest::TearDown();
}
#endif

}  // namespace blender::draw
