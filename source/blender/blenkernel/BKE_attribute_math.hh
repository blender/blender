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

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "DNA_customdata_types.h"

namespace blender::attribute_math {

/**
 * Utility function that simplifies calling a templated function based on a custom data type.
 */
template<typename Func>
void convert_to_static_type(const CustomDataType data_type, const Func &func)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      func(float());
      break;
    case CD_PROP_FLOAT2:
      func(float2());
      break;
    case CD_PROP_FLOAT3:
      func(float3());
      break;
    case CD_PROP_INT32:
      func(int());
      break;
    case CD_PROP_BOOL:
      func(bool());
      break;
    case CD_PROP_COLOR:
      func(Color4f());
      break;
    default:
      BLI_assert(false);
      break;
  }
}

/* Interpolate between three values. */
template<typename T> T mix3(const float3 &weights, const T &v0, const T &v1, const T &v2);

template<> inline bool mix3(const float3 &weights, const bool &v0, const bool &v1, const bool &v2)
{
  return (weights.x * v0 + weights.y * v1 + weights.z * v2) >= 0.5f;
}

template<> inline int mix3(const float3 &weights, const int &v0, const int &v1, const int &v2)
{
  return static_cast<int>(weights.x * v0 + weights.y * v1 + weights.z * v2);
}

template<>
inline float mix3(const float3 &weights, const float &v0, const float &v1, const float &v2)
{
  return weights.x * v0 + weights.y * v1 + weights.z * v2;
}

template<>
inline float2 mix3(const float3 &weights, const float2 &v0, const float2 &v1, const float2 &v2)
{
  return weights.x * v0 + weights.y * v1 + weights.z * v2;
}

template<>
inline float3 mix3(const float3 &weights, const float3 &v0, const float3 &v1, const float3 &v2)
{
  return weights.x * v0 + weights.y * v1 + weights.z * v2;
}

template<>
inline Color4f mix3(const float3 &weights, const Color4f &v0, const Color4f &v1, const Color4f &v2)
{
  Color4f result;
  interp_v4_v4v4v4(result, v0, v1, v2, weights);
  return result;
}

}  // namespace blender::attribute_math
