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

#include "kernel/integrator/path_state.h"
#include "kernel/integrator/state_util.h"

CCL_NAMESPACE_BEGIN

/* Check whether current surface bounce is where path is to be split for the shadow catcher. */
ccl_device_inline bool kernel_shadow_catcher_is_path_split_bounce(KernelGlobals kg,
                                                                  IntegratorState state,
                                                                  const int object_flag)
{
#ifdef __SHADOW_CATCHER__
  if (!kernel_data.integrator.has_shadow_catcher) {
    return false;
  }

  /* Check the flag first, avoiding fetches form global memory. */
  if ((object_flag & SD_OBJECT_SHADOW_CATCHER) == 0) {
    return false;
  }
  if (object_flag & SD_OBJECT_HOLDOUT_MASK) {
    return false;
  }

  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  if ((path_flag & PATH_RAY_TRANSPARENT_BACKGROUND) == 0) {
    /* Split only on primary rays, secondary bounces are to treat shadow catcher as a regular
     * object. */
    return false;
  }

  if (path_flag & PATH_RAY_SHADOW_CATCHER_PASS) {
    return false;
  }

  return true;
#else
  (void)object_flag;
  return false;
#endif
}

/* Check whether the current path can still split. */
ccl_device_inline bool kernel_shadow_catcher_path_can_split(KernelGlobals kg,
                                                            ConstIntegratorState state)
{
  if (INTEGRATOR_PATH_IS_TERMINATED) {
    return false;
  }

  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  if (path_flag & PATH_RAY_SHADOW_CATCHER_HIT) {
    /* Shadow catcher was already hit and the state was split. No further split is allowed. */
    return false;
  }

  return (path_flag & PATH_RAY_TRANSPARENT_BACKGROUND) != 0;
}

/* NOTE: Leaves kernel scheduling information untouched. Use INIT semantic for one of the paths
 * after this function. */
ccl_device_inline bool kernel_shadow_catcher_split(KernelGlobals kg,
                                                   IntegratorState state,
                                                   const int object_flags)
{
#ifdef __SHADOW_CATCHER__

  if (!kernel_shadow_catcher_is_path_split_bounce(kg, state, object_flags)) {
    return false;
  }

  /* The split is to be done. Mark the current state as such, so that it stops contributing to the
   * shadow catcher matte pass, but keeps contributing to the combined pass. */
  INTEGRATOR_STATE_WRITE(state, path, flag) |= PATH_RAY_SHADOW_CATCHER_HIT;

  /* Split new state from the current one. This new state will only track contribution of shadow
   * catcher objects ignoring non-catcher objects. */
  integrator_state_shadow_catcher_split(kg, state);

  return true;
#else
  (void)object_flags;
  return false;
#endif
}

#ifdef __SHADOW_CATCHER__

ccl_device_forceinline bool kernel_shadow_catcher_is_matte_path(const uint32_t path_flag)
{
  return (path_flag & PATH_RAY_SHADOW_CATCHER_HIT) == 0;
}

ccl_device_forceinline bool kernel_shadow_catcher_is_object_pass(const uint32_t path_flag)
{
  return path_flag & PATH_RAY_SHADOW_CATCHER_PASS;
}

#endif /* __SHADOW_CATCHER__ */

CCL_NAMESPACE_END
