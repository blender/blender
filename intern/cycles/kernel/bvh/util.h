/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

/* Offset intersection distance by the smallest possible amount, to skip
 * intersections at this distance. This works in cases where the ray start
 * position is unchanged and only tmin is updated, since for self
 * intersection we'll be comparing against the exact same distances. */
ccl_device_forceinline float intersection_t_offset(const float t)
{
  /* This is a simplified version of nextafterf(t, FLT_MAX), only dealing with
   * non-negative and finite t. */
  kernel_assert(t >= 0.0f && isfinite_safe(t));
  const uint32_t bits = (t == 0.0f) ? 1 : __float_as_uint(t) + 1;
  return __uint_as_float(bits);
}

#if defined(__KERNEL_CPU__)
ccl_device int intersections_compare(const void *a, const void *b)
{
  const Intersection *isect_a = (const Intersection *)a;
  const Intersection *isect_b = (const Intersection *)b;

  if (isect_a->t < isect_b->t)
    return -1;
  else if (isect_a->t > isect_b->t)
    return 1;
  else
    return 0;
}
#endif

/* For subsurface scattering, only sorting a small amount of intersections
 * so bubble sort is fine for CPU and GPU. */
ccl_device_inline void sort_intersections_and_normals(ccl_private Intersection *hits,
                                                      ccl_private float3 *Ng,
                                                      uint num_hits)
{
  bool swapped;
  do {
    swapped = false;
    for (int j = 0; j < num_hits - 1; ++j) {
      if (hits[j].t > hits[j + 1].t) {
        Intersection tmp_hit = hits[j];
        float3 tmp_Ng = Ng[j];
        hits[j] = hits[j + 1];
        Ng[j] = Ng[j + 1];
        hits[j + 1] = tmp_hit;
        Ng[j + 1] = tmp_Ng;
        swapped = true;
      }
    }
    --num_hits;
  } while (swapped);
}

/* Utility to quickly get flags from an intersection. */

ccl_device_forceinline int intersection_get_shader_flags(KernelGlobals kg,
                                                         const int prim,
                                                         const int type)
{
  int shader = 0;

  if (type & PRIMITIVE_TRIANGLE) {
    shader = kernel_data_fetch(tri_shader, prim);
  }
#ifdef __POINTCLOUD__
  else if (type & PRIMITIVE_POINT) {
    shader = kernel_data_fetch(points_shader, prim);
  }
#endif
#ifdef __HAIR__
  else if (type & PRIMITIVE_CURVE) {
    shader = kernel_data_fetch(curves, prim).shader_id;
  }
#endif

  return kernel_data_fetch(shaders, (shader & SHADER_MASK)).flags;
}

ccl_device_forceinline int intersection_get_shader_from_isect_prim(KernelGlobals kg,
                                                                   const int prim,
                                                                   const int isect_type)
{
  int shader = 0;

  if (isect_type & PRIMITIVE_TRIANGLE) {
    shader = kernel_data_fetch(tri_shader, prim);
  }
#ifdef __POINTCLOUD__
  else if (isect_type & PRIMITIVE_POINT) {
    shader = kernel_data_fetch(points_shader, prim);
  }
#endif
#ifdef __HAIR__
  else if (isect_type & PRIMITIVE_CURVE) {
    shader = kernel_data_fetch(curves, prim).shader_id;
  }
#endif

  return shader & SHADER_MASK;
}

ccl_device_forceinline int intersection_get_shader(
    KernelGlobals kg, ccl_private const Intersection *ccl_restrict isect)
{
  return intersection_get_shader_from_isect_prim(kg, isect->prim, isect->type);
}

ccl_device_forceinline int intersection_get_object_flags(
    KernelGlobals kg, ccl_private const Intersection *ccl_restrict isect)
{
  return kernel_data_fetch(object_flag, isect->object);
}

/* TODO: find a better (faster) solution for this. Maybe store offset per object for
 * attributes needed in intersection? */
ccl_device_inline int intersection_find_attribute(KernelGlobals kg,
                                                  const int object,
                                                  const uint id)
{
  uint attr_offset = kernel_data_fetch(objects, object).attribute_map_offset;
  AttributeMap attr_map = kernel_data_fetch(attributes_map, attr_offset);

  while (attr_map.id != id) {
    if (UNLIKELY(attr_map.id == ATTR_STD_NONE)) {
      if (UNLIKELY(attr_map.element == 0)) {
        return (int)ATTR_STD_NOT_FOUND;
      }
      else {
        /* Chain jump to a different part of the table. */
        attr_offset = attr_map.offset;
      }
    }
    else {
      attr_offset += ATTR_PRIM_TYPES;
    }
    attr_map = kernel_data_fetch(attributes_map, attr_offset);
  }

  /* return result */
  return (attr_map.element == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND : (int)attr_map.offset;
}

/* Transparent Shadows */

/* Cut-off value to stop transparent shadow tracing when practically opaque. */
#define CURVE_SHADOW_TRANSPARENCY_CUTOFF 0.001f

ccl_device_inline float intersection_curve_shadow_transparency(KernelGlobals kg,
                                                               const int object,
                                                               const int prim,
                                                               const float u)
{
  /* Find attribute. */
  const int offset = intersection_find_attribute(kg, object, ATTR_STD_SHADOW_TRANSPARENCY);
  if (offset == ATTR_STD_NOT_FOUND) {
    /* If no shadow transparency attribute, assume opaque. */
    return 0.0f;
  }

  /* Interpolate transparency between curve keys. */
  const KernelCurve kcurve = kernel_data_fetch(curves, prim);
  const int k0 = kcurve.first_key + PRIMITIVE_UNPACK_SEGMENT(kcurve.type);
  const int k1 = k0 + 1;

  const float f0 = kernel_data_fetch(attributes_float, offset + k0);
  const float f1 = kernel_data_fetch(attributes_float, offset + k1);

  return (1.0f - u) * f0 + u * f1;
}

ccl_device_inline bool intersection_skip_self(ccl_private const RaySelfPrimitives &self,
                                              const int object,
                                              const int prim)
{
  return (self.prim == prim) && (self.object == object);
}

ccl_device_inline bool intersection_skip_self_shadow(ccl_private const RaySelfPrimitives &self,
                                                     const int object,
                                                     const int prim)
{
  return ((self.prim == prim) && (self.object == object)) ||
         ((self.light_prim == prim) && (self.light_object == object));
}

ccl_device_inline bool intersection_skip_self_local(ccl_private const RaySelfPrimitives &self,
                                                    const int prim)
{
  return (self.prim == prim);
}

CCL_NAMESPACE_END
