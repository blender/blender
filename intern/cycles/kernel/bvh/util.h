/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

ccl_device_inline bool intersection_ray_valid(ccl_private const Ray *ray)
{
  /* NOTE: Due to some vectorization code  non-finite origin point might
   * cause lots of false-positive intersections which will overflow traversal
   * stack.
   * This code is a quick way to perform early output, to avoid crashes in
   * such cases.
   * From production scenes so far it seems it's enough to test first element
   * only.
   * Scene intersection may also called with empty rays for conditional trace
   * calls that evaluate to false, so filter those out.
   */
  return isfinite_safe(ray->P.x) && isfinite_safe(ray->D.x) && len_squared(ray->D) != 0.0f;
}

/* Offset intersection distance by the smallest possible amount, to skip
 * intersections at this distance. This works in cases where the ray start
 * position is unchanged and only tmin is updated, since for self
 * intersection we'll be comparing against the exact same distances.
 *
 * Always returns normalized floating point value. */
ccl_device_forceinline float intersection_t_offset(const float t)
{
  /* This is a simplified version of `nextafterf(t, FLT_MAX)`, only dealing with
   * non-negative and finite t. */
  kernel_assert(t >= 0.0f && isfinite_safe(t));

  /* Special handling of zero, which also includes handling of denormal values:
   * always return smallest normalized value. If a denormalized zero is returned
   * it will cause false-positive intersection detection with a distance of 0.
   *
   * The check relies on the fact that comparison of de-normal values with zero
   * returns true. */
  if (t == 0.0f) {
    /* The exact bit value of this should be 0x1p-126, but hex floating point values notation is
     * not available in CUDA/OptiX. */
    return FLT_MIN;
  }

  const uint32_t bits = __float_as_uint(t) + 1;
  const float result = __uint_as_float(bits);

  /* Assert that the calculated value is indeed considered to be offset from the
   * original value. */
  kernel_assert(result > t);

  return result;
}

/* Ray offset to avoid self intersection.
 *
 * This function can be used to compute a modified ray start position for rays
 * leaving from a surface. This is from:
 * "A Fast and Robust Method for Avoiding Self-Intersection"
 * Ray Tracing Gems, chapter 6.
 */
ccl_device_inline float3 ray_offset(const float3 P, const float3 Ng)
{
  const float int_scale = 256.0f;
  const int3 of_i = make_int3(
      (int)(int_scale * Ng.x), (int)(int_scale * Ng.y), (int)(int_scale * Ng.z));

  const float3 p_i = make_float3(
      __int_as_float(__float_as_int(P.x) + ((P.x < 0) ? -of_i.x : of_i.x)),
      __int_as_float(__float_as_int(P.y) + ((P.y < 0) ? -of_i.y : of_i.y)),
      __int_as_float(__float_as_int(P.z) + ((P.z < 0) ? -of_i.z : of_i.z)));
  const float origin = 1.0f / 32.0f;
  const float float_scale = 1.0f / 65536.0f;
  return make_float3(fabsf(P.x) < origin ? P.x + float_scale * Ng.x : p_i.x,
                     fabsf(P.y) < origin ? P.y + float_scale * Ng.y : p_i.y,
                     fabsf(P.z) < origin ? P.z + float_scale * Ng.z : p_i.z);
}

#ifndef __KERNEL_GPU__
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

ccl_device_inline float intersection_curve_shadow_transparency(
    KernelGlobals kg, const int object, const int prim, const int type, const float u)
{
  /* Find attribute. */
  const int offset = intersection_find_attribute(kg, object, ATTR_STD_SHADOW_TRANSPARENCY);
  if (offset == ATTR_STD_NOT_FOUND) {
    /* If no shadow transparency attribute, assume opaque. */
    return 0.0f;
  }

  /* Interpolate transparency between curve keys. */
  const KernelCurve kcurve = kernel_data_fetch(curves, prim);
  const int k0 = kcurve.first_key + PRIMITIVE_UNPACK_SEGMENT(type);
  const int k1 = k0 + 1;

  const float f0 = kernel_data_fetch(attributes_float, offset + k0);
  const float f1 = kernel_data_fetch(attributes_float, offset + k1);

  return (1.0f - u) * f0 + u * f1;
}

ccl_device_inline bool intersection_skip_self(ccl_ray_data const RaySelfPrimitives &self,
                                              const int object,
                                              const int prim)
{
  return (self.prim == prim) && (self.object == object);
}

ccl_device_inline bool intersection_skip_self_shadow(ccl_ray_data const RaySelfPrimitives &self,
                                                     const int object,
                                                     const int prim)
{
  return ((self.prim == prim) && (self.object == object)) ||
         ((self.light_prim == prim) && (self.light_object == object));
}

ccl_device_inline bool intersection_skip_self_local(ccl_ray_data const RaySelfPrimitives &self,
                                                    const int prim)
{
  return (self.prim == prim);
}

#ifdef __SHADOW_LINKING__
ccl_device_inline uint64_t ray_get_shadow_set_membership(KernelGlobals kg,
                                                         ccl_private const Ray *ray)
{
  if (ray->self.light != LAMP_NONE) {
    return kernel_data_fetch(lights, ray->self.light).shadow_set_membership;
  }

  if (ray->self.light_object != OBJECT_NONE) {
    return kernel_data_fetch(objects, ray->self.light_object).shadow_set_membership;
  }

  return LIGHT_LINK_MASK_ALL;
}
#endif

ccl_device_inline bool intersection_skip_shadow_link(KernelGlobals kg,
                                                     ccl_private const Ray *ray,
                                                     const int isect_object)
{
#ifdef __SHADOW_LINKING__
  if (!(kernel_data.kernel_features & KERNEL_FEATURE_SHADOW_LINKING)) {
    return false;
  }

  const uint64_t set_membership = ray_get_shadow_set_membership(kg, ray);
  if (set_membership == LIGHT_LINK_MASK_ALL) {
    return false;
  }

  const uint blocker_set = kernel_data_fetch(objects, isect_object).blocker_shadow_set;
  return ((uint64_t(1) << uint64_t(blocker_set)) & set_membership) == 0;
#else
  return false;
#endif
}

CCL_NAMESPACE_END
