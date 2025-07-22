/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_data_conversion.hh"
#include "vk_device.hh"

#include "gpu_vertex_format_private.hh"

namespace blender::gpu::tests {

TEST(VulkanDataConversion, clamp_negative_to_zero)
{
  const uint32_t f32_2 = 0b11000000000000000000000000000000;
  const uint32_t f32_inf_min = 0b11111111100000000000000000000000;
  const uint32_t f32_inf_max = 0b01111111100000000000000000000000;
  const uint32_t f32_nan = 0b11111111111111111111111111111111;

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

  const uint32_t f11_inf_expected = 0b11111000000;
  const uint32_t f11_inf = convert_float_formats<FormatF11, FormatF32, true>(f32_inf);
  EXPECT_EQ(f11_inf, f11_inf_expected);

  const uint32_t f10_inf_expected = 0b1111100000;
  const uint32_t f10_inf = convert_float_formats<FormatF10, FormatF32, true>(f32_inf);
  EXPECT_EQ(f10_inf, f10_inf_expected);
}

TEST(VulkanDataConversion, texture_rgb16f_as_floats_to_rgba16f)
{
  const size_t num_pixels = 4;
  const float input[] = {
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
  convert_host_to_device(device,
                         input,
                         num_pixels,
                         GPU_DATA_FLOAT,
                         TextureFormat::SFLOAT_16_16_16,
                         TextureFormat::SFLOAT_16_16_16_16);

  float read_back[num_pixels * 3];
  convert_device_to_host(read_back,
                         device,
                         num_pixels,
                         GPU_DATA_FLOAT,
                         TextureFormat::SFLOAT_16_16_16,
                         TextureFormat::SFLOAT_16_16_16_16);

  for (int i : IndexRange(num_pixels * 3)) {
    EXPECT_NEAR(input[i], read_back[i], 0.01);
  }
}

TEST(VulkanDataConversion, texture_rgb32f_as_floats_to_rgba32f)
{
  const size_t num_pixels = 4;
  const float input[] = {
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
  convert_host_to_device(device,
                         input,
                         num_pixels,
                         GPU_DATA_FLOAT,
                         TextureFormat::SFLOAT_32_32_32,
                         TextureFormat::SFLOAT_32_32_32_32);

  float read_back[num_pixels * 3];
  convert_device_to_host(read_back,
                         device,
                         num_pixels,
                         GPU_DATA_FLOAT,
                         TextureFormat::SFLOAT_32_32_32,
                         TextureFormat::SFLOAT_32_32_32_32);

  for (int i : IndexRange(num_pixels * 3)) {
    EXPECT_NEAR(input[i], read_back[i], 0.01);
  }
}

}  // namespace blender::gpu::tests
