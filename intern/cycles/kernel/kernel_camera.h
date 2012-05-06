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

CCL_NAMESPACE_BEGIN

/* Perspective Camera */

__device float2 camera_sample_aperture(KernelGlobals *kg, float u, float v)
{
	float blades = kernel_data.cam.blades;

	if(blades == 0.0f) {
		/* sample disk */
		return concentric_sample_disk(u, v);
	}
	else {
		/* sample polygon */
		float rotation = kernel_data.cam.bladesrotation;
		return regular_polygon_sample(blades, rotation, u, v);
	}
}

__device void camera_sample_perspective(KernelGlobals *kg, float raster_x, float raster_y, float lens_u, float lens_v, Ray *ray)
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
		float3 Pfocus = ray->P + ray->D*ft;

		/* update ray for effect of lens */
		ray->P = make_float3(lensuv.x, lensuv.y, 0.0f);
		ray->D = normalize(Pfocus - ray->P);
	}

	/* transform ray from camera to world */
	Transform cameratoworld = kernel_data.cam.cameratoworld;

#ifdef __MOTION__
	if(kernel_data.cam.have_motion)
		transform_motion_interpolate(&cameratoworld, &kernel_data.cam.motion, ray->time);
#endif

	ray->P = transform_point(&cameratoworld, ray->P);
	ray->D = transform_direction(&cameratoworld, ray->D);
	ray->D = normalize(ray->D);

#ifdef __RAY_DIFFERENTIALS__
	/* ray differential */
	float3 Ddiff = transform_direction(&cameratoworld, Pcamera);

	ray->dP.dx = make_float3(0.0f, 0.0f, 0.0f);
	ray->dP.dy = make_float3(0.0f, 0.0f, 0.0f);

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

__device void camera_sample_orthographic(KernelGlobals *kg, float raster_x, float raster_y, Ray *ray)
{
	/* create ray form raster position */
	Transform rastertocamera = kernel_data.cam.rastertocamera;
	float3 Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y, 0.0f));

	ray->P = Pcamera;
	ray->D = make_float3(0.0f, 0.0f, 1.0f);

	/* transform ray from camera to world */
	Transform cameratoworld = kernel_data.cam.cameratoworld;

#ifdef __MOTION__
	if(kernel_data.cam.have_motion)
		transform_motion_interpolate(&cameratoworld, &kernel_data.cam.motion, ray->time);
#endif

	ray->P = transform_point(&cameratoworld, ray->P);
	ray->D = transform_direction(&cameratoworld, ray->D);
	ray->D = normalize(ray->D);

#ifdef __RAY_DIFFERENTIALS__
	/* ray differential */
	ray->dP.dx = float4_to_float3(kernel_data.cam.dx);
	ray->dP.dy = float4_to_float3(kernel_data.cam.dy);

	ray->dD.dx = make_float3(0.0f, 0.0f, 0.0f);
	ray->dD.dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

#ifdef __CAMERA_CLIPPING__
	/* clipping */
	ray->t = kernel_data.cam.cliplength;
#else
	ray->t = FLT_MAX;
#endif
}

/* Environment Camera */

__device void camera_sample_environment(KernelGlobals *kg, float raster_x, float raster_y, Ray *ray)
{
	Transform rastertocamera = kernel_data.cam.rastertocamera;
	float3 Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y, 0.0f));

	/* create ray form raster position */
	ray->P = make_float3(0.0f, 0.0f, 0.0f);
	ray->D = equirectangular_to_direction(Pcamera.x, Pcamera.y);

	/* transform ray from camera to world */
	Transform cameratoworld = kernel_data.cam.cameratoworld;

#ifdef __MOTION__
	if(kernel_data.cam.have_motion)
		transform_motion_interpolate(&cameratoworld, &kernel_data.cam.motion, ray->time);
#endif

	ray->P = transform_point(&cameratoworld, ray->P);
	ray->D = transform_direction(&cameratoworld, ray->D);
	ray->D = normalize(ray->D);

#ifdef __RAY_DIFFERENTIALS__
	/* ray differential */
	ray->dP.dx = make_float3(0.0f, 0.0f, 0.0f);
	ray->dP.dy = make_float3(0.0f, 0.0f, 0.0f);

	Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x + 1.0f, raster_y, 0.0f));
	ray->dD.dx = normalize(transform_direction(&cameratoworld, equirectangular_to_direction(Pcamera.x, Pcamera.y))) - ray->D;

	Pcamera = transform_perspective(&rastertocamera, make_float3(raster_x, raster_y + 1.0f, 0.0f));
	ray->dD.dy = normalize(transform_direction(&cameratoworld, equirectangular_to_direction(Pcamera.x, Pcamera.y))) - ray->D;
#endif

#ifdef __CAMERA_CLIPPING__
	/* clipping */
	ray->t = kernel_data.cam.cliplength;
#else
	ray->t = FLT_MAX;
#endif
}

/* Common */

__device void camera_sample(KernelGlobals *kg, int x, int y, float filter_u, float filter_v,
	float lens_u, float lens_v, float time, Ray *ray)
{
	/* pixel filter */
	float raster_x = x + kernel_tex_interp(__filter_table, filter_u, FILTER_TABLE_SIZE);
	float raster_y = y + kernel_tex_interp(__filter_table, filter_v, FILTER_TABLE_SIZE);

#ifdef __MOTION__
	/* motion blur */
	if(kernel_data.cam.shuttertime == 0.0f)
		ray->time = TIME_INVALID;
	else
		ray->time = 0.5f + (time - 0.5f)*kernel_data.cam.shuttertime;
#endif

	/* sample */
	if(kernel_data.cam.type == CAMERA_PERSPECTIVE)
		camera_sample_perspective(kg, raster_x, raster_y, lens_u, lens_v, ray);
	else if(kernel_data.cam.type == CAMERA_ORTHOGRAPHIC)
		camera_sample_orthographic(kg, raster_x, raster_y, ray);
	else
		camera_sample_environment(kg, raster_x, raster_y, ray);
}

CCL_NAMESPACE_END

