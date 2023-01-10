/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/camera/camera.h"

#include "kernel/film/adaptive_sampling.h"
#include "kernel/film/light_passes.h"

#include "kernel/integrator/path_state.h"
#include "kernel/integrator/shadow_catcher.h"

#include "kernel/sample/pattern.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void integrate_camera_sample(KernelGlobals kg,
                                               const int sample,
                                               const int x,
                                               const int y,
                                               const uint rng_hash,
                                               ccl_private Ray *ray)
{
  /* Filter sampling. */
  const float2 rand_filter = (sample == 0) ? make_float2(0.5f, 0.5f) :
                                             path_rng_2D(kg, rng_hash, sample, PRNG_FILTER);

  /* Motion blur (time) and depth of field (lens) sampling. (time, lens_x, lens_y) */
  const float3 rand_time_lens = (kernel_data.cam.shuttertime != -1.0f ||
                                 kernel_data.cam.aperturesize > 0.0f) ?
                                    path_rng_3D(kg, rng_hash, sample, PRNG_LENS_TIME) :
                                    zero_float3();

  /* Generate camera ray. */
  camera_sample(kg,
                x,
                y,
                rand_filter.x,
                rand_filter.y,
                rand_time_lens.y,
                rand_time_lens.z,
                rand_time_lens.x,
                ray);
}

/* Return false to indicate that this pixel is finished.
 * Used by CPU implementation to not attempt to sample pixel for multiple samples once its known
 * that the pixel did converge. */
ccl_device bool integrator_init_from_camera(KernelGlobals kg,
                                            IntegratorState state,
                                            ccl_global const KernelWorkTile *ccl_restrict tile,
                                            ccl_global float *render_buffer,
                                            const int x,
                                            const int y,
                                            const int scheduled_sample)
{
  PROFILING_INIT(kg, PROFILING_RAY_SETUP);

  /* Initialize path state to give basic buffer access and allow early outputs. */
  path_state_init(state, tile, x, y);

  /* Check whether the pixel has converged and should not be sampled anymore. */
  if (!film_need_sample_pixel(kg, state, render_buffer)) {
    return false;
  }

  /* Count the sample and get an effective sample for this pixel.
   *
   * This logic allows to both count actual number of samples per pixel, and to add samples to this
   * pixel after it was converged and samples were added somewhere else (in which case the
   * `scheduled_sample` will be different from actual number of samples in this pixel). */
  const int sample = film_write_sample(
      kg, state, render_buffer, scheduled_sample, tile->sample_offset);

  /* Initialize random number seed for path. */
  const uint rng_hash = path_rng_hash_init(kg, sample, x, y);

  {
    /* Generate camera ray. */
    Ray ray;
    integrate_camera_sample(kg, sample, x, y, rng_hash, &ray);
    if (ray.tmax == 0.0f) {
      return true;
    }

    /* Write camera ray to state. */
    integrator_state_write_ray(kg, state, &ray);
  }

  /* Initialize path state for path integration. */
  path_state_init_integrator(kg, state, sample, rng_hash);

  /* Continue with intersect_closest kernel, optionally initializing volume
   * stack before that if the camera may be inside a volume. */
  if (kernel_data.cam.is_inside_volume) {
    integrator_path_init(kg, state, DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK);
  }
  else {
    integrator_path_init(kg, state, DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
  }

  return true;
}

CCL_NAMESPACE_END
