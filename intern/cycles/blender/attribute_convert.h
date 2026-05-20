/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "scene/attribute.h"

#include "util/color.h"
#include "util/param.h"
#include "util/types.h"

#include "BKE_attribute.hh"

#include "BLI_color_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"

CCL_NAMESPACE_BEGIN

template<typename BlenderT> struct AttributeConverter {
  using CyclesT = void;
};

template<> struct AttributeConverter<float> {
  using CyclesT = float;
  static constexpr auto type_desc = TypeFloat;
  static constexpr bool layout_compatible = true;
  static CyclesT convert(const float &value)
  {
    return value;
  }
};
template<> struct AttributeConverter<int> {
  using CyclesT = float;
  static constexpr auto type_desc = TypeFloat;
  static constexpr bool layout_compatible = false;
  static CyclesT convert(const int &value)
  {
    return float(value);
  }
};
template<> struct AttributeConverter<blender::float2> {
  using CyclesT = float2;
  static constexpr auto type_desc = TypeFloat2;
  static constexpr bool layout_compatible = true;
  static CyclesT convert(const blender::float2 &value)
  {
    return make_float2(value[0], value[1]);
  }
};
template<> struct AttributeConverter<blender::float3> {
  using CyclesT = packed_float3;
  static constexpr auto type_desc = TypeVector;
  static constexpr bool layout_compatible = true;
  static CyclesT convert(const blender::float3 &value)
  {
    return packed_float3(make_float3(value[0], value[1], value[2]));
  }
};
template<> struct AttributeConverter<blender::float4> {
  using CyclesT = float4;
  static constexpr auto type_desc = TypeFloat4;
  /* Allocation alignment is not compatible with Cycles */
  static constexpr bool layout_compatible = false;
  static CyclesT convert(const blender::float4 &value)
  {
    return make_float4(value[0], value[1], value[2], value[3]);
  }
};
template<> struct AttributeConverter<blender::ColorGeometry4f> {
  using CyclesT = float4;
  static constexpr auto type_desc = TypeRGBA;
  /* Allocation alignment is not compatible with Cycles */
  static constexpr bool layout_compatible = false;
  static CyclesT convert(const blender::ColorGeometry4f &value)
  {
    return make_float4(value[0], value[1], value[2], value[3]);
  }
};
template<> struct AttributeConverter<blender::ColorGeometry4b> {
  using CyclesT = float4;
  static constexpr auto type_desc = TypeRGBA;
  static constexpr bool layout_compatible = false;
  static CyclesT convert(const blender::ColorGeometry4b &value)
  {
    return color_srgb_to_linear_v4(make_float4(byte_to_float(value[0]),
                                               byte_to_float(value[1]),
                                               byte_to_float(value[2]),
                                               byte_to_float(value[3])));
  }
};
template<> struct AttributeConverter<bool> {
  using CyclesT = float;
  static constexpr auto type_desc = TypeFloat;
  static constexpr bool layout_compatible = false;
  static CyclesT convert(const bool &value)
  {
    return float(value);
  }
};
template<> struct AttributeConverter<int8_t> {
  using CyclesT = float;
  static constexpr auto type_desc = TypeFloat;
  static constexpr bool layout_compatible = false;
  static CyclesT convert(const int8_t &value)
  {
    return float(value);
  }
};
template<> struct AttributeConverter<blender::math::Quaternion> {
  using CyclesT = float4;
  static constexpr auto type_desc = TypeFloat4;
  /* Allocation alignment is not compatible with Cycles */
  static constexpr bool layout_compatible = false;
  static CyclesT convert(const blender::math::Quaternion &value)
  {
    return make_float4(value.w, value.x, value.y, value.z);
  }
};

/* Add a standard attribute from a Blender attribute reader, sharing the buffer
 * with Blender when possible. */
template<typename BlenderT>
bool sync_attribute_from_blender(AttributeSet &attributes,
                                 const AttributeStandard std,
                                 const blender::bke::AttributeReader<BlenderT> &b_reader,
                                 const int size)
{
  if (!b_reader) {
    return false;
  }
  using Converter = AttributeConverter<BlenderT>;
  using CyclesT = typename Converter::CyclesT;

  /* Try implicit sharing. */
  if constexpr (Converter::layout_compatible) {
    const blender::CommonVArrayInfo info = b_reader.varray.common_info();
    if (info.type == blender::CommonVArrayInfo::Type::Span && b_reader.sharing_info) {
      attributes.add_shared(std, ustring(), info.data, size, b_reader.sharing_info);
      return true;
    }
  }

  /* Otherwise allocate and copy. */
  Attribute *attr = attributes.add(std);
  CyclesT *data = attr->data_for_write<CyclesT>();
  const blender::VArraySpan<BlenderT> src = *b_reader;
  for (const int i : src.index_range()) {
    data[i] = Converter::convert(src[i]);
  }
  return true;
}

/* Same as sync_attribute_from_blender, but for a single motion step of an
 * existing attribute. */
template<typename BlenderT>
bool sync_attribute_motion_step_from_blender(
    Attribute &attr,
    const int motion_step,
    const blender::bke::AttributeReader<BlenderT> &b_reader)
{
  if (!b_reader) {
    return false;
  }
  using Converter = AttributeConverter<BlenderT>;
  using CyclesT = typename Converter::CyclesT;

  /* Try implicit sharing. */
  if constexpr (Converter::layout_compatible) {
    const blender::CommonVArrayInfo info = b_reader.varray.common_info();
    if (info.type == blender::CommonVArrayInfo::Type::Span && b_reader.sharing_info) {
      attr.set_motion_step_shared(
          motion_step, info.data, b_reader.varray.size(), b_reader.sharing_info);
      return true;
    }
  }

  /* Otherwise allocate and copy. */
  CyclesT *data = attr.data_for_write<CyclesT>(motion_step);
  const blender::VArraySpan<BlenderT> src = *b_reader;
  for (const int i : src.index_range()) {
    data[i] = Converter::convert(src[i]);
  }
  return true;
}

CCL_NAMESPACE_END
