/* SPDX-FileCopyrightText: 2021-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/bvh/util.h"
#include "kernel/geom/motion_point.h"

CCL_NAMESPACE_BEGIN

ccl_device_forceinline int gsplat_attr_radiance_base_offset(KernelGlobals kg, const int object)
{
  kernel_assert(kernel_data_fetch(objects, object).gsplat.radiance_base_offset_and_flag !=
                ATTR_STD_NOT_FOUND);
  return kernel_data_fetch(objects, object).gsplat.radiance_base_offset_and_flag >> 1;
}

ccl_device_forceinline int gsplat_attr_radiance_base_has_motion(KernelGlobals kg, const int object)
{
  kernel_assert(kernel_data_fetch(objects, object).gsplat.radiance_base_offset_and_flag !=
                ATTR_STD_NOT_FOUND);
  return kernel_data_fetch(objects, object).gsplat.radiance_base_offset_and_flag & 1;
}

ccl_device_forceinline int gsplat_attr_rotation_offset(KernelGlobals kg, const int object)
{
  kernel_assert(kernel_data_fetch(objects, object).gsplat.rotation_offset_and_flag !=
                ATTR_STD_NOT_FOUND);
  return kernel_data_fetch(objects, object).gsplat.rotation_offset_and_flag >> 1;
}

ccl_device_forceinline int gsplat_attr_rotation_has_motion(KernelGlobals kg, const int object)
{
  kernel_assert(kernel_data_fetch(objects, object).gsplat.rotation_offset_and_flag !=
                ATTR_STD_NOT_FOUND);
  return kernel_data_fetch(objects, object).gsplat.rotation_offset_and_flag & 1;
}

ccl_device_forceinline int gsplat_attr_scale_offset(KernelGlobals kg, const int object)
{
  kernel_assert(kernel_data_fetch(objects, object).gsplat.scale_offset_and_flag !=
                ATTR_STD_NOT_FOUND);
  return kernel_data_fetch(objects, object).gsplat.scale_offset_and_flag >> 1;
}

ccl_device_forceinline int gsplat_attr_scale_has_motion(KernelGlobals kg, const int object)
{
  kernel_assert(kernel_data_fetch(objects, object).gsplat.scale_offset_and_flag !=
                ATTR_STD_NOT_FOUND);
  return kernel_data_fetch(objects, object).gsplat.scale_offset_and_flag & 1;
}

/* Motion Gaussian Splat Primitive
 *
 * These are stored as regular Gaussian splats, plus extra positions, radii (packed into
 * positions), radiances, and opacities (packed into radiance) at times other than the frame
 * center. Computing the point at a given ray time is a matter of interpolation of the two steps
 * between which the ray time lies.
 *
 * The extra points are stored as additional motion steps in ATTR_STD_POSITION. */

#if defined(__GSPLATS__)

ccl_device_inline float4 motion_gsplat_point(KernelGlobals kg,
                                             const int object,
                                             const int prim,
                                             const float time)
{
  return motion_point(kg, object, prim, time);
}

ccl_device_forceinline void motion_gsplat_compute_info(KernelGlobals kg,
                                                       const int object,
                                                       const float time,
                                                       ccl_private int *num_steps,
                                                       ccl_private int *step,
                                                       ccl_private float *t)
{
  *num_steps = kernel_data_fetch(objects, object).num_geom_steps;

  const int max_step = *num_steps - 1;
  *step = min((int)(time * max_step), max_step - 1);
  *t = time * max_step - *step;
}

ccl_device_inline Quaternion motion_gsplat_rotation_for_step(KernelGlobals kg,
                                                             int offset,
                                                             const int num_verts,
                                                             const int num_steps,
                                                             const int step,
                                                             const int prim)
{
  offset += motion_point_offset_for_step(num_verts, num_steps, step);
  return kernel_data_fetch(attributes_quaternion, offset + prim);
}

ccl_device_forceinline Quaternion motion_gsplat_rotation_interpolate(const Quaternion r1,
                                                                     const Quaternion r2,
                                                                     const float t)
{
  /* Use linear interpolation which is cheap and works good enough. This is also what
   * quat_interpolate() does for HW-RT.
   * Note that interpolation is supposed to happen on normalized quaternions, and currently no
   * normalization is enforced on the gsplat attributes. */
  /* TODO(sergey): Investigate whether normalizing ahead of time brings performance improvement.
   * The tricky part with doing it ahead of time is that it could would mean data could no longer
   * be implicitly shared with the host application. */
  return lerp(normalized(r1), normalized(r2), t);
}

ccl_device_inline Quaternion motion_gsplat_rotation(KernelGlobals kg,
                                                    const int object,
                                                    const int prim,
                                                    const float time)
{
  const int offset = gsplat_attr_rotation_offset(kg, object);
  if (!gsplat_attr_rotation_has_motion(kg, object)) {
    return kernel_data_fetch(attributes_quaternion, offset + prim);
  }

  /* Get motion info. */
  const int num_verts = kernel_data_fetch(objects, object).numverts;
  int step, num_steps;
  float t;
  motion_gsplat_compute_info(kg, object, time, &num_steps, &step, &t);

  /* Fetch key rotations. */
  const Quaternion rotation = motion_gsplat_rotation_for_step(
      kg, offset, num_verts, num_steps, step, prim);
  const Quaternion next_rotation = motion_gsplat_rotation_for_step(
      kg, offset, num_verts, num_steps, step + 1, prim);

  /* Interpolate between steps. */
  return motion_gsplat_rotation_interpolate(rotation, next_rotation, t);
}

ccl_device_inline float4 motion_gsplat_radiance_base_for_step(KernelGlobals kg,
                                                              int offset,
                                                              const int num_verts,
                                                              const int num_steps,
                                                              const int step,
                                                              const int prim)
{
  offset += motion_point_offset_for_step(num_verts, num_steps, step);
  return kernel_data_fetch(attributes_float4, offset + prim);
}

ccl_device_inline float4 motion_gsplat_radiance_base(KernelGlobals kg,
                                                     const int object,
                                                     const int prim,
                                                     const float time)
{
  const int offset = gsplat_attr_radiance_base_offset(kg, object);
  if (!gsplat_attr_radiance_base_has_motion(kg, object)) {
    return kernel_data_fetch(attributes_float4, offset + prim);
  }

  /* Get motion info. */
  const int num_verts = kernel_data_fetch(objects, object).numverts;
  int step, num_steps;
  float t;
  motion_gsplat_compute_info(kg, object, time, &num_steps, &step, &t);

  /* Fetch key radiance bases. */
  const float4 radiance_base = motion_gsplat_radiance_base_for_step(
      kg, offset, num_verts, num_steps, step, prim);
  const float4 next_radiance_base = motion_gsplat_radiance_base_for_step(
      kg, offset, num_verts, num_steps, step + 1, prim);

  /* Interpolate between steps. */
  return mix(radiance_base, next_radiance_base, t);
}

ccl_device_inline float3 motion_gsplat_scale_for_step(KernelGlobals kg,
                                                      int offset,
                                                      const int num_verts,
                                                      const int num_steps,
                                                      const int step,
                                                      const int prim)
{
  offset += motion_point_offset_for_step(num_verts, num_steps, step);
  return kernel_data_fetch(attributes_float3, offset + prim);
}

ccl_device_inline float3 motion_gsplat_scale(KernelGlobals kg,
                                             const int object,
                                             const int prim,
                                             const float time)
{
  const int offset = gsplat_attr_scale_offset(kg, object);
  if (!gsplat_attr_scale_has_motion(kg, object)) {
    return kernel_data_fetch(attributes_float3, offset + prim);
  }

  /* Get motion info. */
  const int num_verts = kernel_data_fetch(objects, object).numverts;
  int step, num_steps;
  float t;
  motion_gsplat_compute_info(kg, object, time, &num_steps, &step, &t);

  /* Fetch key scales. */
  const float3 scale = motion_gsplat_scale_for_step(kg, offset, num_verts, num_steps, step, prim);
  const float3 next_scale = motion_gsplat_scale_for_step(
      kg, offset, num_verts, num_steps, step + 1, prim);

  /* Interpolate between steps. */
  return mix(scale, next_scale, t);
}

/* Pre-calculated parameters for faster access to the Gaussian splat attributes for scale,
 * rotation, and base radiance.
 *
 * Naive implementation:
 *     const float3 scale = motion_gsplat_scale(kg, ...);
 *     const Quaternion rotation = motion_gsplat_rotation(kg, ...)
 *     const float opacity = motion_gsplat_radiance_base(kg, ...).w;
 *
 * More efficient implementation using pre-calculated information:
 *     const GSplatMotionInfo motion_info = motion_gsplat_compute_motion_info(kg, object, time);
 *     const float3 scale = motion_gsplat_scale(kg, motion_info, ...);
 *     const Quaternion rotation = motion_gsplat_rotation(kg, motion_info, ...);
 *     const float opacity = motion_gsplat_opacity(kg, motion_info, ...); */
struct GSplatMotionInfo {
  int offset_for_step;
  int offset_for_next_step;
  float t;
};

ccl_device_forceinline GSplatMotionInfo motion_gsplat_compute_motion_info(KernelGlobals kg,
                                                                          const int object,
                                                                          const float time)
{
  GSplatMotionInfo prim_motion_info;

  const int num_verts = kernel_data_fetch(objects, object).numverts;
  int step, num_steps;
  motion_gsplat_compute_info(kg, object, time, &num_steps, &step, &prim_motion_info.t);

  prim_motion_info.offset_for_step = motion_point_offset_for_step(num_verts, num_steps, step);
  prim_motion_info.offset_for_next_step = motion_point_offset_for_step(
      num_verts, num_steps, step + 1);

  return prim_motion_info;
}

ccl_device_forceinline float3 motion_gsplat_scale(KernelGlobals kg,
                                                  ccl_private const GSplatMotionInfo &motion_info,
                                                  const int object,
                                                  const int prim)
{
  const int offset = gsplat_attr_scale_offset(kg, object) + prim;
  if (gsplat_attr_scale_has_motion(kg, object)) {
    const float3 scale = kernel_data_fetch(attributes_float3,
                                           motion_info.offset_for_step + offset);
    const float3 next_scale = kernel_data_fetch(attributes_float3,
                                                motion_info.offset_for_next_step + offset);
    return mix(scale, next_scale, motion_info.t);
  }
  return kernel_data_fetch(attributes_float3, offset);
}

ccl_device_forceinline Quaternion
motion_gsplat_rotation(KernelGlobals kg,
                       ccl_private const GSplatMotionInfo &motion_info,
                       const int object,
                       const int prim)
{
  const int offset = gsplat_attr_rotation_offset(kg, object) + prim;
  if (gsplat_attr_rotation_has_motion(kg, object)) {
    const Quaternion rotation = kernel_data_fetch(attributes_quaternion,
                                                  motion_info.offset_for_step + offset);
    const Quaternion next_rotation = kernel_data_fetch(attributes_quaternion,
                                                       motion_info.offset_for_next_step + offset);
    return motion_gsplat_rotation_interpolate(rotation, next_rotation, motion_info.t);
  }
  return kernel_data_fetch(attributes_quaternion, offset);
}

ccl_device_forceinline float motion_gsplat_opacity(KernelGlobals kg,
                                                   ccl_private const GSplatMotionInfo &motion_info,
                                                   const int object,
                                                   const int prim)
{
  const int offset = gsplat_attr_radiance_base_offset(kg, object) + prim;
  if (gsplat_attr_radiance_base_has_motion(kg, object)) {
    const float opacity =
        kernel_data_fetch(attributes_float4, motion_info.offset_for_step + offset).w;
    const float next_opacity =
        kernel_data_fetch(attributes_float4, motion_info.offset_for_next_step + offset).w;
    return mix(opacity, next_opacity, motion_info.t);
  }
  return kernel_data_fetch(attributes_float4, offset).w;
}

#endif /* __GSPLATS__ */

CCL_NAMESPACE_END
