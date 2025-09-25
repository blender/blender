/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "gpu_testing.hh"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector_types.hh"

#include "GPU_context.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"
#include "GPU_texture_pool.hh"

#include "gpu_texture_private.hh"

/* Not all texture types are supported by all platforms. This define safe guards them until we have
 * a working workaround or decided to remove support for those texture types. */
#define RUN_UNSUPPORTED false

/* Skip tests that haven't been developed yet due to non standard data types or it needs an
 * frame-buffer to create the texture. */
#define RUN_SRGB_UNIMPLEMENTED false
#define RUN_NON_STANDARD_UNIMPLEMENTED false
#define RUN_COMPONENT_UNIMPLEMENTED false

namespace blender::gpu::tests {

static void test_texture_read()
{
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *rgba32u = GPU_texture_create_2d(
      "rgba32u", 1, 1, 1, TextureFormat::UINT_32_32_32_32, usage, nullptr);
  blender::gpu::Texture *rgba16u = GPU_texture_create_2d(
      "rgba16u", 1, 1, 1, TextureFormat::UINT_16_16_16_16, usage, nullptr);
  blender::gpu::Texture *rgba32f = GPU_texture_create_2d(
      "rgba32f", 1, 1, 1, TextureFormat::SFLOAT_32_32_32_32, usage, nullptr);

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

static void test_texture_1d()
{
  if (GPU_backend_get_type() == GPU_BACKEND_OPENGL) {
    GTEST_SKIP() << "OpenGL texture clearing doesn't support 1d textures.";
  }
  const int SIZE = 32;
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ |
                           GPU_TEXTURE_USAGE_SHADER_WRITE;
  blender::gpu::Texture *tex = GPU_texture_create_1d(
      "tex", SIZE, 1, TextureFormat::SFLOAT_32_32_32_32, usage, nullptr);
  float4 clear_color(0.9f, 0.7f, 0.2f, 1.0f);
  GPU_texture_clear(tex, GPU_DATA_FLOAT, clear_color);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  float4 *data = (float4 *)GPU_texture_read(tex, GPU_DATA_FLOAT, 0);
  for (int index : IndexRange(SIZE)) {
    EXPECT_EQ(clear_color, data[index]);
  }
  MEM_freeN(data);

  GPU_texture_free(tex);

  GPU_render_end();
}
GPU_TEST(texture_1d)

static void test_texture_1d_array()
{
  if (GPU_backend_get_type() == GPU_BACKEND_OPENGL) {
    GTEST_SKIP() << "Read back of 1d texture arrays not supported by OpenGL";
  }
  const int LAYERS = 8;
  const int SIZE = 32;
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ |
                           GPU_TEXTURE_USAGE_SHADER_WRITE;
  blender::gpu::Texture *tex = GPU_texture_create_1d_array(
      "tex", SIZE, LAYERS, 1, TextureFormat::SFLOAT_32_32_32_32, usage, nullptr);
  float4 clear_color(1.0f, 0.5f, 0.2f, 1.0f);
  GPU_texture_clear(tex, GPU_DATA_FLOAT, clear_color);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  float4 *data = (float4 *)GPU_texture_read(tex, GPU_DATA_FLOAT, 0);
  for (int index : IndexRange(SIZE * LAYERS)) {
    EXPECT_EQ(clear_color, data[index]);
  }
  MEM_freeN(data);

  GPU_texture_free(tex);

  GPU_render_end();
}
GPU_TEST(texture_1d_array)

static void test_texture_1d_array_upload()
{
  if (GPU_backend_get_type() == GPU_BACKEND_OPENGL) {
    GTEST_SKIP() << "Read back of 1d texture arrays not supported by OpenGL";
  }
  const int LAYERS = 8;
  const int SIZE = 32;
  GPU_render_begin();

  int total_size = LAYERS * SIZE * 4;
  float *data_in = MEM_calloc_arrayN<float>(total_size, __func__);

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *tex = GPU_texture_create_1d_array(
      "tex", SIZE, LAYERS, 1, TextureFormat::SFLOAT_32_32_32_32, usage, data_in);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  float *data_out = static_cast<float *>(GPU_texture_read(tex, GPU_DATA_FLOAT, 0));
  GPU_texture_free(tex);

  EXPECT_EQ(memcmp(data_in, data_out, sizeof(float) * total_size), 0);
  MEM_freeN(data_in);
  MEM_freeN(data_out);

  GPU_render_end();
}
GPU_TEST(texture_1d_array_upload)

static void test_texture_2d_array()
{
  const int LAYERS = 8;
  const int SIZE = 32;
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *tex = GPU_texture_create_2d_array(
      "tex", SIZE, SIZE, LAYERS, 1, TextureFormat::SFLOAT_32_32_32_32, usage, nullptr);
  float4 clear_color(1.0f, 0.5f, 0.2f, 1.0f);
  GPU_texture_clear(tex, GPU_DATA_FLOAT, clear_color);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  float4 *data = (float4 *)GPU_texture_read(tex, GPU_DATA_FLOAT, 0);
  for (int index : IndexRange(SIZE * SIZE * LAYERS)) {
    EXPECT_EQ(clear_color, data[index]);
  }
  MEM_freeN(data);

  GPU_texture_free(tex);

  GPU_render_end();
}
GPU_TEST(texture_2d_array)

static void test_texture_2d_array_upload()
{
  const int LAYERS = 8;
  const int SIZE = 32;
  GPU_render_begin();

  int total_size = LAYERS * SIZE * SIZE * 4;
  float *data_in = MEM_calloc_arrayN<float>(total_size, __func__);

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *tex = GPU_texture_create_2d_array(
      "tex", SIZE, SIZE, LAYERS, 1, TextureFormat::SFLOAT_32_32_32_32, usage, data_in);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  float *data_out = static_cast<float *>(GPU_texture_read(tex, GPU_DATA_FLOAT, 0));
  GPU_texture_free(tex);

  EXPECT_EQ(memcmp(data_in, data_out, sizeof(float) * total_size), 0);
  MEM_freeN(data_in);
  MEM_freeN(data_out);

  GPU_render_end();
}
GPU_TEST(texture_2d_array_upload)

static void test_texture_cube()
{
  const int SIZE = 32;
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *tex = GPU_texture_create_cube(
      "tex", SIZE, 1, TextureFormat::SFLOAT_32_32_32_32, usage, nullptr);
  float4 clear_color(1.0f, 0.5f, 0.2f, 1.0f);
  GPU_texture_clear(tex, GPU_DATA_FLOAT, clear_color);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  float4 *data = (float4 *)GPU_texture_read(tex, GPU_DATA_FLOAT, 0);
  for (int index : IndexRange(SIZE * SIZE * 6)) {
    EXPECT_EQ(clear_color, data[index]);
  }
  MEM_freeN(data);

  GPU_texture_free(tex);

  GPU_render_end();
}
GPU_TEST(texture_cube)

static void test_texture_cube_array()
{
  const int LAYERS = 2;
  const int SIZE = 32;
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *tex = GPU_texture_create_cube_array(
      "tex", SIZE, LAYERS, 1, TextureFormat::SFLOAT_32_32_32_32, usage, nullptr);
  float4 clear_color(1.0f, 0.5f, 0.2f, 1.0f);
  GPU_texture_clear(tex, GPU_DATA_FLOAT, clear_color);

  float4 *data = (float4 *)GPU_texture_read(tex, GPU_DATA_FLOAT, 0);
  for (int index : IndexRange(SIZE * SIZE * 6 * LAYERS)) {
    EXPECT_EQ(clear_color, data[index]);
  }
  MEM_freeN(data);

  GPU_texture_free(tex);

  GPU_render_end();
}
GPU_TEST(texture_cube_array)

static void test_texture_3d()
{
  const int SIZE = 32;
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *tex = GPU_texture_create_3d(
      "tex", SIZE, SIZE, SIZE, 1, TextureFormat::SFLOAT_32_32_32_32, usage, nullptr);
  float4 clear_color(1.0f, 0.5f, 0.2f, 1.0f);
  GPU_texture_clear(tex, GPU_DATA_FLOAT, clear_color);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  float4 *data = (float4 *)GPU_texture_read(tex, GPU_DATA_FLOAT, 0);
  for (int index : IndexRange(SIZE * SIZE * SIZE)) {
    EXPECT_EQ(clear_color, data[index]);
  }
  MEM_freeN(data);

  GPU_texture_free(tex);

  GPU_render_end();
}
GPU_TEST(texture_3d)

static void test_texture_copy()
{
  const int SIZE = 128;
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *src_tx = GPU_texture_create_2d(
      "src", SIZE, SIZE, 1, TextureFormat::SFLOAT_32_32_32_32, usage, nullptr);
  blender::gpu::Texture *dst_tx = GPU_texture_create_2d(
      "dst", SIZE, SIZE, 1, TextureFormat::SFLOAT_32_32_32_32, usage, nullptr);

  const float4 color(0.0, 1.0f, 2.0f, 123.0f);
  const float4 clear_color(0.0f);
  GPU_texture_clear(src_tx, GPU_DATA_FLOAT, color);
  GPU_texture_clear(dst_tx, GPU_DATA_FLOAT, clear_color);

  GPU_texture_copy(dst_tx, src_tx);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  float4 *data = (float4 *)GPU_texture_read(dst_tx, GPU_DATA_FLOAT, 0);
  for (int index : IndexRange(SIZE * SIZE)) {
    EXPECT_EQ(color, data[index]);
  }
  MEM_freeN(data);

  GPU_texture_free(src_tx);
  GPU_texture_free(dst_tx);

  GPU_render_end();
}
GPU_TEST(texture_copy)

template<typename DataType> static DataType *generate_test_data(size_t data_len)
{
  DataType *data = MEM_malloc_arrayN<DataType>(data_len, __func__);
  for (int i : IndexRange(data_len)) {
    if (std::is_same<DataType, float>()) {
      data[i] = (DataType)(i % 8) / 8.0f;
    }
    else {
      data[i] = (DataType)(i % 8);
    }
  }
  return data;
}

template<TextureFormat DeviceFormat, eGPUDataFormat HostFormat, typename DataType, int Size = 16>
static void texture_create_upload_read()
{
  static_assert(!std::is_same<DataType, float>());
  static_assert(validate_data_format(DeviceFormat, HostFormat));
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *texture = GPU_texture_create_2d(
      "texture", Size, Size, 1, DeviceFormat, usage, nullptr);
  if (texture == nullptr) {
    GTEST_SKIP() << "Platform doesn't support texture format [" << STRINGIFY(DeviceFormat) << "]";
  }

  size_t data_len = Size * Size *
                    (HostFormat == GPU_DATA_10_11_11_REV ? to_bytesize(HostFormat) :
                                                           to_component_len(DeviceFormat));

  DataType *data = static_cast<DataType *>(generate_test_data<DataType>(data_len));
  GPU_texture_update(texture, HostFormat, data);

  DataType *read_data = static_cast<DataType *>(GPU_texture_read(texture, HostFormat, 0));
  bool failed = false;
  for (int i : IndexRange(data_len)) {
    EXPECT_EQ(data[i], read_data[i]);
    bool ok = (read_data[i] - data[i]) == 0;
    failed |= !ok;
  }
  EXPECT_FALSE(failed);

  MEM_freeN(read_data);
  MEM_freeN(data);

  GPU_texture_free(texture);
}

template<TextureFormat DeviceFormat, eGPUDataFormat HostFormat, int Size = 16>
static void texture_create_upload_read_with_bias(float max_allowed_bias)
{
  static_assert(validate_data_format(DeviceFormat, HostFormat));
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *texture = GPU_texture_create_2d(
      "texture", Size, Size, 1, DeviceFormat, usage, nullptr);
  if (texture == nullptr) {
    GTEST_SKIP() << "Platform doesn't support texture format [" << STRINGIFY(DeviceFormat) << "]";
  }

  size_t data_len = Size * Size * to_component_len(DeviceFormat);
  float *data = static_cast<float *>(generate_test_data<float>(data_len));
  GPU_texture_update(texture, HostFormat, data);

  float *read_data = static_cast<float *>(GPU_texture_read(texture, HostFormat, 0));
  float max_used_bias = 0.0f;
  for (int i : IndexRange(data_len)) {
    float bias = abs(read_data[i] - data[i]);
    max_used_bias = max_ff(max_used_bias, bias);
  }
  EXPECT_LE(max_used_bias, max_allowed_bias);

  MEM_freeN(read_data);
  MEM_freeN(data);

  GPU_texture_free(texture);
}

/* Derivative of texture_create_upload_read_pixels that doesn't test each component, but a pixel at
 * a time. This is needed to check the R11G11B10 and similar types. */
template<TextureFormat DeviceFormat, eGPUDataFormat HostFormat, int Size = 16>
static void texture_create_upload_read_pixel()
{
  using DataType = uint32_t;
  static_assert(validate_data_format(DeviceFormat, HostFormat));
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  blender::gpu::Texture *texture = GPU_texture_create_2d(
      "texture", Size, Size, 1, DeviceFormat, usage, nullptr);
  ASSERT_NE(texture, nullptr);

  size_t data_len = Size * Size;
  DataType *data = static_cast<DataType *>(generate_test_data<DataType>(data_len));
  GPU_texture_update(texture, HostFormat, data);

  DataType *read_data = static_cast<DataType *>(GPU_texture_read(texture, HostFormat, 0));
  bool failed = false;
  for (int i : IndexRange(data_len)) {
    bool ok = (read_data[i] - data[i]) == 0;
    failed |= !ok;
  }
  EXPECT_FALSE(failed);

  MEM_freeN(read_data);
  MEM_freeN(data);

  GPU_texture_free(texture);
}

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_FLOAT
 * \{ */
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_8_8_8_8, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16F()
{
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_16_16_16_16, GPU_DATA_FLOAT>(0.9f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_16_16_16_16, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA32F()
{
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_32_32_32_32, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA32F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG8()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_8_8, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16F()
{
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_16_16, GPU_DATA_FLOAT>(0.9f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_16_16, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG32F()
{
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_32_32, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG32F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R8()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_8, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R16F()
{
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_16, GPU_DATA_FLOAT>(0.9f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R16F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R16()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_16, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R16);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R32F()
{
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_32, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R32F);

#if RUN_NON_STANDARD_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB10_A2()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_10_10_10_2, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB10_A2);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB10_A2UI()
{
  texture_create_upload_read_with_bias<TextureFormat::UINT_10_10_10_2, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB10_A2UI);
#endif

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R11F_G11F_B10F()
{
  texture_create_upload_read_with_bias<TextureFormat::UFLOAT_11_11_10, GPU_DATA_FLOAT>(0.0009f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R11F_G11F_B10F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8()
{
  texture_create_upload_read_with_bias<TextureFormat::SRGBA_8_8_8_8, GPU_DATA_FLOAT>(0.0035f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_SNORM()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_8_8_8_8, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16_SNORM()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_16_16_16_16, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16_SNORM);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB8()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_8_8_8, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB8_SNORM()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_8_8_8, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB8_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16F()
{
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_16_16_16, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_16_16_16, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16);
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16_SNORM()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_16_16_16, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB32F()
{
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_32_32_32, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB32F);
#endif

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG8_SNORM()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_8_8, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG8_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16_SNORM()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_16_16, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R8_SNORM()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_8, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R8_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R16_SNORM()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_16, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R16_SNORM);

#if RUN_NON_STANDARD_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT1()
{
  texture_create_upload_read_with_bias<TextureFormat::SRGB_DXT1, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT1);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT3()
{
  texture_create_upload_read_with_bias<TextureFormat::SRGB_DXT3, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT3);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT5()
{
  texture_create_upload_read_with_bias<TextureFormat::SRGB_DXT5, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT5);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT1()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_DXT1, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT1);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT3()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_DXT3, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT3);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT5()
{
  texture_create_upload_read_with_bias<TextureFormat::SNORM_DXT5, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT5);
#endif

#if RUN_SRGB_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8()
{
  texture_create_upload_read_with_bias<TextureFormat::SRGBA_8_8_8, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8);
#endif

#if RUN_NON_STANDARD_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB9_E5()
{
  texture_create_upload_read_with_bias<TextureFormat::UFLOAT_9_9_9_EXP_5, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB9_E5);
#endif

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT32F()
{
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_32_DEPTH, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT32F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH32F_STENCIL8()
{
  if (GPU_backend_get_type() == GPU_BACKEND_OPENGL) {
    GTEST_SKIP() << "Float based texture readback not supported on OpenGL";
  }
  texture_create_upload_read_with_bias<TextureFormat::SFLOAT_32_DEPTH_UINT_8, GPU_DATA_FLOAT>(
      0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH32F_STENCIL8);

#if RUN_COMPONENT_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT16()
{
  texture_create_upload_read_with_bias<TextureFormat::UNORM_16_DEPTH, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT16);
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_HALF_FLOAT
 * \{ */

static void test_texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RGBA16F()
{
  texture_create_upload_read<TextureFormat::SFLOAT_16_16_16_16, GPU_DATA_HALF_FLOAT, uint16_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RGBA16F);

static void test_texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RG16F()
{
  texture_create_upload_read<TextureFormat::SFLOAT_16_16, GPU_DATA_HALF_FLOAT, uint16_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RG16F);

static void test_texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_R16F()
{
  texture_create_upload_read<TextureFormat::SFLOAT_16, GPU_DATA_HALF_FLOAT, uint16_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_R16F);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RGB16F()
{
  texture_create_upload_read<TextureFormat::SFLOAT_16_16_16, GPU_DATA_HALF_FLOAT, uint16_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RGB16F);
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_INT
 * \{ */

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGBA8I()
{
  texture_create_upload_read<TextureFormat::SINT_8_8_8_8, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGBA8I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGBA16I()
{
  texture_create_upload_read<TextureFormat::SINT_16_16_16_16, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGBA16I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGBA32I()
{
  texture_create_upload_read<TextureFormat::SINT_32_32_32_32, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGBA32I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RG8I()
{
  texture_create_upload_read<TextureFormat::SINT_8_8, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RG8I);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_INT__GPU_RG16I()
{
  texture_create_upload_read<TextureFormat::SINT_16_16, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RG16I);
#endif

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RG32I()
{
  texture_create_upload_read<TextureFormat::SINT_32_32, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RG32I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_R8I()
{
  texture_create_upload_read<TextureFormat::SINT_8, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_R8I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_R16I()
{
  texture_create_upload_read<TextureFormat::SINT_16, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_R16I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_R32I()
{
  texture_create_upload_read<TextureFormat::SINT_32, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_R32I);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGB8I()
{
  texture_create_upload_read<TextureFormat::SINT_8_8_8, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGB8I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGB16I()
{
  texture_create_upload_read<TextureFormat::SINT_16_16_16, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGB16I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGB32I()
{
  texture_create_upload_read<TextureFormat::SINT_32_32_32, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGB32I);
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_UINT
 * \{ */

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGBA8UI()
{
  texture_create_upload_read<TextureFormat::UINT_8_8_8_8, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGBA8UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGBA16UI()
{
  texture_create_upload_read<TextureFormat::UINT_16_16_16_16, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGBA16UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGBA32UI()
{
  texture_create_upload_read<TextureFormat::UINT_32_32_32_32, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGBA32UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RG8UI()
{
  texture_create_upload_read<TextureFormat::UINT_8_8, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RG8UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RG16UI()
{
  texture_create_upload_read<TextureFormat::UINT_16_16, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RG16UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RG32UI()
{
  texture_create_upload_read<TextureFormat::UINT_32_32, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RG32UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_R8UI()
{
  texture_create_upload_read<TextureFormat::UINT_8, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_R8UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_R16UI()
{
  texture_create_upload_read<TextureFormat::UINT_16, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_R16UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_R32UI()
{
  texture_create_upload_read<TextureFormat::UINT_32, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_R32UI);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH32F_STENCIL8()
{
  texture_create_upload_read<TextureFormat::SFLOAT_32_DEPTH_UINT_8, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH32F_STENCIL8);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGB8UI()
{
  texture_create_upload_read<TextureFormat::UINT_8_8_8, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGB8UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGB16UI()
{
  texture_create_upload_read<TextureFormat::UINT_16_16_16, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGB16UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGB32UI()
{
  texture_create_upload_read<TextureFormat::UINT_32_32_32, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGB32UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT32F()
{
  texture_create_upload_read<TextureFormat::SFLOAT_32_DEPTH, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT32F);
#endif

#if RUN_COMPONENT_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT16()
{
  texture_create_upload_read<TextureFormat::UNORM_16_DEPTH, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT16);
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_UBYTE
 * \{ */

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RGBA8UI()
{
  texture_create_upload_read<TextureFormat::UINT_8_8_8_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RGBA8UI);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RGBA8()
{
  texture_create_upload_read<TextureFormat::UNORM_8_8_8_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RGBA8);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RG8UI()
{
  texture_create_upload_read<TextureFormat::UINT_8_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RG8UI);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RG8()
{
  texture_create_upload_read<TextureFormat::UNORM_8_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RG8);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_R8UI()
{
  texture_create_upload_read<TextureFormat::UINT_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_R8UI);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_R8()
{
  texture_create_upload_read<TextureFormat::UNORM_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_R8);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_SRGB8_A8()
{
  texture_create_upload_read<TextureFormat::SRGBA_8_8_8_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_SRGB8_A8);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RGB8I()
{
  texture_create_upload_read<TextureFormat::SINT_8_8_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RGB8I);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RGB8()
{
  texture_create_upload_read<TextureFormat::UNORM_8_8_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RGB8);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_SRGB8()
{
  texture_create_upload_read<TextureFormat::SRGBA_8_8_8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_SRGB8);
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_UINT_24_8_DEPRECATED
 * \{ */

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_UINT_24_8__GPU_DEPTH32F_STENCIL8()
{
  texture_create_upload_read<TextureFormat::SFLOAT_32_DEPTH_UINT_8,
                             GPU_DATA_UINT_24_8_DEPRECATED,
                             void>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT_24_8__GPU_DEPTH32F_STENCIL8);
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_10_11_11_REV
 * \{ */

static void test_texture_roundtrip__GPU_DATA_10_11_11_REV__GPU_R11F_G11F_B10F()
{
  texture_create_upload_read<TextureFormat::UFLOAT_11_11_10, GPU_DATA_10_11_11_REV, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_10_11_11_REV__GPU_R11F_G11F_B10F);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_2_10_10_10_REV
 * \{ */

static void test_texture_roundtrip__GPU_DATA_2_10_10_10_REV__GPU_RGB10_A2()
{
  texture_create_upload_read_pixel<TextureFormat::UNORM_10_10_10_2, GPU_DATA_2_10_10_10_REV>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_2_10_10_10_REV__GPU_RGB10_A2);

static void test_texture_roundtrip__GPU_DATA_2_10_10_10_REV__GPU_RGB10_A2UI()
{
  if (GPU_backend_get_type() == GPU_BACKEND_OPENGL) {
    GTEST_SKIP() << "Texture readback not supported on OpenGL";
  }
  texture_create_upload_read_pixel<TextureFormat::UINT_10_10_10_2, GPU_DATA_2_10_10_10_REV>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_2_10_10_10_REV__GPU_RGB10_A2UI);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unpack row length
 * \{ */

static void test_texture_update_sub_no_unpack_row_length()
{
  const int2 size(1024);
  const int2 sub_size(256);
  const int2 sub_offset(256);

  blender::gpu::Texture *texture = GPU_texture_create_2d(__func__,
                                                         UNPACK2(size),
                                                         2,
                                                         TextureFormat::SFLOAT_32_32_32_32,
                                                         GPU_TEXTURE_USAGE_GENERAL,
                                                         nullptr);
  const float4 clear_color(0.0f, 0.0f, 0.0f, 0.0f);
  GPU_texture_clear(texture, GPU_DATA_FLOAT, &clear_color);

  const float4 texture_color(0.0f, 1.0f, 0.0f, 1.0f);
  float4 *texture_data = MEM_malloc_arrayN<float4>(sub_size.x * sub_size.y, __func__);
  for (int i = 0; i < sub_size.x * sub_size.y; i++) {
    texture_data[i] = texture_color;
  }

  GPU_texture_update_sub(
      texture, GPU_DATA_FLOAT, texture_data, UNPACK2(sub_offset), 0, UNPACK2(sub_size), 1);
  float4 *texture_data_read = static_cast<float4 *>(GPU_texture_read(texture, GPU_DATA_FLOAT, 0));

  for (int x = 0; x < size.x; x++) {
    for (int y = 0; y < sub_offset.y; y++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], clear_color);
    }
  }
  for (int y = sub_offset.y; y < sub_offset.y + sub_size.y; y++) {
    for (int x = 0; x < sub_offset.x; x++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], clear_color);
    }
    for (int x = sub_offset.x; x < sub_offset.x + sub_size.x; x++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], texture_color);
    }
    for (int x = sub_offset.x + sub_size.x; x < size.x; x++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], clear_color);
    }
  }
  for (int x = 0; x < size.x; x++) {
    for (int y = sub_offset.y + sub_size.y; y < size.y; y++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], clear_color);
    }
  }

  MEM_freeN(texture_data);
  MEM_freeN(texture_data_read);
  GPU_texture_free(texture);
}
GPU_TEST(texture_update_sub_no_unpack_row_length);

static void test_texture_update_sub_unpack_row_length()
{
  const int2 size(1024);
  const int2 sub_size(256);
  const int2 sub_offset(256);

  blender::gpu::Texture *texture = GPU_texture_create_2d(__func__,
                                                         UNPACK2(size),
                                                         2,
                                                         TextureFormat::SFLOAT_32_32_32_32,
                                                         GPU_TEXTURE_USAGE_GENERAL,
                                                         nullptr);
  const float4 clear_color(0.0f, 0.0f, 0.0f, 0.0f);
  GPU_texture_clear(texture, GPU_DATA_FLOAT, &clear_color);

  const float4 texture_color(0.0f, 1.0f, 0.0f, 1.0f);
  const float4 texture_color_off(1.0f, 0.0f, 0.0f, 1.0f);
  float4 *texture_data = MEM_malloc_arrayN<float4>(size.x * size.y, __func__);
  for (int x = 0; x < size.x; x++) {
    for (int y = 0; y < size.y; y++) {
      int index = x + y * size.x;
      texture_data[index] = ((x >= sub_offset.x && x < sub_offset.x + sub_size.x) &&
                             (y >= sub_offset.y && y < sub_offset.y + sub_size.y)) ?
                                texture_color :
                                texture_color_off;
    }
  }

  GPU_unpack_row_length_set(size.x);
  float4 *texture_data_offset = &texture_data[sub_offset.x + sub_offset.y * size.x];
  GPU_texture_update_sub(
      texture, GPU_DATA_FLOAT, texture_data_offset, UNPACK2(sub_offset), 0, UNPACK2(sub_size), 1);
  float4 *texture_data_read = static_cast<float4 *>(GPU_texture_read(texture, GPU_DATA_FLOAT, 0));
  GPU_unpack_row_length_set(0);

  for (int x = 0; x < size.x; x++) {
    for (int y = 0; y < sub_offset.y; y++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], clear_color);
    }
  }
  for (int y = sub_offset.y; y < sub_offset.y + sub_size.y; y++) {
    for (int x = 0; x < sub_offset.x; x++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], clear_color);
    }
    for (int x = sub_offset.x; x < sub_offset.x + sub_size.x; x++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], texture_color);
    }
    for (int x = sub_offset.x + sub_size.x; x < size.x; x++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], clear_color);
    }
  }
  for (int x = 0; x < size.x; x++) {
    for (int y = sub_offset.y + sub_size.y; y < size.y; y++) {
      int index = x + y * size.x;
      ASSERT_EQ(texture_data_read[index], clear_color);
    }
  }

  MEM_freeN(texture_data);
  MEM_freeN(texture_data_read);
  GPU_texture_free(texture);
}
GPU_TEST(texture_update_sub_unpack_row_length);

static void test_texture_pool()
{
  const int2 size1(10);
  const int2 size2(20);
  const int2 size3(30);

  TexturePool &pool = TexturePool::get();

  TextureFormat format1 = TextureFormat::UNORM_8_8_8_8;
  TextureFormat format2 = TextureFormat::SFLOAT_16_16_16_16;
  TextureFormat format3 = TextureFormat::SFLOAT_32_32_32_32;

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;

  auto test_acquire =
      [&](int2 size, TextureFormat format, eGPUTextureUsage usage) -> blender::gpu::Texture * {
    blender::gpu::Texture *tex = pool.acquire_texture(size.x, size.y, format, usage);
    EXPECT_EQ(GPU_texture_format(tex), format);
    EXPECT_EQ(GPU_texture_width(tex), size.x);
    EXPECT_EQ(GPU_texture_height(tex), size.y);
    return tex;
  };

  /* Tests multiple acquire. */
  blender::gpu::Texture *tex1 = test_acquire(size1, format1, usage);
  blender::gpu::Texture *tex2 = test_acquire(size2, format1, usage);
  blender::gpu::Texture *tex3 = test_acquire(size3, format2, usage);
  blender::gpu::Texture *tex4 = test_acquire(size3, format3, usage);

  pool.release_texture(tex1);

  /* Tests texture recycling. */
  /* Note we don't test if the same texture is reused as this is implementation dependent. */
  tex1 = test_acquire(size1, format1, usage);

  pool.release_texture(tex1);

  /* Tests missing release assert. */
  EXPECT_BLI_ASSERT(pool.reset(), "Missing texture release");

  pool.release_texture(tex2);
  pool.release_texture(tex3);
  pool.release_texture(tex4);

  /* Expects no assert. */
  pool.reset();
}
GPU_TEST(texture_pool);

/** \} */

}  // namespace blender::gpu::tests
