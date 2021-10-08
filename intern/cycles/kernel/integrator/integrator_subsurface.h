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

#include "kernel/kernel_path_state.h"
#include "kernel/kernel_projection.h"
#include "kernel/kernel_shader.h"

#include "kernel/bvh/bvh.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_principled_diffuse.h"
#include "kernel/closure/bssrdf.h"
#include "kernel/closure/volume.h"

#include "kernel/integrator/integrator_intersect_volume_stack.h"

CCL_NAMESPACE_BEGIN

#ifdef __SUBSURFACE__

ccl_device int subsurface_bounce(INTEGRATOR_STATE_ARGS, ShaderData *sd, const ShaderClosure *sc)
{
  /* We should never have two consecutive BSSRDF bounces, the second one should
   * be converted to a diffuse BSDF to avoid this. */
  kernel_assert(!(INTEGRATOR_STATE(path, flag) & PATH_RAY_DIFFUSE_ANCESTOR));

  /* Setup path state for intersect_subsurface kernel. */
  const Bssrdf *bssrdf = (const Bssrdf *)sc;

  /* Setup ray into surface. */
  INTEGRATOR_STATE_WRITE(ray, P) = sd->P;
  INTEGRATOR_STATE_WRITE(ray, D) = sd->N;
  INTEGRATOR_STATE_WRITE(ray, t) = FLT_MAX;
  INTEGRATOR_STATE_WRITE(ray, dP) = differential_make_compact(sd->dP);
  INTEGRATOR_STATE_WRITE(ray, dD) = differential_zero_compact();

  /* Pass along object info, reusing isect to save memory. */
  INTEGRATOR_STATE_WRITE(isect, Ng) = sd->Ng;
  INTEGRATOR_STATE_WRITE(isect, object) = sd->object;

  /* Pass BSSRDF parameters. */
  const uint32_t path_flag = INTEGRATOR_STATE_WRITE(path, flag);
  INTEGRATOR_STATE_WRITE(path, flag) = (path_flag & ~PATH_RAY_CAMERA) | PATH_RAY_SUBSURFACE;
  INTEGRATOR_STATE_WRITE(path, throughput) *= shader_bssrdf_sample_weight(sd, sc);

  /* Advance random number offset for bounce. */
  INTEGRATOR_STATE_WRITE(path, rng_offset) += PRNG_BOUNCE_NUM;

  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_PASSES) {
    if (INTEGRATOR_STATE(path, bounce) == 0) {
      INTEGRATOR_STATE_WRITE(path, diffuse_glossy_ratio) = one_float3();
    }
  }

  INTEGRATOR_STATE_WRITE(subsurface, albedo) = bssrdf->albedo;
  INTEGRATOR_STATE_WRITE(subsurface, radius) = bssrdf->radius;
  INTEGRATOR_STATE_WRITE(subsurface, roughness) = bssrdf->roughness;
  INTEGRATOR_STATE_WRITE(subsurface, anisotropy) = bssrdf->anisotropy;

  return LABEL_SUBSURFACE_SCATTER;
}

ccl_device void subsurface_shader_data_setup(INTEGRATOR_STATE_ARGS, ShaderData *sd)
{
  /* Get bump mapped normal from shader evaluation at exit point. */
  float3 N = sd->N;
  if (sd->flag & SD_HAS_BSSRDF_BUMP) {
    N = shader_bssrdf_normal(sd);
  }

  /* Setup diffuse BSDF at the exit point. This replaces shader_eval_surface. */
  sd->flag &= ~SD_CLOSURE_FLAGS;
  sd->num_closure = 0;
  sd->num_closure_left = kernel_data.max_closures;

  const float3 weight = one_float3();
  const float roughness = INTEGRATOR_STATE(subsurface, roughness);

#  ifdef __PRINCIPLED__
  if (roughness != FLT_MAX) {
    PrincipledDiffuseBsdf *bsdf = (PrincipledDiffuseBsdf *)bsdf_alloc(
        sd, sizeof(PrincipledDiffuseBsdf), weight);

    if (bsdf) {
      bsdf->N = N;
      bsdf->roughness = roughness;
      sd->flag |= bsdf_principled_diffuse_setup(bsdf);

      /* replace CLOSURE_BSDF_PRINCIPLED_DIFFUSE_ID with this special ID so render passes
       * can recognize it as not being a regular Disney principled diffuse closure */
      bsdf->type = CLOSURE_BSDF_BSSRDF_PRINCIPLED_ID;
    }
  }
  else
#  endif /* __PRINCIPLED__ */
  {
    DiffuseBsdf *bsdf = (DiffuseBsdf *)bsdf_alloc(sd, sizeof(DiffuseBsdf), weight);

    if (bsdf) {
      bsdf->N = N;
      sd->flag |= bsdf_diffuse_setup(bsdf);

      /* replace CLOSURE_BSDF_DIFFUSE_ID with this special ID so render passes
       * can recognize it as not being a regular diffuse closure */
      bsdf->type = CLOSURE_BSDF_BSSRDF_ID;
    }
  }
}

/* Random walk subsurface scattering.
 *
 * "Practical and Controllable Subsurface Scattering for Production Path
 *  Tracing". Matt Jen-Yuan Chiang, Peter Kutz, Brent Burley. SIGGRAPH 2016. */

/* Support for anisotropy from:
 * "Path Traced Subsurface Scattering using Anisotropic Phase Functions
 * and Non-Exponential Free Flights".
 * Magnus Wrenninge, Ryusuke Villemin, Christophe Hery.
 * https://graphics.pixar.com/library/PathTracedSubsurface/ */

ccl_device void subsurface_random_walk_remap(
    const float albedo, const float d, float g, float *sigma_t, float *alpha)
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

ccl_device void subsurface_random_walk_coefficients(const float3 albedo,
                                                    const float3 radius,
                                                    const float anisotropy,
                                                    float3 *sigma_t,
                                                    float3 *alpha,
                                                    float3 *throughput)
{
  float sigma_t_x, sigma_t_y, sigma_t_z;
  float alpha_x, alpha_y, alpha_z;

  subsurface_random_walk_remap(albedo.x, radius.x, anisotropy, &sigma_t_x, &alpha_x);
  subsurface_random_walk_remap(albedo.y, radius.y, anisotropy, &sigma_t_y, &alpha_y);
  subsurface_random_walk_remap(albedo.z, radius.z, anisotropy, &sigma_t_z, &alpha_z);

  /* Throughput already contains closure weight at this point, which includes the
   * albedo, as well as closure mixing and Fresnel weights. Divide out the albedo
   * which will be added through scattering. */
  *throughput = safe_divide_color(*throughput, albedo);

  /* With low albedo values (like 0.025) we get diffusion_length 1.0 and
   * infinite phase functions. To avoid a sharp discontinuity as we go from
   * such values to 0.0, increase alpha and reduce the throughput to compensate. */
  const float min_alpha = 0.2f;
  if (alpha_x < min_alpha) {
    (*throughput).x *= alpha_x / min_alpha;
    alpha_x = min_alpha;
  }
  if (alpha_y < min_alpha) {
    (*throughput).y *= alpha_y / min_alpha;
    alpha_y = min_alpha;
  }
  if (alpha_z < min_alpha) {
    (*throughput).z *= alpha_z / min_alpha;
    alpha_z = min_alpha;
  }

  *sigma_t = make_float3(sigma_t_x, sigma_t_y, sigma_t_z);
  *alpha = make_float3(alpha_x, alpha_y, alpha_z);
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
  float sin_theta = safe_sqrtf(1.0f - cos_theta * cos_theta);
  float phi = M_2PI_F * randv;
  float3 dir = make_float3(sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta);

  float3 T, B;
  make_orthonormals(D, &T, &B);
  return dir.x * T + dir.y * B + dir.z * D;
}

ccl_device_forceinline float3 subsurface_random_walk_pdf(float3 sigma_t,
                                                         float t,
                                                         bool hit,
                                                         float3 *transmittance)
{
  float3 T = volume_color_transmittance(sigma_t, t);
  if (transmittance) {
    *transmittance = T;
  }
  return hit ? T : sigma_t * T;
}

/* Define the below variable to get the similarity code active,
 * and the value represents the cutoff level */
#  define SUBSURFACE_RANDOM_WALK_SIMILARITY_LEVEL 9

ccl_device_inline bool subsurface_random_walk(INTEGRATOR_STATE_ARGS,
                                              RNGState rng_state,
                                              Ray &ray,
                                              LocalIntersection &ss_isect)
{
  float bssrdf_u, bssrdf_v;
  path_state_rng_2D(kg, &rng_state, PRNG_BSDF_U, &bssrdf_u, &bssrdf_v);

  const float3 P = INTEGRATOR_STATE(ray, P);
  const float3 N = INTEGRATOR_STATE(ray, D);
  const float ray_dP = INTEGRATOR_STATE(ray, dP);
  const float time = INTEGRATOR_STATE(ray, time);
  const float3 Ng = INTEGRATOR_STATE(isect, Ng);
  const int object = INTEGRATOR_STATE(isect, object);

  /* Sample diffuse surface scatter into the object. */
  float3 D;
  float pdf;
  sample_cos_hemisphere(-N, bssrdf_u, bssrdf_v, &D, &pdf);
  if (dot(-Ng, D) <= 0.0f) {
    return false;
  }

  /* Setup ray. */
  ray.P = ray_offset(P, -Ng);
  ray.D = D;
  ray.t = FLT_MAX;
  ray.time = time;
  ray.dP = ray_dP;
  ray.dD = differential_zero_compact();

#  ifndef __KERNEL_OPTIX__
  /* Compute or fetch object transforms. */
  Transform ob_itfm ccl_optional_struct_init;
  Transform ob_tfm = object_fetch_transform_motion_test(kg, object, time, &ob_itfm);
#  endif

  /* Convert subsurface to volume coefficients.
   * The single-scattering albedo is named alpha to avoid confusion with the surface albedo. */
  const float3 albedo = INTEGRATOR_STATE(subsurface, albedo);
  const float3 radius = INTEGRATOR_STATE(subsurface, radius);
  const float anisotropy = INTEGRATOR_STATE(subsurface, anisotropy);

  float3 sigma_t, alpha;
  float3 throughput = INTEGRATOR_STATE_WRITE(path, throughput);
  subsurface_random_walk_coefficients(albedo, radius, anisotropy, &sigma_t, &alpha, &throughput);
  float3 sigma_s = sigma_t * alpha;

  /* Theoretically it should be better to use the exact alpha for the channel we're sampling at
   * each bounce, but in practice there doesn't seem to be a noticeable difference in exchange
   * for making the code significantly more complex and slower (if direction sampling depends on
   * the sampled channel, we need to compute its PDF per-channel and consider it for MIS later on).
   *
   * Since the strength of the guided sampling increases as alpha gets lower, using a value that
   * is too low results in fireflies while one that's too high just gives a bit more noise.
   * Therefore, the code here uses the highest of the three albedos to be safe. */
  const float diffusion_length = diffusion_length_dwivedi(max3(alpha));

  if (diffusion_length == 1.0f) {
    /* With specific values of alpha the length might become 1, which in asymptotic makes phase to
     * be infinite. After first bounce it will cause throughput to be 0. Do early output, avoiding
     * numerical issues and extra unneeded work. */
    return false;
  }

  /* Precompute term for phase sampling. */
  const float phase_log = logf((diffusion_length + 1.0f) / (diffusion_length - 1.0f));

  /* Modify state for RNGs, decorrelated from other paths. */
  rng_state.rng_hash = cmj_hash(rng_state.rng_hash + rng_state.rng_offset, 0xdeadbeef);

  /* Random walk until we hit the surface again. */
  bool hit = false;
  bool have_opposite_interface = false;
  float opposite_distance = 0.0f;

  /* Todo: Disable for alpha>0.999 or so? */
  /* Our heuristic, a compromise between guiding and classic. */
  const float guided_fraction = 1.0f - fmaxf(0.5f, powf(fabsf(anisotropy), 0.125f));

#  ifdef SUBSURFACE_RANDOM_WALK_SIMILARITY_LEVEL
  float3 sigma_s_star = sigma_s * (1.0f - anisotropy);
  float3 sigma_t_star = sigma_t - sigma_s + sigma_s_star;
  float3 sigma_t_org = sigma_t;
  float3 sigma_s_org = sigma_s;
  const float anisotropy_org = anisotropy;
  const float guided_fraction_org = guided_fraction;
#  endif

  for (int bounce = 0; bounce < BSSRDF_MAX_BOUNCES; bounce++) {
    /* Advance random number offset. */
    rng_state.rng_offset += PRNG_BOUNCE_NUM;

#  ifdef SUBSURFACE_RANDOM_WALK_SIMILARITY_LEVEL
    // shadow with local variables according to depth
    float anisotropy, guided_fraction;
    float3 sigma_s, sigma_t;
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
#  endif

    /* Sample color channel, use MIS with balance heuristic. */
    float rphase = path_state_rng_1D(kg, &rng_state, PRNG_PHASE_CHANNEL);
    float3 channel_pdf;
    int channel = volume_sample_channel(alpha, throughput, rphase, &channel_pdf);
    float sample_sigma_t = volume_channel_get(sigma_t, channel);
    float randt = path_state_rng_1D(kg, &rng_state, PRNG_SCATTER_DISTANCE);

    /* We need the result of the raycast to compute the full guided PDF, so just remember the
     * relevant terms to avoid recomputing them later. */
    float backward_fraction = 0.0f;
    float forward_pdf_factor = 0.0f;
    float forward_stretching = 1.0f;
    float backward_pdf_factor = 0.0f;
    float backward_stretching = 1.0f;

    /* For the initial ray, we already know the direction, so just do classic distance sampling. */
    if (bounce > 0) {
      /* Decide whether we should use guided or classic sampling. */
      bool guided = (path_state_rng_1D(kg, &rng_state, PRNG_LIGHT_TERMINATE) < guided_fraction);

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
        guide_backward = path_state_rng_1D(kg, &rng_state, PRNG_TERMINATE) < backward_fraction;
      }

      /* Sample scattering direction. */
      float scatter_u, scatter_v;
      path_state_rng_2D(kg, &rng_state, PRNG_BSDF_U, &scatter_u, &scatter_v);
      float cos_theta;
      float hg_pdf;
      if (guided) {
        cos_theta = sample_phase_dwivedi(diffusion_length, phase_log, scatter_u);
        /* The backwards guiding distribution is just mirrored along sd->N, so swapping the
         * sign here is enough to sample from that instead. */
        if (guide_backward) {
          cos_theta = -cos_theta;
        }
        float3 newD = direction_from_cosine(N, cos_theta, scatter_v);
        hg_pdf = single_peaked_henyey_greenstein(dot(ray.D, newD), anisotropy);
        ray.D = newD;
      }
      else {
        float3 newD = henyey_greenstrein_sample(ray.D, anisotropy, scatter_u, scatter_v, &hg_pdf);
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
       * sd->N (and therefore with negative cos_theta) are preferred. */
      forward_stretching = (1.0f - cos_theta / diffusion_length);
      backward_stretching = (1.0f + cos_theta / diffusion_length);
      if (guided) {
        sample_sigma_t *= guide_backward ? backward_stretching : forward_stretching;
      }
    }

    /* Sample direction along ray. */
    float t = -logf(1.0f - randt) / sample_sigma_t;

    /* On the first bounce, we use the raycast to check if the opposite side is nearby.
     * If yes, we will later use backwards guided sampling in order to have a decent
     * chance of connecting to it.
     * Todo: Maybe use less than 10 times the mean free path? */
    ray.t = (bounce == 0) ? max(t, 10.0f / (min3(sigma_t))) : t;
    scene_intersect_local(kg, &ray, &ss_isect, object, NULL, 1);
    hit = (ss_isect.num_hits > 0);

    if (hit) {
#  ifdef __KERNEL_OPTIX__
      /* t is always in world space with OptiX. */
      ray.t = ss_isect.hits[0].t;
#  else
      /* Compute world space distance to surface hit. */
      float3 D = transform_direction(&ob_itfm, ray.D);
      D = normalize(D) * ss_isect.hits[0].t;
      ray.t = len(transform_direction(&ob_tfm, D));
#  endif
    }

    if (bounce == 0) {
      /* Check if we hit the opposite side. */
      if (hit) {
        have_opposite_interface = true;
        opposite_distance = dot(ray.P + ray.t * ray.D - P, -N);
      }
      /* Apart from the opposite side check, we were supposed to only trace up to distance t,
       * so check if there would have been a hit in that case. */
      hit = ray.t < t;
    }

    /* Use the distance to the exit point for the throughput update if we found one. */
    if (hit) {
      t = ray.t;
    }
    else if (bounce == 0) {
      /* Restore original position if nothing was hit after the first bounce,
       * without the ray_offset() that was added to avoid self-intersection.
       * Otherwise if that offset is relatively large compared to the scattering
       * radius, we never go back up high enough to exit the surface. */
      ray.P = P;
    }

    /* Advance to new scatter location. */
    ray.P += t * ray.D;

    float3 transmittance;
    float3 pdf = subsurface_random_walk_pdf(sigma_t, t, hit, &transmittance);
    if (bounce > 0) {
      /* Compute PDF just like we do for classic sampling, but with the stretched sigma_t. */
      float3 guided_pdf = subsurface_random_walk_pdf(forward_stretching * sigma_t, t, hit, NULL);

      if (have_opposite_interface) {
        /* First step of MIS: Depending on geometry we might have two methods for guided
         * sampling, so perform MIS between them. */
        float3 back_pdf = subsurface_random_walk_pdf(backward_stretching * sigma_t, t, hit, NULL);
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
    else if (throughput.x < VOLUME_THROUGHPUT_EPSILON &&
             throughput.y < VOLUME_THROUGHPUT_EPSILON &&
             throughput.z < VOLUME_THROUGHPUT_EPSILON) {
      /* Avoid unnecessary work and precision issue when throughput gets really small. */
      break;
    }
  }

  if (hit) {
    kernel_assert(isfinite3_safe(throughput));
    INTEGRATOR_STATE_WRITE(path, throughput) = throughput;
  }

  return hit;
}

ccl_device_inline bool subsurface_scatter(INTEGRATOR_STATE_ARGS)
{
  RNGState rng_state;
  path_state_rng_load(INTEGRATOR_STATE_PASS, &rng_state);

  Ray ray ccl_optional_struct_init;
  LocalIntersection ss_isect ccl_optional_struct_init;

  if (!subsurface_random_walk(INTEGRATOR_STATE_PASS, rng_state, ray, ss_isect)) {
    return false;
  }

#  ifdef __VOLUME__
  /* Update volume stack if needed. */
  if (kernel_data.integrator.use_volumes) {
    const int object = ss_isect.hits[0].object;
    const int object_flag = kernel_tex_fetch(__object_flag, object);

    if (object_flag & SD_OBJECT_INTERSECTS_VOLUME) {
      float3 P = INTEGRATOR_STATE(ray, P);
      const float3 Ng = INTEGRATOR_STATE(isect, Ng);
      const float3 offset_P = ray_offset(P, -Ng);

      integrator_volume_stack_update_for_subsurface(INTEGRATOR_STATE_PASS, offset_P, ray.P);
    }
  }
#  endif /* __VOLUME__ */

  /* Pretend ray is coming from the outside towards the exit point. This ensures
   * correct front/back facing normals.
   * TODO: find a more elegant solution? */
  ray.P += ray.D * ray.t * 2.0f;
  ray.D = -ray.D;

  integrator_state_write_isect(INTEGRATOR_STATE_PASS, &ss_isect.hits[0]);
  integrator_state_write_ray(INTEGRATOR_STATE_PASS, &ray);

  /* Advance random number offset for bounce. */
  INTEGRATOR_STATE_WRITE(path, rng_offset) += PRNG_BOUNCE_NUM;

  const int shader = intersection_get_shader(kg, &ss_isect.hits[0]);
  const int shader_flags = kernel_tex_fetch(__shaders, shader).flags;
  if ((shader_flags & SD_HAS_RAYTRACE) || (kernel_data.film.pass_ao != PASS_UNUSED)) {
    INTEGRATOR_PATH_NEXT_SORTED(DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE,
                                DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE,
                                shader);
  }
  else {
    INTEGRATOR_PATH_NEXT_SORTED(DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE,
                                DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE,
                                shader);
  }

  return true;
}

#endif /* __SUBSURFACE__ */

CCL_NAMESPACE_END
