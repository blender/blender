/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"

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

/* Patch index for triangle, -1 if not subdivision triangle */

ccl_device_inline uint subd_triangle_patch(KernelGlobals kg, const int prim)
{
  return (prim != PRIM_NONE) ? kernel_data_fetch(tri_patch, prim) : ~0;
}

ccl_device_inline uint attribute_primitive_type(KernelGlobals kg, const int prim, const int type)
{
  if ((type & PRIMITIVE_TRIANGLE) && subd_triangle_patch(kg, prim) != ~0) {
    return ATTR_PRIM_SUBD;
  }
  return ATTR_PRIM_GEOMETRY;
}

ccl_device_inline AttributeDescriptor attribute_not_found()
{
  const AttributeDescriptor desc = {
      ATTR_ELEMENT_NONE, (NodeAttributeType)0, 0, ATTR_STD_NOT_FOUND};
  return desc;
}

/* Find attribute based on ID */

ccl_device_inline uint object_attribute_map_offset(KernelGlobals kg, const int object)
{
  return kernel_data_fetch(objects, object).attribute_map_offset;
}

ccl_device_inline AttributeDescriptor find_attribute(
    KernelGlobals kg, const int object, const int prim, const int type, const uint64_t id)
{
  if (object == OBJECT_NONE) {
    return attribute_not_found();
  }

  /* for SVM, find attribute by unique id */
  uint attr_offset = object_attribute_map_offset(kg, object);
  attr_offset += attribute_primitive_type(kg, prim, type);
  AttributeMap attr_map = kernel_data_fetch(attributes_map, attr_offset);

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
    attr_map = kernel_data_fetch(attributes_map, attr_offset);
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
  desc.flags = (AttributeFlag)attr_map.flags;

  return desc;
}

ccl_device_inline AttributeDescriptor find_attribute(KernelGlobals kg,
                                                     const ccl_private ShaderData *sd,
                                                     const uint64_t id)
{
  return find_attribute(kg, sd->object, sd->prim, sd->type, id);
}

/* Templated functions to read from the attribute data */
template<typename T> ccl_device_inline T attribute_data_fetch(KernelGlobals kg, int offset);

ccl_device_template_spec float attribute_data_fetch(KernelGlobals kg, int offset)
{
  return kernel_data_fetch(attributes_float, offset);
}

ccl_device_template_spec float2 attribute_data_fetch(KernelGlobals kg, int offset)
{
  return kernel_data_fetch(attributes_float2, offset);
}

ccl_device_template_spec float3 attribute_data_fetch(KernelGlobals kg, int offset)
{
  return kernel_data_fetch(attributes_float3, offset);
}

ccl_device_template_spec float4 attribute_data_fetch(KernelGlobals kg, int offset)
{
  return kernel_data_fetch(attributes_float4, offset);
}

ccl_device_template_spec uchar4 attribute_data_fetch(KernelGlobals kg, int offset)
{
  return kernel_data_fetch(attributes_uchar4, offset);
}

/* ATTR_ELEMENT_CORNER_BYTE is stored as uchar4, but has to be converted to float4.
 * We don't support it for float/float2/float3. */
template<typename T>
ccl_device_inline T attribute_data_fetch_bytecolor(KernelGlobals kg, int offset)
{
  kernel_assert(false);
  return make_zero<T>();
}

ccl_device_template_spec float4 attribute_data_fetch_bytecolor(KernelGlobals kg, int offset)
{
  return color_srgb_to_linear_v4(
      color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, offset)));
}

ccl_device_template_spec Transform attribute_data_fetch(KernelGlobals kg, int offset)
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
  return attribute_data_fetch<Transform>(kg, desc.offset);
}

CCL_NAMESPACE_END
