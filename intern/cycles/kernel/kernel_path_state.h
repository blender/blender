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

CCL_NAMESPACE_BEGIN

ccl_device_inline void path_state_init(KernelGlobals *kg,
                                       ShaderData *stack_sd,
                                       ccl_addr_space PathState *state,
                                       uint rng_hash,
                                       int sample,
                                       ccl_addr_space Ray *ray)
{
  state->flag = PATH_RAY_CAMERA | PATH_RAY_MIS_SKIP | PATH_RAY_TRANSPARENT_BACKGROUND;

  state->rng_hash = rng_hash;
  state->rng_offset = PRNG_BASE_NUM;
  state->sample = sample;
  state->num_samples = kernel_data.integrator.aa_samples;
  state->branch_factor = 1.0f;

  state->bounce = 0;
  state->diffuse_bounce = 0;
  state->glossy_bounce = 0;
  state->transmission_bounce = 0;
  state->transparent_bounce = 0;

#ifdef __DENOISING_FEATURES__
  if (kernel_data.film.pass_denoising_data) {
    state->flag |= PATH_RAY_STORE_SHADOW_INFO;
    state->denoising_feature_weight = 1.0f;
  }
  else {
    state->denoising_feature_weight = 0.0f;
  }
#endif /* __DENOISING_FEATURES__ */

  state->min_ray_pdf = FLT_MAX;
  state->ray_pdf = 0.0f;
#ifdef __LAMP_MIS__
  state->ray_t = 0.0f;
#endif

#ifdef __VOLUME__
  state->volume_bounce = 0;
  state->volume_bounds_bounce = 0;

  if (kernel_data.integrator.use_volumes) {
    /* Initialize volume stack with volume we are inside of. */
    kernel_volume_stack_init(kg, stack_sd, state, ray, state->volume_stack);
  }
  else {
    state->volume_stack[0].shader = SHADER_NONE;
  }
#endif
}

ccl_device_inline void path_state_next(KernelGlobals *kg,
                                       ccl_addr_space PathState *state,
                                       int label)
{
  /* ray through transparent keeps same flags from previous ray and is
   * not counted as a regular bounce, transparent has separate max */
  if (label & LABEL_TRANSPARENT) {
    state->flag |= PATH_RAY_TRANSPARENT;
    state->transparent_bounce++;
    if (state->transparent_bounce >= kernel_data.integrator.transparent_max_bounce) {
      state->flag |= PATH_RAY_TERMINATE_IMMEDIATE;
    }

    if (!kernel_data.integrator.transparent_shadows)
      state->flag |= PATH_RAY_MIS_SKIP;

    /* random number generator next bounce */
    state->rng_offset += PRNG_BOUNCE_NUM;

    return;
  }

  state->bounce++;
  if (state->bounce >= kernel_data.integrator.max_bounce) {
    state->flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
  }

  state->flag &= ~(PATH_RAY_ALL_VISIBILITY | PATH_RAY_MIS_SKIP);

#ifdef __VOLUME__
  if (label & LABEL_VOLUME_SCATTER) {
    /* volume scatter */
    state->flag |= PATH_RAY_VOLUME_SCATTER;
    state->flag &= ~PATH_RAY_TRANSPARENT_BACKGROUND;

    state->volume_bounce++;
    if (state->volume_bounce >= kernel_data.integrator.max_volume_bounce) {
      state->flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
    }
  }
  else
#endif
  {
    /* surface reflection/transmission */
    if (label & LABEL_REFLECT) {
      state->flag |= PATH_RAY_REFLECT;
      state->flag &= ~PATH_RAY_TRANSPARENT_BACKGROUND;

      if (label & LABEL_DIFFUSE) {
        state->diffuse_bounce++;
        if (state->diffuse_bounce >= kernel_data.integrator.max_diffuse_bounce) {
          state->flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
        }
      }
      else {
        state->glossy_bounce++;
        if (state->glossy_bounce >= kernel_data.integrator.max_glossy_bounce) {
          state->flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
        }
      }
    }
    else {
      kernel_assert(label & LABEL_TRANSMIT);

      state->flag |= PATH_RAY_TRANSMIT;

      if (!(label & LABEL_TRANSMIT_TRANSPARENT)) {
        state->flag &= ~PATH_RAY_TRANSPARENT_BACKGROUND;
      }

      state->transmission_bounce++;
      if (state->transmission_bounce >= kernel_data.integrator.max_transmission_bounce) {
        state->flag |= PATH_RAY_TERMINATE_AFTER_TRANSPARENT;
      }
    }

    /* diffuse/glossy/singular */
    if (label & LABEL_DIFFUSE) {
      state->flag |= PATH_RAY_DIFFUSE | PATH_RAY_DIFFUSE_ANCESTOR;
    }
    else if (label & LABEL_GLOSSY) {
      state->flag |= PATH_RAY_GLOSSY;
    }
    else {
      kernel_assert(label & LABEL_SINGULAR);
      state->flag |= PATH_RAY_GLOSSY | PATH_RAY_SINGULAR | PATH_RAY_MIS_SKIP;
    }
  }

  /* random number generator next bounce */
  state->rng_offset += PRNG_BOUNCE_NUM;

#ifdef __DENOISING_FEATURES__
  if ((state->denoising_feature_weight == 0.0f) && !(state->flag & PATH_RAY_SHADOW_CATCHER)) {
    state->flag &= ~PATH_RAY_STORE_SHADOW_INFO;
  }
#endif
}

#ifdef __VOLUME__
ccl_device_inline bool path_state_volume_next(KernelGlobals *kg, ccl_addr_space PathState *state)
{
  /* For volume bounding meshes we pass through without counting transparent
   * bounces, only sanity check in case self intersection gets us stuck. */
  state->volume_bounds_bounce++;
  if (state->volume_bounds_bounce > VOLUME_BOUNDS_MAX) {
    return false;
  }

  /* Random number generator next bounce. */
  if (state->volume_bounds_bounce > 1) {
    state->rng_offset += PRNG_BOUNCE_NUM;
  }

  return true;
}
#endif

ccl_device_inline uint path_state_ray_visibility(KernelGlobals *kg,
                                                 ccl_addr_space PathState *state)
{
  uint flag = state->flag & PATH_RAY_ALL_VISIBILITY;

  /* for visibility, diffuse/glossy are for reflection only */
  if (flag & PATH_RAY_TRANSMIT)
    flag &= ~(PATH_RAY_DIFFUSE | PATH_RAY_GLOSSY);
  /* todo: this is not supported as its own ray visibility yet */
  if (state->flag & PATH_RAY_VOLUME_SCATTER)
    flag |= PATH_RAY_DIFFUSE;

  return flag;
}

ccl_device_inline float path_state_continuation_probability(KernelGlobals *kg,
                                                            ccl_addr_space PathState *state,
                                                            const float3 throughput)
{
  if (state->flag & PATH_RAY_TERMINATE_IMMEDIATE) {
    /* Ray is to be terminated immediately. */
    return 0.0f;
  }
  else if (state->flag & PATH_RAY_TRANSPARENT) {
    /* Do at least specified number of bounces without RR. */
    if (state->transparent_bounce <= kernel_data.integrator.transparent_min_bounce) {
      return 1.0f;
    }
#ifdef __SHADOW_TRICKS__
    /* Exception for shadow catcher not working correctly with RR. */
    else if ((state->flag & PATH_RAY_SHADOW_CATCHER) && (state->transparent_bounce <= 8)) {
      return 1.0f;
    }
#endif
  }
  else {
    /* Do at least specified number of bounces without RR. */
    if (state->bounce <= kernel_data.integrator.min_bounce) {
      return 1.0f;
    }
#ifdef __SHADOW_TRICKS__
    /* Exception for shadow catcher not working correctly with RR. */
    else if ((state->flag & PATH_RAY_SHADOW_CATCHER) && (state->bounce <= 3)) {
      return 1.0f;
    }
#endif
  }

  /* Probabilistic termination: use sqrt() to roughly match typical view
   * transform and do path termination a bit later on average. */
  return min(sqrtf(max3(fabs(throughput)) * state->branch_factor), 1.0f);
}

/* TODO(DingTo): Find more meaningful name for this */
ccl_device_inline void path_state_modify_bounce(ccl_addr_space PathState *state, bool increase)
{
  /* Modify bounce temporarily for shader eval */
  if (increase)
    state->bounce += 1;
  else
    state->bounce -= 1;
}

ccl_device_inline bool path_state_ao_bounce(KernelGlobals *kg, ccl_addr_space PathState *state)
{
  if (state->bounce <= kernel_data.integrator.ao_bounces) {
    return false;
  }

  int bounce = state->bounce - state->transmission_bounce - (state->glossy_bounce > 0);
  return (bounce > kernel_data.integrator.ao_bounces);
}

ccl_device_inline void path_state_branch(ccl_addr_space PathState *state,
                                         int branch,
                                         int num_branches)
{
  if (num_branches > 1) {
    /* Path is splitting into a branch, adjust so that each branch
     * still gets a unique sample from the same sequence. */
    state->sample = state->sample * num_branches + branch;
    state->num_samples = state->num_samples * num_branches;
    state->branch_factor *= num_branches;
  }
}

CCL_NAMESPACE_END
