/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_color_types.hh"
#include "testing/testing.h"

#include "BLI_math_matrix_types.hh"
#include "BLI_math_rotation_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_structure_of_array_fixed.hh"

namespace blender::tests {

struct HeterogeneousSoaValue {
  float a;
  uint8_t b;
  double c;

  friend bool operator==(const HeterogeneousSoaValue &, const HeterogeneousSoaValue &) = default;
};

template<size_t I> constexpr decltype(auto) fetch_soa_component(const HeterogeneousSoaValue &value)
{
  if constexpr (I == 0) {
    return value.a;
  }
  else if constexpr (I == 1) {
    return value.b;
  }
  else {
    static_assert(I == 2);
    return value.c;
  }
}

}  // namespace blender::tests

namespace blender {
template<> constexpr size_t soa_component_num<tests::HeterogeneousSoaValue> = 3;
}

namespace blender::tests {

TEST(StructureOfArray, Simple)
{
  FixedSoaArray<float3, 5> buffer;
  float *xs = buffer.data<0>();
  float *ys = buffer.data<1>();
  float *zs = buffer.data<2>();

  EXPECT_EQ(buffer.data<0>(), buffer.data(0));
  EXPECT_EQ(buffer.data<1>(), buffer.data(1));
  EXPECT_EQ(buffer.data<2>(), buffer.data(2));

  buffer.set(0, float3(5, 3, 1));
  buffer.set(3, float3(0, 1, 2));

  EXPECT_EQ(xs[0], 5);
  EXPECT_EQ(ys[0], 3);
  EXPECT_EQ(zs[0], 1);

  EXPECT_EQ(xs[3], 0);
  EXPECT_EQ(ys[3], 1);
  EXPECT_EQ(zs[3], 2);

  zs[3] = 10;

  EXPECT_EQ(buffer[0], float3(5, 3, 1));
  EXPECT_EQ(buffer[3], float3(0, 1, 10));
}

TEST(StructureOfArray, DerivedColorType)
{
  EXPECT_TRUE(SoaType<ColorGeometry4f>);

  FixedSoaArray<ColorGeometry4f, 4> buffer;
  buffer.set(1, ColorGeometry4f(0.1f, 0.2f, 0.3f, 1.0f));
  EXPECT_EQ(buffer.data<0>()[1], 0.1f);
  EXPECT_EQ(buffer.data<1>()[1], 0.2f);
  EXPECT_EQ(buffer.data<2>()[1], 0.3f);
  EXPECT_EQ(buffer.data<3>()[1], 1.0f);
  EXPECT_EQ(buffer[1], ColorGeometry4f(0.1f, 0.2f, 0.3f, 1.0f));
}

TEST(StructureOfArray, Float3TransposeRoundtrip)
{
  FixedSoaArray<float3, 7> buffer;
  float3 src[7];
  for (const int64_t i : IndexRange(7)) {
    src[i] = float3(float(i), float(i) + 0.25f, float(i) + 0.5f);
  }
  buffer.set_multiple(src, 7);

  for (const int64_t i : IndexRange(7)) {
    EXPECT_EQ(buffer.data<0>()[i], src[i].x);
    EXPECT_EQ(buffer.data<1>()[i], src[i].y);
    EXPECT_EQ(buffer.data<2>()[i], src[i].z);
  }

  float3 dst[7];
  buffer.get_multiple(dst, 7);
  for (const int64_t i : IndexRange(7)) {
    EXPECT_EQ(dst[i], src[i]);
  }
}

TEST(StructureOfArray, Float4AndColorTransposeRoundtrip)
{
  FixedSoaArray<float4, 5> fbuffer;
  float4 fsrc[5];
  for (const int64_t i : IndexRange(5)) {
    fsrc[i] = float4(float(i), float(i) + 0.1f, float(i) + 0.2f, float(i) + 0.3f);
  }
  fbuffer.set_multiple(fsrc, 5);
  float4 fdst[5];
  fbuffer.get_multiple(fdst, 5);
  for (const int64_t i : IndexRange(5)) {
    EXPECT_EQ(fdst[i], fsrc[i]);
  }

  FixedSoaArray<ColorGeometry4f, 6> cbuffer;
  ColorGeometry4f csrc[6];
  for (const int64_t i : IndexRange(6)) {
    csrc[i] = ColorGeometry4f(float(i), float(i) + 0.1f, float(i) + 0.2f, 1.0f);
  }
  cbuffer.set_multiple(csrc, 6);
  ColorGeometry4f cdst[6];
  cbuffer.get_multiple(cdst, 6);
  for (const int64_t i : IndexRange(6)) {
    EXPECT_EQ(cdst[i], csrc[i]);
  }
}

TEST(StructureOfArray, SoaPtr)
{
  float xs[4] = {1, 2, 3, 4};
  float ys[4] = {5, 6, 7, 8};
  float zs[4] = {9, 10, 11, 12};

  MutableSoaPtr<float3> mutable_ptr(xs, ys, zs);
  EXPECT_EQ(mutable_ptr.data(0), xs);
  EXPECT_EQ(mutable_ptr.data(1), ys);
  EXPECT_EQ(mutable_ptr.data(2), zs);
  mutable_ptr.set(1, float3(20, 30, 40));
  EXPECT_EQ(xs[1], 20);
  EXPECT_EQ(ys[1], 30);
  EXPECT_EQ(zs[1], 40);
  EXPECT_EQ(mutable_ptr[1], float3(20, 30, 40));

  const SoaPtr<float3> ptr = mutable_ptr;
  EXPECT_EQ(ptr.data(0), xs);
  EXPECT_EQ(ptr.data(1), ys);
  EXPECT_EQ(ptr.data(2), zs);
  EXPECT_EQ(ptr[0], float3(1, 5, 9));
  EXPECT_EQ(ptr[1], float3(20, 30, 40));

  float3 values[4];
  ptr.get_multiple(values, 4);
  EXPECT_EQ(values[2], float3(3, 7, 11));

  float3 src[4] = {float3(1), float3(2), float3(3), float3(4)};
  mutable_ptr.set_multiple(src, 4);
  EXPECT_EQ(xs[3], 4);
  EXPECT_EQ(ys[3], 4);
  EXPECT_EQ(zs[3], 4);
}

TEST(StructureOfArray, PtrFromFixedSoaArray)
{
  FixedSoaArray<float3, 8> arrays;
  arrays.set(0, float3(1, 2, 3));
  arrays.set(2, float3(4, 5, 6));

  MutableSoaPtr mutable_ptr = arrays.data();
  EXPECT_EQ(mutable_ptr[0], float3(1, 2, 3));
  mutable_ptr.set(1, float3(7, 8, 9));
  EXPECT_EQ(arrays[1], float3(7, 8, 9));

  SoaPtr ptr = arrays.data();
  EXPECT_EQ(ptr[2], float3(4, 5, 6));

  MutableSoaPtr full_mutable_ptr = arrays;
  EXPECT_EQ(full_mutable_ptr[0], float3(1, 2, 3));
  SoaPtr full_ptr = arrays;
  EXPECT_EQ(full_ptr[2], float3(4, 5, 6));
}

TEST(StructureOfArray, RotationTypes)
{
  FixedSoaArray<math::Quaternion, 3> quats;
  quats.set(1, math::Quaternion(0.1f, 0.2f, 0.3f, 0.4f));
  EXPECT_EQ(quats.data<0>()[1], 0.1f);
  EXPECT_EQ(quats.data<1>()[1], 0.2f);
  EXPECT_EQ(quats.data<2>()[1], 0.3f);
  EXPECT_EQ(quats.data<3>()[1], 0.4f);
  EXPECT_EQ(quats[1], math::Quaternion(0.1f, 0.2f, 0.3f, 0.4f));
}

TEST(StructureOfArray, MatBase)
{
  EXPECT_TRUE(SoaType<float3x3>);
  EXPECT_TRUE(SoaType<float4x4>);
  EXPECT_EQ(soa_component_num<float3x3>, 9);
  EXPECT_EQ(soa_component_num<float4x4>, 16);

  FixedSoaArray<float4x4, 2> mats;
  const float4x4 mat(
      float4(1, 2, 3, 4), float4(5, 6, 7, 8), float4(9, 10, 11, 12), float4(13, 14, 15, 16));
  mats.set(0, mat);
  EXPECT_EQ(mats.data<0>()[0], 1.0f);
  EXPECT_EQ(mats.data<1>()[0], 2.0f);
  EXPECT_EQ(mats.data<4>()[0], 5.0f);
  EXPECT_EQ(mats.data<15>()[0], 16.0f);
  EXPECT_EQ(mats[0], mat);
}

TEST(StructureOfArray, Heterogeneous)
{
  EXPECT_TRUE(SoaType<HeterogeneousSoaValue>);
  EXPECT_FALSE(soa_is_homogeneous_v<HeterogeneousSoaValue>);
  EXPECT_TRUE((std::is_same_v<soa_component_t<HeterogeneousSoaValue, 0>, float>));
  EXPECT_TRUE((std::is_same_v<soa_component_t<HeterogeneousSoaValue, 1>, uint8_t>));
  EXPECT_TRUE((std::is_same_v<soa_component_t<HeterogeneousSoaValue, 2>, double>));

  FixedSoaArray<HeterogeneousSoaValue, 3> buffer;
  const HeterogeneousSoaValue value{1.5f, 7, 9.25};
  buffer.set(1, value);
  EXPECT_EQ(buffer.data<0>()[1], 1.5f);
  EXPECT_EQ(buffer.data<1>()[1], 7);
  EXPECT_EQ(buffer.data<2>()[1], 9.25);
  EXPECT_EQ(buffer[1], value);
}

}  // namespace blender::tests
