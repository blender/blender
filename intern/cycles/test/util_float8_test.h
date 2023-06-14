/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"
#include "util/math.h"
#include "util/system.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

static bool validate_cpu_capabilities()
{

#if defined(__KERNEL_AVX2__)
  return system_cpu_support_avx2();
#elif defined(__KERNEL_AVX__)
  return system_cpu_support_avx();
#elif defined(__KERNEL_SSE2__)
  return system_cpu_support_sse2();
#else
  return false;
#endif
}

/* These are not just static variables because we don't want to run the
 * constructor until we know the instructions are supported. */
static vfloat8 float8_a()
{
  return make_vfloat8(0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f);
}

static vfloat8 float8_b()
{
  return make_vfloat8(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f);
}

static vfloat8 float8_c()
{
  return make_vfloat8(1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f);
}

#define INIT_FLOAT8_TEST \
  if (!validate_cpu_capabilities()) \
    return;

#define compare_vector_scalar(a, b) \
  for (size_t index = 0; index < 8; index++) \
    EXPECT_FLOAT_EQ(a[index], b);

#define compare_vector_vector(a, b) \
  for (size_t index = 0; index < 8; index++) \
    EXPECT_FLOAT_EQ(a[index], b[index]);

#define compare_vector_vector_near(a, b, abserror) \
  for (size_t index = 0; index < 8; index++) \
    EXPECT_NEAR(a[index], b[index], abserror);

#define basic_test_vv(a, b, op) \
  INIT_FLOAT8_TEST \
  vfloat8 c = a op b; \
  for (size_t i = 0; i < 8; i++) \
    EXPECT_FLOAT_EQ(c[i], a[i] op b[i]);

/* vector op float tests */
#define basic_test_vf(a, b, op) \
  INIT_FLOAT8_TEST \
  vfloat8 c = a op b; \
  for (size_t i = 0; i < 8; i++) \
    EXPECT_FLOAT_EQ(c[i], a[i] op b);

static const float float_b = 1.5f;

TEST(TEST_CATEGORY_NAME, float8_add_vv){
    basic_test_vv(float8_a(), float8_b(), +)} TEST(TEST_CATEGORY_NAME, float8_sub_vv){
    basic_test_vv(float8_a(), float8_b(), -)} TEST(TEST_CATEGORY_NAME, float8_mul_vv){
    basic_test_vv(float8_a(), float8_b(), *)} TEST(TEST_CATEGORY_NAME, float8_div_vv){
    basic_test_vv(float8_a(), float8_b(), /)} TEST(TEST_CATEGORY_NAME, float8_add_vf){
    basic_test_vf(float8_a(), float_b, +)} TEST(TEST_CATEGORY_NAME, float8_sub_vf){
    basic_test_vf(float8_a(), float_b, -)} TEST(TEST_CATEGORY_NAME, float8_mul_vf){
    basic_test_vf(float8_a(), float_b, *)} TEST(TEST_CATEGORY_NAME, float8_div_vf){
    basic_test_vf(float8_a(), float_b, /)}

TEST(TEST_CATEGORY_NAME, float8_ctor)
{
  INIT_FLOAT8_TEST
  compare_vector_scalar(make_vfloat8(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f),
                        static_cast<float>(index));
  compare_vector_scalar(make_vfloat8(1.0f), 1.0f);
}

TEST(TEST_CATEGORY_NAME, float8_sqrt)
{
  INIT_FLOAT8_TEST
  compare_vector_vector(sqrt(make_vfloat8(1.0f, 4.0f, 9.0f, 16.0f, 25.0f, 36.0f, 49.0f, 64.0f)),
                        make_vfloat8(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f));
}

TEST(TEST_CATEGORY_NAME, float8_min_max)
{
  INIT_FLOAT8_TEST
  compare_vector_vector(min(float8_a(), float8_b()), float8_a());
  compare_vector_vector(max(float8_a(), float8_b()), float8_b());
}

TEST(TEST_CATEGORY_NAME, float8_shuffle)
{
  INIT_FLOAT8_TEST
  vfloat8 res0 = shuffle<0, 1, 2, 3, 1, 3, 2, 0>(float8_a());
  compare_vector_vector(res0, make_vfloat8(0.1f, 0.2f, 0.3f, 0.4f, 0.6f, 0.8f, 0.7f, 0.5f));
  vfloat8 res1 = shuffle<3>(float8_a());
  compare_vector_vector(res1, make_vfloat8(0.4f, 0.4f, 0.4f, 0.4f, 0.8f, 0.8f, 0.8f, 0.8f));
  vfloat8 res2 = shuffle<3, 2, 1, 0>(float8_a(), float8_b());
  compare_vector_vector(res2, make_vfloat8(0.4f, 0.3f, 2.0f, 1.0f, 0.8f, 0.7f, 6.0f, 5.0f));
}

CCL_NAMESPACE_END
