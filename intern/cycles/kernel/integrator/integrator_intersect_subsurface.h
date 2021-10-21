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

#include "kernel/integrator/integrator_subsurface.h"

CCL_NAMESPACE_BEGIN

ccl_device void integrator_intersect_subsurface(KernelGlobals kg, IntegratorState state)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_SUBSURFACE);

#ifdef __SUBSURFACE__
  if (subsurface_scatter(kg, state)) {
    return;
  }
#endif

  INTEGRATOR_PATH_TERMINATE(DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE);
}

CCL_NAMESPACE_END
