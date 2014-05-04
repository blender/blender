/*
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

/* Object Primitive
 *
 * All mesh and curve primitives are part of an object. The same mesh and curves
 * may be instanced multiple times by different objects.
 *
 * If the mesh is not instanced multiple times, the object will not be explicitly
 * stored as a primitive in the BVH, rather the bare triangles are curved are
 * directly primitives in the BVH with world space locations applied, and the object
 * ID is looked up afterwards. */

CCL_NAMESPACE_BEGIN

/* Object attributes, for now a fixed size and contents */

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

/* Object to world space transformation */

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

/* Object to world space transformation for motion vectors */

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

/* Motion blurred object transformations */

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

/* Transform position from object to world space */

ccl_device_inline void object_position_transform(KernelGlobals *kg, const ShaderData *sd, float3 *P)
{
#ifdef __OBJECT_MOTION__
	*P = transform_point(&sd->ob_tfm, *P);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	*P = transform_point(&tfm, *P);
#endif
}

/* Transform position from world to object space */

ccl_device_inline void object_inverse_position_transform(KernelGlobals *kg, const ShaderData *sd, float3 *P)
{
#ifdef __OBJECT_MOTION__
	*P = transform_point(&sd->ob_itfm, *P);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
	*P = transform_point(&tfm, *P);
#endif
}

/* Transform normal from world to object space */

ccl_device_inline void object_inverse_normal_transform(KernelGlobals *kg, const ShaderData *sd, float3 *N)
{
#ifdef __OBJECT_MOTION__
	*N = normalize(transform_direction_transposed(&sd->ob_tfm, *N));
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	*N = normalize(transform_direction_transposed(&tfm, *N));
#endif
}

/* Transform normal from object to world space */

ccl_device_inline void object_normal_transform(KernelGlobals *kg, const ShaderData *sd, float3 *N)
{
#ifdef __OBJECT_MOTION__
	*N = normalize(transform_direction_transposed(&sd->ob_itfm, *N));
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
	*N = normalize(transform_direction_transposed(&tfm, *N));
#endif
}

/* Transform direction vector from object to world space */

ccl_device_inline void object_dir_transform(KernelGlobals *kg, const ShaderData *sd, float3 *D)
{
#ifdef __OBJECT_MOTION__
	*D = transform_direction(&sd->ob_tfm, *D);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	*D = transform_direction(&tfm, *D);
#endif
}

/* Transform direction vector from world to object space */

ccl_device_inline void object_inverse_dir_transform(KernelGlobals *kg, const ShaderData *sd, float3 *D)
{
#ifdef __OBJECT_MOTION__
	*D = transform_direction(&sd->ob_itfm, *D);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
	*D = transform_direction(&tfm, *D);
#endif
}

/* Object center position */

ccl_device_inline float3 object_location(KernelGlobals *kg, const ShaderData *sd)
{
	if(sd->object == OBJECT_NONE)
		return make_float3(0.0f, 0.0f, 0.0f);

#ifdef __OBJECT_MOTION__
	return make_float3(sd->ob_tfm.x.w, sd->ob_tfm.y.w, sd->ob_tfm.z.w);
#else
	Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
	return make_float3(tfm.x.w, tfm.y.w, tfm.z.w);
#endif
}

/* Total surface area of object */

ccl_device_inline float object_surface_area(KernelGlobals *kg, int object)
{
	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.x;
}

/* Pass ID number of object */

ccl_device_inline float object_pass_id(KernelGlobals *kg, int object)
{
	if(object == OBJECT_NONE)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.y;
}

/* Per object random number for shader variation */

ccl_device_inline float object_random_number(KernelGlobals *kg, int object)
{
	if(object == OBJECT_NONE)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.z;
}

/* Particle ID from which this object was generated */

ccl_device_inline int object_particle_id(KernelGlobals *kg, int object)
{
	if(object == OBJECT_NONE)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return __float_as_uint(f.w);
}

/* Generated texture coordinate on surface from where object was instanced */

ccl_device_inline float3 object_dupli_generated(KernelGlobals *kg, int object)
{
	if(object == OBJECT_NONE)
		return make_float3(0.0f, 0.0f, 0.0f);

	int offset = object*OBJECT_SIZE + OBJECT_DUPLI;
	float4 f = kernel_tex_fetch(__objects, offset);
	return make_float3(f.x, f.y, f.z);
}

/* UV texture coordinate on surface from where object was instanced */

ccl_device_inline float3 object_dupli_uv(KernelGlobals *kg, int object)
{
	if(object == OBJECT_NONE)
		return make_float3(0.0f, 0.0f, 0.0f);

	int offset = object*OBJECT_SIZE + OBJECT_DUPLI;
	float4 f = kernel_tex_fetch(__objects, offset + 1);
	return make_float3(f.x, f.y, 0.0f);
}

/* Information about mesh for motion blurred triangles and curves */

ccl_device_inline void object_motion_info(KernelGlobals *kg, int object, int *numsteps, int *numverts, int *numkeys)
{
	int offset = object*OBJECT_SIZE + OBJECT_DUPLI;

	if(numkeys) {
		float4 f = kernel_tex_fetch(__objects, offset);
		*numkeys = __float_as_int(f.w);
	}

	float4 f = kernel_tex_fetch(__objects, offset + 1);
	if(numsteps)
		*numsteps = __float_as_int(f.z);
	if(numverts)
		*numverts = __float_as_int(f.w);
}

/* Pass ID for shader */

ccl_device int shader_pass_id(KernelGlobals *kg, const ShaderData *sd)
{
	return kernel_tex_fetch(__shader_flag, (sd->shader & SHADER_MASK)*2 + 1);
}

/* Particle data from which object was instanced */

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

/* Object intersection in BVH */

ccl_device_inline float3 bvh_clamp_direction(float3 dir)
{
	/* clamp absolute values by exp2f(-80.0f) to avoid division by zero when calculating inverse direction */
	float ooeps = 8.271806E-25f;
	return make_float3((fabsf(dir.x) > ooeps)? dir.x: copysignf(ooeps, dir.x),
	                   (fabsf(dir.y) > ooeps)? dir.y: copysignf(ooeps, dir.y),
	                   (fabsf(dir.z) > ooeps)? dir.z: copysignf(ooeps, dir.z));
}

ccl_device_inline float3 bvh_inverse_direction(float3 dir)
{
	return 1.0f / dir;
}

/* Transform ray into object space to enter static object in BVH */

ccl_device_inline void bvh_instance_push(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *dir, float3 *idir, float *t)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);

	*P = transform_point(&tfm, ray->P);

	float len;
	*dir = bvh_clamp_direction(normalize_len(transform_direction(&tfm, ray->D), &len));
	*idir = bvh_inverse_direction(*dir);

	if(*t != FLT_MAX)
		*t *= len;
}

/* Transorm ray to exit static object in BVH */

ccl_device_inline void bvh_instance_pop(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *dir, float3 *idir, float *t)
{
	if(*t != FLT_MAX) {
		Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
		*t *= len(transform_direction(&tfm, 1.0f/(*idir)));
	}

	*P = ray->P;
	*dir = bvh_clamp_direction(ray->D);
	*idir = bvh_inverse_direction(*dir);
}

/* Same as above, but returns scale factor to apply to multiple intersection distances */

ccl_device_inline void bvh_instance_pop_factor(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *dir, float3 *idir, float *t_fac)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
	*t_fac = len(transform_direction(&tfm, 1.0f/(*idir)));

	*P = ray->P;
	*dir = bvh_clamp_direction(ray->D);
	*idir = bvh_inverse_direction(*dir);
}


#ifdef __OBJECT_MOTION__
/* Transform ray into object space to enter motion blurred object in BVH */

ccl_device_inline void bvh_instance_motion_push(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *dir, float3 *idir, float *t, Transform *tfm)
{
	Transform itfm;
	*tfm = object_fetch_transform_motion_test(kg, object, ray->time, &itfm);

	*P = transform_point(&itfm, ray->P);

	float len;
	*dir = bvh_clamp_direction(normalize_len(transform_direction(&itfm, ray->D), &len));
	*idir = bvh_inverse_direction(*dir);

	if(*t != FLT_MAX)
		*t *= len;
}

/* Transorm ray to exit motion blurred object in BVH */

ccl_device_inline void bvh_instance_motion_pop(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *dir, float3 *idir, float *t, Transform *tfm)
{
	if(*t != FLT_MAX)
		*t *= len(transform_direction(tfm, 1.0f/(*idir)));

	*P = ray->P;
	*dir = bvh_clamp_direction(ray->D);
	*idir = bvh_inverse_direction(*dir);
}

/* Same as above, but returns scale factor to apply to multiple intersection distances */

ccl_device_inline void bvh_instance_motion_pop_factor(KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *dir, float3 *idir, float *t_fac, Transform *tfm)
{
	*t_fac = len(transform_direction(tfm, 1.0f/(*idir)));

	*P = ray->P;
	*dir = bvh_clamp_direction(ray->D);
	*idir = bvh_inverse_direction(*dir);
}

#endif

CCL_NAMESPACE_END

