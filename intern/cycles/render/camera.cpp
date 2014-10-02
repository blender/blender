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

#include "camera.h"
#include "mesh.h"
#include "object.h"
#include "scene.h"

#include "device.h"

#include "util_foreach.h"
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

	aperture_ratio = 1.0f;

	type = CAMERA_PERSPECTIVE;
	panorama_type = PANORAMA_EQUIRECTANGULAR;
	fisheye_fov = M_PI_F;
	fisheye_lens = 10.5f;
	fov = M_PI_4_F;

	sensorwidth = 0.036f;
	sensorheight = 0.024f;

	nearclip = 1e-5f;
	farclip = 1e5f;

	width = 1024;
	height = 512;
	resolution = 1;

	viewplane.left = -((float)width/(float)height);
	viewplane.right = (float)width/(float)height;
	viewplane.bottom = -1.0f;
	viewplane.top = 1.0f;

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
	previous_need_motion = -1;
}

Camera::~Camera()
{
}

void Camera::compute_auto_viewplane()
{
	float aspect = (float)width/(float)height;

	if(width >= height) {
		viewplane.left = -aspect;
		viewplane.right = aspect;
		viewplane.bottom = -1.0f;
		viewplane.top = 1.0f;
	}
	else {
		viewplane.left = -1.0f;
		viewplane.right = 1.0f;
		viewplane.bottom = -1.0f/aspect;
		viewplane.top = 1.0f/aspect;
	}
}

void Camera::update()
{
	if(!need_update)
		return;
	
	/* ndc to raster */
	Transform screentocamera;
	Transform ndctoraster = transform_scale(width, height, 1.0f);

	/* raster to screen */
	Transform screentondc = 
		transform_scale(1.0f/(viewplane.right - viewplane.left),
		                1.0f/(viewplane.top - viewplane.bottom), 1.0f) *
		transform_translate(-viewplane.left, -viewplane.bottom, 0.0f);

	Transform screentoraster = ndctoraster * screentondc;
	Transform rastertoscreen = transform_inverse(screentoraster);

	/* screen to camera */
	if(type == CAMERA_PERSPECTIVE)
		screentocamera = transform_inverse(transform_perspective(fov, nearclip, farclip));
	else if(type == CAMERA_ORTHOGRAPHIC)
		screentocamera = transform_inverse(transform_orthographic(nearclip, farclip));
	else
		screentocamera = transform_identity();
	
	Transform cameratoscreen = transform_inverse(screentocamera);

	rastertocamera = screentocamera * rastertoscreen;
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
	Scene::MotionType need_motion = scene->need_motion(device->info.advanced_shading);

	update();

	if (previous_need_motion != need_motion) {
		/* scene's motion model could have been changed since previous device
		 * camera update this could happen for example in case when one render
		 * layer has got motion pass and another not */
		need_device_update = true;
	}

	if(!need_device_update)
		return;
	
	KernelCamera *kcam = &dscene->data.cam;

	/* store matrices */
	kcam->screentoworld = screentoworld;
	kcam->rastertoworld = rastertoworld;
	kcam->rastertocamera = rastertocamera;
	kcam->cameratoworld = cameratoworld;
	kcam->worldtocamera = worldtocamera;
	kcam->worldtoscreen = worldtoscreen;
	kcam->worldtoraster = worldtoraster;
	kcam->worldtondc = worldtondc;

	/* camera motion */
	kcam->have_motion = 0;

	if(need_motion == Scene::MOTION_PASS) {
		if(type == CAMERA_PANORAMA) {
			if(use_motion) {
				kcam->motion.pre = transform_inverse(motion.pre);
				kcam->motion.post = transform_inverse(motion.post);
			}
			else {
				kcam->motion.pre = kcam->worldtocamera;
				kcam->motion.post = kcam->worldtocamera;
			}
		}
		else {
			if(use_motion) {
				kcam->motion.pre = cameratoraster * transform_inverse(motion.pre);
				kcam->motion.post = cameratoraster * transform_inverse(motion.post);
			}
			else {
				kcam->motion.pre = worldtoraster;
				kcam->motion.post = worldtoraster;
			}
		}
	}
#ifdef __CAMERA_MOTION__
	else if(need_motion == Scene::MOTION_BLUR) {
		if(use_motion) {
			transform_motion_decompose((DecompMotionTransform*)&kcam->motion, &motion, &matrix);
			kcam->have_motion = 1;
		}
	}
#endif

	/* depth of field */
	kcam->aperturesize = aperturesize;
	kcam->focaldistance = focaldistance;
	kcam->blades = (blades < 3)? 0.0f: blades;
	kcam->bladesrotation = bladesrotation;

	/* motion blur */
#ifdef __CAMERA_MOTION__
	kcam->shuttertime = (need_motion == Scene::MOTION_BLUR) ? shuttertime: -1.0f;
#else
	kcam->shuttertime = -1.0f;
#endif

	/* type */
	kcam->type = type;

	/* anamorphic lens bokeh */
	kcam->inv_aperture_ratio = 1.0f / aperture_ratio;

	/* panorama */
	kcam->panorama_type = panorama_type;
	kcam->fisheye_fov = fisheye_fov;
	kcam->fisheye_lens = fisheye_lens;

	/* sensor size */
	kcam->sensorwidth = sensorwidth;
	kcam->sensorheight = sensorheight;

	/* render size */
	kcam->width = width;
	kcam->height = height;
	kcam->resolution = resolution;

	/* store differentials */
	kcam->dx = float3_to_float4(dx);
	kcam->dy = float3_to_float4(dy);

	/* clipping */
	kcam->nearclip = nearclip;
	kcam->cliplength = (farclip == FLT_MAX)? FLT_MAX: farclip - nearclip;

	need_device_update = false;
	previous_need_motion = need_motion;

	/* Camera in volume. */
	kcam->is_inside_volume = 0;
	BoundBox viewplane_boundbox = viewplane_bounds_get();
	for(size_t i = 0; i < scene->objects.size(); ++i) {
		Object *object = scene->objects[i];
		if(object->mesh->has_volume &&
		   viewplane_boundbox.intersects(object->bounds))
		{
			/* TODO(sergey): Consider adding more grained check. */
			kcam->is_inside_volume = 1;
			break;
		}
	}
}

void Camera::device_free(Device *device, DeviceScene *dscene)
{
	/* nothing to free, only writing to constant memory */
}

bool Camera::modified(const Camera& cam)
{
	return !((shuttertime == cam.shuttertime) &&
		(aperturesize == cam.aperturesize) &&
		(blades == cam.blades) &&
		(bladesrotation == cam.bladesrotation) &&
		(focaldistance == cam.focaldistance) &&
		(type == cam.type) &&
		(fov == cam.fov) &&
		(nearclip == cam.nearclip) &&
		(farclip == cam.farclip) &&
		(sensorwidth == cam.sensorwidth) &&
		(sensorheight == cam.sensorheight) &&
		// modified for progressive render
		// (width == cam.width) &&
		// (height == cam.height) &&
		(viewplane == cam.viewplane) &&
		(border == cam.border) &&
		(matrix == cam.matrix) &&
		(aperture_ratio == cam.aperture_ratio) &&
		(panorama_type == cam.panorama_type) &&
		(fisheye_fov == cam.fisheye_fov) &&
		(fisheye_lens == cam.fisheye_lens));
}

bool Camera::motion_modified(const Camera& cam)
{
	return !((motion == cam.motion) &&
		(use_motion == cam.use_motion));
}

void Camera::tag_update()
{
	need_update = true;
}

float3 Camera::transform_raster_to_world(float raster_x, float raster_y)
{
	float3 D, P;
	if(type == CAMERA_PERSPECTIVE) {
		D = transform_perspective(&rastertocamera,
		                          make_float3(raster_x, raster_y, 0.0f));
		P = make_float3(0.0f, 0.0f, 0.0f);
		/* TODO(sergey): Aperture support? */
		P = transform_point(&cameratoworld, P);
		D = normalize(transform_direction(&cameratoworld, D));
		/* TODO(sergey): Clipping is conditional in kernel, and hence it could
		 * be mistakes in here, currently leading to wrong camera-in-volume
		 * detection.
		 */
		P += nearclip * D;
	}
	else if (type == CAMERA_ORTHOGRAPHIC) {
		D = make_float3(0.0f, 0.0f, 1.0f);
		/* TODO(sergey): Aperture support? */
		P = transform_perspective(&rastertocamera,
		                          make_float3(raster_x, raster_y, 0.0f));
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
	 * checks we need in a more clear and smart fasion?
	 */
	BoundBox bounds = BoundBox::empty;

	if(type == CAMERA_PANORAMA) {
		bounds.grow(make_float3(cameratoworld.w.x,
		                        cameratoworld.w.y,
		                        cameratoworld.w.z));
	}
	else {
		bounds.grow(transform_raster_to_world(0.0f, 0.0f));
		bounds.grow(transform_raster_to_world(0.0f, (float)height));
		bounds.grow(transform_raster_to_world((float)width, (float)height));
		bounds.grow(transform_raster_to_world((float)width, 0.0f));
		if(type == CAMERA_PERSPECTIVE) {
			/* Center point has the most distancei in local Z axis,
			 * use it to construct bounding box/
			 */
			bounds.grow(transform_raster_to_world(0.5f*width, 0.5f*height));
		}
	}
	return bounds;
}

CCL_NAMESPACE_END

