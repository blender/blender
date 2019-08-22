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
  OBJECT_INVERSE_TRANSFORM = 1,
};

enum ObjectVectorTransform { OBJECT_PASS_MOTION_PRE = 0, OBJECT_PASS_MOTION_POST = 1 };

/* Object to world space transformation */

ccl_device_inline Transform object_fetch_transform(KernelGlobals *kg,
                                                   int object,
                                                   enum ObjectTransform type)
{
  if (type == OBJECT_INVERSE_TRANSFORM) {
    return kernel_tex_fetch(__objects, object).itfm;
  }
  else {
    return kernel_tex_fetch(__objects, object).tfm;
  }
}

/* Lamp to world space transformation */

ccl_device_inline Transform lamp_fetch_transform(KernelGlobals *kg, int lamp, bool inverse)
{
  if (inverse) {
    return kernel_tex_fetch(__lights, lamp).itfm;
  }
  else {
    return kernel_tex_fetch(__lights, lamp).tfm;
  }
}

/* Object to world space transformation for motion vectors */

ccl_device_inline Transform object_fetch_motion_pass_transform(KernelGlobals *kg,
                                                               int object,
                                                               enum ObjectVectorTransform type)
{
  int offset = object * OBJECT_MOTION_PASS_SIZE + (int)type;
  return kernel_tex_fetch(__object_motion_pass, offset);
}

/* Motion blurred object transformations */

#ifdef __OBJECT_MOTION__
ccl_device_inline Transform object_fetch_transform_motion(KernelGlobals *kg,
                                                          int object,
                                                          float time)
{
  const uint motion_offset = kernel_tex_fetch(__objects, object).motion_offset;
  const ccl_global DecomposedTransform *motion = &kernel_tex_fetch(__object_motion, motion_offset);
  const uint num_steps = kernel_tex_fetch(__objects, object).numsteps * 2 + 1;

  Transform tfm;
#  ifdef __EMBREE__
  if (kernel_data.bvh.scene) {
    transform_motion_array_interpolate_straight(&tfm, motion, num_steps, time);
  }
  else
#  endif
    transform_motion_array_interpolate(&tfm, motion, num_steps, time);

  return tfm;
}

ccl_device_inline Transform object_fetch_transform_motion_test(KernelGlobals *kg,
                                                               int object,
                                                               float time,
                                                               Transform *itfm)
{
  int object_flag = kernel_tex_fetch(__object_flag, object);
  if (object_flag & SD_OBJECT_MOTION) {
    /* if we do motion blur */
    Transform tfm = object_fetch_transform_motion(kg, object, time);

    if (itfm)
      *itfm = transform_quick_inverse(tfm);

    return tfm;
  }
  else {
    Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
    if (itfm)
      *itfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);

    return tfm;
  }
}
#endif

/* Transform position from object to world space */

ccl_device_inline void object_position_transform(KernelGlobals *kg,
                                                 const ShaderData *sd,
                                                 float3 *P)
{
#ifdef __OBJECT_MOTION__
  *P = transform_point_auto(&sd->ob_tfm, *P);
#else
  Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
  *P = transform_point(&tfm, *P);
#endif
}

/* Transform position from world to object space */

ccl_device_inline void object_inverse_position_transform(KernelGlobals *kg,
                                                         const ShaderData *sd,
                                                         float3 *P)
{
#ifdef __OBJECT_MOTION__
  *P = transform_point_auto(&sd->ob_itfm, *P);
#else
  Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
  *P = transform_point(&tfm, *P);
#endif
}

/* Transform normal from world to object space */

ccl_device_inline void object_inverse_normal_transform(KernelGlobals *kg,
                                                       const ShaderData *sd,
                                                       float3 *N)
{
#ifdef __OBJECT_MOTION__
  if ((sd->object != OBJECT_NONE) || (sd->type == PRIMITIVE_LAMP)) {
    *N = normalize(transform_direction_transposed_auto(&sd->ob_tfm, *N));
  }
#else
  if (sd->object != OBJECT_NONE) {
    Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
    *N = normalize(transform_direction_transposed(&tfm, *N));
  }
  else if (sd->type == PRIMITIVE_LAMP) {
    Transform tfm = lamp_fetch_transform(kg, sd->lamp, false);
    *N = normalize(transform_direction_transposed(&tfm, *N));
  }
#endif
}

/* Transform normal from object to world space */

ccl_device_inline void object_normal_transform(KernelGlobals *kg, const ShaderData *sd, float3 *N)
{
#ifdef __OBJECT_MOTION__
  *N = normalize(transform_direction_transposed_auto(&sd->ob_itfm, *N));
#else
  Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
  *N = normalize(transform_direction_transposed(&tfm, *N));
#endif
}

/* Transform direction vector from object to world space */

ccl_device_inline void object_dir_transform(KernelGlobals *kg, const ShaderData *sd, float3 *D)
{
#ifdef __OBJECT_MOTION__
  *D = transform_direction_auto(&sd->ob_tfm, *D);
#else
  Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
  *D = transform_direction(&tfm, *D);
#endif
}

/* Transform direction vector from world to object space */

ccl_device_inline void object_inverse_dir_transform(KernelGlobals *kg,
                                                    const ShaderData *sd,
                                                    float3 *D)
{
#ifdef __OBJECT_MOTION__
  *D = transform_direction_auto(&sd->ob_itfm, *D);
#else
  Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
  *D = transform_direction(&tfm, *D);
#endif
}

/* Object center position */

ccl_device_inline float3 object_location(KernelGlobals *kg, const ShaderData *sd)
{
  if (sd->object == OBJECT_NONE)
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
  return kernel_tex_fetch(__objects, object).surface_area;
}

/* Color of the object */

ccl_device_inline float3 object_color(KernelGlobals *kg, int object)
{
  if (object == OBJECT_NONE)
    return make_float3(0.0f, 0.0f, 0.0f);

  const ccl_global KernelObject *kobject = &kernel_tex_fetch(__objects, object);
  return make_float3(kobject->color[0], kobject->color[1], kobject->color[2]);
}

/* Pass ID number of object */

ccl_device_inline float object_pass_id(KernelGlobals *kg, int object)
{
  if (object == OBJECT_NONE)
    return 0.0f;

  return kernel_tex_fetch(__objects, object).pass_id;
}

/* Per lamp random number for shader variation */

ccl_device_inline float lamp_random_number(KernelGlobals *kg, int lamp)
{
  if (lamp == LAMP_NONE)
    return 0.0f;

  return kernel_tex_fetch(__lights, lamp).random;
}

/* Per object random number for shader variation */

ccl_device_inline float object_random_number(KernelGlobals *kg, int object)
{
  if (object == OBJECT_NONE)
    return 0.0f;

  return kernel_tex_fetch(__objects, object).random_number;
}

/* Particle ID from which this object was generated */

ccl_device_inline int object_particle_id(KernelGlobals *kg, int object)
{
  if (object == OBJECT_NONE)
    return 0;

  return kernel_tex_fetch(__objects, object).particle_index;
}

/* Generated texture coordinate on surface from where object was instanced */

ccl_device_inline float3 object_dupli_generated(KernelGlobals *kg, int object)
{
  if (object == OBJECT_NONE)
    return make_float3(0.0f, 0.0f, 0.0f);

  const ccl_global KernelObject *kobject = &kernel_tex_fetch(__objects, object);
  return make_float3(
      kobject->dupli_generated[0], kobject->dupli_generated[1], kobject->dupli_generated[2]);
}

/* UV texture coordinate on surface from where object was instanced */

ccl_device_inline float3 object_dupli_uv(KernelGlobals *kg, int object)
{
  if (object == OBJECT_NONE)
    return make_float3(0.0f, 0.0f, 0.0f);

  const ccl_global KernelObject *kobject = &kernel_tex_fetch(__objects, object);
  return make_float3(kobject->dupli_uv[0], kobject->dupli_uv[1], 0.0f);
}

/* Information about mesh for motion blurred triangles and curves */

ccl_device_inline void object_motion_info(
    KernelGlobals *kg, int object, int *numsteps, int *numverts, int *numkeys)
{
  if (numkeys) {
    *numkeys = kernel_tex_fetch(__objects, object).numkeys;
  }

  if (numsteps)
    *numsteps = kernel_tex_fetch(__objects, object).numsteps;
  if (numverts)
    *numverts = kernel_tex_fetch(__objects, object).numverts;
}

/* Offset to an objects patch map */

ccl_device_inline uint object_patch_map_offset(KernelGlobals *kg, int object)
{
  if (object == OBJECT_NONE)
    return 0;

  return kernel_tex_fetch(__objects, object).patch_map_offset;
}

/* Pass ID for shader */

ccl_device int shader_pass_id(KernelGlobals *kg, const ShaderData *sd)
{
  return kernel_tex_fetch(__shaders, (sd->shader & SHADER_MASK)).pass_id;
}

/* Cryptomatte ID */

ccl_device_inline float object_cryptomatte_id(KernelGlobals *kg, int object)
{
  if (object == OBJECT_NONE)
    return 0.0f;

  return kernel_tex_fetch(__objects, object).cryptomatte_object;
}

ccl_device_inline float object_cryptomatte_asset_id(KernelGlobals *kg, int object)
{
  if (object == OBJECT_NONE)
    return 0;

  return kernel_tex_fetch(__objects, object).cryptomatte_asset;
}

/* Particle data from which object was instanced */

ccl_device_inline uint particle_index(KernelGlobals *kg, int particle)
{
  return kernel_tex_fetch(__particles, particle).index;
}

ccl_device float particle_age(KernelGlobals *kg, int particle)
{
  return kernel_tex_fetch(__particles, particle).age;
}

ccl_device float particle_lifetime(KernelGlobals *kg, int particle)
{
  return kernel_tex_fetch(__particles, particle).lifetime;
}

ccl_device float particle_size(KernelGlobals *kg, int particle)
{
  return kernel_tex_fetch(__particles, particle).size;
}

ccl_device float4 particle_rotation(KernelGlobals *kg, int particle)
{
  return kernel_tex_fetch(__particles, particle).rotation;
}

ccl_device float3 particle_location(KernelGlobals *kg, int particle)
{
  return float4_to_float3(kernel_tex_fetch(__particles, particle).location);
}

ccl_device float3 particle_velocity(KernelGlobals *kg, int particle)
{
  return float4_to_float3(kernel_tex_fetch(__particles, particle).velocity);
}

ccl_device float3 particle_angular_velocity(KernelGlobals *kg, int particle)
{
  return float4_to_float3(kernel_tex_fetch(__particles, particle).angular_velocity);
}

/* Object intersection in BVH */

ccl_device_inline float3 bvh_clamp_direction(float3 dir)
{
  /* clamp absolute values by exp2f(-80.0f) to avoid division by zero when calculating inverse
   * direction */
#if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE2__)
  const ssef oopes(8.271806E-25f, 8.271806E-25f, 8.271806E-25f, 0.0f);
  const ssef mask = _mm_cmpgt_ps(fabs(dir), oopes);
  const ssef signdir = signmsk(dir.m128) | oopes;
#  ifndef __KERNEL_AVX__
  ssef res = mask & ssef(dir);
  res = _mm_or_ps(res, _mm_andnot_ps(mask, signdir));
#  else
  ssef res = _mm_blendv_ps(signdir, dir, mask);
#  endif
  return float3(res);
#else  /* __KERNEL_SSE__ && __KERNEL_SSE2__ */
  const float ooeps = 8.271806E-25f;
  return make_float3((fabsf(dir.x) > ooeps) ? dir.x : copysignf(ooeps, dir.x),
                     (fabsf(dir.y) > ooeps) ? dir.y : copysignf(ooeps, dir.y),
                     (fabsf(dir.z) > ooeps) ? dir.z : copysignf(ooeps, dir.z));
#endif /* __KERNEL_SSE__ && __KERNEL_SSE2__ */
}

ccl_device_inline float3 bvh_inverse_direction(float3 dir)
{
  return rcp(dir);
}

/* Transform ray into object space to enter static object in BVH */

ccl_device_inline float bvh_instance_push(
    KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *dir, float3 *idir, float t)
{
  Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);

  *P = transform_point(&tfm, ray->P);

  float len;
  *dir = bvh_clamp_direction(normalize_len(transform_direction(&tfm, ray->D), &len));
  *idir = bvh_inverse_direction(*dir);

  if (t != FLT_MAX) {
    t *= len;
  }

  return t;
}

#ifdef __QBVH__
/* Same as above, but optimized for QBVH scene intersection,
 * which needs to modify two max distances.
 *
 * TODO(sergey): Investigate if passing NULL instead of t1 gets optimized
 * so we can avoid having this duplication.
 */
ccl_device_inline void qbvh_instance_push(KernelGlobals *kg,
                                          int object,
                                          const Ray *ray,
                                          float3 *P,
                                          float3 *dir,
                                          float3 *idir,
                                          float *t,
                                          float *t1)
{
  Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);

  *P = transform_point(&tfm, ray->P);

  float len;
  *dir = bvh_clamp_direction(normalize_len(transform_direction(&tfm, ray->D), &len));
  *idir = bvh_inverse_direction(*dir);

  if (*t != FLT_MAX)
    *t *= len;

  if (*t1 != -FLT_MAX)
    *t1 *= len;
}
#endif

/* Transorm ray to exit static object in BVH */

ccl_device_inline float bvh_instance_pop(
    KernelGlobals *kg, int object, const Ray *ray, float3 *P, float3 *dir, float3 *idir, float t)
{
  if (t != FLT_MAX) {
    Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
    t /= len(transform_direction(&tfm, ray->D));
  }

  *P = ray->P;
  *dir = bvh_clamp_direction(ray->D);
  *idir = bvh_inverse_direction(*dir);

  return t;
}

/* Same as above, but returns scale factor to apply to multiple intersection distances */

ccl_device_inline void bvh_instance_pop_factor(KernelGlobals *kg,
                                               int object,
                                               const Ray *ray,
                                               float3 *P,
                                               float3 *dir,
                                               float3 *idir,
                                               float *t_fac)
{
  Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
  *t_fac = 1.0f / len(transform_direction(&tfm, ray->D));

  *P = ray->P;
  *dir = bvh_clamp_direction(ray->D);
  *idir = bvh_inverse_direction(*dir);
}

#ifdef __OBJECT_MOTION__
/* Transform ray into object space to enter motion blurred object in BVH */

ccl_device_inline float bvh_instance_motion_push(KernelGlobals *kg,
                                                 int object,
                                                 const Ray *ray,
                                                 float3 *P,
                                                 float3 *dir,
                                                 float3 *idir,
                                                 float t,
                                                 Transform *itfm)
{
  object_fetch_transform_motion_test(kg, object, ray->time, itfm);

  *P = transform_point(itfm, ray->P);

  float len;
  *dir = bvh_clamp_direction(normalize_len(transform_direction(itfm, ray->D), &len));
  *idir = bvh_inverse_direction(*dir);

  if (t != FLT_MAX) {
    t *= len;
  }

  return t;
}

#  ifdef __QBVH__
/* Same as above, but optimized for QBVH scene intersection,
 * which needs to modify two max distances.
 *
 * TODO(sergey): Investigate if passing NULL instead of t1 gets optimized
 * so we can avoid having this duplication.
 */
ccl_device_inline void qbvh_instance_motion_push(KernelGlobals *kg,
                                                 int object,
                                                 const Ray *ray,
                                                 float3 *P,
                                                 float3 *dir,
                                                 float3 *idir,
                                                 float *t,
                                                 float *t1,
                                                 Transform *itfm)
{
  object_fetch_transform_motion_test(kg, object, ray->time, itfm);

  *P = transform_point(itfm, ray->P);

  float len;
  *dir = bvh_clamp_direction(normalize_len(transform_direction(itfm, ray->D), &len));
  *idir = bvh_inverse_direction(*dir);

  if (*t != FLT_MAX)
    *t *= len;

  if (*t1 != -FLT_MAX)
    *t1 *= len;
}
#  endif

/* Transorm ray to exit motion blurred object in BVH */

ccl_device_inline float bvh_instance_motion_pop(KernelGlobals *kg,
                                                int object,
                                                const Ray *ray,
                                                float3 *P,
                                                float3 *dir,
                                                float3 *idir,
                                                float t,
                                                Transform *itfm)
{
  if (t != FLT_MAX) {
    t /= len(transform_direction(itfm, ray->D));
  }

  *P = ray->P;
  *dir = bvh_clamp_direction(ray->D);
  *idir = bvh_inverse_direction(*dir);

  return t;
}

/* Same as above, but returns scale factor to apply to multiple intersection distances */

ccl_device_inline void bvh_instance_motion_pop_factor(KernelGlobals *kg,
                                                      int object,
                                                      const Ray *ray,
                                                      float3 *P,
                                                      float3 *dir,
                                                      float3 *idir,
                                                      float *t_fac,
                                                      Transform *itfm)
{
  *t_fac = 1.0f / len(transform_direction(itfm, ray->D));
  *P = ray->P;
  *dir = bvh_clamp_direction(ray->D);
  *idir = bvh_inverse_direction(*dir);
}

#endif

/* TODO(sergey): This is only for until we've got OpenCL 2.0
 * on all devices we consider supported. It'll be replaced with
 * generic address space.
 */

#ifdef __KERNEL_OPENCL__
ccl_device_inline void object_position_transform_addrspace(KernelGlobals *kg,
                                                           const ShaderData *sd,
                                                           ccl_addr_space float3 *P)
{
  float3 private_P = *P;
  object_position_transform(kg, sd, &private_P);
  *P = private_P;
}

ccl_device_inline void object_dir_transform_addrspace(KernelGlobals *kg,
                                                      const ShaderData *sd,
                                                      ccl_addr_space float3 *D)
{
  float3 private_D = *D;
  object_dir_transform(kg, sd, &private_D);
  *D = private_D;
}

ccl_device_inline void object_normal_transform_addrspace(KernelGlobals *kg,
                                                         const ShaderData *sd,
                                                         ccl_addr_space float3 *N)
{
  float3 private_N = *N;
  object_normal_transform(kg, sd, &private_N);
  *N = private_N;
}
#endif

#ifndef __KERNEL_OPENCL__
#  define object_position_transform_auto object_position_transform
#  define object_dir_transform_auto object_dir_transform
#  define object_normal_transform_auto object_normal_transform
#else
#  define object_position_transform_auto object_position_transform_addrspace
#  define object_dir_transform_auto object_dir_transform_addrspace
#  define object_normal_transform_auto object_normal_transform_addrspace
#endif

CCL_NAMESPACE_END
