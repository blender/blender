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

enum ObjectTransform {
	OBJECT_TRANSFORM = 0,
	OBJECT_INVERSE_TRANSFORM = 3,
	OBJECT_PROPERTIES = 6,
	OBJECT_TRANSFORM_MOTION_PRE = 8,
	OBJECT_TRANSFORM_MOTION_POST = 12
};

__device_inline Transform object_fetch_transform(KernelGlobals *kg, int object, float time, enum ObjectTransform type)
{
	Transform tfm;

#ifdef __MOTION__
	/* if we do motion blur */
	if(sd->flag & SD_OBJECT_MOTION) {
		/* fetch motion transforms */
		MotionTransform motion;

		motion.pre.x = have_motion;
		motion.pre.y = kernel_tex_fetch(__objects, offset + 1);
		motion.pre.z = kernel_tex_fetch(__objects, offset + 2);
		motion.pre.w = kernel_tex_fetch(__objects, offset + 3);

		motion.post.x = kernel_tex_fetch(__objects, offset + 4);
		motion.post.y = kernel_tex_fetch(__objects, offset + 5);
		motion.post.z = kernel_tex_fetch(__objects, offset + 6);
		motion.post.w = kernel_tex_fetch(__objects, offset + 7);

		/* interpolate (todo: do only once per object) */
		transform_motion_interpolate(&tfm, &motion, time);

		/* invert */
		if(type == OBJECT_INVERSE_TRANSFORM)
			tfm = transform_quick_inverse(tfm);

		return tfm;
	}
#endif

	int offset = object*OBJECT_SIZE + (int)type;

	tfm.x = kernel_tex_fetch(__objects, offset + 0);
	tfm.y = kernel_tex_fetch(__objects, offset + 1);
	tfm.z = kernel_tex_fetch(__objects, offset + 2);
	tfm.w = make_float4(0.0f, 0.0f, 0.0f, 1.0f);

	return tfm;
}

__device_inline void object_position_transform(KernelGlobals *kg, ShaderData *sd, float3 *P)
{
#ifdef __MOTION__
	*P = transform_point(&sd->ob_tfm, *P);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, TIME_INVALID, OBJECT_TRANSFORM);
	*P = transform_point(&tfm, *P);
#endif
}

__device_inline void object_inverse_position_transform(KernelGlobals *kg, ShaderData *sd, float3 *P)
{
#ifdef __MOTION__
	*P = transform_point(&sd->ob_itfm, *P);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, TIME_INVALID, OBJECT_INVERSE_TRANSFORM);
	*P = transform_point(&tfm, *P);
#endif
}

__device_inline void object_inverse_normal_transform(KernelGlobals *kg, ShaderData *sd, float3 *N)
{
#ifdef __MOTION__
	*N = normalize(transform_direction_transposed(&sd->ob_tfm, *N));
#else
	Transform tfm = object_fetch_transform(kg, sd->object, TIME_INVALID, OBJECT_TRANSFORM);
	*N = normalize(transform_direction_transposed(&tfm, *N));
#endif
}

__device_inline void object_normal_transform(KernelGlobals *kg, ShaderData *sd, float3 *N)
{
#ifdef __MOTION__
	*N = normalize(transform_direction_transposed(&sd->ob_itfm, *N));
#else
	Transform tfm = object_fetch_transform(kg, sd->object, TIME_INVALID, OBJECT_INVERSE_TRANSFORM);
	*N = normalize(transform_direction_transposed(&tfm, *N));
#endif
}

__device_inline void object_dir_transform(KernelGlobals *kg, ShaderData *sd, float3 *D)
{
#ifdef __MOTION__
	*D = transform_direction(&sd->ob_tfm, *D);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, 0.0f, OBJECT_TRANSFORM);
	*D = transform_direction(&tfm, *D);
#endif
}

__device_inline float3 object_location(KernelGlobals *kg, ShaderData *sd)
{
#ifdef __MOTION__
	return make_float3(sd->ob_tfm.x.w, sd->ob_tfm.y.w, sd->ob_tfm.z.w);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, 0.0f, OBJECT_TRANSFORM);
	return make_float3(tfm.x.w, tfm.y.w, tfm.z.w);
#endif
}

__device_inline float object_surface_area(KernelGlobals *kg, int object)
{
	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.x;
}

__device_inline float object_pass_id(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.y;
}

__device_inline float object_random_number(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.z;
}

__device int shader_pass_id(KernelGlobals *kg, ShaderData *sd)
{
	return kernel_tex_fetch(__shader_flag, (sd->shader & SHADER_MASK)*2 + 1);
}

CCL_NAMESPACE_END

