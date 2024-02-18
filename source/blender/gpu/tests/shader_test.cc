/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_matrix_types.hh"
#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_context.h"
#include "GPU_framebuffer.h"
#include "GPU_index_buffer.h"
#include "GPU_shader.h"
#include "GPU_shader_shared.h"
#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"
#include "GPU_vertex_format.h"

#include "MEM_guardedalloc.h"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_create_info_private.hh"
#include "gpu_shader_dependency_private.h"
#include "gpu_testing.hh"

namespace blender::gpu::tests {

using namespace blender::gpu::shader;

static void test_shader_compute_2d()
{

  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 512;

  /* Build compute shader. */
  GPUShader *shader = GPU_shader_create_from_info_name("gpu_compute_2d_test");
  EXPECT_NE(shader, nullptr);

  /* Create texture to store result and attach to shader. */
  GPUTexture *texture = GPU_texture_create_2d(
      "gpu_shader_compute_2d", SIZE, SIZE, 1, GPU_RGBA32F, GPU_TEXTURE_USAGE_GENERAL, nullptr);
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

  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 10;

  /* Build compute shader. */
  GPUShader *shader = GPU_shader_create_from_info_name("gpu_compute_1d_test");
  EXPECT_NE(shader, nullptr);

  /* Construct Texture. */
  GPUTexture *texture = GPU_texture_create_1d(
      "gpu_shader_compute_1d", SIZE, 1, GPU_RGBA32F, GPU_TEXTURE_USAGE_GENERAL, nullptr);
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

  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 128;

  /* Build compute shader. */
  GPUShader *shader = GPU_shader_create_from_info_name("gpu_compute_vbo_test");
  EXPECT_NE(shader, nullptr);
  GPU_shader_bind(shader);

  /* Construct VBO. */
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  GPUVertBuf *vbo = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_DEVICE_ONLY);
  GPU_vertbuf_data_alloc(vbo, SIZE);
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

  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 128;

  /* Build compute shader. */
  GPUShader *shader = GPU_shader_create_from_info_name("gpu_compute_ibo_test");
  EXPECT_NE(shader, nullptr);
  GPU_shader_bind(shader);

  /* Construct IBO. */
  GPUIndexBuf *ibo = GPU_indexbuf_build_on_device(SIZE);
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

  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 128;

  /* Build compute shader. */
  GPUShader *shader = GPU_shader_create_from_info_name("gpu_compute_ssbo_test");
  EXPECT_NE(shader, nullptr);
  GPU_shader_bind(shader);

  /* Construct SSBO. */
  GPUStorageBuf *ssbo = GPU_storagebuf_create_ex(
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
  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  /* Build compute shader. */
  GPUShader *shader = GPU_shader_create_from_info_name("gpu_compute_ssbo_binding_test");
  EXPECT_NE(shader, nullptr);

  /* Perform tests. */
  EXPECT_EQ(0, GPU_shader_get_ssbo_binding(shader, "data0"));
  EXPECT_EQ(1, GPU_shader_get_ssbo_binding(shader, "data1"));

  /* Cleanup. */
  GPU_shader_free(shader);
}
GPU_TEST(shader_ssbo_binding)

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
  /* Start at line one like the line report scheme. */
  int64_t line = 1;
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

static void gpu_shader_lib_test(const char *test_src_name, const char *additional_info = nullptr)
{
  using namespace shader;

  GPU_render_begin();

  ShaderCreateInfo create_info(test_src_name);
  create_info.fragment_source(test_src_name);
  create_info.additional_info("gpu_shader_test");
  if (additional_info) {
    create_info.additional_info(additional_info);
  }

  StringRefNull test_src = gpu_shader_dependency_get_source(test_src_name);

  GPUShader *shader = GPU_shader_create_from_info(
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
  GPUTexture *tex = GPU_texture_create_2d(
      "tx", test_output_px_len, test_count, 1, GPU_RGBA32UI, usage, nullptr);
  GPUFrameBuffer *fb = GPU_framebuffer_create("test_fb");
  GPU_framebuffer_ensure_config(&fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(tex)});
  GPU_framebuffer_bind(fb);

  /* TODO(fclem): remove this boilerplate. */
  GPUVertFormat format{};
  GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U32, 1, GPU_FETCH_INT);
  GPUVertBuf *verts = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(verts, 3);
  GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_TRIS, verts, nullptr, GPU_BATCH_OWNS_VBO);

  GPU_batch_set_shader(batch, shader);
  GPU_batch_draw_advanced(batch, 0, 3, 0, 1);

  GPU_batch_discard(batch);

  GPU_finish();

  TestOutput *test_data = (TestOutput *)GPU_texture_read(tex, GPU_DATA_UINT, 0);
  Span<TestOutput> tests(test_data, test_count);

  for (const TestOutput &test : tests) {
    if (ELEM(test.status, TEST_STATUS_NONE, TEST_STATUS_PASSED)) {
      continue;
    }
    else if (test.status == TEST_STATUS_FAILED) {
      ADD_FAILURE_AT(test_src_name, test.line)
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
  // gpu_shader_lib_test("eevee_shadow_test.glsl", "eevee_shared");
  gpu_shader_lib_test("eevee_occupancy_test.glsl");
  gpu_shader_lib_test("eevee_horizon_scan_test.glsl");
  gpu_shader_lib_test("eevee_gbuffer_normal_test.glsl", "eevee_shared");
  gpu_shader_lib_test("eevee_gbuffer_closure_test.glsl", "eevee_shared");
}
GPU_TEST(eevee_lib)

}  // namespace blender::gpu::tests
