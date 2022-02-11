/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GHOST_C-api.h"

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
  GHOST_SystemHandle ghost_system;
  GHOST_ContextHandle ghost_context;
  struct GPUContext *context;

 protected:
  GPUTest(GHOST_TDrawingContextType draw_context_type) : draw_context_type(draw_context_type)
  {
  }

  void SetUp() override;
  void TearDown() override;
};

class GPUOpenGLTest : public GPUTest {
 public:
  GPUOpenGLTest() : GPUTest(GHOST_kDrawingContextTypeOpenGL)
  {
  }
};

#define GPU_TEST(test_name) \
  TEST_F(GPUOpenGLTest, test_name) \
  { \
    test_##test_name(); \
  }

}  // namespace blender::gpu
