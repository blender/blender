/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

/* Visibility for the shadow ray. */
ccl_device_forceinline uint integrate_intersect_shadow_visibility(KernelGlobals kg,
                                                                  ConstIntegratorShadowState state)
{
  uint visibility = PATH_RAY_SHADOW;

#ifdef __SHADOW_CATCHER__
  const uint32_t path_flag = INTEGRATOR_STATE(state, shadow_path, flag);
  visibility = SHADOW_CATCHER_PATH_VISIBILITY(path_flag, visibility);
#endif

  return visibility;
}

ccl_device bool integrate_intersect_shadow_opaque(KernelGlobals kg,
                                                  IntegratorShadowState state,
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
                                                                 ConstIntegratorShadowState state)
{
  const int transparent_max_bounce = kernel_data.integrator.transparent_max_bounce;
  const int transparent_bounce = INTEGRATOR_STATE(state, shadow_path, transparent_bounce);

  return max(transparent_max_bounce - transparent_bounce - 1, 0);
}

#ifdef __TRANSPARENT_SHADOWS__
#  ifndef __KERNEL_GPU__
ccl_device int shadow_intersections_compare(const void *a, const void *b)
{
  const Intersection *isect_a = (const Intersection *)a;
  const Intersection *isect_b = (const Intersection *)b;

  if (isect_a->t < isect_b->t)
    return -1;
  else if (isect_a->t > isect_b->t)
    return 1;
  else
    return 0;
}
#  endif

ccl_device_inline void sort_shadow_intersections(IntegratorShadowState state, uint num_hits)
{
  kernel_assert(num_hits > 0);

#  ifdef __KERNEL_GPU__
  /* Use bubble sort which has more friendly memory pattern on GPU. */
  bool swapped;
  do {
    swapped = false;
    for (int j = 0; j < num_hits - 1; ++j) {
      if (INTEGRATOR_STATE_ARRAY(state, shadow_isect, j, t) >
          INTEGRATOR_STATE_ARRAY(state, shadow_isect, j + 1, t))
      {
        struct Intersection tmp_j ccl_optional_struct_init;
        struct Intersection tmp_j_1 ccl_optional_struct_init;
        integrator_state_read_shadow_isect(state, &tmp_j, j);
        integrator_state_read_shadow_isect(state, &tmp_j_1, j + 1);
        integrator_state_write_shadow_isect(state, &tmp_j_1, j);
        integrator_state_write_shadow_isect(state, &tmp_j, j + 1);
        swapped = true;
      }
    }
    --num_hits;
  } while (swapped);
#  else
  Intersection *isect_array = (Intersection *)state->shadow_isect;
  qsort(isect_array, num_hits, sizeof(Intersection), shadow_intersections_compare);
#  endif
}

ccl_device bool integrate_intersect_shadow_transparent(KernelGlobals kg,
                                                       IntegratorShadowState state,
                                                       ccl_private const Ray *ray,
                                                       const uint visibility)
{
  /* Limit the number hits to the max transparent bounces allowed and the size that we
   * have available in the integrator state. */
  const uint max_hits = integrate_shadow_max_transparent_hits(kg, state);
  uint num_hits = 0;
  float throughput = 1.0f;
  bool opaque_hit = scene_intersect_shadow_all(
      kg, state, ray, visibility, max_hits, &num_hits, &throughput);

  /* Computed throughput from baked shadow transparency, where we can bypass recording
   * intersections and shader evaluation. */
  if (throughput != 1.0f) {
    INTEGRATOR_STATE_WRITE(state, shadow_path, throughput) *= throughput;
  }

  /* If number of hits exceed the transparent bounces limit, make opaque. */
  if (num_hits > max_hits) {
    opaque_hit = true;
  }

  if (!opaque_hit) {
    const uint num_recorded_hits = min(num_hits, min(max_hits, INTEGRATOR_SHADOW_ISECT_SIZE));

    if (num_recorded_hits > 0) {
      sort_shadow_intersections(state, num_recorded_hits);
    }

    INTEGRATOR_STATE_WRITE(state, shadow_path, num_hits) = num_hits;
  }
  else {
    INTEGRATOR_STATE_WRITE(state, shadow_path, num_hits) = 0;
  }

  return opaque_hit;
}
#endif

ccl_device void integrator_intersect_shadow(KernelGlobals kg, IntegratorShadowState state)
{
  PROFILING_INIT(kg, PROFILING_INTERSECT_SHADOW);

  /* Read ray from integrator state into local memory. */
  Ray ray ccl_optional_struct_init;
  integrator_state_read_shadow_ray(state, &ray);
  ray.self.object = INTEGRATOR_STATE_ARRAY(state, shadow_isect, 0, object);
  ray.self.prim = INTEGRATOR_STATE_ARRAY(state, shadow_isect, 0, prim);
  ray.self.light_object = INTEGRATOR_STATE_ARRAY(state, shadow_isect, 1, object);
  ray.self.light_prim = INTEGRATOR_STATE_ARRAY(state, shadow_isect, 1, prim);
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
    integrator_shadow_path_terminate(kg, state, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW);
    return;
  }
  else {
    /* Hit nothing or transparent surfaces, continue to shadow kernel
     * for shading and render buffer output.
     *
     * TODO: could also write to render buffer directly if no transparent shadows?
     * Could save a kernel execution for the common case. */
    integrator_shadow_path_next(kg,
                                state,
                                DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW,
                                DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW);
    return;
  }
}

CCL_NAMESPACE_END
