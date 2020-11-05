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

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "kernel/kernel_types.h"

#include "graph/node.h"

#include "util/util_array.h"
#include "util/util_boundbox.h"
#include "util/util_projection.h"
#include "util/util_transform.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

/* Camera
 *
 * The camera parameters are quite standard, tested to be both compatible with
 * Renderman, and Blender after remapping.
 */

class Camera : public Node {
 public:
  NODE_DECLARE

  /* Specifies an offset for the shutter's time interval. */
  enum MotionPosition {
    /* Shutter opens at the current frame. */
    MOTION_POSITION_START = 0,
    /* Shutter is fully open at the current frame. */
    MOTION_POSITION_CENTER = 1,
    /* Shutter closes at the current frame. */
    MOTION_POSITION_END = 2,

    MOTION_NUM_POSITIONS,
  };

  /* Specifies rolling shutter effect. */
  enum RollingShutterType {
    /* No rolling shutter effect. */
    ROLLING_SHUTTER_NONE = 0,
    /* Sensor is being scanned vertically from top to bottom. */
    ROLLING_SHUTTER_TOP = 1,

    ROLLING_SHUTTER_NUM_TYPES,
  };

  /* Stereo Type */
  enum StereoEye {
    STEREO_NONE,
    STEREO_LEFT,
    STEREO_RIGHT,
  };

  /* motion blur */
  NODE_SOCKET_API(float, shuttertime)
  NODE_SOCKET_API(MotionPosition, motion_position)
  NODE_SOCKET_API_ARRAY(array<float>, shutter_curve)
  size_t shutter_table_offset;

  /* ** Rolling shutter effect. ** */
  /* Defines rolling shutter effect type. */
  NODE_SOCKET_API(RollingShutterType, rolling_shutter_type)
  /* Specifies exposure time of scanlines when using
   * rolling shutter effect.
   */
  NODE_SOCKET_API(float, rolling_shutter_duration)

  /* depth of field */
  NODE_SOCKET_API(float, focaldistance)
  NODE_SOCKET_API(float, aperturesize)
  NODE_SOCKET_API(uint, blades)
  NODE_SOCKET_API(float, bladesrotation)

  /* type */
  NODE_SOCKET_API(CameraType, camera_type)
  NODE_SOCKET_API(float, fov)

  /* panorama */
  NODE_SOCKET_API(PanoramaType, panorama_type)
  NODE_SOCKET_API(float, fisheye_fov)
  NODE_SOCKET_API(float, fisheye_lens)
  NODE_SOCKET_API(float, latitude_min)
  NODE_SOCKET_API(float, latitude_max)
  NODE_SOCKET_API(float, longitude_min)
  NODE_SOCKET_API(float, longitude_max)

  /* panorama stereo */
  NODE_SOCKET_API(StereoEye, stereo_eye)
  NODE_SOCKET_API(bool, use_spherical_stereo)
  NODE_SOCKET_API(float, interocular_distance)
  NODE_SOCKET_API(float, convergence_distance)
  NODE_SOCKET_API(bool, use_pole_merge)
  NODE_SOCKET_API(float, pole_merge_angle_from)
  NODE_SOCKET_API(float, pole_merge_angle_to)

  /* anamorphic lens bokeh */
  NODE_SOCKET_API(float, aperture_ratio)

  /* sensor */
  NODE_SOCKET_API(float, sensorwidth)
  NODE_SOCKET_API(float, sensorheight)

  /* clipping */
  NODE_SOCKET_API(float, nearclip)
  NODE_SOCKET_API(float, farclip)

  /* screen */
  BoundBox2D viewplane;
  NODE_SOCKET_API_STRUCT_MEMBER(float, viewplane, left)
  NODE_SOCKET_API_STRUCT_MEMBER(float, viewplane, right)
  NODE_SOCKET_API_STRUCT_MEMBER(float, viewplane, bottom)
  NODE_SOCKET_API_STRUCT_MEMBER(float, viewplane, top)

  /* width and height change during preview, so we need these for calculating dice rates. */
  NODE_SOCKET_API(int, full_width)
  NODE_SOCKET_API(int, full_height)
  /* controls how fast the dicing rate falls off for geometry out side of view */
  NODE_SOCKET_API(float, offscreen_dicing_scale)

  /* border */
  BoundBox2D border;
  NODE_SOCKET_API_STRUCT_MEMBER(float, border, left)
  NODE_SOCKET_API_STRUCT_MEMBER(float, border, right)
  NODE_SOCKET_API_STRUCT_MEMBER(float, border, bottom)
  NODE_SOCKET_API_STRUCT_MEMBER(float, border, top)

  BoundBox2D viewport_camera_border;
  NODE_SOCKET_API_STRUCT_MEMBER(float, viewport_camera_border, left)
  NODE_SOCKET_API_STRUCT_MEMBER(float, viewport_camera_border, right)
  NODE_SOCKET_API_STRUCT_MEMBER(float, viewport_camera_border, bottom)
  NODE_SOCKET_API_STRUCT_MEMBER(float, viewport_camera_border, top)

  /* transformation */
  NODE_SOCKET_API(Transform, matrix)

  /* motion */
  NODE_SOCKET_API_ARRAY(array<Transform>, motion)
  NODE_SOCKET_API(bool, use_perspective_motion)
  NODE_SOCKET_API(float, fov_pre)
  NODE_SOCKET_API(float, fov_post)

  /* computed camera parameters */
  ProjectionTransform screentoworld;
  ProjectionTransform rastertoworld;
  ProjectionTransform ndctoworld;
  Transform cameratoworld;

  ProjectionTransform worldtoraster;
  ProjectionTransform worldtoscreen;
  ProjectionTransform worldtondc;
  Transform worldtocamera;

  ProjectionTransform rastertocamera;
  ProjectionTransform cameratoraster;

  ProjectionTransform full_rastertocamera;

  float3 dx;
  float3 dy;

  float3 full_dx;
  float3 full_dy;

  float3 frustum_right_normal;
  float3 frustum_top_normal;
  float3 frustum_left_normal;
  float3 frustum_bottom_normal;

  /* update */
  bool need_device_update;
  bool need_flags_update;
  int previous_need_motion;

  /* Kernel camera data, copied here for dicing. */
  KernelCamera kernel_camera;
  array<DecomposedTransform> kernel_camera_motion;

 private:
  int width;
  int height;
  int resolution;

 public:
  /* functions */
  Camera();
  ~Camera();

  void compute_auto_viewplane();

  void update(Scene *scene);

  void device_update(Device *device, DeviceScene *dscene, Scene *scene);
  void device_update_volume(Device *device, DeviceScene *dscene, Scene *scene);
  void device_free(Device *device, DeviceScene *dscene, Scene *scene);

  /* Public utility functions. */
  BoundBox viewplane_bounds_get();

  /* Calculates the width of a pixel at point in world space. */
  float world_to_raster_size(float3 P);

  /* Motion blur. */
  float motion_time(int step) const;
  int motion_step(float time) const;
  bool use_motion() const;

  void set_screen_size_and_resolution(int width_, int height_, int resolution_);

 private:
  /* Private utility functions. */
  float3 transform_raster_to_world(float raster_x, float raster_y);
};

CCL_NAMESPACE_END

#endif /* __CAMERA_H__ */
