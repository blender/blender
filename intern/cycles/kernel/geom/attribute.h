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

ccl_device_inline AttributeDescriptor find_attribute(KernelGlobals kg,
                                                     const int object,
                                                     const int prim,
                                                     const uint64_t id)
{
  if (object == OBJECT_NONE) {
    return attribute_not_found();
  }

  /* for SVM, find attribute by unique id */
  uint attr_offset = object_attribute_map_offset(kg, object);
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

  return desc;
}

ccl_device_inline AttributeDescriptor find_attribute(KernelGlobals kg,
                                                     const ccl_private ShaderData *sd,
                                                     const uint64_t id)
{
  return find_attribute(kg, sd->object, sd->prim, id);
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
ccl_device_inline T attribute_data_fetch_bytecolor(KernelGlobals /*kg*/, int /*offset*/)
{
  kernel_assert(false);
  return make_zero<T>();
}

ccl_device_template_spec float4 attribute_data_fetch_bytecolor(KernelGlobals kg, int offset)
{
  const float4 rec709 = color_srgb_to_linear_v4(
      color_uchar4_to_float4(kernel_data_fetch(attributes_uchar4, offset)));
  return make_float4(rec709_to_rgb(kg, make_float3(rec709)), rec709.w);
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
