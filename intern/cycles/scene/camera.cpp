/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/camera.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/stats.h"
#include "scene/tables.h"

#include "device/device.h"

#include "util/foreach.h"
#include "util/function.h"
#include "util/log.h"
#include "util/math_cdf.h"
#include "util/task.h"
#include "util/time.h"
#include "util/vector.h"

/* needed for calculating differentials */
#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/camera/camera.h"

CCL_NAMESPACE_BEGIN

static float shutter_curve_eval(float x, array<float> &shutter_curve)
{
  if (shutter_curve.size() == 0) {
    return 1.0f;
  }

  x *= shutter_curve.size();
  int index = (int)x;
  float frac = x - index;
  if (index < shutter_curve.size() - 1) {
    return lerp(shutter_curve[index], shutter_curve[index + 1], frac);
  }
  else {
    return shutter_curve[shutter_curve.size() - 1];
  }
}

NODE_DEFINE(Camera)
{
  NodeType *type = NodeType::add("camera", create);

  SOCKET_FLOAT(shuttertime, "Shutter Time", 1.0f);

  static NodeEnum motion_position_enum;
  motion_position_enum.insert("start", MOTION_POSITION_START);
  motion_position_enum.insert("center", MOTION_POSITION_CENTER);
  motion_position_enum.insert("end", MOTION_POSITION_END);
  SOCKET_ENUM(motion_position, "Motion Position", motion_position_enum, MOTION_POSITION_CENTER);

  static NodeEnum rolling_shutter_type_enum;
  rolling_shutter_type_enum.insert("none", ROLLING_SHUTTER_NONE);
  rolling_shutter_type_enum.insert("top", ROLLING_SHUTTER_TOP);
  SOCKET_ENUM(rolling_shutter_type,
              "Rolling Shutter Type",
              rolling_shutter_type_enum,
              ROLLING_SHUTTER_NONE);
  SOCKET_FLOAT(rolling_shutter_duration, "Rolling Shutter Duration", 0.1f);

  SOCKET_FLOAT_ARRAY(shutter_curve, "Shutter Curve", array<float>());

  SOCKET_FLOAT(aperturesize, "Aperture Size", 0.0f);
  SOCKET_FLOAT(focaldistance, "Focal Distance", 10.0f);
  SOCKET_UINT(blades, "Blades", 0);
  SOCKET_FLOAT(bladesrotation, "Blades Rotation", 0.0f);

  SOCKET_TRANSFORM(matrix, "Matrix", transform_identity());
  SOCKET_TRANSFORM_ARRAY(motion, "Motion", array<Transform>());

  SOCKET_FLOAT(aperture_ratio, "Aperture Ratio", 1.0f);

  static NodeEnum type_enum;
  type_enum.insert("perspective", CAMERA_PERSPECTIVE);
  type_enum.insert("orthograph", CAMERA_ORTHOGRAPHIC);
  type_enum.insert("panorama", CAMERA_PANORAMA);
  SOCKET_ENUM(camera_type, "Type", type_enum, CAMERA_PERSPECTIVE);

  static NodeEnum panorama_type_enum;
  panorama_type_enum.insert("equirectangular", PANORAMA_EQUIRECTANGULAR);
  panorama_type_enum.insert("mirrorball", PANORAMA_MIRRORBALL);
  panorama_type_enum.insert("fisheye_equidistant", PANORAMA_FISHEYE_EQUIDISTANT);
  panorama_type_enum.insert("fisheye_equisolid", PANORAMA_FISHEYE_EQUISOLID);
  panorama_type_enum.insert("fisheye_lens_polynomial", PANORAMA_FISHEYE_LENS_POLYNOMIAL);
  SOCKET_ENUM(panorama_type, "Panorama Type", panorama_type_enum, PANORAMA_EQUIRECTANGULAR);

  SOCKET_FLOAT(fisheye_fov, "Fisheye FOV", M_PI_F);
  SOCKET_FLOAT(fisheye_lens, "Fisheye Lens", 10.5f);
  SOCKET_FLOAT(latitude_min, "Latitude Min", -M_PI_2_F);
  SOCKET_FLOAT(latitude_max, "Latitude Max", M_PI_2_F);
  SOCKET_FLOAT(longitude_min, "Longitude Min", -M_PI_F);
  SOCKET_FLOAT(longitude_max, "Longitude Max", M_PI_F);
  SOCKET_FLOAT(fov, "FOV", M_PI_4_F);
  SOCKET_FLOAT(fov_pre, "FOV Pre", M_PI_4_F);
  SOCKET_FLOAT(fov_post, "FOV Post", M_PI_4_F);

  SOCKET_FLOAT(fisheye_polynomial_k0, "Fisheye Polynomial K0", 0.0f);
  SOCKET_FLOAT(fisheye_polynomial_k1, "Fisheye Polynomial K1", 0.0f);
  SOCKET_FLOAT(fisheye_polynomial_k2, "Fisheye Polynomial K2", 0.0f);
  SOCKET_FLOAT(fisheye_polynomial_k3, "Fisheye Polynomial K3", 0.0f);
  SOCKET_FLOAT(fisheye_polynomial_k4, "Fisheye Polynomial K4", 0.0f);

  static NodeEnum stereo_eye_enum;
  stereo_eye_enum.insert("none", STEREO_NONE);
  stereo_eye_enum.insert("left", STEREO_LEFT);
  stereo_eye_enum.insert("right", STEREO_RIGHT);
  SOCKET_ENUM(stereo_eye, "Stereo Eye", stereo_eye_enum, STEREO_NONE);

  SOCKET_BOOLEAN(use_spherical_stereo, "Use Spherical Stereo", false);

  SOCKET_FLOAT(interocular_distance, "Interocular Distance", 0.065f);
  SOCKET_FLOAT(convergence_distance, "Convergence Distance", 30.0f * 0.065f);

  SOCKET_BOOLEAN(use_pole_merge, "Use Pole Merge", false);
  SOCKET_FLOAT(pole_merge_angle_from, "Pole Merge Angle From", 60.0f * M_PI_F / 180.0f);
  SOCKET_FLOAT(pole_merge_angle_to, "Pole Merge Angle To", 75.0f * M_PI_F / 180.0f);

  SOCKET_FLOAT(sensorwidth, "Sensor Width", 0.036f);
  SOCKET_FLOAT(sensorheight, "Sensor Height", 0.024f);

  SOCKET_FLOAT(nearclip, "Near Clip", 1e-5f);
  SOCKET_FLOAT(farclip, "Far Clip", 1e5f);

  SOCKET_FLOAT(viewplane.left, "Viewplane Left", 0);
  SOCKET_FLOAT(viewplane.right, "Viewplane Right", 0);
  SOCKET_FLOAT(viewplane.bottom, "Viewplane Bottom", 0);
  SOCKET_FLOAT(viewplane.top, "Viewplane Top", 0);

  SOCKET_FLOAT(border.left, "Border Left", 0);
  SOCKET_FLOAT(border.right, "Border Right", 0);
  SOCKET_FLOAT(border.bottom, "Border Bottom", 0);
  SOCKET_FLOAT(border.top, "Border Top", 0);

  SOCKET_FLOAT(viewport_camera_border.left, "Viewport Border Left", 0);
  SOCKET_FLOAT(viewport_camera_border.right, "Viewport Border Right", 0);
  SOCKET_FLOAT(viewport_camera_border.bottom, "Viewport Border Bottom", 0);
  SOCKET_FLOAT(viewport_camera_border.top, "Viewport Border Top", 0);

  SOCKET_FLOAT(offscreen_dicing_scale, "Offscreen Dicing Scale", 1.0f);

  SOCKET_INT(full_width, "Full Width", 1024);
  SOCKET_INT(full_height, "Full Height", 512);

  SOCKET_BOOLEAN(use_perspective_motion, "Use Perspective Motion", false);

  return type;
}

Camera::Camera() : Node(get_node_type())
{
  shutter_table_offset = TABLE_OFFSET_INVALID;

  width = 1024;
  height = 512;

  use_perspective_motion = false;

  shutter_curve.resize(RAMP_TABLE_SIZE);
  for (int i = 0; i < shutter_curve.size(); ++i) {
    shutter_curve[i] = 1.0f;
  }

  compute_auto_viewplane();

  screentoworld = projection_identity();
  rastertoworld = projection_identity();
  ndctoworld = projection_identity();
  rastertocamera = projection_identity();
  cameratoworld = transform_identity();
  worldtoraster = projection_identity();

  full_rastertocamera = projection_identity();

  dx = zero_float3();
  dy = zero_float3();

  need_device_update = true;
  need_flags_update = true;
  previous_need_motion = -1;

  memset((void *)&kernel_camera, 0, sizeof(kernel_camera));
}

Camera::~Camera()
{
}

void Camera::compute_auto_viewplane()
{
  if (camera_type == CAMERA_PANORAMA) {
    viewplane.left = 0.0f;
    viewplane.right = 1.0f;
    viewplane.bottom = 0.0f;
    viewplane.top = 1.0f;
  }
  else {
    float aspect = (float)full_width / (float)full_height;
    if (full_width >= full_height) {
      viewplane.left = -aspect;
      viewplane.right = aspect;
      viewplane.bottom = -1.0f;
      viewplane.top = 1.0f;
    }
    else {
      viewplane.left = -1.0f;
      viewplane.right = 1.0f;
      viewplane.bottom = -1.0f / aspect;
      viewplane.top = 1.0f / aspect;
    }
  }
}

void Camera::update(Scene *scene)
{
  Scene::MotionType need_motion = scene->need_motion();

  if (previous_need_motion != need_motion) {
    /* scene's motion model could have been changed since previous device
     * camera update this could happen for example in case when one render
     * layer has got motion pass and another not */
    need_device_update = true;
  }

  if (!is_modified())
    return;

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->camera.times.add_entry({"update", time});
    }
  });

  /* Full viewport to camera border in the viewport. */
  Transform fulltoborder = transform_from_viewplane(viewport_camera_border);
  Transform bordertofull = transform_inverse(fulltoborder);

  /* NDC to raster. */
  Transform ndctoraster = transform_scale(width, height, 1.0f) * bordertofull;
  Transform full_ndctoraster = transform_scale(full_width, full_height, 1.0f) * bordertofull;

  /* Raster to screen. */
  Transform screentondc = fulltoborder * transform_from_viewplane(viewplane);

  Transform screentoraster = ndctoraster * screentondc;
  Transform rastertoscreen = transform_inverse(screentoraster);
  Transform full_screentoraster = full_ndctoraster * screentondc;
  Transform full_rastertoscreen = transform_inverse(full_screentoraster);

  /* Screen to camera. */
  ProjectionTransform cameratoscreen;
  if (camera_type == CAMERA_PERSPECTIVE)
    cameratoscreen = projection_perspective(fov, nearclip, farclip);
  else if (camera_type == CAMERA_ORTHOGRAPHIC)
    cameratoscreen = projection_orthographic(nearclip, farclip);
  else
    cameratoscreen = projection_identity();

  ProjectionTransform screentocamera = projection_inverse(cameratoscreen);

  rastertocamera = screentocamera * rastertoscreen;
  full_rastertocamera = screentocamera * full_rastertoscreen;
  cameratoraster = screentoraster * cameratoscreen;

  cameratoworld = matrix;
  screentoworld = cameratoworld * screentocamera;
  rastertoworld = cameratoworld * rastertocamera;
  ndctoworld = rastertoworld * ndctoraster;

  /* note we recompose matrices instead of taking inverses of the above, this
   * is needed to avoid inverting near degenerate matrices that happen due to
   * precision issues with large scenes */
  worldtocamera = transform_inverse(matrix);
  worldtoscreen = cameratoscreen * worldtocamera;
  worldtondc = screentondc * worldtoscreen;
  worldtoraster = ndctoraster * worldtondc;

  /* differentials */
  if (camera_type == CAMERA_ORTHOGRAPHIC) {
    dx = transform_perspective_direction(&rastertocamera, make_float3(1, 0, 0));
    dy = transform_perspective_direction(&rastertocamera, make_float3(0, 1, 0));
    full_dx = transform_perspective_direction(&full_rastertocamera, make_float3(1, 0, 0));
    full_dy = transform_perspective_direction(&full_rastertocamera, make_float3(0, 1, 0));
  }
  else if (camera_type == CAMERA_PERSPECTIVE) {
    dx = transform_perspective(&rastertocamera, make_float3(1, 0, 0)) -
         transform_perspective(&rastertocamera, make_float3(0, 0, 0));
    dy = transform_perspective(&rastertocamera, make_float3(0, 1, 0)) -
         transform_perspective(&rastertocamera, make_float3(0, 0, 0));
    full_dx = transform_perspective(&full_rastertocamera, make_float3(1, 0, 0)) -
              transform_perspective(&full_rastertocamera, make_float3(0, 0, 0));
    full_dy = transform_perspective(&full_rastertocamera, make_float3(0, 1, 0)) -
              transform_perspective(&full_rastertocamera, make_float3(0, 0, 0));
  }
  else {
    dx = zero_float3();
    dy = zero_float3();
  }

  dx = transform_direction(&cameratoworld, dx);
  dy = transform_direction(&cameratoworld, dy);
  full_dx = transform_direction(&cameratoworld, full_dx);
  full_dy = transform_direction(&cameratoworld, full_dy);

  if (camera_type == CAMERA_PERSPECTIVE) {
    float3 v = transform_perspective(&full_rastertocamera,
                                     make_float3(full_width, full_height, 1.0f));
    frustum_right_normal = normalize(make_float3(v.z, 0.0f, -v.x));
    frustum_top_normal = normalize(make_float3(0.0f, v.z, -v.y));

    v = transform_perspective(&full_rastertocamera, make_float3(0.0f, 0.0f, 1.0f));
    frustum_left_normal = normalize(make_float3(-v.z, 0.0f, v.x));
    frustum_bottom_normal = normalize(make_float3(0.0f, -v.z, v.y));
  }

  /* Compute kernel camera data. */
  KernelCamera *kcam = &kernel_camera;

  /* store matrices */
  kcam->screentoworld = screentoworld;
  kcam->rastertoworld = rastertoworld;
  kcam->rastertocamera = rastertocamera;
  kcam->cameratoworld = cameratoworld;
  kcam->worldtocamera = worldtocamera;
  kcam->worldtoscreen = worldtoscreen;
  kcam->worldtoraster = worldtoraster;
  kcam->worldtondc = worldtondc;
  kcam->ndctoworld = ndctoworld;

  /* camera motion */
  kcam->num_motion_steps = 0;
  kcam->have_perspective_motion = 0;
  kernel_camera_motion.clear();

  /* Test if any of the transforms are actually different. */
  bool have_motion = false;
  for (size_t i = 0; i < motion.size(); i++) {
    have_motion = have_motion || motion[i] != matrix;
  }

  if (need_motion == Scene::MOTION_PASS) {
    /* TODO(sergey): Support perspective (zoom, fov) motion. */
    if (camera_type == CAMERA_PANORAMA) {
      if (have_motion) {
        kcam->motion_pass_pre = transform_inverse(motion[0]);
        kcam->motion_pass_post = transform_inverse(motion[motion.size() - 1]);
      }
      else {
        kcam->motion_pass_pre = kcam->worldtocamera;
        kcam->motion_pass_post = kcam->worldtocamera;
      }
    }
    else {
      if (have_motion) {
        kcam->perspective_pre = cameratoraster * transform_inverse(motion[0]);
        kcam->perspective_post = cameratoraster * transform_inverse(motion[motion.size() - 1]);
      }
      else {
        kcam->perspective_pre = worldtoraster;
        kcam->perspective_post = worldtoraster;
      }
    }
  }
  else if (need_motion == Scene::MOTION_BLUR) {
    if (have_motion) {
      kernel_camera_motion.resize(motion.size());
      transform_motion_decompose(kernel_camera_motion.data(), motion.data(), motion.size());
      kcam->num_motion_steps = motion.size();
    }

    /* TODO(sergey): Support other types of camera. */
    if (use_perspective_motion && camera_type == CAMERA_PERSPECTIVE) {
      /* TODO(sergey): Move to an utility function and de-duplicate with
       * calculation above.
       */
      ProjectionTransform screentocamera_pre = projection_inverse(
          projection_perspective(fov_pre, nearclip, farclip));
      ProjectionTransform screentocamera_post = projection_inverse(
          projection_perspective(fov_post, nearclip, farclip));

      kcam->perspective_pre = screentocamera_pre * rastertoscreen;
      kcam->perspective_post = screentocamera_post * rastertoscreen;
      kcam->have_perspective_motion = 1;
    }
  }

  /* depth of field */
  kcam->aperturesize = aperturesize;
  kcam->focaldistance = focaldistance;
  kcam->blades = (blades < 3) ? 0.0f : blades;
  kcam->bladesrotation = bladesrotation;

  /* motion blur */
  kcam->shuttertime = (need_motion == Scene::MOTION_BLUR) ? shuttertime : -1.0f;
  kcam->motion_position = motion_position;

  /* type */
  kcam->type = camera_type;

  /* anamorphic lens bokeh */
  kcam->inv_aperture_ratio = 1.0f / aperture_ratio;

  /* panorama */
  kcam->panorama_type = panorama_type;
  kcam->fisheye_fov = fisheye_fov;
  kcam->fisheye_lens = fisheye_lens;
  kcam->equirectangular_range = make_float4(longitude_min - longitude_max,
                                            -longitude_min,
                                            latitude_min - latitude_max,
                                            -latitude_min + M_PI_2_F);
  kcam->fisheye_lens_polynomial_bias = fisheye_polynomial_k0;
  kcam->fisheye_lens_polynomial_coefficients = make_float4(
      fisheye_polynomial_k1, fisheye_polynomial_k2, fisheye_polynomial_k3, fisheye_polynomial_k4);

  switch (stereo_eye) {
    case STEREO_LEFT:
      kcam->interocular_offset = -interocular_distance * 0.5f;
      break;
    case STEREO_RIGHT:
      kcam->interocular_offset = interocular_distance * 0.5f;
      break;
    case STEREO_NONE:
    default:
      kcam->interocular_offset = 0.0f;
      break;
  }

  kcam->convergence_distance = convergence_distance;
  if (use_pole_merge) {
    kcam->pole_merge_angle_from = pole_merge_angle_from;
    kcam->pole_merge_angle_to = pole_merge_angle_to;
  }
  else {
    kcam->pole_merge_angle_from = -1.0f;
    kcam->pole_merge_angle_to = -1.0f;
  }

  /* sensor size */
  kcam->sensorwidth = sensorwidth;
  kcam->sensorheight = sensorheight;

  /* render size */
  kcam->width = width;
  kcam->height = height;

  /* store differentials */
  kcam->dx = float3_to_float4(dx);
  kcam->dy = float3_to_float4(dy);

  /* clipping */
  kcam->nearclip = nearclip;
  kcam->cliplength = (farclip == FLT_MAX) ? FLT_MAX : farclip - nearclip;

  /* Camera in volume. */
  kcam->is_inside_volume = 0;

  /* Rolling shutter effect */
  kcam->rolling_shutter_type = rolling_shutter_type;
  kcam->rolling_shutter_duration = rolling_shutter_duration;

  /* Set further update flags */
  clear_modified();
  need_device_update = true;
  need_flags_update = true;
  previous_need_motion = need_motion;
}

void Camera::device_update(Device * /* device */, DeviceScene *dscene, Scene *scene)
{
  update(scene);

  if (!need_device_update)
    return;

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->camera.times.add_entry({"device_update", time});
    }
  });

  scene->lookup_tables->remove_table(&shutter_table_offset);
  if (kernel_camera.shuttertime != -1.0f) {
    vector<float> shutter_table;
    util_cdf_inverted(SHUTTER_TABLE_SIZE,
                      0.0f,
                      1.0f,
                      function_bind(shutter_curve_eval, _1, shutter_curve),
                      false,
                      shutter_table);
    shutter_table_offset = scene->lookup_tables->add_table(dscene, shutter_table);
    kernel_camera.shutter_table_offset = (int)shutter_table_offset;
  }

  dscene->data.cam = kernel_camera;

  size_t num_motion_steps = kernel_camera_motion.size();
  if (num_motion_steps) {
    DecomposedTransform *camera_motion = dscene->camera_motion.alloc(num_motion_steps);
    memcpy(camera_motion, kernel_camera_motion.data(), sizeof(*camera_motion) * num_motion_steps);
    dscene->camera_motion.copy_to_device();
  }
  else {
    dscene->camera_motion.free();
  }
}

void Camera::device_update_volume(Device * /*device*/, DeviceScene *dscene, Scene *scene)
{
  if (!need_device_update && !need_flags_update) {
    return;
  }

  KernelIntegrator *kintegrator = &dscene->data.integrator;
  if (kintegrator->use_volumes) {
    KernelCamera *kcam = &dscene->data.cam;
    BoundBox viewplane_boundbox = viewplane_bounds_get();

    /* Parallel object update, with grain size to avoid too much threading overhead
     * for individual objects. */
    static const int OBJECTS_PER_TASK = 32;
    parallel_for(blocked_range<size_t>(0, scene->objects.size(), OBJECTS_PER_TASK),
                 [&](const blocked_range<size_t> &r) {
                   for (size_t i = r.begin(); i != r.end(); i++) {
                     Object *object = scene->objects[i];
                     if (object->get_geometry()->has_volume &&
                         viewplane_boundbox.intersects(object->bounds)) {
                       /* TODO(sergey): Consider adding more grained check. */
                       VLOG_INFO << "Detected camera inside volume.";
                       kcam->is_inside_volume = 1;
                       parallel_for_cancel();
                       break;
                     }
                   }
                 });

    if (!kcam->is_inside_volume) {
      VLOG_INFO << "Camera is outside of the volume.";
    }
  }

  need_device_update = false;
  need_flags_update = false;
}

void Camera::device_free(Device * /*device*/, DeviceScene *dscene, Scene *scene)
{
  scene->lookup_tables->remove_table(&shutter_table_offset);
  dscene->camera_motion.free();
}

float3 Camera::transform_raster_to_world(float raster_x, float raster_y)
{
  float3 D, P;
  if (camera_type == CAMERA_PERSPECTIVE) {
    D = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y, 0.0f));
    float3 Pclip = normalize(D);
    P = zero_float3();
    /* TODO(sergey): Aperture support? */
    P = transform_point(&cameratoworld, P);
    D = normalize(transform_direction(&cameratoworld, D));
    /* TODO(sergey): Clipping is conditional in kernel, and hence it could
     * be mistakes in here, currently leading to wrong camera-in-volume
     * detection.
     */
    P += nearclip * D / Pclip.z;
  }
  else if (camera_type == CAMERA_ORTHOGRAPHIC) {
    D = make_float3(0.0f, 0.0f, 1.0f);
    /* TODO(sergey): Aperture support? */
    P = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y, 0.0f));
    P = transform_point(&cameratoworld, P);
    D = normalize(transform_direction(&cameratoworld, D));
  }
  else {
    assert(!"unsupported camera type");
  }
  return P;
}

BoundBox Camera::viewplane_bounds_get()
{
  /* TODO(sergey): This is all rather stupid, but is there a way to perform
   * checks we need in a more clear and smart fashion? */
  BoundBox bounds = BoundBox::empty;

  if (camera_type == CAMERA_PANORAMA) {
    if (use_spherical_stereo == false) {
      bounds.grow(make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w), nearclip);
    }
    else {
      float half_eye_distance = interocular_distance * 0.5f;

      bounds.grow(
          make_float3(cameratoworld.x.w + half_eye_distance, cameratoworld.y.w, cameratoworld.z.w),
          nearclip);

      bounds.grow(
          make_float3(cameratoworld.z.w, cameratoworld.y.w + half_eye_distance, cameratoworld.z.w),
          nearclip);

      bounds.grow(
          make_float3(cameratoworld.x.w - half_eye_distance, cameratoworld.y.w, cameratoworld.z.w),
          nearclip);

      bounds.grow(
          make_float3(cameratoworld.x.w, cameratoworld.y.w - half_eye_distance, cameratoworld.z.w),
          nearclip);
    }
  }
  else {
    bounds.grow(transform_raster_to_world(0.0f, 0.0f));
    bounds.grow(transform_raster_to_world(0.0f, (float)height));
    bounds.grow(transform_raster_to_world((float)width, (float)height));
    bounds.grow(transform_raster_to_world((float)width, 0.0f));
    if (camera_type == CAMERA_PERSPECTIVE) {
      /* Center point has the most distance in local Z axis,
       * use it to construct bounding box/
       */
      bounds.grow(transform_raster_to_world(0.5f * width, 0.5f * height));
    }
  }
  return bounds;
}

float Camera::world_to_raster_size(float3 P)
{
  float res = 1.0f;

  if (camera_type == CAMERA_ORTHOGRAPHIC) {
    res = min(len(full_dx), len(full_dy));

    if (offscreen_dicing_scale > 1.0f) {
      float3 p = transform_point(&worldtocamera, P);
      float3 v1 = transform_perspective(&full_rastertocamera,
                                        make_float3(full_width, full_height, 0.0f));
      float3 v2 = transform_perspective(&full_rastertocamera, zero_float3());

      /* Create point clamped to frustum */
      float3 c;
      c.x = max(v2.x, min(v1.x, p.x));
      c.y = max(v2.y, min(v1.y, p.y));
      c.z = max(0.0f, p.z);

      /* Check right side */
      float f_dist = len(p - c) / sqrtf((v1.x * v1.x + v1.y * v1.y) * 0.5f);
      if (f_dist < 0.0f) {
        /* Check left side */
        f_dist = len(p - c) / sqrtf((v2.x * v2.x + v2.y * v2.y) * 0.5f);
      }
      if (f_dist > 0.0f) {
        res += res * f_dist * (offscreen_dicing_scale - 1.0f);
      }
    }
  }
  else if (camera_type == CAMERA_PERSPECTIVE) {
    /* Calculate as if point is directly ahead of the camera. */
    float3 raster = make_float3(0.5f * full_width, 0.5f * full_height, 0.0f);
    float3 Pcamera = transform_perspective(&full_rastertocamera, raster);

    /* dDdx */
    float3 Ddiff = transform_direction(&cameratoworld, Pcamera);
    float3 dx = len_squared(full_dx) < len_squared(full_dy) ? full_dx : full_dy;
    float3 dDdx = normalize(Ddiff + dx) - normalize(Ddiff);

    /* dPdx */
    float dist = len(transform_point(&worldtocamera, P));
    float3 D = normalize(Ddiff);
    res = len(dist * dDdx - dot(dist * dDdx, D) * D);

    /* Decent approx distance to frustum
     * (doesn't handle corners correctly, but not that big of a deal) */
    float f_dist = 0.0f;

    if (offscreen_dicing_scale > 1.0f) {
      float3 p = transform_point(&worldtocamera, P);

      /* Distance from the four planes */
      float r = dot(p, frustum_right_normal);
      float t = dot(p, frustum_top_normal);
      float l = dot(p, frustum_left_normal);
      float b = dot(p, frustum_bottom_normal);

      if (r <= 0.0f && l <= 0.0f && t <= 0.0f && b <= 0.0f) {
        /* Point is inside frustum */
        f_dist = 0.0f;
      }
      else if (r > 0.0f && l > 0.0f && t > 0.0f && b > 0.0f) {
        /* Point is behind frustum */
        f_dist = len(p);
      }
      else {
        /* Point may be behind or off to the side, need to check */
        float3 along_right = make_float3(-frustum_right_normal.z, 0.0f, frustum_right_normal.x);
        float3 along_left = make_float3(frustum_left_normal.z, 0.0f, -frustum_left_normal.x);
        float3 along_top = make_float3(0.0f, -frustum_top_normal.z, frustum_top_normal.y);
        float3 along_bottom = make_float3(0.0f, frustum_bottom_normal.z, -frustum_bottom_normal.y);

        float dist[] = {r, l, t, b};
        float3 along[] = {along_right, along_left, along_top, along_bottom};

        bool test_o = false;

        float *d = dist;
        float3 *a = along;
        for (int i = 0; i < 4; i++, d++, a++) {
          /* Test if we should check this side at all */
          if (*d > 0.0f) {
            if (dot(p, *a) >= 0.0f) {
              /* We are in front of the back edge of this side of the frustum */
              f_dist = max(f_dist, *d);
            }
            else {
              /* Possibly far enough behind the frustum to use distance to origin instead of edge
               */
              test_o = true;
            }
          }
        }

        if (test_o) {
          f_dist = (f_dist > 0) ? min(f_dist, len(p)) : len(p);
        }
      }

      if (f_dist > 0.0f) {
        res += len(dDdx - dot(dDdx, D) * D) * f_dist * (offscreen_dicing_scale - 1.0f);
      }
    }
  }
  else if (camera_type == CAMERA_PANORAMA) {
    float3 D = transform_point(&worldtocamera, P);
    float dist = len(D);

    Ray ray;
    memset(&ray, 0, sizeof(ray));

    /* Distortion can become so great that the results become meaningless, there
     * may be a better way to do this, but calculating differentials from the
     * point directly ahead seems to produce good enough results. */
#if 0
    float2 dir = direction_to_panorama(&kernel_camera, kernel_camera_motion.data(), normalize(D));
    float3 raster = transform_perspective(&full_cameratoraster, make_float3(dir.x, dir.y, 0.0f));

    ray.t = 1.0f;
    camera_sample_panorama(
        &kernel_camera, kernel_camera_motion.data(), raster.x, raster.y, 0.0f, 0.0f, &ray);
    if (ray.t == 0.0f) {
      /* No differentials, just use from directly ahead. */
      camera_sample_panorama(&kernel_camera,
                             kernel_camera_motion.data(),
                             0.5f * full_width,
                             0.5f * full_height,
                             0.0f,
                             0.0f,
                             &ray);
    }
#else
    camera_sample_panorama(&kernel_camera,
#  ifdef __CAMERA_MOTION__
                           kernel_camera_motion.data(),
#  endif
                           0.5f * full_width,
                           0.5f * full_height,
                           0.0f,
                           0.0f,
                           &ray);
#endif

    /* TODO: would it help to use more accurate differentials here? */
    differential3 dP;
    differential_transfer_compact(&dP, ray.dP, ray.D, ray.dD, ray.D, dist);

    return max(len(dP.dx), len(dP.dy));
  }

  return res;
}

bool Camera::use_motion() const
{
  return motion.size() > 1;
}

void Camera::set_screen_size(int width_, int height_)
{
  if (width_ != width || height_ != height) {
    width = width_;
    height = height_;
    tag_modified();
  }
}

float Camera::motion_time(int step) const
{
  return (use_motion()) ? 2.0f * step / (motion.size() - 1) - 1.0f : 0.0f;
}

int Camera::motion_step(float time) const
{
  if (use_motion()) {
    for (int step = 0; step < motion.size(); step++) {
      if (time == motion_time(step)) {
        return step;
      }
    }
  }

  return -1;
}

CCL_NAMESPACE_END
