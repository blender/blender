/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

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

ccl_device_inline uint subd_triangle_patch(KernelGlobals kg, ccl_private const ShaderData *sd)
{
  return (sd->prim != PRIM_NONE) ? kernel_tex_fetch(__tri_patch, sd->prim) : ~0;
}

ccl_device_inline uint attribute_primitive_type(KernelGlobals kg, ccl_private const ShaderData *sd)
{
  if ((sd->type & PRIMITIVE_TRIANGLE) && subd_triangle_patch(kg, sd) != ~0) {
    return ATTR_PRIM_SUBD;
  }
  else {
    return ATTR_PRIM_GEOMETRY;
  }
}

ccl_device_inline AttributeDescriptor attribute_not_found()
{
  const AttributeDescriptor desc = {
      ATTR_ELEMENT_NONE, (NodeAttributeType)0, 0, ATTR_STD_NOT_FOUND};
  return desc;
}

/* Find attribute based on ID */

ccl_device_inline uint object_attribute_map_offset(KernelGlobals kg, int object)
{
  return kernel_tex_fetch(__objects, object).attribute_map_offset;
}

ccl_device_inline AttributeDescriptor find_attribute(KernelGlobals kg,
                                                     ccl_private const ShaderData *sd,
                                                     uint id)
{
  if (sd->object == OBJECT_NONE) {
    return attribute_not_found();
  }

  /* for SVM, find attribute by unique id */
  uint attr_offset = object_attribute_map_offset(kg, sd->object);
  attr_offset += attribute_primitive_type(kg, sd);
  AttributeMap attr_map = kernel_tex_fetch(__attributes_map, attr_offset);

  while (attr_map.id != id) {
    if (UNLIKELY(attr_map.id == ATTR_STD_NONE)) {
      if (UNLIKELY(attr_map.element == 0)) {
        return attribute_not_found();
      }
      else {
        /* Chain jump to a different part of the table. */
        attr_offset = attr_map.offset;
      }
    }
    else {
      attr_offset += ATTR_PRIM_TYPES;
    }
    attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
  }

  AttributeDescriptor desc;
  desc.element = (AttributeElement)attr_map.element;

  if (sd->prim == PRIM_NONE && desc.element != ATTR_ELEMENT_MESH &&
      desc.element != ATTR_ELEMENT_VOXEL && desc.element != ATTR_ELEMENT_OBJECT) {
    return attribute_not_found();
  }

  /* return result */
  desc.offset = (attr_map.element == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND :
                                                          (int)attr_map.offset;
  desc.type = (NodeAttributeType)attr_map.type;
  desc.flags = (AttributeFlag)attr_map.flags;

  return desc;
}

/* Transform matrix attribute on meshes */

ccl_device Transform primitive_attribute_matrix(KernelGlobals kg,
                                                ccl_private const ShaderData *sd,
                                                const AttributeDescriptor desc)
{
  Transform tfm;

  tfm.x = kernel_tex_fetch(__attributes_float4, desc.offset + 0);
  tfm.y = kernel_tex_fetch(__attributes_float4, desc.offset + 1);
  tfm.z = kernel_tex_fetch(__attributes_float4, desc.offset + 2);

  return tfm;
}

CCL_NAMESPACE_END
