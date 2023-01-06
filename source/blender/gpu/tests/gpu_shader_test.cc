/* SPDX-License-Identifier: Apache-2.0 */

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
#include "gpu_shader_dependency_private.h"
#include "gpu_testing.hh"

namespace blender::gpu::tests {

static void test_gpu_shader_compute_2d()
{

  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 512;

  /* Build compute shader. */
  const char *compute_glsl = R"(

layout(local_size_x = 1, local_size_y = 1) in;
layout(rgba32f, binding = 0) uniform image2D img_output;

void main() {
  vec4 pixel = vec4(1.0, 0.5, 0.2, 1.0);
  imageStore(img_output, ivec2(gl_GlobalInvocationID.xy), pixel);
}

)";

  GPUShader *shader = GPU_shader_create_compute(
      compute_glsl, nullptr, nullptr, "gpu_shader_compute_2d");
  EXPECT_NE(shader, nullptr);

  /* Create texture to store result and attach to shader. */
  GPUTexture *texture = GPU_texture_create_2d(
      "gpu_shader_compute_2d", SIZE, SIZE, 1, GPU_RGBA32F, nullptr);
  EXPECT_NE(texture, nullptr);

  GPU_shader_bind(shader);
  GPU_texture_image_bind(texture, GPU_shader_get_texture_binding(shader, "img_output"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, SIZE, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);
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
GPU_TEST(gpu_shader_compute_2d)

static void test_gpu_shader_compute_1d()
{

  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 10;

  /* Build compute shader. */
  const char *compute_glsl = R"(

layout(local_size_x = 1) in;

layout(rgba32f, binding = 1) uniform image1D outputVboData;

void main() {
  int index = int(gl_GlobalInvocationID.x);
  vec4 pos = vec4(gl_GlobalInvocationID.x);
  imageStore(outputVboData, index, pos);
}

)";

  GPUShader *shader = GPU_shader_create_compute(
      compute_glsl, nullptr, nullptr, "gpu_shader_compute_1d");
  EXPECT_NE(shader, nullptr);

  /* Construct Texture. */
  GPUTexture *texture = GPU_texture_create_1d(
      "gpu_shader_compute_1d", SIZE, 1, GPU_RGBA32F, nullptr);
  EXPECT_NE(texture, nullptr);

  GPU_shader_bind(shader);
  GPU_texture_image_bind(texture, GPU_shader_get_texture_binding(shader, "outputVboData"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, 1, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);

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
GPU_TEST(gpu_shader_compute_1d)

static void test_gpu_shader_compute_vbo()
{

  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 128;

  /* Build compute shader. */
  const char *compute_glsl = R"(

layout(local_size_x = 1) in;

layout(std430, binding = 0) writeonly buffer outputVboData
{
  vec4 out_positions[];
};

void main() {
  uint index = gl_GlobalInvocationID.x;
  vec4 pos = vec4(gl_GlobalInvocationID.x);
  out_positions[index] = pos;
}

)";

  GPUShader *shader = GPU_shader_create_compute(
      compute_glsl, nullptr, nullptr, "gpu_shader_compute_vbo");
  EXPECT_NE(shader, nullptr);
  GPU_shader_bind(shader);

  /* Construct VBO. */
  static GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  GPUVertBuf *vbo = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_DEVICE_ONLY);
  GPU_vertbuf_data_alloc(vbo, SIZE);
  GPU_vertbuf_bind_as_ssbo(vbo, GPU_shader_get_ssbo(shader, "outputVboData"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, 1, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);

  /* Download the vertex buffer. */
  const float *data = static_cast<const float *>(GPU_vertbuf_read(vbo));
  ASSERT_NE(data, nullptr);
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
GPU_TEST(gpu_shader_compute_vbo)

static void test_gpu_shader_compute_ibo()
{

  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  static constexpr uint SIZE = 128;

  /* Build compute shader. */
  const char *compute_glsl = R"(

layout(local_size_x = 1) in;

layout(std430, binding = 1) writeonly buffer outputIboData
{
  uint out_indexes[];
};

void main() {
  uint store_index = int(gl_GlobalInvocationID.x);
  out_indexes[store_index] = store_index;
}

)";

  GPUShader *shader = GPU_shader_create_compute(
      compute_glsl, nullptr, nullptr, "gpu_shader_compute_vbo");
  EXPECT_NE(shader, nullptr);
  GPU_shader_bind(shader);

  /* Construct IBO. */
  GPUIndexBuf *ibo = GPU_indexbuf_build_on_device(SIZE);
  GPU_indexbuf_bind_as_ssbo(ibo, GPU_shader_get_ssbo(shader, "outputIboData"));

  /* Dispatch compute task. */
  GPU_compute_dispatch(shader, SIZE, 1, 1);

  /* Check if compute has been done. */
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);

  /* Download the index buffer. */
  const uint32_t *data = GPU_indexbuf_read(ibo);
  ASSERT_NE(data, nullptr);
  for (int index = 0; index < SIZE; index++) {
    uint32_t expected = index;
    EXPECT_EQ(data[index], expected);
  }

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_indexbuf_discard(ibo);
  GPU_shader_free(shader);
}
GPU_TEST(gpu_shader_compute_ibo)

static void test_gpu_shader_ssbo_binding()
{
  if (!GPU_compute_shader_support()) {
    /* We can't test as a the platform does not support compute shaders. */
    std::cout << "Skipping compute shader test: platform not supported";
    return;
  }

  /* Build compute shader. */
  const char *compute_glsl = R"(

layout(local_size_x = 1) in;

layout(std430, binding = 0) buffer ssboBinding0
{
  int data0[];
};
layout(std430, binding = 1) buffer ssboBinding1
{
  int data1[];
};

void main() {
}

)";

  GPUShader *shader = GPU_shader_create_compute(compute_glsl, nullptr, nullptr, "gpu_shader_ssbo");
  EXPECT_NE(shader, nullptr);
  GPU_shader_bind(shader);

  EXPECT_EQ(0, GPU_shader_get_ssbo(shader, "ssboBinding0"));
  EXPECT_EQ(1, GPU_shader_get_ssbo(shader, "ssboBinding1"));

  /* Cleanup. */
  GPU_shader_unbind();
  GPU_shader_free(shader);
}
GPU_TEST(gpu_shader_ssbo_binding)

static void test_gpu_texture_read()
{
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *rgba32u = GPU_texture_create_2d_ex("rgba32u", 1, 1, 1, GPU_RGBA32UI, usage, nullptr);
  GPUTexture *rgba16u = GPU_texture_create_2d_ex("rgba16u", 1, 1, 1, GPU_RGBA16UI, usage, nullptr);
  GPUTexture *rgba32f = GPU_texture_create_2d_ex("rgba32f", 1, 1, 1, GPU_RGBA32F, usage, nullptr);

  const float4 fcol = {0.0f, 1.3f, -231.0f, 1000.0f};
  const uint4 ucol = {0, 1, 2, 12223};
  GPU_texture_clear(rgba32u, GPU_DATA_UINT, ucol);
  GPU_texture_clear(rgba16u, GPU_DATA_UINT, ucol);
  GPU_texture_clear(rgba32f, GPU_DATA_FLOAT, fcol);

  GPU_finish();

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
GPU_TEST(gpu_texture_read)

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

static void gpu_shader_lib_test(const char *test_src_name)
{
  using namespace shader;

  GPU_render_begin();

  ShaderCreateInfo create_info(test_src_name);
  create_info.fragment_source(test_src_name);
  create_info.additional_info("gpu_shader_test");

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
  GPUTexture *tex = GPU_texture_create_2d_ex(
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

static void test_gpu_math_lib()
{
  gpu_shader_lib_test("gpu_math_test.glsl");
}
GPU_TEST(gpu_math_lib)

}  // namespace blender::gpu::tests
