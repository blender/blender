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

#include "kernel_types.h"

#include "util_boundbox.h"
#include "util_transform.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

/* Camera
 *
 * The camera parameters are quite standard, tested to be both compatible with
 * Renderman, and Blender after remapping.
 */

class Camera {
public:
	/* Specifies an offset for the shutter's time interval. */
	enum MotionPosition {
		/* Shutter opens at the current frame. */
		MOTION_POSITION_START,
		/* Shutter is fully open at the current frame. */
		MOTION_POSITION_CENTER,
		/* Shutter closes at the current frame. */
		MOTION_POSITION_END,
	};

	/* Specifies rolling shutter effect. */
	enum RollingShutterType {
		/* No rolling shutter effect. */
		ROLLING_SHUTTER_NONE = 0,
		/* Sensor is being scanned vertically from top to bottom. */
		ROLLING_SHUTTER_TOP,
	};

	/* motion blur */
	float shuttertime;
	MotionPosition motion_position;
	float shutter_curve[RAMP_TABLE_SIZE];
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

	/* border */
	BoundBox2D border;
	BoundBox2D viewport_camera_border;

	/* transformation */
	Transform matrix;

	/* motion */
	MotionTransform motion;
	bool use_motion, use_perspective_motion;
	float fov_pre, fov_post;
	PerspectiveMotionTransform perspective_motion;

	/* computed camera parameters */
	Transform screentoworld;
	Transform rastertoworld;
	Transform ndctoworld;
	Transform cameratoworld;

	Transform worldtoraster;
	Transform worldtoscreen;
	Transform worldtondc;
	Transform worldtocamera;

	Transform rastertocamera;
	Transform cameratoraster;

	float3 dx;
	float3 dy;

	/* update */
	bool need_update;
	bool need_device_update;
	bool need_flags_update;
	int previous_need_motion;

	/* functions */
	Camera();
	~Camera();
	
	void compute_auto_viewplane();

	void update();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene);
	void device_update_volume(Device *device, DeviceScene *dscene, Scene *scene);
	void device_free(Device *device, DeviceScene *dscene, Scene *scene);

	bool modified(const Camera& cam);
	bool motion_modified(const Camera& cam);
	void tag_update();

	/* Public utility functions. */
	BoundBox viewplane_bounds_get();

private:
	/* Private utility functions. */
	float3 transform_raster_to_world(float raster_x, float raster_y);
};

CCL_NAMESPACE_END

#endif /* __CAMERA_H__ */

