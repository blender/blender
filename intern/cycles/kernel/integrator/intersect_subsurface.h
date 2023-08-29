/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

  integrator_path_terminate(kg, state, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE);
}

CCL_NAMESPACE_END
