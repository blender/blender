/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/bvh/bvh.h"
#include "kernel/integrator/path_state.h"
#include "kernel/integrator/shade_surface.h"
#include "kernel/integrator/shadow_linking.h"
#include "kernel/light/light.h"
#include "kernel/sample/lcg.h"

CCL_NAMESPACE_BEGIN

#ifdef __SHADOW_LINKING__

#  define SHADOW_LINK_MAX_INTERSECTION_COUNT 1024

/* Intersect mesh objects.
 *
 * Returns the total number of emissive surfaces hit, and the intersection contains a random
 * intersected emitter to which the dedicated shadow ray is to eb traced.
 *
 * NOTE: Sets the ray tmax to the maximum intersection distance (past which no lights are to be
 * considered for shadow linking). */
ccl_device int shadow_linking_pick_mesh_intersection(KernelGlobals kg,
                                                     IntegratorState state,
                                                     ccl_private Ray *ccl_restrict ray,
                                                     const int object_receiver,
                                                     ccl_private Intersection *ccl_restrict
                                                         linked_isect,
                                                     ccl_private uint *lcg_state,
                                                     int num_hits)
{
  /* The tmin will be offset, so store its current value and restore later on, allowing a separate
   * light intersection loop starting from the actual ray origin. */
  const float old_tmin = ray->tmin;

  const uint visibility = path_state_ray_visibility(state);

  for (int i = 0; i < SHADOW_LINK_MAX_INTERSECTION_COUNT; i++) {
    Intersection current_isect ccl_optional_struct_init;
    current_isect.object = OBJECT_NONE;
    current_isect.prim = PRIM_NONE;

    const bool hit = scene_intersect(kg, ray, visibility, &current_isect);
    if (!hit) {
      break;
    }

    /* Only record primitives that potentially have emission.
     * TODO: optimize with a dedicated ray visibility flag, which could then also be
     * used once lights are in the BVH as geometry? */
    const int shader = intersection_get_shader(kg, &current_isect);
    const int shader_flags = kernel_data_fetch(shaders, shader).flags;
    if (light_link_object_match(kg, object_receiver, current_isect.object) &&
        (shader_flags & SD_HAS_EMISSION))
    {
      const uint64_t set_membership =
          kernel_data_fetch(objects, current_isect.object).shadow_set_membership;
      if (set_membership != LIGHT_LINK_MASK_ALL) {
        ++num_hits;

        if ((linked_isect->prim == PRIM_NONE) || (lcg_step_float(lcg_state) < 1.0f / num_hits)) {
          *linked_isect = current_isect;
        }
      }
    }

    /* Contribution from the lights past the default opaque blocker is accumulated
     * using the main path. */
    if (!(shader_flags & (SD_HAS_ONLY_VOLUME | SD_HAS_TRANSPARENT_SHADOW))) {
      const uint blocker_set = kernel_data_fetch(objects, current_isect.object).blocker_shadow_set;
      if (blocker_set == 0) {
        ray->tmax = current_isect.t;
        break;
      }
    }

    /* Move the ray forward. */
    ray->tmin = intersection_t_offset(current_isect.t);
  }

  ray->tmin = old_tmin;

  return num_hits;
}

/* Pick a light for tracing a shadow ray for the shadow linking.
 * Picks a random light which is intersected by the given ray, and stores the intersection result.
 * If no lights were hit false is returned.
 *
 * NOTE: Sets the ray tmax to the maximum intersection distance (past which no lights are to be
 * considered for shadow linking). */
ccl_device bool shadow_linking_pick_light_intersection(KernelGlobals kg,
                                                       IntegratorState state,
                                                       ccl_private Ray *ccl_restrict ray,
                                                       ccl_private Intersection *ccl_restrict
                                                           linked_isect)
{
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  const int last_type = INTEGRATOR_STATE(state, isect, type);

  const int object_receiver = light_link_receiver_forward(kg, state);

  uint lcg_state = lcg_state_init(INTEGRATOR_STATE(state, path, rng_hash),
                                  INTEGRATOR_STATE(state, path, rng_offset),
                                  INTEGRATOR_STATE(state, path, sample),
                                  0x68bc21eb);

  /* Indicate that no intersection has been picked yet. */
  linked_isect->prim = PRIM_NONE;

  int num_hits = 0;

  // TODO: Only if there are emissive meshes in the scene?

  // TODO: Only if the ray hits any light? As in, check that there is a light first, before
  // tracing potentially expensive ray.

  num_hits = shadow_linking_pick_mesh_intersection(
      kg, state, ray, object_receiver, linked_isect, &lcg_state, num_hits);

  num_hits = lights_intersect_shadow_linked(kg,
                                            ray,
                                            linked_isect,
                                            ray->self.prim,
                                            ray->self.object,
                                            last_type,
                                            path_flag,
                                            object_receiver,
                                            &lcg_state,
                                            num_hits);

  if (num_hits == 0) {
    return false;
  }

  INTEGRATOR_STATE_WRITE(state, shadow_link, dedicated_light_weight) = num_hits;

  return true;
}

/* Check whether a special shadow ray is needed to calculate direct light contribution which comes
 * from emitters which are behind objects which are blocking light for the main path, but are
 * excluded from blocking light via shadow linking.
 *
 * If a special ray is needed a blocked light kernel is scheduled and true is returned, otherwise
 * false is returned. */
ccl_device bool shadow_linking_intersect(KernelGlobals kg, IntegratorState state)
{
  /* Verify that the kernel is only scheduled if it is actually needed. */
  kernel_assert(shadow_linking_scene_need_shadow_ray(kg));

  /* Read ray from integrator state into local memory. */
  Ray ray ccl_optional_struct_init;
  integrator_state_read_ray(state, &ray);

  ray.self.prim = INTEGRATOR_STATE(state, isect, prim);
  ray.self.object = INTEGRATOR_STATE(state, isect, object);
  ray.self.light_object = OBJECT_NONE;
  ray.self.light_prim = PRIM_NONE;
  ray.self.light = LAMP_NONE;

  Intersection isect ccl_optional_struct_init;
  if (!shadow_linking_pick_light_intersection(kg, state, &ray, &isect)) {
    /* No light is hit, no need in the extra shadow ray for the direct light. */
    return false;
  }

  /* Make a copy of primitives needed by the main path self-intersection check before writing the
   * new intersection. Those primitives will be restored before the main path is returned to the
   * intersect_closest state. */
  shadow_linking_store_last_primitives(state);

  /* Write intersection result into global integrator state memory, so that the
   * shade_dedicated_light kernel can use it for calculation of the light sample. */
  integrator_state_write_isect(state, &isect);

  integrator_path_next(kg,
                       state,
                       DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT,
                       DEVICE_KERNEL_INTEGRATOR_SHADE_DEDICATED_LIGHT);

  return true;
}

#endif /* __SHADOW_LINKING__ */

ccl_device void integrator_intersect_dedicated_light(KernelGlobals kg, IntegratorState state)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_DEDICATED_LIGHT);

#ifdef __SHADOW_LINKING__
  if (shadow_linking_intersect(kg, state)) {
    return;
  }
#else
  kernel_assert(!"integrator_intersect_dedicated_light is not supposed to be scheduled");
#endif

  integrator_shade_surface_next_kernel<DEVICE_KERNEL_INTEGRATOR_INTERSECT_DEDICATED_LIGHT>(kg,
                                                                                           state);
}

CCL_NAMESPACE_END
