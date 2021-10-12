/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include "BLI_float3.hh"
#include "BLI_math_matrix.h"

namespace blender {

struct float4x4 {
  float values[4][4];

  float4x4() = default;

  float4x4(const float *matrix)
  {
    memcpy(values, matrix, sizeof(float) * 16);
  }

  float4x4(const float matrix[4][4]) : float4x4(static_cast<const float *>(matrix[0]))
  {
  }

  /* Assumes an XYZ euler order. */
  static float4x4 from_loc_eul_scale(const float3 location,
                                     const float3 rotation,
                                     const float3 scale)
  {
    float4x4 mat;
    loc_eul_size_to_mat4(mat.values, location, rotation, scale);
    return mat;
  }

  static float4x4 from_location(const float3 location)
  {
    float4x4 mat = float4x4::identity();
    copy_v3_v3(mat.values[3], location);
    return mat;
  }

  static float4x4 from_normalized_axis_data(const float3 location,
                                            const float3 forward,
                                            const float3 up)
  {
    BLI_ASSERT_UNIT_V3(forward);
    BLI_ASSERT_UNIT_V3(up);

    /* Negate the cross product so that the resulting matrix has determinant 1 (instead of -1).
     * Without the negation, the result would be a so called improper rotation. That means it
     * contains a reflection. Such an improper rotation matrix could not be converted to another
     * representation of a rotation such as euler angles. */
    const float3 cross = -float3::cross(forward, up);

    float4x4 matrix;
    matrix.values[0][0] = forward.x;
    matrix.values[1][0] = cross.x;
    matrix.values[2][0] = up.x;
    matrix.values[3][0] = location.x;

    matrix.values[0][1] = forward.y;
    matrix.values[1][1] = cross.y;
    matrix.values[2][1] = up.y;
    matrix.values[3][1] = location.y;

    matrix.values[0][2] = forward.z;
    matrix.values[1][2] = cross.z;
    matrix.values[2][2] = up.z;
    matrix.values[3][2] = location.z;

    matrix.values[0][3] = 0.0f;
    matrix.values[1][3] = 0.0f;
    matrix.values[2][3] = 0.0f;
    matrix.values[3][3] = 1.0f;

    return matrix;
  }

  static float4x4 identity()
  {
    float4x4 mat;
    unit_m4(mat.values);
    return mat;
  }

  operator float *()
  {
    return &values[0][0];
  }

  operator const float *() const
  {
    return &values[0][0];
  }

  using c_style_float4x4 = float[4][4];
  c_style_float4x4 &ptr()
  {
    return values;
  }

  const c_style_float4x4 &ptr() const
  {
    return values;
  }

  friend float4x4 operator*(const float4x4 &a, const float4x4 &b)
  {
    float4x4 result;
    mul_m4_m4m4(result.values, a.values, b.values);
    return result;
  }

  void operator*=(const float4x4 &other)
  {
    mul_m4_m4_post(values, other.values);
  }

  /**
   * This also applies the translation on the vector. Use `m.ref_3x3() * v` if that is not
   * intended.
   */
  friend float3 operator*(const float4x4 &m, const float3 &v)
  {
    float3 result;
    mul_v3_m4v3(result, m.values, v);
    return result;
  }

  friend float3 operator*(const float4x4 &m, const float (*v)[3])
  {
    return m * float3(v);
  }

  float3 translation() const
  {
    return float3(values[3]);
  }

  /* Assumes XYZ rotation order. */
  float3 to_euler() const
  {
    float3 euler;
    mat4_to_eul(euler, values);
    return euler;
  }

  float3 scale() const
  {
    float3 scale;
    mat4_to_size(scale, values);
    return scale;
  }

  void apply_scale(const float scale)
  {
    values[0][0] *= scale;
    values[0][1] *= scale;
    values[0][2] *= scale;
    values[1][0] *= scale;
    values[1][1] *= scale;
    values[1][2] *= scale;
    values[2][0] *= scale;
    values[2][1] *= scale;
    values[2][2] *= scale;
  }

  float4x4 inverted() const
  {
    float4x4 result;
    invert_m4_m4(result.values, values);
    return result;
  }

  /**
   * Matrix inversion can be implemented more efficiently for affine matrices.
   */
  float4x4 inverted_affine() const
  {
    BLI_assert(values[0][3] == 0.0f && values[1][3] == 0.0f && values[2][3] == 0.0f &&
               values[3][3] == 1.0f);
    return this->inverted();
  }

  float4x4 transposed() const
  {
    float4x4 result;
    transpose_m4_m4(result.values, values);
    return result;
  }

  float4x4 inverted_transposed_affine() const
  {
    return this->inverted_affine().transposed();
  }

  struct float3x3_ref {
    const float4x4 &data;

    friend float3 operator*(const float3x3_ref &m, const float3 &v)
    {
      float3 result;
      mul_v3_mat3_m4v3(result, m.data.values, v);
      return result;
    }
  };

  float3x3_ref ref_3x3() const
  {
    return {*this};
  }

  static float4x4 interpolate(const float4x4 &a, const float4x4 &b, float t)
  {
    float result[4][4];
    interp_m4_m4m4(result, a.values, b.values, t);
    return result;
  }

  bool is_negative() const
  {
    return is_negative_m4(ptr());
  }

  uint64_t hash() const
  {
    uint64_t h = 435109;
    for (int i = 0; i < 16; i++) {
      float value = (static_cast<const float *>(values[0]))[i];
      h = h * 33 + *reinterpret_cast<const uint32_t *>(&value);
    }
    return h;
  }
};

}  // namespace blender
