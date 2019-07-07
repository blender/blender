/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* Perspective Camera */

ccl_device float2 camera_sample_aperture(ccl_constant KernelCamera *cam, float u, float v)
{
  float blades = cam->blades;
  float2 bokeh;

  if (blades == 0.0f) {
    /* sample disk */
    bokeh = concentric_sample_disk(u, v);
  }
  else {
    /* sample polygon */
    float rotation = cam->bladesrotation;
    bokeh = regular_polygon_sample(blades, rotation, u, v);
  }

  /* anamorphic lens bokeh */
  bokeh.x *= cam->inv_aperture_ratio;

  return bokeh;
}

ccl_device void camera_sample_perspective(KernelGlobals *kg,
                                          float raster_x,
                                          float raster_y,
                                          float lens_u,
                                          float lens_v,
                                          ccl_addr_space Ray *ray)
{
  /* create ray form raster position */
  ProjectionTransform rastertocamera = kernel_data.cam.rastertocamera;
  float3 raster = make_float3(raster_x, raster_y, 0.0f);
  float3 Pcamera = transform_perspective(&rastertocamera, raster);

#ifdef __CAMERA_MOTION__
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
#endif

  float3 P = make_float3(0.0f, 0.0f, 0.0f);
  float3 D = Pcamera;

  /* modify ray for depth of field */
  float aperturesize = kernel_data.cam.aperturesize;

  if (aperturesize > 0.0f) {
    /* sample point on aperture */
    float2 lensuv = camera_sample_aperture(&kernel_data.cam, lens_u, lens_v) * aperturesize;

    /* compute point on plane of focus */
    float ft = kernel_data.cam.focaldistance / D.z;
    float3 Pfocus = D * ft;

    /* update ray for effect of lens */
    P = make_float3(lensuv.x, lensuv.y, 0.0f);
    D = normalize(Pfocus - P);
  }

  /* transform ray from camera to world */
  Transform cameratoworld = kernel_data.cam.cameratoworld;

#ifdef __CAMERA_MOTION__
  if (kernel_data.cam.num_motion_steps) {
    transform_motion_array_interpolate(&cameratoworld,
                                       kernel_tex_array(__camera_motion),
                                       kernel_data.cam.num_motion_steps,
                                       ray->time);
  }
#endif

  P = transform_point(&cameratoworld, P);
  D = normalize(transform_direction(&cameratoworld, D));

  bool use_stereo = kernel_data.cam.interocular_offset != 0.0f;
  if (!use_stereo) {
    /* No stereo */
    ray->P = P;
    ray->D = D;

#ifdef __RAY_DIFFERENTIALS__
    float3 Dcenter = transform_direction(&cameratoworld, Pcamera);

    ray->dP = differential3_zero();
    ray->dD.dx = normalize(Dcenter + float4_to_float3(kernel_data.cam.dx)) - normalize(Dcenter);
    ray->dD.dy = normalize(Dcenter + float4_to_float3(kernel_data.cam.dy)) - normalize(Dcenter);
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
    float3 Pnostereo = transform_point(&cameratoworld, make_float3(0.0f, 0.0f, 0.0f));

    float3 Pcenter = Pnostereo;
    float3 Dcenter = Pcamera;
    Dcenter = normalize(transform_direction(&cameratoworld, Dcenter));
    spherical_stereo_transform(&kernel_data.cam, &Pcenter, &Dcenter);

    float3 Px = Pnostereo;
    float3 Dx = transform_perspective(&rastertocamera,
                                      make_float3(raster_x + 1.0f, raster_y, 0.0f));
    Dx = normalize(transform_direction(&cameratoworld, Dx));
    spherical_stereo_transform(&kernel_data.cam, &Px, &Dx);

    ray->dP.dx = Px - Pcenter;
    ray->dD.dx = Dx - Dcenter;

    float3 Py = Pnostereo;
    float3 Dy = transform_perspective(&rastertocamera,
                                      make_float3(raster_x, raster_y + 1.0f, 0.0f));
    Dy = normalize(transform_direction(&cameratoworld, Dy));
    spherical_stereo_transform(&kernel_data.cam, &Py, &Dy);

    ray->dP.dy = Py - Pcenter;
    ray->dD.dy = Dy - Dcenter;
#endif
  }

#ifdef __CAMERA_CLIPPING__
  /* clipping */
  float z_inv = 1.0f / normalize(Pcamera).z;
  float nearclip = kernel_data.cam.nearclip * z_inv;
  ray->P += nearclip * ray->D;
  ray->dP.dx += nearclip * ray->dD.dx;
  ray->dP.dy += nearclip * ray->dD.dy;
  ray->t = kernel_data.cam.cliplength * z_inv;
#else
  ray->t = FLT_MAX;
#endif
}

/* Orthographic Camera */
ccl_device void camera_sample_orthographic(KernelGlobals *kg,
                                           float raster_x,
                                           float raster_y,
                                           float lens_u,
                                           float lens_v,
                                           ccl_addr_space Ray *ray)
{
  /* create ray form raster position */
  ProjectionTransform rastertocamera = kernel_data.cam.rastertocamera;
  float3 Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y, 0.0f));

  float3 P;
  float3 D = make_float3(0.0f, 0.0f, 1.0f);

  /* modify ray for depth of field */
  float aperturesize = kernel_data.cam.aperturesize;

  if (aperturesize > 0.0f) {
    /* sample point on aperture */
    float2 lensuv = camera_sample_aperture(&kernel_data.cam, lens_u, lens_v) * aperturesize;

    /* compute point on plane of focus */
    float3 Pfocus = D * kernel_data.cam.focaldistance;

    /* update ray for effect of lens */
    float3 lensuvw = make_float3(lensuv.x, lensuv.y, 0.0f);
    P = Pcamera + lensuvw;
    D = normalize(Pfocus - lensuvw);
  }
  else {
    P = Pcamera;
  }
  /* transform ray from camera to world */
  Transform cameratoworld = kernel_data.cam.cameratoworld;

#ifdef __CAMERA_MOTION__
  if (kernel_data.cam.num_motion_steps) {
    transform_motion_array_interpolate(&cameratoworld,
                                       kernel_tex_array(__camera_motion),
                                       kernel_data.cam.num_motion_steps,
                                       ray->time);
  }
#endif

  ray->P = transform_point(&cameratoworld, P);
  ray->D = normalize(transform_direction(&cameratoworld, D));

#ifdef __RAY_DIFFERENTIALS__
  /* ray differential */
  ray->dP.dx = float4_to_float3(kernel_data.cam.dx);
  ray->dP.dy = float4_to_float3(kernel_data.cam.dy);

  ray->dD = differential3_zero();
#endif

#ifdef __CAMERA_CLIPPING__
  /* clipping */
  ray->t = kernel_data.cam.cliplength;
#else
  ray->t = FLT_MAX;
#endif
}

/* Panorama Camera */

ccl_device_inline void camera_sample_panorama(ccl_constant KernelCamera *cam,
                                              const ccl_global DecomposedTransform *cam_motion,
                                              float raster_x,
                                              float raster_y,
                                              float lens_u,
                                              float lens_v,
                                              ccl_addr_space Ray *ray)
{
  ProjectionTransform rastertocamera = cam->rastertocamera;
  float3 Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y, 0.0f));

  /* create ray form raster position */
  float3 P = make_float3(0.0f, 0.0f, 0.0f);
  float3 D = panorama_to_direction(cam, Pcamera.x, Pcamera.y);

  /* indicates ray should not receive any light, outside of the lens */
  if (is_zero(D)) {
    ray->t = 0.0f;
    return;
  }

  /* modify ray for depth of field */
  float aperturesize = cam->aperturesize;

  if (aperturesize > 0.0f) {
    /* sample point on aperture */
    float2 lensuv = camera_sample_aperture(cam, lens_u, lens_v) * aperturesize;

    /* compute point on plane of focus */
    float3 Dfocus = normalize(D);
    float3 Pfocus = Dfocus * cam->focaldistance;

    /* calculate orthonormal coordinates perpendicular to Dfocus */
    float3 U, V;
    U = normalize(make_float3(1.0f, 0.0f, 0.0f) - Dfocus.x * Dfocus);
    V = normalize(cross(Dfocus, U));

    /* update ray for effect of lens */
    P = U * lensuv.x + V * lensuv.y;
    D = normalize(Pfocus - P);
  }

  /* transform ray from camera to world */
  Transform cameratoworld = cam->cameratoworld;

#ifdef __CAMERA_MOTION__
  if (cam->num_motion_steps) {
    transform_motion_array_interpolate(
        &cameratoworld, cam_motion, cam->num_motion_steps, ray->time);
  }
#endif

  P = transform_point(&cameratoworld, P);
  D = normalize(transform_direction(&cameratoworld, D));

  /* Stereo transform */
  bool use_stereo = cam->interocular_offset != 0.0f;
  if (use_stereo) {
    spherical_stereo_transform(cam, &P, &D);
  }

  ray->P = P;
  ray->D = D;

#ifdef __RAY_DIFFERENTIALS__
  /* Ray differentials, computed from scratch using the raster coordinates
   * because we don't want to be affected by depth of field. We compute
   * ray origin and direction for the center and two neighboring pixels
   * and simply take their differences. */
  float3 Pcenter = Pcamera;
  float3 Dcenter = panorama_to_direction(cam, Pcenter.x, Pcenter.y);
  Pcenter = transform_point(&cameratoworld, Pcenter);
  Dcenter = normalize(transform_direction(&cameratoworld, Dcenter));
  if (use_stereo) {
    spherical_stereo_transform(cam, &Pcenter, &Dcenter);
  }

  float3 Px = transform_perspective(&rastertocamera, make_float3(raster_x + 1.0f, raster_y, 0.0f));
  float3 Dx = panorama_to_direction(cam, Px.x, Px.y);
  Px = transform_point(&cameratoworld, Px);
  Dx = normalize(transform_direction(&cameratoworld, Dx));
  if (use_stereo) {
    spherical_stereo_transform(cam, &Px, &Dx);
  }

  ray->dP.dx = Px - Pcenter;
  ray->dD.dx = Dx - Dcenter;

  float3 Py = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y + 1.0f, 0.0f));
  float3 Dy = panorama_to_direction(cam, Py.x, Py.y);
  Py = transform_point(&cameratoworld, Py);
  Dy = normalize(transform_direction(&cameratoworld, Dy));
  if (use_stereo) {
    spherical_stereo_transform(cam, &Py, &Dy);
  }

  ray->dP.dy = Py - Pcenter;
  ray->dD.dy = Dy - Dcenter;
#endif

#ifdef __CAMERA_CLIPPING__
  /* clipping */
  float nearclip = cam->nearclip;
  ray->P += nearclip * ray->D;
  ray->dP.dx += nearclip * ray->dD.dx;
  ray->dP.dy += nearclip * ray->dD.dy;
  ray->t = cam->cliplength;
#else
  ray->t = FLT_MAX;
#endif
}

/* Common */

ccl_device_inline void camera_sample(KernelGlobals *kg,
                                     int x,
                                     int y,
                                     float filter_u,
                                     float filter_v,
                                     float lens_u,
                                     float lens_v,
                                     float time,
                                     ccl_addr_space Ray *ray)
{
  /* pixel filter */
  int filter_table_offset = kernel_data.film.filter_table_offset;
  float raster_x = x + lookup_table_read(kg, filter_u, filter_table_offset, FILTER_TABLE_SIZE);
  float raster_y = y + lookup_table_read(kg, filter_v, filter_table_offset, FILTER_TABLE_SIZE);

#ifdef __CAMERA_MOTION__
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
     * where scan-lines are acquired from top to bottom and whole scanline
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
#endif

  /* sample */
  if (kernel_data.cam.type == CAMERA_PERSPECTIVE) {
    camera_sample_perspective(kg, raster_x, raster_y, lens_u, lens_v, ray);
  }
  else if (kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    camera_sample_orthographic(kg, raster_x, raster_y, lens_u, lens_v, ray);
  }
  else {
    const ccl_global DecomposedTransform *cam_motion = kernel_tex_array(__camera_motion);
    camera_sample_panorama(&kernel_data.cam, cam_motion, raster_x, raster_y, lens_u, lens_v, ray);
  }
}

/* Utilities */

ccl_device_inline float3 camera_position(KernelGlobals *kg)
{
  Transform cameratoworld = kernel_data.cam.cameratoworld;
  return make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);
}

ccl_device_inline float camera_distance(KernelGlobals *kg, float3 P)
{
  Transform cameratoworld = kernel_data.cam.cameratoworld;
  float3 camP = make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);

  if (kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
    float3 camD = make_float3(cameratoworld.x.z, cameratoworld.y.z, cameratoworld.z.z);
    return fabsf(dot((P - camP), camD));
  }
  else
    return len(P - camP);
}

ccl_device_inline float3 camera_direction_from_point(KernelGlobals *kg, float3 P)
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

ccl_device_inline float3 camera_world_to_ndc(KernelGlobals *kg, ShaderData *sd, float3 P)
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
