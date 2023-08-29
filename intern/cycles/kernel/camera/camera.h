/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/camera/projection.h"
#include "kernel/sample/mapping.h"
#include "kernel/util/differential.h"
#include "kernel/util/lookup_table.h"

CCL_NAMESPACE_BEGIN

/* Perspective Camera */

ccl_device float2 camera_sample_aperture(ccl_constant KernelCamera *cam, const float2 rand)
{
  float blades = cam->blades;
  float2 bokeh;

  if (blades == 0.0f) {
    /* sample disk */
    bokeh = sample_uniform_disk(rand);
  }
  else {
    /* sample polygon */
    float rotation = cam->bladesrotation;
    bokeh = regular_polygon_sample(blades, rotation, rand);
  }

  /* anamorphic lens bokeh */
  bokeh.x *= cam->inv_aperture_ratio;

  return bokeh;
}

ccl_device void camera_sample_perspective(KernelGlobals kg,
                                          const float2 raster_xy,
                                          const float2 rand_lens,
                                          ccl_private Ray *ray)
{
  /* create ray form raster position */
  ProjectionTransform rastertocamera = kernel_data.cam.rastertocamera;
  const float3 raster = float2_to_float3(raster_xy);
  float3 Pcamera = transform_perspective(&rastertocamera, raster);

  if (kernel_data.cam.have_perspective_motion) {
    /* TODO(sergey): Currently we interpolate projected coordinate which
     * gives nice looking result and which is simple, but is in fact a bit
     * different comparing to constructing projective matrix from an
     * interpolated field of view.
     */
    if (ray->time < 0.5f) {
      ProjectionTransform rastertocamera_pre = kernel_data.cam.perspective_pre;
      float3 Pcamera_pre = transform_perspective(&rastertocamera_pre, raster);
      Pcamera = interp(Pcamera_pre, Pcamera, ray->time * 2.0f);
    }
    else {
      ProjectionTransform rastertocamera_post = kernel_data.cam.perspective_post;
      float3 Pcamera_post = transform_perspective(&rastertocamera_post, raster);
      Pcamera = interp(Pcamera, Pcamera_post, (ray->time - 0.5f) * 2.0f);
    }
  }

  float3 P = zero_float3();
  float3 D = Pcamera;

  /* modify ray for depth of field */
  float aperturesize = kernel_data.cam.aperturesize;

  if (aperturesize > 0.0f) {
    /* sample point on aperture */
    float2 lens_uv = camera_sample_aperture(&kernel_data.cam, rand_lens) * aperturesize;

    /* compute point on plane of focus */
    float ft = kernel_data.cam.focaldistance / D.z;
    float3 Pfocus = D * ft;

    /* update ray for effect of lens */
    P = float2_to_float3(lens_uv);
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

  bool use_stereo = kernel_data.cam.interocular_offset != 0.0f;
  if (!use_stereo) {
    /* No stereo */
    ray->P = P;
    ray->D = D;

#ifdef __RAY_DIFFERENTIALS__
    float3 Dcenter = transform_direction(&cameratoworld, Pcamera);
    float3 Dcenter_normalized = normalize(Dcenter);

    /* TODO: can this be optimized to give compact differentials directly? */
    ray->dP = differential_zero_compact();
    differential3 dD;
    dD.dx = normalize(Dcenter + float4_to_float3(kernel_data.cam.dx)) - Dcenter_normalized;
    dD.dy = normalize(Dcenter + float4_to_float3(kernel_data.cam.dy)) - Dcenter_normalized;
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
    float3 Pnostereo = transform_point(&cameratoworld, zero_float3());

    float3 Pcenter = Pnostereo;
    float3 Dcenter = Pcamera;
    Dcenter = normalize(transform_direction(&cameratoworld, Dcenter));
    spherical_stereo_transform(&kernel_data.cam, &Pcenter, &Dcenter);

    float3 Px = Pnostereo;
    float3 Dx = transform_perspective(&rastertocamera,
                                      make_float3(raster.x + 1.0f, raster.y, 0.0f));
    Dx = normalize(transform_direction(&cameratoworld, Dx));
    spherical_stereo_transform(&kernel_data.cam, &Px, &Dx);

    differential3 dP, dD;

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
  float z_inv = 1.0f / normalize(Pcamera).z;
  float nearclip = kernel_data.cam.nearclip * z_inv;
  ray->P += nearclip * ray->D;
  ray->dP += nearclip * ray->dD;
  ray->tmin = 0.0f;
  ray->tmax = kernel_data.cam.cliplength * z_inv;
}

/* Orthographic Camera */
ccl_device void camera_sample_orthographic(KernelGlobals kg,
                                           const float2 raster_xy,
                                           const float2 rand_lens,
                                           ccl_private Ray *ray)
{
  /* create ray form raster position */
  ProjectionTransform rastertocamera = kernel_data.cam.rastertocamera;
  float3 Pcamera = transform_perspective(&rastertocamera, float2_to_float3(raster_xy));

  float3 P;
  float3 D = make_float3(0.0f, 0.0f, 1.0f);

  /* modify ray for depth of field */
  float aperturesize = kernel_data.cam.aperturesize;

  if (aperturesize > 0.0f) {
    /* sample point on aperture */
    float2 lens_uv = camera_sample_aperture(&kernel_data.cam, rand_lens) * aperturesize;

    /* compute point on plane of focus */
    float3 Pfocus = D * kernel_data.cam.focaldistance;

    /* update ray for effect of lens */
    float3 lens_uvw = float2_to_float3(lens_uv);
    P = Pcamera + lens_uvw;
    D = normalize(Pfocus - lens_uvw);
  }
  else {
    P = Pcamera;
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
  dP.dx = float4_to_float3(kernel_data.cam.dx);
  dP.dy = float4_to_float3(kernel_data.cam.dx);

  ray->dP = differential_make_compact(dP);
  ray->dD = differential_zero_compact();
#endif

  /* clipping */
  ray->tmin = 0.0f;
  ray->tmax = kernel_data.cam.cliplength;
}

/* Panorama Camera */

ccl_device_inline void camera_sample_panorama(ccl_constant KernelCamera *cam,
                                              ccl_global const DecomposedTransform *cam_motion,
                                              const float2 raster,
                                              const float2 rand_lens,
                                              ccl_private Ray *ray)
{
  ProjectionTransform rastertocamera = cam->rastertocamera;
  float3 Pcamera = transform_perspective(&rastertocamera, float2_to_float3(raster));

  /* create ray form raster position */
  float3 P = zero_float3();
  float3 D = panorama_to_direction(cam, Pcamera.x, Pcamera.y);

  /* indicates ray should not receive any light, outside of the lens */
  if (is_zero(D)) {
    ray->tmax = 0.0f;
    return;
  }

  /* modify ray for depth of field */
  float aperturesize = cam->aperturesize;

  if (aperturesize > 0.0f) {
    /* sample point on aperture */
    float2 lens_uv = camera_sample_aperture(cam, rand_lens) * aperturesize;

    /* compute point on plane of focus */
    float3 Dfocus = normalize(D);
    float3 Pfocus = Dfocus * cam->focaldistance;

    /* calculate orthonormal coordinates perpendicular to Dfocus */
    float3 U, V;
    U = normalize(make_float3(1.0f, 0.0f, 0.0f) - Dfocus.x * Dfocus);
    V = normalize(cross(Dfocus, U));

    /* update ray for effect of lens */
    P = U * lens_uv.x + V * lens_uv.y;
    D = normalize(Pfocus - P);
  }

  /* transform ray from camera to world */
  Transform cameratoworld = cam->cameratoworld;

  if (cam->num_motion_steps) {
    transform_motion_array_interpolate(
        &cameratoworld, cam_motion, cam->num_motion_steps, ray->time);
  }

  /* Stereo transform */
  bool use_stereo = cam->interocular_offset != 0.0f;
  if (use_stereo) {
    spherical_stereo_transform(cam, &P, &D);
  }

  P = transform_point(&cameratoworld, P);
  D = normalize(transform_direction(&cameratoworld, D));

  ray->P = P;
  ray->D = D;

#ifdef __RAY_DIFFERENTIALS__
  /* Ray differentials, computed from scratch using the raster coordinates
   * because we don't want to be affected by depth of field. We compute
   * ray origin and direction for the center and two neighboring pixels
   * and simply take their differences. */
  float3 Pcenter = Pcamera;
  float3 Dcenter = panorama_to_direction(cam, Pcenter.x, Pcenter.y);
  if (use_stereo) {
    spherical_stereo_transform(cam, &Pcenter, &Dcenter);
  }
  Pcenter = transform_point(&cameratoworld, Pcenter);
  Dcenter = normalize(transform_direction(&cameratoworld, Dcenter));

  float3 Px = transform_perspective(&rastertocamera, make_float3(raster.x + 1.0f, raster.y, 0.0f));
  float3 Dx = panorama_to_direction(cam, Px.x, Px.y);
  if (use_stereo) {
    spherical_stereo_transform(cam, &Px, &Dx);
  }
  Px = transform_point(&cameratoworld, Px);
  Dx = normalize(transform_direction(&cameratoworld, Dx));

  differential3 dP, dD;
  dP.dx = Px - Pcenter;
  dD.dx = Dx - Dcenter;

  float3 Py = transform_perspective(&rastertocamera, make_float3(raster.x, raster.y + 1.0f, 0.0f));
  float3 Dy = panorama_to_direction(cam, Py.x, Py.y);
  if (use_stereo) {
    spherical_stereo_transform(cam, &Py, &Dy);
  }
  Py = transform_point(&cameratoworld, Py);
  Dy = normalize(transform_direction(&cameratoworld, Dy));

  dP.dy = Py - Pcenter;
  dD.dy = Dy - Dcenter;
  ray->dD = differential_make_compact(dD);
  ray->dP = differential_make_compact(dP);
#endif

  /* clipping */
  float nearclip = cam->nearclip;
  ray->P += nearclip * ray->D;
  ray->dP += nearclip * ray->dD;
  ray->tmin = 0.0f;
  ray->tmax = cam->cliplength;
}

/* Common */

ccl_device_inline void camera_sample(KernelGlobals kg,
                                     int x,
                                     int y,
                                     const float2 filter_uv,
                                     const float time,
                                     const float2 lens_uv,
                                     ccl_private Ray *ray)
{
  /* pixel filter */
  int filter_table_offset = kernel_data.tables.filter_table_offset;
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
    camera_sample_perspective(kg, raster, lens_uv, ray);
  }
  else if (kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    camera_sample_orthographic(kg, raster, lens_uv, ray);
  }
  else {
    ccl_global const DecomposedTransform *cam_motion = kernel_data_array(camera_motion);
    camera_sample_panorama(&kernel_data.cam, cam_motion, raster, lens_uv, ray);
  }
}

/* Utilities */

ccl_device_inline float3 camera_position(KernelGlobals kg)
{
  Transform cameratoworld = kernel_data.cam.cameratoworld;
  return make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);
}

ccl_device_inline float camera_distance(KernelGlobals kg, float3 P)
{
  Transform cameratoworld = kernel_data.cam.cameratoworld;
  float3 camP = make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);

  if (kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    float3 camD = make_float3(cameratoworld.x.z, cameratoworld.y.z, cameratoworld.z.z);
    return fabsf(dot((P - camP), camD));
  }
  else {
    return len(P - camP);
  }
}

ccl_device_inline float camera_z_depth(KernelGlobals kg, float3 P)
{
  if (kernel_data.cam.type != CAMERA_PANORAMA) {
    Transform worldtocamera = kernel_data.cam.worldtocamera;
    return transform_point(&worldtocamera, P).z;
  }
  else {
    Transform cameratoworld = kernel_data.cam.cameratoworld;
    float3 camP = make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);
    return len(P - camP);
  }
}

ccl_device_inline float3 camera_direction_from_point(KernelGlobals kg, float3 P)
{
  Transform cameratoworld = kernel_data.cam.cameratoworld;

  if (kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    float3 camD = make_float3(cameratoworld.x.z, cameratoworld.y.z, cameratoworld.z.z);
    return -camD;
  }
  else {
    float3 camP = make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);
    return normalize(camP - P);
  }
}

ccl_device_inline float3 camera_world_to_ndc(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             float3 P)
{
  if (kernel_data.cam.type != CAMERA_PANORAMA) {
    /* perspective / ortho */
    if (sd->object == PRIM_NONE && kernel_data.cam.type == CAMERA_PERSPECTIVE)
      P += camera_position(kg);

    ProjectionTransform tfm = kernel_data.cam.worldtondc;
    return transform_perspective(&tfm, P);
  }
  else {
    /* panorama */
    Transform tfm = kernel_data.cam.worldtocamera;

    if (sd->object != OBJECT_NONE)
      P = normalize(transform_point(&tfm, P));
    else
      P = normalize(transform_direction(&tfm, P));

    float2 uv = direction_to_panorama(&kernel_data.cam, P);

    return make_float3(uv.x, uv.y, 0.0f);
  }
}

CCL_NAMESPACE_END
