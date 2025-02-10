/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Object Primitive
 *
 * All mesh and curve primitives are part of an object. The same mesh and curves
 * may be instanced multiple times by different objects.
 *
 * If the mesh is not instanced multiple times, the object will not be explicitly
 * stored as a primitive in the BVH, rather the bare triangles are curved are
 * directly primitives in the BVH with world space locations applied, and the object
 * ID is looked up afterwards. */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"

CCL_NAMESPACE_BEGIN

/* Object attributes, for now a fixed size and contents */

enum ObjectTransform {
  OBJECT_TRANSFORM = 0,
  OBJECT_INVERSE_TRANSFORM = 1,
};

enum ObjectVectorTransform { OBJECT_PASS_MOTION_PRE = 0, OBJECT_PASS_MOTION_POST = 1 };

/* Object to world space transformation */

ccl_device_inline Transform object_fetch_transform(KernelGlobals kg,
                                                   const int object,
                                                   enum ObjectTransform type)
{
  if (type == OBJECT_INVERSE_TRANSFORM) {
    return kernel_data_fetch(objects, object).itfm;
  }
  return kernel_data_fetch(objects, object).tfm;
}

/* Lamp to world space transformation */

ccl_device_inline Transform lamp_fetch_transform(KernelGlobals kg, const int lamp, bool inverse)
{
  if (inverse) {
    return kernel_data_fetch(lights, lamp).itfm;
  }
  return kernel_data_fetch(lights, lamp).tfm;
}

/* Object to world space transformation for motion vectors */

ccl_device_inline Transform object_fetch_motion_pass_transform(KernelGlobals kg,
                                                               const int object,
                                                               enum ObjectVectorTransform type)
{
  const int offset = object * OBJECT_MOTION_PASS_SIZE + (int)type;
  return kernel_data_fetch(object_motion_pass, offset);
}

/* Motion blurred object transformations */

#ifdef __OBJECT_MOTION__
ccl_device_inline Transform object_fetch_transform_motion(KernelGlobals kg,
                                                          const int object,
                                                          const float time)
{
  const uint motion_offset = kernel_data_fetch(objects, object).motion_offset;
  const ccl_global DecomposedTransform *motion = &kernel_data_fetch(object_motion, motion_offset);
  const uint num_steps = kernel_data_fetch(objects, object).num_tfm_steps;

  Transform tfm;
  transform_motion_array_interpolate(&tfm, motion, num_steps, time);

  return tfm;
}
#endif /* __OBJECT_MOTION__ */

ccl_device_inline Transform object_fetch_transform_motion_test(KernelGlobals kg,
                                                               const int object,
                                                               const float time,
                                                               ccl_private Transform *itfm)
{
#ifdef __OBJECT_MOTION__
  const int object_flag = kernel_data_fetch(object_flag, object);
  if (object_flag & SD_OBJECT_MOTION) {
    /* if we do motion blur */
    Transform tfm = object_fetch_transform_motion(kg, object, time);

    if (itfm) {
      *itfm = transform_inverse(tfm);
    }

    return tfm;
  }

#endif /* __OBJECT_MOTION__ */

  Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
  if (itfm) {
    *itfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
  }

  return tfm;
}

/* Get transform matrix for shading point. */

ccl_device_inline Transform object_get_transform(KernelGlobals kg,
                                                 const ccl_private ShaderData *sd)
{
#ifdef __OBJECT_MOTION__
  return (sd->object_flag & SD_OBJECT_MOTION) ?
             sd->ob_tfm_motion :
             object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
#else
  return object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
#endif
}

ccl_device_inline Transform object_get_inverse_transform(KernelGlobals kg,
                                                         const ccl_private ShaderData *sd)
{
#ifdef __OBJECT_MOTION__
  return (sd->object_flag & SD_OBJECT_MOTION) ?
             sd->ob_itfm_motion :
             object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
#else
  return object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
#endif
}
/* Transform position from object to world space */

ccl_device_inline void object_position_transform(KernelGlobals kg,
                                                 const ccl_private ShaderData *sd,
                                                 ccl_private float3 *P)
{
#ifdef __OBJECT_MOTION__
  if (sd->object_flag & SD_OBJECT_MOTION) {
    *P = transform_point_auto(&sd->ob_tfm_motion, *P);
    return;
  }
#endif

  const Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
  *P = transform_point(&tfm, *P);
}

/* Transform position from world to object space */

ccl_device_inline void object_inverse_position_transform(KernelGlobals kg,
                                                         const ccl_private ShaderData *sd,
                                                         ccl_private float3 *P)
{
#ifdef __OBJECT_MOTION__
  if (sd->object_flag & SD_OBJECT_MOTION) {
    *P = transform_point_auto(&sd->ob_itfm_motion, *P);
    return;
  }
#endif

  const Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
  *P = transform_point(&tfm, *P);
}

/* Transform normal from world to object space */

ccl_device_inline void object_inverse_normal_transform(KernelGlobals kg,
                                                       const ccl_private ShaderData *sd,
                                                       ccl_private float3 *N)
{
#ifdef __OBJECT_MOTION__
  if (sd->object_flag & SD_OBJECT_MOTION) {
    if ((sd->object != OBJECT_NONE) || (sd->type == PRIMITIVE_LAMP)) {
      *N = safe_normalize(transform_direction_transposed_auto(&sd->ob_tfm_motion, *N));
    }
    return;
  }
#endif

  if (sd->object != OBJECT_NONE) {
    const Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
    *N = safe_normalize(transform_direction_transposed(&tfm, *N));
  }
  else if (sd->type == PRIMITIVE_LAMP) {
    const Transform tfm = lamp_fetch_transform(kg, sd->lamp, false);
    *N = safe_normalize(transform_direction_transposed(&tfm, *N));
  }
}

/* Transform normal from object to world space */

ccl_device_inline void object_normal_transform(KernelGlobals kg,
                                               const ccl_private ShaderData *sd,
                                               ccl_private float3 *N)
{
#ifdef __OBJECT_MOTION__
  if (sd->object_flag & SD_OBJECT_MOTION) {
    *N = normalize(transform_direction_transposed_auto(&sd->ob_itfm_motion, *N));
    return;
  }
#endif

  if (sd->object != OBJECT_NONE) {
    const Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
    *N = normalize(transform_direction_transposed(&tfm, *N));
  }
  else if (sd->type == PRIMITIVE_LAMP) {
    const Transform tfm = lamp_fetch_transform(kg, sd->lamp, true);
    *N = normalize(transform_direction_transposed(&tfm, *N));
  }
}

ccl_device_inline bool object_negative_scale_applied(const int object_flag)
{
  return ((object_flag & SD_OBJECT_NEGATIVE_SCALE) && (object_flag & SD_OBJECT_TRANSFORM_APPLIED));
}

/* Transform direction vector from object to world space */

ccl_device_inline void object_dir_transform(KernelGlobals kg,
                                            const ccl_private ShaderData *sd,
                                            ccl_private float3 *D)
{
#ifdef __OBJECT_MOTION__
  if (sd->object_flag & SD_OBJECT_MOTION) {
    *D = transform_direction_auto(&sd->ob_tfm_motion, *D);
    return;
  }
#endif

  const Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
  *D = transform_direction(&tfm, *D);
}

/* Transform direction vector from world to object space */

ccl_device_inline void object_inverse_dir_transform(KernelGlobals kg,
                                                    const ccl_private ShaderData *sd,
                                                    ccl_private float3 *D)
{
#ifdef __OBJECT_MOTION__
  if (sd->object_flag & SD_OBJECT_MOTION) {
    *D = transform_direction_auto(&sd->ob_itfm_motion, *D);
    return;
  }
#endif

  const Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
  *D = transform_direction(&tfm, *D);
}

/* Object center position */

ccl_device_inline float3 object_location(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  if (sd->object == OBJECT_NONE) {
    return make_float3(0.0f, 0.0f, 0.0f);
  }

#ifdef __OBJECT_MOTION__
  if (sd->object_flag & SD_OBJECT_MOTION) {
    return make_float3(sd->ob_tfm_motion.x.w, sd->ob_tfm_motion.y.w, sd->ob_tfm_motion.z.w);
  }
#endif

  const Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
  return make_float3(tfm.x.w, tfm.y.w, tfm.z.w);
}

/* Color of the object */

ccl_device_inline float3 object_color(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return make_float3(0.0f, 0.0f, 0.0f);
  }

  const ccl_global KernelObject *kobject = &kernel_data_fetch(objects, object);
  return make_float3(kobject->color[0], kobject->color[1], kobject->color[2]);
}

/* Alpha of the object */

ccl_device_inline float object_alpha(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return 0.0f;
  }

  return kernel_data_fetch(objects, object).alpha;
}

/* Pass ID number of object */

ccl_device_inline float object_pass_id(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return 0.0f;
  }

  return kernel_data_fetch(objects, object).pass_id;
}

/* Light-group of lamp. */

ccl_device_inline int lamp_lightgroup(KernelGlobals kg, const int lamp)
{
  if (lamp == LAMP_NONE) {
    return LIGHTGROUP_NONE;
  }

  return kernel_data_fetch(lights, lamp).lightgroup;
}

/* Light-group of object. */

ccl_device_inline int object_lightgroup(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return LIGHTGROUP_NONE;
  }

  return kernel_data_fetch(objects, object).lightgroup;
}

/* Per lamp random number for shader variation */

ccl_device_inline float lamp_random_number(KernelGlobals kg, const int lamp)
{
  if (lamp == LAMP_NONE) {
    return 0.0f;
  }

  return kernel_data_fetch(lights, lamp).random;
}

/* Per object random number for shader variation */

ccl_device_inline float object_random_number(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return 0.0f;
  }

  return kernel_data_fetch(objects, object).random_number;
}

/* Particle ID from which this object was generated */

ccl_device_inline int object_particle_id(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return 0;
  }

  return kernel_data_fetch(objects, object).particle_index;
}

/* Generated texture coordinate on surface from where object was instanced */

ccl_device_inline float3 object_dupli_generated(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return make_float3(0.0f, 0.0f, 0.0f);
  }

  const ccl_global KernelObject *kobject = &kernel_data_fetch(objects, object);
  return make_float3(
      kobject->dupli_generated[0], kobject->dupli_generated[1], kobject->dupli_generated[2]);
}

/* UV texture coordinate on surface from where object was instanced */

ccl_device_inline float3 object_dupli_uv(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return make_float3(0.0f, 0.0f, 0.0f);
  }

  const ccl_global KernelObject *kobject = &kernel_data_fetch(objects, object);
  return make_float3(kobject->dupli_uv[0], kobject->dupli_uv[1], 0.0f);
}

/* Offset to an objects patch map */

ccl_device_inline uint object_patch_map_offset(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return 0;
  }

  return kernel_data_fetch(objects, object).patch_map_offset;
}

/* Volume step size */

ccl_device_inline float object_volume_density(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return 1.0f;
  }

  return kernel_data_fetch(objects, object).volume_density;
}

ccl_device_inline float object_volume_step_size(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return kernel_data.background.volume_step_size;
  }

  return kernel_data_fetch(object_volume_step, object);
}

/* Pass ID for shader */

ccl_device int shader_pass_id(KernelGlobals kg, const ccl_private ShaderData *sd)
{
  return kernel_data_fetch(shaders, (sd->shader & SHADER_MASK)).pass_id;
}

/* Cryptomatte ID */

ccl_device_inline float object_cryptomatte_id(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return 0.0f;
  }

  return kernel_data_fetch(objects, object).cryptomatte_object;
}

ccl_device_inline float object_cryptomatte_asset_id(KernelGlobals kg, const int object)
{
  if (object == OBJECT_NONE) {
    return 0;
  }

  return kernel_data_fetch(objects, object).cryptomatte_asset;
}

/* Particle data from which object was instanced */

ccl_device_inline uint particle_index(KernelGlobals kg, const int particle)
{
  return kernel_data_fetch(particles, particle).index;
}

ccl_device float particle_age(KernelGlobals kg, const int particle)
{
  return kernel_data_fetch(particles, particle).age;
}

ccl_device float particle_lifetime(KernelGlobals kg, const int particle)
{
  return kernel_data_fetch(particles, particle).lifetime;
}

ccl_device float particle_size(KernelGlobals kg, const int particle)
{
  return kernel_data_fetch(particles, particle).size;
}

ccl_device float4 particle_rotation(KernelGlobals kg, const int particle)
{
  return kernel_data_fetch(particles, particle).rotation;
}

ccl_device float3 particle_location(KernelGlobals kg, const int particle)
{
  return make_float3(kernel_data_fetch(particles, particle).location);
}

ccl_device float3 particle_velocity(KernelGlobals kg, const int particle)
{
  return make_float3(kernel_data_fetch(particles, particle).velocity);
}

ccl_device float3 particle_angular_velocity(KernelGlobals kg, const int particle)
{
  return make_float3(kernel_data_fetch(particles, particle).angular_velocity);
}

/* Object intersection in BVH */

ccl_device_inline float3 bvh_clamp_direction(const float3 dir)
{
  const float ooeps = 8.271806E-25f;
  return make_float3((fabsf(dir.x) > ooeps) ? dir.x : copysignf(ooeps, dir.x),
                     (fabsf(dir.y) > ooeps) ? dir.y : copysignf(ooeps, dir.y),
                     (fabsf(dir.z) > ooeps) ? dir.z : copysignf(ooeps, dir.z));
}

ccl_device_inline float3 bvh_inverse_direction(const float3 dir)
{
  return reciprocal(dir);
}

/* Transform ray into object space to enter static object in BVH */

ccl_device_inline void bvh_instance_push(KernelGlobals kg,
                                         const int object,
                                         const ccl_private Ray *ray,
                                         ccl_private float3 *P,
                                         ccl_private float3 *dir,
                                         ccl_private float3 *idir)
{
  const Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);

  *P = transform_point(&tfm, ray->P);

  *dir = bvh_clamp_direction(transform_direction(&tfm, ray->D));
  *idir = bvh_inverse_direction(*dir);
}

#ifdef __OBJECT_MOTION__
/* Transform ray into object space to enter motion blurred object in BVH */

ccl_device_inline void bvh_instance_motion_push(KernelGlobals kg,
                                                const int object,
                                                const ccl_private Ray *ray,
                                                ccl_private float3 *P,
                                                ccl_private float3 *dir,
                                                ccl_private float3 *idir)
{
  Transform tfm;
  object_fetch_transform_motion_test(kg, object, ray->time, &tfm);

  *P = transform_point(&tfm, ray->P);

  *dir = bvh_clamp_direction(transform_direction(&tfm, ray->D));
  *idir = bvh_inverse_direction(*dir);
}

#endif

/* Transform ray to exit static object in BVH. */

ccl_device_inline void bvh_instance_pop(const ccl_private Ray *ray,
                                        ccl_private float3 *P,
                                        ccl_private float3 *dir,
                                        ccl_private float3 *idir)
{
  *P = ray->P;
  *dir = bvh_clamp_direction(ray->D);
  *idir = bvh_inverse_direction(*dir);
}

/* TODO: This can be removed when we know if no devices will require explicit
 * address space qualifiers for this case. */

#define object_position_transform_auto object_position_transform
#define object_dir_transform_auto object_dir_transform
#define object_normal_transform_auto object_normal_transform

CCL_NAMESPACE_END
