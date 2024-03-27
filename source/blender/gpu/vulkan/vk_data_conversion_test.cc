/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_data_conversion.hh"
#include "vk_device.hh"

#include "gpu_vertex_format_private.hh"

namespace blender::gpu::tests {
static void test_f32_f16(uint32_t f32_in, uint32_t f16_expected)
{
  const uint32_t f16 = convert_float_formats<FormatF16, FormatF32>(f32_in);
  EXPECT_EQ(f16, f16_expected);
  const uint32_t f32_reverse = convert_float_formats<FormatF32, FormatF16>(f16);
  EXPECT_EQ(f32_reverse, f32_in);
}

TEST(VulkanDataConversion, ConvertF32F16)
{
  /* 0.0 */
  test_f32_f16(0b00000000000000000000000000000000, 0b0000000000000000);
  /* 0.125 */
  test_f32_f16(0b00111110000000000000000000000000, 0b0011000000000000);
  /* 2.0 */
  test_f32_f16(0b01000000000000000000000000000000, 0b0100000000000000);
  /* 3.0 */
  test_f32_f16(0b01000000010000000000000000000000, 0b0100001000000000);
  /* 4.0 */
  test_f32_f16(0b01000000100000000000000000000000, 0b0100010000000000);
}

TEST(VulkanDataConversion, clamp_negative_to_zero)
{
  const uint32_t f32_2 = 0b11000000000000000000000000000000;
  const uint32_t f32_inf_min = 0b11111111100000000000000000000000;
  const uint32_t f32_inf_max = 0b01111111100000000000000000000000;
  const uint32_t f32_nan = 0b11111111111111111111111111111111;

  /* F32(-2) fits in F16. */
  const uint32_t f16_2_expected = 0b1100000000000000;
  const uint32_t f16_2a = convert_float_formats<FormatF16, FormatF32, true>(f32_2);
  EXPECT_EQ(f16_2a, f16_2_expected);

  const uint32_t f16_2b = convert_float_formats<FormatF16, FormatF32, false>(f32_2);
  EXPECT_EQ(f16_2b, f16_2_expected);

  /* F32(-2) doesn't fit in F11 as F11 only supports unsigned values. Clamp to zero. */
  const uint32_t f11_0_expected = 0b00000000000;
  const uint32_t f11_2_expected = 0b10000000000;
  const uint32_t f11_inf_expected = 0b11111000000;
  const uint32_t f11_nan_expected = 0b11111111111;
  {
    const uint32_t f11_0 = convert_float_formats<FormatF11, FormatF32, true>(f32_2);
    EXPECT_EQ(f11_0, f11_0_expected);
    const uint32_t f11_0b = convert_float_formats<FormatF11, FormatF32, true>(f32_inf_min);
    EXPECT_EQ(f11_0b, f11_0_expected);
    const uint32_t f11_inf = convert_float_formats<FormatF11, FormatF32, true>(f32_inf_max);
    EXPECT_EQ(f11_inf, f11_inf_expected);
    const uint32_t f11_nan = convert_float_formats<FormatF11, FormatF32, true>(f32_nan);
    EXPECT_EQ(f11_nan, f11_nan_expected);
  }

  /* F32(-2) doesn't fit in F11 as F11 only supports unsigned values. Make absolute. */
  {
    const uint32_t f11_2 = convert_float_formats<FormatF11, FormatF32, false>(f32_2);
    EXPECT_EQ(f11_2, f11_2_expected);
    const uint32_t f11_inf = convert_float_formats<FormatF11, FormatF32, false>(f32_inf_min);
    EXPECT_EQ(f11_inf, f11_inf_expected);
    const uint32_t f11_infb = convert_float_formats<FormatF11, FormatF32, false>(f32_inf_max);
    EXPECT_EQ(f11_infb, f11_inf_expected);
    const uint32_t f11_nan = convert_float_formats<FormatF11, FormatF32, false>(f32_nan);
    EXPECT_EQ(f11_nan, f11_nan_expected);
  }
}

TEST(VulkanDataConversion, infinity_upper)
{
  const uint32_t f32_inf = 0b01111111100000000000000000000000;

  const uint32_t f16_inf_expected = 0b0111110000000000;
  const uint32_t f16_inf = convert_float_formats<FormatF16, FormatF32, true>(f32_inf);
  EXPECT_EQ(f16_inf, f16_inf_expected);

  const uint32_t f11_inf_expected = 0b11111000000;
  const uint32_t f11_inf = convert_float_formats<FormatF11, FormatF32, true>(f32_inf);
  EXPECT_EQ(f11_inf, f11_inf_expected);

  const uint32_t f10_inf_expected = 0b1111100000;
  const uint32_t f10_inf = convert_float_formats<FormatF10, FormatF32, true>(f32_inf);
  EXPECT_EQ(f10_inf, f10_inf_expected);
}

TEST(VulkanDataConversion, infinity_lower)
{
  const uint32_t f32_inf = 0b11111111100000000000000000000000;

  const uint32_t f16_inf_expected = 0b1111110000000000;
  const uint32_t f16_inf = convert_float_formats<FormatF16, FormatF32, true>(f32_inf);
  EXPECT_EQ(f16_inf, f16_inf_expected);
}

TEST(VulkanDataConversion, vertex_format_i32_as_float)
{
  GPUVertFormat source_format;
  GPU_vertformat_clear(&source_format);
  GPU_vertformat_attr_add(&source_format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  VertexFormat_pack(&source_format);

  union TestData {
    int2 pos_i;
    float2 pos_fl;
  };
  TestData test_data[4];
  test_data[0].pos_i = int2(0, 1);
  test_data[1].pos_i = int2(1, 2);
  test_data[2].pos_i = int2(2, 3);
  test_data[3].pos_i = int2(3, 4);

  VKWorkarounds workarounds = {};
  VertexFormatConverter converter;
  converter.init(&source_format, workarounds);

  EXPECT_TRUE(converter.needs_conversion());

  converter.convert(&test_data, &test_data, 4);

  EXPECT_EQ(test_data[0].pos_fl, float2(0.0, 1.0));
  EXPECT_EQ(test_data[1].pos_fl, float2(1.0, 2.0));
  EXPECT_EQ(test_data[2].pos_fl, float2(2.0, 3.0));
  EXPECT_EQ(test_data[3].pos_fl, float2(3.0, 4.0));
}

TEST(VulkanDataConversion, vertex_format_u32_as_float)
{
  GPUVertFormat source_format;
  GPU_vertformat_clear(&source_format);
  GPU_vertformat_attr_add(&source_format, "pos", GPU_COMP_U32, 3, GPU_FETCH_INT_TO_FLOAT);
  VertexFormat_pack(&source_format);

  union TestData {
    uint3 pos_u;
    float3 pos_fl;
  };
  TestData test_data[4];
  test_data[0].pos_u = uint3(0, 1, 2);
  test_data[1].pos_u = uint3(1, 2, 3);
  test_data[2].pos_u = uint3(2, 3, 4);
  test_data[3].pos_u = uint3(3, 4, 5);

  VKWorkarounds workarounds = {};
  VertexFormatConverter converter;
  converter.init(&source_format, workarounds);

  EXPECT_TRUE(converter.needs_conversion());

  converter.convert(&test_data, &test_data, 4);

  EXPECT_EQ(test_data[0].pos_fl, float3(0.0, 1.0, 2.0));
  EXPECT_EQ(test_data[1].pos_fl, float3(1.0, 2.0, 3.0));
  EXPECT_EQ(test_data[2].pos_fl, float3(2.0, 3.0, 4.0));
  EXPECT_EQ(test_data[3].pos_fl, float3(3.0, 4.0, 5.0));
}

TEST(VulkanDataConversion, vertex_format_r8g8b8)
{
  GPUVertFormat source_format;
  GPU_vertformat_clear(&source_format);
  GPU_vertformat_attr_add(&source_format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
  VertexFormat_pack(&source_format);

  struct SourceData {
    uchar3 color;
    uint8_t _pad;
  };
  struct DeviceData {
    uchar4 color;
  };

  SourceData test_data_in[4];
  test_data_in[0].color = uchar3(255, 0, 0);
  test_data_in[1].color = uchar3(255, 255, 255);
  test_data_in[2].color = uchar3(255, 0, 0);
  test_data_in[3].color = uchar3(255, 255, 255);

  VKWorkarounds workarounds = {};
  VertexFormatConverter converter;
  converter.init(&source_format, workarounds);

  EXPECT_FALSE(converter.needs_conversion());

  /* Enable workaround for r8g8b8 vertex formats. */
  workarounds.vertex_formats.r8g8b8 = true;

  converter.init(&source_format, workarounds);
  EXPECT_TRUE(converter.needs_conversion());

  DeviceData test_data_out[4];
  converter.convert(test_data_out, test_data_in, 4);

  EXPECT_EQ(test_data_out[0].color, uchar4(255, 0, 0, 255));
  EXPECT_EQ(test_data_out[1].color, uchar4(255, 255, 255, 255));
  EXPECT_EQ(test_data_out[2].color, uchar4(255, 0, 0, 255));
  EXPECT_EQ(test_data_out[3].color, uchar4(255, 255, 255, 255));
}

TEST(VulkanDataConversion, vertex_format_multiple_attributes)
{
  GPUVertFormat source_format;
  GPU_vertformat_clear(&source_format);
  GPU_vertformat_attr_add(&source_format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  GPU_vertformat_attr_add(&source_format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
  GPU_vertformat_attr_add(&source_format, "flag", GPU_COMP_U32, 1, GPU_FETCH_INT);
  VertexFormat_pack(&source_format);

  struct SourceData {
    float3 pos;
    uchar3 color;
    uint8_t _pad;
    uint flag;
  };
  struct DeviceData {
    float3 pos;
    uchar4 color;
    uint flag;
  };

  SourceData test_data_in[4];
  test_data_in[0] = {float3(1.0, 2.0, 3.0), uchar3(255, 0, 0), 0, 0};
  test_data_in[1] = {float3(4.0, 5.0, 6.0), uchar3(0, 255, 0), 0, 1};
  test_data_in[2] = {float3(7.0, 8.0, 9.0), uchar3(0, 0, 255), 0, 2};
  test_data_in[3] = {float3(10.0, 11.0, 12.0), uchar3(255, 255, 255), 0, 3};

  VKWorkarounds workarounds = {};
  workarounds.vertex_formats.r8g8b8 = true;
  VertexFormatConverter converter;
  converter.init(&source_format, workarounds);
  EXPECT_TRUE(converter.needs_conversion());

  DeviceData test_data_out[4];
  converter.convert(test_data_out, test_data_in, 4);

  DeviceData expected_data[4];
  expected_data[0] = {float3(1.0, 2.0, 3.0), uchar4(255, 0, 0, 255), 0};
  expected_data[1] = {float3(4.0, 5.0, 6.0), uchar4(0, 255, 0, 255), 1};
  expected_data[2] = {float3(7.0, 8.0, 9.0), uchar4(0, 0, 255, 255), 2};
  expected_data[3] = {float3(10.0, 11.0, 12.0), uchar4(255, 255, 255, 255), 3};
  for (int i : IndexRange(4)) {
    EXPECT_EQ(test_data_out[i].pos, expected_data[i].pos);
    EXPECT_EQ(test_data_out[i].color, expected_data[i].color);
    EXPECT_EQ(test_data_out[i].flag, expected_data[i].flag);
  }
}

TEST(VulkanDataConversion, texture_rgb16f_as_floats_to_rgba16f)
{
  const size_t num_pixels = 4;
  float input[] = {
      1.0,
      0.5,
      0.2,

      0.2,
      1.0,
      0.3,

      0.4,
      0.2,
      1.0,

      1.0,
      1.0,
      1.0,
  };

  uint64_t device[num_pixels];
  convert_host_to_device(device, input, num_pixels, GPU_DATA_FLOAT, GPU_RGB16F, GPU_RGBA16F);

  float read_back[num_pixels * 3];
  convert_device_to_host(read_back, device, num_pixels, GPU_DATA_FLOAT, GPU_RGB16F, GPU_RGBA16F);

  for (int i : IndexRange(num_pixels * 3)) {
    EXPECT_NEAR(input[i], read_back[i], 0.01);
  }
}

TEST(VulkanDataConversion, texture_rgb32f_as_floats_to_rgba32f)
{
  const size_t num_pixels = 4;
  float input[] = {
      1.0,
      0.5,
      0.2,

      0.2,
      1.0,
      0.3,

      0.4,
      0.2,
      1.0,

      1.0,
      1.0,
      1.0,
  };

  float device[num_pixels * 4];
  convert_host_to_device(device, input, num_pixels, GPU_DATA_FLOAT, GPU_RGB32F, GPU_RGBA32F);

  float read_back[num_pixels * 3];
  convert_device_to_host(read_back, device, num_pixels, GPU_DATA_FLOAT, GPU_RGB32F, GPU_RGBA32F);

  for (int i : IndexRange(num_pixels * 3)) {
    EXPECT_NEAR(input[i], read_back[i], 0.01);
  }
}

}  // namespace blender::gpu::tests
