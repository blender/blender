/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "kernel/camera/projection.h"

#include "kernel/bvh/bvh.h"

#include "kernel/integrator/guiding.h"

CCL_NAMESPACE_BEGIN

/* Random walk subsurface scattering.
 *
 * "Practical and Controllable Subsurface Scattering for Production Path
 *  Tracing". Matt Jen-Yuan Chiang, Peter Kutz, Brent Burley. SIGGRAPH 2016. */

/* Support for anisotropy from:
 * "Path Traced Subsurface Scattering using Anisotropic Phase Functions
 * and Non-Exponential Free Flights".
 * Magnus Wrenninge, Ryusuke Villemin, Christophe Hery.
 * https://graphics.pixar.com/library/PathTracedSubsurface/ */

ccl_device void subsurface_random_walk_remap(const float albedo,
                                             const float d,
                                             float g,
                                             ccl_private float *sigma_t,
                                             ccl_private float *alpha)
{
  /* Compute attenuation and scattering coefficients from albedo. */
  const float g2 = g * g;
  const float g3 = g2 * g;
  const float g4 = g3 * g;
  const float g5 = g4 * g;
  const float g6 = g5 * g;
  const float g7 = g6 * g;

  const float A = 1.8260523782f + -1.28451056436f * g + -1.79904629312f * g2 +
                  9.19393289202f * g3 + -22.8215585862f * g4 + 32.0234874259f * g5 +
                  -23.6264803333f * g6 + 7.21067002658f * g7;
  const float B = 4.98511194385f +
                  0.127355959438f *
                      expf(31.1491581433f * g + -201.847017512f * g2 + 841.576016723f * g3 +
                           -2018.09288505f * g4 + 2731.71560286f * g5 + -1935.41424244f * g6 +
                           559.009054474f * g7);
  const float C = 1.09686102424f + -0.394704063468f * g + 1.05258115941f * g2 +
                  -8.83963712726f * g3 + 28.8643230661f * g4 + -46.8802913581f * g5 +
                  38.5402837518f * g6 + -12.7181042538f * g7;
  const float D = 0.496310210422f + 0.360146581622f * g + -2.15139309747f * g2 +
                  17.8896899217f * g3 + -55.2984010333f * g4 + 82.065982243f * g5 +
                  -58.5106008578f * g6 + 15.8478295021f * g7;
  const float E = 4.23190299701f +
                  0.00310603949088f *
                      expf(76.7316253952f * g + -594.356773233f * g2 + 2448.8834203f * g3 +
                           -5576.68528998f * g4 + 7116.60171912f * g5 + -4763.54467887f * g6 +
                           1303.5318055f * g7);
  const float F = 2.40602999408f + -2.51814844609f * g + 9.18494908356f * g2 +
                  -79.2191708682f * g3 + 259.082868209f * g4 + -403.613804597f * g5 +
                  302.85712436f * g6 + -87.4370473567f * g7;

  const float blend = powf(albedo, 0.25f);

  *alpha = (1.0f - blend) * A * powf(atanf(B * albedo), C) +
           blend * D * powf(atanf(E * albedo), F);
  *alpha = clamp(*alpha, 0.0f, 0.999999f);  // because of numerical precision

  float sigma_t_prime = 1.0f / fmaxf(d, 1e-16f);
  *sigma_t = sigma_t_prime / (1.0f - g);
}

ccl_device void subsurface_random_walk_coefficients(const Spectrum albedo,
                                                    const Spectrum radius,
                                                    const float anisotropy,
                                                    ccl_private Spectrum *sigma_t,
                                                    ccl_private Spectrum *alpha,
                                                    ccl_private Spectrum *throughput)
{
  FOREACH_SPECTRUM_CHANNEL (i) {
    subsurface_random_walk_remap(GET_SPECTRUM_CHANNEL(albedo, i),
                                 GET_SPECTRUM_CHANNEL(radius, i),
                                 anisotropy,
                                 &GET_SPECTRUM_CHANNEL(*sigma_t, i),
                                 &GET_SPECTRUM_CHANNEL(*alpha, i));
  }

  /* Throughput already contains closure weight at this point, which includes the
   * albedo, as well as closure mixing and Fresnel weights. Divide out the albedo
   * which will be added through scattering. */
  *throughput = safe_divide_color(*throughput, albedo);

  /* With low albedo values (like 0.025) we get diffusion_length 1.0 and
   * infinite phase functions. To avoid a sharp discontinuity as we go from
   * such values to 0.0, increase alpha and reduce the throughput to compensate. */
  const float min_alpha = 0.2f;
  FOREACH_SPECTRUM_CHANNEL (i) {
    if (GET_SPECTRUM_CHANNEL(*alpha, i) < min_alpha) {
      GET_SPECTRUM_CHANNEL(*throughput, i) *= GET_SPECTRUM_CHANNEL(*alpha, i) / min_alpha;
      GET_SPECTRUM_CHANNEL(*alpha, i) = min_alpha;
    }
  }
}

/* References for Dwivedi sampling:
 *
 * [1] "A Zero-variance-based Sampling Scheme for Monte Carlo Subsurface Scattering"
 * by Jaroslav Křivánek and Eugene d'Eon (SIGGRAPH 2014)
 * https://cgg.mff.cuni.cz/~jaroslav/papers/2014-zerovar/
 *
 * [2] "Improving the Dwivedi Sampling Scheme"
 * by Johannes Meng, Johannes Hanika, and Carsten Dachsbacher (EGSR 2016)
 * https://cg.ivd.kit.edu/1951.php
 *
 * [3] "Zero-Variance Theory for Efficient Subsurface Scattering"
 * by Eugene d'Eon and Jaroslav Křivánek (SIGGRAPH 2020)
 * https://iliyan.com/publications/RenderingCourse2020
 */

ccl_device_forceinline float eval_phase_dwivedi(float v, float phase_log, float cos_theta)
{
  /* Eq. 9 from [2] using precomputed log((v + 1) / (v - 1)) */
  return 1.0f / ((v - cos_theta) * phase_log);
}

ccl_device_forceinline float sample_phase_dwivedi(float v, float phase_log, float rand)
{
  /* Based on Eq. 10 from [2]: `v - (v + 1) * pow((v - 1) / (v + 1), rand)`
   * Since we're already pre-computing `phase_log = log((v + 1) / (v - 1))` for the evaluation,
   * we can implement the power function like this. */
  return v - (v + 1.0f) * expf(-rand * phase_log);
}

ccl_device_forceinline float diffusion_length_dwivedi(float alpha)
{
  /* Eq. 67 from [3] */
  return 1.0f / sqrtf(1.0f - powf(alpha, 2.44294f - 0.0215813f * alpha + 0.578637f / alpha));
}

ccl_device_forceinline float3 direction_from_cosine(float3 D, float cos_theta, float randv)
{
  float sin_theta = sin_from_cos(cos_theta);
  float phi = M_2PI_F * randv;
  float3 dir = make_float3(sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta);

  float3 T, B;
  make_orthonormals(D, &T, &B);
  return dir.x * T + dir.y * B + dir.z * D;
}

ccl_device_forceinline Spectrum subsurface_random_walk_pdf(Spectrum sigma_t,
                                                           float t,
                                                           bool hit,
                                                           ccl_private Spectrum *transmittance)
{
  Spectrum T = volume_color_transmittance(sigma_t, t);
  if (transmittance) {
    *transmittance = T;
  }
  return hit ? T : sigma_t * T;
}

/* Define the below variable to get the similarity code active,
 * and the value represents the cutoff level */
#define SUBSURFACE_RANDOM_WALK_SIMILARITY_LEVEL 9

ccl_device_inline bool subsurface_random_walk(KernelGlobals kg,
                                              IntegratorState state,
                                              RNGState rng_state,
                                              ccl_private Ray &ray,
                                              ccl_private LocalIntersection &ss_isect)
{
  const float3 P = INTEGRATOR_STATE(state, ray, P);
  const float3 D = INTEGRATOR_STATE(state, ray, D);
  const float ray_dP = INTEGRATOR_STATE(state, ray, dP);
  const float time = INTEGRATOR_STATE(state, ray, time);
  const float3 N = INTEGRATOR_STATE(state, subsurface, N);
  const int object = INTEGRATOR_STATE(state, isect, object);
  const int prim = INTEGRATOR_STATE(state, isect, prim);

  /* Setup ray. */
  ray.P = P;
  ray.D = D;
  ray.tmin = 0.0f;
  ray.tmax = FLT_MAX;
  ray.time = time;
  ray.dP = ray_dP;
  ray.dD = differential_zero_compact();
  ray.self.object = object;
  ray.self.prim = prim;
  ray.self.light_object = OBJECT_NONE;
  ray.self.light_prim = PRIM_NONE;
  ray.self.light = LAMP_NONE;

  /* Convert subsurface to volume coefficients.
   * The single-scattering albedo is named alpha to avoid confusion with the surface albedo. */
  const Spectrum albedo = INTEGRATOR_STATE(state, subsurface, albedo);
  const Spectrum radius = INTEGRATOR_STATE(state, subsurface, radius);
  const float anisotropy = INTEGRATOR_STATE(state, subsurface, anisotropy);

  Spectrum sigma_t, alpha;
  Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
  subsurface_random_walk_coefficients(albedo, radius, anisotropy, &sigma_t, &alpha, &throughput);
  Spectrum sigma_s = sigma_t * alpha;

  /* Theoretically it should be better to use the exact alpha for the channel we're sampling at
   * each bounce, but in practice there doesn't seem to be a noticeable difference in exchange
   * for making the code significantly more complex and slower (if direction sampling depends on
   * the sampled channel, we need to compute its PDF per-channel and consider it for MIS later on).
   *
   * Since the strength of the guided sampling increases as alpha gets lower, using a value that
   * is too low results in fireflies while one that's too high just gives a bit more noise.
   * Therefore, the code here uses the highest of the three albedos to be safe. */
  const float diffusion_length = diffusion_length_dwivedi(reduce_max(alpha));

  if (diffusion_length == 1.0f) {
    /* With specific values of alpha the length might become 1, which in asymptotic makes phase to
     * be infinite. After first bounce it will cause throughput to be 0. Do early output, avoiding
     * numerical issues and extra unneeded work. */
    return false;
  }

  /* Precompute term for phase sampling. */
  const float phase_log = logf((diffusion_length + 1.0f) / (diffusion_length - 1.0f));

  /* Modify state for RNGs, decorrelated from other paths. */
  rng_state.rng_hash = hash_hp_seeded_uint(rng_state.rng_hash + rng_state.rng_offset, 0xdeadbeef);

  /* Random walk until we hit the surface again. */
  bool hit = false;
  bool have_opposite_interface = false;
  float opposite_distance = 0.0f;

  /* TODO: Disable for `alpha > 0.999` or so? */
  /* Our heuristic, a compromise between guiding and classic. */
  const float guided_fraction = 1.0f - fmaxf(0.5f, powf(fabsf(anisotropy), 0.125f));

#ifdef SUBSURFACE_RANDOM_WALK_SIMILARITY_LEVEL
  Spectrum sigma_s_star = sigma_s * (1.0f - anisotropy);
  Spectrum sigma_t_star = sigma_t - sigma_s + sigma_s_star;
  Spectrum sigma_t_org = sigma_t;
  Spectrum sigma_s_org = sigma_s;
  const float anisotropy_org = anisotropy;
  const float guided_fraction_org = guided_fraction;
#endif

  for (int bounce = 0; bounce < BSSRDF_MAX_BOUNCES; bounce++) {
    /* Advance random number offset. */
    rng_state.rng_offset += PRNG_BOUNCE_NUM;

#ifdef SUBSURFACE_RANDOM_WALK_SIMILARITY_LEVEL
    // shadow with local variables according to depth
    float anisotropy, guided_fraction;
    Spectrum sigma_s, sigma_t;
    if (bounce <= SUBSURFACE_RANDOM_WALK_SIMILARITY_LEVEL) {
      anisotropy = anisotropy_org;
      guided_fraction = guided_fraction_org;
      sigma_t = sigma_t_org;
      sigma_s = sigma_s_org;
    }
    else {
      anisotropy = 0.0f;
      guided_fraction = 0.75f;  // back to isotropic heuristic from Blender
      sigma_t = sigma_t_star;
      sigma_s = sigma_s_star;
    }
#endif

    /* Sample color channel, use MIS with balance heuristic. */
    float rphase = path_state_rng_1D(kg, &rng_state, PRNG_SUBSURFACE_PHASE_CHANNEL);
    Spectrum channel_pdf;
    int channel = volume_sample_channel(alpha, throughput, rphase, &channel_pdf);
    float sample_sigma_t = volume_channel_get(sigma_t, channel);
    float randt = path_state_rng_1D(kg, &rng_state, PRNG_SUBSURFACE_SCATTER_DISTANCE);

    /* We need the result of the ray-cast to compute the full guided PDF, so just remember the
     * relevant terms to avoid recomputing them later. */
    float backward_fraction = 0.0f;
    float forward_pdf_factor = 0.0f;
    float forward_stretching = 1.0f;
    float backward_pdf_factor = 0.0f;
    float backward_stretching = 1.0f;

    /* For the initial ray, we already know the direction, so just do classic distance sampling. */
    if (bounce > 0) {
      /* Decide whether we should use guided or classic sampling. */
      bool guided = (path_state_rng_1D(kg, &rng_state, PRNG_SUBSURFACE_GUIDE_STRATEGY) <
                     guided_fraction);

      /* Determine if we want to sample away from the incoming interface.
       * This only happens if we found a nearby opposite interface, and the probability for it
       * depends on how close we are to it already.
       * This probability term comes from the recorded presentation of [3]. */
      bool guide_backward = false;
      if (have_opposite_interface) {
        /* Compute distance of the random walk between the tangent plane at the starting point
         * and the assumed opposite interface (the parallel plane that contains the point we
         * found in our ray query for the opposite side). */
        float x = clamp(dot(ray.P - P, -N), 0.0f, opposite_distance);
        backward_fraction = 1.0f /
                            (1.0f + expf((opposite_distance - 2.0f * x) / diffusion_length));
        guide_backward = path_state_rng_1D(kg, &rng_state, PRNG_SUBSURFACE_GUIDE_DIRECTION) <
                         backward_fraction;
      }

      /* Sample scattering direction. */
      const float2 rand_scatter = path_state_rng_2D(kg, &rng_state, PRNG_SUBSURFACE_BSDF);
      float cos_theta;
      float hg_pdf;
      if (guided) {
        cos_theta = sample_phase_dwivedi(diffusion_length, phase_log, rand_scatter.x);
        /* The backwards guiding distribution is just mirrored along `sd->N`, so swapping the
         * sign here is enough to sample from that instead. */
        if (guide_backward) {
          cos_theta = -cos_theta;
        }
        float3 newD = direction_from_cosine(N, cos_theta, rand_scatter.y);
        hg_pdf = single_peaked_henyey_greenstein(dot(ray.D, newD), anisotropy);
        ray.D = newD;
      }
      else {
        float3 newD = henyey_greenstrein_sample(ray.D, anisotropy, rand_scatter, &hg_pdf);
        cos_theta = dot(newD, N);
        ray.D = newD;
      }

      /* Compute PDF factor caused by phase sampling (as the ratio of guided / classic).
       * Since phase sampling is channel-independent, we can get away with applying a factor
       * to the guided PDF, which implicitly means pulling out the classic PDF term and letting
       * it cancel with an equivalent term in the numerator of the full estimator.
       * For the backward PDF, we again reuse the same probability distribution with a sign swap.
       */
      forward_pdf_factor = M_1_2PI_F * eval_phase_dwivedi(diffusion_length, phase_log, cos_theta) /
                           hg_pdf;
      backward_pdf_factor = M_1_2PI_F *
                            eval_phase_dwivedi(diffusion_length, phase_log, -cos_theta) / hg_pdf;

      /* Prepare distance sampling.
       * For the backwards case, this also needs the sign swapped since now directions against
       * `sd->N` (and therefore with negative cos_theta) are preferred. */
      forward_stretching = (1.0f - cos_theta / diffusion_length);
      backward_stretching = (1.0f + cos_theta / diffusion_length);
      if (guided) {
        sample_sigma_t *= guide_backward ? backward_stretching : forward_stretching;
      }
    }

    /* Sample distance along ray. */
    float t = -logf(1.0f - randt) / sample_sigma_t;

    /* On the first bounce, we use the ray-cast to check if the opposite side is nearby.
     * If yes, we will later use backwards guided sampling in order to have a decent
     * chance of connecting to it.
     * TODO: Maybe use less than 10 times the mean free path? */
    if (bounce == 0) {
      ray.tmax = max(t, 10.0f / (reduce_min(sigma_t)));
    }
    else {
      ray.tmax = t;
      /* After the first bounce the object can intersect the same surface again */
      ray.self.object = OBJECT_NONE;
      ray.self.prim = PRIM_NONE;
    }
    scene_intersect_local(kg, &ray, &ss_isect, object, NULL, 1);
    hit = (ss_isect.num_hits > 0);

    if (hit) {
      ray.tmax = ss_isect.hits[0].t;
    }

    if (bounce == 0) {
      /* Check if we hit the opposite side. */
      if (hit) {
        have_opposite_interface = true;
        opposite_distance = dot(ray.P + ray.tmax * ray.D - P, -N);
      }
      /* Apart from the opposite side check, we were supposed to only trace up to distance t,
       * so check if there would have been a hit in that case. */
      hit = ray.tmax < t;
    }

    /* Use the distance to the exit point for the throughput update if we found one. */
    if (hit) {
      t = ray.tmax;
    }

    /* Advance to new scatter location. */
    ray.P += t * ray.D;

    Spectrum transmittance;
    Spectrum pdf = subsurface_random_walk_pdf(sigma_t, t, hit, &transmittance);
    if (bounce > 0) {
      /* Compute PDF just like we do for classic sampling, but with the stretched sigma_t. */
      Spectrum guided_pdf = subsurface_random_walk_pdf(forward_stretching * sigma_t, t, hit, NULL);

      if (have_opposite_interface) {
        /* First step of MIS: Depending on geometry we might have two methods for guided
         * sampling, so perform MIS between them. */
        Spectrum back_pdf = subsurface_random_walk_pdf(
            backward_stretching * sigma_t, t, hit, NULL);
        guided_pdf = mix(
            guided_pdf * forward_pdf_factor, back_pdf * backward_pdf_factor, backward_fraction);
      }
      else {
        /* Just include phase sampling factor otherwise. */
        guided_pdf *= forward_pdf_factor;
      }

      /* Now we apply the MIS balance heuristic between the classic and guided sampling. */
      pdf = mix(pdf, guided_pdf, guided_fraction);
    }

    /* Finally, we're applying MIS again to combine the three color channels.
     * Altogether, the MIS computation combines up to nine different estimators:
     * {classic, guided, backward_guided} x {r, g, b} */
    throughput *= (hit ? transmittance : sigma_s * transmittance) / dot(channel_pdf, pdf);

    if (hit) {
      /* If we hit the surface, we are done. */
      break;
    }
    else if (reduce_max(throughput) < VOLUME_THROUGHPUT_EPSILON) {
      /* Avoid unnecessary work and precision issue when throughput gets really small. */
      break;
    }
  }

  if (hit) {
    kernel_assert(isfinite_safe(throughput));

    /* TODO(lukas): Which PDF should we report here? Entry bounce? The random walk? Just 1.0? */
    guiding_record_bssrdf_bounce(
        kg,
        state,
        1.0f,
        N,
        D,
        safe_divide_color(throughput, INTEGRATOR_STATE(state, path, throughput)),
        albedo);

    INTEGRATOR_STATE_WRITE(state, path, throughput) = throughput;
  }

  return hit;
}

CCL_NAMESPACE_END
