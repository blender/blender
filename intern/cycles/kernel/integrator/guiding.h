/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf.h"
#include "kernel/film/write.h"

CCL_NAMESPACE_BEGIN

/* Utilities. */

struct GuidingRISSample {
  float3 rand;
  float2 sampled_roughness;
  float eta{1.0f};
  int label;
  float3 wo;
  float bsdf_pdf{0.0f};
  float guide_pdf{0.0f};
  float ris_target{0.0f};
  float ris_pdf{0.0f};
  float ris_weight{0.0f};

  float incoming_radiance_pdf{0.0f};
  BsdfEval bsdf_eval;
  float avg_bsdf_eval{0.0f};
  Spectrum eval{zero_spectrum()};
};

ccl_device_forceinline bool calculate_ris_target(ccl_private GuidingRISSample *ris_sample,
                                                 ccl_private const float guiding_sampling_prob)
{
#if defined(__PATH_GUIDING__)
  const float pi_factor = 2.0f;
  if (ris_sample->avg_bsdf_eval > 0.0f && ris_sample->bsdf_pdf > 1e-10f &&
      ris_sample->guide_pdf > 0.0f)
  {
    ris_sample->ris_target = (ris_sample->avg_bsdf_eval *
                              ((((1.0f - guiding_sampling_prob) * (1.0f / (pi_factor * M_PI_F))) +
                                (guiding_sampling_prob * ris_sample->incoming_radiance_pdf))));
    ris_sample->ris_pdf = (0.5f * (ris_sample->bsdf_pdf + ris_sample->guide_pdf));
    ris_sample->ris_weight = ris_sample->ris_target / ris_sample->ris_pdf;
    return true;
  }
  ris_sample->ris_target = 0.0f;
  ris_sample->ris_pdf = 0.0f;
  return false;
#else
  return false;
#endif
}

#if defined(__PATH_GUIDING__)
static pgl_vec3f guiding_vec3f(const float3 v)
{
  return openpgl::cpp::Vector3(v.x, v.y, v.z);
}

static pgl_point3f guiding_point3f(const float3 v)
{
  return openpgl::cpp::Point3(v.x, v.y, v.z);
}
#endif

/* Path recording for guiding. */

/* Record Surface Interactions */

/* Records/Adds a new path segment with the current path vertex on a surface.
 * If the path is not terminated this call is usually followed by a call of
 * guiding_record_surface_bounce. */
ccl_device_forceinline void guiding_record_surface_segment(KernelGlobals kg,
                                                           IntegratorState state,
                                                           ccl_private const ShaderData *sd)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }

  const pgl_vec3f zero = guiding_vec3f(zero_float3());
  const pgl_vec3f one = guiding_vec3f(one_float3());

  state->guiding.path_segment = kg->opgl_path_segment_storage->NextSegment();
  openpgl::cpp::SetPosition(state->guiding.path_segment, guiding_point3f(sd->P));
  openpgl::cpp::SetDirectionOut(state->guiding.path_segment, guiding_vec3f(sd->wi));
  openpgl::cpp::SetVolumeScatter(state->guiding.path_segment, false);
  openpgl::cpp::SetScatteredContribution(state->guiding.path_segment, zero);
  openpgl::cpp::SetDirectContribution(state->guiding.path_segment, zero);
  openpgl::cpp::SetTransmittanceWeight(state->guiding.path_segment, one);
  openpgl::cpp::SetEta(state->guiding.path_segment, 1.0);
#endif
}

/* Records the surface scattering event at the current vertex position of the segment. */
ccl_device_forceinline void guiding_record_surface_bounce(KernelGlobals kg,
                                                          IntegratorState state,
                                                          ccl_private const ShaderData *sd,
                                                          const Spectrum weight,
                                                          const float pdf,
                                                          const float3 N,
                                                          const float3 wo,
                                                          const float2 roughness,
                                                          const float eta)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  if (!kernel_data.integrator.train_guiding) {
    return;
  }
  const float min_roughness = safe_sqrtf(fminf(roughness.x, roughness.y));
  const bool is_delta = (min_roughness == 0.0f);
  const float3 weight_rgb = spectrum_to_rgb(weight);
  const float3 normal = clamp(N, -one_float3(), one_float3());

  kernel_assert(state->guiding.path_segment != nullptr);

  openpgl::cpp::SetTransmittanceWeight(state->guiding.path_segment, guiding_vec3f(one_float3()));
  openpgl::cpp::SetVolumeScatter(state->guiding.path_segment, false);
  openpgl::cpp::SetNormal(state->guiding.path_segment, guiding_vec3f(normal));
  openpgl::cpp::SetDirectionIn(state->guiding.path_segment, guiding_vec3f(wo));
  openpgl::cpp::SetPDFDirectionIn(state->guiding.path_segment, pdf);
  openpgl::cpp::SetScatteringWeight(state->guiding.path_segment, guiding_vec3f(weight_rgb));
  openpgl::cpp::SetIsDelta(state->guiding.path_segment, is_delta);
  openpgl::cpp::SetEta(state->guiding.path_segment, eta);
  openpgl::cpp::SetRoughness(state->guiding.path_segment, min_roughness);
#endif
}

/* Records the emission at the current surface intersection (physical or virtual) */
ccl_device_forceinline void guiding_record_surface_emission(KernelGlobals kg,
                                                            IntegratorState state,
                                                            const Spectrum Le,
                                                            const float mis_weight)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }
  const float3 Le_rgb = spectrum_to_rgb(Le);

  openpgl::cpp::SetDirectContribution(state->guiding.path_segment, guiding_vec3f(Le_rgb));
  openpgl::cpp::SetMiWeight(state->guiding.path_segment, mis_weight);
#endif
}

/* Record BSSRDF Interactions */

/* Records/Adds a new path segment where the vertex position is the point of entry
 * of the sub surface scattering boundary.
 * If the path is not terminated this call is usually followed by a call of
 * guiding_record_bssrdf_weight and guiding_record_bssrdf_bounce. */
ccl_device_forceinline void guiding_record_bssrdf_segment(KernelGlobals kg,
                                                          IntegratorState state,
                                                          const float3 P,
                                                          const float3 wi)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }
  const pgl_vec3f zero = guiding_vec3f(zero_float3());
  const pgl_vec3f one = guiding_vec3f(one_float3());

  state->guiding.path_segment = kg->opgl_path_segment_storage->NextSegment();
  openpgl::cpp::SetPosition(state->guiding.path_segment, guiding_point3f(P));
  openpgl::cpp::SetDirectionOut(state->guiding.path_segment, guiding_vec3f(wi));
  openpgl::cpp::SetVolumeScatter(state->guiding.path_segment, true);
  openpgl::cpp::SetScatteredContribution(state->guiding.path_segment, zero);
  openpgl::cpp::SetDirectContribution(state->guiding.path_segment, zero);
  openpgl::cpp::SetTransmittanceWeight(state->guiding.path_segment, one);
  openpgl::cpp::SetEta(state->guiding.path_segment, 1.0);
#endif
}

/* Records the transmission of the path at the point of entry while passing
 * the surface boundary. */
ccl_device_forceinline void guiding_record_bssrdf_weight(KernelGlobals kg,
                                                         IntegratorState state,
                                                         const Spectrum weight,
                                                         const Spectrum albedo)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }

  /* Note albedo left out here, will be included in guiding_record_bssrdf_bounce. */
  const float3 weight_rgb = spectrum_to_rgb(safe_divide_color(weight, albedo));

  kernel_assert(state->guiding.path_segment != nullptr);

  openpgl::cpp::SetTransmittanceWeight(state->guiding.path_segment, guiding_vec3f(zero_float3()));
  openpgl::cpp::SetScatteringWeight(state->guiding.path_segment, guiding_vec3f(weight_rgb));
  openpgl::cpp::SetIsDelta(state->guiding.path_segment, false);
  openpgl::cpp::SetEta(state->guiding.path_segment, 1.0f);
  openpgl::cpp::SetRoughness(state->guiding.path_segment, 1.0f);
#endif
}

/* Records the direction at the point of entry the path takes when sampling the SSS contribution.
 * If not terminated this function is usually followed by a call of
 * guiding_record_volume_transmission to record the transmittance between the point of entry and
 * the point of exit. */
ccl_device_forceinline void guiding_record_bssrdf_bounce(KernelGlobals kg,
                                                         IntegratorState state,
                                                         const float pdf,
                                                         const float3 N,
                                                         const float3 wo,
                                                         const Spectrum weight,
                                                         const Spectrum albedo)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }
  const float3 normal = clamp(N, -one_float3(), one_float3());
  const float3 weight_rgb = spectrum_to_rgb(weight * albedo);

  kernel_assert(state->guiding.path_segment != nullptr);

  openpgl::cpp::SetVolumeScatter(state->guiding.path_segment, false);
  openpgl::cpp::SetNormal(state->guiding.path_segment, guiding_vec3f(normal));
  openpgl::cpp::SetDirectionIn(state->guiding.path_segment, guiding_vec3f(wo));
  openpgl::cpp::SetPDFDirectionIn(state->guiding.path_segment, pdf);
  openpgl::cpp::SetTransmittanceWeight(state->guiding.path_segment, guiding_vec3f(weight_rgb));
#endif
}

/* Record Volume Interactions */

/* Records/Adds a new path segment with the current path vertex being inside a volume.
 * If the path is not terminated this call is usually followed by a call of
 * guiding_record_volume_bounce. */
ccl_device_forceinline void guiding_record_volume_segment(KernelGlobals kg,
                                                          IntegratorState state,
                                                          const float3 P,
                                                          const float3 I)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }
  const pgl_vec3f zero = guiding_vec3f(zero_float3());
  const pgl_vec3f one = guiding_vec3f(one_float3());

  state->guiding.path_segment = kg->opgl_path_segment_storage->NextSegment();

  openpgl::cpp::SetPosition(state->guiding.path_segment, guiding_point3f(P));
  openpgl::cpp::SetDirectionOut(state->guiding.path_segment, guiding_vec3f(I));
  openpgl::cpp::SetVolumeScatter(state->guiding.path_segment, true);
  openpgl::cpp::SetScatteredContribution(state->guiding.path_segment, zero);
  openpgl::cpp::SetDirectContribution(state->guiding.path_segment, zero);
  openpgl::cpp::SetTransmittanceWeight(state->guiding.path_segment, one);
  openpgl::cpp::SetEta(state->guiding.path_segment, 1.0);
#endif
}

/* Records the volume scattering event at the current vertex position of the segment. */
ccl_device_forceinline void guiding_record_volume_bounce(KernelGlobals kg,
                                                         IntegratorState state,
                                                         ccl_private const ShaderData *sd,
                                                         const Spectrum weight,
                                                         const float pdf,
                                                         const float3 wo,
                                                         const float roughness)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  if (!kernel_data.integrator.train_guiding) {
    return;
  }
  const float3 weight_rgb = spectrum_to_rgb(weight);
  const float3 normal = make_float3(0.0f, 0.0f, 1.0f);

  kernel_assert(state->guiding.path_segment != nullptr);

  openpgl::cpp::SetVolumeScatter(state->guiding.path_segment, true);
  openpgl::cpp::SetTransmittanceWeight(state->guiding.path_segment, guiding_vec3f(one_float3()));
  openpgl::cpp::SetNormal(state->guiding.path_segment, guiding_vec3f(normal));
  openpgl::cpp::SetDirectionIn(state->guiding.path_segment, guiding_vec3f(wo));
  openpgl::cpp::SetPDFDirectionIn(state->guiding.path_segment, pdf);
  openpgl::cpp::SetScatteringWeight(state->guiding.path_segment, guiding_vec3f(weight_rgb));
  openpgl::cpp::SetIsDelta(state->guiding.path_segment, false);
  openpgl::cpp::SetEta(state->guiding.path_segment, 1.0f);
  openpgl::cpp::SetRoughness(state->guiding.path_segment, roughness);
#endif
}

/* Records the transmission (a.k.a. transmittance weight) between the current path segment
 * and the next one, when the path is inside or passes a volume. */
ccl_device_forceinline void guiding_record_volume_transmission(KernelGlobals kg,
                                                               IntegratorState state,
                                                               const float3 transmittance_weight)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }

  if (state->guiding.path_segment) {
    // TODO (sherholz): need to find a better way to avoid this check
    if ((transmittance_weight[0] < 0.0f || !std::isfinite(transmittance_weight[0]) ||
         std::isnan(transmittance_weight[0])) ||
        (transmittance_weight[1] < 0.0f || !std::isfinite(transmittance_weight[1]) ||
         std::isnan(transmittance_weight[1])) ||
        (transmittance_weight[2] < 0.0f || !std::isfinite(transmittance_weight[2]) ||
         std::isnan(transmittance_weight[2])))
    {
    }
    else {
      openpgl::cpp::SetTransmittanceWeight(state->guiding.path_segment,
                                           guiding_vec3f(transmittance_weight));
    }
  }
#endif
}

/* Records the emission of a volume at the vertex of the current path segment. */
ccl_device_forceinline void guiding_record_volume_emission(KernelGlobals kg,
                                                           IntegratorState state,
                                                           const Spectrum Le)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }

  if (state->guiding.path_segment) {
    const float3 Le_rgb = spectrum_to_rgb(Le);

    openpgl::cpp::SetDirectContribution(state->guiding.path_segment, guiding_vec3f(Le_rgb));
    openpgl::cpp::SetMiWeight(state->guiding.path_segment, 1.0f);
  }
#endif
}

/* Record Light Interactions */

/* Adds a pseudo path vertex/segment when intersecting a virtual light source.
 * (e.g., area, sphere, or disk light). This call is often followed
 * a call of guiding_record_surface_emission, if the intersected light source
 * emits light in the direction of the path. */
ccl_device_forceinline void guiding_record_light_surface_segment(
    KernelGlobals kg, IntegratorState state, ccl_private const Intersection *ccl_restrict isect)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }
  const pgl_vec3f zero = guiding_vec3f(zero_float3());
  const pgl_vec3f one = guiding_vec3f(one_float3());
  const float3 ray_P = INTEGRATOR_STATE(state, ray, P);
  const float3 ray_D = INTEGRATOR_STATE(state, ray, D);
  const float3 P = ray_P + isect->t * ray_D;

  state->guiding.path_segment = kg->opgl_path_segment_storage->NextSegment();
  openpgl::cpp::SetPosition(state->guiding.path_segment, guiding_point3f(P));
  openpgl::cpp::SetDirectionOut(state->guiding.path_segment, guiding_vec3f(-ray_D));
  openpgl::cpp::SetNormal(state->guiding.path_segment, guiding_vec3f(-ray_D));
  openpgl::cpp::SetDirectionIn(state->guiding.path_segment, guiding_vec3f(ray_D));
  openpgl::cpp::SetPDFDirectionIn(state->guiding.path_segment, 1.0f);
  openpgl::cpp::SetVolumeScatter(state->guiding.path_segment, false);
  openpgl::cpp::SetScatteredContribution(state->guiding.path_segment, zero);
  openpgl::cpp::SetDirectContribution(state->guiding.path_segment, zero);
  openpgl::cpp::SetTransmittanceWeight(state->guiding.path_segment, one);
  openpgl::cpp::SetScatteringWeight(state->guiding.path_segment, one);
  openpgl::cpp::SetEta(state->guiding.path_segment, 1.0f);
#endif
}

/* Records/Adds a final path segment when the path leaves the scene and
 * intersects with a background light (e.g., background color,
 * distant light, or env map). The vertex for this segment is placed along
 * the current ray far out the scene. */
ccl_device_forceinline void guiding_record_background(KernelGlobals kg,
                                                      IntegratorState state,
                                                      const Spectrum L,
                                                      const float mis_weight)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }

  const float3 L_rgb = spectrum_to_rgb(L);
  const float3 ray_P = INTEGRATOR_STATE(state, ray, P);
  const float3 ray_D = INTEGRATOR_STATE(state, ray, D);
  const float3 P = ray_P + (1e6f) * ray_D;
  const float3 normal = make_float3(0.0f, 0.0f, 1.0f);

  openpgl::cpp::PathSegment background_segment;
  openpgl::cpp::SetPosition(&background_segment, guiding_vec3f(P));
  openpgl::cpp::SetNormal(&background_segment, guiding_vec3f(normal));
  openpgl::cpp::SetDirectionOut(&background_segment, guiding_vec3f(-ray_D));
  openpgl::cpp::SetDirectContribution(&background_segment, guiding_vec3f(L_rgb));
  openpgl::cpp::SetMiWeight(&background_segment, mis_weight);
  kg->opgl_path_segment_storage->AddSegment(background_segment);
#endif
}

/* Records direct lighting from either next event estimation or a dedicated BSDF
 * sampled shadow ray. */
ccl_device_forceinline void guiding_record_direct_light(KernelGlobals kg,
                                                        IntegratorShadowState state)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }
  if (state->shadow_path.path_segment) {
    const Spectrum Lo = safe_divide_color(INTEGRATOR_STATE(state, shadow_path, throughput),
                                          INTEGRATOR_STATE(state, shadow_path, unlit_throughput));

    const float3 Lo_rgb = spectrum_to_rgb(Lo);

    const float mis_weight = INTEGRATOR_STATE(state, shadow_path, guiding_mis_weight);

    if (mis_weight == 0.0f) {
      /* Scattered contribution of a next event estimation (i.e., a direct light estimate
       * scattered at the current path vertex towards the previous vertex). */
      openpgl::cpp::AddScatteredContribution(state->shadow_path.path_segment,
                                             guiding_vec3f(Lo_rgb));
    }
    else {
      /* Dedicated shadow ray for BSDF sampled ray direction.
       * The mis weight was already folded into the throughput, so need to divide it out. */
      openpgl::cpp::SetDirectContribution(state->shadow_path.path_segment,
                                          guiding_vec3f(Lo_rgb / mis_weight));
      openpgl::cpp::SetMiWeight(state->shadow_path.path_segment, mis_weight);
    }
  }
#endif
}

/* Record Russian Roulette */
/* Records the probability of continuing the path at the current path segment. */
ccl_device_forceinline void guiding_record_continuation_probability(
    KernelGlobals kg, IntegratorState state, const float continuation_probability)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 1
  if (!kernel_data.integrator.train_guiding) {
    return;
  }

  if (state->guiding.path_segment) {
    openpgl::cpp::SetRussianRouletteProbability(state->guiding.path_segment,
                                                continuation_probability);
  }
#endif
}

/* Path guiding debug render passes. */

/* Write a set of path guiding related debug information (e.g., guiding probability at first
 * bounce) into separate rendering passes. */
ccl_device_forceinline void guiding_write_debug_passes(KernelGlobals kg,
                                                       IntegratorState state,
                                                       ccl_private const ShaderData *sd,
                                                       ccl_global float *ccl_restrict
                                                           render_buffer)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
#  ifdef WITH_CYCLES_DEBUG
  if (!kernel_data.integrator.train_guiding) {
    return;
  }

  if (INTEGRATOR_STATE(state, path, bounce) != 0) {
    return;
  }

  const uint32_t render_pixel_index = INTEGRATOR_STATE(state, path, render_pixel_index);
  const uint64_t render_buffer_offset = (uint64_t)render_pixel_index *
                                        kernel_data.film.pass_stride;
  ccl_global float *buffer = render_buffer + render_buffer_offset;

  if (kernel_data.film.pass_guiding_probability != PASS_UNUSED) {
    float guiding_prob = state->guiding.surface_guiding_sampling_prob;
    film_write_pass_float(buffer + kernel_data.film.pass_guiding_probability, guiding_prob);
  }

  if (kernel_data.film.pass_guiding_avg_roughness != PASS_UNUSED) {
    float avg_roughness = 0.0f;
    float sum_sample_weight = 0.0f;
    for (int i = 0; i < sd->num_closure; i++) {
      ccl_private const ShaderClosure *sc = &sd->closure[i];

      if (!CLOSURE_IS_BSDF_OR_BSSRDF(sc->type)) {
        continue;
      }
      avg_roughness += sc->sample_weight * bsdf_get_specular_roughness_squared(sc);
      sum_sample_weight += sc->sample_weight;
    }

    avg_roughness = avg_roughness > 0.0f ? avg_roughness / sum_sample_weight : 0.0f;

    film_write_pass_float(buffer + kernel_data.film.pass_guiding_avg_roughness, avg_roughness);
  }
#  endif
#endif
}

/* Guided BSDFs */

ccl_device_forceinline bool guiding_bsdf_init(KernelGlobals kg,
                                              IntegratorState state,
                                              const float3 P,
                                              const float3 N,
                                              ccl_private float &rand)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  if (kg->opgl_surface_sampling_distribution->Init(
          kg->opgl_guiding_field, guiding_point3f(P), rand)) {
    kg->opgl_surface_sampling_distribution->ApplyCosineProduct(guiding_point3f(N));
    return true;
  }
#endif

  return false;
}

ccl_device_forceinline float guiding_bsdf_sample(KernelGlobals kg,
                                                 IntegratorState state,
                                                 const float2 rand_bsdf,
                                                 ccl_private float3 *wo)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  pgl_vec3f pgl_wo;
  const pgl_point2f rand = openpgl::cpp::Point2(rand_bsdf.x, rand_bsdf.y);
  const float pdf = kg->opgl_surface_sampling_distribution->SamplePDF(rand, pgl_wo);
  *wo = make_float3(pgl_wo.x, pgl_wo.y, pgl_wo.z);
  return pdf;
#else
  return 0.0f;
#endif
}

ccl_device_forceinline float guiding_bsdf_pdf(KernelGlobals kg,
                                              IntegratorState state,
                                              const float3 wo)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  return kg->opgl_surface_sampling_distribution->PDF(guiding_vec3f(wo));
#else
  return 0.0f;
#endif
}

ccl_device_forceinline float guiding_surface_incoming_radiance_pdf(KernelGlobals kg,
                                                                   IntegratorState state,
                                                                   const float3 wo)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  return kg->opgl_surface_sampling_distribution->IncomingRadiancePDF(guiding_vec3f(wo));
#else
  return 0.0f;
#endif
}

/* Guided Volume Phases */

ccl_device_forceinline bool guiding_phase_init(KernelGlobals kg,
                                               IntegratorState state,
                                               const float3 P,
                                               const float3 D,
                                               const float g,
                                               ccl_private float &rand)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  /* we do not need to guide almost delta phase functions */
  if (fabsf(g) >= 0.99f) {
    return false;
  }

  if (kg->opgl_volume_sampling_distribution->Init(
          kg->opgl_guiding_field, guiding_point3f(P), rand)) {
    kg->opgl_volume_sampling_distribution->ApplySingleLobeHenyeyGreensteinProduct(guiding_vec3f(D),
                                                                                  g);
    return true;
  }
#endif

  return false;
}

ccl_device_forceinline float guiding_phase_sample(KernelGlobals kg,
                                                  IntegratorState state,
                                                  const float2 rand_phase,
                                                  ccl_private float3 *wo)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  pgl_vec3f pgl_wo;
  const pgl_point2f rand = openpgl::cpp::Point2(rand_phase.x, rand_phase.y);
  const float pdf = kg->opgl_volume_sampling_distribution->SamplePDF(rand, pgl_wo);
  *wo = make_float3(pgl_wo.x, pgl_wo.y, pgl_wo.z);
  return pdf;
#else
  return 0.0f;
#endif
}

ccl_device_forceinline float guiding_phase_pdf(KernelGlobals kg,
                                               IntegratorState state,
                                               const float3 wo)
{
#if defined(__PATH_GUIDING__) && PATH_GUIDING_LEVEL >= 4
  return kg->opgl_volume_sampling_distribution->PDF(guiding_vec3f(wo));
#else
  return 0.0f;
#endif
}

CCL_NAMESPACE_END
