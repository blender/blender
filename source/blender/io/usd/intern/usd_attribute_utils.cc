/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_attribute_utils.hh"
#include "usd_hash_types.hh"

#include "BLI_map.hh"
#include "BLI_offset_indices.hh"
#include "BLI_sys_types.h"

#include "BKE_attribute.hh"

#include "DNA_customdata_types.h"

#include <pxr/usd/sdf/valueTypeName.h>

#include <optional>

namespace blender::io::usd {

std::optional<pxr::SdfValueTypeName> convert_blender_type_to_usd(const bke::AttrType blender_type,
                                                                 bool use_color3f_type)
{
  switch (blender_type) {
    case bke::AttrType::Float:
      return pxr::SdfValueTypeNames->FloatArray;
    case bke::AttrType::Int8:
      return pxr::SdfValueTypeNames->UCharArray;
    case bke::AttrType::Int32:
      return pxr::SdfValueTypeNames->IntArray;
    case bke::AttrType::Float2:
      return pxr::SdfValueTypeNames->Float2Array;
    case bke::AttrType::Float3:
      return pxr::SdfValueTypeNames->Float3Array;
    case bke::AttrType::String:
      return pxr::SdfValueTypeNames->StringArray;
    case bke::AttrType::Bool:
      return pxr::SdfValueTypeNames->BoolArray;
    case bke::AttrType::ColorFloat:
    case bke::AttrType::ColorByte:
      return use_color3f_type ? pxr::SdfValueTypeNames->Color3fArray :
                                pxr::SdfValueTypeNames->Color4fArray;
    case bke::AttrType::Quaternion:
      return pxr::SdfValueTypeNames->QuatfArray;
    default:
      return std::nullopt;
  }
}

std::optional<bke::AttrType> convert_usd_type_to_blender(const pxr::SdfValueTypeName usd_type)
{
  static const Map<pxr::SdfValueTypeName, bke::AttrType> type_map = []() {
    Map<pxr::SdfValueTypeName, bke::AttrType> map;
    map.add_new(pxr::SdfValueTypeNames->FloatArray, bke::AttrType::Float);
    map.add_new(pxr::SdfValueTypeNames->Double, bke::AttrType::Float);
    map.add_new(pxr::SdfValueTypeNames->UCharArray, bke::AttrType::Int8);
    map.add_new(pxr::SdfValueTypeNames->IntArray, bke::AttrType::Int32);
    map.add_new(pxr::SdfValueTypeNames->Float2Array, bke::AttrType::Float2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord2dArray, bke::AttrType::Float2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord2fArray, bke::AttrType::Float2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord2hArray, bke::AttrType::Float2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord3dArray, bke::AttrType::Float2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord3fArray, bke::AttrType::Float2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord3hArray, bke::AttrType::Float2);
    map.add_new(pxr::SdfValueTypeNames->Float3Array, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Point3fArray, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Point3dArray, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Point3hArray, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Normal3fArray, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Normal3dArray, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Normal3hArray, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Vector3fArray, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Vector3hArray, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Vector3dArray, bke::AttrType::Float3);
    map.add_new(pxr::SdfValueTypeNames->Color3fArray, bke::AttrType::ColorFloat);
    map.add_new(pxr::SdfValueTypeNames->Color3hArray, bke::AttrType::ColorFloat);
    map.add_new(pxr::SdfValueTypeNames->Color3dArray, bke::AttrType::ColorFloat);
    map.add_new(pxr::SdfValueTypeNames->Color4fArray, bke::AttrType::ColorFloat);
    map.add_new(pxr::SdfValueTypeNames->Color4hArray, bke::AttrType::ColorFloat);
    map.add_new(pxr::SdfValueTypeNames->Color4dArray, bke::AttrType::ColorFloat);
    map.add_new(pxr::SdfValueTypeNames->BoolArray, bke::AttrType::Bool);
    map.add_new(pxr::SdfValueTypeNames->QuatfArray, bke::AttrType::Quaternion);
    map.add_new(pxr::SdfValueTypeNames->QuatdArray, bke::AttrType::Quaternion);
    map.add_new(pxr::SdfValueTypeNames->QuathArray, bke::AttrType::Quaternion);
    return map;
  }();

  const bke::AttrType *value = type_map.lookup_ptr(usd_type);
  if (value == nullptr) {
    return std::nullopt;
  }

  return *value;
}

void copy_primvar_to_blender_attribute(const pxr::UsdGeomPrimvar &primvar,
                                       const pxr::UsdTimeCode time,
                                       const bke::AttrType data_type,
                                       const bke::AttrDomain domain,
                                       const OffsetIndices<int> face_indices,
                                       bke::MutableAttributeAccessor attributes)
{
  const pxr::TfToken pv_name = pxr::UsdGeomPrimvar::StripPrimvarsName(primvar.GetPrimvarName());

  bke::GSpanAttributeWriter attribute = attributes.lookup_or_add_for_write_span(
      pv_name.GetText(), domain, data_type);

  switch (data_type) {
    case bke::AttrType::Float:
      copy_primvar_to_blender_buffer<float>(
          primvar, time, face_indices, attribute.span.typed<float>());
      break;
    case bke::AttrType::Int8:
      copy_primvar_to_blender_buffer<uchar>(
          primvar, time, face_indices, attribute.span.typed<int8_t>());
      break;
    case bke::AttrType::Int32:
      copy_primvar_to_blender_buffer<int32_t>(
          primvar, time, face_indices, attribute.span.typed<int>());
      break;
    case bke::AttrType::Float2:
      copy_primvar_to_blender_buffer<pxr::GfVec2f>(
          primvar, time, face_indices, attribute.span.typed<float2>());
      break;
    case bke::AttrType::Float3:
      copy_primvar_to_blender_buffer<pxr::GfVec3f>(
          primvar, time, face_indices, attribute.span.typed<float3>());
      break;
    case bke::AttrType::ColorFloat: {
      const pxr::SdfValueTypeName pv_type = primvar.GetTypeName();
      if (ELEM(pv_type,
               pxr::SdfValueTypeNames->Color3fArray,
               pxr::SdfValueTypeNames->Color3hArray,
               pxr::SdfValueTypeNames->Color3dArray))
      {
        copy_primvar_to_blender_buffer<pxr::GfVec3f>(
            primvar, time, face_indices, attribute.span.typed<ColorGeometry4f>());
      }
      else {
        copy_primvar_to_blender_buffer<pxr::GfVec4f>(
            primvar, time, face_indices, attribute.span.typed<ColorGeometry4f>());
      }
    } break;
    case bke::AttrType::Bool:
      copy_primvar_to_blender_buffer<bool>(
          primvar, time, face_indices, attribute.span.typed<bool>());
      break;
    case bke::AttrType::Quaternion:
      copy_primvar_to_blender_buffer<pxr::GfQuatf>(
          primvar, time, face_indices, attribute.span.typed<math::Quaternion>());
      break;

    default:
      BLI_assert_unreachable();
  }

  attribute.finish();
}

void copy_blender_attribute_to_primvar(const GVArray &attribute,
                                       const bke::AttrType data_type,
                                       const pxr::UsdTimeCode time,
                                       const pxr::UsdGeomPrimvar &primvar,
                                       pxr::UsdUtilsSparseValueWriter &value_writer)
{
  switch (data_type) {
    case bke::AttrType::Float:
      copy_blender_buffer_to_primvar<float, float>(
          attribute.typed<float>(), time, primvar, value_writer);
      break;
    case bke::AttrType::Int8:
      copy_blender_buffer_to_primvar<int8_t, uchar>(
          attribute.typed<int8_t>(), time, primvar, value_writer);
      break;
    case bke::AttrType::Int32:
      copy_blender_buffer_to_primvar<int, int32_t>(
          attribute.typed<int>(), time, primvar, value_writer);
      break;
    case bke::AttrType::Float2:
      copy_blender_buffer_to_primvar<float2, pxr::GfVec2f>(
          attribute.typed<float2>(), time, primvar, value_writer);
      break;
    case bke::AttrType::Float3:
      copy_blender_buffer_to_primvar<float3, pxr::GfVec3f>(
          attribute.typed<float3>(), time, primvar, value_writer);
      break;
    case bke::AttrType::Bool:
      copy_blender_buffer_to_primvar<bool, bool>(
          attribute.typed<bool>(), time, primvar, value_writer);
      break;
    case bke::AttrType::ColorFloat:
      if (primvar.GetTypeName() == pxr::SdfValueTypeNames->Color3fArray) {
        copy_blender_buffer_to_primvar<ColorGeometry4f, pxr::GfVec3f>(
            attribute.typed<ColorGeometry4f>(), time, primvar, value_writer);
      }
      else {
        copy_blender_buffer_to_primvar<ColorGeometry4f, pxr::GfVec4f>(
            attribute.typed<ColorGeometry4f>(), time, primvar, value_writer);
      }
      break;
    case bke::AttrType::ColorByte:
      if (primvar.GetTypeName() == pxr::SdfValueTypeNames->Color3fArray) {
        copy_blender_buffer_to_primvar<ColorGeometry4b, pxr::GfVec3f>(
            attribute.typed<ColorGeometry4b>(), time, primvar, value_writer);
      }
      else {
        copy_blender_buffer_to_primvar<ColorGeometry4b, pxr::GfVec4f>(
            attribute.typed<ColorGeometry4b>(), time, primvar, value_writer);
      }
      break;
    case bke::AttrType::Quaternion:
      copy_blender_buffer_to_primvar<math::Quaternion, pxr::GfQuatf>(
          attribute.typed<math::Quaternion>(), time, primvar, value_writer);
      break;
    default:
      BLI_assert_unreachable();
  }
}

}  // namespace blender::io::usd
