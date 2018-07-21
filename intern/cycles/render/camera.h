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
	float shuttertime;
	MotionPosition motion_position;
	array<float> shutter_curve;
	size_t shutter_table_offset;

	/* ** Rolling shutter effect. ** */
	/* Defines rolling shutter effect type. */
	RollingShutterType rolling_shutter_type;
	/* Specifies exposure time of scanlines when using
	 * rolling shutter effect.
	 */
	float rolling_shutter_duration;

	/* depth of field */
	float focaldistance;
	float aperturesize;
	uint blades;
	float bladesrotation;

	/* type */
	CameraType type;
	float fov;

	/* panorama */
	PanoramaType panorama_type;
	float fisheye_fov;
	float fisheye_lens;
	float latitude_min;
	float latitude_max;
	float longitude_min;
	float longitude_max;

	/* panorama stereo */
	StereoEye stereo_eye;
	bool use_spherical_stereo;
	float interocular_distance;
	float convergence_distance;
	bool use_pole_merge;
	float pole_merge_angle_from;
	float pole_merge_angle_to;

	/* anamorphic lens bokeh */
	float aperture_ratio;

	/* sensor */
	float sensorwidth;
	float sensorheight;

	/* clipping */
	float nearclip;
	float farclip;

	/* screen */
	int width, height;
	int resolution;
	BoundBox2D viewplane;
	/* width and height change during preview, so we need these for calculating dice rates. */
	int full_width, full_height;
	/* controls how fast the dicing rate falls off for geometry out side of view */
	float offscreen_dicing_scale;

	/* border */
	BoundBox2D border;
	BoundBox2D viewport_camera_border;

	/* transformation */
	Transform matrix;

	/* motion */
	array<Transform> motion;
	bool use_perspective_motion;
	float fov_pre, fov_post;

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

	float3 dx;
	float3 dy;

	float3 full_dx;
	float3 full_dy;

	float3 frustum_right_normal;
	float3 frustum_top_normal;

	/* update */
	bool need_update;
	bool need_device_update;
	bool need_flags_update;
	int previous_need_motion;

	/* Kernel camera data, copied here for dicing. */
	KernelCamera kernel_camera;
	array<DecomposedTransform> kernel_camera_motion;

	/* functions */
	Camera();
	~Camera();

	void compute_auto_viewplane();

	void update(Scene *scene);

	void device_update(Device *device, DeviceScene *dscene, Scene *scene);
	void device_update_volume(Device *device, DeviceScene *dscene, Scene *scene);
	void device_free(Device *device, DeviceScene *dscene, Scene *scene);

	bool modified(const Camera& cam);
	bool motion_modified(const Camera& cam);
	void tag_update();

	/* Public utility functions. */
	BoundBox viewplane_bounds_get();

	/* Calculates the width of a pixel at point in world space. */
	float world_to_raster_size(float3 P);

	/* Motion blur. */
	float motion_time(int step) const;
	int motion_step(float time) const;
	bool use_motion() const;

private:
	/* Private utility functions. */
	float3 transform_raster_to_world(float raster_x, float raster_y);
};

CCL_NAMESPACE_END

#endif /* __CAMERA_H__ */
