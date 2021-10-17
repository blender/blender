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

#include "kernel/kernel_accumulate.h"
#include "kernel/kernel_adaptive_sampling.h"
#include "kernel/kernel_camera.h"
#include "kernel/kernel_path_state.h"
#include "kernel/kernel_random.h"
#include "kernel/kernel_shadow_catcher.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void integrate_camera_sample(KernelGlobals kg,
                                               const int sample,
                                               const int x,
                                               const int y,
                                               const uint rng_hash,
                                               ccl_private Ray *ray)
{
  /* Filter sampling. */
  float filter_u, filter_v;

  if (sample == 0) {
    filter_u = 0.5f;
    filter_v = 0.5f;
  }
  else {
    path_rng_2D(kg, rng_hash, sample, PRNG_FILTER_U, &filter_u, &filter_v);
  }

  /* Depth of field sampling. */
  float lens_u = 0.0f, lens_v = 0.0f;
  if (kernel_data.cam.aperturesize > 0.0f) {
    path_rng_2D(kg, rng_hash, sample, PRNG_LENS_U, &lens_u, &lens_v);
  }

  /* Motion blur time sampling. */
  float time = 0.0f;
#ifdef __CAMERA_MOTION__
  if (kernel_data.cam.shuttertime != -1.0f)
    time = path_rng_1D(kg, rng_hash, sample, PRNG_TIME);
#endif

  /* Generate camera ray. */
  camera_sample(kg, x, y, filter_u, filter_v, lens_u, lens_v, time, ray);
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
  if (!kernel_need_sample_pixel(kg, state, render_buffer)) {
    return false;
  }

  /* Count the sample and get an effective sample for this pixel.
   *
   * This logic allows to both count actual number of samples per pixel, and to add samples to this
   * pixel after it was converged and samples were added somewhere else (in which case the
   * `scheduled_sample` will be different from actual number of samples in this pixel). */
  const int sample = kernel_accum_sample(kg, state, render_buffer, scheduled_sample);

  /* Initialize random number seed for path. */
  const uint rng_hash = path_rng_hash_init(kg, sample, x, y);

  {
    /* Generate camera ray. */
    Ray ray;
    integrate_camera_sample(kg, sample, x, y, rng_hash, &ray);
    if (ray.t == 0.0f) {
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
    INTEGRATOR_PATH_INIT(DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK);
  }
  else {
    INTEGRATOR_PATH_INIT(DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
  }

  return true;
}

CCL_NAMESPACE_END
