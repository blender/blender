/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cmath>
#include <cstdint>

#include "BLI_assert.h"
#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

namespace blender {

struct float3x3 {
  /* A 3x3 matrix in column major order. */
  float values[3][3];

  float3x3() = default;

  float3x3(const float *matrix)
  {
    memcpy(values, matrix, sizeof(float) * 3 * 3);
  }

  float3x3(const float matrix[3][3]) : float3x3(static_cast<const float *>(matrix[0]))
  {
  }

  static float3x3 zero()
  {
    float3x3 result;
    zero_m3(result.values);
    return result;
  }

  static float3x3 identity()
  {
    float3x3 result;
    unit_m3(result.values);
    return result;
  }

  static float3x3 from_translation(const float2 translation)
  {
    float3x3 result = identity();
    result.values[2][0] = translation.x;
    result.values[2][1] = translation.y;
    return result;
  }

  static float3x3 from_rotation(float rotation)
  {
    float3x3 result = zero();
    const float cosine = std::cos(rotation);
    const float sine = std::sin(rotation);
    result.values[0][0] = cosine;
    result.values[0][1] = sine;
    result.values[1][0] = -sine;
    result.values[1][1] = cosine;
    result.values[2][2] = 1.0f;
    return result;
  }

  static float3x3 from_scale(const float2 scale)
  {
    float3x3 result = zero();
    result.values[0][0] = scale.x;
    result.values[1][1] = scale.y;
    result.values[2][2] = 1.0f;
    return result;
  }

  static float3x3 from_translation_rotation_scale(const float2 translation,
                                                  float rotation,
                                                  const float2 scale)
  {
    float3x3 result;
    const float cosine = std::cos(rotation);
    const float sine = std::sin(rotation);
    result.values[0][0] = scale.x * cosine;
    result.values[0][1] = scale.x * sine;
    result.values[0][2] = 0.0f;
    result.values[1][0] = scale.y * -sine;
    result.values[1][1] = scale.y * cosine;
    result.values[1][2] = 0.0f;
    result.values[2][0] = translation.x;
    result.values[2][1] = translation.y;
    result.values[2][2] = 1.0f;
    return result;
  }

  static float3x3 from_normalized_axes(const float2 translation,
                                       const float2 horizontal,
                                       const float2 vertical)
  {
    BLI_ASSERT_UNIT_V2(horizontal);
    BLI_ASSERT_UNIT_V2(vertical);

    float3x3 result;
    result.values[0][0] = horizontal.x;
    result.values[0][1] = horizontal.y;
    result.values[0][2] = 0.0f;
    result.values[1][0] = vertical.x;
    result.values[1][1] = vertical.y;
    result.values[1][2] = 0.0f;
    result.values[2][0] = translation.x;
    result.values[2][1] = translation.y;
    result.values[2][2] = 1.0f;
    return result;
  }

  /* Construct a transformation that is pivoted around the given origin point. So for instance,
   * from_origin_transformation(from_rotation(M_PI_2), float2(0.0f, 2.0f))
   * will construct a transformation representing a 90 degree rotation around the point (0, 2). */
  static float3x3 from_origin_transformation(const float3x3 &transformation, const float2 origin)
  {
    return from_translation(origin) * transformation * from_translation(-origin);
  }

  operator float *()
  {
    return &values[0][0];
  }

  operator const float *() const
  {
    return &values[0][0];
  }

  float *operator[](const int64_t index)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < 3);
    return &values[index][0];
  }

  const float *operator[](const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < 3);
    return &values[index][0];
  }

  using c_style_float3x3 = float[3][3];
  c_style_float3x3 &ptr()
  {
    return values;
  }

  const c_style_float3x3 &ptr() const
  {
    return values;
  }

  friend float3x3 operator*(const float3x3 &a, const float3x3 &b)
  {
    float3x3 result;
    mul_m3_m3m3(result.values, a.values, b.values);
    return result;
  }

  friend float3 operator*(const float3x3 &a, const float3 &b)
  {
    float3 result;
    mul_v3_m3v3(result, a.values, b);
    return result;
  }

  void operator*=(const float3x3 &other)
  {
    mul_m3_m3_post(values, other.values);
  }

  friend float2 operator*(const float3x3 &transformation, const float2 &vector)
  {
    float2 result;
    mul_v2_m3v2(result, transformation.values, vector);
    return result;
  }

  friend float2 operator*(const float3x3 &transformation, const float (*vector)[2])
  {
    return transformation * float2(vector);
  }

  float3x3 transposed() const
  {
    float3x3 result;
    transpose_m3_m3(result.values, values);
    return result;
  }

  float3x3 inverted() const
  {
    float3x3 result;
    invert_m3_m3(result.values, values);
    return result;
  }

  float2 scale_2d() const
  {
    float2 scale;
    mat3_to_size_2d(scale, values);
    return scale;
  }

  friend bool operator==(const float3x3 &a, const float3x3 &b)
  {
    return equals_m3m3(a.values, b.values);
  }
};

}  // namespace blender
