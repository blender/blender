/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GHOST_C-api.h"
#include "GPU_platform.h"

struct GPUContext;

namespace blender::gpu {

/* Test class that setups a GPUContext for test cases.
 *
 * Usage:
 *   TEST_F(GPUTest, my_gpu_test) {
 *     ...
 *   }
 */
class GPUTest : public ::testing::Test {
 private:
  GHOST_TDrawingContextType draw_context_type = GHOST_kDrawingContextTypeNone;
  eGPUBackendType gpu_backend_type;
  GHOST_SystemHandle ghost_system;
  GHOST_ContextHandle ghost_context;
  GPUContext *context;

  int32_t prev_g_debug_;

 protected:
  GPUTest(GHOST_TDrawingContextType draw_context_type, eGPUBackendType gpu_backend_type)
      : draw_context_type(draw_context_type), gpu_backend_type(gpu_backend_type)
  {
  }

  void SetUp() override;
  void TearDown() override;
};

#ifdef WITH_OPENGL_BACKEND
class GPUOpenGLTest : public GPUTest {
 public:
  GPUOpenGLTest() : GPUTest(GHOST_kDrawingContextTypeOpenGL, GPU_BACKEND_OPENGL) {}
};
#  define GPU_OPENGL_TEST(test_name) \
    TEST_F(GPUOpenGLTest, test_name) \
    { \
      test_##test_name(); \
    }
#else
#  define GPU_OPENGL_TEST(test_name)
#endif

#ifdef WITH_METAL_BACKEND
class GPUMetalTest : public GPUTest {
 public:
  GPUMetalTest() : GPUTest(GHOST_kDrawingContextTypeMetal, GPU_BACKEND_METAL) {}
};
#  define GPU_METAL_TEST(test_name) \
    TEST_F(GPUMetalTest, test_name) \
    { \
      test_##test_name(); \
    }
#else
#  define GPU_METAL_TEST(test_name)
#endif

#ifdef WITH_VULKAN_BACKEND
class GPUVulkanTest : public GPUTest {
 public:
  GPUVulkanTest() : GPUTest(GHOST_kDrawingContextTypeVulkan, GPU_BACKEND_VULKAN) {}
};
#  define GPU_VULKAN_TEST(test_name) \
    TEST_F(GPUVulkanTest, test_name) \
    { \
      test_##test_name(); \
    }
#else
#  define GPU_VULKAN_TEST(test_name)
#endif

#define GPU_TEST(test_name) \
  GPU_OPENGL_TEST(test_name) \
  GPU_METAL_TEST(test_name) \
  GPU_VULKAN_TEST(test_name)

}  // namespace blender::gpu
