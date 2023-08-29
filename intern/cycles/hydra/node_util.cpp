/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/node_util.h"
#include "util/transform.h"

#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/matrix3f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/assetPath.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

namespace {

template<typename DstType> DstType convertToCycles(const VtValue &value)
{
  if (value.IsHolding<DstType>()) {
    return value.UncheckedGet<DstType>();
  }

  VtValue castedValue = VtValue::Cast<DstType>(value);
  if (castedValue.IsHolding<DstType>()) {
    return castedValue.UncheckedGet<DstType>();
  }

  TF_WARN("Could not convert VtValue to Cycles type");
  return DstType(0);
}

template<> float2 convertToCycles<float2>(const VtValue &value)
{
  const GfVec2f convertedValue = convertToCycles<GfVec2f>(value);
  return make_float2(convertedValue[0], convertedValue[1]);
}

template<> float3 convertToCycles<float3>(const VtValue &value)
{
  if (value.IsHolding<GfVec3f>()) {
    const GfVec3f convertedValue = value.UncheckedGet<GfVec3f>();
    return make_float3(convertedValue[0], convertedValue[1], convertedValue[2]);
  }
  if (value.IsHolding<GfVec4f>()) {
    const GfVec4f convertedValue = value.UncheckedGet<GfVec4f>();
    return make_float3(convertedValue[0], convertedValue[1], convertedValue[2]);
  }

  if (value.CanCast<GfVec3f>()) {
    const GfVec3f convertedValue = VtValue::Cast<GfVec3f>(value).UncheckedGet<GfVec3f>();
    return make_float3(convertedValue[0], convertedValue[1], convertedValue[2]);
  }
  if (value.CanCast<GfVec4f>()) {
    const GfVec4f convertedValue = VtValue::Cast<GfVec4f>(value).UncheckedGet<GfVec4f>();
    return make_float3(convertedValue[0], convertedValue[1], convertedValue[2]);
  }

  TF_WARN("Could not convert VtValue to float3");
  return zero_float3();
}

template<> ustring convertToCycles<ustring>(const VtValue &value)
{
  if (value.IsHolding<TfToken>()) {
    return ustring(value.UncheckedGet<TfToken>().GetString());
  }
  if (value.IsHolding<std::string>()) {
    return ustring(value.UncheckedGet<std::string>());
  }
  if (value.IsHolding<SdfAssetPath>()) {
    const SdfAssetPath &path = value.UncheckedGet<SdfAssetPath>();
    return ustring(path.GetResolvedPath());
  }

  if (value.CanCast<TfToken>()) {
    return convertToCycles<ustring>(VtValue::Cast<TfToken>(value));
  }
  if (value.CanCast<std::string>()) {
    return convertToCycles<ustring>(VtValue::Cast<std::string>(value));
  }
  if (value.CanCast<SdfAssetPath>()) {
    return convertToCycles<ustring>(VtValue::Cast<SdfAssetPath>(value));
  }

  TF_WARN("Could not convert VtValue to ustring");
  return ustring();
}

template<typename Matrix>
Transform convertMatrixToCycles(
    const typename std::enable_if<Matrix::numRows == 3 && Matrix::numColumns == 3, Matrix>::type
        &matrix)
{
  return make_transform(matrix[0][0],
                        matrix[1][0],
                        matrix[2][0],
                        0,
                        matrix[0][1],
                        matrix[1][1],
                        matrix[2][1],
                        0,
                        matrix[0][2],
                        matrix[1][2],
                        matrix[2][2],
                        0);
}

template<typename Matrix>
Transform convertMatrixToCycles(
    const typename std::enable_if<Matrix::numRows == 4 && Matrix::numColumns == 4, Matrix>::type
        &matrix)
{
  return make_transform(matrix[0][0],
                        matrix[1][0],
                        matrix[2][0],
                        matrix[3][0],
                        matrix[0][1],
                        matrix[1][1],
                        matrix[2][1],
                        matrix[3][1],
                        matrix[0][2],
                        matrix[1][2],
                        matrix[2][2],
                        matrix[3][2]);
}

template<> Transform convertToCycles<Transform>(const VtValue &value)
{
  if (value.IsHolding<GfMatrix4f>()) {
    return convertMatrixToCycles<GfMatrix4f>(value.UncheckedGet<GfMatrix4f>());
  }
  if (value.IsHolding<GfMatrix3f>()) {
    return convertMatrixToCycles<GfMatrix3f>(value.UncheckedGet<GfMatrix3f>());
  }
  if (value.IsHolding<GfMatrix4d>()) {
    return convertMatrixToCycles<GfMatrix4d>(value.UncheckedGet<GfMatrix4d>());
  }
  if (value.IsHolding<GfMatrix3d>()) {
    return convertMatrixToCycles<GfMatrix3d>(value.UncheckedGet<GfMatrix3d>());
  }

  if (value.CanCast<GfMatrix4f>()) {
    return convertToCycles<Transform>(VtValue::Cast<GfMatrix4f>(value));
  }
  if (value.CanCast<GfMatrix3f>()) {
    return convertToCycles<Transform>(VtValue::Cast<GfMatrix3f>(value));
  }
  if (value.CanCast<GfMatrix4d>()) {
    return convertToCycles<Transform>(VtValue::Cast<GfMatrix4d>(value));
  }
  if (value.CanCast<GfMatrix3d>()) {
    return convertToCycles<Transform>(VtValue::Cast<GfMatrix3d>(value));
  }

  TF_WARN("Could not convert VtValue to Transform");
  return transform_identity();
}

template<typename DstType, typename SrcType = DstType>
array<DstType> convertToCyclesArray(const VtValue &value)
{
  static_assert(sizeof(DstType) == sizeof(SrcType),
                "Size mismatch between VtArray and array base type");

  using SrcArray = VtArray<SrcType>;

  if (value.IsHolding<SrcArray>()) {
    const auto &valueData = value.UncheckedGet<SrcArray>();
    array<DstType> cyclesArray;
    cyclesArray.resize(valueData.size());
    std::memcpy(cyclesArray.data(), valueData.data(), valueData.size() * sizeof(DstType));
    return cyclesArray;
  }

  if (value.CanCast<SrcArray>()) {
    VtValue castedValue = VtValue::Cast<SrcArray>(value);
    const auto &valueData = castedValue.UncheckedGet<SrcArray>();
    array<DstType> cyclesArray;
    cyclesArray.resize(valueData.size());
    std::memcpy(cyclesArray.data(), valueData.data(), valueData.size() * sizeof(DstType));
    return cyclesArray;
  }

  return array<DstType>();
}

template<> array<float3> convertToCyclesArray<float3, GfVec3f>(const VtValue &value)
{
  if (value.IsHolding<VtVec3fArray>()) {
    const auto &valueData = value.UncheckedGet<VtVec3fArray>();
    array<float3> cyclesArray;
    cyclesArray.reserve(valueData.size());
    for (const GfVec3f &vec : valueData) {
      cyclesArray.push_back_reserved(make_float3(vec[0], vec[1], vec[2]));
    }
    return cyclesArray;
  }
  if (value.IsHolding<VtVec4fArray>()) {
    const auto &valueData = value.UncheckedGet<VtVec4fArray>();
    array<float3> cyclesArray;
    cyclesArray.reserve(valueData.size());
    for (const GfVec4f &vec : valueData) {
      cyclesArray.push_back_reserved(make_float3(vec[0], vec[1], vec[2]));
    }
    return cyclesArray;
  }

  if (value.CanCast<VtVec3fArray>()) {
    return convertToCyclesArray<float3, GfVec3f>(VtValue::Cast<VtVec3fArray>(value));
  }
  if (value.CanCast<VtVec4fArray>()) {
    return convertToCyclesArray<float3, GfVec3f>(VtValue::Cast<VtVec4fArray>(value));
  }

  return array<float3>();
}

template<> array<ustring> convertToCyclesArray<ustring, void>(const VtValue &value)
{
  using SdfPathArray = VtArray<SdfAssetPath>;

  if (value.IsHolding<VtStringArray>()) {
    const auto &valueData = value.UncheckedGet<VtStringArray>();
    array<ustring> cyclesArray;
    cyclesArray.reserve(valueData.size());
    for (const auto &element : valueData) {
      cyclesArray.push_back_reserved(ustring(element));
    }
    return cyclesArray;
  }
  if (value.IsHolding<VtTokenArray>()) {
    const auto &valueData = value.UncheckedGet<VtTokenArray>();
    array<ustring> cyclesArray;
    cyclesArray.reserve(valueData.size());
    for (const auto &element : valueData) {
      cyclesArray.push_back_reserved(ustring(element.GetString()));
    }
    return cyclesArray;
  }
  if (value.IsHolding<SdfPathArray>()) {
    const auto &valueData = value.UncheckedGet<SdfPathArray>();
    array<ustring> cyclesArray;
    cyclesArray.reserve(valueData.size());
    for (const auto &element : valueData) {
      cyclesArray.push_back_reserved(ustring(element.GetResolvedPath()));
    }
    return cyclesArray;
  }

  if (value.CanCast<VtStringArray>()) {
    return convertToCyclesArray<ustring, void>(VtValue::Cast<VtStringArray>(value));
  }
  if (value.CanCast<VtTokenArray>()) {
    return convertToCyclesArray<ustring, void>(VtValue::Cast<VtTokenArray>(value));
  }
  if (value.CanCast<SdfPathArray>()) {
    return convertToCyclesArray<ustring, void>(VtValue::Cast<SdfPathArray>(value));
  }

  TF_WARN("Could not convert VtValue to array<ustring>");
  return array<ustring>();
}

template<typename MatrixArray> array<Transform> convertToCyclesTransformArray(const VtValue &value)
{
  assert(value.IsHolding<MatrixArray>());

  const auto &valueData = value.UncheckedGet<MatrixArray>();
  array<Transform> cyclesArray;
  cyclesArray.reserve(valueData.size());
  for (const auto &element : valueData) {
    cyclesArray.push_back_reserved(
        convertMatrixToCycles<typename MatrixArray::value_type>(element));
  }
  return cyclesArray;
}

template<> array<Transform> convertToCyclesArray<Transform, void>(const VtValue &value)
{
  if (value.IsHolding<VtMatrix4fArray>()) {
    return convertToCyclesTransformArray<VtMatrix4fArray>(value);
  }
  if (value.IsHolding<VtMatrix3fArray>()) {
    return convertToCyclesTransformArray<VtMatrix3fArray>(value);
  }
  if (value.IsHolding<VtMatrix4dArray>()) {
    return convertToCyclesTransformArray<VtMatrix4dArray>(value);
  }
  if (value.IsHolding<VtMatrix3dArray>()) {
    return convertToCyclesTransformArray<VtMatrix3dArray>(value);
  }

  if (value.CanCast<VtMatrix4fArray>()) {
    return convertToCyclesTransformArray<VtMatrix4fArray>(VtValue::Cast<VtMatrix4fArray>(value));
  }
  if (value.CanCast<VtMatrix3fArray>()) {
    return convertToCyclesTransformArray<VtMatrix3fArray>(VtValue::Cast<VtMatrix3fArray>(value));
  }
  if (value.CanCast<VtMatrix4dArray>()) {
    return convertToCyclesTransformArray<VtMatrix4dArray>(VtValue::Cast<VtMatrix4dArray>(value));
  }
  if (value.CanCast<VtMatrix3dArray>()) {
    return convertToCyclesTransformArray<VtMatrix3dArray>(VtValue::Cast<VtMatrix3dArray>(value));
  }

  TF_WARN("Could not convert VtValue to array<Transform>");
  return array<Transform>();
}

template<typename SrcType> VtValue convertFromCycles(const SrcType &value)
{
  return VtValue(value);
}

template<> VtValue convertFromCycles<float2>(const float2 &value)
{
  const GfVec2f convertedValue(value.x, value.y);
  return VtValue(convertedValue);
}

template<> VtValue convertFromCycles<float3>(const float3 &value)
{
  const GfVec3f convertedValue(value.x, value.y, value.z);
  return VtValue(convertedValue);
}

template<> VtValue convertFromCycles<ustring>(const ustring &value)
{
  return VtValue(value.string());
}

GfMatrix4f convertMatrixFromCycles(const Transform &matrix)
{
  return GfMatrix4f(matrix[0][0],
                    matrix[1][0],
                    matrix[2][0],
                    0.0f,
                    matrix[0][1],
                    matrix[1][1],
                    matrix[2][1],
                    0.0f,
                    matrix[0][2],
                    matrix[1][2],
                    matrix[2][2],
                    0.0f,
                    0.0f,
                    0.0f,
                    0.0f,
                    1.0f);
}

template<> VtValue convertFromCycles<Transform>(const Transform &value)
{
  return VtValue(convertMatrixFromCycles(value));
}

template<typename SrcType, typename DstType = SrcType>
VtValue convertFromCyclesArray(const array<SrcType> &value)
{
  static_assert(sizeof(DstType) == sizeof(SrcType),
                "Size mismatch between VtArray and array base type");

  VtArray<DstType> convertedValue;
  convertedValue.resize(value.size());
  std::memcpy(convertedValue.data(), value.data(), value.size() * sizeof(SrcType));
  return VtValue(convertedValue);
}

template<> VtValue convertFromCyclesArray<float2, GfVec2f>(const array<float2> &value)
{
  VtVec2fArray convertedValue;
  convertedValue.reserve(value.size());
  for (const auto &element : value) {
    convertedValue.push_back(GfVec2f(element.x, element.y));
  }
  return VtValue(convertedValue);
}

template<> VtValue convertFromCyclesArray<float3, GfVec3f>(const array<float3> &value)
{
  VtVec3fArray convertedValue;
  convertedValue.reserve(value.size());
  for (const auto &element : value) {
    convertedValue.push_back(GfVec3f(element.x, element.y, element.z));
  }
  return VtValue(convertedValue);
}

template<> VtValue convertFromCyclesArray<ustring, void>(const array<ustring> &value)
{
  VtStringArray convertedValue;
  convertedValue.reserve(value.size());
  for (const auto &element : value) {
    convertedValue.push_back(element.string());
  }
  return VtValue(convertedValue);
}

template<> VtValue convertFromCyclesArray<Transform, void>(const array<Transform> &value)
{
  VtMatrix4fArray convertedValue;
  convertedValue.reserve(value.size());
  for (const auto &element : value) {
    convertedValue.push_back(convertMatrixFromCycles(element));
  }
  return VtValue(convertedValue);
}

}  // namespace

void SetNodeValue(Node *node, const SocketType &socket, const VtValue &value)
{
  switch (socket.type) {
    default:
    case SocketType::UNDEFINED:
      TF_RUNTIME_ERROR("Unexpected conversion: SocketType::UNDEFINED");
      break;

    case SocketType::BOOLEAN:
      node->set(socket, convertToCycles<bool>(value));
      break;
    case SocketType::FLOAT:
      node->set(socket, convertToCycles<float>(value));
      break;
    case SocketType::INT:
      node->set(socket, convertToCycles<int>(value));
      break;
    case SocketType::UINT:
      node->set(socket, convertToCycles<unsigned int>(value));
      break;
    case SocketType::COLOR:
    case SocketType::VECTOR:
    case SocketType::POINT:
    case SocketType::NORMAL:
      node->set(socket, convertToCycles<float3>(value));
      break;
    case SocketType::POINT2:
      node->set(socket, convertToCycles<float2>(value));
      break;
    case SocketType::CLOSURE:
      // Handled by node connections
      break;
    case SocketType::STRING:
      node->set(socket, convertToCycles<ustring>(value));
      break;
    case SocketType::ENUM:
      // Enum's can accept a string or an int
      if (value.IsHolding<TfToken>() || value.IsHolding<std::string>()) {
        node->set(socket, convertToCycles<ustring>(value));
      }
      else {
        node->set(socket, convertToCycles<int>(value));
      }
      break;
    case SocketType::TRANSFORM:
      node->set(socket, convertToCycles<Transform>(value));
      break;
    case SocketType::NODE:
      // TODO: renderIndex->GetRprim()->cycles_node ?
      TF_WARN("Unimplemented conversion: SocketType::NODE");
      break;

    case SocketType::BOOLEAN_ARRAY: {
      auto cyclesArray = convertToCyclesArray<bool>(value);
      node->set(socket, cyclesArray);
      break;
    }
    case SocketType::FLOAT_ARRAY: {
      auto cyclesArray = convertToCyclesArray<float>(value);
      node->set(socket, cyclesArray);
      break;
    }
    case SocketType::INT_ARRAY: {
      auto cyclesArray = convertToCyclesArray<int>(value);
      node->set(socket, cyclesArray);
      break;
    }
    case SocketType::COLOR_ARRAY:
    case SocketType::VECTOR_ARRAY:
    case SocketType::POINT_ARRAY:
    case SocketType::NORMAL_ARRAY: {
      auto cyclesArray = convertToCyclesArray<float3, GfVec3f>(value);
      node->set(socket, cyclesArray);
      break;
    }
    case SocketType::POINT2_ARRAY: {
      auto cyclesArray = convertToCyclesArray<float2, GfVec2f>(value);
      node->set(socket, cyclesArray);
      break;
    }
    case SocketType::STRING_ARRAY: {
      auto cyclesArray = convertToCyclesArray<ustring, void>(value);
      node->set(socket, cyclesArray);
      break;
    }
    case SocketType::TRANSFORM_ARRAY: {
      auto cyclesArray = convertToCyclesArray<Transform, void>(value);
      node->set(socket, cyclesArray);
      break;
    }
    case SocketType::NODE_ARRAY: {
      // TODO: renderIndex->GetRprim()->cycles_node ?
      TF_WARN("Unimplemented conversion: SocketType::NODE_ARRAY");
      break;
    }
  }
}

VtValue GetNodeValue(const Node *node, const SocketType &socket)
{
  switch (socket.type) {
    default:
    case SocketType::UNDEFINED:
      TF_RUNTIME_ERROR("Unexpected conversion: SocketType::UNDEFINED");
      return VtValue();

    case SocketType::BOOLEAN:
      return convertFromCycles(node->get_bool(socket));
    case SocketType::FLOAT:
      return convertFromCycles(node->get_float(socket));
    case SocketType::INT:
      return convertFromCycles(node->get_int(socket));
    case SocketType::UINT:
      return convertFromCycles(node->get_uint(socket));
    case SocketType::COLOR:
    case SocketType::VECTOR:
    case SocketType::POINT:
    case SocketType::NORMAL:
      return convertFromCycles(node->get_float3(socket));
    case SocketType::POINT2:
      return convertFromCycles(node->get_float2(socket));
    case SocketType::CLOSURE:
      return VtValue();
    case SocketType::STRING:
      return convertFromCycles(node->get_string(socket));
    case SocketType::ENUM:
      return convertFromCycles(node->get_int(socket));
    case SocketType::TRANSFORM:
      return convertFromCycles(node->get_transform(socket));
    case SocketType::NODE:
      TF_WARN("Unimplemented conversion: SocketType::NODE");
      return VtValue();

    case SocketType::BOOLEAN_ARRAY:
      return convertFromCyclesArray(node->get_bool_array(socket));
    case SocketType::FLOAT_ARRAY:
      return convertFromCyclesArray(node->get_float_array(socket));
    case SocketType::INT_ARRAY:
      return convertFromCyclesArray(node->get_int_array(socket));
    case SocketType::COLOR_ARRAY:
    case SocketType::VECTOR_ARRAY:
    case SocketType::POINT_ARRAY:
    case SocketType::NORMAL_ARRAY:
      return convertFromCyclesArray<float3, GfVec3f>(node->get_float3_array(socket));
    case SocketType::POINT2_ARRAY:
      return convertFromCyclesArray<float2, GfVec2f>(node->get_float2_array(socket));
    case SocketType::STRING_ARRAY:
      return convertFromCyclesArray<ustring, void>(node->get_string_array(socket));
    case SocketType::TRANSFORM_ARRAY:
      return convertFromCyclesArray<Transform, void>(node->get_transform_array(socket));
    case SocketType::NODE_ARRAY: {
      TF_WARN("Unimplemented conversion: SocketType::NODE_ARRAY");
      return VtValue();
    }
  }
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
