/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_context.hh"
#include "GPU_framebuffer.hh"
#include "GPU_shader.hh"
#include "gpu_testing.hh"

#include "BLI_math_vector.hh"

#include "gpu_shader_create_info.hh"

namespace blender::gpu::tests {

static void test_framebuffer_clear_color_single_attachment()
{
  const int2 size(10, 10);
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture = GPU_texture_create_2d(
      __func__, UNPACK2(size), 1, GPU_RGBA32F, usage, nullptr);

  GPUFrameBuffer *framebuffer = GPU_framebuffer_create(__func__);
  GPU_framebuffer_ensure_config(&framebuffer,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(texture)});
  GPU_framebuffer_bind(framebuffer);

  const float4 clear_color(0.1f, 0.2f, 0.5f, 1.0f);
  GPU_framebuffer_clear_color(framebuffer, clear_color);
  GPU_finish();

  float4 *read_data = static_cast<float4 *>(GPU_texture_read(texture, GPU_DATA_FLOAT, 0));
  for (float4 pixel_color : Span<float4>(read_data, size.x * size.y)) {
    EXPECT_EQ(pixel_color, clear_color);
  }
  MEM_freeN(read_data);

  GPU_framebuffer_free(framebuffer);
  GPU_texture_free(texture);
}
GPU_TEST(framebuffer_clear_color_single_attachment);

static void test_framebuffer_clear_color_multiple_attachments()
{
  const int2 size(10, 10);
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture1 = GPU_texture_create_2d(
      __func__, UNPACK2(size), 1, GPU_RGBA32F, usage, nullptr);
  GPUTexture *texture2 = GPU_texture_create_2d(
      __func__, UNPACK2(size), 1, GPU_RGBA32UI, usage, nullptr);

  GPUFrameBuffer *framebuffer = GPU_framebuffer_create(__func__);
  GPU_framebuffer_ensure_config(
      &framebuffer,
      {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(texture1), GPU_ATTACHMENT_TEXTURE(texture2)});
  GPU_framebuffer_bind(framebuffer);

  const float4 clear_color(0.1f, 0.2f, 0.5f, 1.0f);
  GPU_framebuffer_clear_color(framebuffer, clear_color);
  GPU_finish();

  float4 *read_data1 = static_cast<float4 *>(GPU_texture_read(texture1, GPU_DATA_FLOAT, 0));
  for (float4 pixel_color : Span<float4>(read_data1, size.x * size.y)) {
    EXPECT_EQ(pixel_color, clear_color);
  }
  MEM_freeN(read_data1);

  uint4 *read_data2 = static_cast<uint4 *>(GPU_texture_read(texture2, GPU_DATA_UINT, 0));
  uint4 clear_color_uint(1036831949, 1045220557, 1056964608, 1065353216);
  for (uint4 pixel_color : Span<uint4>(read_data2, size.x * size.y)) {
    EXPECT_EQ(pixel_color, clear_color_uint);
  }
  MEM_freeN(read_data2);

  GPU_framebuffer_free(framebuffer);
  GPU_texture_free(texture1);
  GPU_texture_free(texture2);
}
GPU_TEST(framebuffer_clear_color_multiple_attachments);

static void test_framebuffer_clear_multiple_color_multiple_attachments()
{
  const int2 size(10, 10);
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture1 = GPU_texture_create_2d(
      __func__, UNPACK2(size), 1, GPU_RGBA32F, usage, nullptr);
  GPUTexture *texture2 = GPU_texture_create_2d(
      __func__, UNPACK2(size), 1, GPU_RGBA32F, usage, nullptr);

  GPUFrameBuffer *framebuffer = GPU_framebuffer_create(__func__);
  GPU_framebuffer_ensure_config(
      &framebuffer,
      {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(texture1), GPU_ATTACHMENT_TEXTURE(texture2)});
  GPU_framebuffer_bind(framebuffer);

  const float4 clear_color[2] = {float4(0.1f, 0.2f, 0.5f, 1.0f), float4(0.5f, 0.2f, 0.1f, 1.0f)};
  GPU_framebuffer_multi_clear(
      framebuffer, static_cast<const float(*)[4]>(static_cast<const void *>(clear_color)));
  GPU_finish();

  float4 *read_data1 = static_cast<float4 *>(GPU_texture_read(texture1, GPU_DATA_FLOAT, 0));
  for (float4 pixel_color : Span<float4>(read_data1, size.x * size.y)) {
    EXPECT_EQ(pixel_color, clear_color[0]);
  }
  MEM_freeN(read_data1);

  float4 *read_data2 = static_cast<float4 *>(GPU_texture_read(texture2, GPU_DATA_FLOAT, 0));
  for (float4 pixel_color : Span<float4>(read_data1, size.x * size.y)) {
    EXPECT_EQ(pixel_color, clear_color[1]);
  }
  MEM_freeN(read_data2);

  GPU_framebuffer_free(framebuffer);
  GPU_texture_free(texture1);
  GPU_texture_free(texture2);
}
GPU_TEST(framebuffer_clear_multiple_color_multiple_attachments);

static void test_framebuffer_clear_depth()
{
  const int2 size(10, 10);
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture = GPU_texture_create_2d(
      __func__, UNPACK2(size), 1, GPU_DEPTH_COMPONENT32F, usage, nullptr);

  GPUFrameBuffer *framebuffer = GPU_framebuffer_create(__func__);
  GPU_framebuffer_ensure_config(&framebuffer, {GPU_ATTACHMENT_TEXTURE(texture)});
  GPU_framebuffer_bind(framebuffer);

  const float clear_depth = 0.5f;
  GPU_framebuffer_clear_depth(framebuffer, clear_depth);
  GPU_finish();

  float *read_data = static_cast<float *>(GPU_texture_read(texture, GPU_DATA_FLOAT, 0));
  for (float pixel_depth : Span<float>(read_data, size.x * size.y)) {
    EXPECT_EQ(pixel_depth, clear_depth);
  }
  MEM_freeN(read_data);

  GPU_framebuffer_free(framebuffer);
  GPU_texture_free(texture);
}
GPU_TEST(framebuffer_clear_depth);

static void test_framebuffer_scissor_test()
{
  const int2 size(128, 128);
  const int bar_size = 16;
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture = GPU_texture_create_2d(
      __func__, UNPACK2(size), 1, GPU_RGBA32F, usage, nullptr);

  GPUFrameBuffer *framebuffer = GPU_framebuffer_create(__func__);
  GPU_framebuffer_ensure_config(&framebuffer,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(texture)});
  GPU_framebuffer_bind(framebuffer);

  const float4 color1(0.0f);
  const float4 color2(0.5f);
  const float4 color3(1.0f);
  GPU_framebuffer_clear_color(framebuffer, color1);

  GPU_scissor_test(true);
  for (int x = 0; x < size.x; x += 2 * bar_size) {
    GPU_scissor(x, 0, bar_size, size.y);
    GPU_framebuffer_clear_color(framebuffer, color2);
  }
  for (int y = 0; y < size.y; y += 2 * bar_size) {
    GPU_scissor(0, y, size.x, bar_size);
    GPU_framebuffer_clear_color(framebuffer, color3);
  }
  GPU_scissor_test(false);
  GPU_finish();

  float4 *read_data = static_cast<float4 *>(GPU_texture_read(texture, GPU_DATA_FLOAT, 0));
  int offset = 0;
  for (float4 pixel_color : Span<float4>(read_data, size.x * size.y)) {
    int x = offset % size.x;
    int y = offset / size.x;
    int bar_x = x / bar_size;
    int bar_y = y / bar_size;

    if (bar_y % 2 == 0) {
      EXPECT_EQ(pixel_color, color3);
    }
    else {
      if (bar_x % 2 == 0) {
        EXPECT_EQ(pixel_color, color2);
      }
      else {
        EXPECT_EQ(pixel_color, color1);
      }
    }

    offset++;
  }
  MEM_freeN(read_data);

  GPU_framebuffer_free(framebuffer);
  GPU_texture_free(texture);
}
GPU_TEST(framebuffer_scissor_test);

/* Color each side of a cube-map with a different color. */
static void test_framebuffer_cube()
{
  const int SIZE = 32;
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *tex = GPU_texture_create_cube("tex", SIZE, 1, GPU_RGBA32F, usage, nullptr);

  const float4 clear_colors[6] = {
      {0.5f, 0.0f, 0.0f, 1.0f},
      {1.0f, 0.0f, 0.0f, 1.0f},
      {0.0f, 0.5f, 0.0f, 1.0f},
      {0.0f, 1.0f, 0.0f, 1.0f},
      {0.0f, 0.0f, 0.5f, 1.0f},
      {0.0f, 0.0f, 1.0f, 1.0f},
  };
  GPUFrameBuffer *framebuffers[6] = {nullptr};

  for (int i : IndexRange(6)) {
    GPU_framebuffer_ensure_config(&framebuffers[i],
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE_CUBEFACE(tex, i),
                                  });
    GPU_framebuffer_bind(framebuffers[i]);
    GPU_framebuffer_clear_color(framebuffers[i], clear_colors[i]);
  };

  float4 *data = (float4 *)GPU_texture_read(tex, GPU_DATA_FLOAT, 0);
  for (int side : IndexRange(6)) {
    for (int pixel_index : IndexRange(SIZE * SIZE)) {
      int index = pixel_index + (SIZE * SIZE) * side;
      EXPECT_EQ(clear_colors[side], data[index]);
    }
  }
  MEM_freeN(data);

  GPU_texture_free(tex);

  for (int i : IndexRange(6)) {
    GPU_FRAMEBUFFER_FREE_SAFE(framebuffers[i]);
  }

  GPU_render_end();
}
GPU_TEST(framebuffer_cube)

/* Effectively tests the same way EEVEE-Next shadows are rendered. */
static void test_framebuffer_multi_viewport()
{
  using namespace gpu::shader;

  GPU_render_begin();

  const int2 size(4, 4);
  const int layers = 256;
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture = GPU_texture_create_2d_array(
      __func__, UNPACK2(size), layers, 1, GPU_RG32I, usage, nullptr);

  GPUFrameBuffer *framebuffer = GPU_framebuffer_create(__func__);
  GPU_framebuffer_ensure_config(&framebuffer,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(texture)});
  GPU_framebuffer_bind(framebuffer);

  int viewport_rects[16][4];
  for (int i = 0; i < 16; i++) {
    viewport_rects[i][0] = i % 4;
    viewport_rects[i][1] = i / 4;
    viewport_rects[i][2] = 1;
    viewport_rects[i][3] = 1;
  }
  GPU_framebuffer_multi_viewports_set(framebuffer, viewport_rects);

  const float4 clear_color(0.0f);
  GPU_framebuffer_clear_color(framebuffer, clear_color);

  ShaderCreateInfo create_info("");
  create_info.vertex_source("gpu_framebuffer_layer_viewport_test.glsl");
  create_info.fragment_source("gpu_framebuffer_layer_viewport_test.glsl");
  create_info.builtins(BuiltinBits::VIEWPORT_INDEX | BuiltinBits::LAYER);
  create_info.fragment_out(0, Type::IVEC2, "out_value");

  GPUShader *shader = GPU_shader_create_from_info(
      reinterpret_cast<GPUShaderCreateInfo *>(&create_info));

  /* TODO(fclem): remove this boilerplate. */
  GPUVertFormat format{};
  GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U32, 1, GPU_FETCH_INT);
  VertBuf *verts = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(verts, 3);
  Batch *batch = GPU_batch_create_ex(GPU_PRIM_TRIS, verts, nullptr, GPU_BATCH_OWNS_VBO);

  GPU_batch_set_shader(batch, shader);

  int tri_count = size.x * size.y * layers;
  GPU_batch_draw_advanced(batch, 0, tri_count * 3, 0, 1);

  GPU_batch_discard(batch);

  GPU_finish();

  int2 *read_data = static_cast<int2 *>(GPU_texture_read(texture, GPU_DATA_INT, 0));
  for (auto layer : IndexRange(layers)) {
    for (auto viewport : IndexRange(16)) {
      int2 expected_color(layer, viewport);
      int2 pixel_color = read_data[viewport + layer * 16];
      EXPECT_EQ(pixel_color, expected_color);
    }
  }
  MEM_freeN(read_data);

  GPU_framebuffer_free(framebuffer);
  GPU_texture_free(texture);
  GPU_shader_free(shader);

  GPU_render_end();
}
GPU_TEST(framebuffer_multi_viewport)

/**
 * Test sub-pass inputs on Vulkan and raster order groups on Metal and its emulation on other
 * backend.
 */
static void test_framebuffer_subpass_input()
{
  using namespace gpu::shader;

  GPU_render_begin();

  const int2 size(1, 1);
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture_a = GPU_texture_create_2d(
      __func__, UNPACK2(size), 1, GPU_R32I, usage, nullptr);
  GPUTexture *texture_b = GPU_texture_create_2d(
      __func__, UNPACK2(size), 1, GPU_R32I, usage, nullptr);

  GPUFrameBuffer *framebuffer = GPU_framebuffer_create(__func__);
  GPU_framebuffer_ensure_config(
      &framebuffer,
      {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(texture_a), GPU_ATTACHMENT_TEXTURE(texture_b)});
  GPU_framebuffer_bind(framebuffer);

  const float4 clear_color(0.0f);
  GPU_framebuffer_clear_color(framebuffer, clear_color);

  ShaderCreateInfo create_info_write("");
  create_info_write.define("WRITE");
  create_info_write.vertex_source("gpu_framebuffer_subpass_input_test.glsl");
  create_info_write.fragment_source("gpu_framebuffer_subpass_input_test.glsl");
  create_info_write.fragment_out(0, Type::INT, "out_value", DualBlend::NONE, 0);

  GPUShader *shader_write = GPU_shader_create_from_info(
      reinterpret_cast<GPUShaderCreateInfo *>(&create_info_write));

  ShaderCreateInfo create_info_read("");
  create_info_read.define("READ");
  create_info_read.vertex_source("gpu_framebuffer_subpass_input_test.glsl");
  create_info_read.fragment_source("gpu_framebuffer_subpass_input_test.glsl");
  create_info_read.subpass_in(0, Type::INT, "in_value", 0);
  create_info_read.fragment_out(1, Type::INT, "out_value");

  GPUShader *shader_read = GPU_shader_create_from_info(
      reinterpret_cast<GPUShaderCreateInfo *>(&create_info_read));

  /* TODO(fclem): remove this boilerplate. */
  GPUVertFormat format{};
  GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U32, 1, GPU_FETCH_INT);
  VertBuf *verts = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(verts, 3);
  Batch *batch = GPU_batch_create_ex(GPU_PRIM_TRIS, verts, nullptr, GPU_BATCH_OWNS_VBO);

  /* Metal Raster Order Group does not need that. */
  GPU_framebuffer_subpass_transition(
      framebuffer, {GPU_ATTACHEMENT_IGNORE, GPU_ATTACHEMENT_WRITE, GPU_ATTACHEMENT_IGNORE});

  GPU_batch_set_shader(batch, shader_write);
  GPU_batch_draw(batch);

  /* Metal Raster Order Group does not need that. */
  GPU_framebuffer_subpass_transition(
      framebuffer, {GPU_ATTACHEMENT_IGNORE, GPU_ATTACHEMENT_READ, GPU_ATTACHEMENT_WRITE});

  GPU_batch_set_shader(batch, shader_read);
  GPU_batch_draw(batch);

  GPU_batch_discard(batch);

  GPU_finish();

  int *read_data_a = static_cast<int *>(GPU_texture_read(texture_a, GPU_DATA_INT, 0));
  EXPECT_EQ(*read_data_a, 0xDEADBEEF);
  MEM_freeN(read_data_a);

  int *read_data_b = static_cast<int *>(GPU_texture_read(texture_b, GPU_DATA_INT, 0));
  EXPECT_EQ(*read_data_b, 0xDEADC0DE);
  MEM_freeN(read_data_b);

  GPU_framebuffer_free(framebuffer);
  GPU_texture_free(texture_a);
  GPU_texture_free(texture_b);
  GPU_shader_free(shader_write);
  GPU_shader_free(shader_read);

  GPU_render_end();
}
GPU_TEST(framebuffer_subpass_input)

}  // namespace blender::gpu::tests
