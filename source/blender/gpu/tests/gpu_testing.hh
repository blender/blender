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
  GHOST_SystemHandle ghost_system;
  GHOST_ContextHandle ghost_context;
  struct GPUContext *context;

 protected:
  void SetUp() override;
  void TearDown() override;
};

}  // namespace blender::gpu
