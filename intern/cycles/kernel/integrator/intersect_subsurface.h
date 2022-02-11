/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/integrator/subsurface.h"

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
