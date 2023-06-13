/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_context.h"
#include "GPU_framebuffer.h"
#include "gpu_testing.hh"

#include "BLI_math_vector.hh"

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

/* Color each side of a cubemap with a different color. */
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

}  // namespace blender::gpu::tests
