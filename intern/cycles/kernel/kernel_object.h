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
	OBJECT_TRANSFORM_MOTION_PRE = 0,
	OBJECT_INVERSE_TRANSFORM = 4,
	OBJECT_TRANSFORM_MOTION_POST = 4,
	OBJECT_PROPERTIES = 8,
	OBJECT_DUPLI = 9
};

enum ObjectVectorTransform {
	OBJECT_VECTOR_MOTION_PRE = 0,
	OBJECT_VECTOR_MOTION_POST = 3
};

__device_inline Transform object_fetch_transform(KernelGlobals *kg, int object, enum ObjectTransform type)
{
	int offset = object*OBJECT_SIZE + (int)type;

	Transform tfm;
	tfm.x = kernel_tex_fetch(__objects, offset + 0);
	tfm.y = kernel_tex_fetch(__objects, offset + 1);
	tfm.z = kernel_tex_fetch(__objects, offset + 2);
	tfm.w = make_float4(0.0f, 0.0f, 0.0f, 1.0f);

	return tfm;
}

__device_inline Transform object_fetch_vector_transform(KernelGlobals *kg, int object, enum ObjectVectorTransform type)
{
	int offset = object*OBJECT_VECTOR_SIZE + (int)type;

	Transform tfm;
	tfm.x = kernel_tex_fetch(__objects_vector, offset + 0);
	tfm.y = kernel_tex_fetch(__objects_vector, offset + 1);
	tfm.z = kernel_tex_fetch(__objects_vector, offset + 2);
	tfm.w = make_float4(0.0f, 0.0f, 0.0f, 1.0f);

	return tfm;
}

#ifdef __OBJECT_MOTION__
__device_inline Transform object_fetch_transform_motion(KernelGlobals *kg, int object, float time)
{
	DecompMotionTransform motion;

	int offset = object*OBJECT_SIZE + (int)OBJECT_TRANSFORM_MOTION_PRE;

	motion.mid.x = kernel_tex_fetch(__objects, offset + 0);
	motion.mid.y = kernel_tex_fetch(__objects, offset + 1);
	motion.mid.z = kernel_tex_fetch(__objects, offset + 2);
	motion.mid.w = kernel_tex_fetch(__objects, offset + 3);

	motion.pre_x = kernel_tex_fetch(__objects, offset + 4);
	motion.pre_y = kernel_tex_fetch(__objects, offset + 5);
	motion.post_x = kernel_tex_fetch(__objects, offset + 6);
	motion.post_y = kernel_tex_fetch(__objects, offset + 7);

	Transform tfm;
	transform_motion_interpolate(&tfm, &motion, time);

	return tfm;
}

__device_inline Transform object_fetch_transform_motion_test(KernelGlobals *kg, int object, float time, Transform *itfm)
{
	int object_flag = kernel_tex_fetch(__object_flag, object);

	if(object_flag & SD_OBJECT_MOTION) {
		/* if we do motion blur */
		Transform tfm = object_fetch_transform_motion(kg, object, time);

		if(itfm)
			*itfm = transform_quick_inverse(tfm);

		return tfm;
	}
	else {
		Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
		if(itfm)
			*itfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);

		return tfm;
	}
}
#endif

__device_inline void object_position_transform(KernelGlobals *kg, ShaderData *sd, float3 *P)
{
#ifdef __OBJECT_MOTION__
	*P = transform_point(&sd->ob_tfm, *P);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	*P = transform_point(&tfm, *P);
#endif
}

__device_inline void object_inverse_position_transform(KernelGlobals *kg, ShaderData *sd, float3 *P)
{
#ifdef __OBJECT_MOTION__
	*P = transform_point(&sd->ob_itfm, *P);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
	*P = transform_point(&tfm, *P);
#endif
}

__device_inline void object_inverse_normal_transform(KernelGlobals *kg, ShaderData *sd, float3 *N)
{
#ifdef __OBJECT_MOTION__
	*N = normalize(transform_direction_transposed(&sd->ob_tfm, *N));
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	*N = normalize(transform_direction_transposed(&tfm, *N));
#endif
}

__device_inline void object_normal_transform(KernelGlobals *kg, ShaderData *sd, float3 *N)
{
#ifdef __OBJECT_MOTION__
	*N = normalize(transform_direction_transposed(&sd->ob_itfm, *N));
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
	*N = normalize(transform_direction_transposed(&tfm, *N));
#endif
}

__device_inline void object_dir_transform(KernelGlobals *kg, ShaderData *sd, float3 *D)
{
#ifdef __OBJECT_MOTION__
	*D = transform_direction(&sd->ob_tfm, *D);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	*D = transform_direction(&tfm, *D);
#endif
}

__device_inline float3 object_location(KernelGlobals *kg, ShaderData *sd)
{
	if(sd->object == ~0)
		return make_float3(0.0f, 0.0f, 0.0f);

#ifdef __OBJECT_MOTION__
	return make_float3(sd->ob_tfm.x.w, sd->ob_tfm.y.w, sd->ob_tfm.z.w);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
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

__device_inline uint object_particle_id(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return __float_as_int(f.w);
}

__device_inline float3 object_dupli_generated(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return make_float3(0.0f, 0.0f, 0.0f);

	int offset = object*OBJECT_SIZE + OBJECT_DUPLI;
	float4 f = kernel_tex_fetch(__objects, offset);
	return make_float3(f.x, f.y, f.z);
}

__device_inline float3 object_dupli_uv(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return make_float3(0.0f, 0.0f, 0.0f);

	int offset = object*OBJECT_SIZE + OBJECT_DUPLI;
	float4 f = kernel_tex_fetch(__objects, offset + 1);
	return make_float3(f.x, f.y, 0.0f);
}


__device int shader_pass_id(KernelGlobals *kg, ShaderData *sd)
{
	return kernel_tex_fetch(__shader_flag, (sd->shader & SHADER_MASK)*2 + 1);
}

__device_inline float particle_index(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 0);
	return f.x;
}

__device float particle_age(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 0);
	return f.y;
}

__device float particle_lifetime(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 0);
	return f.z;
}

__device float particle_size(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 0);
	return f.w;
}

__device float4 particle_rotation(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 1);
	return f;
}

__device float3 particle_location(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 2);
	return make_float3(f.x, f.y, f.z);
}

__device float3 particle_velocity(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f2 = kernel_tex_fetch(__particles, offset + 2);
	float4 f3 = kernel_tex_fetch(__particles, offset + 3);
	return make_float3(f2.w, f3.x, f3.y);
}

__device float3 particle_angular_velocity(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f3 = kernel_tex_fetch(__particles, offset + 3);
	float4 f4 = kernel_tex_fetch(__particles, offset + 4);
	return make_float3(f3.z, f3.w, f4.x);
}

#ifdef __HAIR__
/* Hair Info Node fns */

__device float hair_radius(KernelGlobals *kg, int prim, float u)
{	
	float r = 0.0f;

	if (prim != -1) {
		float4 v00 = kernel_tex_fetch(__cur_segs, prim);

		int v1 = __float_as_int(v00.x);
		int v2 = __float_as_int(v00.y);

		float4 P1 = kernel_tex_fetch(__cur_keys, v1);
		float4 P2 = kernel_tex_fetch(__cur_keys, v2);
		r = (P2.w - P1.w) * u + P1.w;
	}

	return r;
}

__device float3 hair_tangent_normal(KernelGlobals *kg, ShaderData *sd)
{	
	float3 tgN = make_float3(0.0f,0.0f,0.0f);

	if (sd->curve_seg != ~0) {
		tgN = -(-sd->I - sd->dPdu * (dot(sd->dPdu,-sd->I) * kernel_data.curve_kernel_data.normalmix / len_squared(sd->dPdu)));
		tgN = normalize(tgN);
		/*if (kernel_data.curve_kernel_data.use_tangent_normal_correction) need to find suitable scaled gd for corrected normal
		{
			tgN = normalize(tgN - gd * sd->dPdu);
		}*/
	}

	return tgN;
}

__device float intercept(KernelGlobals *kg, int prim, int triAddr, float u)
{	
	float t = 0.0f;

	if (prim != -1) {
		float4 sd2 = kernel_tex_fetch(__tri_woop, triAddr*3+2);
		t = (sd2.y - sd2.x) * u + sd2.x;
	}

	return t;
}
#endif

CCL_NAMESPACE_END

