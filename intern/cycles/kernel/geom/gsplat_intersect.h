/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/geom/motion_gsplat.h"
#include "kernel/geom/object.h"
#include "kernel/geom/point_intersect.h"

CCL_NAMESPACE_BEGIN

/* Gaussian splats intersection functions.
 *
 * References:
 *
 *  [3dgrt2024]  Nicolas Moenne-Loccoz et. al.
 *               "3D Gaussian Ray Tracing: Fast Tracing of Particle Scenes" (2024)
 *               ACM Transactions on Graphics and SIGGRAPH Asia
 *               https://gaussiantracer.github.io/
 *
 *  [sun2025]  Xin Sun and Iliyan Georgiev and Yun Fei and Miloš Hašan
 *             "Stochastic ray tracing of transparent 3D Gaussians" (2025)
 *             EGSR conference proceedings
 *             https://iliyan.com/publications/GaussianRayTracing/
 *
 *  [Rijsdijk2026] Joris Rijsdijk et. al.
 *                 "Gaussian Point Splatting" (2026)
 *                 ACM Trans. Graph.
 *                 https://jorisar.nl/gaussian_point_splatting/
 *
 *  [chen2024pgsr]  Nicolas Moenne-Loccoz et. al.
 *                  "PGSR: Planar-based Gaussian Splatting for Efficient and High-Fidelity Surface
 *                  Reconstruction" (2024)
 *                  https://zju3dv.github.io/pgsr/
 */

#ifdef __POINTCLOUD__

#  if defined(__GSPLATS__)

/* Parameters of the Gaussian splat that are needed for intersection. */
struct GSplatIsectParams {
  float3 scale;
  Quaternion rotation;
  float opacity;
};

/* Optimized access to Gaussian splat parameters for the intersection purposes.
 * Reuses steps calculation to access multiple motion attributes. */
ccl_device_forceinline GSplatIsectParams gsplat_intersect_get_params(
    KernelGlobals kg, const int object, const int prim, const float time, const int primitive_type)
{
  GSplatIsectParams params;

  if ((primitive_type & PRIMITIVE_MOTION) == 0) {
    const int scale_offset = gsplat_attr_scale_offset(kg, object);
    const int rotation_offset = gsplat_attr_rotation_offset(kg, object);
    const int radiance_base_offset = gsplat_attr_radiance_base_offset(kg, object);
    params.scale = kernel_data_fetch(attributes_float3, scale_offset + prim);
    params.rotation = kernel_data_fetch(attributes_quaternion, rotation_offset + prim);
    params.opacity = kernel_data_fetch(attributes_float4, radiance_base_offset + prim).w;
    return params;
  }

  const GSplatMotionInfo motion_info = motion_gsplat_compute_motion_info(kg, object, time);
  params.scale = motion_gsplat_scale(kg, motion_info, object, prim);
  params.rotation = motion_gsplat_rotation(kg, motion_info, object, prim);
  params.opacity = motion_gsplat_opacity(kg, motion_info, object, prim);
  return params;
}

/* Intersect ray against Gaussian splat.
 * Performs stochastic alpha test described in [sun2025]. */
ccl_device_forceinline bool gsplat_intersect_test(KernelGlobals kg,
                                                  const float3 ray_P,
                                                  const float3 ray_D,
                                                  const float ray_tmin,
                                                  const float ray_tmax,
                                                  const int object,
                                                  const int prim,
                                                  const float time,
                                                  const int primitive_type,
                                                  ccl_private float &r_t)
{
  const int position_offset = kernel_data_fetch(objects, object).position_offset;
  const float4 point = (primitive_type & PRIMITIVE_MOTION) ?
                           motion_gsplat_point(kg, object, prim, time) :
                           kernel_data_fetch(points, position_offset + prim);

  /* Intersect with the bounding sphere.
   * Equivalent to IsNegligible() check in Algorithm 1 of [sun2025].
   *
   * Disable backface culling so that we can intersect splats with the ray origin which is inside
   * of the bounding sphere. */
  float sphere_t;
  if (!point_intersect_test<false>(point, ray_P, ray_D, ray_tmin, ray_tmax, &sphere_t)) {
    return false;
  }

  const GSplatIsectParams params = gsplat_intersect_get_params(
      kg, object, prim, time, primitive_type);

  /* Clamp the scale to avoid division near zero when calculating the inverse covariance matrix.
   *
   * Without this clamping the floaters in bicycle and garden scenes from Inria models set are
   * rendered as large almost opaque spheres (instead of being almost transparent disks). The
   * reason for this seems to be a precision issue of some sort when evaluating the particle
   * response.
   *
   * The actual clamping value is a bit tricky: as values that work well for those scenes do not
   * work well for the Le Mont Saint Michel scene where all splats are very small, making it so
   * clamping makes the scene much more blurry than it should be. */
  /* TODO(sergey): Look into more reliable solutions. */
  const float max_scale = reduce_max(params.scale);
  const float3 scale_clamped = max(params.scale, make_float3(max_scale * 1e-3f));

  const float3 mu = make_float3(point);

  /* [3dgrt2024] Eq. (8). */
  const float3x3 S_inv = float3x3_scale(1.0f / scale_clamped);
  const float3x3 R = quaternion_to_rotation(params.rotation);
  const float3x3 R_T = transposed(R);
  const float3x3 S_inv_x_R_T = S_inv * R_T;
  const float3 o_g = S_inv_x_R_T * (ray_P - mu);
  const float3 d_g = S_inv_x_R_T * ray_D;
  const float t = -dot(o_g, d_g) / dot(d_g, d_g); /* tau_max in [3dgrt2024] */

  /* Check the intersection with the Gaussian splat is within the valid ray range.
   * The previous checks are based on the bounding sphere distance.
   *
   * The t might be lower than the ray_tmin when the ray origin is within the bounding sphere but
   * is "past" the density extrema. */
  if (t < ray_tmin || t > ray_tmax) {
    return false;
  }

  /* Calculate intersection point. */
  const float3 x = ray_P + t * ray_D;

  /* Covariance matrix. [3dgrt2024] Eq. (2).
   * Note that for the particle response evaluation we actually need an inverse of the covariance
   * matrix.
   *
   * Start with the original equation  \Sigma = RSS^TR^T
   * It could be simplified since S^T \equiv S due to the matrix being diagonal scale matrix:
   * \Sigma = RS^2R^T.
   *
   *     \Sigma^{-1} = (RS^2R^T)^{-1} = (R^T)^{-1}(S^2)^{-1}R^{-1} = (R^{-1})^T(S^2)^{-1}R^{-1}
   *
   * Since R is an orthonormal rotation matrix: R^{-1} \equiv R^T */
  const float3x3 Sigma_inv = R * float3x3_scale(1.0f / (scale_clamped * scale_clamped)) * R_T;

  /* Particle response.
   * [sun2025] Algorithm 1 (line 12). */
  const float3 x_minus_mu = x - mu;
  const float particle_opacity = params.opacity *
                                 fast_expf(-1.0f / 2.0f * dot(x_minus_mu, Sigma_inv * x_minus_mu));

  /* Approach similar to [sun2025] who uses stateless trigonometric hash function as a source of
   * RNG: hash the intersection point to a [0 .. 1] floating point. */
  /* TODO(sergey): Check on using potentially cheaper PCG hash?
   * Internally hash_float3_to_float() uses Jenkins Lookup3 hash function. */
  if (hash_float3_to_float(x) > particle_opacity) {
    return false;
  }

  r_t = t;

  return true;
}

ccl_device_forceinline bool gsplat_intersect(KernelGlobals kg,
                                             ccl_private Intersection *isect,
                                             const float3 ray_P,
                                             const float3 ray_D,
                                             const float ray_tmin,
                                             const float ray_tmax,
                                             const int object,
                                             const int prim,
                                             const float time,
                                             const int primitive_type)
{
  kernel_assert(primitive_type & PRIMITIVE_GSPLAT);

  if (!gsplat_intersect_test(
          kg, ray_P, ray_D, ray_tmin, ray_tmax, object, prim, time, primitive_type, isect->t))
  {
    return false;
  }

  isect->prim = prim;
  isect->object = object;
  isect->type = primitive_type;
  isect->u = 0.0f;
  isect->v = 0.0f;
  return true;
}
#  endif /* __GSPLATS__ */

ccl_device_forceinline bool point_or_gsplat_intersect(KernelGlobals kg,
                                                      ccl_private Intersection *isect,
                                                      const float3 ray_P,
                                                      const float3 ray_D,
                                                      const float ray_tmin,
                                                      const float ray_tmax,
                                                      const int object,
                                                      const int prim,
                                                      const float time,
                                                      const int primitive_type)
{
#  if defined(__GSPLATS__)
  if (primitive_type & PRIMITIVE_POINT)
#  endif
  {
    return point_intersect(
        kg, isect, ray_P, ray_D, ray_tmin, ray_tmax, object, prim, time, primitive_type);
  }

#  if defined(__GSPLATS__)
  kernel_assert(primitive_type & PRIMITIVE_GSPLAT);
  return gsplat_intersect(
      kg, isect, ray_P, ray_D, ray_tmin, ray_tmax, object, prim, time, primitive_type);
#  endif
}

#endif

CCL_NAMESPACE_END
