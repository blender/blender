/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/attribute.h"
#include "scene/attribute.h"
#include "scene/geometry.h"
#include "scene/scene.h"

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/tokens.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

void ApplyPrimvars(AttributeSet &attributes,
                   const ustring &name,
                   VtValue value,
                   AttributeElement elem,
                   AttributeStandard std)
{
  const void *data = HdGetValueData(value);
  size_t size = value.GetArraySize();

  const HdType valueType = HdGetValueTupleType(value).type;

  TypeDesc attrType = CCL_NS::TypeUnknown;
  switch (valueType) {
    case HdTypeFloat:
      attrType = CCL_NS::TypeFloat;
      size *= sizeof(float);
      break;
    case HdTypeFloatVec2:
      attrType = CCL_NS::TypeFloat2;
      size *= sizeof(float2);
      static_assert(sizeof(GfVec2f) == sizeof(float2));
      break;
    case HdTypeFloatVec3: {
      attrType = CCL_NS::TypeVector;
      size *= sizeof(float3);
      // The Cycles "float3" data type is padded to "float4", so need to convert the array
      const auto &valueData = value.Get<VtVec3fArray>();
      VtArray<float3> valueConverted;
      valueConverted.reserve(valueData.size());
      for (const GfVec3f &vec : valueData) {
        valueConverted.push_back(make_float3(vec[0], vec[1], vec[2]));
      }
      data = valueConverted.data();
      value = std::move(valueConverted);
      break;
    }
    case HdTypeFloatVec4:
      attrType = CCL_NS::TypeFloat4;
      size *= sizeof(float4);
      static_assert(sizeof(GfVec4f) == sizeof(float4));
      break;
    default:
      TF_WARN("Unsupported attribute type %d", static_cast<int>(valueType));
      return;
  }

  Attribute *const attr = attributes.add(name, attrType, elem);
  attr->std = std;

  assert(size == attr->buffer.size());
  std::memcpy(attr->data(), data, size);
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
