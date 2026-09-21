/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/geom/motion_gsplat.h"
#include "kernel/geom/object.h"
#include "kernel/util/colorspace.h"

/* Gaussian splats evaluation and utility functions.
 *
 * References:
 *
 *  [chen2024pgsr]  Nicolas Moenne-Loccoz et. al.
 *                  "PGSR: Planar-based Gaussian Splatting for Efficient and High-Fidelity Surface
 *                  Reconstruction" (2024)
 *                  https://zju3dv.github.io/pgsr/
 *
 *  [3dgs2023]  Bernhard Kebl, Georgios Kopanas et al.
 *              3D Gaussian Splatting for Real-Time Radiance Field Rendering
 *              ACM Transactions on Graphics (SIGGRAPH 2023)
 *              https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/
 */

CCL_NAMESPACE_BEGIN

#if defined(__GSPLATS__)

ccl_device_inline float3 gsplat_radiance(KernelGlobals kg, const ccl_private ShaderData &sd)
{
  kernel_assert(sd.type & PRIMITIVE_GSPLAT);

  const int radiance_base_offset = gsplat_attr_radiance_base_offset(kg, sd.object);
  const float4 radiance_base = (sd.type & PRIMITIVE_MOTION) ?
                                   motion_gsplat_radiance_base(kg, sd.object, sd.prim, sd.time) :
                                   kernel_data_fetch(attributes_float4,
                                                     radiance_base_offset + sd.prim);

  /* Transform incident direction into the local pointcloud object.
   * This is easier than transforming spherical harmonics into the world space.
   *
   * Note that the wi is not guaranteed to be normalized, but the spherical harmonic evaluation
   * requires direction to be normalized.
   *
   * Use safe_normalize() to avoid possible NaN values appearing in color channels. */
  float3 direction = -sd.wi;
  object_inverse_dir_transform(kg, &sd, &direction);
  direction = safe_normalize(direction);
  kernel_assert((kernel_data_fetch(object_flag, sd.object) & SD_OBJECT_TRANSFORM_APPLIED) == 0);

  float3 color_srgb = spherical_harmonics_evaluate_band_L0(direction, make_float3(radiance_base));

  /* TODO(sergey): Support motion blur for spherical harmonics. */
  const int radiance_spherical_harmonics_rest_offset =
      kernel_data_fetch(objects, sd.object).gsplat.radiance_spherical_harmonics_rest_offset;
  if (radiance_spherical_harmonics_rest_offset != ATTR_STD_NOT_FOUND) {
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest =
        kernel_data_fetch(attributes_spherical_harmonics_rest,
                          radiance_spherical_harmonics_rest_offset + sd.prim);
    color_srgb += spherical_harmonics_evaluate_rest(packed_spherical_harmonics_rest, direction);
  }

  /* Map to linear color space, add 1/2 offset to mirror source code of [3dgs2023]. */
  const float3 color_rec709 = color_srgb_to_linear_v3(color_srgb + 0.5f);
  return rec709_to_rgb(kg, color_rec709);
}

/* Get Gaussian splat normal in the object space. */
ccl_device_inline float3 gsplat_normal(
    KernelGlobals kg, const int object, const int prim, const float time, const int primitive_type)
{
  /* Follows the "Flattening 3D Gaussian" section of [chen2024pgsr].
   *
   * The intuitive way to think about it is that gsplat is a pancake, so its normal in the Gaussian
   * splat space points in the direction of the smallest scale (orthogonal to two largest scales).
   * This means, for example, that if the scale's z component is the smallest then the normal in
   * the splat space is (0, 0, 1).
   *
   * It needs to be converted to the object space. The naive implementation would be is
   *
   *   N_{object_space} = R * N_{splat_space}
   *
   * where R is a transformation matrix constructed from the Gaussian splat rotation:
   *
   *  R = quaternion_to_rotation(rotation_quat)
   *
   * We can avoid the matrix multiplication by utilizing the knowledge that the normal is the
   * Gaussian splat space has only one component set to 1 and the rest are set to 0: the result
   * is the column of the matrix R with the same index as the component of the local normal that is
   * set to 1. */

  /* TODO(sergey): Solve ambiguity when there are two smallest axis. */
  /* The paper mentions this case explicitly and that it is solved using the view direction. */

  float3 scale;
  Quaternion rotation;
  if ((primitive_type & PRIMITIVE_MOTION) == 0) {
    const int scale_offset = gsplat_attr_scale_offset(kg, object);
    const int rotation_offset = gsplat_attr_rotation_offset(kg, object);
    scale = kernel_data_fetch(attributes_float3, scale_offset + prim);
    rotation = kernel_data_fetch(attributes_quaternion, rotation_offset + prim);
  }
  else {
    const GSplatMotionInfo motion_info = motion_gsplat_compute_motion_info(kg, object, time);
    scale = motion_gsplat_scale(kg, motion_info, object, prim);
    rotation = motion_gsplat_rotation(kg, motion_info, object, prim);
  }

#  if 0
  /* The naive implementation, for the reference. */
  const float3x3 R = quaternion_to_rotation(rotation);
  if ((scale.x <= scale.y) && (scale.x <= scale.z)) {
    return R * make_float3(1.0f, 0.0f, 0.0f);
  }
  if ((scale.y < scale.x) && (scale.y <= scale.z)) {
    return R * make_float3(0.0f, 1.0f, 0.0f);
  }
  kernel_assert((scale.z < scale.x) && (scale.z < scale.y));
  return R * make_float3(0.0f, 0.0f, 1.0f);
#  else
  /* More optimal implementation: only evaluate the actually required elements of the rotation
   * matrix. */
  const float normalizer = 1.0f / (rotation.x * rotation.x + rotation.y * rotation.y +
                                   rotation.z * rotation.z + rotation.w * rotation.w);

  const float a = rotation.w;
  const float b = rotation.x;
  const float c = rotation.y;
  const float d = rotation.z;

  if ((scale.x <= scale.y) && (scale.x <= scale.z)) {
    return make_float3(
               a * a + b * b - c * c - d * d, 2.0f * (a * d + b * c), 2.0f * (b * d - a * c)) *
           normalizer;
  }
  if ((scale.y < scale.x) && (scale.y <= scale.z)) {
    return make_float3(
               2.0f * (b * c - a * d), a * a - b * b + c * c - d * d, 2.0f * (a * b + c * d)) *
           normalizer;
  }
  kernel_assert((scale.z < scale.x) && (scale.z < scale.y));
  return make_float3(
             2.0f * (a * c + b * d), 2.0f * (c * d - a * b), a * a - b * b - c * c + d * d) *
         normalizer;
#  endif
}

#endif /* __GSPLATS__ */

CCL_NAMESPACE_END
