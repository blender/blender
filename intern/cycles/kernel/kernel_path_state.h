/*
 * Copyright 2011-2013 Blender Foundation
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

#include "kernel_random.h"

CCL_NAMESPACE_BEGIN

/* Initialize queues, so that the this path is considered terminated.
 * Used for early outputs in the camera ray initialization, as well as initialization of split
 * states for shadow catcher. */
ccl_device_inline void path_state_init_queues(INTEGRATOR_STATE_ARGS)
{
  INTEGRATOR_STATE_WRITE(path, queued_kernel) = 0;
  INTEGRATOR_STATE_WRITE(shadow_path, queued_kernel) = 0;
}

/* Minimalistic initialization of the path state, which is needed for early outputs in the
 * integrator initialization to work. */
ccl_device_inline void path_state_init(INTEGRATOR_STATE_ARGS,
                                       ccl_global const KernelWorkTile *ccl_restrict tile,
                                       const int x,
                                       const int y)
{
  const uint render_pixel_index = (uint)tile->offset + x + y * tile->stride;

  INTEGRATOR_STATE_WRITE(path, render_pixel_index) = render_pixel_index;

  path_state_init_queues(INTEGRATOR_STATE_PASS);
}

/* Initialize the rest of the path state needed to continue the path integration. */
ccl_device_inline void path_state_init_integrator(INTEGRATOR_STATE_ARGS,
                                                  const int sample,
                                                  const uint rng_hash)
{
  INTEGRATOR_STATE_WRITE(path, sample) = sample;
  INTEGRATOR_STATE_WRITE(path, bounce) = 0;
  INTEGRATOR_STATE_WRITE(path, diffuse_bounce) = 0;
  INTEGRATOR_STATE_WRITE(path, glossy_bounce) = 0;
  INTEGRATOR_STATE_WRITE(path, transmission_bounce) = 0;
  INTEGRATOR_STATE_WRITE(path, transparent_bounce) = 0;
  INTEGRATOR_STATE_WRITE(path, volume_bounce) = 0;
  INTEGRATOR_STATE_WRITE(path, volume_bounds_bounce) = 0;
  INTEGRATOR_STATE_WRITE(path, rng_hash) = rng_hash;
  INTEGRATOR_STATE_WRITE(path, rng_offset) = PRNG_BASE_NUM;
  INTEGRATOR_STATE_WRITE(path, flag) = PATH_RAY_CAMERA | PATH_RAY_MIS_SKIP |
                                       PATH_RAY_TRANSPARENT_BACKGROUND;
  INTEGRATOR_STATE_WRITE(path, mis_ray_pdf) = 0.0f;
  INTEGRATOR_STATE_WRITE(path, mis_ray_t) = 0.0f;
  INTEGRATOR_STATE_WRITE(path, min_ray_pdf) = FLT_MAX;
  INTEGRATOR_STATE_WRITE(path, throughput) = make_float3(1.0f, 1.0f, 1.0f);

  if (kernel_data.kernel_features & KERNEL_FEATURE_VOLUME) {
    INTEGRATOR_STATE_ARRAY_WRITE(volume_stack, 0, object) = OBJECT_NONE;
    INTEGRATOR_STATE_ARRAY_WRITE(volume_stack, 0, shader) = kernel_data.background.volume_shader;
    INTEGRATOR_STATE_ARRAY_WRITE(volume_stack, 1, object) = OBJECT_NONE;
    INTEGRATOR_STATE_ARRAY_WRITE(volume_stack, 1, shader) = SHADER_NONE;
  }

#ifdef __DENOISING_FEATURES__
  if (kernel_data.kernel_features & KERNEL_FEATURE_DENOISING) {
    INTEGRATOR_STATE_WRITE(path, flag) |= PATH_RAY_DENOISING_FEATURES;
    INTEGRATOR_STATE_WRITE(path, denoising_feature_throughput) = one_float3();
  }
#endif
}

ccl_device_inline void path_state_next(INTEGRATOR_STATE_ARGS, int label)
{
  uint32_t flag = INTEGRATOR_STATE(path, flag);

  /* ray through transparent keeps same flags from previous ray and is
   * not counted as a regular bounce, transparent has separate max */
  if (label & LABEL_TRANSPARENT) {
    uint32_t transparent_bounce = INTEGRATOR_STATE(path, transparent_bounce) + 1;

    flag |= PATH_RAY_TRANSPARENT;
    if (transparent_bounce >= kernel_data.integrator.transparent_max_bounce) {
      flag |= PATH_RAY_TERMINATE_ON_NEXT_SURFACE;
    }

    if (!kernel_data.integrator.transparent_shadows)
      flag |= PATH_RAY_MIS_SKIP;

    INTEGRATOR_STATE_WRITE(path, flag) = flag;
    INTEGRATOR_STATE_WRITE(path, transparent_bounce) = transparent_bounce;
    /* Random number generator next bounce. */
    INTEGRATOR_STATE_WRITE(path, rng_offset) += PRNG_BOUNCE_NUM;
    return;
  }

  uint32_t bounce = INTEGRATOR_STATE(path, bounce) + 1;
  if (bounce >= kernel_data.integrator.max_bounce) {
    flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
  }

  flag &= ~(PATH_RAY_ALL_VISIBILITY | PATH_RAY_MIS_SKIP);

#ifdef __VOLUME__
  if (label & LABEL_VOLUME_SCATTER) {
    /* volume scatter */
    flag |= PATH_RAY_VOLUME_SCATTER;
    flag &= ~PATH_RAY_TRANSPARENT_BACKGROUND;
    if (bounce == 1) {
      flag |= PATH_RAY_VOLUME_PASS;
    }

    const int volume_bounce = INTEGRATOR_STATE(path, volume_bounce) + 1;
    INTEGRATOR_STATE_WRITE(path, volume_bounce) = volume_bounce;
    if (volume_bounce >= kernel_data.integrator.max_volume_bounce) {
      flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
    }
  }
  else
#endif
  {
    /* surface reflection/transmission */
    if (label & LABEL_REFLECT) {
      flag |= PATH_RAY_REFLECT;
      flag &= ~PATH_RAY_TRANSPARENT_BACKGROUND;

      if (label & LABEL_DIFFUSE) {
        const int diffuse_bounce = INTEGRATOR_STATE(path, diffuse_bounce) + 1;
        INTEGRATOR_STATE_WRITE(path, diffuse_bounce) = diffuse_bounce;
        if (diffuse_bounce >= kernel_data.integrator.max_diffuse_bounce) {
          flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
        }
      }
      else {
        const int glossy_bounce = INTEGRATOR_STATE(path, glossy_bounce) + 1;
        INTEGRATOR_STATE_WRITE(path, glossy_bounce) = glossy_bounce;
        if (glossy_bounce >= kernel_data.integrator.max_glossy_bounce) {
          flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
        }
      }
    }
    else {
      kernel_assert(label & LABEL_TRANSMIT);

      flag |= PATH_RAY_TRANSMIT;

      if (!(label & LABEL_TRANSMIT_TRANSPARENT)) {
        flag &= ~PATH_RAY_TRANSPARENT_BACKGROUND;
      }

      const int transmission_bounce = INTEGRATOR_STATE(path, transmission_bounce) + 1;
      INTEGRATOR_STATE_WRITE(path, transmission_bounce) = transmission_bounce;
      if (transmission_bounce >= kernel_data.integrator.max_transmission_bounce) {
        flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
      }
    }

    /* diffuse/glossy/singular */
    if (label & LABEL_DIFFUSE) {
      flag |= PATH_RAY_DIFFUSE | PATH_RAY_DIFFUSE_ANCESTOR;
    }
    else if (label & LABEL_GLOSSY) {
      flag |= PATH_RAY_GLOSSY;
    }
    else {
      kernel_assert(label & LABEL_SINGULAR);
      flag |= PATH_RAY_GLOSSY | PATH_RAY_SINGULAR | PATH_RAY_MIS_SKIP;
    }

    /* Render pass categories. */
    if (bounce == 1) {
      flag |= (label & LABEL_TRANSMIT) ? PATH_RAY_TRANSMISSION_PASS : PATH_RAY_REFLECT_PASS;
    }
  }

  INTEGRATOR_STATE_WRITE(path, flag) = flag;
  INTEGRATOR_STATE_WRITE(path, bounce) = bounce;

  /* Random number generator next bounce. */
  INTEGRATOR_STATE_WRITE(path, rng_offset) += PRNG_BOUNCE_NUM;
}

#ifdef __VOLUME__
ccl_device_inline bool path_state_volume_next(INTEGRATOR_STATE_ARGS)
{
  /* For volume bounding meshes we pass through without counting transparent
   * bounces, only sanity check in case self intersection gets us stuck. */
  uint32_t volume_bounds_bounce = INTEGRATOR_STATE(path, volume_bounds_bounce) + 1;
  INTEGRATOR_STATE_WRITE(path, volume_bounds_bounce) = volume_bounds_bounce;
  if (volume_bounds_bounce > VOLUME_BOUNDS_MAX) {
    return false;
  }

  /* Random number generator next bounce. */
  if (volume_bounds_bounce > 1) {
    INTEGRATOR_STATE_WRITE(path, rng_offset) += PRNG_BOUNCE_NUM;
  }

  return true;
}
#endif

ccl_device_inline uint path_state_ray_visibility(INTEGRATOR_STATE_CONST_ARGS)
{
  const uint32_t path_flag = INTEGRATOR_STATE(path, flag);

  uint32_t visibility = path_flag & PATH_RAY_ALL_VISIBILITY;

  /* For visibility, diffuse/glossy are for reflection only. */
  if (visibility & PATH_RAY_TRANSMIT) {
    visibility &= ~(PATH_RAY_DIFFUSE | PATH_RAY_GLOSSY);
  }

  /* todo: this is not supported as its own ray visibility yet. */
  if (path_flag & PATH_RAY_VOLUME_SCATTER) {
    visibility |= PATH_RAY_DIFFUSE;
  }

  visibility = SHADOW_CATCHER_PATH_VISIBILITY(path_flag, visibility);

  return visibility;
}

ccl_device_inline float path_state_continuation_probability(INTEGRATOR_STATE_CONST_ARGS,
                                                            const uint32_t path_flag)
{
  if (path_flag & PATH_RAY_TRANSPARENT) {
    const uint32_t transparent_bounce = INTEGRATOR_STATE(path, transparent_bounce);
    /* Do at least specified number of bounces without RR. */
    if (transparent_bounce <= kernel_data.integrator.transparent_min_bounce) {
      return 1.0f;
    }
  }
  else {
    const uint32_t bounce = INTEGRATOR_STATE(path, bounce);
    /* Do at least specified number of bounces without RR. */
    if (bounce <= kernel_data.integrator.min_bounce) {
      return 1.0f;
    }
  }

  /* Probabilistic termination: use sqrt() to roughly match typical view
   * transform and do path termination a bit later on average. */
  return min(sqrtf(max3(fabs(INTEGRATOR_STATE(path, throughput)))), 1.0f);
}

ccl_device_inline bool path_state_ao_bounce(INTEGRATOR_STATE_CONST_ARGS)
{
  if (!kernel_data.integrator.ao_bounces) {
    return false;
  }

  const int bounce = INTEGRATOR_STATE(path, bounce) - INTEGRATOR_STATE(path, transmission_bounce) -
                     (INTEGRATOR_STATE(path, glossy_bounce) > 0) + 1;
  return (bounce > kernel_data.integrator.ao_bounces);
}

/* Random Number Sampling Utility Functions
 *
 * For each random number in each step of the path we must have a unique
 * dimension to avoid using the same sequence twice.
 *
 * For branches in the path we must be careful not to reuse the same number
 * in a sequence and offset accordingly.
 */

/* RNG State loaded onto stack. */
typedef struct RNGState {
  uint rng_hash;
  uint rng_offset;
  int sample;
} RNGState;

ccl_device_inline void path_state_rng_load(INTEGRATOR_STATE_CONST_ARGS,
                                           ccl_private RNGState *rng_state)
{
  rng_state->rng_hash = INTEGRATOR_STATE(path, rng_hash);
  rng_state->rng_offset = INTEGRATOR_STATE(path, rng_offset);
  rng_state->sample = INTEGRATOR_STATE(path, sample);
}

ccl_device_inline void shadow_path_state_rng_load(INTEGRATOR_STATE_CONST_ARGS,
                                                  ccl_private RNGState *rng_state)
{
  const uint shadow_bounces = INTEGRATOR_STATE(shadow_path, transparent_bounce) -
                              INTEGRATOR_STATE(path, transparent_bounce);

  rng_state->rng_hash = INTEGRATOR_STATE(path, rng_hash);
  rng_state->rng_offset = INTEGRATOR_STATE(path, rng_offset) + PRNG_BOUNCE_NUM * shadow_bounces;
  rng_state->sample = INTEGRATOR_STATE(path, sample);
}

ccl_device_inline float path_state_rng_1D(ccl_global const KernelGlobals *kg,
                                          ccl_private const RNGState *rng_state,
                                          int dimension)
{
  return path_rng_1D(
      kg, rng_state->rng_hash, rng_state->sample, rng_state->rng_offset + dimension);
}

ccl_device_inline void path_state_rng_2D(ccl_global const KernelGlobals *kg,
                                         ccl_private const RNGState *rng_state,
                                         int dimension,
                                         ccl_private float *fx,
                                         ccl_private float *fy)
{
  path_rng_2D(
      kg, rng_state->rng_hash, rng_state->sample, rng_state->rng_offset + dimension, fx, fy);
}

ccl_device_inline float path_state_rng_1D_hash(ccl_global const KernelGlobals *kg,
                                               ccl_private const RNGState *rng_state,
                                               uint hash)
{
  /* Use a hash instead of dimension, this is not great but avoids adding
   * more dimensions to each bounce which reduces quality of dimensions we
   * are already using. */
  return path_rng_1D(
      kg, cmj_hash_simple(rng_state->rng_hash, hash), rng_state->sample, rng_state->rng_offset);
}

ccl_device_inline float path_branched_rng_1D(ccl_global const KernelGlobals *kg,
                                             ccl_private const RNGState *rng_state,
                                             int branch,
                                             int num_branches,
                                             int dimension)
{
  return path_rng_1D(kg,
                     rng_state->rng_hash,
                     rng_state->sample * num_branches + branch,
                     rng_state->rng_offset + dimension);
}

ccl_device_inline void path_branched_rng_2D(ccl_global const KernelGlobals *kg,
                                            ccl_private const RNGState *rng_state,
                                            int branch,
                                            int num_branches,
                                            int dimension,
                                            ccl_private float *fx,
                                            ccl_private float *fy)
{
  path_rng_2D(kg,
              rng_state->rng_hash,
              rng_state->sample * num_branches + branch,
              rng_state->rng_offset + dimension,
              fx,
              fy);
}

/* Utility functions to get light termination value,
 * since it might not be needed in many cases.
 */
ccl_device_inline float path_state_rng_light_termination(ccl_global const KernelGlobals *kg,
                                                         ccl_private const RNGState *state)
{
  if (kernel_data.integrator.light_inv_rr_threshold > 0.0f) {
    return path_state_rng_1D(kg, state, PRNG_LIGHT_TERMINATE);
  }
  return 0.0f;
}

CCL_NAMESPACE_END
