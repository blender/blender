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

CCL_NAMESPACE_BEGIN

/* Visibility for the shadow ray. */
ccl_device_forceinline uint integrate_intersect_shadow_visibility(KernelGlobals kg,
                                                                  ConstIntegratorState state)
{
  uint visibility = PATH_RAY_SHADOW;

#ifdef __SHADOW_CATCHER__
  const uint32_t path_flag = INTEGRATOR_STATE(state, shadow_path, flag);
  visibility = SHADOW_CATCHER_PATH_VISIBILITY(path_flag, visibility);
#endif

  return visibility;
}

ccl_device bool integrate_intersect_shadow_opaque(KernelGlobals kg,
                                                  IntegratorState state,
                                                  ccl_private const Ray *ray,
                                                  const uint visibility)
{
  /* Mask which will pick only opaque visibility bits from the `visibility`.
   * Calculate the mask at compile time: the visibility will either be a high bits for the shadow
   * catcher objects, or lower bits for the regular objects (there is no need to check the path
   * state here again). */
  constexpr const uint opaque_mask = SHADOW_CATCHER_VISIBILITY_SHIFT(PATH_RAY_SHADOW_OPAQUE) |
                                     PATH_RAY_SHADOW_OPAQUE;

  Intersection isect;
  const bool opaque_hit = scene_intersect(kg, ray, visibility & opaque_mask, &isect);

  if (!opaque_hit) {
    INTEGRATOR_STATE_WRITE(state, shadow_path, num_hits) = 0;
  }

  return opaque_hit;
}

ccl_device_forceinline int integrate_shadow_max_transparent_hits(KernelGlobals kg,
                                                                 ConstIntegratorState state)
{
  const int transparent_max_bounce = kernel_data.integrator.transparent_max_bounce;
  const int transparent_bounce = INTEGRATOR_STATE(state, shadow_path, transparent_bounce);

  return max(transparent_max_bounce - transparent_bounce - 1, 0);
}

#ifdef __TRANSPARENT_SHADOWS__
ccl_device bool integrate_intersect_shadow_transparent(KernelGlobals kg,
                                                       IntegratorState state,
                                                       ccl_private const Ray *ray,
                                                       const uint visibility)
{
  Intersection isect[INTEGRATOR_SHADOW_ISECT_SIZE];

  /* Limit the number hits to the max transparent bounces allowed and the size that we
   * have available in the integrator state. */
  const uint max_transparent_hits = integrate_shadow_max_transparent_hits(kg, state);
  const uint max_hits = min(max_transparent_hits, (uint)INTEGRATOR_SHADOW_ISECT_SIZE);
  uint num_hits = 0;
  bool opaque_hit = scene_intersect_shadow_all(kg, ray, isect, visibility, max_hits, &num_hits);

  /* If number of hits exceed the transparent bounces limit, make opaque. */
  if (num_hits > max_transparent_hits) {
    opaque_hit = true;
  }

  if (!opaque_hit) {
    uint num_recorded_hits = min(num_hits, max_hits);

    if (num_recorded_hits > 0) {
      sort_intersections(isect, num_recorded_hits);

      /* Write intersection result into global integrator state memory.
       * More efficient may be to do this directly from the intersection kernel. */
      for (int hit = 0; hit < num_recorded_hits; hit++) {
        integrator_state_write_shadow_isect(state, &isect[hit], hit);
      }
    }

    INTEGRATOR_STATE_WRITE(state, shadow_path, num_hits) = num_hits;
  }
  else {
    INTEGRATOR_STATE_WRITE(state, shadow_path, num_hits) = 0;
  }

  return opaque_hit;
}
#endif

ccl_device void integrator_intersect_shadow(KernelGlobals kg, IntegratorState state)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_SHADOW);

  /* Read ray from integrator state into local memory. */
  Ray ray ccl_optional_struct_init;
  integrator_state_read_shadow_ray(kg, state, &ray);

  /* Compute visibility. */
  const uint visibility = integrate_intersect_shadow_visibility(kg, state);

#ifdef __TRANSPARENT_SHADOWS__
  /* TODO: compile different kernels depending on this? Especially for OptiX
   * conditional trace calls are bad. */
  const bool opaque_hit = (kernel_data.integrator.transparent_shadows) ?
                              integrate_intersect_shadow_transparent(kg, state, &ray, visibility) :
                              integrate_intersect_shadow_opaque(kg, state, &ray, visibility);
#else
  const bool opaque_hit = integrate_intersect_shadow_opaque(kg, state, &ray, visibility);
#endif

  if (opaque_hit) {
    /* Hit an opaque surface, shadow path ends here. */
    INTEGRATOR_SHADOW_PATH_TERMINATE(DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW);
    return;
  }
  else {
    /* Hit nothing or transparent surfaces, continue to shadow kernel
     * for shading and render buffer output.
     *
     * TODO: could also write to render buffer directly if no transparent shadows?
     * Could save a kernel execution for the common case. */
    INTEGRATOR_SHADOW_PATH_NEXT(DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW,
                                DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW);
    return;
  }
}

CCL_NAMESPACE_END
