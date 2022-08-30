/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_capabilities.h"
#include "GPU_compute.h"
#include "GPU_index_buffer.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"
#include "GPU_vertex_format.h"

#include "MEM_guardedalloc.h"

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

}  // namespace blender::gpu::tests
