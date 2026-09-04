/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Camera projection / uv functions and utils.
 */

#include "eevee_camera_shared.hh"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

namespace eevee::camera {

/* -------------------------------------------------------------------- */
/** \name Panoramic Projections
 *
 * Adapted from Cycles to match EEVEE's coordinate system.
 * \{ */

float2 equirectangular_from_direction(CameraData cam, float3 dir)
{
  float phi = atan(-dir.z, dir.x);
  float theta = acos(dir.y / length(dir));
  return (float2(phi, theta) - cam.equirect_bias) * cam.equirect_scale_inv;
}

float3 equirectangular_to_direction(CameraData cam, float2 uv)
{
  uv = uv * cam.equirect_scale + cam.equirect_bias;
  float phi = uv.x;
  float theta = uv.y;
  float sin_theta = sin(theta);
  return float3(sin_theta * cos(phi), cos(theta), -sin_theta * sin(phi));
}

float2 equiangular_cubemap_face_from_direction(CameraData cam, float3 dir)
{
  float2 uv;
  uv.x = 0.5f - atan(-dir.x, -dir.z) * 2.0f / M_PI;
  uv.y = atan(dir.y, -dir.z) * 2.0f / M_PI + 0.5f;
  return (uv - cam.uv_bias) / cam.uv_scale;
}

float3 equiangular_cubemap_face_to_direction(CameraData cam, float2 uv)
{
  uv = uv * cam.uv_scale + cam.uv_bias;
  float u = tan((0.5f - uv.x) * M_PI_2);
  float v = tan((uv.y - 0.5f) * M_PI_2);
  return normalize(float3(-u, v, -1.0f));
}

float2 central_cylindrical_from_direction(CameraData cam, float3 dir)
{
  const float cylinder_height = dir.y / length(dir.xz);
  const float theta = atan(-dir.x, -dir.z);
  const float2 uv = (float2(theta, cylinder_height) - cam.central_cylindrical_range.xz) /
                    (cam.central_cylindrical_range.yw - cam.central_cylindrical_range.xz);
  return (uv - cam.uv_bias) / cam.uv_scale;
}

float3 central_cylindrical_to_direction(CameraData cam, float2 uv)
{
  uv = uv * cam.uv_scale + cam.uv_bias;
  const float theta = mix(cam.central_cylindrical_range.x, cam.central_cylindrical_range.y, uv.x);
  const float cylinder_height = mix(
      cam.central_cylindrical_range.z, cam.central_cylindrical_range.w, uv.y);
  return normalize(float3(-sin(theta), cylinder_height, -cos(theta)));
}

float2 fisheye_from_direction(CameraData cam, float3 dir)
{
  float r = atan(length(dir.xy), -dir.z) / cam.fisheye_fov;
  float phi = atan(dir.y, dir.x);
  float2 uv = r * float2(cos(phi), sin(phi)) + 0.5f;
  return (uv - cam.uv_bias) / cam.uv_scale;
}

float3 fisheye_to_direction(CameraData cam, float2 uv)
{
  uv = uv * cam.uv_scale + cam.uv_bias;
  uv = (uv - 0.5f) * 2.0f;
  float r = length(uv);
  if (r > 1.0f) {
    return float3(0.0f);
  }
  float phi = safe_acos(uv.x * safe_rcp(r));
  float theta = r * cam.fisheye_fov * 0.5f;
  if (uv.y < 0.0f) {
    phi = -phi;
  }
  return float3(cos(phi) * sin(theta), sin(phi) * sin(theta), -cos(theta));
}

float2 fisheye_equisolid_from_direction(CameraData cam, float3 dir)
{
  float theta = safe_acos(-dir.z);
  float r = 2.0f * cam.fisheye_lens * sin(theta * 0.5f);
  float2 dir_xy = dir.xy;
  float2 uv = r * safe_normalize(dir_xy);
  uv = uv / cam.fisheye_sensor + 0.5f;
  return (uv - cam.uv_bias) / cam.uv_scale;
}

float3 fisheye_equisolid_to_direction(CameraData cam, float2 uv)
{
  uv = uv * cam.uv_scale + cam.uv_bias;
  uv = (uv - 0.5f) * cam.fisheye_sensor;

  float r = length(uv);
  float rmax = 2.0f * cam.fisheye_lens * sin(cam.fisheye_fov * 0.25f);
  if (r > rmax) {
    return float3(0.0f);
  }

  float theta = 2.0f * asin(r / (2.0f * cam.fisheye_lens));
  float phi = safe_acos(uv.x * safe_rcp(r));
  if (uv.y < 0.0f) {
    phi = -phi;
  }
  return float3(cos(phi) * sin(theta), sin(phi) * sin(theta), -cos(theta));
}

float2 fisheye_lens_polynomial_from_direction(CameraData cam, float3 dir)
{
  const float theta = -safe_acos(-dir.z);

  float r = (theta - cam.fisheye_polynomial_bias) / cam.fisheye_polynomial_coefficients.x;
  const float4 diff_coefficients = float4(1.0f, 2.0f, 3.0f, 4.0f) *
                                   cam.fisheye_polynomial_coefficients;

  for (int i = 0; i < 20; i++) {
    const float old_r = r;
    const float r2 = r * r;
    const float4 rr = float4(r, r2, r2 * r, r2 * r2);
    const float F_r = theta -
                      (cam.fisheye_polynomial_bias + dot(cam.fisheye_polynomial_coefficients, rr));
    const float dF_r = dot(diff_coefficients, float4(1.0f, r, r2, r2 * r));
    r += F_r / dF_r;

    if (abs(r - old_r) < 1.0e-6f) {
      break;
    }
  }

  float2 dir_xy = dir.xy;
  float2 uv = r * safe_normalize(dir_xy);
  uv = uv / cam.fisheye_sensor + 0.5f;
  return (uv - cam.uv_bias) / cam.uv_scale;
}

float3 fisheye_lens_polynomial_to_direction(CameraData cam, float2 uv)
{
  uv = uv * cam.uv_scale + cam.uv_bias;
  uv = (uv - 0.5f) * cam.fisheye_sensor;

  const float r = length(uv);
  const float r2 = r * r;
  const float4 rr = float4(r, r2, r2 * r, r2 * r2);
  const float theta = -(cam.fisheye_polynomial_bias +
                        dot(cam.fisheye_polynomial_coefficients, rr));

  if (abs(theta) > 0.5f * cam.fisheye_fov) {
    return float3(0.0f);
  }

  float phi = safe_acos(uv.x * safe_rcp(r));
  if (uv.y < 0.0f) {
    phi = -phi;
  }
  return float3(cos(phi) * sin(theta), sin(phi) * sin(theta), -cos(theta));
}

float2 mirror_ball_from_direction(CameraData cam, float3 dir)
{
  dir = normalize(dir);
  dir.z -= 1.0f;
  dir *= safe_rcp(2.0f * safe_sqrt(-0.5f * dir.z));
  float2 uv = 0.5f * dir.xy + 0.5f;
  return (uv - cam.uv_bias) / cam.uv_scale;
}

float3 mirror_ball_to_direction(CameraData cam, float2 uv)
{
  uv = uv * cam.uv_scale + cam.uv_bias;
  float3 dir;
  dir.xy = uv * 2.0f - 1.0f;
  if (length_squared(dir.xy) > 1.0f) {
    return float3(0.0f);
  }
  dir.z = -safe_sqrt(1.0f - square(dir.x) - square(dir.y));
  constexpr float3 I = float3(0.0f, 0.0f, 1.0f);
  return reflect(I, dir);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Regular projections
 * \{ */

float3 view_from_uv(float4x4 projmat, float2 uv)
{
  return project_point(projmat, float3(uv * 2.0f - 1.0f, 0.0f));
}

float2 uv_from_view(float4x4 projmat, bool is_persp, float3 vV)
{
  float4 tmp = projmat * float4(vV, 1.0f);
  if (is_persp && tmp.w <= 0.0f) {
    /* Return invalid coordinates for points behind the camera.
     * This can happen with panoramic projections. */
    return float2(-1.0f);
  }
  return (tmp.xy / tmp.w) * 0.5f + 0.5f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General functions handling all projections
 * \{ */

float3 view_from_uv(CameraData cam, float2 uv)
{
  float3 vV;
  switch (cam.type) {
    default:
    case CAMERA_ORTHO:
    case CAMERA_PERSP:
      return view_from_uv(cam.wininv, uv);
    case CAMERA_PANO_EQUIRECT:
      vV = equirectangular_to_direction(cam, uv);
      break;
    case CAMERA_PANO_EQUIANGULAR_CUBEMAP_FACE:
      vV = equiangular_cubemap_face_to_direction(cam, uv);
      break;
    case CAMERA_PANO_EQUIDISTANT:
      vV = fisheye_to_direction(cam, uv);
      break;
    case CAMERA_PANO_EQUISOLID:
      vV = fisheye_equisolid_to_direction(cam, uv);
      break;
    case CAMERA_PANO_FISHEYE_LENS_POLYNOMIAL:
      vV = fisheye_lens_polynomial_to_direction(cam, uv);
      break;
    case CAMERA_PANO_CENTRAL_CYLINDRICAL:
      vV = central_cylindrical_to_direction(cam, uv);
      break;
    case CAMERA_PANO_MIRROR:
      vV = mirror_ball_to_direction(cam, uv);
      break;
  }
  return vV;
}

float2 uv_from_view(CameraData cam, float3 vV)
{
  switch (cam.type) {
    default:
    case CAMERA_ORTHO:
      return uv_from_view(cam.winmat, false, vV);
    case CAMERA_PERSP:
      return uv_from_view(cam.winmat, true, vV);
    case CAMERA_PANO_EQUIRECT:
      return equirectangular_from_direction(cam, vV);
    case CAMERA_PANO_EQUIANGULAR_CUBEMAP_FACE:
      return equiangular_cubemap_face_from_direction(cam, vV);
    case CAMERA_PANO_EQUISOLID:
      return fisheye_equisolid_from_direction(cam, vV);
    case CAMERA_PANO_FISHEYE_LENS_POLYNOMIAL:
      return fisheye_lens_polynomial_from_direction(cam, vV);
    case CAMERA_PANO_CENTRAL_CYLINDRICAL:
      return central_cylindrical_from_direction(cam, vV);
    case CAMERA_PANO_EQUIDISTANT:
      return fisheye_from_direction(cam, vV);
    case CAMERA_PANO_MIRROR:
      return mirror_ball_from_direction(cam, vV);
  }
}

float2 uv_from_world(CameraData cam, float3 P)
{
  float3 vV = transform_direction(cam.viewmat, normalize(P));
  return uv_from_view(cam, vV);
}

/** \} */

}  // namespace eevee::camera
