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

#ifndef __BLI_FLOAT4X4_HH__
#define __BLI_FLOAT4X4_HH__

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

  float4x4(const float matrix[4][4]) : float4x4((float *)matrix)
  {
  }

  operator float *()
  {
    return (float *)this;
  }

  operator const float *() const
  {
    return (const float *)this;
  }

  float4x4 inverted() const
  {
    float result[4][4];
    invert_m4_m4(result, values);
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

  friend float4x4 operator*(const float4x4 &a, const float4x4 &b)
  {
    float4x4 result;
    mul_m4_m4m4(result.values, a.values, b.values);
    return result;
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
};

}  // namespace blender

#endif /* __BLI_FLOAT4X4_HH__ */
