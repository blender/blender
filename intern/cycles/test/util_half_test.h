/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include "util/half.h"
#include "util/system.h"
#include <array>

CCL_NAMESPACE_BEGIN

static bool validate_cpu_capabilities()
{
#if defined(__KERNEL_AVX2__)
  return system_cpu_support_avx2();
#else
  return true;
#endif
}

#define EXPECT_HALF_EQ(a, b) \
  do { \
    const uint16_t a_ = (a); \
    const uint16_t b_ = (b); \
    if ((a_ & 0x7fff) == 0 && (b_ & 0x7fff) == 0) { \
      /* Ignore sign of zero, -0.0 getting converted to 0.0 is fine. */ \
    } \
    else { \
      EXPECT_EQ(a_, b_); \
    } \
  } while (0)

struct HalfTestVal {
  float f;
  uint16_t h;
};

/* We don't support NaN and inf, so not tested here. */
static const std::array<HalfTestVal, 13> test_values = {{{0.0f, 0x0000},
                                                         {-0.0f, 0x8000},
                                                         {1.0f, 0x3c00},
                                                         {2.0f, 0x4000},
                                                         {10.0f, 0x4900},
                                                         {100.0f, 0x5640},
                                                         {0.0999755859375f, 0x2e66},
                                                         {-0.0999755859375f, 0xae66},
                                                         {-0.5f, 0xb800},
                                                         {-1.0f, 0xbc00},
                                                         /* Smallest positive normal (2^-14) */
                                                         {6.103515625e-5f, 0x0400},
                                                         /* Largest denormal (2^-14 - 2^-24) */
                                                         {6.0975551605e-5f, 0x03ff},
                                                         /* Smallest positive denormal (2^-24) */
                                                         {5.9604644775e-8f, 0x0001}}};

TEST(TEST_CATEGORY_NAME, float_to_half)
{
  if (!validate_cpu_capabilities()) {
    GTEST_SKIP();
    return;
  }
  for (const HalfTestVal &val : test_values) {
    EXPECT_HALF_EQ(float_to_half(val.f), val.h);
    EXPECT_EQ(half_to_float(half(val.h)), val.f);
  }
}

TEST(TEST_CATEGORY_NAME, float3_to_half3)
{
  if (!validate_cpu_capabilities()) {
    GTEST_SKIP();
    return;
  }
  for (size_t i = 0; i < test_values.size(); i++) {
    const size_t i0 = i;
    const size_t i1 = (i + 1) % test_values.size();
    const size_t i2 = (i + 2) % test_values.size();

    const float3 in = make_float3(test_values[i0].f, test_values[i1].f, test_values[i2].f);

    const half3 h = float3_to_half3(in);
    const float3 out = half3_to_float3(h);

    EXPECT_EQ(out.x, in.x);
    EXPECT_EQ(out.y, in.y);
    EXPECT_EQ(out.z, in.z);

    EXPECT_HALF_EQ(uint16_t(h.x), test_values[i0].h);
    EXPECT_HALF_EQ(uint16_t(h.y), test_values[i1].h);
    EXPECT_HALF_EQ(uint16_t(h.z), test_values[i2].h);
  }
}

TEST(TEST_CATEGORY_NAME, float4_to_half4)
{
  if (!validate_cpu_capabilities()) {
    GTEST_SKIP();
    return;
  }
  for (size_t i = 0; i < test_values.size(); i += 4) {
    const size_t i0 = i;
    const size_t i1 = (i + 1) % test_values.size();
    const size_t i2 = (i + 2) % test_values.size();
    const size_t i3 = (i + 3) % test_values.size();

    const float4 in = make_float4(
        test_values[i0].f, test_values[i1].f, test_values[i2].f, test_values[i3].f);

    const half4 h = float4_to_half4(in);
    const float4 out = half4_to_float4(h);

    EXPECT_EQ(out.x, in.x);
    EXPECT_EQ(out.y, in.y);
    EXPECT_EQ(out.z, in.z);
    EXPECT_EQ(out.w, in.w);

    EXPECT_HALF_EQ(uint16_t(h.x), test_values[i0].h);
    EXPECT_HALF_EQ(uint16_t(h.y), test_values[i1].h);
    EXPECT_HALF_EQ(uint16_t(h.z), test_values[i2].h);
    EXPECT_HALF_EQ(uint16_t(h.w), test_values[i3].h);
  }
}

TEST(TEST_CATEGORY_NAME, fallback_float_to_half)
{
  if (!validate_cpu_capabilities()) {
    GTEST_SKIP();
    return;
  }
  for (const HalfTestVal &val : test_values) {
    EXPECT_HALF_EQ(fallback_float_to_half(val.f), val.h);
    EXPECT_EQ(fallback_half_to_float(half(val.h)), val.f);
  }
}

TEST(TEST_CATEGORY_NAME, fallback_float3_to_half3)
{
  if (!validate_cpu_capabilities()) {
    GTEST_SKIP();
    return;
  }
  for (size_t i = 0; i < test_values.size(); i++) {
    const size_t i0 = i;
    const size_t i1 = (i + 1) % test_values.size();
    const size_t i2 = (i + 2) % test_values.size();

    const float3 in = make_float3(test_values[i0].f, test_values[i1].f, test_values[i2].f);

    const half3 h = fallback_float3_to_half3(in);
    const float3 out = fallback_half3_to_float3(h);

    EXPECT_EQ(out.x, in.x);
    EXPECT_EQ(out.y, in.y);
    EXPECT_EQ(out.z, in.z);

    EXPECT_HALF_EQ(uint16_t(h.x), test_values[i0].h);
    EXPECT_HALF_EQ(uint16_t(h.y), test_values[i1].h);
    EXPECT_HALF_EQ(uint16_t(h.z), test_values[i2].h);
  }
}

TEST(TEST_CATEGORY_NAME, fallback_float4_to_half4)
{
  if (!validate_cpu_capabilities()) {
    GTEST_SKIP();
    return;
  }
  for (size_t i = 0; i < test_values.size(); i += 4) {
    const size_t i0 = i;
    const size_t i1 = (i + 1) % test_values.size();
    const size_t i2 = (i + 2) % test_values.size();
    const size_t i3 = (i + 3) % test_values.size();

    const float4 in = make_float4(
        test_values[i0].f, test_values[i1].f, test_values[i2].f, test_values[i3].f);

    const half4 h = fallback_float4_to_half4(in);
    const float4 out = fallback_half4_to_float4(h);

    EXPECT_EQ(out.x, in.x);
    EXPECT_EQ(out.y, in.y);
    EXPECT_EQ(out.z, in.z);
    EXPECT_EQ(out.w, in.w);

    EXPECT_HALF_EQ(uint16_t(h.x), test_values[i0].h);
    EXPECT_HALF_EQ(uint16_t(h.y), test_values[i1].h);
    EXPECT_HALF_EQ(uint16_t(h.z), test_values[i2].h);
    EXPECT_HALF_EQ(uint16_t(h.w), test_values[i3].h);
  }
}

CCL_NAMESPACE_END
