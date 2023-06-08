/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "gpu_testing.hh"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

#include "GPU_context.h"
#include "GPU_texture.h"

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
  GPUTexture *rgba32u = GPU_texture_create_2d("rgba32u", 1, 1, 1, GPU_RGBA32UI, usage, nullptr);
  GPUTexture *rgba16u = GPU_texture_create_2d("rgba16u", 1, 1, 1, GPU_RGBA16UI, usage, nullptr);
  GPUTexture *rgba32f = GPU_texture_create_2d("rgba32f", 1, 1, 1, GPU_RGBA32F, usage, nullptr);

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

static void test_texture_copy()
{
  const int SIZE = 128;
  GPU_render_begin();

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *src_tx = GPU_texture_create_2d("src", SIZE, SIZE, 1, GPU_RGBA32F, usage, nullptr);
  GPUTexture *dst_tx = GPU_texture_create_2d("dst", SIZE, SIZE, 1, GPU_RGBA32F, usage, nullptr);

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
  DataType *data = static_cast<DataType *>(MEM_mallocN(data_len * sizeof(DataType), __func__));
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

template<eGPUTextureFormat DeviceFormat,
         eGPUDataFormat HostFormat,
         typename DataType,
         int Size = 16>
static void texture_create_upload_read()
{
  static_assert(!std::is_same<DataType, float>());
  static_assert(validate_data_format(DeviceFormat, HostFormat));
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture = GPU_texture_create_2d(
      "texture", Size, Size, 1, DeviceFormat, usage, nullptr);
  if (texture == nullptr) {
    GTEST_SKIP() << "Platform doesn't support texture format [" << STRINGIFY(DeviceFormat) << "]";
  }

  size_t data_len = Size * Size * to_component_len(DeviceFormat);
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

template<eGPUTextureFormat DeviceFormat, eGPUDataFormat HostFormat, int Size = 16>
static void texture_create_upload_read_with_bias(float max_allowed_bias)
{
  static_assert(validate_data_format(DeviceFormat, HostFormat));
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture = GPU_texture_create_2d(
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
template<eGPUTextureFormat DeviceFormat, eGPUDataFormat HostFormat, int Size = 16>
static void texture_create_upload_read_pixel()
{
  using DataType = uint32_t;
  static_assert(validate_data_format(DeviceFormat, HostFormat));
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_HOST_READ;
  GPUTexture *texture = GPU_texture_create_2d(
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
  texture_create_upload_read_with_bias<GPU_RGBA8, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16F()
{
  texture_create_upload_read_with_bias<GPU_RGBA16F, GPU_DATA_FLOAT>(0.9f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16()
{
  texture_create_upload_read_with_bias<GPU_RGBA16, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA32F()
{
  texture_create_upload_read_with_bias<GPU_RGBA32F, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA32F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG8()
{
  texture_create_upload_read_with_bias<GPU_RG8, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16F()
{
  texture_create_upload_read_with_bias<GPU_RG16F, GPU_DATA_FLOAT>(0.9f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16()
{
  texture_create_upload_read_with_bias<GPU_RG16, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG32F()
{
  texture_create_upload_read_with_bias<GPU_RG32F, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG32F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R8()
{
  texture_create_upload_read_with_bias<GPU_R8, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R16F()
{
  texture_create_upload_read_with_bias<GPU_R16F, GPU_DATA_FLOAT>(0.9f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R16F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R16()
{
  texture_create_upload_read_with_bias<GPU_R16, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R16);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R32F()
{
  texture_create_upload_read_with_bias<GPU_R32F, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R32F);

#if RUN_NON_STANDARD_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB10_A2()
{
  texture_create_upload_read_with_bias<GPU_RGB10_A2, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB10_A2);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB10_A2UI()
{
  texture_create_upload_read_with_bias<GPU_RGB10_A2UI, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB10_A2UI);
#endif

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R11F_G11F_B10F()
{
  texture_create_upload_read_with_bias<GPU_R11F_G11F_B10F, GPU_DATA_FLOAT>(0.0009f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R11F_G11F_B10F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8()
{
  texture_create_upload_read_with_bias<GPU_SRGB8_A8, GPU_DATA_FLOAT>(0.003f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_SNORM()
{
  texture_create_upload_read_with_bias<GPU_RGBA8_SNORM, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16_SNORM()
{
  texture_create_upload_read_with_bias<GPU_RGBA16_SNORM, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA16_SNORM);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB8()
{
  texture_create_upload_read_with_bias<GPU_RGB8, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB8);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB8_SNORM()
{
  texture_create_upload_read_with_bias<GPU_RGB8_SNORM, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB8_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16F()
{
  texture_create_upload_read_with_bias<GPU_RGB16F, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16F);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16()
{
  texture_create_upload_read_with_bias<GPU_RGB16, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16);
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16_SNORM()
{
  texture_create_upload_read_with_bias<GPU_RGB16_SNORM, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB16_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB32F()
{
  texture_create_upload_read_with_bias<GPU_RGB32F, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB32F);
#endif

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG8_SNORM()
{
  texture_create_upload_read_with_bias<GPU_RG8_SNORM, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG8_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16_SNORM()
{
  texture_create_upload_read_with_bias<GPU_RG16_SNORM, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RG16_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R8_SNORM()
{
  texture_create_upload_read_with_bias<GPU_R8_SNORM, GPU_DATA_FLOAT>(0.004f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R8_SNORM);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_R16_SNORM()
{
  texture_create_upload_read_with_bias<GPU_R16_SNORM, GPU_DATA_FLOAT>(0.00002f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_R16_SNORM);

#if RUN_NON_STANDARD_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT1()
{
  texture_create_upload_read_with_bias<GPU_SRGB8_A8_DXT1, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT1);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT3()
{
  texture_create_upload_read_with_bias<GPU_SRGB8_A8_DXT3, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT3);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT5()
{
  texture_create_upload_read_with_bias<GPU_SRGB8_A8_DXT5, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8_A8_DXT5);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT1()
{
  texture_create_upload_read_with_bias<GPU_RGBA8_DXT1, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT1);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT3()
{
  texture_create_upload_read_with_bias<GPU_RGBA8_DXT3, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT3);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT5()
{
  texture_create_upload_read_with_bias<GPU_RGBA8_DXT5, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGBA8_DXT5);
#endif

#if RUN_SRGB_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8()
{
  texture_create_upload_read_with_bias<GPU_SRGB8, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_SRGB8);
#endif

#if RUN_NON_STANDARD_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB9_E5()
{
  texture_create_upload_read_with_bias<GPU_RGB9_E5, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_RGB9_E5);
#endif

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT32F()
{
  texture_create_upload_read_with_bias<GPU_DEPTH_COMPONENT32F, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT32F);

#if RUN_COMPONENT_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT24()
{
  texture_create_upload_read_with_bias<GPU_DEPTH_COMPONENT24, GPU_DATA_FLOAT>(0.0000001f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT24);

static void test_texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT16()
{
  texture_create_upload_read_with_bias<GPU_DEPTH_COMPONENT16, GPU_DATA_FLOAT>(0.0f);
}
GPU_TEST(texture_roundtrip__GPU_DATA_FLOAT__GPU_DEPTH_COMPONENT16);
#endif

/* \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_HALF_FLOAT
 * \{ */

static void test_texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RGBA16F()
{
  texture_create_upload_read<GPU_RGBA16F, GPU_DATA_HALF_FLOAT, uint16_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RGBA16F);

static void test_texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RG16F()
{
  texture_create_upload_read<GPU_RG16F, GPU_DATA_HALF_FLOAT, uint16_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RG16F);

static void test_texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_R16F()
{
  texture_create_upload_read<GPU_R16F, GPU_DATA_HALF_FLOAT, uint16_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_R16F);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RGB16F()
{
  texture_create_upload_read<GPU_RGB16F, GPU_DATA_HALF_FLOAT, uint16_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_HALF_FLOAT__GPU_RGB16F);
#endif

/* \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_INT
 * \{ */

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGBA8I()
{
  texture_create_upload_read<GPU_RGBA8I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGBA8I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGBA16I()
{
  texture_create_upload_read<GPU_RGBA16I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGBA16I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGBA32I()
{
  texture_create_upload_read<GPU_RGBA32I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGBA32I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RG8I()
{
  texture_create_upload_read<GPU_RG8I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RG8I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RG16I()
{
  texture_create_upload_read<GPU_RG16I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RG16I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RG32I()
{
  texture_create_upload_read<GPU_RG32I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RG32I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_R8I()
{
  texture_create_upload_read<GPU_R8I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_R8I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_R16I()
{
  texture_create_upload_read<GPU_R16I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_R16I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_R32I()
{
  texture_create_upload_read<GPU_R32I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_R32I);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGB8I()
{
  texture_create_upload_read<GPU_RGB8I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGB8I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGB16I()
{
  texture_create_upload_read<GPU_RGB16I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGB16I);

static void test_texture_roundtrip__GPU_DATA_INT__GPU_RGB32I()
{
  texture_create_upload_read<GPU_RGB32I, GPU_DATA_INT, int32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_INT__GPU_RGB32I);
#endif

/* \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_UINT
 * \{ */

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGBA8UI()
{
  texture_create_upload_read<GPU_RGBA8UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGBA8UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGBA16UI()
{
  texture_create_upload_read<GPU_RGBA16UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGBA16UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGBA32UI()
{
  texture_create_upload_read<GPU_RGBA32UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGBA32UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RG8UI()
{
  texture_create_upload_read<GPU_RG8UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RG8UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RG16UI()
{
  texture_create_upload_read<GPU_RG16UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RG16UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RG32UI()
{
  texture_create_upload_read<GPU_RG32UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RG32UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_R8UI()
{
  texture_create_upload_read<GPU_R8UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_R8UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_R16UI()
{
  texture_create_upload_read<GPU_R16UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_R16UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_R32UI()
{
  texture_create_upload_read<GPU_R32UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_R32UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH32F_STENCIL8()
{
  texture_create_upload_read<GPU_DEPTH32F_STENCIL8, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH32F_STENCIL8);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH24_STENCIL8()
{
  texture_create_upload_read<GPU_DEPTH24_STENCIL8, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH24_STENCIL8);

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGB8UI()
{
  texture_create_upload_read<GPU_RGB8UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGB8UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGB16UI()
{
  texture_create_upload_read<GPU_RGB16UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGB16UI);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_RGB32UI()
{
  texture_create_upload_read<GPU_RGB32UI, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_RGB32UI);
#endif

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT32F()
{
  texture_create_upload_read<GPU_DEPTH_COMPONENT32F, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT32F);

static void test_texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT24()
{
  texture_create_upload_read<GPU_DEPTH_COMPONENT24, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT24);

#if RUN_COMPONENT_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT16()
{
  texture_create_upload_read<GPU_DEPTH_COMPONENT16, GPU_DATA_UINT, uint32_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT__GPU_DEPTH_COMPONENT16);
#endif

/* \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_UBYTE
 * \{ */

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RGBA8UI()
{
  texture_create_upload_read<GPU_RGBA8UI, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RGBA8UI);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RGBA8()
{
  texture_create_upload_read<GPU_RGBA8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RGBA8);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RG8UI()
{
  texture_create_upload_read<GPU_RG8UI, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RG8UI);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RG8()
{
  texture_create_upload_read<GPU_RG8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RG8);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_R8UI()
{
  texture_create_upload_read<GPU_R8UI, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_R8UI);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_R8()
{
  texture_create_upload_read<GPU_R8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_R8);

#if RUN_SRGB_UNIMPLEMENTED
static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_SRGB8_A8()
{
  texture_create_upload_read<GPU_SRGB8_A8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_SRGB8_A8);
#endif

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RGB8I()
{
  texture_create_upload_read<GPU_RGB8I, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RGB8I);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_RGB8()
{
  texture_create_upload_read<GPU_RGB8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_RGB8);

static void test_texture_roundtrip__GPU_DATA_UBYTE__GPU_SRGB8()
{
  texture_create_upload_read<GPU_SRGB8, GPU_DATA_UBYTE, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UBYTE__GPU_SRGB8);
#endif
/* \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_UINT_24_8
 * \{ */

#if RUN_UNSUPPORTED
static void test_texture_roundtrip__GPU_DATA_UINT_24_8__GPU_DEPTH32F_STENCIL8()
{
  texture_create_upload_read<GPU_DEPTH32F_STENCIL8, GPU_DATA_UINT_24_8, void>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT_24_8__GPU_DEPTH32F_STENCIL8);

static void test_texture_roundtrip__GPU_DATA_UINT_24_8__GPU_DEPTH24_STENCIL8()
{
  texture_create_upload_read<GPU_DEPTH24_STENCIL8, GPU_DATA_UINT_24_8, void>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_UINT_24_8__GPU_DEPTH24_STENCIL8);
#endif

/* \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_10_11_11_REV
 * \{ */

static void test_texture_roundtrip__GPU_DATA_10_11_11_REV__GPU_R11F_G11F_B10F()
{
  texture_create_upload_read<GPU_R11F_G11F_B10F, GPU_DATA_10_11_11_REV, uint8_t>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_10_11_11_REV__GPU_R11F_G11F_B10F);

/* \} */

/* -------------------------------------------------------------------- */
/** \name Round-trip testing GPU_DATA_2_10_10_10_REV
 * \{ */

static void test_texture_roundtrip__GPU_DATA_2_10_10_10_REV__GPU_RGB10_A2()
{
  texture_create_upload_read_pixel<GPU_RGB10_A2, GPU_DATA_2_10_10_10_REV>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_2_10_10_10_REV__GPU_RGB10_A2);

static void test_texture_roundtrip__GPU_DATA_2_10_10_10_REV__GPU_RGB10_A2UI()
{
  texture_create_upload_read_pixel<GPU_RGB10_A2UI, GPU_DATA_2_10_10_10_REV>();
}
GPU_TEST(texture_roundtrip__GPU_DATA_2_10_10_10_REV__GPU_RGB10_A2UI);

/* \} */

}  // namespace blender::gpu::tests
