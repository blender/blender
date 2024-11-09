/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "GPU_context.hh"
#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_shader.hh"
#include "GPU_vertex_buffer.hh"
#include "GPU_vertex_format.hh"

#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"

#include "gpu_testing.hh"

namespace blender::gpu::tests {

static constexpr int Size = 4;

template<GPUVertCompType comp_type, GPUVertFetchMode fetch_mode, typename ColorType>
static void vertex_buffer_fetch_mode(ColorType color)
{
  GPUOffScreen *offscreen = GPU_offscreen_create(Size,
                                                 Size,
                                                 false,
                                                 GPU_RGBA16F,
                                                 GPU_TEXTURE_USAGE_ATTACHMENT |
                                                     GPU_TEXTURE_USAGE_HOST_READ,
                                                 nullptr);
  BLI_assert(offscreen != nullptr);
  GPU_offscreen_bind(offscreen, false);
  GPUTexture *color_texture = GPU_offscreen_color_texture(offscreen);
  GPU_texture_clear(color_texture, GPU_DATA_FLOAT, float4(1.0f, 2.0f, 3.0f, 0.0f));

  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  GPU_vertformat_attr_add(&format, "color", comp_type, 4, fetch_mode);

  VertBuf *vbo = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*vbo, 4);

  struct Vert {
    float2 pos;
    ColorType color;
  };
  Vert data[4] = {
      {float2(-1.0, -1.0), color},
      {float2(3.0, -1.0), color},
      {float2(-1.0, 3.0), color},
  };
  for (int i : IndexRange(4)) {
    GPU_vertbuf_vert_set(vbo, i, &data[i]);
  }

  Batch *batch = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_FLAT_COLOR);
  GPU_batch_draw(batch);

  GPU_offscreen_unbind(offscreen, false);
  GPU_flush();

  /* Read back data and perform some basic tests. */
  Vector<float4> read_data(Size * Size);
  GPU_offscreen_read_color(offscreen, GPU_DATA_FLOAT, read_data.data());
  for (const float4 &read_color : read_data) {
    EXPECT_EQ(read_color, float4(color));
  }

  GPU_batch_discard(batch);
  GPU_offscreen_free(offscreen);
}

#ifndef __APPLE__ /* Not supported on metal backend. Also could be phased out eventually. */
static void test_vertex_buffer_fetch_mode__GPU_COMP_I8__GPU_FETCH_INT_TO_FLOAT()
{
  vertex_buffer_fetch_mode<GPU_COMP_I8, GPU_FETCH_INT_TO_FLOAT, char4>(char4(4, 5, 6, 1));
}
GPU_TEST(vertex_buffer_fetch_mode__GPU_COMP_I8__GPU_FETCH_INT_TO_FLOAT);

static void test_vertex_buffer_fetch_mode__GPU_COMP_U8__GPU_FETCH_INT_TO_FLOAT()
{
  vertex_buffer_fetch_mode<GPU_COMP_U8, GPU_FETCH_INT_TO_FLOAT, uchar4>(uchar4(4, 5, 6, 1));
}
GPU_TEST(vertex_buffer_fetch_mode__GPU_COMP_U8__GPU_FETCH_INT_TO_FLOAT);

static void test_vertex_buffer_fetch_mode__GPU_COMP_I16__GPU_FETCH_INT_TO_FLOAT()
{
  vertex_buffer_fetch_mode<GPU_COMP_I16, GPU_FETCH_INT_TO_FLOAT, short4>(short4(4, 5, 6, 1));
}
GPU_TEST(vertex_buffer_fetch_mode__GPU_COMP_I16__GPU_FETCH_INT_TO_FLOAT);

static void test_vertex_buffer_fetch_mode__GPU_COMP_U16__GPU_FETCH_INT_TO_FLOAT()
{
  vertex_buffer_fetch_mode<GPU_COMP_U16, GPU_FETCH_INT_TO_FLOAT, ushort4>(ushort4(4, 5, 6, 1));
}
GPU_TEST(vertex_buffer_fetch_mode__GPU_COMP_U16__GPU_FETCH_INT_TO_FLOAT);
#endif

static void test_vertex_buffer_fetch_mode__GPU_COMP_I32__GPU_FETCH_INT_TO_FLOAT()
{
  vertex_buffer_fetch_mode<GPU_COMP_I32, GPU_FETCH_INT_TO_FLOAT, int4>(int4(4, 5, 6, 1));
}
GPU_TEST(vertex_buffer_fetch_mode__GPU_COMP_I32__GPU_FETCH_INT_TO_FLOAT);

static void test_vertex_buffer_fetch_mode__GPU_COMP_U32__GPU_FETCH_INT_TO_FLOAT()
{
  vertex_buffer_fetch_mode<GPU_COMP_U32, GPU_FETCH_INT_TO_FLOAT, uint4>(uint4(4, 5, 6, 1));
}
GPU_TEST(vertex_buffer_fetch_mode__GPU_COMP_U32__GPU_FETCH_INT_TO_FLOAT);

static void test_vertex_buffer_fetch_mode__GPU_COMP_F32__GPU_FETCH_FLOAT()
{
  vertex_buffer_fetch_mode<GPU_COMP_F32, GPU_FETCH_FLOAT, float4>(float4(4, 5, 6, 1));
}
GPU_TEST(vertex_buffer_fetch_mode__GPU_COMP_F32__GPU_FETCH_FLOAT);

}  // namespace blender::gpu::tests
