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

std::optional<pxr::SdfValueTypeName> convert_blender_type_to_usd(
    const eCustomDataType blender_type, bool use_color3f_type)
{
  switch (blender_type) {
    case CD_PROP_FLOAT:
      return pxr::SdfValueTypeNames->FloatArray;
    case CD_PROP_INT8:
      return pxr::SdfValueTypeNames->UCharArray;
    case CD_PROP_INT32:
      return pxr::SdfValueTypeNames->IntArray;
    case CD_PROP_FLOAT2:
      return pxr::SdfValueTypeNames->Float2Array;
    case CD_PROP_FLOAT3:
      return pxr::SdfValueTypeNames->Float3Array;
    case CD_PROP_STRING:
      return pxr::SdfValueTypeNames->StringArray;
    case CD_PROP_BOOL:
      return pxr::SdfValueTypeNames->BoolArray;
    case CD_PROP_COLOR:
    case CD_PROP_BYTE_COLOR:
      return use_color3f_type ? pxr::SdfValueTypeNames->Color3fArray :
                                pxr::SdfValueTypeNames->Color4fArray;
    case CD_PROP_QUATERNION:
      return pxr::SdfValueTypeNames->QuatfArray;
    default:
      return std::nullopt;
  }
}

std::optional<eCustomDataType> convert_usd_type_to_blender(const pxr::SdfValueTypeName usd_type)
{
  static const Map<pxr::SdfValueTypeName, eCustomDataType> type_map = []() {
    Map<pxr::SdfValueTypeName, eCustomDataType> map;
    map.add_new(pxr::SdfValueTypeNames->FloatArray, CD_PROP_FLOAT);
    map.add_new(pxr::SdfValueTypeNames->Double, CD_PROP_FLOAT);
    map.add_new(pxr::SdfValueTypeNames->UCharArray, CD_PROP_INT8);
    map.add_new(pxr::SdfValueTypeNames->IntArray, CD_PROP_INT32);
    map.add_new(pxr::SdfValueTypeNames->Float2Array, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord2dArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord2fArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord2hArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord3dArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord3fArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->TexCoord3hArray, CD_PROP_FLOAT2);
    map.add_new(pxr::SdfValueTypeNames->Float3Array, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Point3fArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Point3dArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Point3hArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Normal3fArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Normal3dArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Normal3hArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Vector3fArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Vector3hArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Vector3dArray, CD_PROP_FLOAT3);
    map.add_new(pxr::SdfValueTypeNames->Color3fArray, CD_PROP_COLOR);
    map.add_new(pxr::SdfValueTypeNames->Color3hArray, CD_PROP_COLOR);
    map.add_new(pxr::SdfValueTypeNames->Color3dArray, CD_PROP_COLOR);
    map.add_new(pxr::SdfValueTypeNames->Color4fArray, CD_PROP_COLOR);
    map.add_new(pxr::SdfValueTypeNames->Color4hArray, CD_PROP_COLOR);
    map.add_new(pxr::SdfValueTypeNames->Color4dArray, CD_PROP_COLOR);
    map.add_new(pxr::SdfValueTypeNames->BoolArray, CD_PROP_BOOL);
    map.add_new(pxr::SdfValueTypeNames->QuatfArray, CD_PROP_QUATERNION);
    map.add_new(pxr::SdfValueTypeNames->QuatdArray, CD_PROP_QUATERNION);
    map.add_new(pxr::SdfValueTypeNames->QuathArray, CD_PROP_QUATERNION);
    return map;
  }();

  const eCustomDataType *value = type_map.lookup_ptr(usd_type);
  if (value == nullptr) {
    return std::nullopt;
  }

  return *value;
}

void copy_primvar_to_blender_attribute(const pxr::UsdGeomPrimvar &primvar,
                                       const pxr::UsdTimeCode timecode,
                                       const eCustomDataType data_type,
                                       const bke::AttrDomain domain,
                                       const OffsetIndices<int> face_indices,
                                       bke::MutableAttributeAccessor attributes)
{
  const pxr::TfToken pv_name = pxr::UsdGeomPrimvar::StripPrimvarsName(primvar.GetPrimvarName());

  bke::GSpanAttributeWriter attribute = attributes.lookup_or_add_for_write_span(
      pv_name.GetText(), domain, data_type);

  switch (data_type) {
    case CD_PROP_FLOAT:
      copy_primvar_to_blender_buffer<float>(
          primvar, timecode, face_indices, attribute.span.typed<float>());
      break;
    case CD_PROP_INT8:
      copy_primvar_to_blender_buffer<uchar>(
          primvar, timecode, face_indices, attribute.span.typed<int8_t>());
      break;
    case CD_PROP_INT32:
      copy_primvar_to_blender_buffer<int32_t>(
          primvar, timecode, face_indices, attribute.span.typed<int>());
      break;
    case CD_PROP_FLOAT2:
      copy_primvar_to_blender_buffer<pxr::GfVec2f>(
          primvar, timecode, face_indices, attribute.span.typed<float2>());
      break;
    case CD_PROP_FLOAT3:
      copy_primvar_to_blender_buffer<pxr::GfVec3f>(
          primvar, timecode, face_indices, attribute.span.typed<float3>());
      break;
    case CD_PROP_COLOR: {
      const pxr::SdfValueTypeName pv_type = primvar.GetTypeName();
      if (ELEM(pv_type,
               pxr::SdfValueTypeNames->Color3fArray,
               pxr::SdfValueTypeNames->Color3hArray,
               pxr::SdfValueTypeNames->Color3dArray))
      {
        copy_primvar_to_blender_buffer<pxr::GfVec3f>(
            primvar, timecode, face_indices, attribute.span.typed<ColorGeometry4f>());
      }
      else {
        copy_primvar_to_blender_buffer<pxr::GfVec4f>(
            primvar, timecode, face_indices, attribute.span.typed<ColorGeometry4f>());
      }
    } break;
    case CD_PROP_BOOL:
      copy_primvar_to_blender_buffer<bool>(
          primvar, timecode, face_indices, attribute.span.typed<bool>());
      break;
    case CD_PROP_QUATERNION:
      copy_primvar_to_blender_buffer<pxr::GfQuatf>(
          primvar, timecode, face_indices, attribute.span.typed<math::Quaternion>());
      break;

    default:
      BLI_assert_unreachable();
  }

  attribute.finish();
}

void copy_blender_attribute_to_primvar(const GVArray &attribute,
                                       const eCustomDataType data_type,
                                       const pxr::UsdTimeCode timecode,
                                       const pxr::UsdGeomPrimvar &primvar,
                                       pxr::UsdUtilsSparseValueWriter &value_writer)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      copy_blender_buffer_to_primvar<float, float>(
          attribute.typed<float>(), timecode, primvar, value_writer);
      break;
    case CD_PROP_INT8:
      copy_blender_buffer_to_primvar<int8_t, uchar>(
          attribute.typed<int8_t>(), timecode, primvar, value_writer);
      break;
    case CD_PROP_INT32:
      copy_blender_buffer_to_primvar<int, int32_t>(
          attribute.typed<int>(), timecode, primvar, value_writer);
      break;
    case CD_PROP_FLOAT2:
      copy_blender_buffer_to_primvar<float2, pxr::GfVec2f>(
          attribute.typed<float2>(), timecode, primvar, value_writer);
      break;
    case CD_PROP_FLOAT3:
      copy_blender_buffer_to_primvar<float3, pxr::GfVec3f>(
          attribute.typed<float3>(), timecode, primvar, value_writer);
      break;
    case CD_PROP_BOOL:
      copy_blender_buffer_to_primvar<bool, bool>(
          attribute.typed<bool>(), timecode, primvar, value_writer);
      break;
    case CD_PROP_COLOR:
      if (primvar.GetTypeName() == pxr::SdfValueTypeNames->Color3fArray) {
        copy_blender_buffer_to_primvar<ColorGeometry4f, pxr::GfVec3f>(
            attribute.typed<ColorGeometry4f>(), timecode, primvar, value_writer);
      }
      else {
        copy_blender_buffer_to_primvar<ColorGeometry4f, pxr::GfVec4f>(
            attribute.typed<ColorGeometry4f>(), timecode, primvar, value_writer);
      }
      break;
    case CD_PROP_BYTE_COLOR:
      if (primvar.GetTypeName() == pxr::SdfValueTypeNames->Color3fArray) {
        copy_blender_buffer_to_primvar<ColorGeometry4b, pxr::GfVec3f>(
            attribute.typed<ColorGeometry4b>(), timecode, primvar, value_writer);
      }
      else {
        copy_blender_buffer_to_primvar<ColorGeometry4b, pxr::GfVec4f>(
            attribute.typed<ColorGeometry4b>(), timecode, primvar, value_writer);
      }
      break;
    case CD_PROP_QUATERNION:
      copy_blender_buffer_to_primvar<math::Quaternion, pxr::GfQuatf>(
          attribute.typed<math::Quaternion>(), timecode, primvar, value_writer);
      break;
    default:
      BLI_assert_unreachable();
  }
}

}  // namespace blender::io::usd
