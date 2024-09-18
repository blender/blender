/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_half.hh"
#include "BLI_time.h"

#include <cmath>

// #define DO_PERF_TESTS 1

namespace blender::tests {

TEST(math_half, half_to_float_scalar)
{
  EXPECT_EQ(blender::math::half_to_float(0), 0.0f);
  EXPECT_EQ(blender::math::half_to_float(1), 5.960464478e-08f);
  EXPECT_EQ(blender::math::half_to_float(32), 1.907348633e-06f);
  EXPECT_EQ(blender::math::half_to_float(37), 2.205371857e-06f);
  EXPECT_EQ(blender::math::half_to_float(511), 3.045797348e-05f);
  EXPECT_EQ(blender::math::half_to_float(999), 5.954504013e-05f);
  EXPECT_EQ(blender::math::half_to_float(1024), 6.103515625e-05f);
  EXPECT_EQ(blender::math::half_to_float(1357), 8.088350296e-05f);
  EXPECT_EQ(blender::math::half_to_float(6789), 0.003183364868f);
  EXPECT_EQ(blender::math::half_to_float(16383), 1.999023438f);
  EXPECT_EQ(blender::math::half_to_float(16384), 2.0f);
  EXPECT_EQ(blender::math::half_to_float(31743), 65504.0f);
  EXPECT_EQ(blender::math::half_to_float(31744), std::numeric_limits<float>::infinity());
  EXPECT_TRUE(std::isnan(blender::math::half_to_float(31746)));
  EXPECT_TRUE(std::isnan(blender::math::half_to_float(32767)));
  EXPECT_EQ(blender::math::half_to_float(32768), -0.0f);
  EXPECT_EQ(blender::math::half_to_float(32769), -5.960464478e-08f);
  EXPECT_EQ(blender::math::half_to_float(46765), -0.4172363281f);
  EXPECT_EQ(blender::math::half_to_float(54501), -78.3125f);
  EXPECT_EQ(blender::math::half_to_float(64511), -65504.0f);
  EXPECT_EQ(blender::math::half_to_float(64512), -std::numeric_limits<float>::infinity());
  EXPECT_TRUE(std::isnan(blender::math::half_to_float(64513)));
  EXPECT_TRUE(std::isnan(blender::math::half_to_float(65535)));
}

TEST(math_half, float_to_half_scalar)
{
#define HFUN(v) blender::math::float_to_half(v)
  EXPECT_EQ(HFUN(0.0f), 0);
  EXPECT_EQ(HFUN(std::numeric_limits<float>::min()), 0);
  EXPECT_EQ(HFUN(5.960464478e-08f), 1);
  EXPECT_EQ(HFUN(1.907348633e-06f), 32);
  EXPECT_EQ(HFUN(2.205371857e-06f), 37);
  EXPECT_EQ(HFUN(3.045797348e-05f), 511);
  EXPECT_EQ(HFUN(5.954504013e-05f), 999);
  EXPECT_EQ(HFUN(6.103515625e-05f), 1024);
  EXPECT_EQ(HFUN(8.088350296e-05f), 1357);
  EXPECT_EQ(HFUN(0.003183364868f), 6789);
  EXPECT_EQ(HFUN(0.1f), 11878);
  EXPECT_EQ(HFUN(1.0f), 15360);
  EXPECT_EQ(HFUN(1.999023438f), 16383);
  EXPECT_EQ(HFUN(1.999523438f), 16384);
  EXPECT_EQ(HFUN(2.0f), 16384);
  EXPECT_EQ(HFUN(11.0f), 18816);
  EXPECT_EQ(HFUN(65504.0f), 31743);
  EXPECT_EQ(HFUN(65535.0f), 31744); /* FP16 inf */
  EXPECT_EQ(HFUN(1.0e6f), 31744);   /* FP16 inf */
  EXPECT_EQ(HFUN(std::numeric_limits<float>::infinity()), 31744);
  EXPECT_EQ(HFUN(std::numeric_limits<float>::max()), 31744);
  EXPECT_EQ(HFUN(std::numeric_limits<float>::quiet_NaN()), 32256);
  EXPECT_EQ(HFUN(-0.0f), 32768);
  EXPECT_EQ(HFUN(-5.960464478e-08f), 32769);
  EXPECT_EQ(HFUN(-0.4172363281f), 46765);
  EXPECT_EQ(HFUN(-1.0f), 48128);
  EXPECT_EQ(HFUN(-78.3125f), 54501);
  EXPECT_EQ(HFUN(-123.5f), 55224);
  EXPECT_EQ(HFUN(-65504.0f), 64511);
  EXPECT_EQ(HFUN(-65536.0f), 64512); /* FP16 -inf */
  EXPECT_EQ(HFUN(-1.0e6f), 64512);   /* FP16 -inf */
  EXPECT_EQ(HFUN(-std::numeric_limits<float>::infinity()), 64512);
#undef HFUN
}

#ifdef DO_PERF_TESTS

/*
 * Performance numbers of various other solutions, all on Ryzen 5950X, VS2022.
 * This is time taken to convert 100 million numbers FP16 -> FP32.
 *
 * - CPU: F16C instructions 44ms
 * - OpenEXR/Imath: 21ms
 * - blender::math::half_to_float: 164ms
 * - convert_float_formats from VK_data_conversion.hh: 244ms [converts 2046 values wrong]
 *
 * On Mac M1 Max (Clang 15):
 * - blender::math::half_to_float: 127ms (C), 97ms (NEON vcvt)
 */
TEST(math_half_perf, half_to_float_scalar)
{
  double t0 = BLI_time_now_seconds();
  size_t sum = 0;
  for (int i = 0; i < 100'000'000; i++) {
    float f = blender::math::half_to_float(uint16_t(i & 0xFFFF));
    uint32_t fu;
    memcpy(&fu, &f, sizeof(f));
    sum += fu;
  }
  double t1 = BLI_time_now_seconds();
  printf("- FP16->FP32 blimath: %.3fs sum %zu\n", t1 - t0, sum);
}

/*
 * Performance numbers of various other solutions, all on Ryzen 5950X, VS2022.
 * This is time taken to convert 100 million numbers FP32 -> FP16.
 *
 * - CPU: F16C instructions 61ms
 * - OpenEXR/Imath: 240ms
 * - blender::math::float_to_half: 242ms
 * - convert_float_formats from VK_data_conversion.hh: 247ms [converts many values wrong]
 *
 * On Mac M1 Max (Clang 15):
 * - blender::math::half_to_float: 198ms (C), 97ms (NEON vcvt)
 */
TEST(math_half_perf, float_to_half_scalar_math)  // 242ms
{
  double t0 = BLI_time_now_seconds();
  uint32_t sum = 0;
  for (int i = 0; i < 100'000'000; i++) {
    float f = ((i & 0xFFFF) - 0x8000) + 0.1f;
    uint16_t h = blender::math::float_to_half(f);
    sum += h;
  }
  double t1 = BLI_time_now_seconds();
  printf("- FP32->FP16 blimath: %.3fs sum %u\n", t1 - t0, sum);
}

#endif  // #ifdef DO_PERF_TESTS

}  // namespace blender::tests
