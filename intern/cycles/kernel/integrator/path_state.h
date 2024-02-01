/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/sample/pattern.h"

CCL_NAMESPACE_BEGIN

/* Initialize queues, so that the this path is considered terminated.
 * Used for early outputs in the camera ray initialization, as well as initialization of split
 * states for shadow catcher. */
ccl_device_inline void path_state_init_queues(IntegratorState state)
{
  INTEGRATOR_STATE_WRITE(state, path, queued_kernel) = 0;
#ifndef __KERNEL_GPU__
  INTEGRATOR_STATE_WRITE(&state->shadow, shadow_path, queued_kernel) = 0;
  INTEGRATOR_STATE_WRITE(&state->ao, shadow_path, queued_kernel) = 0;
#endif
}

/* Minimalistic initialization of the path state, which is needed for early outputs in the
 * integrator initialization to work. */
ccl_device_inline void path_state_init(IntegratorState state,
                                       ccl_global const KernelWorkTile *ccl_restrict tile,
                                       const int x,
                                       const int y)
{
  const uint render_pixel_index = (uint)tile->offset + x + y * tile->stride;

  INTEGRATOR_STATE_WRITE(state, path, render_pixel_index) = render_pixel_index;

  path_state_init_queues(state);
}

/* Initialize the rest of the path state needed to continue the path integration. */
ccl_device_inline void path_state_init_integrator(KernelGlobals kg,
                                                  IntegratorState state,
                                                  const int sample,
                                                  const uint rng_hash)
{
  INTEGRATOR_STATE_WRITE(state, path, sample) = sample;
  INTEGRATOR_STATE_WRITE(state, path, bounce) = 0;
  INTEGRATOR_STATE_WRITE(state, path, diffuse_bounce) = 0;
  INTEGRATOR_STATE_WRITE(state, path, glossy_bounce) = 0;
  INTEGRATOR_STATE_WRITE(state, path, transmission_bounce) = 0;
  INTEGRATOR_STATE_WRITE(state, path, transparent_bounce) = 0;
  INTEGRATOR_STATE_WRITE(state, path, volume_bounce) = 0;
  INTEGRATOR_STATE_WRITE(state, path, volume_bounds_bounce) = 0;
  INTEGRATOR_STATE_WRITE(state, path, rng_hash) = rng_hash;
  INTEGRATOR_STATE_WRITE(state, path, rng_offset) = PRNG_BOUNCE_NUM;
  INTEGRATOR_STATE_WRITE(state, path, flag) = PATH_RAY_CAMERA | PATH_RAY_MIS_SKIP |
                                              PATH_RAY_TRANSPARENT_BACKGROUND;
  INTEGRATOR_STATE_WRITE(state, path, mis_ray_pdf) = 0.0f;
  INTEGRATOR_STATE_WRITE(state, path, min_ray_pdf) = FLT_MAX;
  INTEGRATOR_STATE_WRITE(state, path, continuation_probability) = 1.0f;
  INTEGRATOR_STATE_WRITE(state, path, throughput) = one_spectrum();

#ifdef __PATH_GUIDING__
  INTEGRATOR_STATE_WRITE(state, path, unguided_throughput) = 1.0f;
  INTEGRATOR_STATE_WRITE(state, guiding, path_segment) = nullptr;
  INTEGRATOR_STATE_WRITE(state, guiding, use_surface_guiding) = false;
  INTEGRATOR_STATE_WRITE(state, guiding, sample_surface_guiding_rand) = 0.5f;
  INTEGRATOR_STATE_WRITE(state, guiding, surface_guiding_sampling_prob) = 0.0f;
  INTEGRATOR_STATE_WRITE(state, guiding, bssrdf_sampling_prob) = 0.0f;
  INTEGRATOR_STATE_WRITE(state, guiding, use_volume_guiding) = false;
  INTEGRATOR_STATE_WRITE(state, guiding, sample_volume_guiding_rand) = 0.5f;
  INTEGRATOR_STATE_WRITE(state, guiding, volume_guiding_sampling_prob) = 0.0f;
#endif

#ifdef __MNEE__
  INTEGRATOR_STATE_WRITE(state, path, mnee) = 0;
#endif

  INTEGRATOR_STATE_WRITE(state, isect, object) = OBJECT_NONE;
  INTEGRATOR_STATE_WRITE(state, isect, prim) = PRIM_NONE;

  if (kernel_data.kernel_features & KERNEL_FEATURE_VOLUME) {
    INTEGRATOR_STATE_ARRAY_WRITE(state, volume_stack, 0, object) = OBJECT_NONE;
    INTEGRATOR_STATE_ARRAY_WRITE(
        state, volume_stack, 0, shader) = kernel_data.background.volume_shader;
    INTEGRATOR_STATE_ARRAY_WRITE(state, volume_stack, 1, object) = OBJECT_NONE;
    INTEGRATOR_STATE_ARRAY_WRITE(state, volume_stack, 1, shader) = SHADER_NONE;
  }

#ifdef __DENOISING_FEATURES__
  if (kernel_data.kernel_features & KERNEL_FEATURE_DENOISING) {
    INTEGRATOR_STATE_WRITE(state, path, flag) |= PATH_RAY_DENOISING_FEATURES;
    INTEGRATOR_STATE_WRITE(state, path, denoising_feature_throughput) = one_spectrum();
  }
#endif
}

ccl_device_inline void path_state_next(KernelGlobals kg,
                                       IntegratorState state,
                                       const int label,
                                       const int shader_flag)
{
  uint32_t flag = INTEGRATOR_STATE(state, path, flag);

  /* ray through transparent keeps same flags from previous ray and is
   * not counted as a regular bounce, transparent has separate max */
  if (label & LABEL_TRANSPARENT) {
    uint32_t transparent_bounce = INTEGRATOR_STATE(state, path, transparent_bounce) + 1;

    flag |= PATH_RAY_TRANSPARENT;
    if (transparent_bounce >= kernel_data.integrator.transparent_max_bounce) {
      flag |= PATH_RAY_TERMINATE_ON_NEXT_SURFACE;
    }

    INTEGRATOR_STATE_WRITE(state, path, flag) = flag;
    INTEGRATOR_STATE_WRITE(state, path, transparent_bounce) = transparent_bounce;
    /* Random number generator next bounce. */
    INTEGRATOR_STATE_WRITE(state, path, rng_offset) += PRNG_BOUNCE_NUM;
    return;
  }

  uint32_t bounce = INTEGRATOR_STATE(state, path, bounce) + 1;
  if (bounce >= kernel_data.integrator.max_bounce) {
    flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
  }

  flag &= ~(PATH_RAY_ALL_VISIBILITY | PATH_RAY_MIS_SKIP | PATH_RAY_MIS_HAD_TRANSMISSION);

#ifdef __VOLUME__
  if (label & LABEL_VOLUME_SCATTER) {
    /* volume scatter */
    flag |= PATH_RAY_VOLUME_SCATTER | PATH_RAY_MIS_HAD_TRANSMISSION;
    flag &= ~PATH_RAY_TRANSPARENT_BACKGROUND;
    if (!(flag & PATH_RAY_ANY_PASS)) {
      flag |= PATH_RAY_VOLUME_PASS;
    }

    const int volume_bounce = INTEGRATOR_STATE(state, path, volume_bounce) + 1;
    INTEGRATOR_STATE_WRITE(state, path, volume_bounce) = volume_bounce;
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
        const int diffuse_bounce = INTEGRATOR_STATE(state, path, diffuse_bounce) + 1;
        INTEGRATOR_STATE_WRITE(state, path, diffuse_bounce) = diffuse_bounce;
        if (diffuse_bounce >= kernel_data.integrator.max_diffuse_bounce) {
          flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
        }
      }
      else {
        const int glossy_bounce = INTEGRATOR_STATE(state, path, glossy_bounce) + 1;
        INTEGRATOR_STATE_WRITE(state, path, glossy_bounce) = glossy_bounce;
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

      const int transmission_bounce = INTEGRATOR_STATE(state, path, transmission_bounce) + 1;
      INTEGRATOR_STATE_WRITE(state, path, transmission_bounce) = transmission_bounce;
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

    /* Flag for consistent MIS weights with light tree. */
    if (shader_flag & SD_BSDF_HAS_TRANSMISSION) {
      flag |= PATH_RAY_MIS_HAD_TRANSMISSION;
    }

    /* Render pass categories. */
    if (!(flag & PATH_RAY_ANY_PASS) && !(flag & PATH_RAY_TRANSPARENT_BACKGROUND)) {
      flag |= PATH_RAY_SURFACE_PASS;
    }
  }

  INTEGRATOR_STATE_WRITE(state, path, flag) = flag;
  INTEGRATOR_STATE_WRITE(state, path, bounce) = bounce;

  /* Random number generator next bounce. */
  INTEGRATOR_STATE_WRITE(state, path, rng_offset) += PRNG_BOUNCE_NUM;
}

#ifdef __VOLUME__
ccl_device_inline bool path_state_volume_next(IntegratorState state)
{
  /* For volume bounding meshes we pass through without counting transparent
   * bounces, only sanity check in case self intersection gets us stuck. */
  uint32_t volume_bounds_bounce = INTEGRATOR_STATE(state, path, volume_bounds_bounce) + 1;
  INTEGRATOR_STATE_WRITE(state, path, volume_bounds_bounce) = volume_bounds_bounce;
  if (volume_bounds_bounce > VOLUME_BOUNDS_MAX) {
    return false;
  }

  /* Random number generator next bounce. */
  INTEGRATOR_STATE_WRITE(state, path, rng_offset) += PRNG_BOUNCE_NUM;

  return true;
}
#endif

ccl_device_inline uint path_state_ray_visibility(ConstIntegratorState state)
{
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

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

ccl_device_inline float path_state_continuation_probability(KernelGlobals kg,
                                                            ConstIntegratorState state,
                                                            const uint32_t path_flag)
{
  if (path_flag & PATH_RAY_TRANSPARENT) {
    const uint32_t transparent_bounce = INTEGRATOR_STATE(state, path, transparent_bounce);
    /* Do at least specified number of bounces without RR. */
    if (transparent_bounce <= kernel_data.integrator.transparent_min_bounce) {
      return 1.0f;
    }
  }
  else {
    const uint32_t bounce = INTEGRATOR_STATE(state, path, bounce);
    /* Do at least specified number of bounces without RR. */
    if (bounce <= kernel_data.integrator.min_bounce) {
      return 1.0f;
    }
  }

  /* Probabilistic termination: use sqrt() to roughly match typical view
   * transform and do path termination a bit later on average. */
  Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  throughput *= INTEGRATOR_STATE(state, path, unguided_throughput);
#endif
  return min(sqrtf(reduce_max(fabs(throughput))), 1.0f);
}

ccl_device_inline bool path_state_ao_bounce(KernelGlobals kg, ConstIntegratorState state)
{
  if (!kernel_data.integrator.ao_bounces) {
    return false;
  }

  const int bounce = INTEGRATOR_STATE(state, path, bounce) -
                     INTEGRATOR_STATE(state, path, transmission_bounce) -
                     (INTEGRATOR_STATE(state, path, glossy_bounce) > 0) + 1;
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

ccl_device_inline void path_state_rng_load(ConstIntegratorState state,
                                           ccl_private RNGState *rng_state)
{
  rng_state->rng_hash = INTEGRATOR_STATE(state, path, rng_hash);
  rng_state->rng_offset = INTEGRATOR_STATE(state, path, rng_offset);
  rng_state->sample = INTEGRATOR_STATE(state, path, sample);
}

ccl_device_inline void shadow_path_state_rng_load(ConstIntegratorShadowState state,
                                                  ccl_private RNGState *rng_state)
{
  rng_state->rng_hash = INTEGRATOR_STATE(state, shadow_path, rng_hash);
  rng_state->rng_offset = INTEGRATOR_STATE(state, shadow_path, rng_offset);
  rng_state->sample = INTEGRATOR_STATE(state, shadow_path, sample);
}

ccl_device_inline float path_state_rng_1D(KernelGlobals kg,
                                          ccl_private const RNGState *rng_state,
                                          const int dimension)
{
  return path_rng_1D(
      kg, rng_state->rng_hash, rng_state->sample, rng_state->rng_offset + dimension);
}

ccl_device_inline float2 path_state_rng_2D(KernelGlobals kg,
                                           ccl_private const RNGState *rng_state,
                                           const int dimension)
{
  return path_rng_2D(
      kg, rng_state->rng_hash, rng_state->sample, rng_state->rng_offset + dimension);
}

ccl_device_inline float3 path_state_rng_3D(KernelGlobals kg,
                                           ccl_private const RNGState *rng_state,
                                           const int dimension)
{
  return path_rng_3D(
      kg, rng_state->rng_hash, rng_state->sample, rng_state->rng_offset + dimension);
}

ccl_device_inline float path_branched_rng_1D(KernelGlobals kg,
                                             ccl_private const RNGState *rng_state,
                                             const int branch,
                                             const int num_branches,
                                             const int dimension)
{
  return path_rng_1D(kg,
                     rng_state->rng_hash,
                     rng_state->sample * num_branches + branch,
                     rng_state->rng_offset + dimension);
}

ccl_device_inline float2 path_branched_rng_2D(KernelGlobals kg,
                                              ccl_private const RNGState *rng_state,
                                              const int branch,
                                              const int num_branches,
                                              const int dimension)
{
  return path_rng_2D(kg,
                     rng_state->rng_hash,
                     rng_state->sample * num_branches + branch,
                     rng_state->rng_offset + dimension);
}

ccl_device_inline float3 path_branched_rng_3D(KernelGlobals kg,
                                              ccl_private const RNGState *rng_state,
                                              const int branch,
                                              const int num_branches,
                                              const int dimension)
{
  return path_rng_3D(kg,
                     rng_state->rng_hash,
                     rng_state->sample * num_branches + branch,
                     rng_state->rng_offset + dimension);
}

/* Utility functions to get light termination value,
 * since it might not be needed in many cases.
 */
ccl_device_inline float path_state_rng_light_termination(KernelGlobals kg,
                                                         ccl_private const RNGState *state)
{
  if (kernel_data.integrator.light_inv_rr_threshold > 0.0f) {
    return path_state_rng_1D(kg, state, PRNG_LIGHT_TERMINATE);
  }
  return 0.0f;
}

CCL_NAMESPACE_END
