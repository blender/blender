/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/camera/projection.h"
#include "kernel/sample/mapping.h"
#include "kernel/util/differential.h"
#include "kernel/util/lookup_table.h"

#ifdef WITH_OSL
#  include "kernel/osl/camera.h"
#endif

CCL_NAMESPACE_BEGIN

/* Perspective Camera */

ccl_device float2 camera_sample_aperture(ccl_constant KernelCamera *cam, const float2 rand)
{
  const float blades = cam->blades;
  float2 bokeh;

  if (blades == 0.0f) {
    /* sample disk */
    bokeh = sample_uniform_disk(rand);
  }
  else {
    /* sample polygon */
    const float rotation = cam->bladesrotation;
    bokeh = regular_polygon_sample(blades, rotation, rand);
  }

  /* anamorphic lens bokeh */
  bokeh.x *= cam->inv_aperture_ratio;

  return bokeh;
}

ccl_device Spectrum camera_sample_perspective(KernelGlobals kg,
                                              const float2 raster_xy,
                                              const float2 rand_lens,
                                              ccl_private Ray *ray)
{
  /* create ray form raster position */
  const ProjectionTransform rastertocamera = kernel_data.cam.rastertocamera;
  const float3 raster = make_float3(raster_xy);
  float3 Pcamera = transform_perspective(&rastertocamera, raster);

  if (kernel_data.cam.have_perspective_motion) {
    /* TODO(sergey): Currently we interpolate projected coordinate which
     * gives nice looking result and which is simple, but is in fact a bit
     * different comparing to constructing projective matrix from an
     * interpolated field of view.
     */
    if (ray->time < 0.5f) {
      const ProjectionTransform rastertocamera_pre = kernel_data.cam.perspective_pre;
      const float3 Pcamera_pre = transform_perspective(&rastertocamera_pre, raster);
      Pcamera = interp(Pcamera_pre, Pcamera, ray->time * 2.0f);
    }
    else {
      const ProjectionTransform rastertocamera_post = kernel_data.cam.perspective_post;
      const float3 Pcamera_post = transform_perspective(&rastertocamera_post, raster);
      Pcamera = interp(Pcamera, Pcamera_post, (ray->time - 0.5f) * 2.0f);
    }
  }

  float3 P = zero_float3();
  float3 D = Pcamera;

  /* modify ray for depth of field */
  const float aperturesize = kernel_data.cam.aperturesize;

  if (aperturesize > 0.0f) {
    /* sample point on aperture */
    const float2 lens_uv = camera_sample_aperture(&kernel_data.cam, rand_lens) * aperturesize;

    /* compute point on plane of focus */
    const float ft = kernel_data.cam.focaldistance / D.z;
    const float3 Pfocus = D * ft;

    /* update ray for effect of lens */
    P = make_float3(lens_uv);
    D = normalize(Pfocus - P);
  }

  /* transform ray from camera to world */
  Transform cameratoworld = kernel_data.cam.cameratoworld;

  if (kernel_data.cam.num_motion_steps) {
    transform_motion_array_interpolate(&cameratoworld,
                                       kernel_data_array(camera_motion),
                                       kernel_data.cam.num_motion_steps,
                                       ray->time);
  }

  P = transform_point(&cameratoworld, P);
  D = normalize(transform_direction(&cameratoworld, D));

  const bool use_stereo = kernel_data.cam.interocular_offset != 0.0f;
  if (!use_stereo) {
    /* No stereo */
    ray->P = P;
    ray->D = D;

#ifdef __RAY_DIFFERENTIALS__
    const float3 Dcenter = transform_direction(&cameratoworld, Pcamera);
    const float3 Dcenter_normalized = normalize(Dcenter);

    /* TODO: can this be optimized to give compact differentials directly? */
    ray->dP = differential_zero_compact();
    differential3 dD;
    dD.dx = normalize(Dcenter + make_float3(kernel_data.cam.dx)) - Dcenter_normalized;
    dD.dy = normalize(Dcenter + make_float3(kernel_data.cam.dy)) - Dcenter_normalized;
    ray->dD = differential_make_compact(dD);
#endif
  }
  else {
    /* Spherical stereo */
    spherical_stereo_transform(&kernel_data.cam, &P, &D);
    ray->P = P;
    ray->D = D;

#ifdef __RAY_DIFFERENTIALS__
    /* Ray differentials, computed from scratch using the raster coordinates
     * because we don't want to be affected by depth of field. We compute
     * ray origin and direction for the center and two neighboring pixels
     * and simply take their differences. */
    const float3 Pnostereo = transform_point(&cameratoworld, zero_float3());

    float3 Pcenter = Pnostereo;
    float3 Dcenter = Pcamera;
    Dcenter = normalize(transform_direction(&cameratoworld, Dcenter));
    spherical_stereo_transform(&kernel_data.cam, &Pcenter, &Dcenter);

    float3 Px = Pnostereo;
    float3 Dx = transform_perspective(&rastertocamera,
                                      make_float3(raster.x + 1.0f, raster.y, 0.0f));
    Dx = normalize(transform_direction(&cameratoworld, Dx));
    spherical_stereo_transform(&kernel_data.cam, &Px, &Dx);

    differential3 dP;
    differential3 dD;

    dP.dx = Px - Pcenter;
    dD.dx = Dx - Dcenter;

    float3 Py = Pnostereo;
    float3 Dy = transform_perspective(&rastertocamera,
                                      make_float3(raster.x, raster.y + 1.0f, 0.0f));
    Dy = normalize(transform_direction(&cameratoworld, Dy));
    spherical_stereo_transform(&kernel_data.cam, &Py, &Dy);

    dP.dy = Py - Pcenter;
    dD.dy = Dy - Dcenter;
    ray->dD = differential_make_compact(dD);
    ray->dP = differential_make_compact(dP);
#endif
  }

  /* clipping */
  const float z_inv = 1.0f / normalize(Pcamera).z;
  const float nearclip = kernel_data.cam.nearclip * z_inv;
  ray->P += nearclip * ray->D;
  ray->dP += nearclip * ray->dD;
  ray->tmin = 0.0f;
  ray->tmax = kernel_data.cam.cliplength * z_inv;

  return one_spectrum();
}

/* Orthographic Camera */
ccl_device Spectrum camera_sample_orthographic(KernelGlobals kg,
                                               const float2 raster_xy,
                                               const float2 rand_lens,
                                               ccl_private Ray *ray)
{
  /* create ray form raster position */
  const ProjectionTransform rastertocamera = kernel_data.cam.rastertocamera;
  const float3 Pcamera = transform_perspective(&rastertocamera, make_float3(raster_xy));

  float3 P;
  float3 D = make_float3(0.0f, 0.0f, 1.0f);

  /* modify ray for depth of field */
  const float aperturesize = kernel_data.cam.aperturesize;

  if (aperturesize > 0.0f) {
    /* sample point on aperture */
    const float2 lens_uv = camera_sample_aperture(&kernel_data.cam, rand_lens) * aperturesize;

    /* compute point on plane of focus */
    const float3 Pfocus = D * kernel_data.cam.focaldistance;

    /* Update ray for effect of lens */
    const float3 lens_uvw = make_float3(lens_uv);

    D = normalize(Pfocus - lens_uvw);
    /* Compute position the ray will be if it traveled until it intersected the near clip plane.
     * This allows for correct DOF while allowing near clipping. */
    P = Pcamera + lens_uvw + (D * (kernel_data.cam.nearclip / D.z));
  }
  else {
    P = Pcamera + make_float3(0.0f, 0.0f, kernel_data.cam.nearclip);
  }
  /* transform ray from camera to world */
  Transform cameratoworld = kernel_data.cam.cameratoworld;

  if (kernel_data.cam.num_motion_steps) {
    transform_motion_array_interpolate(&cameratoworld,
                                       kernel_data_array(camera_motion),
                                       kernel_data.cam.num_motion_steps,
                                       ray->time);
  }

  ray->P = transform_point(&cameratoworld, P);
  ray->D = normalize(transform_direction(&cameratoworld, D));

#ifdef __RAY_DIFFERENTIALS__
  /* ray differential */
  differential3 dP;
  dP.dx = make_float3(kernel_data.cam.dx);
  dP.dy = make_float3(kernel_data.cam.dx);

  ray->dP = differential_make_compact(dP);
  ray->dD = differential_zero_compact();
#endif

  /* clipping */
  ray->tmin = 0.0f;
  ray->tmax = kernel_data.cam.cliplength;

  return one_spectrum();
}

/* Custom Camera */

ccl_device_inline void camera_sample_to_ray(ccl_constant KernelCamera *cam,
                                            const ccl_global DecomposedTransform *cam_motion,
                                            float3 P,
                                            float3 D,
#ifdef __RAY_DIFFERENTIALS__
                                            float3 Pcenter,
                                            float3 Dcenter,
                                            float3 Px,
                                            float3 Dx,
                                            float3 Py,
                                            float3 Dy,
#endif
                                            ccl_private Ray *ray)
{
  /* Transform the ray from camera to world. */
  Transform cameratoworld = cam->cameratoworld;

  if (cam->num_motion_steps) {
    transform_motion_array_interpolate(
        &cameratoworld, cam_motion, cam->num_motion_steps, ray->time);
  }

  /* Stereo transform */
  const bool use_stereo = cam->interocular_offset != 0.0f;
  if (use_stereo) {
    spherical_stereo_transform(cam, &P, &D);
  }

  P = transform_point(&cameratoworld, P);
  D = normalize(transform_direction(&cameratoworld, D));

  ray->P = P;
  ray->D = D;

#ifdef __RAY_DIFFERENTIALS__
  if (use_stereo) {
    spherical_stereo_transform(cam, &Pcenter, &Dcenter);
    spherical_stereo_transform(cam, &Px, &Dx);
    spherical_stereo_transform(cam, &Py, &Dy);

    differential3 dP;
    Pcenter = transform_point(&cameratoworld, Pcenter);
    dP.dx = transform_point(&cameratoworld, Px) - Pcenter;
    dP.dy = transform_point(&cameratoworld, Py) - Pcenter;
    ray->dP = differential_make_compact(dP);
  }
  else {
    ray->dP = differential_zero_compact();
  }

  differential3 dD;
  Dcenter = normalize(transform_direction(&cameratoworld, Dcenter));
  dD.dx = normalize(transform_direction(&cameratoworld, Dx)) - Dcenter;
  dD.dy = normalize(transform_direction(&cameratoworld, Dy)) - Dcenter;
  ray->dD = differential_make_compact(dD);
#endif

  /* clipping */
  const float nearclip = cam->nearclip;
  ray->P += nearclip * ray->D;
  ray->dP += nearclip * ray->dD;
  ray->tmin = 0.0f;
  ray->tmax = cam->cliplength;
}

ccl_device_inline Spectrum camera_sample_custom(KernelGlobals kg,
                                                ccl_constant KernelCamera *cam,
                                                const ccl_global DecomposedTransform *cam_motion,
                                                const float2 raster,
                                                const float2 rand_lens,
                                                ccl_private Ray *ray)
{
#ifdef WITH_OSL
  /* Transform raster position to camera space. */
  const ProjectionTransform rastertocamera = cam->rastertocamera;
  float3 sensor = transform_perspective(&rastertocamera, make_float3(raster.x, raster.y, 0.0f));
  float3 dSdx = transform_perspective_direction(&rastertocamera, make_float3(1.0f, 0.0f, 0.0f));
  float3 dSdy = transform_perspective_direction(&rastertocamera, make_float3(0.0f, 1.0f, 0.0f));
  /* Execute OSL shader to sample position, direction and transmission. */
  packed_float3 P, dPdx, dPdy, D, dDdx, dDdy, throughput;
  throughput = osl_eval_camera(kg, sensor, dSdx, dSdy, rand_lens, P, dPdx, dPdy, D, dDdx, dDdy);
  /* Zero throughput indicates failed sampling. */
  if (is_zero(throughput)) {
    return zero_spectrum();
  }

  camera_sample_to_ray(cam,
                       cam_motion,
                       P,
                       D,
#  ifdef __RAY_DIFFERENTIALS__
                       P,
                       D,
                       P + dPdx,
                       D + dDdx,
                       P + dPdy,
                       D + dDdy,
#  endif
                       ray);

  return throughput;
#else
  (void)kg;
  (void)cam;
  (void)cam_motion;
  (void)raster;
  (void)rand_lens;
  (void)ray;
  return zero_spectrum();
#endif
}

/* Panorama Camera */

ccl_device_inline float3 camera_panorama_direction(ccl_constant KernelCamera *cam,
                                                   const float x,
                                                   const float y)
{
  const ProjectionTransform rastertocamera = cam->rastertocamera;
  const float3 Pcamera = transform_perspective(&rastertocamera, make_float3(x, y, 0.0f));
  return panorama_to_direction(cam, Pcamera.x, Pcamera.y);
}

ccl_device_inline Spectrum camera_sample_panorama(ccl_constant KernelCamera *cam,
                                                  const ccl_global DecomposedTransform *cam_motion,
                                                  const float2 raster,
                                                  const float2 rand_lens,
                                                  ccl_private Ray *ray)
{
  /* Create ray from raster position. */
  float3 P = zero_float3();
  float3 D = camera_panorama_direction(cam, raster.x, raster.y);

#ifdef __RAY_DIFFERENTIALS__
  /* Ray differentials, computed from scratch using the raster coordinates
   * because we don't want to be affected by depth of field. We compute
   * ray origin and direction for the center and two neighboring pixels
   * and simply take their differences. */
  float3 Dcenter = D;
  float3 Dx = camera_panorama_direction(cam, raster.x + 1.0f, raster.y);
  float3 Dy = camera_panorama_direction(cam, raster.x, raster.y + 1.0f);
#endif

  /* Here, zero indicates failed sampling, e.g. when the raster position is outside
   * the fisheye lens. */
  if (is_zero(D)) {
    return zero_spectrum();
  }

  /* Perform depth-of-field sampling. */
  const float aperturesize = cam->aperturesize;

  if (aperturesize > 0.0f) {
    /* Sample a point on the aperture. */
    const float2 lens_uv = camera_sample_aperture(cam, rand_lens) * aperturesize;

    /* Compute the intersection of the original ray with the focal plane. */
    const float3 Dfocus = normalize(D);
    const float3 Pfocus = Dfocus * cam->focaldistance;

    /* Calculate orthonormal coordinate system perpendicular to Dfocus. */
    const float3 U = normalize(make_float3(1.0f, 0.0f, 0.0f) - Dfocus.x * Dfocus);
    const float3 V = normalize(cross(Dfocus, U));

    /* Compute new ray by shifting its origin (to account for aperture position) and
     * setting its direction to meet the original ray at the focal plane. */
    P = U * lens_uv.x + V * lens_uv.y;
    D = normalize(Pfocus - P);
  }

  camera_sample_to_ray(cam,
                       cam_motion,
                       P,
                       D,
#ifdef __RAY_DIFFERENTIALS__
                       zero_float3(),
                       Dcenter,
                       zero_float3(),
                       Dx,
                       zero_float3(),
                       Dy,
#endif
                       ray);

  return one_spectrum();
}

/* Common */

/* Generates an outgoing camera ray for the given raster position and random inputs.
 * Returns camera sensitivity (used to initialize path throughput). */
ccl_device_inline Spectrum camera_sample(KernelGlobals kg,
                                         const int x,
                                         const int y,
                                         const float2 filter_uv,
                                         const float time,
                                         const float2 lens_uv,
                                         ccl_private Ray *ray)
{
  /* pixel filter */
  const int filter_table_offset = kernel_data.tables.filter_table_offset;
  const float2 raster = make_float2(
      x + lookup_table_read(kg, filter_uv.x, filter_table_offset, FILTER_TABLE_SIZE),
      y + lookup_table_read(kg, filter_uv.y, filter_table_offset, FILTER_TABLE_SIZE));

  /* motion blur */
  if (kernel_data.cam.shuttertime == -1.0f) {
    ray->time = 0.5f;
  }
  else {
    /* TODO(sergey): Such lookup is unneeded when there's rolling shutter
     * effect in use but rolling shutter duration is set to 0.0.
     */
    const int shutter_table_offset = kernel_data.cam.shutter_table_offset;
    ray->time = lookup_table_read(kg, time, shutter_table_offset, SHUTTER_TABLE_SIZE);
    /* TODO(sergey): Currently single rolling shutter effect type only
     * where scan-lines are acquired from top to bottom and whole scan-line
     * is acquired at once (no delay in acquisition happens between pixels
     * of single scan-line).
     *
     * Might want to support more models in the future.
     */
    if (kernel_data.cam.rolling_shutter_type) {
      /* Time corresponding to a fully rolling shutter only effect:
       * top of the frame is time 0.0, bottom of the frame is time 1.0.
       */
      const float time = 1.0f - (float)y / kernel_data.cam.height;
      const float duration = kernel_data.cam.rolling_shutter_duration;
      if (duration != 0.0f) {
        /* This isn't fully physical correct, but lets us to have simple
         * controls in the interface. The idea here is basically sort of
         * linear interpolation between how much rolling shutter effect
         * exist on the frame and how much of it is a motion blur effect.
         */
        ray->time = (ray->time - 0.5f) * duration;
        ray->time += (time - 0.5f) * (1.0f - duration) + 0.5f;
      }
      else {
        ray->time = time;
      }
    }
  }

  /* sample */
  if (kernel_data.cam.type == CAMERA_PERSPECTIVE) {
    return camera_sample_perspective(kg, raster, lens_uv, ray);
  }
  if (kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    return camera_sample_orthographic(kg, raster, lens_uv, ray);
  }
  if (kernel_data.cam.type == CAMERA_PANORAMA) {
    const ccl_global DecomposedTransform *cam_motion = kernel_data_array(camera_motion);
    return camera_sample_panorama(&kernel_data.cam, cam_motion, raster, lens_uv, ray);
  }
  if (kernel_data.cam.type == CAMERA_CUSTOM) {
    const ccl_global DecomposedTransform *cam_motion = kernel_data_array(camera_motion);
    return camera_sample_custom(kg, &kernel_data.cam, cam_motion, raster, lens_uv, ray);
  }
  kernel_assert(false);
  return zero_spectrum();
}

/* Utilities */

ccl_device_inline float3 camera_position(KernelGlobals kg)
{
  const Transform cameratoworld = kernel_data.cam.cameratoworld;
  return make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);
}

ccl_device_inline float camera_distance(KernelGlobals kg, const float3 P)
{
  const Transform cameratoworld = kernel_data.cam.cameratoworld;
  const float3 camP = make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);

  if (kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    const float3 camD = make_float3(cameratoworld.x.z, cameratoworld.y.z, cameratoworld.z.z);
    return fabsf(dot((P - camP), camD));
  }
  return len(P - camP);
}

ccl_device_inline float camera_z_depth(KernelGlobals kg, const float3 P)
{
  if (kernel_data.cam.type == CAMERA_PERSPECTIVE || kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    const Transform worldtocamera = kernel_data.cam.worldtocamera;
    return transform_point(&worldtocamera, P).z;
  }
  const Transform cameratoworld = kernel_data.cam.cameratoworld;
  const float3 camP = make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);
  return len(P - camP);
}

ccl_device_inline float3 camera_direction_from_point(KernelGlobals kg, const float3 P)
{
  const Transform cameratoworld = kernel_data.cam.cameratoworld;

  if (kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    const float3 camD = make_float3(cameratoworld.x.z, cameratoworld.y.z, cameratoworld.z.z);
    return -camD;
  }
  const float3 camP = make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);
  return normalize(camP - P);
}

ccl_device_inline float3 camera_world_to_ndc(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             float3 P)
{
  if (kernel_data.cam.type == CAMERA_PERSPECTIVE || kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    /* perspective / ortho */
    if (sd->object == PRIM_NONE && kernel_data.cam.type == CAMERA_PERSPECTIVE) {
      P += camera_position(kg);
    }

    const ProjectionTransform tfm = kernel_data.cam.worldtondc;
    return transform_perspective(&tfm, P);
  }
  /* panorama or custom */
  const Transform tfm = kernel_data.cam.worldtocamera;

  if (sd->object != OBJECT_NONE) {
    P = normalize(transform_point(&tfm, P));
  }
  else {
    P = normalize(transform_direction(&tfm, P));
  }

  if (kernel_data.cam.type == CAMERA_PANORAMA) {
    return make_float3(direction_to_panorama(&kernel_data.cam, P));
  }
  /* TODO: Fall back to camera coordinates until we have inverse mappings for custom cameras. */
  return P;
}

CCL_NAMESPACE_END
