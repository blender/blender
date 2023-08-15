/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_data_conversion.hh"

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

}  // namespace blender::gpu::tests
