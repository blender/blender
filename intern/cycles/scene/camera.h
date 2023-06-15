/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "kernel/types.h"

#include "graph/node.h"

#include "util/array.h"
#include "util/boundbox.h"
#include "util/projection.h"
#include "util/transform.h"
#include "util/types.h"

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
  /* Specifies exposure time of scan-lines when using
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

  NODE_SOCKET_API(float, fisheye_polynomial_k0)
  NODE_SOCKET_API(float, fisheye_polynomial_k1)
  NODE_SOCKET_API(float, fisheye_polynomial_k2)
  NODE_SOCKET_API(float, fisheye_polynomial_k3)
  NODE_SOCKET_API(float, fisheye_polynomial_k4)

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

  void set_screen_size(int width_, int height_);

 private:
  /* Private utility functions. */
  float3 transform_raster_to_world(float raster_x, float raster_y);
};

CCL_NAMESPACE_END

#endif /* __CAMERA_H__ */
