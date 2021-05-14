/* Apache License, Version 2.0 */

#include "gpu_testing.hh"

namespace blender::draw {

/* Base class for draw test cases. It will setup and tear down the GPU part around each test. */
class DrawTest : public blender::gpu::GPUTest {
 public:
  void SetUp() override;
};

}  // namespace blender::draw
