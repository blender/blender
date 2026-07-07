/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "gpu_testing.hh"

namespace blender::draw {

/* Base class for draw test cases. It will setup and tear down the GPU part around each test. */
#ifdef WITH_OPENGL_BACKEND
class DrawOpenGLTest : public gpu::GPUOpenGLTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

#  define DRAW_OPENGL_TEST(test_name) \
    TEST_F(DrawOpenGLTest, test_name) \
    { \
      test_##test_name(); \
    }
#else
#  define DRAW_OPENGL_TEST(test_name)
#endif

#ifdef WITH_METAL_BACKEND
class DrawMetalTest : public gpu::GPUMetalTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

#  define DRAW_METAL_TEST(test_name) \
    TEST_F(DrawMetalTest, test_name) \
    { \
      test_##test_name(); \
    }
#else
#  define DRAW_METAL_TEST(test_name)
#endif

#ifdef WITH_VULKAN_BACKEND
class DrawVulkanTest : public gpu::GPUVulkanTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

#  define DRAW_VULKAN_TEST(test_name) \
    TEST_F(DrawVulkanTest, test_name) \
    { \
      test_##test_name(); \
    }
#else
#  define DRAW_VULKAN_TEST(test_name)
#endif

#define DRAW_TEST(test_name) \
  DRAW_OPENGL_TEST(test_name) \
  DRAW_METAL_TEST(test_name) \
  DRAW_VULKAN_TEST(test_name)

}  // namespace blender::draw
