/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Volume Primitive
 *
 * Volumes are just regions inside meshes with the mesh surface as boundaries.
 * There isn't as much data to access as for surfaces, there is only a position
 * to do lookups in 3D voxel or procedural textures.
 *
 * 3D voxel textures can be assigned as attributes per mesh, which means the
 * same shader can be used for volume objects with different densities, etc. */

#pragma once

#include "kernel/globals.h"
#include "kernel/image.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/object.h"

CCL_NAMESPACE_BEGIN

#ifdef __VOLUME__

/* Return position normalized to 0..1 in mesh bounds */

ccl_device_inline float3 volume_normalized_position(KernelGlobals kg,
                                                    const ccl_private ShaderData *sd,
                                                    float3 P)
{
  /* todo: optimize this so it's just a single matrix multiplication when
   * possible (not motion blur), or perhaps even just translation + scale */
  const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_GENERATED_TRANSFORM);

  object_inverse_position_transform(kg, sd, &P);

  if (desc.offset != ATTR_STD_NOT_FOUND) {
    const Transform tfm = primitive_attribute_matrix(kg, desc);
    P = transform_point(&tfm, P);
  }

  return P;
}

ccl_device float volume_attribute_value_to_float(const float4 value)
{
  return average(make_float3(value));
}

ccl_device float volume_attribute_value_to_alpha(const float4 value)
{
  return value.w;
}

ccl_device float3 volume_attribute_value_to_float3(const float4 value)
{
  if (value.w > 1e-6f && value.w != 1.0f) {
    /* For RGBA colors, unpremultiply after interpolation. */
    return make_float3(value) / value.w;
  }
  return make_float3(value);
}

ccl_device float4 volume_attribute_float4(KernelGlobals kg,
                                          const ccl_private ShaderData *sd,
                                          const AttributeDescriptor desc)
{
  if (desc.element & (ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) {
    return kernel_data_fetch(attributes_float4, desc.offset);
  }
  if (desc.element == ATTR_ELEMENT_VOXEL) {
    /* todo: optimize this so we don't have to transform both here and in
     * kernel_tex_image_interp_3d when possible. Also could optimize for the
     * common case where transform is translation/scale only. */
    float3 P = sd->P;
    object_inverse_position_transform(kg, sd, &P);
    const InterpolationType interp = (sd->flag & SD_VOLUME_CUBIC) ? INTERPOLATION_CUBIC :
                                                                    INTERPOLATION_NONE;
    return kernel_tex_image_interp_3d(kg, desc.offset, P, interp);
  }
  return zero_float4();
}

#endif

CCL_NAMESPACE_END
