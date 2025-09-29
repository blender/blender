/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/integrator/intersect_closest.h"
#include "kernel/integrator/intersect_dedicated_light.h"
#include "kernel/integrator/intersect_shadow.h"
#include "kernel/integrator/intersect_subsurface.h"
#include "kernel/integrator/intersect_volume_stack.h"
#include "kernel/integrator/shade_background.h"
#include "kernel/integrator/shade_dedicated_light.h"
#include "kernel/integrator/shade_light.h"
#include "kernel/integrator/shade_shadow.h"
#include "kernel/integrator/shade_surface.h"
#include "kernel/integrator/shade_volume.h"

CCL_NAMESPACE_BEGIN

ccl_device void integrator_megakernel(KernelGlobals kg,
                                      IntegratorState state,
                                      ccl_global float *ccl_restrict render_buffer)
{
  /* Each kernel indicates the next kernel to execute, so here we simply
   * have to check what that kernel is and execute it. */
  while (true) {
    /* Handle any shadow paths before we potentially create more shadow paths. */
    const uint32_t shadow_queued_kernel = INTEGRATOR_STATE(
        &state->shadow, shadow_path, queued_kernel);
    if (shadow_queued_kernel) {
      switch (shadow_queued_kernel) {
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW:
          integrator_intersect_shadow(kg, &state->shadow);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW:
          integrator_shade_shadow(kg, &state->shadow, render_buffer);
          break;
        default:
          kernel_assert(0);
          break;
      }
      continue;
    }

    /* Handle any AO paths before we potentially create more AO paths. */
    const uint32_t ao_queued_kernel = INTEGRATOR_STATE(&state->ao, shadow_path, queued_kernel);
    if (ao_queued_kernel) {
      switch (ao_queued_kernel) {
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW:
          integrator_intersect_shadow(kg, &state->ao);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW:
          integrator_shade_shadow(kg, &state->ao, render_buffer);
          break;
        default:
          kernel_assert(0);
          break;
      }
      continue;
    }

    /* Then handle regular path kernels. */
    const uint32_t queued_kernel = INTEGRATOR_STATE(state, path, queued_kernel);
    if (queued_kernel) {
      switch (queued_kernel) {
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST:
          integrator_intersect_closest(kg, state, render_buffer);
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
        case DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME_RAY_MARCHING:
          integrator_shade_volume_ray_marching(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE:
          integrator_shade_surface_raytrace(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE:
          integrator_shade_surface_mnee(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT:
          integrator_shade_light(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_SHADE_DEDICATED_LIGHT:
          integrator_shade_dedicated_light(kg, state, render_buffer);
          break;
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE:
          integrator_intersect_subsurface(kg, state);
          break;
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK:
          integrator_intersect_volume_stack(kg, state);
          break;
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT:
          integrator_intersect_dedicated_light(kg, state);
          break;
        default:
          kernel_assert(0);
          break;
      }
      continue;
    }

    break;
  }
}

CCL_NAMESPACE_END
