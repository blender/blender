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

#include "camera.h"
#include "scene.h"

#include "util_vector.h"

CCL_NAMESPACE_BEGIN

Camera::Camera()
{
	shuttertime = 1.0f;

	aperturesize = 0.0f;
	focaldistance = 10.0f;
	blades = 0;
	bladesrotation = 0.0f;

	matrix = transform_identity();

	motion.pre = transform_identity();
	motion.post = transform_identity();
	use_motion = false;

	type = CAMERA_PERSPECTIVE;
	fov = M_PI_F/4.0f;

	nearclip = 1e-5f;
	farclip = 1e5f;

	width = 1024;
	height = 512;

	left = -((float)width/(float)height);
	right = (float)width/(float)height;
	bottom = -1.0f;
	top = 1.0f;

	screentoworld = transform_identity();
	rastertoworld = transform_identity();
	ndctoworld = transform_identity();
	rastertocamera = transform_identity();
	cameratoworld = transform_identity();
	worldtoraster = transform_identity();

	dx = make_float3(0.0f, 0.0f, 0.0f);
	dy = make_float3(0.0f, 0.0f, 0.0f);

	need_update = true;
	need_device_update = true;
}

Camera::~Camera()
{
}

void Camera::update()
{
	if(!need_update)
		return;
	
	/* ndc to raster */
	Transform screentocamera;
	Transform ndctoraster = transform_scale(width, height, 1.0f);

	/* raster to screen */
	Transform screentoraster = ndctoraster;
	
	screentoraster = ndctoraster *
		transform_scale(1.0f/(right - left), 1.0f/(top - bottom), 1.0f) *
		transform_translate(-left, -bottom, 0.0f);

	Transform rastertoscreen = transform_inverse(screentoraster);

	/* screen to camera */
	if(type == CAMERA_PERSPECTIVE)
		screentocamera = transform_inverse(transform_perspective(fov, nearclip, farclip));
	else if(type == CAMERA_ORTHOGRAPHIC)
		screentocamera = transform_inverse(transform_orthographic(nearclip, farclip));
	else
		screentocamera = transform_identity();

	rastertocamera = screentocamera * rastertoscreen;

	cameratoworld = matrix;
	screentoworld = cameratoworld * screentocamera;
	rastertoworld = cameratoworld * rastertocamera;
	ndctoworld = rastertoworld * ndctoraster;
	worldtoraster = transform_inverse(rastertoworld);

	/* differentials */
	if(type == CAMERA_ORTHOGRAPHIC) {
		dx = transform_direction(&rastertocamera, make_float3(1, 0, 0));
		dy = transform_direction(&rastertocamera, make_float3(0, 1, 0));
	}
	else if(type == CAMERA_PERSPECTIVE) {
		dx = transform_perspective(&rastertocamera, make_float3(1, 0, 0)) -
		     transform_perspective(&rastertocamera, make_float3(0, 0, 0));
		dy = transform_perspective(&rastertocamera, make_float3(0, 1, 0)) -
		     transform_perspective(&rastertocamera, make_float3(0, 0, 0));
	}
	else {
		dx = make_float3(0, 0, 0);
		dy = make_float3(0, 0, 0);
	}

	dx = transform_direction(&cameratoworld, dx);
	dy = transform_direction(&cameratoworld, dy);

	need_update = false;
	need_device_update = true;
}

void Camera::device_update(Device *device, DeviceScene *dscene, Scene *scene)
{
	update();

	if(!need_device_update)
		return;
	
	KernelCamera *kcam = &dscene->data.cam;

	/* store matrices */
	kcam->screentoworld = screentoworld;
	kcam->rastertoworld = rastertoworld;
	kcam->ndctoworld = ndctoworld;
	kcam->rastertocamera = rastertocamera;
	kcam->cameratoworld = cameratoworld;
	kcam->worldtoscreen = transform_inverse(screentoworld);
	kcam->worldtoraster = worldtoraster;
	kcam->worldtondc = transform_inverse(ndctoworld);
	kcam->worldtocamera = transform_inverse(cameratoworld);

	/* camera motion */
	Scene::MotionType need_motion = scene->need_motion();
	kcam->have_motion = 0;

	if(need_motion == Scene::MOTION_PASS) {
		if(use_motion) {
			kcam->motion.pre = transform_inverse(motion.pre * rastertocamera);
			kcam->motion.post = transform_inverse(motion.post * rastertocamera);
		}
		else {
			kcam->motion.pre = worldtoraster;
			kcam->motion.post = worldtoraster;
		}
	}
	else if(need_motion == Scene::MOTION_BLUR) {
		/* todo: exact camera position will not be hit this way */
		if(use_motion) {
			transform_motion_decompose(&kcam->motion, &motion);
			kcam->have_motion = 1;
		}
	}

	/* depth of field */
	kcam->aperturesize = aperturesize;
	kcam->focaldistance = focaldistance;
	kcam->blades = (blades < 3)? 0.0f: blades;
	kcam->bladesrotation = bladesrotation;

	/* motion blur */
	kcam->shuttertime= (need_motion == Scene::MOTION_BLUR)? shuttertime: 0.0f;

	/* type */
	kcam->type = type;

	/* store differentials */
	kcam->dx = float3_to_float4(dx);
	kcam->dy = float3_to_float4(dy);

	/* clipping */
	kcam->nearclip = nearclip;
	kcam->cliplength = (farclip == FLT_MAX)? FLT_MAX: farclip - nearclip;

	need_device_update = false;
}

void Camera::device_free(Device *device, DeviceScene *dscene)
{
	/* nothing to free, only writing to constant memory */
}

bool Camera::modified(const Camera& cam)
{
	return !((shuttertime== cam.shuttertime) &&
		(aperturesize == cam.aperturesize) &&
		(blades == cam.blades) &&
		(bladesrotation == cam.bladesrotation) &&
		(focaldistance == cam.focaldistance) &&
		(type == cam.type) &&
		(fov == cam.fov) &&
		(nearclip == cam.nearclip) &&
		(farclip == cam.farclip) &&
		// modified for progressive render
		// (width == cam.width) &&
		// (height == cam.height) &&
		(left == cam.left) &&
		(right == cam.right) &&
		(bottom == cam.bottom) &&
		(top == cam.top) &&
		(matrix == cam.matrix) &&
		(motion == cam.motion) &&
		(use_motion == cam.use_motion));
}

void Camera::tag_update()
{
	need_update = true;
}

CCL_NAMESPACE_END

