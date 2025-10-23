/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BKE_global.hh"

#include "GHOST_C-api.h"

#include "GPU_platform.hh"

struct GPUContext;

namespace blender::gpu {

/**
 * Test class that setups a GPUContext for test cases.
 */
class GPUTest : public ::testing::Test {
 private:
  static GHOST_SystemHandle ghost_system_;
  static GHOST_ContextHandle ghost_context_;
  static GPUContext *context_;

  static int32_t prev_g_debug_;
  std::string debug_group_name_;

 protected:
  static void SetUpTestSuite(GHOST_TDrawingContextType draw_context_type,
                             GPUBackendType gpu_backend_type,
                             int32_t g_debug_flags);
  static void TearDownTestSuite();

  void SetUp() override;
  void TearDown() override;
};

#ifdef WITH_OPENGL_BACKEND
class GPUOpenGLTest : public GPUTest {
 public:
  static void SetUpTestSuite()
  {
    GPUTest::SetUpTestSuite(GHOST_kDrawingContextTypeOpenGL,
                            GPU_BACKEND_OPENGL,
                            G_DEBUG_GPU | G_DEBUG_GPU_COMPILE_SHADERS | G_DEBUG_GPU_RENDERDOC);
  }
  static void TearDownTestSuite()
  {
    GPUTest::TearDownTestSuite();
  }
};

class GPUOpenGLWorkaroundsTest : public GPUTest {
 public:
  static void SetUpTestSuite()
  {
    GPUTest::SetUpTestSuite(GHOST_kDrawingContextTypeOpenGL,
                            GPU_BACKEND_OPENGL,
                            G_DEBUG_GPU | G_DEBUG_GPU_COMPILE_SHADERS | G_DEBUG_GPU_RENDERDOC |
                                G_DEBUG_GPU_FORCE_WORKAROUNDS);
  }
  static void TearDownTestSuite()
  {
    GPUTest::TearDownTestSuite();
  }
};
#  define GPU_OPENGL_TEST(test_name) \
    TEST_F(GPUOpenGLTest, test_name) \
    { \
      test_##test_name(); \
    } \
    TEST_F(GPUOpenGLWorkaroundsTest, test_name) \
    { \
      test_##test_name(); \
    }
#else
#  define GPU_OPENGL_TEST(test_name)
#endif

#ifdef WITH_METAL_BACKEND
class GPUMetalTest : public GPUTest {
 public:
  static void SetUpTestSuite()
  {
    GPUTest::SetUpTestSuite(GHOST_kDrawingContextTypeMetal, GPU_BACKEND_METAL, G_DEBUG_GPU);
  }
  static void TearDownTestSuite()
  {
    GPUTest::TearDownTestSuite();
  }
};

class GPUMetalWorkaroundsTest : public GPUTest {
 public:
  static void SetUpTestSuite()
  {
    GPUTest::SetUpTestSuite(GHOST_kDrawingContextTypeMetal,
                            GPU_BACKEND_METAL,
                            G_DEBUG_GPU | G_DEBUG_GPU_FORCE_WORKAROUNDS);
  }
  static void TearDownTestSuite()
  {
    GPUTest::TearDownTestSuite();
  }
};
#  define GPU_METAL_TEST(test_name) \
    TEST_F(GPUMetalTest, test_name) \
    { \
      test_##test_name(); \
    } \
    TEST_F(GPUMetalWorkaroundsTest, test_name) \
    { \
      test_##test_name(); \
    }
#else
#  define GPU_METAL_TEST(test_name)
#endif

#ifdef WITH_VULKAN_BACKEND
class GPUVulkanTest : public GPUTest {
 public:
  static void SetUpTestSuite()
  {
    GPUTest::SetUpTestSuite(GHOST_kDrawingContextTypeVulkan,
                            GPU_BACKEND_VULKAN,
                            G_DEBUG_GPU | G_DEBUG_GPU_SHADER_DEBUG_INFO | G_DEBUG_GPU_RENDERDOC);
  }
  static void TearDownTestSuite()
  {
    GPUTest::TearDownTestSuite();
  }
};

class GPUVulkanWorkaroundsTest : public GPUTest {
 public:
  static void SetUpTestSuite()
  {
    GPUTest::SetUpTestSuite(GHOST_kDrawingContextTypeVulkan,
                            GPU_BACKEND_VULKAN,
                            G_DEBUG_GPU | G_DEBUG_GPU_SHADER_DEBUG_INFO | G_DEBUG_GPU_RENDERDOC |
                                G_DEBUG_GPU_FORCE_WORKAROUNDS);
  }
  static void TearDownTestSuite()
  {
    GPUTest::TearDownTestSuite();
  }
};
#  define GPU_VULKAN_TEST(test_name) \
    TEST_F(GPUVulkanTest, test_name) \
    { \
      test_##test_name(); \
    } \
    TEST_F(GPUVulkanWorkaroundsTest, test_name) \
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

#define BLOCK_GPU_TEST_ON(device_type, os_type, driver_type, backend_type) \
  if (!blender::tests::should_ignore_blocklist() && \
      GPU_type_matches_ex(device_type, os_type, driver_type, backend_type)) \
  { \
    GTEST_SKIP(); \
    return; \
  }

}  // namespace blender::gpu
