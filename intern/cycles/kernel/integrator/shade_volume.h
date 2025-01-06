/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/closure/volume.h"

#include "kernel/film/denoising_passes.h"
#include "kernel/film/light_passes.h"

#include "kernel/integrator/guiding.h"
#include "kernel/integrator/intersect_closest.h"
#include "kernel/integrator/path_state.h"
#include "kernel/integrator/shadow_linking.h"
#include "kernel/integrator/volume_shader.h"
#include "kernel/integrator/volume_stack.h"

#include "kernel/light/light.h"
#include "kernel/light/sample.h"

#include "kernel/geom/shader_data.h"

CCL_NAMESPACE_BEGIN

#ifdef __VOLUME__

/* Events for probabilistic scattering. */

enum VolumeIntegrateEvent {
  VOLUME_PATH_SCATTERED = 0,
  VOLUME_PATH_ATTENUATED = 1,
  VOLUME_PATH_MISSED = 2
};

struct VolumeIntegrateResult {
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
};

/* Ignore paths that have volume throughput below this value, to avoid unnecessary work
 * and precision issues.
 * todo: this value could be tweaked or turned into a probability to avoid unnecessary
 * work in volumes and subsurface scattering. */
#  define VOLUME_THROUGHPUT_EPSILON 1e-6f

/* Volume shader properties
 *
 * extinction coefficient = absorption coefficient + scattering coefficient
 * sigma_t = sigma_a + sigma_s */

struct VolumeShaderCoefficients {
  Spectrum sigma_t;
  Spectrum sigma_s;
  Spectrum emission;
};

struct EquiangularCoefficients {
  float3 P;
  Interval<float> t_range;
};

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

  *extinction = sd->closure_transparent_extinction;
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
      const ccl_private ShaderClosure *sc = &sd->closure[i];

      if (CLOSURE_IS_VOLUME(sc->type)) {
        coeff->sigma_s += sc->weight;
      }
    }
  }

  return true;
}

/* Determines the next shading position. */
struct VolumeStep {
  /* Shift starting point of all segments by a random amount to avoid banding artifacts due to
   * biased ray marching with insufficient step size. */
  float offset;

  /* Step size taken at each marching step. */
  float size;

  /* Perform shading at this offset within a step, to integrate over the entire step segment. */
  float shade_offset;

  /* Maximal steps allowed between `ray->tmin` and `ray->tmax`. */
  int max_steps;

  /* Current active segment. */
  Interval<float> t;
};

template<const bool shadow>
ccl_device_forceinline void volume_step_init(KernelGlobals kg,
                                             const ccl_private RNGState *rng_state,
                                             const float object_step_size,
                                             const float tmin,
                                             const float tmax,
                                             ccl_private VolumeStep *vstep)
{
  vstep->t.min = vstep->t.max = tmin;

  if (object_step_size == FLT_MAX) {
    /* Homogeneous volume. */
    vstep->size = tmax - tmin;
    vstep->shade_offset = 0.0f;
    vstep->offset = 1.0f;
    vstep->max_steps = 1;
  }
  else {
    /* Heterogeneous volume. */
    vstep->max_steps = kernel_data.integrator.volume_max_steps;
    const float t = tmax - tmin;
    float step_size = min(object_step_size, t);

    if (t > vstep->max_steps * step_size) {
      /* Increase step size to cover the whole ray segment. */
      step_size = t / (float)vstep->max_steps;
    }

    vstep->size = step_size;
    vstep->shade_offset = path_state_rng_1D(kg, rng_state, PRNG_VOLUME_SHADE_OFFSET);

    if (shadow) {
      /* For shadows we do not offset all segments, since the starting point is already a random
       * distance inside the volume. It also appears to create banding artifacts for unknown
       * reasons. */
      vstep->offset = 1.0f;
    }
    else {
      vstep->offset = path_state_rng_1D(kg, rng_state, PRNG_VOLUME_OFFSET);
    }
  }
}

ccl_device_inline bool volume_integrate_advance(const int step,
                                                const ccl_private Ray *ccl_restrict ray,
                                                ccl_private float3 *shade_P,
                                                ccl_private VolumeStep &vstep)
{
  if (vstep.t.max == ray->tmax) {
    /* Reached the last segment. */
    return false;
  }

  /* Advance to new position. */
  vstep.t.min = vstep.t.max;
  vstep.t.max = min(ray->tmax, ray->tmin + (step + vstep.offset) * vstep.size);
  const float shade_t = mix(vstep.t.min, vstep.t.max, vstep.shade_offset);
  *shade_P = ray->P + ray->D * shade_t;

  return step < vstep.max_steps;
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

  /* Prepare for stepping. */
  VolumeStep vstep;
  volume_step_init<true>(kg, &rng_state, object_step_size, ray->tmin, ray->tmax, &vstep);

  /* compute extinction at the start */
  Spectrum sum = zero_spectrum();
  for (int step = 0; volume_integrate_advance(step, ray, &sd->P, vstep); step++) {
    /* compute attenuation over segment */
    Spectrum sigma_t = zero_spectrum();
    if (shadow_volume_shader_sample(kg, state, sd, &sigma_t)) {
      /* Compute `expf()` only for every Nth step, to save some calculations
       * because `exp(a)*exp(b) = exp(a+b)`, also do a quick #VOLUME_THROUGHPUT_EPSILON
       * check then. */
      sum += (-sigma_t * vstep.t.length());
      if ((step & 0x07) == 0) { /* TODO: Other interval? */
        tp = *throughput * exp(sum);

        /* stop if nearly all light is blocked */
        if (reduce_max(tp) < VOLUME_THROUGHPUT_EPSILON) {
          break;
        }
      }
    }
  }

  if (vstep.t.max == ray->tmax) {
    /* Update throughput in case we haven't done it above. */
    tp = *throughput * exp(sum);
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

ccl_device float volume_equiangular_sample(const ccl_private Ray *ccl_restrict ray,
                                           const ccl_private EquiangularCoefficients &coeffs,
                                           const float xi,
                                           ccl_private float *pdf)
{
  const float delta = dot((coeffs.P - ray->P), ray->D);
  const float D = safe_sqrtf(len_squared(coeffs.P - ray->P) - delta * delta);
  if (UNLIKELY(D == 0.0f)) {
    *pdf = 0.0f;
    return 0.0f;
  }
  const float tmin = coeffs.t_range.min;
  const float tmax = coeffs.t_range.max;
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

ccl_device float volume_equiangular_pdf(const ccl_private Ray *ccl_restrict ray,
                                        const ccl_private EquiangularCoefficients &coeffs,
                                        const float sample_t)
{
  const float delta = dot((coeffs.P - ray->P), ray->D);
  const float D = safe_sqrtf(len_squared(coeffs.P - ray->P) - delta * delta);
  if (UNLIKELY(D == 0.0f)) {
    return 0.0f;
  }

  const float tmin = coeffs.t_range.min;
  const float tmax = coeffs.t_range.max;
  const float t_ = sample_t - delta;

  const float theta_a = atan2f(tmin - delta, D);
  const float theta_b = atan2f(tmax - delta, D);
  if (UNLIKELY(theta_b == theta_a)) {
    return 0.0f;
  }

  const float pdf = D / ((theta_b - theta_a) * (D * D + t_ * t_));

  return pdf;
}

ccl_device_inline bool volume_equiangular_valid_ray_segment(KernelGlobals kg,
                                                            const float3 ray_P,
                                                            const float3 ray_D,
                                                            ccl_private Interval<float> *t_range,
                                                            const ccl_private LightSample *ls)
{
  if (ls->type == LIGHT_SPOT) {
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, ls->lamp);
    return spot_light_valid_ray_segment(klight, ray_P, ray_D, t_range);
  }
  if (ls->type == LIGHT_AREA) {
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, ls->lamp);
    return area_light_valid_ray_segment(&klight->area, ray_P - klight->co, ray_D, t_range);
  }
  if (ls->type == LIGHT_TRIANGLE) {
    return triangle_light_valid_ray_segment(kg, ray_P - ls->P, ray_D, t_range, ls);
  }

  /* Point light, the whole range of the ray is visible. */
  kernel_assert(ls->type == LIGHT_POINT);
  return true;
}

/* Emission */

ccl_device Spectrum volume_emission_integrate(ccl_private VolumeShaderCoefficients *coeff,
                                              const int closure_flag,
                                              Spectrum transmittance,
                                              const float t)
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
  else {
    emission *= t;
  }

  return emission;
}

/* Volume Integration */

struct VolumeIntegrateState {
  /* Random numbers for scattering. */
  float rscatter;
  float rchannel;

  /* Multiple importance sampling. */
  VolumeSampleMethod direct_sample_method;
  bool use_mis;
  float distance_pdf;
  float equiangular_pdf;
};

ccl_device bool volume_integrate_should_stop(ccl_private VolumeIntegrateResult &result)
{
  /* Stop if nearly all light blocked. */
  if (!result.indirect_scatter) {
    if (reduce_max(result.indirect_throughput) < VOLUME_THROUGHPUT_EPSILON) {
      result.indirect_throughput = zero_spectrum();
      return true;
    }
  }
  else if (!result.direct_scatter) {
    if (reduce_max(result.direct_throughput) < VOLUME_THROUGHPUT_EPSILON) {
      return true;
    }
  }

  /* If we have scattering data for both direct and indirect, we're done. */
  return (result.direct_scatter && result.indirect_scatter);
}

/* Returns true if we found the indirect scatter position within the current active ray segment. */
ccl_device bool volume_sample_indirect_scatter(
    const Spectrum transmittance,
    const Spectrum channel_pdf,
    const int channel,
    const ccl_private ShaderData *ccl_restrict sd,
    const ccl_private VolumeShaderCoefficients &ccl_restrict coeff,
    const ccl_private Interval<float> &t,
    ccl_private VolumeIntegrateState &ccl_restrict vstate,
    ccl_private VolumeIntegrateResult &ccl_restrict result)
{
  if (result.indirect_scatter) {
    /* Already sampled indirect scatter position. */
    return false;
  }

  /* If sampled distance does not go beyond the current segment, we have found the scatter
   * position. Otherwise continue searching and accumulate the transmittance along the ray. */
  const float sample_transmittance = volume_channel_get(transmittance, channel);
  if (1.0f - vstate.rscatter >= sample_transmittance) {
    /* Pick `sigma_t` from a random channel. */
    const float sample_sigma_t = volume_channel_get(coeff.sigma_t, channel);

    /* Generate the next distance using random walk, following exponential distribution
     * p(dt) = sigma_t * exp(-sigma_t * dt). */
    const float new_dt = -logf(1.0f - vstate.rscatter) / sample_sigma_t;
    const float new_t = t.min + new_dt;

    const Spectrum new_transmittance = volume_color_transmittance(coeff.sigma_t, new_dt);
    /* PDF for density-based distance sampling is handled implicitly via
     * transmittance / pdf = exp(-sigma_t * dt) / (sigma_t * exp(-sigma_t * dt)) = 1 / sigma_t. */
    const float distance_pdf = dot(channel_pdf, coeff.sigma_t * new_transmittance);

    if (vstate.distance_pdf * distance_pdf > VOLUME_SAMPLE_PDF_CUTOFF) {
      /* Update throughput. */
      result.indirect_scatter = true;
      result.indirect_t = new_t;
      result.indirect_throughput *= coeff.sigma_s * new_transmittance / distance_pdf;
      if (vstate.direct_sample_method != VOLUME_SAMPLE_EQUIANGULAR) {
        vstate.distance_pdf *= distance_pdf;
      }

      volume_shader_copy_phases(&result.indirect_phases, sd);

      return true;
    }
  }
  else {
    /* Update throughput. */
    const float distance_pdf = dot(channel_pdf, transmittance);
    result.indirect_throughput *= transmittance / distance_pdf;
    if (vstate.direct_sample_method != VOLUME_SAMPLE_EQUIANGULAR) {
      vstate.distance_pdf *= distance_pdf;
    }

    /* Remap rscatter so we can reuse it and keep thing stratified. */
    vstate.rscatter = 1.0f - (1.0f - vstate.rscatter) / sample_transmittance;
  }

  return false;
}

/* Find direct and indirect scatter positions. */
ccl_device_forceinline void volume_integrate_step_scattering(
    const ccl_private ShaderData *sd,
    const ccl_private Ray *ray,
    const ccl_private EquiangularCoefficients &equiangular_coeffs,
    const ccl_private VolumeShaderCoefficients &ccl_restrict coeff,
    const Spectrum transmittance,
    const ccl_private Interval<float> &t,
    ccl_private VolumeIntegrateState &ccl_restrict vstate,
    ccl_private VolumeIntegrateResult &ccl_restrict result)
{
  /* Pick random color channel for sampling the scatter distance. We use the Veach one-sample model
   * with balance heuristic for the channels.
   * Set `albedo` to 1 for the channel where extinction coefficient `sigma_t` is zero, to make sure
   * that we sample a distance outside the current segment when that channel is picked, meaning
   * light passes through without attenuation. */
  const Spectrum albedo = safe_divide_color(coeff.sigma_s, coeff.sigma_t, 1.0f);
  Spectrum channel_pdf;
  const int channel = volume_sample_channel(
      albedo, result.indirect_throughput, &vstate.rchannel, &channel_pdf);

  /* Equiangular sampling for direct lighting. */
  if (vstate.direct_sample_method == VOLUME_SAMPLE_EQUIANGULAR && !result.direct_scatter) {
    if (t.contains(result.direct_t) && vstate.equiangular_pdf > VOLUME_SAMPLE_PDF_CUTOFF) {
      const float new_dt = result.direct_t - t.min;
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
  if (volume_sample_indirect_scatter(
          transmittance, channel_pdf, channel, sd, coeff, t, vstate, result))
  {
    if (vstate.direct_sample_method != VOLUME_SAMPLE_EQUIANGULAR) {
      /* If using distance sampling for direct light, just copy parameters of indirect light
       * since we scatter at the same point. */
      result.direct_scatter = true;
      result.direct_t = result.indirect_t;
      result.direct_throughput = result.indirect_throughput;
      volume_shader_copy_phases(&result.direct_phases, sd);

      /* Multiple importance sampling. */
      if (vstate.use_mis) {
        const float equiangular_pdf = volume_equiangular_pdf(
            ray, equiangular_coeffs, result.indirect_t);
        const float mis_weight = power_heuristic(vstate.distance_pdf, equiangular_pdf);
        result.direct_throughput *= 2.0f * mis_weight;
      }
    }
  }
}

ccl_device_inline void volume_integrate_state_init(KernelGlobals kg,
                                                   const ccl_private RNGState *rng_state,
                                                   const VolumeSampleMethod direct_sample_method,
                                                   ccl_private VolumeIntegrateState &vstate)
{
  vstate.rscatter = path_state_rng_1D(kg, rng_state, PRNG_VOLUME_SCATTER_DISTANCE);
  vstate.rchannel = path_state_rng_1D(kg, rng_state, PRNG_VOLUME_COLOR_CHANNEL);

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
    const ccl_private RNGState *rng_state,
    ccl_global float *ccl_restrict render_buffer,
    const float object_step_size,
    const VolumeSampleMethod direct_sample_method,
    const ccl_private EquiangularCoefficients &equiangular_coeffs,
    ccl_private VolumeIntegrateResult &result)
{
  PROFILING_INIT(kg, PROFILING_SHADE_VOLUME_INTEGRATE);

  /* Prepare for stepping. */
  VolumeStep vstep;
  volume_step_init<false>(kg, rng_state, object_step_size, ray->tmin, ray->tmax, &vstep);

  /* Initialize volume integration state. */
  VolumeIntegrateState vstate ccl_optional_struct_init;
  volume_integrate_state_init(kg, rng_state, direct_sample_method, vstate);

  /* Initialize volume integration result. */
  const Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
  result.direct_throughput = throughput;
  result.indirect_throughput = throughput;

  /* Equiangular sampling: compute distance and PDF in advance. */
  if (vstate.direct_sample_method == VOLUME_SAMPLE_EQUIANGULAR) {
    result.direct_t = volume_equiangular_sample(
        ray, equiangular_coeffs, vstate.rscatter, &vstate.equiangular_pdf);
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

  for (int step = 0; volume_integrate_advance(step, ray, &sd->P, vstep); step++) {
    /* compute segment */
    VolumeShaderCoefficients coeff ccl_optional_struct_init;
    if (volume_shader_sample(kg, state, sd, &coeff)) {
      const int closure_flag = sd->flag;

      /* Evaluate transmittance over segment. */
      const float dt = vstep.t.length();
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

      if (closure_flag & SD_SCATTER) {
#  ifdef __DENOISING_FEATURES__
        /* Accumulate albedo for denoising features. */
        if (write_denoising_features && (closure_flag & SD_SCATTER)) {
          const Spectrum albedo = safe_divide_color(coeff.sigma_s, coeff.sigma_t);
          accum_albedo += result.indirect_throughput * albedo * (one_spectrum() - transmittance);
        }
#  endif

        /* Scattering and absorption. */
        volume_integrate_step_scattering(
            sd, ray, equiangular_coeffs, coeff, transmittance, vstep.t, vstate, result);
      }
      else if (closure_flag & SD_EXTINCTION) {
        /* Absorption only. */
        result.indirect_throughput *= transmittance;
        result.direct_throughput *= transmittance;
      }

      if (volume_integrate_should_stop(result)) {
        break;
      }
    }
  }

  /* Write accumulated emission. */
  if (!is_zero(accum_emission)) {
    if (light_link_object_match(kg, light_link_receiver_forward(kg, state), sd->object)) {
      film_write_volume_emission(
          kg, state, accum_emission, render_buffer, object_lightgroup(kg, sd->object));
    }
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
    const ccl_private Ray *ccl_restrict ray,
    const ccl_private ShaderData *ccl_restrict sd,
    const ccl_private RNGState *ccl_restrict rng_state,
    ccl_private EquiangularCoefficients *ccl_restrict equiangular_coeffs,
    ccl_private LightSample &ccl_restrict ls)
{
  /* Test if there is a light or BSDF that needs direct light. */
  if (!kernel_data.integrator.use_direct_light) {
    return false;
  }

  /* Sample position on a light. */
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  const uint bounce = INTEGRATOR_STATE(state, path, bounce);
  const float3 rand_light = path_state_rng_3D(kg, rng_state, PRNG_LIGHT);

  if (!light_sample_from_volume_segment(kg,
                                        rand_light,
                                        sd->time,
                                        sd->P,
                                        ray->D,
                                        ray->tmax - ray->tmin,
                                        light_link_receiver_nee(kg, sd),
                                        bounce,
                                        path_flag,
                                        &ls))
  {
    ls.emitter_id = EMITTER_NONE;
    return false;
  }

  if (ls.shader & SHADER_EXCLUDE_SCATTER) {
    ls.emitter_id = EMITTER_NONE;
    return false;
  }

  if (ls.t == FLT_MAX) {
    /* Sampled distant/background light is valid in volume segment, but we are going to sample the
     * light position with distance sampling instead of equiangular. */
    return false;
  }

  equiangular_coeffs->P = ls.P;

  return volume_equiangular_valid_ray_segment(
      kg, ray->P, ray->D, &equiangular_coeffs->t_range, &ls);
}

/* Path tracing: sample point on light and evaluate light shader, then
 * queue shadow ray to be traced. */
ccl_device_forceinline void integrate_volume_direct_light(
    KernelGlobals kg,
    IntegratorState state,
    const ccl_private ShaderData *ccl_restrict sd,
    const ccl_private RNGState *ccl_restrict rng_state,
    const float3 P,
    const ccl_private ShaderVolumePhases *ccl_restrict phases,
#  ifdef __PATH_GUIDING__
    const ccl_private Spectrum unlit_throughput,
#  endif
    const ccl_private Spectrum throughput,
    ccl_private LightSample &ccl_restrict ls)
{
  PROFILING_INIT(kg, PROFILING_SHADE_VOLUME_DIRECT_LIGHT);

  if (!kernel_data.integrator.use_direct_light || ls.emitter_id == EMITTER_NONE) {
    return;
  }

  /* Sample position on the same light again, now from the shading point where we scattered. */
  {
    const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
    const uint bounce = INTEGRATOR_STATE(state, path, bounce);
    const float3 rand_light = path_state_rng_3D(kg, rng_state, PRNG_LIGHT);
    const float3 N = zero_float3();
    const int object_receiver = light_link_receiver_nee(kg, sd);
    const int shader_flags = SD_BSDF_HAS_TRANSMISSION;

    if (!light_sample<false>(
            kg, rand_light, sd->time, P, N, object_receiver, shader_flags, bounce, path_flag, &ls))
    {
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
  const float phase_pdf = volume_shader_phase_eval(
      kg, state, sd, phases, ls.D, &phase_eval, ls.shader);
  const float mis_weight = light_sample_mis_weight_nee(kg, ls.pdf, phase_pdf);
  bsdf_eval_mul(&phase_eval, light_eval / ls.pdf * mis_weight);

  /* Path termination. */
  const float terminate = path_state_rng_light_termination(kg, rng_state);
  if (light_sample_terminate(kg, &phase_eval, terminate)) {
    return;
  }

  /* Create shadow ray. */
  Ray ray ccl_optional_struct_init;
  light_sample_to_volume_shadow_ray(kg, sd, &ls, P, &ray);

  /* Branch off shadow kernel. */
  IntegratorShadowState shadow_state = integrator_shadow_path_init(
      kg, state, DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW, false);

  /* Write shadow ray and associated state to global memory. */
  integrator_state_write_shadow_ray(shadow_state, &ray);
  integrator_state_write_shadow_ray_self(kg, shadow_state, &ray);

  /* Copy state from main path to shadow path. */
  const uint16_t bounce = INTEGRATOR_STATE(state, path, bounce);
  const uint16_t transparent_bounce = INTEGRATOR_STATE(state, path, transparent_bounce);
  uint32_t shadow_flag = INTEGRATOR_STATE(state, path, flag);
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
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, rng_pixel) = INTEGRATOR_STATE(
      state, path, rng_pixel);
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

  /* Write Light-group, +1 as light-group is int but we need to encode into a uint8_t. */
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, lightgroup) = ls.group + 1;

#  ifdef __PATH_GUIDING__
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, unlit_throughput) = unlit_throughput;
  INTEGRATOR_STATE_WRITE(shadow_state, shadow_path, path_segment) = INTEGRATOR_STATE(
      state, guiding, path_segment);
  INTEGRATOR_STATE(shadow_state, shadow_path, guiding_mis_weight) = 0.0f;
#  endif

  integrator_state_copy_volume_stack_to_shadow(kg, shadow_state, state);
}

/* Path tracing: scatter in new direction using phase function */
ccl_device_forceinline bool integrate_volume_phase_scatter(
    KernelGlobals kg,
    IntegratorState state,
    ccl_private ShaderData *sd,
    const ccl_private Ray *ray,
    const ccl_private RNGState *rng_state,
    const ccl_private ShaderVolumePhases *phases)
{
  PROFILING_INIT(kg, PROFILING_SHADE_VOLUME_INDIRECT_LIGHT);

  float2 rand_phase = path_state_rng_2D(kg, rng_state, PRNG_VOLUME_PHASE);

  const ccl_private ShaderVolumeClosure *svc = volume_shader_phase_pick(phases, &rand_phase);

  /* Phase closure, sample direction. */
  float phase_pdf = 0.0f;
  float unguided_phase_pdf = 0.0f;
  BsdfEval phase_eval ccl_optional_struct_init;
  float3 phase_wo ccl_optional_struct_init;
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
                                              &phase_wo,
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
    label = volume_shader_phase_sample(
        kg, sd, phases, svc, rand_phase, &phase_eval, &phase_wo, &phase_pdf, &sampled_roughness);

    if (phase_pdf == 0.0f || bsdf_eval_is_zero(&phase_eval)) {
      return false;
    }

    unguided_phase_pdf = phase_pdf;
  }

  /* Setup ray. */
  INTEGRATOR_STATE_WRITE(state, ray, P) = sd->P;
  INTEGRATOR_STATE_WRITE(state, ray, D) = normalize(phase_wo);
  INTEGRATOR_STATE_WRITE(state, ray, tmin) = 0.0f;
#  ifdef __LIGHT_TREE__
  if (kernel_data.integrator.use_light_tree) {
    INTEGRATOR_STATE_WRITE(state, ray, previous_dt) = ray->tmax - ray->tmin;
  }
#  endif
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
      kg, state, sd, phase_weight, phase_pdf, normalize(phase_wo), sampled_roughness);

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
  const float3 previous_P = ray->P + ray->D * ray->tmin;
  INTEGRATOR_STATE_WRITE(state, path, mis_origin_n) = sd->P - previous_P;
  INTEGRATOR_STATE_WRITE(state, path, min_ray_pdf) = fminf(
      unguided_phase_pdf, INTEGRATOR_STATE(state, path, min_ray_pdf));

#  ifdef __LIGHT_LINKING__
  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_LINKING) {
    INTEGRATOR_STATE_WRITE(state, path, mis_ray_object) = sd->object;
  }
#  endif

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
  /* FIXME: `object` is used for light linking. We read the bottom of the stack for simplicity, but
   * this does not work for overlapping volumes. */
  shader_setup_from_volume(kg, &sd, ray, INTEGRATOR_STATE_ARRAY(state, volume_stack, 0, object));

  /* Load random number state. */
  RNGState rng_state;
  path_state_rng_load(state, &rng_state);

  /* Sample light ahead of volume stepping, for equiangular sampling. */
  /* TODO: distant lights are ignored now, but could instead use even distribution. */
  LightSample ls ccl_optional_struct_init;
  const bool need_light_sample = !(INTEGRATOR_STATE(state, path, flag) & PATH_RAY_TERMINATE);

  EquiangularCoefficients equiangular_coeffs = {zero_float3(), {ray->tmin, ray->tmax}};

  const bool have_equiangular_sample =
      need_light_sample && integrate_volume_equiangular_sample_light(
                               kg, state, ray, &sd, &rng_state, &equiangular_coeffs, ls);

  const VolumeSampleMethod direct_sample_method = (have_equiangular_sample) ?
                                                      volume_stack_sample_method(kg, state) :
                                                      VOLUME_SAMPLE_DISTANCE;

  /* Step through volume. */
  VOLUME_READ_LAMBDA(integrator_state_read_volume_stack(state, i))
  const float step_size = volume_stack_step_size(kg, volume_read_lambda_pass);

#  if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  /* The current path throughput which is used later to calculate per-segment throughput. */
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
                                 equiangular_coeffs,
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
         * will contribute to the position of the next path segment. */
        const float3 transmittance_weight = spectrum_to_rgb(
            safe_divide_color(result.indirect_throughput, initial_throughput));
        guiding_record_volume_transmission(kg, state, transmittance_weight);
        guiding_record_volume_segment(kg, state, direct_P, sd.wi);
        guiding_generated_new_segment = true;
        unlit_throughput = result.indirect_throughput / continuation_probability;
        rand_phase_guiding = path_state_rng_1D(kg, &rng_state, PRNG_VOLUME_PHASE_GUIDING_DISTANCE);
      }
      else {
        /* If the direct scatter event is generated using VOLUME_SAMPLE_EQUIANGULAR the direct
         * event will happen at a separate position as the indirect event and the direct light
         * contribution will contribute to the position of the current/previous path segment. The
         * unlit_throughput has to be adjusted to include the scattering at the previous segment.
         */
        float3 scatterEval = one_float3();
        if (state->guiding.path_segment) {
          const pgl_vec3f scatteringWeight = state->guiding.path_segment->scatteringWeight;
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
                                  result.direct_throughput,
                                  ls);
  }

  /* Indirect light.
   *
   * Only divide throughput by continuation_probability if we scatter. For the attenuation
   * case the next surface will already do this division. */
  if (result.indirect_scatter) {
#  if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
    if (!guiding_generated_new_segment) {
      const float3 transmittance_weight = spectrum_to_rgb(
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
      guiding_record_volume_segment(kg, state, sd.P, sd.wi);
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

    if (integrate_volume_phase_scatter(kg, state, &sd, ray, &rng_state, &result.indirect_phases)) {
      return VOLUME_PATH_SCATTERED;
    }
    return VOLUME_PATH_MISSED;
  }
#  if defined(__PATH_GUIDING__)
  /* No guiding if we don't scatter. */
  state->guiding.use_volume_guiding = false;
#  endif
  return VOLUME_PATH_ATTENUATED;
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
  integrator_state_read_ray(state, &ray);

  Intersection isect ccl_optional_struct_init;
  integrator_state_read_isect(state, &isect);

  /* Set ray length to current segment. */
  ray.tmax = (isect.prim != PRIM_NONE) ? isect.t : FLT_MAX;

  /* Clean volume stack for background rays. */
  if (isect.prim == PRIM_NONE) {
    volume_stack_clean(kg, state);
  }

  const VolumeIntegrateEvent event = volume_integrate(kg, state, &ray, render_buffer);
  if (event == VOLUME_PATH_MISSED) {
    /* End path. */
    integrator_path_terminate(kg, state, DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME);
    return;
  }

  if (event == VOLUME_PATH_ATTENUATED) {
    /* Continue to background, light or surface. */
    integrator_intersect_next_kernel_after_volume<DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME>(
        kg, state, &isect, render_buffer);
    return;
  }

#  ifdef __SHADOW_LINKING__
  if (shadow_linking_schedule_intersection_kernel<DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME>(kg,
                                                                                         state))
  {
    return;
  }
#  endif /* __SHADOW_LINKING__ */

  /* Queue intersect_closest kernel. */
  integrator_path_next(kg,
                       state,
                       DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME,
                       DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST);
#endif /* __VOLUME__ */
}

CCL_NAMESPACE_END
