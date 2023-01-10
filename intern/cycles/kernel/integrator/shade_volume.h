/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/film/data_passes.h"
#include "kernel/film/denoising_passes.h"
#include "kernel/film/light_passes.h"

#include "kernel/integrator/guiding.h"
#include "kernel/integrator/intersect_closest.h"
#include "kernel/integrator/path_state.h"
#include "kernel/integrator/volume_shader.h"
#include "kernel/integrator/volume_stack.h"

#include "kernel/light/light.h"
#include "kernel/light/sample.h"

CCL_NAMESPACE_BEGIN

#ifdef __VOLUME__

/* Events for probabilistic scattering. */

typedef enum VolumeIntegrateEvent {
  VOLUME_PATH_SCATTERED = 0,
  VOLUME_PATH_ATTENUATED = 1,
  VOLUME_PATH_MISSED = 2
} VolumeIntegrateEvent;

typedef struct VolumeIntegrateResult {
  /* Throughput and offset for direct light scattering. */
  bool direct_scatter;
  Spectrum direct_throughput;
  float direct_t;
  ShaderVolumePhases direct_phases;
#  ifdef __PATH_GUIDING__
  VolumeSampleMethod direct_sample_method;
#  endif

  /* Throughput and offset for indirect light scattering. */
  bool indirect_scatter;
  Spectrum indirect_throughput;
  float indirect_t;
  ShaderVolumePhases indirect_phases;
} VolumeIntegrateResult;

/* Ignore paths that have volume throughput below this value, to avoid unnecessary work
 * and precision issues.
 * todo: this value could be tweaked or turned into a probability to avoid unnecessary
 * work in volumes and subsurface scattering. */
#  define VOLUME_THROUGHPUT_EPSILON 1e-6f

/* Volume shader properties
 *
 * extinction coefficient = absorption coefficient + scattering coefficient
 * sigma_t = sigma_a + sigma_s */

typedef struct VolumeShaderCoefficients {
  Spectrum sigma_t;
  Spectrum sigma_s;
  Spectrum emission;
} VolumeShaderCoefficients;

/* Evaluate shader to get extinction coefficient at P. */
ccl_device_inline bool shadow_volume_shader_sample(KernelGlobals kg,
                                                   IntegratorShadowState state,
                                                   ccl_private ShaderData *ccl_restrict sd,
                                                   ccl_private Spectrum *ccl_restrict extinction)
{
  VOLUME_READ_LAMBDA(integrator_state_read_shadow_volume_stack(state, i))
  volume_shader_eval<true>(kg, state, sd, PATH_RAY_SHADOW, volume_read_lambda_pass);

  if (!(sd->flag & SD_EXTINCTION)) {
    return false;
  }

  const float density = object_volume_density(kg, sd->object);
  *extinction = sd->closure_transparent_extinction * density;
  return true;
}

/* Evaluate shader to get absorption, scattering and emission at P. */
ccl_device_inline bool volume_shader_sample(KernelGlobals kg,
                                            IntegratorState state,
                                            ccl_private ShaderData *ccl_restrict sd,
                                            ccl_private VolumeShaderCoefficients *coeff)
{
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  VOLUME_READ_LAMBDA(integrator_state_read_volume_stack(state, i))
  volume_shader_eval<false>(kg, state, sd, path_flag, volume_read_lambda_pass);

  if (!(sd->flag & (SD_EXTINCTION | SD_SCATTER | SD_EMISSION))) {
    return false;
  }

  coeff->sigma_s = zero_spectrum();
  coeff->sigma_t = (sd->flag & SD_EXTINCTION) ? sd->closure_transparent_extinction :
                                                zero_spectrum();
  coeff->emission = (sd->flag & SD_EMISSION) ? sd->closure_emission_background : zero_spectrum();

  if (sd->flag & SD_SCATTER) {
    for (int i = 0; i < sd->num_closure; i++) {
      ccl_private const ShaderClosure *sc = &sd->closure[i];

      if (CLOSURE_IS_VOLUME(sc->type)) {
        coeff->sigma_s += sc->weight;
      }
    }
  }

  const float density = object_volume_density(kg, sd->object);
  coeff->sigma_s *= density;
  coeff->sigma_t *= density;
  coeff->emission *= density;

  return true;
}

ccl_device_forceinline void volume_step_init(KernelGlobals kg,
                                             ccl_private const RNGState *rng_state,
                                             const float object_step_size,
                                             const float tmin,
                                             const float tmax,
                                             ccl_private float *step_size,
                                             ccl_private float *step_shade_offset,
                                             ccl_private float *steps_offset,
                                             ccl_private int *max_steps)
{
  if (object_step_size == FLT_MAX) {
    /* Homogeneous volume. */
    *step_size = tmax - tmin;
    *step_shade_offset = 0.0f;
    *steps_offset = 1.0f;
    *max_steps = 1;
  }
  else {
    /* Heterogeneous volume. */
    *max_steps = kernel_data.integrator.volume_max_steps;
    const float t = tmax - tmin;
    float step = min(object_step_size, t);

    /* compute exact steps in advance for malloc */
    if (t > *max_steps * step) {
      step = t / (float)*max_steps;
    }

    *step_size = step;

    /* Perform shading at this offset within a step, to integrate over
     * over the entire step segment. */
    *step_shade_offset = path_state_rng_1D(kg, rng_state, PRNG_VOLUME_SHADE_OFFSET);

    /* Shift starting point of all segment by this random amount to avoid
     * banding artifacts from the volume bounding shape. */
    *steps_offset = path_state_rng_1D(kg, rng_state, PRNG_VOLUME_OFFSET);
  }
}

/* Volume Shadows
 *
 * These functions are used to attenuate shadow rays to lights. Both absorption
 * and scattering will block light, represented by the extinction coefficient. */

#  if 0
/* homogeneous volume: assume shader evaluation at the starts gives
 * the extinction coefficient for the entire line segment */
ccl_device void volume_shadow_homogeneous(KernelGlobals kg, IntegratorState state,
                                          ccl_private Ray *ccl_restrict ray,
                                          ccl_private ShaderData *ccl_restrict sd,
                                          ccl_global Spectrum *ccl_restrict throughput)
{
  Spectrum sigma_t = zero_spectrum();

  if (shadow_volume_shader_sample(kg, state, sd, &sigma_t)) {
    *throughput *= volume_color_transmittance(sigma_t, ray->tmax - ray->tmin);
  }
}
#  endif

/* heterogeneous volume: integrate stepping through the volume until we
 * reach the end, get absorbed entirely, or run out of iterations */
ccl_device void volume_shadow_heterogeneous(KernelGlobals kg,
                                            IntegratorShadowState state,
                                            ccl_private Ray *ccl_restrict ray,
                                            ccl_private ShaderData *ccl_restrict sd,
                                            ccl_private Spectrum *ccl_restrict throughput,
                                            const float object_step_size)
{
  /* Load random number state. */
  RNGState rng_state;
  shadow_path_state_rng_load(state, &rng_state);

  Spectrum tp = *throughput;

  /* Prepare for stepping.
   * For shadows we do not offset all segments, since the starting point is
   * already a random distance inside the volume. It also appears to create
   * banding artifacts for unknown reasons. */
  int max_steps;
  float step_size, step_shade_offset, unused;
  volume_step_init(kg,
                   &rng_state,
                   object_step_size,
                   ray->tmin,
                   ray->tmax,
                   &step_size,
                   &step_shade_offset,
                   &unused,
                   &max_steps);
  const float steps_offset = 1.0f;

  /* compute extinction at the start */
  float t = ray->tmin;

  Spectrum sum = zero_spectrum();

  for (int i = 0; i < max_steps; i++) {
    /* advance to new position */
    float new_t = min(ray->tmax, ray->tmin + (i + steps_offset) * step_size);
    float dt = new_t - t;

    float3 new_P = ray->P + ray->D * (t + dt * step_shade_offset);
    Spectrum sigma_t = zero_spectrum();

    /* compute attenuation over segment */
    sd->P = new_P;
    if (shadow_volume_shader_sample(kg, state, sd, &sigma_t)) {
      /* Compute `expf()` only for every Nth step, to save some calculations
       * because `exp(a)*exp(b) = exp(a+b)`, also do a quick #VOLUME_THROUGHPUT_EPSILON
       * check then. */
      sum += (-sigma_t * dt);
      if ((i & 0x07) == 0) { /* TODO: Other interval? */
        tp = *throughput * exp(sum);

        /* stop if nearly all light is blocked */
        if (reduce_max(tp) < VOLUME_THROUGHPUT_EPSILON)
          break;
      }
    }

    /* stop if at the end of the volume */
    t = new_t;
    if (t == ray->tmax) {
      /* Update throughput in case we haven't done it above */
      tp = *throughput * exp(sum);
      break;
    }
  }

  *throughput = tp;
}

/* Equi-angular sampling as in:
 * "Importance Sampling Techniques for Path Tracing in Participating Media" */

/* Below this pdf we ignore samples, as they tend to lead to very long distances.
 * This can cause performance issues with BVH traversal in OptiX, leading it to
 * traverse many nodes. Since these contribute very little to the image, just ignore
 * those samples. */
#  define VOLUME_SAMPLE_PDF_CUTOFF 1e-8f

ccl_device float volume_equiangular_sample(ccl_private const Ray *ccl_restrict ray,
                                           const float3 light_P,
                                           const float xi,
                                           ccl_private float *pdf)
{
  const float tmin = ray->tmin;
  const float tmax = ray->tmax;
  const float delta = dot((light_P - ray->P), ray->D);
  const float D = safe_sqrtf(len_squared(light_P - ray->P) - delta * delta);
  if (UNLIKELY(D == 0.0f)) {
    *pdf = 0.0f;
    return 0.0f;
  }
  const float theta_a = atan2f(tmin - delta, D);
  const float theta_b = atan2f(tmax - delta, D);
  const float t_ = D * tanf((xi * theta_b) + (1 - xi) * theta_a);
  if (UNLIKELY(theta_b == theta_a)) {
    *pdf = 0.0f;
    return 0.0f;
  }
  *pdf = D / ((theta_b - theta_a) * (D * D + t_ * t_));

  return clamp(delta + t_, tmin, tmax); /* clamp is only for float precision errors */
}

ccl_device float volume_equiangular_pdf(ccl_private const Ray *ccl_restrict ray,
                                        const float3 light_P,
                                        const float sample_t)
{
  const float delta = dot((light_P - ray->P), ray->D);
  const float D = safe_sqrtf(len_squared(light_P - ray->P) - delta * delta);
  if (UNLIKELY(D == 0.0f)) {
    return 0.0f;
  }

  const float tmin = ray->tmin;
  const float tmax = ray->tmax;
  const float t_ = sample_t - delta;

  const float theta_a = atan2f(tmin - delta, D);
  const float theta_b = atan2f(tmax - delta, D);
  if (UNLIKELY(theta_b == theta_a)) {
    return 0.0f;
  }

  const float pdf = D / ((theta_b - theta_a) * (D * D + t_ * t_));

  return pdf;
}

ccl_device float volume_equiangular_cdf(ccl_private const Ray *ccl_restrict ray,
                                        const float3 light_P,
                                        const float sample_t)
{
  float delta = dot((light_P - ray->P), ray->D);
  float D = safe_sqrtf(len_squared(light_P - ray->P) - delta * delta);
  if (UNLIKELY(D == 0.0f)) {
    return 0.0f;
  }

  const float tmin = ray->tmin;
  const float tmax = ray->tmax;
  const float t_ = sample_t - delta;

  const float theta_a = atan2f(tmin - delta, D);
  const float theta_b = atan2f(tmax - delta, D);
  if (UNLIKELY(theta_b == theta_a)) {
    return 0.0f;
  }

  const float theta_sample = atan2f(t_, D);
  const float cdf = (theta_sample - theta_a) / (theta_b - theta_a);

  return cdf;
}

/* Distance sampling */

ccl_device float volume_distance_sample(float max_t,
                                        Spectrum sigma_t,
                                        int channel,
                                        float xi,
                                        ccl_private Spectrum *transmittance,
                                        ccl_private Spectrum *pdf)
{
  /* xi is [0, 1[ so log(0) should never happen, division by zero is
   * avoided because sample_sigma_t > 0 when SD_SCATTER is set */
  float sample_sigma_t = volume_channel_get(sigma_t, channel);
  Spectrum full_transmittance = volume_color_transmittance(sigma_t, max_t);
  float sample_transmittance = volume_channel_get(full_transmittance, channel);

  float sample_t = min(max_t, -logf(1.0f - xi * (1.0f - sample_transmittance)) / sample_sigma_t);

  *transmittance = volume_color_transmittance(sigma_t, sample_t);
  *pdf = safe_divide_color(sigma_t * *transmittance, one_spectrum() - full_transmittance);

  /* todo: optimization: when taken together with hit/miss decision,
   * the full_transmittance cancels out drops out and xi does not
   * need to be remapped */

  return sample_t;
}

ccl_device Spectrum volume_distance_pdf(float max_t, Spectrum sigma_t, float sample_t)
{
  Spectrum full_transmittance = volume_color_transmittance(sigma_t, max_t);
  Spectrum transmittance = volume_color_transmittance(sigma_t, sample_t);

  return safe_divide_color(sigma_t * transmittance, one_spectrum() - full_transmittance);
}

/* Emission */

ccl_device Spectrum volume_emission_integrate(ccl_private VolumeShaderCoefficients *coeff,
                                              int closure_flag,
                                              Spectrum transmittance,
                                              float t)
{
  /* integral E * exp(-sigma_t * t) from 0 to t = E * (1 - exp(-sigma_t * t))/sigma_t
   * this goes to E * t as sigma_t goes to zero
   *
   * todo: we should use an epsilon to avoid precision issues near zero sigma_t */
  Spectrum emission = coeff->emission;

  if (closure_flag & SD_EXTINCTION) {
    Spectrum sigma_t = coeff->sigma_t;

    FOREACH_SPECTRUM_CHANNEL (i) {
      GET_SPECTRUM_CHANNEL(emission, i) *= (GET_SPECTRUM_CHANNEL(sigma_t, i) > 0.0f) ?
                                               (1.0f - GET_SPECTRUM_CHANNEL(transmittance, i)) /
                                                   GET_SPECTRUM_CHANNEL(sigma_t, i) :
                                               t;
    }
  }
  else
    emission *= t;

  return emission;
}

/* Volume Integration */

typedef struct VolumeIntegrateState {
  /* Volume segment extents. */
  float tmin;
  float tmax;

  /* If volume is absorption-only up to this point, and no probabilistic
   * scattering or termination has been used yet. */
  bool absorption_only;

  /* Random numbers for scattering. */
  float rscatter;
  float rphase;

  /* Multiple importance sampling. */
  VolumeSampleMethod direct_sample_method;
  bool use_mis;
  float distance_pdf;
  float equiangular_pdf;
} VolumeIntegrateState;

ccl_device_forceinline void volume_integrate_step_scattering(
    ccl_private const ShaderData *sd,
    ccl_private const Ray *ray,
    const float3 equiangular_light_P,
    ccl_private const VolumeShaderCoefficients &ccl_restrict coeff,
    const Spectrum transmittance,
    ccl_private VolumeIntegrateState &ccl_restrict vstate,
    ccl_private VolumeIntegrateResult &ccl_restrict result)
{
  /* Pick random color channel, we use the Veach one-sample
   * model with balance heuristic for the channels. */
  const Spectrum albedo = safe_divide_color(coeff.sigma_s, coeff.sigma_t);
  Spectrum channel_pdf;
  const int channel = volume_sample_channel(
      albedo, result.indirect_throughput, vstate.rphase, &channel_pdf);

  /* Equiangular sampling for direct lighting. */
  if (vstate.direct_sample_method == VOLUME_SAMPLE_EQUIANGULAR && !result.direct_scatter) {
    if (result.direct_t >= vstate.tmin && result.direct_t <= vstate.tmax &&
        vstate.equiangular_pdf > VOLUME_SAMPLE_PDF_CUTOFF) {
      const float new_dt = result.direct_t - vstate.tmin;
      const Spectrum new_transmittance = volume_color_transmittance(coeff.sigma_t, new_dt);

      result.direct_scatter = true;
      result.direct_throughput *= coeff.sigma_s * new_transmittance / vstate.equiangular_pdf;
      volume_shader_copy_phases(&result.direct_phases, sd);

      /* Multiple importance sampling. */
      if (vstate.use_mis) {
        const float distance_pdf = vstate.distance_pdf *
                                   dot(channel_pdf, coeff.sigma_t * new_transmittance);
        const float mis_weight = 2.0f * power_heuristic(vstate.equiangular_pdf, distance_pdf);
        result.direct_throughput *= mis_weight;
      }
    }
    else {
      result.direct_throughput *= transmittance;
      vstate.distance_pdf *= dot(channel_pdf, transmittance);
    }
  }

  /* Distance sampling for indirect and optional direct lighting. */
  if (!result.indirect_scatter) {
    /* decide if we will scatter or continue */
    const float sample_transmittance = volume_channel_get(transmittance, channel);

    if (1.0f - vstate.rscatter >= sample_transmittance) {
      /* compute sampling distance */
      const float sample_sigma_t = volume_channel_get(coeff.sigma_t, channel);
      const float new_dt = -logf(1.0f - vstate.rscatter) / sample_sigma_t;
      const float new_t = vstate.tmin + new_dt;

      /* transmittance and pdf */
      const Spectrum new_transmittance = volume_color_transmittance(coeff.sigma_t, new_dt);
      const float distance_pdf = dot(channel_pdf, coeff.sigma_t * new_transmittance);

      if (vstate.distance_pdf * distance_pdf > VOLUME_SAMPLE_PDF_CUTOFF) {
        /* throughput */
        result.indirect_scatter = true;
        result.indirect_t = new_t;
        result.indirect_throughput *= coeff.sigma_s * new_transmittance / distance_pdf;
        volume_shader_copy_phases(&result.indirect_phases, sd);

        if (vstate.direct_sample_method != VOLUME_SAMPLE_EQUIANGULAR) {
          /* If using distance sampling for direct light, just copy parameters
           * of indirect light since we scatter at the same point then. */
          result.direct_scatter = true;
          result.direct_t = result.indirect_t;
          result.direct_throughput = result.indirect_throughput;
          volume_shader_copy_phases(&result.direct_phases, sd);

          /* Multiple importance sampling. */
          if (vstate.use_mis) {
            const float equiangular_pdf = volume_equiangular_pdf(ray, equiangular_light_P, new_t);
            const float mis_weight = power_heuristic(vstate.distance_pdf * distance_pdf,
                                                     equiangular_pdf);
            result.direct_throughput *= 2.0f * mis_weight;
          }
        }
      }
    }
    else {
      /* throughput */
      const float pdf = dot(channel_pdf, transmittance);
      result.indirect_throughput *= transmittance / pdf;
      if (vstate.direct_sample_method != VOLUME_SAMPLE_EQUIANGULAR) {
        vstate.distance_pdf *= pdf;
      }

      /* remap rscatter so we can reuse it and keep thing stratified */
      vstate.rscatter = 1.0f - (1.0f - vstate.rscatter) / sample_transmittance;
    }
  }
}

/* heterogeneous volume distance sampling: integrate stepping through the
 * volume until we reach the end, get absorbed entirely, or run out of
 * iterations. this does probabilistically scatter or get transmitted through
 * for path tracing where we don't want to branch. */
ccl_device_forceinline void volume_integrate_heterogeneous(
    KernelGlobals kg,
    IntegratorState state,
    ccl_private Ray *ccl_restrict ray,
    ccl_private ShaderData *ccl_restrict sd,
    ccl_private const RNGState *rng_state,
    ccl_global float *ccl_restrict render_buffer,
    const float object_step_size,
    const VolumeSampleMethod direct_sample_method,
    const float3 equiangular_light_P,
    ccl_private VolumeIntegrateResult &result)
{
  PROFILING_INIT(kg, PROFILING_SHADE_VOLUME_INTEGRATE);

  /* Prepare for stepping.
   * Using a different step offset for the first step avoids banding artifacts. */
  int max_steps;
  float step_size, step_shade_offset, steps_offset;
  volume_step_init(kg,
                   rng_state,
                   object_step_size,
                   ray->tmin,
                   ray->tmax,
                   &step_size,
                   &step_shade_offset,
                   &steps_offset,
                   &max_steps);

  /* Initialize volume integration state. */
  VolumeIntegrateState vstate ccl_optional_struct_init;
  vstate.tmin = ray->tmin;
  vstate.tmax = ray->tmin;
  vstate.absorption_only = true;
  vstate.rscatter = path_state_rng_1D(kg, rng_state, PRNG_VOLUME_SCATTER_DISTANCE);
  vstate.rphase = path_state_rng_1D(kg, rng_state, PRNG_VOLUME_PHASE_CHANNEL);

  /* Multiple importance sampling: pick between equiangular and distance sampling strategy. */
  vstate.direct_sample_method = direct_sample_method;
  vstate.use_mis = (direct_sample_method == VOLUME_SAMPLE_MIS);
  if (vstate.use_mis) {
    if (vstate.rscatter < 0.5f) {
      vstate.rscatter *= 2.0f;
      vstate.direct_sample_method = VOLUME_SAMPLE_DISTANCE;
    }
    else {
      vstate.rscatter = (vstate.rscatter - 0.5f) * 2.0f;
      vstate.direct_sample_method = VOLUME_SAMPLE_EQUIANGULAR;
    }
  }
  vstate.equiangular_pdf = 0.0f;
  vstate.distance_pdf = 1.0f;

  /* Initialize volume integration result. */
  const Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
  result.direct_throughput = throughput;
  result.indirect_throughput = throughput;

  /* Equiangular sampling: compute distance and PDF in advance. */
  if (vstate.direct_sample_method == VOLUME_SAMPLE_EQUIANGULAR) {
    result.direct_t = volume_equiangular_sample(
        ray, equiangular_light_P, vstate.rscatter, &vstate.equiangular_pdf);
  }
#  ifdef __PATH_GUIDING__
  result.direct_sample_method = vstate.direct_sample_method;
#  endif

#  ifdef __DENOISING_FEATURES__
  const bool write_denoising_features = (INTEGRATOR_STATE(state, path, flag) &
                                         PATH_RAY_DENOISING_FEATURES);
  Spectrum accum_albedo = zero_spectrum();
#  endif
  Spectrum accum_emission = zero_spectrum();

  for (int i = 0; i < max_steps; i++) {
    /* Advance to new position */
    vstate.tmax = min(ray->tmax, ray->tmin + (i + steps_offset) * step_size);
    const float shade_t = vstate.tmin + (vstate.tmax - vstate.tmin) * step_shade_offset;
    sd->P = ray->P + ray->D * shade_t;

    /* compute segment */
    VolumeShaderCoefficients coeff ccl_optional_struct_init;
    if (volume_shader_sample(kg, state, sd, &coeff)) {
      const int closure_flag = sd->flag;

      /* Evaluate transmittance over segment. */
      const float dt = (vstate.tmax - vstate.tmin);
      const Spectrum transmittance = (closure_flag & SD_EXTINCTION) ?
                                         volume_color_transmittance(coeff.sigma_t, dt) :
                                         one_spectrum();

      /* Emission. */
      if (closure_flag & SD_EMISSION) {
        /* Only write emission before indirect light scatter position, since we terminate
         * stepping at that point if we have already found a direct light scatter position. */
        if (!result.indirect_scatter) {
          const Spectrum emission = volume_emission_integrate(
              &coeff, closure_flag, transmittance, dt);
          accum_emission += result.indirect_throughput * emission;
          guiding_record_volume_emission(kg, state, emission);
        }
      }

      if (closure_flag & SD_EXTINCTION) {
        if ((closure_flag & SD_SCATTER) || !vstate.absorption_only) {
#  ifdef __DENOISING_FEATURES__
          /* Accumulate albedo for denoising features. */
          if (write_denoising_features && (closure_flag & SD_SCATTER)) {
            const Spectrum albedo = safe_divide_color(coeff.sigma_s, coeff.sigma_t);
            accum_albedo += result.indirect_throughput * albedo * (one_spectrum() - transmittance);
          }
#  endif

          /* Scattering and absorption. */
          volume_integrate_step_scattering(
              sd, ray, equiangular_light_P, coeff, transmittance, vstate, result);
        }
        else {
          /* Absorption only. */
          result.indirect_throughput *= transmittance;
          result.direct_throughput *= transmittance;
        }

        /* Stop if nearly all light blocked. */
        if (!result.indirect_scatter) {
          if (reduce_max(result.indirect_throughput) < VOLUME_THROUGHPUT_EPSILON) {
            result.indirect_throughput = zero_spectrum();
            break;
          }
        }
        else if (!result.direct_scatter) {
          if (reduce_max(result.direct_throughput) < VOLUME_THROUGHPUT_EPSILON) {
            break;
          }
        }
      }

      /* If we have scattering data for both direct and indirect, we're done. */
      if (result.direct_scatter && result.indirect_scatter) {
        break;
      }
    }

    /* Stop if at the end of the volume. */
    vstate.tmin = vstate.tmax;
    if (vstate.tmin == ray->tmax) {
      break;
    }
  }

  /* Write accumulated emission. */
  if (!is_zero(accum_emission)) {
    film_write_volume_emission(
        kg, state, accum_emission, render_buffer, object_lightgroup(kg, sd->object));
  }

#  ifdef __DENOISING_FEATURES__
  /* Write denoising features. */
  if (write_denoising_features) {
    film_write_denoising_features_volume(
        kg, state, accum_albedo, result.indirect_scatter, render_buffer);
  }
#  endif /* __DENOISING_FEATURES__ */
}

/* Path tracing: sample point on light for equiangular sampling. */
ccl_device_forceinline bool integrate_volume_equiangular_sample_light(
    KernelGlobals kg,
    IntegratorState state,
    ccl_private const Ray *ccl_restrict ray,
    ccl_private const ShaderData *ccl_restrict sd,
    ccl_private const RNGState *ccl_restrict rng_state,
    ccl_private float3 *ccl_restrict P)
{
  /* Test if there is a light or BSDF that needs direct light. */
  if (!kernel_data.integrator.use_direct_light) {
    return false;
  }

  /* Sample position on a light. */
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const uint bounce = INTEGRATOR_STATE(state, path, bounce);
  const float3 rand_light = path_state_rng_3D(kg, rng_state, PRNG_LIGHT);

  LightSample ls ccl_optional_struct_init;
  if (!light_sample_from_volume_segment(kg,
                                        rand_light.z,
                                        rand_light.x,
                                        rand_light.y,
                                        sd->time,
                                        sd->P,
                                        ray->D,
                                        ray->tmax - ray->tmin,
                                        bounce,
                                        path_flag,
                                        &ls)) {
    return false;
  }

  if (ls.shader & SHADER_EXCLUDE_SCATTER) {
    return false;
  }

  if (ls.t == FLT_MAX) {
    return false;
  }

  *P = ls.P;

  return true;
}

/* Path tracing: sample point on light and evaluate light shader, then
 * queue shadow ray to be traced. */
ccl_device_forceinline void integrate_volume_direct_light(
    KernelGlobals kg,
    IntegratorState state,
    ccl_private const ShaderData *ccl_restrict sd,
    ccl_private const RNGState *ccl_restrict rng_state,
    const float3 P,
    ccl_private const ShaderVolumePhases *ccl_restrict phases,
#  ifdef __PATH_GUIDING__
    ccl_private const Spectrum unlit_throughput,
#  endif
    ccl_private const Spectrum throughput)
{
  PROFILING_INIT(kg, PROFILING_SHADE_VOLUME_DIRECT_LIGHT);

  if (!kernel_data.integrator.use_direct_light) {
    return;
  }

  /* Sample position on the same light again, now from the shading point where we scattered.
   *
   * Note that this means we sample the light tree twice when equiangular sampling is used.
   * We could consider sampling the light tree just once and use the same light position again.
   *
   * This would make the PDFs for MIS weights more complicated due to having to account for
   * both distance/equiangular and direct/indirect light sampling, but could be more accurate.
   * Additionally we could end up behind the light or outside a spot light cone, which might
   * waste a sample. Though on the other hand it would be possible to prevent that with
   * equiangular sampling restricted to a smaller sub-segment where the light has influence. */
  LightSample ls ccl_optional_struct_init;
  {
    const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
    const uint bounce = INTEGRATOR_STATE(state, path, bounce);
    const float3 rand_light = path_state_rng_3D(kg, rng_state, PRNG_LIGHT);

    if (!light_sample_from_position(kg,
                                    rng_state,
                                    rand_light.z,
                                    rand_light.x,
                                    rand_light.y,
                                    sd->time,
                                    P,
                                    zero_float3(),
                                    SD_BSDF_HAS_TRANSMISSION,
                                    bounce,
                                    path_flag,
                                    &ls)) {
      return;
    }
  }

  if (ls.shader & SHADER_EXCLUDE_SCATTER) {
    return;
  }

  /* Evaluate light shader.
   *
   * TODO: can we reuse sd memory? In theory we can move this after
   * integrate_surface_bounce, evaluate the BSDF, and only then evaluate
   * the light shader. This could also move to its own kernel, for
   * non-constant light sources. */
  ShaderDataTinyStorage emission_sd_storage;
  ccl_private ShaderData *emission_sd = AS_SHADER_DATA(&emission_sd_storage);
  const Spectrum light_eval = light_sample_shader_eval(kg, state, emission_sd, &ls, sd->time);
  if (is_zero(light_eval)) {
    return;
  }

  /* Evaluate BSDF. */
  BsdfEval phase_eval ccl_optional_struct_init;
  float phase_pdf = volume_shader_phase_eval(kg, state, sd, phases, ls.D, &phase_eval);

  if (ls.shader & SHADER_USE_MIS) {
    float mis_weight = light_sample_mis_weight_nee(kg, ls.pdf, phase_pdf);
    bsdf_eval_mul(&phase_eval, mis_weight);
  }

  bsdf_eval_mul(&phase_eval, light_eval / ls.pdf);

  /* Path termination. */
  const float terminate = path_state_rng_light_termination(kg, rng_state);
  if (light_sample_terminate(kg, &ls, &phase_eval, terminate)) {
    return;
  }

  /* Create shadow ray. */
  Ray ray ccl_optional_struct_init;
  light_sample_to_volume_shadow_ray(kg, sd, &ls, P, &ray);
  const bool is_light = light_sample_is_light(&ls);

  /* Branch off shadow kernel. */
  IntegratorShadowState shadow_state = integrator_shadow_path_init(
      kg, state, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW, false);

  /* Write shadow ray and associated state to global memory. */
  integrator_state_write_shadow_ray(kg, shadow_state, &ray);
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_state, shadow_isect, 0, object) = ray.self.object;
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_state, shadow_isect, 0, prim) = ray.self.prim;
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_state, shadow_isect, 1, object) = ray.self.light_object;
  INTEGRATOR_STATE_ARRAY_WRITE(shadow_state, shadow_isect, 1, prim) = ray.self.light_prim;

  /* Copy state from main path to shadow path. */
  const uint16_t bounce = INTEGRATOR_STATE(state, path, bounce);
  const uint16_t transparent_bounce = INTEGRATOR_STATE(state, path, transparent_bounce);
  uint32_t shadow_flag = INTEGRATOR_STATE(state, path, flag);
  shadow_flag |= (is_light) ? PATH_RAY_SHADOW_FOR_LIGHT : 0;
  const Spectrum throughput_phase = throughput * bsdf_eval_sum(&phase_eval);

  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_PASSES) {
    PackedSpectrum pass_diffuse_weight;
    PackedSpectrum pass_glossy_weight;

    if (shadow_flag & PATH_RAY_ANY_PASS) {
      /* Indirect bounce, use weights from earlier surface or volume bounce. */
      pass_diffuse_weight = INTEGRATOR_STATE(state, path, pass_diffuse_weight);
      pass_glossy_weight = INTEGRATOR_STATE(state, path, pass_glossy_weight);
    }
    else {
      /* Direct light, no diffuse/glossy distinction needed for volumes. */
      shadow_flag |= PATH_RAY_VOLUME_PASS;
      pass_diffuse_weight = one_spectrum();
      pass_glossy_weight = zero_spectrum();
    }

    INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, pass_diffuse_weight) = pass_diffuse_weight;
    INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, pass_glossy_weight) = pass_glossy_weight;
  }

  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, render_pixel_index) = INTEGRATOR_STATE(
      state, path, render_pixel_index);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, rng_offset) = INTEGRATOR_STATE(
      state, path, rng_offset);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, rng_hash) = INTEGRATOR_STATE(
      state, path, rng_hash);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, sample) = INTEGRATOR_STATE(
      state, path, sample);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, flag) = shadow_flag;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, bounce) = bounce;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, transparent_bounce) = transparent_bounce;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, diffuse_bounce) = INTEGRATOR_STATE(
      state, path, diffuse_bounce);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, glossy_bounce) = INTEGRATOR_STATE(
      state, path, glossy_bounce);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, transmission_bounce) = INTEGRATOR_STATE(
      state, path, transmission_bounce);
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, throughput) = throughput_phase;

  /* Write Lightgroup, +1 as lightgroup is int but we need to encode into a uint8_t. */
  INTEGRATOR_STATE_WRITE(
      shadow_state, shadow_path, lightgroup) = (ls.type != LIGHT_BACKGROUND) ?
                                                   ls.group + 1 :
                                                   kernel_data.background.lightgroup + 1;

#  ifdef __PATH_GUIDING__
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, unlit_throughput) = unlit_throughput;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, path_segment) = INTEGRATOR_STATE(
      state, guiding, path_segment);
#  endif

  integrator_state_copy_volume_stack_to_shadow(kg, shadow_state, state);
}

/* Path tracing: scatter in new direction using phase function */
ccl_device_forceinline bool integrate_volume_phase_scatter(
    KernelGlobals kg,
    IntegratorState state,
    ccl_private ShaderData *sd,
    ccl_private const RNGState *rng_state,
    ccl_private const ShaderVolumePhases *phases)
{
  PROFILING_INIT(kg, PROFILING_SHADE_VOLUME_INDIRECT_LIGHT);

  float2 rand_phase = path_state_rng_2D(kg, rng_state, PRNG_VOLUME_PHASE);

  ccl_private const ShaderVolumeClosure *svc = volume_shader_phase_pick(phases, &rand_phase);

  /* Phase closure, sample direction. */
  float phase_pdf = 0.0f, unguided_phase_pdf = 0.0f;
  BsdfEval phase_eval ccl_optional_struct_init;
  float3 phase_omega_in ccl_optional_struct_init;
  float sampled_roughness = 1.0f;
  int label;

#  if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  if (kernel_data.integrator.use_guiding) {
    label = volume_shader_phase_guided_sample(kg,
                                              state,
                                              sd,
                                              svc,
                                              rand_phase,
                                              &phase_eval,
                                              &phase_omega_in,
                                              &phase_pdf,
                                              &unguided_phase_pdf,
                                              &sampled_roughness);

    if (phase_pdf == 0.0f || bsdf_eval_is_zero(&phase_eval)) {
      return false;
    }

    INTEGRATOR_STATE_WRITE(state, path, unguided_throughput) *= phase_pdf / unguided_phase_pdf;
  }
  else
#  endif
  {
    label = volume_shader_phase_sample(kg,
                                       sd,
                                       phases,
                                       svc,
                                       rand_phase,
                                       &phase_eval,
                                       &phase_omega_in,
                                       &phase_pdf,
                                       &sampled_roughness);

    if (phase_pdf == 0.0f || bsdf_eval_is_zero(&phase_eval)) {
      return false;
    }

    unguided_phase_pdf = phase_pdf;
  }

  /* Setup ray. */
  INTEGRATOR_STATE_WRITE(state, ray, P) = sd->P;
  INTEGRATOR_STATE_WRITE(state, ray, D) = normalize(phase_omega_in);
  INTEGRATOR_STATE_WRITE(state, ray, tmin) = 0.0f;
  INTEGRATOR_STATE_WRITE(state, ray, tmax) = FLT_MAX;
#  ifdef __RAY_DIFFERENTIALS__
  INTEGRATOR_STATE_WRITE(state, ray, dP) = differential_make_compact(sd->dP);
#  endif
  // Save memory by storing last hit prim and object in isect
  INTEGRATOR_STATE_WRITE(state, isect, prim) = sd->prim;
  INTEGRATOR_STATE_WRITE(state, isect, object) = sd->object;

  const Spectrum phase_weight = bsdf_eval_sum(&phase_eval) / phase_pdf;

  /* Add phase function sampling data to the path segment. */
  guiding_record_volume_bounce(
      kg, state, sd, phase_weight, phase_pdf, normalize(phase_omega_in), sampled_roughness);

  /* Update throughput. */
  const Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
  const Spectrum throughput_phase = throughput * phase_weight;
  INTEGRATOR_STATE_WRITE(state, path, throughput) = throughput_phase;

  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_PASSES) {
    if (INTEGRATOR_STATE(state, path, bounce) == 0) {
      INTEGRATOR_STATE_WRITE(state, path, pass_diffuse_weight) = one_spectrum();
      INTEGRATOR_STATE_WRITE(state, path, pass_glossy_weight) = zero_spectrum();
    }
  }

  /* Update path state */
  INTEGRATOR_STATE_WRITE(state, path, mis_ray_pdf) = phase_pdf;
  INTEGRATOR_STATE_WRITE(state, path, mis_origin_n) = zero_float3();
  INTEGRATOR_STATE_WRITE(state, path, min_ray_pdf) = fminf(
      unguided_phase_pdf, INTEGRATOR_STATE(state, path, min_ray_pdf));

  path_state_next(kg, state, label, sd->flag);
  return true;
}

/* get the volume attenuation and emission over line segment defined by
 * ray, with the assumption that there are no surfaces blocking light
 * between the endpoints. distance sampling is used to decide if we will
 * scatter or not. */
ccl_device VolumeIntegrateEvent volume_integrate(KernelGlobals kg,
                                                 IntegratorState state,
                                                 ccl_private Ray *ccl_restrict ray,
                                                 ccl_global float *ccl_restrict render_buffer)
{
  ShaderData sd;
  shader_setup_from_volume(kg, &sd, ray);

  /* Load random number state. */
  RNGState rng_state;
  path_state_rng_load(state, &rng_state);

  /* Sample light ahead of volume stepping, for equiangular sampling. */
  /* TODO: distant lights are ignored now, but could instead use even distribution. */
  const bool need_light_sample = !(INTEGRATOR_STATE(state, path, flag) & PATH_RAY_TERMINATE);
  float3 equiangular_P = zero_float3();
  const bool have_equiangular_sample = need_light_sample &&
                                       integrate_volume_equiangular_sample_light(
                                           kg, state, ray, &sd, &rng_state, &equiangular_P);

  VolumeSampleMethod direct_sample_method = (have_equiangular_sample) ?
                                                volume_stack_sample_method(kg, state) :
                                                VOLUME_SAMPLE_DISTANCE;

  /* Step through volume. */
  VOLUME_READ_LAMBDA(integrator_state_read_volume_stack(state, i))
  const float step_size = volume_stack_step_size(kg, volume_read_lambda_pass);

#  if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  /* The current path throughput which is used later to calculate per-segment throughput.*/
  const float3 initial_throughput = INTEGRATOR_STATE(state, path, throughput);
  /* The path throughput used to calculate the throughput for direct light. */
  float3 unlit_throughput = initial_throughput;
  /* If a new path segment is generated at the direct scatter position. */
  bool guiding_generated_new_segment = false;
  float rand_phase_guiding = 0.5f;
#  endif

  /* TODO: expensive to zero closures? */
  VolumeIntegrateResult result = {};
  volume_integrate_heterogeneous(kg,
                                 state,
                                 ray,
                                 &sd,
                                 &rng_state,
                                 render_buffer,
                                 step_size,
                                 direct_sample_method,
                                 equiangular_P,
                                 result);

  /* Perform path termination. The intersect_closest will have already marked this path
   * to be terminated. That will shading evaluating to leave out any scattering closures,
   * but emission and absorption are still handled for multiple importance sampling. */
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const float continuation_probability = (path_flag & PATH_RAY_TERMINATE_IN_NEXT_VOLUME) ?
                                             0.0f :
                                             INTEGRATOR_STATE(
                                                 state, path, continuation_probability);
  if (continuation_probability == 0.0f) {
    return VOLUME_PATH_MISSED;
  }

  /* Direct light. */
  if (result.direct_scatter) {
    const float3 direct_P = ray->P + result.direct_t * ray->D;

#  ifdef __PATH_GUIDING__
    if (kernel_data.integrator.use_guiding) {
#    if PATH_GUIDING_LEVEL >= 1
      if (result.direct_sample_method == VOLUME_SAMPLE_DISTANCE) {
        /* If the direct scatter event is generated using VOLUME_SAMPLE_DISTANCE the direct event
         * will happen at the same position as the indirect event and the direct light contribution
         * will contribute to the position of the next path segment.*/
        float3 transmittance_weight = spectrum_to_rgb(
            safe_divide_color(result.indirect_throughput, initial_throughput));
        guiding_record_volume_transmission(kg, state, transmittance_weight);
        guiding_record_volume_segment(kg, state, direct_P, sd.I);
        guiding_generated_new_segment = true;
        unlit_throughput = result.indirect_throughput / continuation_probability;
        rand_phase_guiding = path_state_rng_1D(kg, &rng_state, PRNG_VOLUME_PHASE_GUIDING_DISTANCE);
      }
      else {
        /* If the direct scatter event is generated using VOLUME_SAMPLE_EQUIANGULAR the direct
         * event will happen at a separate position as the indirect event and the direct light
         * contribution will contribute to the position of the current/previous path segment. The
         * unlit_throughput has to be adjusted to include the scattering at the previous segment.*/
        float3 scatterEval = one_float3();
        if (state->guiding.path_segment) {
          pgl_vec3f scatteringWeight = state->guiding.path_segment->scatteringWeight;
          scatterEval = make_float3(scatteringWeight.x, scatteringWeight.y, scatteringWeight.z);
        }
        unlit_throughput /= scatterEval;
        unlit_throughput *= continuation_probability;
        rand_phase_guiding = path_state_rng_1D(
            kg, &rng_state, PRNG_VOLUME_PHASE_GUIDING_EQUIANGULAR);
      }
#    endif
#    if PATH_GUIDING_LEVEL >= 4
      volume_shader_prepare_guiding(
          kg, state, &sd, rand_phase_guiding, direct_P, ray->D, &result.direct_phases);
#    endif
    }
#  endif

    result.direct_throughput /= continuation_probability;
    integrate_volume_direct_light(kg,
                                  state,
                                  &sd,
                                  &rng_state,
                                  direct_P,
                                  &result.direct_phases,
#  ifdef __PATH_GUIDING__
                                  unlit_throughput,
#  endif
                                  result.direct_throughput);
  }

  /* Indirect light.
   *
   * Only divide throughput by continuation_probability if we scatter. For the attenuation
   * case the next surface will already do this division. */
  if (result.indirect_scatter) {
#  if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
    if (!guiding_generated_new_segment) {
      float3 transmittance_weight = spectrum_to_rgb(
          safe_divide_color(result.indirect_throughput, initial_throughput));
      guiding_record_volume_transmission(kg, state, transmittance_weight);
    }
#  endif
    result.indirect_throughput /= continuation_probability;
  }
  INTEGRATOR_STATE_WRITE(state, path, throughput) = result.indirect_throughput;

  if (result.indirect_scatter) {
    sd.P = ray->P + result.indirect_t * ray->D;

#  if defined(__PATH_GUIDING__)
#    if PATH_GUIDING_LEVEL >= 1
    if (!guiding_generated_new_segment) {
      guiding_record_volume_segment(kg, state, sd.P, sd.I);
    }
#    endif
#    if PATH_GUIDING_LEVEL >= 4
    /* If the direct scatter event was generated using VOLUME_SAMPLE_EQUIANGULAR we need to
     * initialize the guiding distribution at the indirect scatter position. */
    if (result.direct_sample_method == VOLUME_SAMPLE_EQUIANGULAR) {
      rand_phase_guiding = path_state_rng_1D(kg, &rng_state, PRNG_VOLUME_PHASE_GUIDING_DISTANCE);
      volume_shader_prepare_guiding(
          kg, state, &sd, rand_phase_guiding, sd.P, ray->D, &result.indirect_phases);
    }
#    endif
#  endif

    if (integrate_volume_phase_scatter(kg, state, &sd, &rng_state, &result.indirect_phases)) {
      return VOLUME_PATH_SCATTERED;
    }
    else {
      return VOLUME_PATH_MISSED;
    }
  }
  else {
#  if defined(__PATH_GUIDING__)
    /* No guiding if we don't scatter. */
    state->guiding.use_volume_guiding = false;
#  endif
    return VOLUME_PATH_ATTENUATED;
  }
}

#endif

ccl_device void integrator_shade_volume(KernelGlobals kg,
                                        IntegratorState state,
                                        ccl_global float *ccl_restrict render_buffer)
{
  PROFILING_INIT(kg, PROFILING_SHADE_VOLUME_SETUP);

#ifdef __VOLUME__
  /* Setup shader data. */
  Ray ray ccl_optional_struct_init;
  integrator_state_read_ray(kg, state, &ray);

  Intersection isect ccl_optional_struct_init;
  integrator_state_read_isect(kg, state, &isect);

  /* Set ray length to current segment. */
  ray.tmax = (isect.prim != PRIM_NONE) ? isect.t : FLT_MAX;

  /* Clean volume stack for background rays. */
  if (isect.prim == PRIM_NONE) {
    volume_stack_clean(kg, state);
  }

  VolumeIntegrateEvent event = volume_integrate(kg, state, &ray, render_buffer);

  if (event == VOLUME_PATH_SCATTERED) {
    /* Queue intersect_closest kernel. */
    integrator_path_next(kg,
                         state,
                         DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME,
                         DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
    return;
  }
  else if (event == VOLUME_PATH_MISSED) {
    /* End path. */
    integrator_path_terminate(kg, state, DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME);
    return;
  }
  else {
    /* Continue to background, light or surface. */
    integrator_intersect_next_kernel_after_volume<DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME>(
        kg, state, &isect, render_buffer);
    return;
  }
#endif /* __VOLUME__ */
}

CCL_NAMESPACE_END
