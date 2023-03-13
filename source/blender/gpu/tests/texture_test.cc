#include "gpu_testing.hh"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.hh"

#include "GPU_context.h"
#include "GPU_texture.h"

namespace blender::gpu::tests {

static void test_texture_read()
{
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *rgba32u = GPU_texture_create_2d("rgba32u", 1, 1, 1, GPU_RGBA32UI, usage, nullptr);
  GPUTexture *rgba16u = GPU_texture_create_2d("rgba16u", 1, 1, 1, GPU_RGBA16UI, usage, nullptr);
  GPUTexture *rgba32f = GPU_texture_create_2d("rgba32f", 1, 1, 1, GPU_RGBA32F, usage, nullptr);

  const float4 fcol = {0.0f, 1.3f, -231.0f, 1000.0f};
  const uint4 ucol = {0, 1, 2, 12223};
  GPU_texture_clear(rgba32u, GPU_DATA_UINT, ucol);
  GPU_texture_clear(rgba16u, GPU_DATA_UINT, ucol);
  GPU_texture_clear(rgba32f, GPU_DATA_FLOAT, fcol);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  uint4 *rgba32u_data = (uint4 *)GPU_texture_read(rgba32u, GPU_DATA_UINT, 0);
  uint4 *rgba16u_data = (uint4 *)GPU_texture_read(rgba16u, GPU_DATA_UINT, 0);
  float4 *rgba32f_data = (float4 *)GPU_texture_read(rgba32f, GPU_DATA_FLOAT, 0);

  EXPECT_EQ(ucol, *rgba32u_data);
  EXPECT_EQ(ucol, *rgba16u_data);
  EXPECT_EQ(fcol, *rgba32f_data);

  MEM_freeN(rgba32u_data);
  MEM_freeN(rgba16u_data);
  MEM_freeN(rgba32f_data);

  GPU_texture_free(rgba32u);
  GPU_texture_free(rgba16u);
  GPU_texture_free(rgba32f);

  GPU_render_end();
}
GPU_TEST(texture_read)

}  // namespace blender::gpu::tests