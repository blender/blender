/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Camera projection / uv functions and utils.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Panoramic Projections
 *
 * Adapted from Cycles to match EEVEE's coordinate system.
 * \{ */

vec2 camera_equirectangular_from_direction(CameraData cam, vec3 dir)
{
  float phi = atan(-dir.z, dir.x);
  float theta = acos(dir.y / length(dir));
  return (vec2(phi, theta) - cam.equirect_bias) * cam.equirect_scale_inv;
}

vec3 camera_equirectangular_to_direction(CameraData cam, vec2 uv)
{
  uv = uv * cam.equirect_scale + cam.equirect_bias;
  float phi = uv.x;
  float theta = uv.y;
  float sin_theta = sin(theta);
  return vec3(sin_theta * cos(phi), cos(theta), -sin_theta * sin(phi));
}

vec2 camera_fisheye_from_direction(CameraData cam, vec3 dir)
{
  float r = atan(length(dir.xy), -dir.z) / cam.fisheye_fov;
  float phi = atan(dir.y, dir.x);
  vec2 uv = r * vec2(cos(phi), sin(phi)) + 0.5;
  return (uv - cam.uv_bias) / cam.uv_scale;
}

vec3 camera_fisheye_to_direction(CameraData cam, vec2 uv)
{
  uv = uv * cam.uv_scale + cam.uv_bias;
  uv = (uv - 0.5) * 2.0;
  float r = length(uv);
  if (r > 1.0) {
    return vec3(0.0);
  }
  float phi = safe_acos(uv.x * safe_rcp(r));
  float theta = r * cam.fisheye_fov * 0.5;
  if (uv.y < 0.0) {
    phi = -phi;
  }
  return vec3(cos(phi) * sin(theta), sin(phi) * sin(theta), -cos(theta));
}

vec2 camera_mirror_ball_from_direction(CameraData cam, vec3 dir)
{
  dir = normalize(dir);
  dir.z -= 1.0;
  dir *= safe_rcp(2.0 * safe_sqrt(-0.5 * dir.z));
  vec2 uv = 0.5 * dir.xy + 0.5;
  return (uv - cam.uv_bias) / cam.uv_scale;
}

vec3 camera_mirror_ball_to_direction(CameraData cam, vec2 uv)
{
  uv = uv * cam.uv_scale + cam.uv_bias;
  vec3 dir;
  dir.xy = uv * 2.0 - 1.0;
  if (len_squared(dir.xy) > 1.0) {
    return vec3(0.0);
  }
  dir.z = -safe_sqrt(1.0 - sqr(dir.x) - sqr(dir.y));
  const vec3 I = vec3(0.0, 0.0, 1.0);
  return reflect(I, dir);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Regular projections
 * \{ */

vec3 camera_view_from_uv(mat4 projmat, vec2 uv)
{
  return project_point(projmat, vec3(uv * 2.0 - 1.0, 0.0));
}

vec2 camera_uv_from_view(mat4 projmat, bool is_persp, vec3 vV)
{
  vec4 tmp = projmat * vec4(vV, 1.0);
  if (is_persp && tmp.w <= 0.0) {
    /* Return invalid coordinates for points behind the camera.
     * This can happen with panoramic projections. */
    return vec2(-1.0);
  }
  return (tmp.xy / tmp.w) * 0.5 + 0.5;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General functions handling all projections
 * \{ */

vec3 camera_view_from_uv(CameraData cam, vec2 uv)
{
  vec3 vV;
  switch (cam.type) {
    default:
    case CAMERA_ORTHO:
    case CAMERA_PERSP:
      return camera_view_from_uv(cam.wininv, uv);
    case CAMERA_PANO_EQUIRECT:
      vV = camera_equirectangular_to_direction(cam, uv);
      break;
    case CAMERA_PANO_EQUIDISTANT:
      /* ATTR_FALLTHROUGH; */
    case CAMERA_PANO_EQUISOLID:
      vV = camera_fisheye_to_direction(cam, uv);
      break;
    case CAMERA_PANO_MIRROR:
      vV = camera_mirror_ball_to_direction(cam, uv);
      break;
  }
  return vV;
}

vec2 camera_uv_from_view(CameraData cam, vec3 vV)
{
  switch (cam.type) {
    default:
    case CAMERA_ORTHO:
      return camera_uv_from_view(cam.winmat, false, vV);
    case CAMERA_PERSP:
      return camera_uv_from_view(cam.winmat, true, vV);
    case CAMERA_PANO_EQUIRECT:
      return camera_equirectangular_from_direction(cam, vV);
    case CAMERA_PANO_EQUISOLID:
      /* ATTR_FALLTHROUGH; */
    case CAMERA_PANO_EQUIDISTANT:
      return camera_fisheye_from_direction(cam, vV);
    case CAMERA_PANO_MIRROR:
      return camera_mirror_ball_from_direction(cam, vV);
  }
}

vec2 camera_uv_from_world(CameraData cam, vec3 P)
{
  vec3 vV = transform_direction(cam.viewmat, normalize(P));
  return camera_uv_from_view(cam, vV);
}

/** \} */
