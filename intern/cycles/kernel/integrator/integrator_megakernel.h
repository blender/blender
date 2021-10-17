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

#include "kernel/integrator/integrator_init_from_camera.h"
#include "kernel/integrator/integrator_intersect_closest.h"
#include "kernel/integrator/integrator_intersect_shadow.h"
#include "kernel/integrator/integrator_intersect_subsurface.h"
#include "kernel/integrator/integrator_intersect_volume_stack.h"
#include "kernel/integrator/integrator_shade_background.h"
#include "kernel/integrator/integrator_shade_light.h"
#include "kernel/integrator/integrator_shade_shadow.h"
#include "kernel/integrator/integrator_shade_surface.h"
#include "kernel/integrator/integrator_shade_volume.h"

CCL_NAMESPACE_BEGIN

ccl_device void integrator_megakernel(KernelGlobals kg,
                                      IntegratorState state,
                                      ccl_global float *ccl_restrict render_buffer)
{
  /* Each kernel indicates the next kernel to execute, so here we simply
   * have to check what that kernel is and execute it.
   *
   * TODO: investigate if we can use device side enqueue for GPUs to avoid
   * having to compile this big kernel. */
  while (true) {
    if (INTEGRATOR_STATE(state, shadow_path, queued_kernel)) {
      /* First handle any shadow paths before we potentially create more shadow paths. */
      switch (INTEGRATOR_STATE(state, shadow_path, queued_kernel)) {
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW:
          integrator_intersect_shadow(kg, state);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW:
          integrator_shade_shadow(kg, state, render_buffer);
          break;
        default:
          kernel_assert(0);
          break;
      }
    }
    else if (INTEGRATOR_STATE(state, path, queued_kernel)) {
      /* Then handle regular path kernels. */
      switch (INTEGRATOR_STATE(state, path, queued_kernel)) {
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST:
          integrator_intersect_closest(kg, state);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND:
          integrator_shade_background(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE:
          integrator_shade_surface(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME:
          integrator_shade_volume(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE:
          integrator_shade_surface_raytrace(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT:
          integrator_shade_light(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE:
          integrator_intersect_subsurface(kg, state);
          break;
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK:
          integrator_intersect_volume_stack(kg, state);
          break;
        default:
          kernel_assert(0);
          break;
      }
    }
    else {
      break;
    }
  }
}

CCL_NAMESPACE_END
