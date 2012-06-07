/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "kernel_types.h"

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

	/* sensor */
	float sensorwidth;
	float sensorheight;

	/* clipping */
	float nearclip;
	float farclip;

	/* screen */
	int width, height;
	float left, right, bottom, top;

	/* border */
	float border_left, border_right, border_bottom, border_top;

	/* transformation */
	Transform matrix;

	/* motion */
	MotionTransform motion;
	bool use_motion;

	/* computed camera parameters */
    Transform screentoworld;
	Transform rastertoworld;
	Transform ndctoworld;
	Transform rastertocamera;
	Transform cameratoworld;
	Transform worldtoraster;

	float3 dx;
	float3 dy;

	/* update */
	bool need_update;
	bool need_device_update;

	/* functions */
	Camera();
	~Camera();

	void update();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene);
	void device_free(Device *device, DeviceScene *dscene);

	bool modified(const Camera& cam);
	void tag_update();
};

CCL_NAMESPACE_END

#endif /* __CAMERA_H__ */

