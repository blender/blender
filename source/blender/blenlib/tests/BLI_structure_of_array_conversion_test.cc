/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_structure_of_array_conversion.hh"

#include "BLI_strict_flags.hh" /* IWYU pragma: keep. Keep last. */

namespace blender::tests {

/* Cover scalar tails, SSE (4/8), AVX2 (8/16), and mixed remainders. */
static const int64_t test_sizes[] = {0,  1,  2,  3,  4,  5,  7,  8,  9,  12,
                                     15, 16, 17, 19, 24, 31, 32, 33, 64, 100};

TEST(structure_of_array_conversion, AosToSoa3x4byte)
{
  for (const int64_t size : test_sizes) {
    Array<float3> aos(size);
    Array<float> x(size);
    Array<float> y(size);
    Array<float> z(size);
    for (const int64_t i : IndexRange(size)) {
      aos[i] = float3(float(i), float(i) + 0.25f, float(i) + 0.5f);
    }

    aos_to_soa_3x4byte(size, aos.data(), x.data(), y.data(), z.data());

    for (const int64_t i : IndexRange(size)) {
      EXPECT_EQ(x[i], aos[i].x) << "size=" << size << " i=" << i;
      EXPECT_EQ(y[i], aos[i].y) << "size=" << size << " i=" << i;
      EXPECT_EQ(z[i], aos[i].z) << "size=" << size << " i=" << i;
    }
  }
}

TEST(structure_of_array_conversion, SoaToAos3x4byte)
{
  for (const int64_t size : test_sizes) {
    Array<float> x(size);
    Array<float> y(size);
    Array<float> z(size);
    Array<float3> aos(size);
    for (const int64_t i : IndexRange(size)) {
      x[i] = float(i);
      y[i] = float(i) + 0.25f;
      z[i] = float(i) + 0.5f;
    }

    soa_to_aos_3x4byte(size, x.data(), y.data(), z.data(), aos.data());

    for (const int64_t i : IndexRange(size)) {
      EXPECT_EQ(aos[i], float3(x[i], y[i], z[i])) << "size=" << size << " i=" << i;
    }
  }
}

TEST(structure_of_array_conversion, AosToSoa4x4byte)
{
  for (const int64_t size : test_sizes) {
    Array<float4> aos(size);
    Array<float> x(size);
    Array<float> y(size);
    Array<float> z(size);
    Array<float> w(size);
    for (const int64_t i : IndexRange(size)) {
      aos[i] = float4(float(i), float(i) + 0.1f, float(i) + 0.2f, float(i) + 0.3f);
    }

    aos_to_soa_4x4byte(size, aos.data(), x.data(), y.data(), z.data(), w.data());

    for (const int64_t i : IndexRange(size)) {
      EXPECT_EQ(x[i], aos[i].x) << "size=" << size << " i=" << i;
      EXPECT_EQ(y[i], aos[i].y) << "size=" << size << " i=" << i;
      EXPECT_EQ(z[i], aos[i].z) << "size=" << size << " i=" << i;
      EXPECT_EQ(w[i], aos[i].w) << "size=" << size << " i=" << i;
    }
  }
}

TEST(structure_of_array_conversion, SoaToAos4x4byte)
{
  for (const int64_t size : test_sizes) {
    Array<float> x(size);
    Array<float> y(size);
    Array<float> z(size);
    Array<float> w(size);
    Array<float4> aos(size);
    for (const int64_t i : IndexRange(size)) {
      x[i] = float(i);
      y[i] = float(i) + 0.1f;
      z[i] = float(i) + 0.2f;
      w[i] = float(i) + 0.3f;
    }

    soa_to_aos_4x4byte(size, x.data(), y.data(), z.data(), w.data(), aos.data());

    for (const int64_t i : IndexRange(size)) {
      EXPECT_EQ(aos[i], float4(x[i], y[i], z[i], w[i])) << "size=" << size << " i=" << i;
    }
  }
}

TEST(structure_of_array_conversion, Roundtrip3x4byteInt)
{
  for (const int64_t size : test_sizes) {
    Array<int3> aos(size);
    Array<int> x(size);
    Array<int> y(size);
    Array<int> z(size);
    for (const int64_t i : IndexRange(size)) {
      aos[i] = int3(int(i), int(i) + 1, int(i) + 2);
    }

    aos_to_soa_3x4byte(size, aos.data(), x.data(), y.data(), z.data());
    Array<int3> roundtrip(size);
    soa_to_aos_3x4byte(size, x.data(), y.data(), z.data(), roundtrip.data());

    for (const int64_t i : IndexRange(size)) {
      EXPECT_EQ(roundtrip[i], aos[i]) << "size=" << size << " i=" << i;
    }
  }
}

TEST(structure_of_array_conversion, Roundtrip4x4byteInt)
{
  for (const int64_t size : test_sizes) {
    Array<int4> aos(size);
    Array<int> x(size);
    Array<int> y(size);
    Array<int> z(size);
    Array<int> w(size);
    for (const int64_t i : IndexRange(size)) {
      aos[i] = int4(int(i), int(i) + 1, int(i) + 2, int(i) + 3);
    }

    aos_to_soa_4x4byte(size, aos.data(), x.data(), y.data(), z.data(), w.data());
    Array<int4> roundtrip(size);
    soa_to_aos_4x4byte(size, x.data(), y.data(), z.data(), w.data(), roundtrip.data());

    for (const int64_t i : IndexRange(size)) {
      EXPECT_EQ(roundtrip[i], aos[i]) << "size=" << size << " i=" << i;
    }
  }
}

}  // namespace blender::tests
