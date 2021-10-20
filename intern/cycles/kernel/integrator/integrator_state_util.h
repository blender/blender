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

ccl_device_forceinline void integrator_state_write_ray(KernelGlobals kg,
                                                       IntegratorState state,
                                                       ccl_private const Ray *ccl_restrict ray)
{
  INTEGRATOR_STATE_WRITE(state, ray, P) = ray->P;
  INTEGRATOR_STATE_WRITE(state, ray, D) = ray->D;
  INTEGRATOR_STATE_WRITE(state, ray, t) = ray->t;
  INTEGRATOR_STATE_WRITE(state, ray, time) = ray->time;
  INTEGRATOR_STATE_WRITE(state, ray, dP) = ray->dP;
  INTEGRATOR_STATE_WRITE(state, ray, dD) = ray->dD;
}

ccl_device_forceinline void integrator_state_read_ray(KernelGlobals kg,
                                                      ConstIntegratorState state,
                                                      ccl_private Ray *ccl_restrict ray)
{
  ray->P = INTEGRATOR_STATE(state, ray, P);
  ray->D = INTEGRATOR_STATE(state, ray, D);
  ray->t = INTEGRATOR_STATE(state, ray, t);
  ray->time = INTEGRATOR_STATE(state, ray, time);
  ray->dP = INTEGRATOR_STATE(state, ray, dP);
  ray->dD = INTEGRATOR_STATE(state, ray, dD);
}

/* Shadow Ray */

ccl_device_forceinline void integrator_state_write_shadow_ray(
    KernelGlobals kg, IntegratorShadowState state, ccl_private const Ray *ccl_restrict ray)
{
  INTEGRATOR_STATE_WRITE(state, shadow_ray, P) = ray->P;
  INTEGRATOR_STATE_WRITE(state, shadow_ray, D) = ray->D;
  INTEGRATOR_STATE_WRITE(state, shadow_ray, t) = ray->t;
  INTEGRATOR_STATE_WRITE(state, shadow_ray, time) = ray->time;
  INTEGRATOR_STATE_WRITE(state, shadow_ray, dP) = ray->dP;
}

ccl_device_forceinline void integrator_state_read_shadow_ray(KernelGlobals kg,
                                                             ConstIntegratorShadowState state,
                                                             ccl_private Ray *ccl_restrict ray)
{
  ray->P = INTEGRATOR_STATE(state, shadow_ray, P);
  ray->D = INTEGRATOR_STATE(state, shadow_ray, D);
  ray->t = INTEGRATOR_STATE(state, shadow_ray, t);
  ray->time = INTEGRATOR_STATE(state, shadow_ray, time);
  ray->dP = INTEGRATOR_STATE(state, shadow_ray, dP);
  ray->dD = differential_zero_compact();
}

/* Intersection */

ccl_device_forceinline void integrator_state_write_isect(
    KernelGlobals kg, IntegratorState state, ccl_private const Intersection *ccl_restrict isect)
{
  INTEGRATOR_STATE_WRITE(state, isect, t) = isect->t;
  INTEGRATOR_STATE_WRITE(state, isect, u) = isect->u;
  INTEGRATOR_STATE_WRITE(state, isect, v) = isect->v;
  INTEGRATOR_STATE_WRITE(state, isect, object) = isect->object;
  INTEGRATOR_STATE_WRITE(state, isect, prim) = isect->prim;
  INTEGRATOR_STATE_WRITE(state, isect, type) = isect->type;
}

ccl_device_forceinline void integrator_state_read_isect(
    KernelGlobals kg, ConstIntegratorState state, ccl_private Intersection *ccl_restrict isect)
{
  isect->prim = INTEGRATOR_STATE(state, isect, prim);
  isect->object = INTEGRATOR_STATE(state, isect, object);
  isect->type = INTEGRATOR_STATE(state, isect, type);
  isect->u = INTEGRATOR_STATE(state, isect, u);
  isect->v = INTEGRATOR_STATE(state, isect, v);
  isect->t = INTEGRATOR_STATE(state, isect, t);
}

ccl_device_forceinline VolumeStack integrator_state_read_volume_stack(ConstIntegratorState state,
                                                                      int i)
{
  VolumeStack entry = {INTEGRATOR_STATE_ARRAY(state, volume_stack, i, object),
                       INTEGRATOR_STATE_ARRAY(state, volume_stack, i, shader)};
  return entry;
}

ccl_device_forceinline void integrator_state_write_volume_stack(IntegratorState state,
                                                                int i,
                                                                VolumeStack entry)
{
  INTEGRATOR_STATE_ARRAY_WRITE(state, volume_stack, i, object) = entry.object;
  INTEGRATOR_STATE_ARRAY_WRITE(state, volume_stack, i, shader) = entry.shader;
}

ccl_device_forceinline bool integrator_state_volume_stack_is_empty(KernelGlobals kg,
                                                                   ConstIntegratorState state)
{
  return (kernel_data.kernel_features & KERNEL_FEATURE_VOLUME) ?
             INTEGRATOR_STATE_ARRAY(state, volume_stack, 0, shader) == SHADER_NONE :
             true;
}

/* Shadow Intersection */

ccl_device_forceinline void integrator_state_write_shadow_isect(
    IntegratorShadowState state,
    ccl_private const Intersection *ccl_restrict isect,
    const int index)
{
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, index, t) = isect->t;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, index, u) = isect->u;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, index, v) = isect->v;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, index, object) = isect->object;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, index, prim) = isect->prim;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_isect, index, type) = isect->type;
}

ccl_device_forceinline void integrator_state_read_shadow_isect(
    ConstIntegratorShadowState state,
    ccl_private Intersection *ccl_restrict isect,
    const int index)
{
  isect->prim = INTEGRATOR_STATE_ARRAY(state, shadow_isect, index, prim);
  isect->object = INTEGRATOR_STATE_ARRAY(state, shadow_isect, index, object);
  isect->type = INTEGRATOR_STATE_ARRAY(state, shadow_isect, index, type);
  isect->u = INTEGRATOR_STATE_ARRAY(state, shadow_isect, index, u);
  isect->v = INTEGRATOR_STATE_ARRAY(state, shadow_isect, index, v);
  isect->t = INTEGRATOR_STATE_ARRAY(state, shadow_isect, index, t);
}

ccl_device_forceinline void integrator_state_copy_volume_stack_to_shadow(
    KernelGlobals kg, IntegratorShadowState shadow_state, ConstIntegratorState state)
{
  if (kernel_data.kernel_features & KERNEL_FEATURE_VOLUME) {
    int index = 0;
    int shader;
    do {
      shader = INTEGRATOR_STATE_ARRAY(state, volume_stack, index, shader);

      INTEGRATOR_STATE_ARRAY_WRITE(shadow_state, shadow_volume_stack, index, object) =
          INTEGRATOR_STATE_ARRAY(state, volume_stack, index, object);
      INTEGRATOR_STATE_ARRAY_WRITE(shadow_state, shadow_volume_stack, index, shader) = shader;

      ++index;
    } while (shader != OBJECT_NONE);
  }
}

ccl_device_forceinline void integrator_state_copy_volume_stack(KernelGlobals kg,
                                                               IntegratorState to_state,
                                                               ConstIntegratorState state)
{
  if (kernel_data.kernel_features & KERNEL_FEATURE_VOLUME) {
    int index = 0;
    int shader;
    do {
      shader = INTEGRATOR_STATE_ARRAY(state, volume_stack, index, shader);

      INTEGRATOR_STATE_ARRAY_WRITE(to_state, volume_stack, index, object) = INTEGRATOR_STATE_ARRAY(
          state, volume_stack, index, object);
      INTEGRATOR_STATE_ARRAY_WRITE(to_state, volume_stack, index, shader) = shader;

      ++index;
    } while (shader != OBJECT_NONE);
  }
}

ccl_device_forceinline VolumeStack
integrator_state_read_shadow_volume_stack(ConstIntegratorShadowState state, int i)
{
  VolumeStack entry = {INTEGRATOR_STATE_ARRAY(state, shadow_volume_stack, i, object),
                       INTEGRATOR_STATE_ARRAY(state, shadow_volume_stack, i, shader)};
  return entry;
}

ccl_device_forceinline bool integrator_state_shadow_volume_stack_is_empty(
    KernelGlobals kg, ConstIntegratorShadowState state)
{
  return (kernel_data.kernel_features & KERNEL_FEATURE_VOLUME) ?
             INTEGRATOR_STATE_ARRAY(state, shadow_volume_stack, 0, shader) == SHADER_NONE :
             true;
}

ccl_device_forceinline void integrator_state_write_shadow_volume_stack(IntegratorShadowState state,
                                                                       int i,
                                                                       VolumeStack entry)
{
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_volume_stack, i, object) = entry.object;
  INTEGRATOR_STATE_ARRAY_WRITE(state, shadow_volume_stack, i, shader) = entry.shader;
}

#if defined(__KERNEL_GPU__)
ccl_device_inline void integrator_state_copy_only(KernelGlobals kg,
                                                  ConstIntegratorState to_state,
                                                  ConstIntegratorState state)
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

ccl_device_inline void integrator_state_move(KernelGlobals kg,
                                             ConstIntegratorState to_state,
                                             ConstIntegratorState state)
{
  integrator_state_copy_only(kg, to_state, state);

  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = 0;
}

#endif

/* NOTE: Leaves kernel scheduling information untouched. Use INIT semantic for one of the paths
 * after this function. */
ccl_device_inline void integrator_state_shadow_catcher_split(KernelGlobals kg,
                                                             IntegratorState state)
{
#if defined(__KERNEL_GPU__)
  ConstIntegratorState to_state = atomic_fetch_and_add_uint32(
      &kernel_integrator_state.next_shadow_catcher_path_index[0], 1);

  integrator_state_copy_only(kg, to_state, state);
#else
  IntegratorStateCPU *ccl_restrict to_state = state + 1;

  /* Only copy the required subset, since shadow intersections are big and irrelevant here. */
  to_state->path = state->path;
  to_state->ray = state->ray;
  to_state->isect = state->isect;
  integrator_state_copy_volume_stack(kg, to_state, state);
#endif

  INTEGRATOR_STATE_WRITE(to_state, path, flag) |= PATH_RAY_SHADOW_CATCHER_PASS;
}

#ifdef __KERNEL_CPU__
ccl_device_inline int integrator_state_bounce(ConstIntegratorState state, const int)
{
  return INTEGRATOR_STATE(state, path, bounce);
}

ccl_device_inline int integrator_state_bounce(ConstIntegratorShadowState state, const int)
{
  return INTEGRATOR_STATE(state, shadow_path, bounce);
}

ccl_device_inline int integrator_state_diffuse_bounce(ConstIntegratorState state, const int)
{
  return INTEGRATOR_STATE(state, path, diffuse_bounce);
}

ccl_device_inline int integrator_state_diffuse_bounce(ConstIntegratorShadowState state, const int)
{
  return INTEGRATOR_STATE(state, shadow_path, diffuse_bounce);
}

ccl_device_inline int integrator_state_glossy_bounce(ConstIntegratorState state, const int)
{
  return INTEGRATOR_STATE(state, path, glossy_bounce);
}

ccl_device_inline int integrator_state_glossy_bounce(ConstIntegratorShadowState state, const int)
{
  return INTEGRATOR_STATE(state, shadow_path, glossy_bounce);
}

ccl_device_inline int integrator_state_transmission_bounce(ConstIntegratorState state, const int)
{
  return INTEGRATOR_STATE(state, path, transmission_bounce);
}

ccl_device_inline int integrator_state_transmission_bounce(ConstIntegratorShadowState state,
                                                           const int)
{
  return INTEGRATOR_STATE(state, shadow_path, transmission_bounce);
}

ccl_device_inline int integrator_state_transparent_bounce(ConstIntegratorState state, const int)
{
  return INTEGRATOR_STATE(state, path, transparent_bounce);
}

ccl_device_inline int integrator_state_transparent_bounce(ConstIntegratorShadowState state,
                                                          const int)
{
  return INTEGRATOR_STATE(state, shadow_path, transparent_bounce);
}
#else
ccl_device_inline int integrator_state_bounce(ConstIntegratorShadowState state,
                                              const uint32_t path_flag)
{
  return (path_flag & PATH_RAY_SHADOW) ? INTEGRATOR_STATE(state, shadow_path, bounce) :
                                         INTEGRATOR_STATE(state, path, bounce);
}

ccl_device_inline int integrator_state_diffuse_bounce(ConstIntegratorShadowState state,
                                                      const uint32_t path_flag)
{
  return (path_flag & PATH_RAY_SHADOW) ? INTEGRATOR_STATE(state, shadow_path, diffuse_bounce) :
                                         INTEGRATOR_STATE(state, path, diffuse_bounce);
}

ccl_device_inline int integrator_state_glossy_bounce(ConstIntegratorShadowState state,
                                                     const uint32_t path_flag)
{
  return (path_flag & PATH_RAY_SHADOW) ? INTEGRATOR_STATE(state, shadow_path, glossy_bounce) :
                                         INTEGRATOR_STATE(state, path, glossy_bounce);
}

ccl_device_inline int integrator_state_transmission_bounce(ConstIntegratorShadowState state,
                                                           const uint32_t path_flag)
{
  return (path_flag & PATH_RAY_SHADOW) ?
             INTEGRATOR_STATE(state, shadow_path, transmission_bounce) :
             INTEGRATOR_STATE(state, path, transmission_bounce);
}

ccl_device_inline int integrator_state_transparent_bounce(ConstIntegratorShadowState state,
                                                          const uint32_t path_flag)
{
  return (path_flag & PATH_RAY_SHADOW) ? INTEGRATOR_STATE(state, shadow_path, transparent_bounce) :
                                         INTEGRATOR_STATE(state, path, transparent_bounce);
}
#endif

CCL_NAMESPACE_END
