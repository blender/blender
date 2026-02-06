/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"
#include "kernel/util/colorspace.h"

#include "util/color.h"

CCL_NAMESPACE_BEGIN

/* Attributes
 *
 * We support an arbitrary number of attributes on various mesh elements.
 * On vertices, triangles, curve keys, curves, meshes and volume grids.
 * Most of the code for attribute reading is in the primitive files.
 *
 * Lookup of attributes is different between OSL and SVM, as OSL is ustring
 * based while for SVM we use integer ids. */

ccl_device_inline AttributeDescriptor attribute_not_found()
{
  const AttributeDescriptor desc = {ATTR_ELEMENT_NONE, (NodeAttributeType)0, ATTR_STD_NOT_FOUND};
  return desc;
}

/* Find attribute based on ID */

ccl_device_inline uint object_attribute_map_offset(KernelGlobals kg, const int object)
{
  return kernel_data_fetch(objects, object).attribute_map_offset;
}

ccl_device_inline AttributeDescriptor find_attribute(const ccl_global AttributeMap *attributes_map,
                                                     uint attr_offset,
                                                     const int prim,
                                                     const uint64_t id)
{
  /* for SVM, find attribute by unique id */
  AttributeMap attr_map = attributes_map[attr_offset];

  while (attr_map.id != id) {
    if (UNLIKELY(attr_map.id == ATTR_STD_NONE)) {
      if (UNLIKELY(attr_map.element == 0)) {
        return attribute_not_found();
      }
      /* Chain jump to a different part of the table. */
      attr_offset = attr_map.offset;
    }
    else {
      attr_offset += ATTR_PRIM_TYPES;
    }
    attr_map = attributes_map[attr_offset];
  }

  AttributeDescriptor desc;
  desc.element = (AttributeElement)attr_map.element;

  if (prim == PRIM_NONE && desc.element != ATTR_ELEMENT_MESH &&
      desc.element != ATTR_ELEMENT_VOXEL && desc.element != ATTR_ELEMENT_OBJECT)
  {
    return attribute_not_found();
  }

  /* return result */
  desc.offset = (attr_map.element == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND :
                                                          attr_map.offset;
  desc.type = (NodeAttributeType)attr_map.type;

  return desc;
}

ccl_device_inline AttributeDescriptor find_attribute(KernelGlobals kg,
                                                     const int object,
                                                     const int prim,
                                                     const uint64_t id)
{
  if (object == OBJECT_NONE) {
    return attribute_not_found();
  }

  return find_attribute(
      &kernel_data_fetch(attributes_map, 0), object_attribute_map_offset(kg, object), prim, id);
}

ccl_device_inline AttributeDescriptor find_attribute(KernelGlobals kg,
                                                     const ccl_private ShaderData *sd,
                                                     const uint64_t id)
{
  return find_attribute(kg, sd->object, sd->prim, id);
}

/* Templated functions to read from the attribute data */
template<typename T>
ccl_device_inline T attribute_data_fetch(KernelGlobals kg, AttributeElement element, int offset);

ccl_device_template_spec float attribute_data_fetch(KernelGlobals kg,
                                                    AttributeElement /*element*/,
                                                    int offset)
{
  return kernel_data_fetch(attributes_float, offset);
}

ccl_device_template_spec float2 attribute_data_fetch(KernelGlobals kg,
                                                     AttributeElement /*element*/,
                                                     int offset)
{
  return kernel_data_fetch(attributes_float2, offset);
}

ccl_device_template_spec float3 attribute_data_fetch(KernelGlobals kg,
                                                     AttributeElement element,
                                                     int offset)
{
  if (element & ATTR_ELEMENT_IS_NORMAL) {
    const packed_normal normal = kernel_data_fetch(attributes_normal, offset);
    return normal.decode();
  }
  return kernel_data_fetch(attributes_float3, offset);
}

ccl_device_template_spec float4 attribute_data_fetch(KernelGlobals kg,
                                                     AttributeElement element,
                                                     int offset)
{
  if (element & ATTR_ELEMENT_IS_BYTE) {
    const float4 rec709 = color_srgb_to_linear_v4(
        color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, offset)));
    return make_float4(rec709_to_rgb(kg, make_float3(rec709)), rec709.w);
  }
  return kernel_data_fetch(attributes_float4, offset);
}

ccl_device_inline float3 attribute_data_fetch_normal(KernelGlobals kg, int offset)
{
  const packed_normal normal = kernel_data_fetch(attributes_normal, offset);
  return normal.decode();
}

ccl_device_inline void attribute_data_fetch_normals(KernelGlobals kg,
                                                    const int offset,
                                                    const int i0,
                                                    const int i1,
                                                    const int i2,
                                                    ccl_private float3 N[3])
{
#ifndef __KERNEL_GPU__
  float4 nx, ny, nz;
  const int4 packed_values = make_int4(kernel_data_fetch(attributes_normal, offset + i0).value,
                                       kernel_data_fetch(attributes_normal, offset + i1).value,
                                       kernel_data_fetch(attributes_normal, offset + i2).value,
                                       0);
  packed_normal_decode_simd(packed_values, nx, ny, nz);
  N[0] = make_float3(nx.x, ny.x, nz.x);
  N[1] = make_float3(nx.y, ny.y, nz.y);
  N[2] = make_float3(nx.z, ny.z, nz.z);
#else
  N[0] = attribute_data_fetch_normal(kg, offset + i0);
  N[1] = attribute_data_fetch_normal(kg, offset + i1);
  N[2] = attribute_data_fetch_normal(kg, offset + i2);
#endif
}

ccl_device_inline float3 attribute_data_interpolate_normals(KernelGlobals kg,
                                                            const int offset,
                                                            const int i0,
                                                            const int i1,
                                                            const int i2,
                                                            const float u,
                                                            const float v)
{
#ifndef __KERNEL_GPU__
  float4 nx, ny, nz;
  const int4 packed_values = make_int4(kernel_data_fetch(attributes_normal, offset + i0).value,
                                       kernel_data_fetch(attributes_normal, offset + i1).value,
                                       kernel_data_fetch(attributes_normal, offset + i2).value,
                                       0);
  packed_normal_decode_simd(packed_values, nx, ny, nz);

  const float4 weights = make_float4(1.0f - u - v, u, v, 0.0f);
  return make_float3(dot(nx, weights), dot(ny, weights), dot(nz, weights));
#else
  const float3 n0 = attribute_data_fetch_normal(kg, offset + i0);
  const float3 n1 = attribute_data_fetch_normal(kg, offset + i1);
  const float3 n2 = attribute_data_fetch_normal(kg, offset + i2);
  return (1.0f - u - v) * n0 + u * n1 + v * n2;
#endif
}

#ifdef __KERNEL_METAL__
template<typename U, typename V> using attribute_data_type_is_same = metal::is_same<U, V>;
#else
template<typename U, typename V> using attribute_data_type_is_same = std::is_same<U, V>;
#endif

template<typename T>
ccl_device_inline void attribute_data_fetch_3(KernelGlobals kg,
                                              const AttributeElement element,
                                              const int offset,
                                              const int i0,
                                              const int i1,
                                              const int i2,
                                              ccl_private T f[3])
{
  if constexpr (attribute_data_type_is_same<T, float3>::value) {
    if (element & ATTR_ELEMENT_IS_NORMAL) {
      attribute_data_fetch_normals(kg, offset, i0, i1, i2, f);
    }
    else {
      f[0] = kernel_data_fetch(attributes_float3, offset + i0);
      f[1] = kernel_data_fetch(attributes_float3, offset + i1);
      f[2] = kernel_data_fetch(attributes_float3, offset + i2);
    }
  }
  else {
    f[0] = attribute_data_fetch<T>(kg, element, offset + i0);
    f[1] = attribute_data_fetch<T>(kg, element, offset + i1);
    f[2] = attribute_data_fetch<T>(kg, element, offset + i2);
  }
}

ccl_device_template_spec Transform attribute_data_fetch(KernelGlobals kg,

                                                        AttributeElement /*element*/,
                                                        int offset)
{
  Transform tfm;

  tfm.x = kernel_data_fetch(attributes_float4, offset + 0);
  tfm.y = kernel_data_fetch(attributes_float4, offset + 1);
  tfm.z = kernel_data_fetch(attributes_float4, offset + 2);

  return tfm;
}

/* Transform matrix attribute on meshes */

ccl_device Transform primitive_attribute_matrix(KernelGlobals kg, const AttributeDescriptor desc)
{
  return attribute_data_fetch<Transform>(kg, desc.element, desc.offset);
}

CCL_NAMESPACE_END
