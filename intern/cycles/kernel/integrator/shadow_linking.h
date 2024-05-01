/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/integrator/path_state.h"
#include "kernel/integrator/state_util.h"

CCL_NAMESPACE_BEGIN

#ifdef __SHADOW_LINKING__

/* Check whether special shadow rays for shadow linking are needed in the current scene
 * configuration. */
ccl_device_forceinline bool shadow_linking_scene_need_shadow_ray(KernelGlobals kg)
{
  if (!(kernel_data.kernel_features & KERNEL_FEATURE_SHADOW_LINKING)) {
    /* No shadow linking in the scene, so no need to trace any extra rays. */
    return false;
  }

  /* The distant lights might be using shadow linking, and they are not counted as
   * kernel_data.integrator.use_light_mis.
   * So there is a potential to avoid extra rays from being traced, but it requires more granular
   * flags set in the integrator. */

  return true;
}

/* Shadow linking re-used the main path intersection to store information about the light to which
 * the extra ray is to be traced (this intersection communicates light between the shadow blocker
 * intersection and shading kernels).
 * These utilities makes a copy of the fields from the main intersection which are needed by the
 * intersect_closest kernel after the surface bounce. */

ccl_device_forceinline void shadow_linking_store_last_primitives(IntegratorState state)
{
  INTEGRATOR_STATE_WRITE(state, shadow_link, last_isect_prim) = INTEGRATOR_STATE(
      state, isect, prim);
  INTEGRATOR_STATE_WRITE(state, shadow_link, last_isect_object) = INTEGRATOR_STATE(
      state, isect, object);
}

ccl_device_forceinline void shadow_linking_restore_last_primitives(IntegratorState state)
{
  INTEGRATOR_STATE_WRITE(state, isect, prim) = INTEGRATOR_STATE(
      state, shadow_link, last_isect_prim);
  INTEGRATOR_STATE_WRITE(state, isect, object) = INTEGRATOR_STATE(
      state, shadow_link, last_isect_object);
}

/* Schedule shadow linking intersection kernel if it is needed.
 * Returns true if the shadow linking specific kernel has been scheduled, false otherwise. */
template<DeviceKernel current_kernel>
ccl_device_inline bool shadow_linking_schedule_intersection_kernel(KernelGlobals kg,
                                                                   IntegratorState state)
{
  if (!shadow_linking_scene_need_shadow_ray(kg)) {
    return false;
  }

  integrator_path_next(
      kg, state, current_kernel, DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT);

  return true;
}

#endif /* __SHADOW_LINKING__ */

CCL_NAMESPACE_END
