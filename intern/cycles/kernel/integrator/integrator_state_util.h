/*
 * Copyright 2011-2021 Blender Foundation
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

#pragma once

#include "kernel/integrator/integrator_state.h"
#include "kernel/kernel_differential.h"

CCL_NAMESPACE_BEGIN

/* Ray */

ccl_device_forceinline void integrator_state_write_ray(INTEGRATOR_STATE_ARGS,
                                                       const Ray *ccl_restrict ray)
{
  INTEGRATOR_STATE_WRITE(ray, P) = ray->P;
  INTEGRATOR_STATE_WRITE(ray, D) = ray->D;
  INTEGRATOR_STATE_WRITE(ray, t) = ray->t;
  INTEGRATOR_STATE_WRITE(ray, time) = ray->time;
  INTEGRATOR_STATE_WRITE(ray, dP) = ray->dP;
  INTEGRATOR_STATE_WRITE(ray, dD) = ray->dD;
}

ccl_device_forceinline void integrator_state_read_ray(INTEGRATOR_STATE_CONST_ARGS,
                                                      Ray *ccl_restrict ray)
{
  ray->P = INTEGRATOR_STATE(ray, P);
  ray->D = INTEGRATOR_STATE(ray, D);
  ray->t = INTEGRATOR_STATE(ray, t);
  ray->time = INTEGRATOR_STATE(ray, time);
  ray->dP = INTEGRATOR_STATE(ray, dP);
  ray->dD = INTEGRATOR_STATE(ray, dD);
}

/* Shadow Ray */

ccl_device_forceinline void integrator_state_write_shadow_ray(INTEGRATOR_STATE_ARGS,
                                                              const Ray *ccl_restrict ray)
{
  INTEGRATOR_STATE_WRITE(shadow_ray, P) = ray->P;
  INTEGRATOR_STATE_WRITE(shadow_ray, D) = ray->D;
  INTEGRATOR_STATE_WRITE(shadow_ray, t) = ray->t;
  INTEGRATOR_STATE_WRITE(shadow_ray, time) = ray->time;
  INTEGRATOR_STATE_WRITE(shadow_ray, dP) = ray->dP;
}

ccl_device_forceinline void integrator_state_read_shadow_ray(INTEGRATOR_STATE_CONST_ARGS,
                                                             Ray *ccl_restrict ray)
{
  ray->P = INTEGRATOR_STATE(shadow_ray, P);
  ray->D = INTEGRATOR_STATE(shadow_ray, D);
  ray->t = INTEGRATOR_STATE(shadow_ray, t);
  ray->time = INTEGRATOR_STATE(shadow_ray, time);
  ray->dP = INTEGRATOR_STATE(shadow_ray, dP);
  ray->dD = differential_zero_compact();
}

/* Intersection */

ccl_device_forceinline void integrator_state_write_isect(INTEGRATOR_STATE_ARGS,
                                                         const Intersection *ccl_restrict isect)
{
  INTEGRATOR_STATE_WRITE(isect, t) = isect->t;
  INTEGRATOR_STATE_WRITE(isect, u) = isect->u;
  INTEGRATOR_STATE_WRITE(isect, v) = isect->v;
  INTEGRATOR_STATE_WRITE(isect, object) = isect->object;
  INTEGRATOR_STATE_WRITE(isect, prim) = isect->prim;
  INTEGRATOR_STATE_WRITE(isect, type) = isect->type;
#ifdef __EMBREE__
  INTEGRATOR_STATE_WRITE(isect, Ng) = isect->Ng;
#endif
}

ccl_device_forceinline void integrator_state_read_isect(INTEGRATOR_STATE_CONST_ARGS,
                                                        Intersection *ccl_restrict isect)
{
  isect->prim = INTEGRATOR_STATE(isect, prim);
  isect->object = INTEGRATOR_STATE(isect, object);
  isect->type = INTEGRATOR_STATE(isect, type);
  isect->u = INTEGRATOR_STATE(isect, u);
  isect->v = INTEGRATOR_STATE(isect, v);
  isect->t = INTEGRATOR_STATE(isect, t);
#ifdef __EMBREE__
  isect->Ng = INTEGRATOR_STATE(isect, Ng);
#endif
}

ccl_device_forceinline VolumeStack integrator_state_read_volume_stack(INTEGRATOR_STATE_CONST_ARGS,
                                                                      int i)
{
  VolumeStack entry = {INTEGRATOR_STATE_ARRAY(volume_stack, i, object),
                       INTEGRATOR_STATE_ARRAY(volume_stack, i, shader)};
  return entry;
}

ccl_device_forceinline void integrator_state_write_volume_stack(INTEGRATOR_STATE_ARGS,
                                                                int i,
                                                                VolumeStack entry)
{
  INTEGRATOR_STATE_ARRAY_WRITE(volume_stack, i, object) = entry.object;
  INTEGRATOR_STATE_ARRAY_WRITE(volume_stack, i, shader) = entry.shader;
}

ccl_device_forceinline bool integrator_state_volume_stack_is_empty(INTEGRATOR_STATE_CONST_ARGS)
{
  return (kernel_data.kernel_features & KERNEL_FEATURE_VOLUME) ?
             INTEGRATOR_STATE_ARRAY(volume_stack, 0, shader) == SHADER_NONE :
             true;
}

/* Shadow Intersection */

ccl_device_forceinline void integrator_state_write_shadow_isect(
    INTEGRATOR_STATE_ARGS, const Intersection *ccl_restrict isect, const int index)
{
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_isect, index, t) = isect->t;
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_isect, index, u) = isect->u;
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_isect, index, v) = isect->v;
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_isect, index, object) = isect->object;
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_isect, index, prim) = isect->prim;
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_isect, index, type) = isect->type;
#ifdef __EMBREE__
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_isect, index, Ng) = isect->Ng;
#endif
}

ccl_device_forceinline void integrator_state_read_shadow_isect(INTEGRATOR_STATE_CONST_ARGS,
                                                               Intersection *ccl_restrict isect,
                                                               const int index)
{
  isect->prim = INTEGRATOR_STATE_ARRAY(shadow_isect, index, prim);
  isect->object = INTEGRATOR_STATE_ARRAY(shadow_isect, index, object);
  isect->type = INTEGRATOR_STATE_ARRAY(shadow_isect, index, type);
  isect->u = INTEGRATOR_STATE_ARRAY(shadow_isect, index, u);
  isect->v = INTEGRATOR_STATE_ARRAY(shadow_isect, index, v);
  isect->t = INTEGRATOR_STATE_ARRAY(shadow_isect, index, t);
#ifdef __EMBREE__
  isect->Ng = INTEGRATOR_STATE_ARRAY(shadow_isect, index, Ng);
#endif
}

ccl_device_forceinline void integrator_state_copy_volume_stack_to_shadow(INTEGRATOR_STATE_ARGS)
{
  if (kernel_data.kernel_features & KERNEL_FEATURE_VOLUME) {
    int index = 0;
    int shader;
    do {
      shader = INTEGRATOR_STATE_ARRAY(volume_stack, index, shader);

      INTEGRATOR_STATE_ARRAY_WRITE(shadow_volume_stack, index, object) = INTEGRATOR_STATE_ARRAY(
          volume_stack, index, object);
      INTEGRATOR_STATE_ARRAY_WRITE(shadow_volume_stack, index, shader) = shader;

      ++index;
    } while (shader != OBJECT_NONE);
  }
}

ccl_device_forceinline VolumeStack
integrator_state_read_shadow_volume_stack(INTEGRATOR_STATE_CONST_ARGS, int i)
{
  VolumeStack entry = {INTEGRATOR_STATE_ARRAY(shadow_volume_stack, i, object),
                       INTEGRATOR_STATE_ARRAY(shadow_volume_stack, i, shader)};
  return entry;
}

ccl_device_forceinline bool integrator_state_shadow_volume_stack_is_empty(
    INTEGRATOR_STATE_CONST_ARGS)
{
  return (kernel_data.kernel_features & KERNEL_FEATURE_VOLUME) ?
             INTEGRATOR_STATE_ARRAY(shadow_volume_stack, 0, shader) == SHADER_NONE :
             true;
}

ccl_device_forceinline void integrator_state_write_shadow_volume_stack(INTEGRATOR_STATE_ARGS,
                                                                       int i,
                                                                       VolumeStack entry)
{
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_volume_stack, i, object) = entry.object;
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_volume_stack, i, shader) = entry.shader;
}

#if defined(__KERNEL_GPU__)
ccl_device_inline void integrator_state_copy_only(const IntegratorState to_state,
                                                  const IntegratorState state)
{
  int index;

  /* Rely on the compiler to optimize out unused assignments and `while(false)`'s. */

#  define KERNEL_STRUCT_BEGIN(name) \
    index = 0; \
    do {

#  define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) \
    if (kernel_integrator_state.parent_struct.name != nullptr) { \
      kernel_integrator_state.parent_struct.name[to_state] = \
          kernel_integrator_state.parent_struct.name[state]; \
    }

#  define KERNEL_STRUCT_ARRAY_MEMBER(parent_struct, type, name, feature) \
    if (kernel_integrator_state.parent_struct[index].name != nullptr) { \
      kernel_integrator_state.parent_struct[index].name[to_state] = \
          kernel_integrator_state.parent_struct[index].name[state]; \
    }

#  define KERNEL_STRUCT_END(name) \
    } \
    while (false) \
      ;

#  define KERNEL_STRUCT_END_ARRAY(name, cpu_array_size, gpu_array_size) \
    ++index; \
    } \
    while (index < gpu_array_size) \
      ;

#  define KERNEL_STRUCT_VOLUME_STACK_SIZE kernel_data.volume_stack_size

#  include "kernel/integrator/integrator_state_template.h"

#  undef KERNEL_STRUCT_BEGIN
#  undef KERNEL_STRUCT_MEMBER
#  undef KERNEL_STRUCT_ARRAY_MEMBER
#  undef KERNEL_STRUCT_END
#  undef KERNEL_STRUCT_END_ARRAY
#  undef KERNEL_STRUCT_VOLUME_STACK_SIZE
}

ccl_device_inline void integrator_state_move(const IntegratorState to_state,
                                             const IntegratorState state)
{
  integrator_state_copy_only(to_state, state);

  INTEGRATOR_STATE_WRITE(path, queued_kernel) = 0;
  INTEGRATOR_STATE_WRITE(shadow_path, queued_kernel) = 0;
}

#endif

/* NOTE: Leaves kernel scheduling information untouched. Use INIT semantic for one of the paths
 * after this function. */
ccl_device_inline void integrator_state_shadow_catcher_split(INTEGRATOR_STATE_ARGS)
{
#if defined(__KERNEL_GPU__)
  const IntegratorState to_state = atomic_fetch_and_add_uint32(
      &kernel_integrator_state.next_shadow_catcher_path_index[0], 1);

  integrator_state_copy_only(to_state, state);

  kernel_integrator_state.path.flag[to_state] |= PATH_RAY_SHADOW_CATCHER_PASS;

  /* Sanity check: expect to split in the intersect-closest kernel, where there is no shadow ray
   * and no sorting yet. */
  kernel_assert(INTEGRATOR_STATE(shadow_path, queued_kernel) == 0);
  kernel_assert(kernel_integrator_state.sort_key_counter[INTEGRATOR_STATE(path, queued_kernel)] ==
                nullptr);
#else

  IntegratorStateCPU *ccl_restrict split_state = state + 1;

  /* Only copy the required subset, since shadow intersections are big and irrelevant here. */
  split_state->path = state->path;
  split_state->ray = state->ray;
  split_state->isect = state->isect;
  memcpy(split_state->volume_stack, state->volume_stack, sizeof(state->volume_stack));
  split_state->shadow_path = state->shadow_path;

  split_state->path.flag |= PATH_RAY_SHADOW_CATCHER_PASS;
#endif
}

CCL_NAMESPACE_END
