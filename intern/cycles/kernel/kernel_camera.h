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

ccl_device float2 camera_sample_aperture(KernelGlobals *kg, float u, float v)
{
	float blades = kernel_data.cam.blades;
	float2 bokeh;

	if(blades == 0.0f) {
		/* sample disk */
		bokeh = concentric_sample_disk(u, v);
	}
	else {
		/* sample polygon */
		float rotation = kernel_data.cam.bladesrotation;
		bokeh = regular_polygon_sample(blades, rotation, u, v);
	}

	/* anamorphic lens bokeh */
	bokeh.x *= kernel_data.cam.inv_aperture_ratio;

	return bokeh;
}

ccl_device void camera_sample_perspective(KernelGlobals *kg, float raster_x, float raster_y, float lens_u, float lens_v, Ray *ray)
{
	/* create ray form raster position */
	Transform rastertocamera = kernel_data.cam.rastertocamera;
	float3 Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y, 0.0f));

	ray->P = make_float3(0.0f, 0.0f, 0.0f);
	ray->D = Pcamera;

	/* modify ray for depth of field */
	float aperturesize = kernel_data.cam.aperturesize;

	if(aperturesize > 0.0f) {
		/* sample point on aperture */
		float2 lensuv = camera_sample_aperture(kg, lens_u, lens_v)*aperturesize;

		/* compute point on plane of focus */
		float ft = kernel_data.cam.focaldistance/ray->D.z;
		float3 Pfocus = ray->D*ft;

		/* update ray for effect of lens */
		ray->P = make_float3(lensuv.x, lensuv.y, 0.0f);
		ray->D = normalize(Pfocus - ray->P);
	}

	/* transform ray from camera to world */
	Transform cameratoworld = kernel_data.cam.cameratoworld;

#ifdef __CAMERA_MOTION__
	if(kernel_data.cam.have_motion)
		transform_motion_interpolate(&cameratoworld, (const DecompMotionTransform*)&kernel_data.cam.motion, ray->time);
#endif

	ray->P = transform_point(&cameratoworld, ray->P);
	ray->D = transform_direction(&cameratoworld, ray->D);
	ray->D = normalize(ray->D);

#ifdef __RAY_DIFFERENTIALS__
	/* ray differential */
	float3 Ddiff = transform_direction(&cameratoworld, Pcamera);

	ray->dP = differential3_zero();

	ray->dD.dx = normalize(Ddiff + float4_to_float3(kernel_data.cam.dx)) - normalize(Ddiff);
	ray->dD.dy = normalize(Ddiff + float4_to_float3(kernel_data.cam.dy)) - normalize(Ddiff);
#endif

#ifdef __CAMERA_CLIPPING__
	/* clipping */
	ray->P += kernel_data.cam.nearclip*ray->D;
	ray->t = kernel_data.cam.cliplength;
#else
	ray->t = FLT_MAX;
#endif
}

/* Orthographic Camera */

ccl_device void camera_sample_orthographic(KernelGlobals *kg, float raster_x, float raster_y, float lens_u, float lens_v, Ray *ray)
{
	/* create ray form raster position */
	Transform rastertocamera = kernel_data.cam.rastertocamera;
	float3 Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y, 0.0f));

	ray->D = make_float3(0.0f, 0.0f, 1.0f);

	/* modify ray for depth of field */
	float aperturesize = kernel_data.cam.aperturesize;

	if(aperturesize > 0.0f) {
		/* sample point on aperture */
		float2 lensuv = camera_sample_aperture(kg, lens_u, lens_v)*aperturesize;

		/* compute point on plane of focus */
		float3 Pfocus = ray->D * kernel_data.cam.focaldistance;

		/* update ray for effect of lens */
		float3 lensuvw = make_float3(lensuv.x, lensuv.y, 0.0f);
		ray->P = Pcamera + lensuvw;
		ray->D = normalize(Pfocus - lensuvw);
	}
	else {
		ray->P = Pcamera;
	}
	/* transform ray from camera to world */
	Transform cameratoworld = kernel_data.cam.cameratoworld;

#ifdef __CAMERA_MOTION__
	if(kernel_data.cam.have_motion)
		transform_motion_interpolate(&cameratoworld, (const DecompMotionTransform*)&kernel_data.cam.motion, ray->time);
#endif

	ray->P = transform_point(&cameratoworld, ray->P);
	ray->D = transform_direction(&cameratoworld, ray->D);
	ray->D = normalize(ray->D);

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

ccl_device void camera_sample_panorama(KernelGlobals *kg, float raster_x, float raster_y, float lens_u, float lens_v, Ray *ray)
{
	Transform rastertocamera = kernel_data.cam.rastertocamera;
	float3 Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y, 0.0f));

	/* create ray form raster position */
	ray->P = make_float3(0.0f, 0.0f, 0.0f);

#ifdef __CAMERA_CLIPPING__
	/* clipping */
	ray->t = kernel_data.cam.cliplength;
#else
	ray->t = FLT_MAX;
#endif

	ray->D = panorama_to_direction(kg, Pcamera.x, Pcamera.y);

	/* indicates ray should not receive any light, outside of the lens */
	if(is_zero(ray->D)) {	
		ray->t = 0.0f;
		return;
	}

	/* modify ray for depth of field */
	float aperturesize = kernel_data.cam.aperturesize;

	if(aperturesize > 0.0f) {
		/* sample point on aperture */
		float2 lensuv = camera_sample_aperture(kg, lens_u, lens_v)*aperturesize;

		/* compute point on plane of focus */
		float3 D = normalize(ray->D);
		float3 Pfocus = D * kernel_data.cam.focaldistance;

		/* calculate orthonormal coordinates perpendicular to D */
		float3 U, V;
		U = normalize(make_float3(1.0f, 0.0f, 0.0f) -  D.x * D);
		V = normalize(cross(D, U));

		/* update ray for effect of lens */
		ray->P = U * lensuv.x + V * lensuv.y;
		ray->D = normalize(Pfocus - ray->P);
	}

	/* transform ray from camera to world */
	Transform cameratoworld = kernel_data.cam.cameratoworld;

#ifdef __CAMERA_MOTION__
	if(kernel_data.cam.have_motion)
		transform_motion_interpolate(&cameratoworld, (const DecompMotionTransform*)&kernel_data.cam.motion, ray->time);
#endif

	ray->P = transform_point(&cameratoworld, ray->P);
	ray->D = transform_direction(&cameratoworld, ray->D);
	ray->D = normalize(ray->D);

#ifdef __RAY_DIFFERENTIALS__
	/* ray differential */
	ray->dP = differential3_zero();

	Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x + 1.0f, raster_y, 0.0f));
	ray->dD.dx = normalize(transform_direction(&cameratoworld, panorama_to_direction(kg, Pcamera.x, Pcamera.y))) - ray->D;

	Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y + 1.0f, 0.0f));
	ray->dD.dy = normalize(transform_direction(&cameratoworld, panorama_to_direction(kg, Pcamera.x, Pcamera.y))) - ray->D;
#endif
}

/* Common */

ccl_device void camera_sample(KernelGlobals *kg, int x, int y, float filter_u, float filter_v,
	float lens_u, float lens_v, float time, Ray *ray)
{
	/* pixel filter */
	int filter_table_offset = kernel_data.film.filter_table_offset;
	float raster_x = x + lookup_table_read(kg, filter_u, filter_table_offset, FILTER_TABLE_SIZE);
	float raster_y = y + lookup_table_read(kg, filter_v, filter_table_offset, FILTER_TABLE_SIZE);

#ifdef __CAMERA_MOTION__
	/* motion blur */
	if(kernel_data.cam.shuttertime == -1.0f)
		ray->time = TIME_INVALID;
	else
		ray->time = time;
#endif

	/* sample */
	if(kernel_data.cam.type == CAMERA_PERSPECTIVE)
		camera_sample_perspective(kg, raster_x, raster_y, lens_u, lens_v, ray);
	else if(kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
		camera_sample_orthographic(kg, raster_x, raster_y, lens_u, lens_v, ray);
	else
		camera_sample_panorama(kg, raster_x, raster_y, lens_u, lens_v, ray);
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

	if(kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
		float3 camD = make_float3(cameratoworld.x.z, cameratoworld.y.z, cameratoworld.z.z);
		return fabsf(dot((P - camP), camD));
	}
	else
		return len(P - camP);
}

ccl_device_inline float3 camera_direction_from_point(KernelGlobals *kg, float3 P)
{
	Transform cameratoworld = kernel_data.cam.cameratoworld;

	if(kernel_data.cam.type == CAMERA_ORTHOGRAPHIC) {
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
	if(kernel_data.cam.type != CAMERA_PANORAMA) {
		/* perspective / ortho */
		if(sd->object == PRIM_NONE && kernel_data.cam.type == CAMERA_PERSPECTIVE)
			P += camera_position(kg);

		Transform tfm = kernel_data.cam.worldtondc;
		return transform_perspective(&tfm, P);
	}
	else {
		/* panorama */
		Transform tfm = kernel_data.cam.worldtocamera;

		if(sd->object != OBJECT_NONE)
			P = normalize(transform_point(&tfm, P));
		else
			P = normalize(transform_direction(&tfm, P));

		float2 uv = direction_to_panorama(kg, P);

		return make_float3(uv.x, uv.y, 0.0f);
	}
}

CCL_NAMESPACE_END

