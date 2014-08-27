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
 * limitations under the License
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
 * Renderman, and Blender after remapping. */

class Camera {
public:
	/* motion blur */
	float shuttertime;

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

	/* transformation */
	Transform matrix;

	/* motion */
	MotionTransform motion;
	bool use_motion;

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
	int previous_need_motion;

	/* functions */
	Camera();
	~Camera();
	
	void compute_auto_viewplane();

	void update();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene);
	void device_free(Device *device, DeviceScene *dscene);

	bool modified(const Camera& cam);
	bool motion_modified(const Camera& cam);
	void tag_update();
};

CCL_NAMESPACE_END

#endif /* __CAMERA_H__ */

