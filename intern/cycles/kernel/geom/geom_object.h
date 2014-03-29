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

ccl_device_inline Transform object_fetch_transform(KernelGlobals *kg, int object, enum ObjectTransform type)
{
	int offset = object*OBJECT_SIZE + (int)type;

	Transform tfm;
	tfm.x = kernel_tex_fetch(__objects, offset + 0);
	tfm.y = kernel_tex_fetch(__objects, offset + 1);
	tfm.z = kernel_tex_fetch(__objects, offset + 2);
	tfm.w = make_float4(0.0f, 0.0f, 0.0f, 1.0f);

	return tfm;
}

ccl_device_inline Transform object_fetch_vector_transform(KernelGlobals *kg, int object, enum ObjectVectorTransform type)
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
ccl_device_inline Transform object_fetch_transform_motion(KernelGlobals *kg, int object, float time)
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

ccl_device_inline Transform object_fetch_transform_motion_test(KernelGlobals *kg, int object, float time, Transform *itfm)
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

ccl_device_inline void object_position_transform(KernelGlobals *kg, ShaderData *sd, float3 *P)
{
#ifdef __OBJECT_MOTION__
	*P = transform_point(&sd->ob_tfm, *P);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	*P = transform_point(&tfm, *P);
#endif
}

ccl_device_inline void object_inverse_position_transform(KernelGlobals *kg, ShaderData *sd, float3 *P)
{
#ifdef __OBJECT_MOTION__
	*P = transform_point(&sd->ob_itfm, *P);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
	*P = transform_point(&tfm, *P);
#endif
}

ccl_device_inline void object_inverse_normal_transform(KernelGlobals *kg, ShaderData *sd, float3 *N)
{
#ifdef __OBJECT_MOTION__
	*N = normalize(transform_direction_transposed(&sd->ob_tfm, *N));
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	*N = normalize(transform_direction_transposed(&tfm, *N));
#endif
}

ccl_device_inline void object_normal_transform(KernelGlobals *kg, ShaderData *sd, float3 *N)
{
#ifdef __OBJECT_MOTION__
	*N = normalize(transform_direction_transposed(&sd->ob_itfm, *N));
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
	*N = normalize(transform_direction_transposed(&tfm, *N));
#endif
}

ccl_device_inline void object_dir_transform(KernelGlobals *kg, ShaderData *sd, float3 *D)
{
#ifdef __OBJECT_MOTION__
	*D = transform_direction(&sd->ob_tfm, *D);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	*D = transform_direction(&tfm, *D);
#endif
}

ccl_device_inline void object_inverse_dir_transform(KernelGlobals *kg, ShaderData *sd, float3 *D)
{
#ifdef __OBJECT_MOTION__
	*D = transform_direction(&sd->ob_itfm, *D);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
	*D = transform_direction(&tfm, *D);
#endif
}

ccl_device_inline float3 object_location(KernelGlobals *kg, ShaderData *sd)
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

ccl_device_inline float object_surface_area(KernelGlobals *kg, int object)
{
	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.x;
}

ccl_device_inline float object_pass_id(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.y;
}

ccl_device_inline float object_random_number(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.z;
}

ccl_device_inline uint object_particle_id(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return __float_as_uint(f.w);
}

ccl_device_inline float3 object_dupli_generated(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return make_float3(0.0f, 0.0f, 0.0f);

	int offset = object*OBJECT_SIZE + OBJECT_DUPLI;
	float4 f = kernel_tex_fetch(__objects, offset);
	return make_float3(f.x, f.y, f.z);
}

ccl_device_inline float3 object_dupli_uv(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return make_float3(0.0f, 0.0f, 0.0f);

	int offset = object*OBJECT_SIZE + OBJECT_DUPLI;
	float4 f = kernel_tex_fetch(__objects, offset + 1);
	return make_float3(f.x, f.y, 0.0f);
}


ccl_device int shader_pass_id(KernelGlobals *kg, ShaderData *sd)
{
	return kernel_tex_fetch(__shader_flag, (sd->shader & SHADER_MASK)*2 + 1);
}

ccl_device_inline float particle_index(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 0);
	return f.x;
}

ccl_device float particle_age(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 0);
	return f.y;
}

ccl_device float particle_lifetime(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 0);
	return f.z;
}

ccl_device float particle_size(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 0);
	return f.w;
}

ccl_device float4 particle_rotation(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 1);
	return f;
}

ccl_device float3 particle_location(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f = kernel_tex_fetch(__particles, offset + 2);
	return make_float3(f.x, f.y, f.z);
}

ccl_device float3 particle_velocity(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f2 = kernel_tex_fetch(__particles, offset + 2);
	float4 f3 = kernel_tex_fetch(__particles, offset + 3);
	return make_float3(f2.w, f3.x, f3.y);
}

ccl_device float3 particle_angular_velocity(KernelGlobals *kg, int particle)
{
	int offset = particle*PARTICLE_SIZE;
	float4 f3 = kernel_tex_fetch(__particles, offset + 3);
	float4 f4 = kernel_tex_fetch(__particles, offset + 4);
	return make_float3(f3.z, f3.w, f4.x);
}

CCL_NAMESPACE_END

