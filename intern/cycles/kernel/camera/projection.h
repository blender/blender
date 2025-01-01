/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/types.h"

#include "util/math.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* Equirectangular coordinates <-> Cartesian direction */

ccl_device float2 direction_to_equirectangular_range(const float3 dir, const float4 range)
{
  if (is_zero(dir)) {
    return zero_float2();
  }

  const float u = (atan2f(dir.y, dir.x) - range.y) / range.x;
  const float v = (acosf(dir.z / len(dir)) - range.w) / range.z;

  return make_float2(u, v);
}

ccl_device float3 equirectangular_range_to_direction(const float u,
                                                     const float v,
                                                     const float4 range)
{
  const float phi = range.x * u + range.y;
  const float theta = range.z * v + range.w;
  return spherical_to_direction(theta, phi);
}

ccl_device float2 direction_to_equirectangular(const float3 dir)
{
  return direction_to_equirectangular_range(dir, make_float4(-M_2PI_F, M_PI_F, -M_PI_F, M_PI_F));
}

ccl_device float3 equirectangular_to_direction(const float u, const float v)
{
  return equirectangular_range_to_direction(u, v, make_float4(-M_2PI_F, M_PI_F, -M_PI_F, M_PI_F));
}

ccl_device float2 direction_to_central_cylindrical(const float3 dir, const float4 range)
{
  const float z = dir.z / len(make_float2(dir.x, dir.y));
  const float theta = atan2f(dir.y, dir.x);
  const float u = inverse_lerp(range.x, range.y, theta);
  const float v = inverse_lerp(range.z, range.w, z);
  return make_float2(u, v);
}

ccl_device float3 central_cylindrical_to_direction(const float u,
                                                   const float v,
                                                   const float4 range)
{
  const float theta = mix(range.x, range.y, u);
  const float z = mix(range.z, range.w, v);
  return make_float3(cosf(theta), sinf(theta), z);
}

/* Fisheye <-> Cartesian direction */

ccl_device_inline float3 fisheye_to_direction(const float theta,
                                              const float u,
                                              float v,
                                              const float r)
{
  float phi = safe_acosf(safe_divide(u, r));
  if (v < 0.0f) {
    phi = -phi;
  }

  return make_float3(cosf(theta), -cosf(phi) * sinf(theta), sinf(phi) * sinf(theta));
}

ccl_device float2 direction_to_fisheye_equidistant(const float3 dir, const float fov)
{
  const float r = atan2f(len(make_float2(dir.y, dir.z)), dir.x) / fov;
  const float2 uv = r * safe_normalize(make_float2(dir.y, dir.z));
  return make_float2(0.5f - uv.x, uv.y + 0.5f);
}

ccl_device float3 fisheye_equidistant_to_direction(float u, float v, float fov)
{
  u = (u - 0.5f) * 2.0f;
  v = (v - 0.5f) * 2.0f;

  const float r = sqrtf(u * u + v * v);

  if (r > 1.0f) {
    return zero_float3();
  }

  const float theta = r * fov * 0.5f;

  return fisheye_to_direction(theta, u, v, r);
}

ccl_device float2 direction_to_fisheye_equisolid(const float3 dir,
                                                 const float lens,
                                                 const float width,
                                                 const float height)
{
  const float theta = safe_acosf(dir.x);
  const float r = 2.0f * lens * sinf(theta * 0.5f);

  const float2 uv = r * safe_normalize(make_float2(dir.y, dir.z));
  return make_float2(0.5f - uv.x / width, uv.y / height + 0.5f);
}

ccl_device_inline float3 fisheye_equisolid_to_direction(
    float u, float v, float lens, const float fov, const float width, const float height)
{
  u = (u - 0.5f) * width;
  v = (v - 0.5f) * height;

  const float rmax = 2.0f * lens * sinf(fov * 0.25f);
  const float r = sqrtf(u * u + v * v);

  if (r > rmax) {
    return zero_float3();
  }

  const float theta = 2.0f * asinf(r / (2.0f * lens));

  return fisheye_to_direction(theta, u, v, r);
}

ccl_device_inline float3 fisheye_lens_polynomial_to_direction(float u,
                                                              float v,
                                                              float coeff0,
                                                              const float4 coeffs,
                                                              const float fov,
                                                              const float width,
                                                              const float height)
{
  u = (u - 0.5f) * width;
  v = (v - 0.5f) * height;

  const float r = sqrtf(u * u + v * v);
  const float r2 = r * r;
  const float4 rr = make_float4(r, r2, r2 * r, r2 * r2);
  const float theta = -(coeff0 + dot(coeffs, rr));

  if (fabsf(theta) > 0.5f * fov) {
    return zero_float3();
  }

  return fisheye_to_direction(theta, u, v, r);
}

ccl_device float2 direction_to_fisheye_lens_polynomial(
    float3 dir, const float coeff0, const float4 coeffs, const float width, const float height)
{
  const float theta = -safe_acosf(dir.x);

  /* Initialize r with the closed-form solution for the special case
   * coeffs.y = coeffs.z = coeffs.w = 0 */
  float r = (theta - coeff0) / coeffs.x;

  const float4 diff_coeffs = make_float4(1.0f, 2.0f, 3.0f, 4.0f) * coeffs;

  for (int i = 0; i < 20; i++) {
    /*  Newton's method for finding roots
     *
     *  Given is the result theta = distortion_model(r),
     *  we need to find r.
     *  Let F(r) := theta - distortion_model(r).
     *  Then F(r) = 0 <=> distortion_model(r) = theta
     *  Therefore we apply Newton's method for finding a root of F(r).
     *  Newton step for the function F:
     *  r_n+1 = r_n - F(r_n) / F'(r_n)
     *  The addition in the implementation is due to canceling of signs.
     * \{ */
    const float old_r = r;
    const float r2 = r * r;
    const float F_r = theta - (coeff0 + dot(coeffs, make_float4(r, r2, r2 * r, r2 * r2)));
    const float dF_r = dot(diff_coeffs, make_float4(1.0f, r, r2, r2 * r));
    r += F_r / dF_r;

    /* Early termination if the change is below the threshold */
    if (fabsf(r - old_r) < 1e-6f) {
      break;
    }
    /** \} */
  }

  const float2 uv = r * safe_normalize(make_float2(dir.y, dir.z));
  return make_float2(0.5f - uv.x / width, uv.y / height + 0.5f);
}

/* Mirror Ball <-> Cartesion direction */

ccl_device float3 mirrorball_to_direction(const float u, const float v)
{
  /* point on sphere */
  float3 dir;

  dir.x = 2.0f * u - 1.0f;
  dir.z = 2.0f * v - 1.0f;

  if (dir.x * dir.x + dir.z * dir.z > 1.0f) {
    return zero_float3();
  }

  dir.y = -sqrtf(max(1.0f - dir.x * dir.x - dir.z * dir.z, 0.0f));

  /* reflection */
  const float3 I = make_float3(0.0f, -1.0f, 0.0f);

  return 2.0f * dot(dir, I) * dir - I;
}

ccl_device float2 direction_to_mirrorball(float3 dir)
{
  /* inverse of mirrorball_to_direction */
  dir.y -= 1.0f;

  const float div = 2.0f * sqrtf(max(-0.5f * dir.y, 0.0f));
  if (div > 0.0f) {
    dir /= div;
  }

  const float u = 0.5f * (dir.x + 1.0f);
  const float v = 0.5f * (dir.z + 1.0f);

  return make_float2(u, v);
}

/* Single face of a equiangular cube map projection as described in
 * https://blog.google/products/google-ar-vr/bringing-pixels-front-and-center-vr-video/ */
ccl_device float3 equiangular_cubemap_face_to_direction(float u, float v)
{
  u = tanf((0.5f - u) * M_PI_2_F);
  v = tanf((v - 0.5f) * M_PI_2_F);

  return normalize(make_float3(1.0f, u, v));
}

ccl_device float2 direction_to_equiangular_cubemap_face(const float3 dir)
{
  const float u = 0.5f - atan2f(dir.y, dir.x) * 2.0f / M_PI_F;
  const float v = atan2f(dir.z, dir.x) * 2.0f / M_PI_F + 0.5f;

  return make_float2(u, v);
}

ccl_device_inline float3 panorama_to_direction(ccl_constant KernelCamera *cam,
                                               const float u,
                                               float v)
{
  switch (cam->panorama_type) {
    case PANORAMA_EQUIRECTANGULAR:
      return equirectangular_range_to_direction(u, v, cam->equirectangular_range);
    case PANORAMA_EQUIANGULAR_CUBEMAP_FACE:
      return equiangular_cubemap_face_to_direction(u, v);
    case PANORAMA_MIRRORBALL:
      return mirrorball_to_direction(u, v);
    case PANORAMA_FISHEYE_EQUIDISTANT:
      return fisheye_equidistant_to_direction(u, v, cam->fisheye_fov);
    case PANORAMA_FISHEYE_LENS_POLYNOMIAL:
      return fisheye_lens_polynomial_to_direction(u,
                                                  v,
                                                  cam->fisheye_lens_polynomial_bias,
                                                  cam->fisheye_lens_polynomial_coefficients,
                                                  cam->fisheye_fov,
                                                  cam->sensorwidth,
                                                  cam->sensorheight);
    case PANORAMA_CENTRAL_CYLINDRICAL:
      return central_cylindrical_to_direction(u, v, cam->central_cylindrical_range);
    case PANORAMA_FISHEYE_EQUISOLID:
    default:
      return fisheye_equisolid_to_direction(
          u, v, cam->fisheye_lens, cam->fisheye_fov, cam->sensorwidth, cam->sensorheight);
  }
}

ccl_device_inline float2 direction_to_panorama(ccl_constant KernelCamera *cam, const float3 dir)
{
  switch (cam->panorama_type) {
    case PANORAMA_EQUIRECTANGULAR:
      return direction_to_equirectangular_range(dir, cam->equirectangular_range);
    case PANORAMA_EQUIANGULAR_CUBEMAP_FACE:
      return direction_to_equiangular_cubemap_face(dir);
    case PANORAMA_MIRRORBALL:
      return direction_to_mirrorball(dir);
    case PANORAMA_FISHEYE_EQUIDISTANT:
      return direction_to_fisheye_equidistant(dir, cam->fisheye_fov);
    case PANORAMA_FISHEYE_LENS_POLYNOMIAL:
      return direction_to_fisheye_lens_polynomial(dir,
                                                  cam->fisheye_lens_polynomial_bias,
                                                  cam->fisheye_lens_polynomial_coefficients,
                                                  cam->sensorwidth,
                                                  cam->sensorheight);
    case PANORAMA_CENTRAL_CYLINDRICAL:
      return direction_to_central_cylindrical(dir, cam->central_cylindrical_range);
    case PANORAMA_FISHEYE_EQUISOLID:
    default:
      return direction_to_fisheye_equisolid(
          dir, cam->fisheye_lens, cam->sensorwidth, cam->sensorheight);
  }
}

ccl_device_inline void spherical_stereo_transform(ccl_constant KernelCamera *cam,
                                                  ccl_private float3 *P,
                                                  ccl_private float3 *D)
{
  float interocular_offset = cam->interocular_offset;

  /* Interocular offset of zero means either non stereo, or stereo without
   * spherical stereo. */
  kernel_assert(interocular_offset != 0.0f);

  if (cam->pole_merge_angle_to > 0.0f) {
    const float pole_merge_angle_from = cam->pole_merge_angle_from;
    const float pole_merge_angle_to = cam->pole_merge_angle_to;
    const float altitude = fabsf(safe_asinf((*D).z));
    if (altitude > pole_merge_angle_to) {
      interocular_offset = 0.0f;
    }
    else if (altitude > pole_merge_angle_from) {
      const float fac = (altitude - pole_merge_angle_from) /
                        (pole_merge_angle_to - pole_merge_angle_from);
      const float fade = cosf(fac * M_PI_2_F);
      interocular_offset *= fade;
    }
  }

  const float3 up = make_float3(0.0f, 0.0f, 1.0f);
  const float3 side = normalize(cross(*D, up));
  const float3 stereo_offset = side * interocular_offset;

  *P += stereo_offset;

  /* Convergence distance is FLT_MAX in the case of parallel convergence mode,
   * no need to modify direction in this case either. */
  const float convergence_distance = cam->convergence_distance;

  if (convergence_distance != FLT_MAX) {
    const float3 screen_offset = convergence_distance * (*D);
    *D = normalize(screen_offset - stereo_offset);
  }
}

CCL_NAMESPACE_END
