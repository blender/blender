/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __BLENDER_ATTRIBUTE_CONVERT_H__
#define __BLENDER_ATTRIBUTE_CONVERT_H__

#include "util/array.h"
#include "util/color.h"
#include "util/param.h"
#include "util/types.h"

#include "BKE_attribute.hh"
#include "BLI_math_color.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"

CCL_NAMESPACE_BEGIN

template<typename BlenderT> struct AttributeConverter {
  using CyclesT = void;
};

template<> struct AttributeConverter<float> {
  using CyclesT = float;
  static constexpr auto type_desc = TypeFloat;
  static CyclesT convert(const float &value)
  {
    return value;
  }
};
template<> struct AttributeConverter<int> {
  using CyclesT = float;
  static constexpr auto type_desc = TypeFloat;
  static CyclesT convert(const int &value)
  {
    return float(value);
  }
};
template<> struct AttributeConverter<blender::float3> {
  using CyclesT = float3;
  static constexpr auto type_desc = TypeVector;
  static CyclesT convert(const blender::float3 &value)
  {
    return make_float3(value[0], value[1], value[2]);
  }
};
template<> struct AttributeConverter<blender::ColorGeometry4f> {
  using CyclesT = float4;
  static constexpr auto type_desc = TypeRGBA;
  static CyclesT convert(const blender::ColorGeometry4f &value)
  {
    return make_float4(value[0], value[1], value[2], value[3]);
  }
};
template<> struct AttributeConverter<blender::ColorGeometry4b> {
  using CyclesT = float4;
  static constexpr auto type_desc = TypeRGBA;
  static CyclesT convert(const blender::ColorGeometry4b &value)
  {
    return color_srgb_to_linear(make_float4(byte_to_float(value[0]),
                                            byte_to_float(value[1]),
                                            byte_to_float(value[2]),
                                            byte_to_float(value[3])));
  }
};
template<> struct AttributeConverter<bool> {
  using CyclesT = float;
  static constexpr auto type_desc = TypeFloat;
  static CyclesT convert(const bool &value)
  {
    return float(value);
  }
};
template<> struct AttributeConverter<int8_t> {
  using CyclesT = float;
  static constexpr auto type_desc = TypeFloat;
  static CyclesT convert(const int8_t &value)
  {
    return float(value);
  }
};
template<> struct AttributeConverter<blender::math::Quaternion> {
  using CyclesT = float4;
  static constexpr auto type_desc = TypeFloat4;
  static CyclesT convert(const blender::math::Quaternion &value)
  {
    return make_float4(value.w, value.x, value.y, value.z);
  }
};

CCL_NAMESPACE_END

#endif /* __BLENDER_ATTRIBUTE_CONVERT_H__ */
