/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_base.h"
#include "BLI_math_matrix_types.hh"
#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_context.hh"
#include "GPU_framebuffer.hh"
#include "GPU_index_buffer.hh"
#include "GPU_shader.hh"
#include "GPU_shader_shared.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"
#include "GPU_vertex_buffer.hh"
#include "GPU_vertex_format.hh"

#include "MEM_guardedalloc.h"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_shader_dependency_private.hh"
#include "gpu_testing.hh"

namespace blender::gpu::tests {

using namespace blender::gpu::shader;

static void test_shader_compute_2d()
{

  static constexpr uint SIZE = 512;

  /* Build compute shader. */
  gpu::Shader *shader = GPU_shader_create_from_info_name("gpu_compute_2d_test");
  EXPECT_NE(shader, nullptr);

  /* Create texture to store result and attach to shader. */
  blender::gpu::Texture *texture = GPU_texture_create_2d("gpu_shader_compute_2d",
                                                         SIZE,
                                                         SIZE,
                                                         1,
                                                         TextureFormat::SFLOAT_32_32_32_32,
                                                         GPU_TEXTURE_USAGE_GENERAL,
                                                         nullptr);
  EXPECT_NE(texture, nullptr);

  GPU_shader_bind(shader);
  GPU_texture_image_bind(texture, GPU_shader_get_sampler_binding(shader, "img_output"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, SIZE, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  float *data = static_cast<float *>(GPU_texture_read(texture, GPU_DATA_FLOAT, 0));
  EXPECT_NE(data, nullptr);
  for (int index = 0; index < SIZE * SIZE; index++) {
    EXPECT_FLOAT_EQ(data[index * 4 + 0], 1.0f);
    EXPECT_FLOAT_EQ(data[index * 4 + 1], 0.5f);
    EXPECT_FLOAT_EQ(data[index * 4 + 2], 0.2f);
    EXPECT_FLOAT_EQ(data[index * 4 + 3], 1.0f);
  }
  MEM_freeN(data);

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_texture_unbind(texture);
  GPU_texture_free(texture);
  GPU_shader_free(shader);
}
GPU_TEST(shader_compute_2d)

static void test_shader_compute_1d()
{
  static constexpr uint SIZE = 10;

  /* Build compute shader. */
  gpu::Shader *shader = GPU_shader_create_from_info_name("gpu_compute_1d_test");
  EXPECT_NE(shader, nullptr);

  /* Construct Texture. */
  blender::gpu::Texture *texture = GPU_texture_create_1d("gpu_shader_compute_1d",
                                                         SIZE,
                                                         1,
                                                         TextureFormat::SFLOAT_32_32_32_32,
                                                         GPU_TEXTURE_USAGE_GENERAL,
                                                         nullptr);
  EXPECT_NE(texture, nullptr);

  GPU_shader_bind(shader);
  GPU_texture_image_bind(texture, GPU_shader_get_sampler_binding(shader, "img_output"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, 1, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  /* Create texture to load back result. */
  float *data = static_cast<float *>(GPU_texture_read(texture, GPU_DATA_FLOAT, 0));
  EXPECT_NE(data, nullptr);
  for (int index = 0; index < SIZE; index++) {
    float expected_value = index;
    EXPECT_FLOAT_EQ(data[index * 4 + 0], expected_value);
    EXPECT_FLOAT_EQ(data[index * 4 + 1], expected_value);
    EXPECT_FLOAT_EQ(data[index * 4 + 2], expected_value);
    EXPECT_FLOAT_EQ(data[index * 4 + 3], expected_value);
  }
  MEM_freeN(data);

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_texture_unbind(texture);
  GPU_texture_free(texture);
  GPU_shader_free(shader);
}
GPU_TEST(shader_compute_1d)

static void test_shader_compute_vbo()
{
  static constexpr uint SIZE = 128;

  /* Build compute shader. */
  gpu::Shader *shader = GPU_shader_create_from_info_name("gpu_compute_vbo_test");
  EXPECT_NE(shader, nullptr);
  GPU_shader_bind(shader);

  /* Construct VBO. */
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32_32_32);
  VertBuf *vbo = GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_DEVICE_ONLY);
  GPU_vertbuf_data_alloc(*vbo, SIZE);
  GPU_vertbuf_bind_as_ssbo(vbo, GPU_shader_get_ssbo_binding(shader, "out_positions"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, 1, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);

  /* Download the vertex buffer. */
  float data[SIZE * 4];
  GPU_vertbuf_read(vbo, data);
  for (int index = 0; index < SIZE; index++) {
    float expected_value = index;
    EXPECT_FLOAT_EQ(data[index * 4 + 0], expected_value);
    EXPECT_FLOAT_EQ(data[index * 4 + 1], expected_value);
    EXPECT_FLOAT_EQ(data[index * 4 + 2], expected_value);
    EXPECT_FLOAT_EQ(data[index * 4 + 3], expected_value);
  }

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_vertbuf_discard(vbo);
  GPU_shader_free(shader);
}
GPU_TEST(shader_compute_vbo)

static void test_shader_compute_ibo()
{
  static constexpr uint SIZE = 128;

  /* Build compute shader. */
  gpu::Shader *shader = GPU_shader_create_from_info_name("gpu_compute_ibo_test");
  EXPECT_NE(shader, nullptr);
  GPU_shader_bind(shader);

  /* Construct IBO. */
  IndexBuf *ibo = GPU_indexbuf_build_on_device(SIZE);
  GPU_indexbuf_bind_as_ssbo(ibo, GPU_shader_get_ssbo_binding(shader, "out_indices"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, 1, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);

  /* Download the index buffer. */
  uint32_t data[SIZE];
  GPU_indexbuf_read(ibo, data);
  for (int index = 0; index < SIZE; index++) {
    uint32_t expected = index;
    EXPECT_EQ(data[index], expected);
  }

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_indexbuf_discard(ibo);
  GPU_shader_free(shader);
}
GPU_TEST(shader_compute_ibo)

static void test_shader_compute_ssbo()
{
  static constexpr uint SIZE = 128;

  /* Build compute shader. */
  gpu::Shader *shader = GPU_shader_create_from_info_name("gpu_compute_ssbo_test");
  EXPECT_NE(shader, nullptr);
  GPU_shader_bind(shader);

  /* Construct SSBO. */
  StorageBuf *ssbo = GPU_storagebuf_create_ex(
      SIZE * sizeof(uint32_t), nullptr, GPU_USAGE_DEVICE_ONLY, __func__);
  GPU_storagebuf_bind(ssbo, GPU_shader_get_ssbo_binding(shader, "data_out"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, 1, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);

  /* Download the storage buffer. */
  uint32_t data[SIZE];
  GPU_storagebuf_read(ssbo, data);
  for (int index = 0; index < SIZE; index++) {
    uint32_t expected = index * 4;
    EXPECT_EQ(data[index], expected);
  }

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_storagebuf_free(ssbo);
  GPU_shader_free(shader);
}
GPU_TEST(shader_compute_ssbo)

static void test_shader_ssbo_binding()
{
  /* Build compute shader. */
  gpu::Shader *shader = GPU_shader_create_from_info_name("gpu_compute_ssbo_binding_test");
  EXPECT_NE(shader, nullptr);

  /* Perform tests. */
  EXPECT_EQ(0, GPU_shader_get_ssbo_binding(shader, "data0"));
  EXPECT_EQ(1, GPU_shader_get_ssbo_binding(shader, "data1"));

  /* Cleanup. */
  GPU_shader_free(shader);
}
GPU_TEST(shader_ssbo_binding)

#ifdef WITH_METAL_BACKEND
static void test_shader_sampler_argument_buffer_binding()
{
  gpu::Shader *shader = GPU_shader_create_from_info_name("gpu_sampler_arg_buf_test");
  EXPECT_NE(shader, nullptr);

  gpu::StorageBuf *ssbo = GPU_storagebuf_create(sizeof(float) * 4 * 18);

  GPU_storagebuf_bind(ssbo, GPU_shader_get_ssbo_binding(shader, "data_out"));

  blender::float4 tx_data(-1.0f, 1.0f, 2.0f, 3.0f);
  blender::gpu::Texture *tex = GPU_texture_create_2d(
      "tx", 1, 1, 1, TextureFormat::SFLOAT_32_32_32_32, GPU_TEXTURE_USAGE_SHADER_READ, &tx_data.x);

  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_1"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_2"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_3"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_4"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_5"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_6"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_7"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_8"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_9"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_10"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_11"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_12"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_13"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_14"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_15"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_16"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_17"));
  GPU_texture_bind(tex, GPU_shader_get_sampler_binding(shader, "tex_18"));

  gpu::FrameBuffer *fb = GPU_framebuffer_create("test_fb");
  GPU_framebuffer_default_size(fb, 1, 1);
  GPU_framebuffer_bind(fb);

  Batch *batch = GPU_batch_create_procedural(GPU_PRIM_POINTS, 3);

  GPU_batch_set_shader(batch, shader);
  GPU_batch_draw(batch);

  GPU_batch_discard(batch);

  GPU_finish();

  float4 data[18];
  GPU_storagebuf_read(ssbo, &data);

  for (int index = 0; index < 18; index++) {
    EXPECT_EQ(data[index], tx_data);
  }

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_framebuffer_free(fb);
  GPU_storagebuf_free(ssbo);
  GPU_texture_free(tex);
  GPU_shader_free(shader);
}
GPU_TEST(shader_sampler_argument_buffer_binding)
#endif

static void test_shader_texture_atomic()
{
  gpu::Shader *shader = GPU_shader_create_from_info_name("gpu_texture_atomic_test");
  EXPECT_NE(shader, nullptr);

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                           GPU_TEXTURE_USAGE_ATOMIC;
  uint32_t tx_data[4] = {0u, 0u, 0u, 0u};
  blender::gpu::Texture *tex_2d = GPU_texture_create_2d(
      "tex_2d", 1, 1, 1, TextureFormat::UINT_32, usage, nullptr);
  blender::gpu::Texture *tex_2d_array = GPU_texture_create_2d_array(
      "tex_2d_array", 1, 1, 2, 1, TextureFormat::UINT_32, usage, nullptr);
  blender::gpu::Texture *tex_3d = GPU_texture_create_3d(
      "tex_3d", 1, 1, 2, 1, TextureFormat::UINT_32, usage, nullptr);

  GPU_texture_clear(tex_2d, eGPUDataFormat::GPU_DATA_UINT, &tx_data[0]);
  GPU_texture_clear(tex_2d_array, eGPUDataFormat::GPU_DATA_UINT, &tx_data[0]);
  GPU_texture_clear(tex_3d, eGPUDataFormat::GPU_DATA_UINT, &tx_data[0]);

  GPU_texture_image_bind(tex_2d, GPU_shader_get_sampler_binding(shader, "img_atomic_2D"));
  GPU_texture_image_bind(tex_2d_array,
                         GPU_shader_get_sampler_binding(shader, "img_atomic_2D_array"));
  GPU_texture_image_bind(tex_3d, GPU_shader_get_sampler_binding(shader, "img_atomic_3D"));

  gpu::StorageBuf *ssbo = GPU_storagebuf_create(sizeof(uint32_t) * 5);
  GPU_storagebuf_bind(ssbo, GPU_shader_get_ssbo_binding(shader, "data_out"));

  GPU_shader_bind(shader);
  GPU_shader_uniform_1b(shader, "write_phase", true);
  GPU_compute_dispatch(shader, 1, 1, 1);
  GPU_memory_barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);

  /* We can't host read atomic texture. So we do a manual read phase to a SSBO. */
  GPU_shader_uniform_1b(shader, "write_phase", false);
  GPU_compute_dispatch(shader, 1, 1, 1);
  GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
  GPU_finish();

  uint32_t data[5];
  GPU_storagebuf_read(ssbo, &data);

  EXPECT_EQ(data[0], 0xFFFFFFFFu);
  EXPECT_EQ(data[1], 0xFFFFFFFFu);
  EXPECT_EQ(data[2], 0xFFFFFFFFu);
  EXPECT_EQ(data[3], 0xFFFFFFFFu);
  EXPECT_EQ(data[4], 0xFFFFFFFFu);

  /* Cleanup. */
  GPU_texture_free(tex_2d);
  GPU_texture_free(tex_2d_array);
  GPU_texture_free(tex_3d);
  GPU_storagebuf_free(ssbo);
  GPU_shader_unbind();
  GPU_shader_free(shader);
}
GPU_TEST(shader_texture_atomic)

static std::string print_test_data(const TestOutputRawData &raw, TestType type)
{
  std::stringstream ss;
  switch (type) {
    case TEST_TYPE_BOOL:
    case TEST_TYPE_UINT:
      ss << *reinterpret_cast<const uint *>(&raw);
      break;
    case TEST_TYPE_INT:
      ss << *reinterpret_cast<const int *>(&raw);
      break;
    case TEST_TYPE_FLOAT:
      ss << *reinterpret_cast<const float *>(&raw);
      break;
    case TEST_TYPE_IVEC2:
      ss << *reinterpret_cast<const int2 *>(&raw);
      break;
    case TEST_TYPE_IVEC3:
      ss << *reinterpret_cast<const int3 *>(&raw);
      break;
    case TEST_TYPE_IVEC4:
      ss << *reinterpret_cast<const int4 *>(&raw);
      break;
    case TEST_TYPE_UVEC2:
      ss << *reinterpret_cast<const uint2 *>(&raw);
      break;
    case TEST_TYPE_UVEC3:
      ss << *reinterpret_cast<const uint3 *>(&raw);
      break;
    case TEST_TYPE_UVEC4:
      ss << *reinterpret_cast<const uint4 *>(&raw);
      break;
    case TEST_TYPE_VEC2:
      ss << *reinterpret_cast<const float2 *>(&raw);
      break;
    case TEST_TYPE_VEC3:
      ss << *reinterpret_cast<const float3 *>(&raw);
      break;
    case TEST_TYPE_VEC4:
      ss << *reinterpret_cast<const float4 *>(&raw);
      break;
    case TEST_TYPE_MAT2X2:
      ss << *reinterpret_cast<const float2x2 *>(&raw);
      break;
    case TEST_TYPE_MAT2X3:
      ss << *reinterpret_cast<const float2x3 *>(&raw);
      break;
    case TEST_TYPE_MAT2X4:
      ss << *reinterpret_cast<const float2x4 *>(&raw);
      break;
    case TEST_TYPE_MAT3X2:
      ss << *reinterpret_cast<const float3x2 *>(&raw);
      break;
    case TEST_TYPE_MAT3X3:
      ss << *reinterpret_cast<const float3x3 *>(&raw);
      break;
    case TEST_TYPE_MAT3X4:
      ss << *reinterpret_cast<const float3x4 *>(&raw);
      break;
    case TEST_TYPE_MAT4X2:
      ss << *reinterpret_cast<const float4x2 *>(&raw);
      break;
    case TEST_TYPE_MAT4X3:
      ss << *reinterpret_cast<const float4x3 *>(&raw);
      break;
    case TEST_TYPE_MAT4X4:
      ss << *reinterpret_cast<const float4x4 *>(&raw);
      break;
    default:
      ss << *reinterpret_cast<const MatBase<uint, 4, 4> *>(&raw);
      break;
  }
  return ss.str();
}

static StringRef print_test_line(StringRefNull test_src, int64_t test_line)
{
  /* Start at line one like the line report scheme.
   * However, our preprocessor adds a line directive at the top of the file. Skip it. */
  int64_t line = 1 - 1;
  int64_t last_pos = 0;
  int64_t pos = 0;
  while ((pos = test_src.find('\n', pos)) != std::string::npos) {
    if (line == test_line) {
      return test_src.substr(last_pos, pos - last_pos);
    }
    pos += 1; /* Skip newline */
    last_pos = pos;
    line++;
  }
  return "";
}

static void gpu_shader_lib_test(StringRefNull test_src_name, const char *additional_info = nullptr)
{
  using namespace shader;

  GPU_render_begin();

  std::string create_info_name = test_src_name.substr(0, test_src_name.find('.'));

  ShaderCreateInfo create_info(create_info_name.c_str());
  create_info.builtins(BuiltinBits::FRAG_COORD);
  create_info.fragment_source(test_src_name);
  create_info.additional_info("gpu_shader_test");
  if (additional_info) {
    create_info.additional_info(additional_info);
  }

  StringRefNull test_src = gpu_shader_dependency_get_source(test_src_name);

  gpu::Shader *shader = GPU_shader_create_from_info(
      reinterpret_cast<GPUShaderCreateInfo *>(&create_info));

  int test_count = 0;
  /* Count tests. */
  int64_t pos = 0;
  StringRefNull target = "EXPECT_";
  while ((pos = test_src.find(target, pos)) != std::string::npos) {
    test_count++;
    pos += sizeof("EXPECT_");
  }

  int test_output_px_len = divide_ceil_u(sizeof(TestOutput), 4 * 4);

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *tex = GPU_texture_create_2d(
      "tx", test_output_px_len, test_count, 1, TextureFormat::UINT_32_32_32_32, usage, nullptr);
  gpu::FrameBuffer *fb = GPU_framebuffer_create("test_fb");
  GPU_framebuffer_ensure_config(&fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(tex)});
  GPU_framebuffer_bind(fb);

  Batch *batch = GPU_batch_create_procedural(GPU_PRIM_TRIS, 3);

  GPU_batch_set_shader(batch, shader);
  GPU_batch_draw(batch);

  GPU_batch_discard(batch);

  GPU_finish();

  TestOutput *test_data = (TestOutput *)GPU_texture_read(tex, GPU_DATA_UINT, 0);
  Span<TestOutput> tests(test_data, test_count);

  for (const TestOutput &test : tests) {
    if (ELEM(test.status, TEST_STATUS_NONE, TEST_STATUS_PASSED)) {
      continue;
    }
    if (test.status == TEST_STATUS_FAILED) {
      ADD_FAILURE_AT(test_src_name.c_str(), test.line)
          << "Value of: " << print_test_line(test_src, test.line) << "\n"
          << "  Actual: " << print_test_data(test.expect, TestType(test.type)) << "\n"
          << "Expected: " << print_test_data(test.result, TestType(test.type)) << "\n";
    }
    else {
      BLI_assert_unreachable();
    }
  }

  MEM_freeN(test_data);

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_shader_free(shader);
  GPU_framebuffer_free(fb);
  GPU_texture_free(tex);

  GPU_render_end();
}

static void test_math_lib()
{
  gpu_shader_lib_test("gpu_math_test.glsl");
}
GPU_TEST(math_lib)

static void test_eevee_lib()
{
  /* TODO(fclem): Not passing currently. Need to be updated. */
  // gpu_shader_lib_test("eevee_shadow_test.glsl", "eevee_tests_data");
  gpu_shader_lib_test("eevee_occupancy_test.glsl");
  gpu_shader_lib_test("eevee_horizon_scan_test.glsl");
#ifndef __APPLE__ /* PSOs fail to compile on Mac. Try to port them to compute shader to see if it \
                   * fixes the issue. */
  gpu_shader_lib_test("eevee_gbuffer_normal_test.glsl", "eevee_tests_data");
  gpu_shader_lib_test("eevee_gbuffer_closure_test.glsl", "eevee_tests_data");
#endif
}
GPU_TEST(eevee_lib)

}  // namespace blender::gpu::tests
