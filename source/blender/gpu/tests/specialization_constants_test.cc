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
#include "GPU_state.hh"
#include "GPU_storage_buffer.hh"
#include "GPU_vertex_format.hh"

#include "BLI_vector.hh"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_testing.hh"

namespace blender::gpu::tests {

struct ShaderSpecializationConst {
  gpu::Shader *shader = nullptr;
  StorageBuf *ssbo = nullptr;
  Vector<int> data;

  float float_in;
  uint uint_in;
  int int_in;
  bool bool_in;

  bool is_graphic = false;

  ShaderSpecializationConst(const char *info_name)
  {
    GPU_render_begin();

    this->init_shader(info_name);

    GPU_storagebuf_bind(ssbo, GPU_shader_get_ssbo_binding(shader, "data_out"));

    /* Test values. */
    float_in = 52;
    uint_in = 324;
    int_in = 455;
    bool_in = false;

    int float_in_loc = GPU_shader_get_constant(shader, "float_in");
    int uint_in_loc = GPU_shader_get_constant(shader, "uint_in");
    int int_in_loc = GPU_shader_get_constant(shader, "int_in");
    int bool_in_loc = GPU_shader_get_constant(shader, "bool_in");

    shader::SpecializationConstants constants = GPU_shader_get_default_constant_state(shader);
    constants.set_value(float_in_loc, float_in);
    constants.set_value(uint_in_loc, uint_in);
    constants.set_value(int_in_loc, int_in);
    constants.set_value(bool_in_loc, bool_in);

    this->validate(constants);

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

  void validate(shader::SpecializationConstants &constants)
  {
    if (is_graphic) {
      gpu::FrameBuffer *fb = GPU_framebuffer_create("test_fb");
      GPU_framebuffer_default_size(fb, 1, 1);
      GPU_framebuffer_bind(fb);

      Batch *batch = GPU_batch_create_procedural(GPU_PRIM_POINTS, 1);

      GPU_batch_set_shader(batch, shader, &constants);
      GPU_batch_draw_advanced(batch, 0, 1, 0, 1);
      GPU_batch_discard(batch);

      GPU_framebuffer_free(fb);
    }
    else {
      GPU_compute_dispatch(shader, 1, 1, 1, &constants);
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
