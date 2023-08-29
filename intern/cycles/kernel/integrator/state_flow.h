/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"
#include "util/atomic.h"

CCL_NAMESPACE_BEGIN

/* Control Flow
 *
 * Utilities for control flow between kernels. The implementation is different between CPU and
 * GPU devices. For the latter part of the logic is handled on the host side with wavefronts.
 *
 * There is a main path for regular path tracing camera for path tracing. Shadows for next
 * event estimation branch off from this into their own path, that may be computed in
 * parallel while the main path continues. Additionally, shading kernels are sorted using
 * a key for coherence.
 *
 * Each kernel on the main path must call one of these functions. These may not be called
 * multiple times from the same kernel.
 *
 * integrator_path_init(kg, state, next_kernel)
 * integrator_path_next(kg, state, current_kernel, next_kernel)
 * integrator_path_terminate(kg, state, current_kernel)
 *
 * For the shadow path similar functions are used, and again each shadow kernel must call
 * one of them, and only once.
 */

ccl_device_forceinline bool integrator_path_is_terminated(ConstIntegratorState state)
{
  return INTEGRATOR_STATE(state, path, queued_kernel) == 0;
}

ccl_device_forceinline bool integrator_shadow_path_is_terminated(ConstIntegratorShadowState state)
{
  return INTEGRATOR_STATE(state, shadow_path, queued_kernel) == 0;
}

#ifdef __KERNEL_GPU__

ccl_device_forceinline void integrator_path_init(KernelGlobals kg,
                                                 IntegratorState state,
                                                 const DeviceKernel next_kernel)
{
  atomic_fetch_and_add_uint32(&kernel_integrator_state.queue_counter->num_queued[next_kernel], 1);
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = next_kernel;
}

ccl_device_forceinline void integrator_path_next(KernelGlobals kg,
                                                 IntegratorState state,
                                                 const DeviceKernel current_kernel,
                                                 const DeviceKernel next_kernel)
{
  atomic_fetch_and_sub_uint32(&kernel_integrator_state.queue_counter->num_queued[current_kernel],
                              1);
  atomic_fetch_and_add_uint32(&kernel_integrator_state.queue_counter->num_queued[next_kernel], 1);
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = next_kernel;
}

ccl_device_forceinline void integrator_path_terminate(KernelGlobals kg,
                                                      IntegratorState state,
                                                      const DeviceKernel current_kernel)
{
  atomic_fetch_and_sub_uint32(&kernel_integrator_state.queue_counter->num_queued[current_kernel],
                              1);
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = 0;
}

ccl_device_forceinline IntegratorShadowState integrator_shadow_path_init(
    KernelGlobals kg, IntegratorState state, const DeviceKernel next_kernel, const bool is_ao)
{
  IntegratorShadowState shadow_state = atomic_fetch_and_add_uint32(
      &kernel_integrator_state.next_shadow_path_index[0], 1);
  atomic_fetch_and_add_uint32(&kernel_integrator_state.queue_counter->num_queued[next_kernel], 1);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, queued_kernel) = next_kernel;
#  ifdef __PATH_GUIDING__
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, path_segment) = nullptr;
#  endif
  return shadow_state;
}

ccl_device_forceinline void integrator_shadow_path_next(KernelGlobals kg,
                                                        IntegratorShadowState state,
                                                        const DeviceKernel current_kernel,
                                                        const DeviceKernel next_kernel)
{
  atomic_fetch_and_sub_uint32(&kernel_integrator_state.queue_counter->num_queued[current_kernel],
                              1);
  atomic_fetch_and_add_uint32(&kernel_integrator_state.queue_counter->num_queued[next_kernel], 1);
  INTEGRATOR_STATE_WRITE(state, shadow_path, queued_kernel) = next_kernel;
}

ccl_device_forceinline void integrator_shadow_path_terminate(KernelGlobals kg,
                                                             IntegratorShadowState state,
                                                             const DeviceKernel current_kernel)
{
  atomic_fetch_and_sub_uint32(&kernel_integrator_state.queue_counter->num_queued[current_kernel],
                              1);
  INTEGRATOR_STATE_WRITE(state, shadow_path, queued_kernel) = 0;
}

/* Sort first by truncated state index (for good locality), then by key (for good coherence). */
#  define INTEGRATOR_SORT_KEY(key, state) \
    (key + kernel_data.max_shaders * (state / kernel_integrator_state.sort_partition_divisor))

ccl_device_forceinline void integrator_path_init_sorted(KernelGlobals kg,
                                                        IntegratorState state,
                                                        const DeviceKernel next_kernel,
                                                        const uint32_t key)
{
  const int key_ = INTEGRATOR_SORT_KEY(key, state);
  atomic_fetch_and_add_uint32(&kernel_integrator_state.queue_counter->num_queued[next_kernel], 1);
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = next_kernel;
  INTEGRATOR_STATE_WRITE(state, path, shader_sort_key) = key_;

#  if defined(__KERNEL_LOCAL_ATOMIC_SORT__)
  if (!kernel_integrator_state.sort_key_counter[next_kernel]) {
    return;
  }
#  endif

  atomic_fetch_and_add_uint32(&kernel_integrator_state.sort_key_counter[next_kernel][key_], 1);
}

ccl_device_forceinline void integrator_path_next_sorted(KernelGlobals kg,
                                                        IntegratorState state,
                                                        const DeviceKernel current_kernel,
                                                        const DeviceKernel next_kernel,
                                                        const uint32_t key)
{
  const int key_ = INTEGRATOR_SORT_KEY(key, state);
  atomic_fetch_and_sub_uint32(&kernel_integrator_state.queue_counter->num_queued[current_kernel],
                              1);
  atomic_fetch_and_add_uint32(&kernel_integrator_state.queue_counter->num_queued[next_kernel], 1);
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = next_kernel;
  INTEGRATOR_STATE_WRITE(state, path, shader_sort_key) = key_;

#  if defined(__KERNEL_LOCAL_ATOMIC_SORT__)
  if (!kernel_integrator_state.sort_key_counter[next_kernel]) {
    return;
  }
#  endif

  atomic_fetch_and_add_uint32(&kernel_integrator_state.sort_key_counter[next_kernel][key_], 1);
}

#else

ccl_device_forceinline void integrator_path_init(KernelGlobals kg,
                                                 IntegratorState state,
                                                 const DeviceKernel next_kernel)
{
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = next_kernel;
}

ccl_device_forceinline void integrator_path_init_sorted(KernelGlobals kg,
                                                        IntegratorState state,
                                                        const DeviceKernel next_kernel,
                                                        const uint32_t key)
{
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = next_kernel;
  (void)key;
}

ccl_device_forceinline void integrator_path_next(KernelGlobals kg,
                                                 IntegratorState state,
                                                 const DeviceKernel current_kernel,
                                                 const DeviceKernel next_kernel)
{
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = next_kernel;
  (void)current_kernel;
}

ccl_device_forceinline void integrator_path_terminate(KernelGlobals kg,
                                                      IntegratorState state,
                                                      const DeviceKernel current_kernel)
{
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = 0;
  (void)current_kernel;
}

ccl_device_forceinline void integrator_path_next_sorted(KernelGlobals kg,
                                                        IntegratorState state,
                                                        const DeviceKernel current_kernel,
                                                        const DeviceKernel next_kernel,
                                                        const uint32_t key)
{
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = next_kernel;
  (void)key;
  (void)current_kernel;
}

ccl_device_forceinline IntegratorShadowState integrator_shadow_path_init(
    KernelGlobals kg, IntegratorState state, const DeviceKernel next_kernel, const bool is_ao)
{
  IntegratorShadowState shadow_state = (is_ao) ? &state->ao : &state->shadow;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, queued_kernel) = next_kernel;
#  ifdef __PATH_GUIDING__
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, path_segment) = nullptr;
#  endif
  return shadow_state;
}

ccl_device_forceinline void integrator_shadow_path_next(KernelGlobals kg,
                                                        IntegratorShadowState state,
                                                        const DeviceKernel current_kernel,
                                                        const DeviceKernel next_kernel)
{
  INTEGRATOR_STATE_WRITE(state, shadow_path, queued_kernel) = next_kernel;
  (void)current_kernel;
}

ccl_device_forceinline void integrator_shadow_path_terminate(KernelGlobals kg,
                                                             IntegratorShadowState state,
                                                             const DeviceKernel current_kernel)
{
  INTEGRATOR_STATE_WRITE(state, shadow_path, queued_kernel) = 0;
  (void)current_kernel;
}

#endif

CCL_NAMESPACE_END
