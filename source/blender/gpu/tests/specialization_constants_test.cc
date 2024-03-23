/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_batch.hh"
#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_context.hh"
#include "GPU_framebuffer.hh"
#include "GPU_shader.hh"
#include "GPU_storage_buffer.hh"

#include "BLI_math_vector.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_testing.hh"

namespace blender::gpu::tests {

struct ShaderSpecializationConst {
  GPUShader *shader = nullptr;
  GPUStorageBuf *ssbo = nullptr;
  Vector<int> data;

  float float_in;
  uint uint_in;
  int int_in;
  bool bool_in;

  bool is_graphic = false;

  ShaderSpecializationConst(const char *info_name)
  {
    if (!GPU_compute_shader_support()) {
      /* We can't test as a the platform does not support compute shaders. */
      std::cout << "Skipping test: platform not supported";
      return;
    }

    GPU_render_begin();

    this->init_shader(info_name);

    GPU_storagebuf_bind(ssbo, GPU_shader_get_ssbo_binding(shader, "data_out"));

    /* Expect defaults. */
    float_in = 2;
    uint_in = 3;
    int_in = 4;
    bool_in = true;

    this->validate();

    /* Test values. */
    float_in = 52;
    uint_in = 324;
    int_in = 455;
    bool_in = false;

    GPU_shader_constant_float(shader, "float_in", float_in);
    GPU_shader_constant_uint(shader, "uint_in", uint_in);
    GPU_shader_constant_int(shader, "int_in", int_in);
    GPU_shader_constant_bool(shader, "bool_in", bool_in);

    this->validate();

    GPU_render_end();
  }

  ~ShaderSpecializationConst()
  {
    if (shader != nullptr) {
      GPU_shader_unbind();
      GPU_shader_free(shader);
    }
    if (ssbo != nullptr) {
      GPU_storagebuf_free(ssbo);
    }
  }

  void init_shader(const char *info_name)
  {
    using namespace blender::gpu::shader;

    uint data_len = 4;
    ssbo = GPU_storagebuf_create_ex(data_len * sizeof(int), nullptr, GPU_USAGE_STREAM, __func__);
    data.resize(data_len);

    const GPUShaderCreateInfo *_info = gpu_shader_create_info_get(info_name);
    const ShaderCreateInfo &info = *reinterpret_cast<const ShaderCreateInfo *>(_info);
    is_graphic = info.compute_source_.is_empty();
    shader = GPU_shader_create_from_info_name(info_name);
    EXPECT_NE(shader, nullptr);
  }

  void validate()
  {
    if (is_graphic) {
      GPUFrameBuffer *fb = GPU_framebuffer_create("test_fb");
      GPU_framebuffer_default_size(fb, 1, 1);
      GPU_framebuffer_bind(fb);

      /* TODO(fclem): remove this boilerplate. */
      GPUVertFormat format{};
      GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U32, 1, GPU_FETCH_INT);
      GPUVertBuf *verts = GPU_vertbuf_create_with_format(&format);

      GPU_vertbuf_data_alloc(verts, 1);
      GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_POINTS, verts, nullptr, GPU_BATCH_OWNS_VBO);
      GPU_batch_set_shader(batch, shader);
      GPU_batch_draw_advanced(batch, 0, 1, 0, 1);
      GPU_batch_discard(batch);

      GPU_framebuffer_free(fb);
    }
    else {
      GPU_compute_dispatch(shader, 1, 1, 1);
    }

    GPU_finish();
    GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
    GPU_storagebuf_read(ssbo, data.data());

    EXPECT_EQ(data[0], int(float_in));
    EXPECT_EQ(data[1], int(uint_in));
    EXPECT_EQ(data[2], int(int_in));
    EXPECT_EQ(data[3], int(bool_in));
  }
};

static void test_specialization_constants_compute()
{
  ShaderSpecializationConst("gpu_compute_specialization_test");
}
GPU_TEST(specialization_constants_compute)

static void test_specialization_constants_graphic()
{
  ShaderSpecializationConst("gpu_graphic_specialization_test");
}
GPU_TEST(specialization_constants_graphic)

}  // namespace blender::gpu::tests
