/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "subd/interpolation.h"
#include "subd/osd.h"

#include "scene/attribute.h"
#include "scene/mesh.h"

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENSUBDIV

void OsdData::subdivide_attribute(Attribute &attr)
{
  const Far::PrimvarRefiner primvar_refiner(*refiner);

  if (attr.element == ATTR_ELEMENT_VERTEX) {
    const int num_refiner_verts = refiner->GetNumVerticesTotal();
    const int num_local_points = patch_table->GetNumLocalPoints();

    attr.resize(num_refiner_verts + num_local_points);
    attr.flags |= ATTR_FINAL_SIZE;

    char *src = attr.buffer.data();

    for (int i = 0; i < refiner->GetMaxLevel(); i++) {
      char *dest = src + refiner->GetLevel(i).GetNumVertices() * attr.data_sizeof();

      if (ccl::Attribute::same_storage(attr.type, TypeFloat)) {
        primvar_refiner.Interpolate(i + 1, (OsdValue<float> *)src, (OsdValue<float> *&)dest);
      }
      else if (ccl::Attribute::same_storage(attr.type, TypeFloat2)) {
        primvar_refiner.Interpolate(i + 1, (OsdValue<float2> *)src, (OsdValue<float2> *&)dest);
        // float3 is not interchangeable with float4 and so needs to be handled
        // separately
      }
      else if (ccl::Attribute::same_storage(attr.type, TypeFloat4)) {
        primvar_refiner.Interpolate(i + 1, (OsdValue<float4> *)src, (OsdValue<float4> *&)dest);
      }
      else {
        primvar_refiner.Interpolate(i + 1, (OsdValue<float3> *)src, (OsdValue<float3> *&)dest);
      }

      src = dest;
    }

    if (num_local_points) {
      if (ccl::Attribute::same_storage(attr.type, TypeFloat)) {
        patch_table->ComputeLocalPointValues(
            (OsdValue<float> *)attr.buffer.data(),
            (OsdValue<float> *)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
      }
      else if (ccl::Attribute::same_storage(attr.type, TypeFloat2)) {
        patch_table->ComputeLocalPointValues(
            (OsdValue<float2> *)attr.buffer.data(),
            (OsdValue<float2> *)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
      }
      else if (ccl::Attribute::same_storage(attr.type, TypeFloat4)) {
        // float3 is not interchangeable with float4 and so needs to be handled
        // separately
        patch_table->ComputeLocalPointValues(
            (OsdValue<float4> *)attr.buffer.data(),
            (OsdValue<float4> *)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
      }
      else {
        // float3 is not interchangeable with float4 and so needs to be handled
        // separately
        patch_table->ComputeLocalPointValues(
            (OsdValue<float3> *)attr.buffer.data(),
            (OsdValue<float3> *)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
      }
    }
  }
  else if (attr.element == ATTR_ELEMENT_CORNER || attr.element == ATTR_ELEMENT_CORNER_BYTE) {
    // TODO(mai): fvar interpolation
  }
}

#endif

CCL_NAMESPACE_END
