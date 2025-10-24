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

TEST(math_half, float_to_half_make_finite_scalar)
{
#define HFUN(v) blender::math::float_to_half_make_finite(v)
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
  /* Too large: result is FP16 65504. */
  EXPECT_EQ(HFUN(65535.0f), 31743);
  EXPECT_EQ(HFUN(1.0e6f), 31743);
  EXPECT_EQ(HFUN(std::numeric_limits<float>::infinity()), 31743);
  EXPECT_EQ(HFUN(std::numeric_limits<float>::max()), 31743);
  /* NaN: result is zero. */
  EXPECT_EQ(HFUN(std::numeric_limits<float>::quiet_NaN()), 0);
  EXPECT_EQ(HFUN(std::numeric_limits<float>::signaling_NaN()), 0);
  EXPECT_EQ(HFUN(-0.0f), 32768);
  EXPECT_EQ(HFUN(-5.960464478e-08f), 32769);
  EXPECT_EQ(HFUN(-0.4172363281f), 46765);
  EXPECT_EQ(HFUN(-1.0f), 48128);
  EXPECT_EQ(HFUN(-78.3125f), 54501);
  EXPECT_EQ(HFUN(-123.5f), 55224);
  EXPECT_EQ(HFUN(-65504.0f), 64511);
  /* Too large negative: result is FP16 -65504. */
  EXPECT_EQ(HFUN(-65536.0f), 64511);
  EXPECT_EQ(HFUN(-1.0e6f), 64511);
  EXPECT_EQ(HFUN(-std::numeric_limits<float>::infinity()), 64511);
  /* -NaN: result is negative zero. */
  EXPECT_EQ(HFUN(-std::numeric_limits<float>::quiet_NaN()), 32768);
  EXPECT_EQ(HFUN(-std::numeric_limits<float>::signaling_NaN()), 32768);
#undef HFUN
}

TEST(math_half, half_to_float_array)
{
  const uint16_t src[13] = {
      0, 1, 6789, 16383, 16384, 31743, 31744, 32768, 32769, 46765, 54501, 64511, 64512};
  /* One extra entry in destination, to check that function leaves it intact. */
  const float exp[14] = {
      0.0f,
      5.960464478e-08f,
      0.003183364868f,
      1.999023438f,
      2.0f,
      65504.0f,
      std::numeric_limits<float>::infinity(),
      -0.0f,
      -5.960464478e-08f,
      -0.4172363281f,
      -78.3125f,
      -65504.0f,
      -std::numeric_limits<float>::infinity(),
      1.2345f,
  };
  float dst[14] = {};
  dst[13] = 1.2345f;

  blender::math::half_to_float_array(src, dst, 13);
  EXPECT_EQ_ARRAY(exp, dst, 14);
}

TEST(math_half, float_to_half_array)
{
  const float src[13] = {0.0f,
                         5.960464478e-08f,
                         0.003183364868f,
                         1.999023438f,
                         2.0f,
                         65504.0f,
                         std::numeric_limits<float>::infinity(),
                         -0.0f,
                         -5.960464478e-08f,
                         -0.4172363281f,
                         -78.3125f,
                         -65504.0f,
                         -std::numeric_limits<float>::infinity()};
  /* One extra entry in destination, to check that function leaves it intact. */
  const uint16_t exp[14] = {
      0, 1, 6789, 16383, 16384, 31743, 31744, 32768, 32769, 46765, 54501, 64511, 64512, 12345};
  uint16_t dst[14] = {};
  dst[13] = 12345;

  blender::math::float_to_half_array(src, dst, 13);
  EXPECT_EQ_ARRAY(exp, dst, 14);
}

TEST(math_half, float_to_half_make_finite_array)
{
  const float src[17] = {
      0.0f,
      5.960464478e-08f,
      0.003183364868f,
      1.999023438f,
      2.0f,
      65504.0f,
      std::numeric_limits<float>::infinity(),
      -0.0f,
      -5.960464478e-08f,
      -0.4172363281f,
      -78.3125f,
      -65504.0f,
      -std::numeric_limits<float>::infinity(),
      100000.0f,
      -100000.0f,
      std::numeric_limits<float>::quiet_NaN(),
      -std::numeric_limits<float>::quiet_NaN(),
  };
  /* One extra entry in destination, to check that function leaves it intact. */
  const uint16_t exp[18] = {0,
                            1,
                            6789,
                            16383,
                            16384,
                            31743,
                            31743,
                            32768,
                            32769,
                            46765,
                            54501,
                            64511,
                            64511,
                            31743,
                            64511,
                            0,
                            32768,
                            12345};
  uint16_t dst[18] = {};
  dst[17] = 12345;
  blender::math::float_to_half_make_finite_array(src, dst, 17);
  EXPECT_EQ_ARRAY(exp, dst, 18);
}

#ifdef DO_PERF_TESTS

/*
 * Time to convert 100 million numbers FP16 -> FP32.
 *
 * Ryzen 5950X (VS2022):
 * - `half_to_float`: 164ms
 * - `half_to_float_array`: 132ms (scalar)
 * - `half_to_float_array`:  84ms (SSE2 4x wide path)
 * - `half_to_float_array`:  86ms (w/ AVX2 F16C - however Blender is not compiled for AVX2 yet)
 *
 * Mac M1 Max (Clang 15), using NEON VCVT:
 * - `half_to_float`: 97ms
 * - `half_to_float_array`: 53ms
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
  printf("- FP16->FP32 scalar: %.3fs sum %zu\n", t1 - t0, sum);
}

TEST(math_half_perf, half_to_float_array)
{
  const int test_size = 100'000'000;
  uint16_t *src = new uint16_t[test_size];
  float *dst = new float[test_size];
  for (int i = 0; i < test_size; i++) {
    src[i] = i & 0xFFFF;
  }
  double t0 = BLI_time_now_seconds();
  size_t sum = 0;
  blender::math::half_to_float_array(src, dst, test_size);
  for (int i = 0; i < test_size; i++) {
    uint32_t fu;
    memcpy(&fu, &dst[i], sizeof(fu));
    sum += fu;
  }
  double t1 = BLI_time_now_seconds();
  printf("- FP16->FP32 array : %.3fs sum %zu\n", t1 - t0, sum);
  delete[] src;
  delete[] dst;
}

/*
 * Time to convert 100 million numbers FP32 -> FP16.
 *
 * Ryzen 5950X (VS2022):
 * - `float_to_half`: 242ms
 * - `float_to_half_array`: 184ms (scalar)
 * - `float_to_half_array`:  68ms (SSE2 4x wide path)
 * - `float_to_half_array`:  50ms (w/ AVX2 F16C - however Blender is not compiled for AVX2 yet)
 *
 * Mac M1 Max (Clang 15), using NEON VCVT:
 * - `float_to_half`: 93ms
 * - `float_to_half_array`: 21ms
 */
TEST(math_half_perf, float_to_half_scalar)
{
  double t0 = BLI_time_now_seconds();
  uint32_t sum = 0;
  for (int i = 0; i < 100'000'000; i++) {
    float f = ((i & 0xFFFF) - 0x8000) + 0.1f;
    uint16_t h = blender::math::float_to_half(f);
    sum += h;
  }
  double t1 = BLI_time_now_seconds();
  printf("- FP32->FP16 scalar: %.3fs sum %u\n", t1 - t0, sum);
}

TEST(math_half_perf, float_to_half_array)
{
  const int test_size = 100'000'000;
  float *src = new float[test_size];
  uint16_t *dst = new uint16_t[test_size];
  for (int i = 0; i < test_size; i++) {
    src[i] = ((i & 0xFFFF) - 0x8000) + 0.1f;
  }

  double t0 = BLI_time_now_seconds();
  uint32_t sum = 0;
  blender::math::float_to_half_array(src, dst, test_size);
  for (int i = 0; i < test_size; i++) {
    sum += dst[i];
  }
  double t1 = BLI_time_now_seconds();
  printf("- FP32->FP16 array : %.3fs sum %u\n", t1 - t0, sum);
  delete[] src;
  delete[] dst;
}

TEST(math_half_perf, float_to_half_make_finite_array)
{
  const int test_size = 100'000'000;
  float *src = new float[test_size];
  uint16_t *dst = new uint16_t[test_size];
  for (int i = 0; i < test_size; i++) {
    src[i] = ((i & 0xFFFF) - 0x8000) + 0.1f;
  }
  double t0 = BLI_time_now_seconds();
  uint32_t sum = 0;
  blender::math::float_to_half_make_finite_array(src, dst, test_size);
  for (int i = 0; i < test_size; i++) {
    sum += dst[i];
  }
  double t1 = BLI_time_now_seconds();
  printf("- FP32->FP16 finite array : %.3fs sum %u\n", t1 - t0, sum);
  delete[] src;
  delete[] dst;
}

#endif  // #ifdef DO_PERF_TESTS

}  // namespace blender::tests
