/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_info.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_utility_texture)

#include "eevee_bxdf_lib.glsl"
#include "eevee_thickness_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_matrix_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Microfacet GGX distribution
 * \{ */

float bxdf_ggx_D(float NH, float a2)
{
  float NH2 = square(NH);
  return a2 / (M_PI * square((1.0 - NH2) + a2 * NH2));
}

float bxdf_ggx_smith_G1(float NX, float a2)
{
  return 2.0 / (1.0 + sqrt(1.0 + a2 * (1.0 / square(NX) - 1.0)));
}

/* -------------------------------------------------------------------- */
/** "Bounded VNDF Sampling for Smith-GGX Reflections."
 * Eto, Kenta, and Yusuke Tokuyoshi. SIGGRAPH Asia 2023 Technical Communications. 2023. 1-4.
 * https://gpuopen.com/download/publications/Bounded_VNDF_Sampling_for_Smith-GGX_Reflections.pdf
 * \{ */

/**
 * Returns a tangent space reflection direction following the GGX distribution.
 *
 * \param rand: random point on the unit cylinder (result of sample_cylinder).
 *              The Z component can be biased towards 1.
 * \param Vt: View vector in tangent space.
 * \param alpha: roughness parameter.
 * \param do_clamp: clamp for numerical stability during ray tracing.
 *
 * \return: the sampled direction and the pdf of sampling the direction.
 */
BsdfSample bxdf_ggx_sample_reflection(vec3 rand, vec3 Vt, float alpha, const bool do_clamp)
{
  if (do_clamp && alpha < square(BSDF_ROUGHNESS_THRESHOLD)) {
    BsdfSample samp;
    samp.pdf = 1e6;
    samp.direction = reflect(-Vt, vec3(0.0, 0.0, 1.0));
    return samp;
  }

  /* Transforming the view direction to the hemisphere configuration. */
  vec2 Vt_alpha = alpha * Vt.xy;
  float Vh_norm;
  vec3 Vh = normalize_and_get_length(vec3(Vt_alpha, Vt.z), Vh_norm);

  /* Compute the bounded cap. */
  float a2 = square(alpha);
  float s2 = square(1.0 + length(Vt.xy));
  float k = (1.0 - a2) * s2 / (s2 + a2 * square(Vt.z));

  /* Sample a spherical cap in (-Vh.z * k, 1]. */
  float cos_theta = mix(-Vh.z * k, 1.0, rand.x);
  float sin_theta = sqrt(saturate(1.0 - square(cos_theta)));
  vec3 Lh = vec3(sin_theta * rand.yz, cos_theta);

  /* Compute unnormalized halfway direction. */
  vec3 Hh = Vh + Lh;

  /* Transforming the normal back to the ellipsoid configuration. */
  vec3 Ht = normalize(vec3(alpha * Hh.xy, max(0.0, Hh.z)));

  /* Normal Distribution Function. */
  float D = bxdf_ggx_D(saturate(Ht.z), a2);

  float pdf;
  if (Vt.z >= 0.0) {
    pdf = D / (2.0 * (k * Vt.z + Vh_norm));
  }
  else {
    float len_Vt_alpha_sqr = length_squared(Vt_alpha);
    float t = sqrt(len_Vt_alpha_sqr + square(Vt.z));
    pdf = D * (t - Vt.z) / (2.0 * len_Vt_alpha_sqr);
  }

  BsdfSample samp;
  samp.direction = reflect(-Vt, Ht);
  samp.pdf = pdf;
  return samp;
}

/* Evaluate the GGX BRDF without the Fresnel term, multiplied by the cosine foreshortening term.
 * Also evaluate the probability of sampling the reflection direction. */
BsdfEval bxdf_ggx_eval_reflection(vec3 N, vec3 L, vec3 V, float alpha, const bool do_clamp)
{
  float NV = dot(N, V);
  if (NV <= 0.0) {
#if 0 /* TODO(fclem): Creates black areas when denoising. Find out why. */
      /* Impossible configuration for reflection. */
      BsdfEval eval;
      eval.throughput = 0.0;
      eval.pdf = 0.0;
      eval.weight = 0.0;
      return eval;
#endif
  }

  vec3 H = safe_normalize(L + V);

  /* This threshold was computed based on precision of NVidia compiler (see #118997).
   * These drivers tend to produce NaNs in the computation of the NDF (`D`) if alpha is close to 0.
   */
  if (do_clamp) {
    alpha = max(1e-3, alpha);
  }

  float a2 = square(alpha);

  BsdfEval eval;

  float NV2 = square(NV);
  float s2 = square(1.0 + sqrt(1.0 - NV2));
  float len_ai_sqr = a2 * (1.0 - NV2);
  float t = sqrt(len_ai_sqr + NV2);
  if (NV >= 0.0) {
    float k = (1.0 - a2) * s2 / (s2 + a2 * NV2);
    eval.pdf = 1.0 / (2.0 * (k * NV + t));
  }
  else {
    eval.pdf = (t - NV) / (2.0 * len_ai_sqr);
  }

  float NH = dot(N, H);
  float NL = dot(N, L);
  if (do_clamp) {
    NH = max(NH, 1e-8);
    NL = max(NL, 1e-8);
  }

  float D = bxdf_ggx_D(NH, a2);
  float G_V = bxdf_ggx_smith_G1(NV, a2);
  float G_L = bxdf_ggx_smith_G1(NL, a2);

  eval.throughput = (0.25f * G_V * G_L) / NV;
  eval.weight = eval.throughput / eval.pdf;

  /* Multiply `D` after computing the weighted throughput for numerical stability. */
  eval.throughput *= D;
  eval.pdf *= D;

  return eval;
}
/** \} */

/* -------------------------------------------------------------------- */
/** "Sampling Visible GGX Normals with Spherical Caps."
 * Jonathan Dupuy and Anis Benyoub, HPG Vol. 42, No. 8, 2023.
 * https://diglib.eg.org/bitstream/handle/10.1111/cgf14867/v42i8_03_14867.pdf
 * \{ */

vec3 bxdf_ggx_sample_vndf(vec3 rand, vec3 Vt, float alpha, out float G_V)
{
  /* Transforming the view direction to the hemisphere configuration. */
  vec3 Vh = normalize(vec3(alpha * Vt.xy, Vt.z));

  /* Smith shadowing-masking term. */
  G_V = 2.0 * Vh.z / (1.0 + Vh.z);

  /* Sample a spherical cap in (-Vh.z, 1]. */
  float cos_theta = mix(-Vh.z, 1.0, rand.x);
  float sin_theta = sqrt(saturate(1.0 - square(cos_theta)));
  vec3 Lh = vec3(sin_theta * rand.yz, cos_theta);

  /* Compute unnormalized halfway direction. */
  vec3 Hh = Vh + Lh;

  /* Transforming the normal back to the ellipsoid configuration. */
  return normalize(vec3(alpha * Hh.xy, max(0.0, Hh.z)));
}

/**
 * Returns a tangent space refraction direction following the GGX distribution.
 *
 * \param rand: random point on the unit cylinder (result of sample_cylinder).
 *              The Z component can be biased towards 1.
 * \param Vt: View vector in tangent space.
 * \param alpha: roughness parameter
 * \param ior: refractive index of the material.
 * \param do_clamp: clamp for numerical stability during ray tracing.
 *
 * \return: the sampled direction and the pdf of sampling the direction.
 */
BsdfSample bxdf_ggx_sample_refraction(
    vec3 rand, vec3 Vt, float alpha, float ior, float thickness, const bool do_clamp)
{
  if (thickness != 0.0) {
    /* The incoming ray is inside the material for the second refraction event. */
    ior = 1.0 / ior;
  }

  if (do_clamp && alpha < square(BSDF_ROUGHNESS_THRESHOLD)) {
    BsdfSample samp;
    samp.pdf = 1e6;
    samp.direction = refract(-Vt, vec3(0.0, 0.0, 1.0), 1.0 / ior);
    return samp;
  }

  float G_V;
  vec3 Ht = bxdf_ggx_sample_vndf(rand, Vt, alpha, G_V);

  vec3 Lt = refract(-Vt, Ht, 1.0 / ior);

  /* Normal Distribution Function. */
  float D = bxdf_ggx_D(saturate(Ht.z), square(alpha));

  float VH = dot(Vt, Ht);
  float LH = dot(Lt, Ht);
  float Ht2 = square(ior * LH + VH);
  float pdf = D * G_V * abs(VH * LH) * square(ior) / (Vt.z * Ht2);

  BsdfSample samp;
  samp.direction = Lt;
  samp.pdf = pdf;
  return samp;
}

/* Evaluate the GGX BTDF without the Fresnel term, multiplied by the cosine foreshortening term.
 * Also evaluate the probability of sampling the refraction direction. */
BsdfEval bxdf_ggx_eval_refraction(
    vec3 N, vec3 L, vec3 V, float alpha, float ior, float thickness, const bool do_clamp)
{
  if (thickness != 0.0) {
    ior = 1.0 / ior;
  }

  float LV = dot(L, V);
  if (do_clamp && is_equal(ior, 1.0, 1e-4)) {
    /* Only valid when `L` and `V` point in the opposite directions. */
    BsdfEval eval;
    eval.throughput = float(is_equal(LV, -1.0, 1e-3));
    eval.pdf = 1e6;
    eval.weight = eval.throughput;
    return eval;
  }

  bool valid = (ior < 1.0) ? (LV < -ior) : (LV * ior < -1.0);
  if (!valid) {
    /* Impossible configuration for transmission due to total internal reflection. */
    BsdfEval eval;
    eval.throughput = 0.0;
    eval.pdf = 0.0;
    eval.weight = 0.0;
    return eval;
  }

  vec3 H = ior * L + V;
  /* Ensure `H` is on the same hemisphere as `V`. */
  H = (ior < 1.0) ? H : -H;
  float inv_len_H = safe_rcp(length(H));
  H *= inv_len_H;

  /* For transmission, `L` lies in the opposite hemisphere as `H`, therefore negate `L`. */
  float NL = dot(N, -L);
  float NH = dot(N, H);
  float NV = dot(N, V);

  if (do_clamp) {
    NL = max(NL, 1e-8);
    NH = max(NH, 1e-8);
    /* This threshold was computed based on precision of NVidia compiler (see #118997).
     * These drivers tend to produce NaNs in the computation of the NDF (`D`) if alpha is close to
     * 0. */
    alpha = max(1e-3, alpha);
  }

  float a2 = square(alpha);
  float G_V = bxdf_ggx_smith_G1(NV, a2);
  float G_L = bxdf_ggx_smith_G1(NL, a2);
  float D = bxdf_ggx_D(NH, a2);

  float VH = saturate(dot(V, H));
  float LH = saturate(dot(-L, H));

  BsdfEval eval;
  eval.pdf = (D * G_V * VH * LH * square(ior * inv_len_H)) / NV;
  eval.throughput = eval.pdf * G_L;
  eval.weight = G_L;

  return eval;
}

/** \} */

/**
 * `roughness` is expected to be the linear (from UI) roughness.
 */
float bxdf_ggx_perceived_roughness_reflection(float roughness)
{
  return roughness;
}

/**
 * Return the equivalent reflective roughness resulting in a similar lobe.
 * `roughness` is expected to be the linear (from UI) roughness.
 */
float bxdf_ggx_perceived_roughness_transmission(float roughness, float ior)
{
  /* This is a very rough mapping used by manually curve fitting the apparent roughness
   * (blurriness) of GGX reflections and GGX refraction.
   * A better fit is desirable if it is in the same order of complexity.  */
  return roughness * sqrt_fast((ior > 1.0) ? (1.0 - 1.0 / ior) : (saturate(1.0 - ior) * 0.64));
}

/**
 * Returns the dominant direction for one reflection event.
 * `roughness` is expected to be the linear (from UI) roughness.
 */
vec3 bxdf_ggx_dominant_direction_reflection(vec3 N, vec3 V, float roughness)
{
  /* From Frostbite PBR Course
   * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
   * Listing 22.
   * Note that the reference labels squared roughness (GGX input) as roughness. */
  float m = square(roughness);
  vec3 R = -reflect(V, N);
  float smoothness = 1.0 - m;
  float fac = smoothness * (sqrt(smoothness) + m);
  return normalize(mix(N, R, fac));
}

/**
 * Returns the dominant direction for one transmission event.
 * `roughness` is expected to be the reflection roughness from
 * `bxdf_ggx_perceived_roughness_transmission`.
 */
vec3 bxdf_ggx_dominant_direction_transmission(vec3 N, vec3 V, float ior, float roughness)
{
  /* Reusing same thing as bxdf_ggx_dominant_direction_reflection for now with the roughness mapped
   * to reflection roughness. */
  float m = square(roughness);
  vec3 R = refract(-V, N, 1.0 / ior);
  float smoothness = 1.0 - m;
  float fac = smoothness * (sqrt(smoothness) + m);
  return normalize(mix(-N, R, fac));
}

LightProbeRay bxdf_ggx_lightprobe_reflection(ClosureReflection cl, vec3 V)
{
  LightProbeRay probe;
  probe.perceptual_roughness = cl.roughness;
  probe.dominant_direction = bxdf_ggx_dominant_direction_reflection(
      cl.N, V, probe.perceptual_roughness);
  return probe;
}

LightProbeRay bxdf_ggx_lightprobe_transmission(ClosureRefraction cl, vec3 V, float thickness)
{
  LightProbeRay probe;
  probe.perceptual_roughness = bxdf_ggx_perceived_roughness_transmission(cl.roughness, cl.ior);
  probe.dominant_direction = bxdf_ggx_dominant_direction_transmission(
      cl.N, V, thickness != 0.0 ? 1.0 / cl.ior : cl.ior, probe.perceptual_roughness);
  return probe;
}

void bxdf_ggx_context_amend_transmission(inout ClosureUndetermined cl,
                                         inout vec3 V,
                                         float thickness)
{
  if (thickness != 0.0) {
    ClosureRefraction bsdf = to_closure_refraction(cl);
    float perceived_roughness = bxdf_ggx_perceived_roughness_transmission(bsdf.roughness,
                                                                          bsdf.ior);
    vec3 L = bxdf_ggx_dominant_direction_transmission(bsdf.N, V, bsdf.ior, perceived_roughness);
    cl.N = -thickness_shape_intersect(thickness, bsdf.N, L).hit_N;
    V = -L;
  }
}

Ray bxdf_ggx_ray_amend_transmission(ClosureUndetermined cl, vec3 V, Ray ray, float thickness)
{
  if (thickness != 0.0) {
    ClosureRefraction bsdf = to_closure_refraction(cl);
    float perceived_roughness = bxdf_ggx_perceived_roughness_transmission(bsdf.roughness,
                                                                          bsdf.ior);
    vec3 L = bxdf_ggx_dominant_direction_transmission(bsdf.N, V, bsdf.ior, perceived_roughness);
    ray.origin += thickness_shape_intersect(thickness, bsdf.N, L).hit_P;
  }
  return ray;
}

#ifdef EEVEE_UTILITY_TX

ClosureLight bxdf_ggx_light_reflection(ClosureReflection cl, vec3 V)
{
  float cos_theta = dot(cl.N, V);
  ClosureLight light;
  light.ltc_mat = utility_tx_sample_lut(utility_tx, cos_theta, cl.roughness, UTIL_LTC_MAT_LAYER);
  light.N = cl.N;
  light.type = LIGHT_SPECULAR;
  return light;
}

ClosureLight bxdf_ggx_light_transmission(ClosureRefraction cl, vec3 V, float thickness)
{
  float perceptual_roughness = bxdf_ggx_perceived_roughness_transmission(cl.roughness, cl.ior);

  if (thickness != 0.0) {
    vec3 L = bxdf_ggx_dominant_direction_transmission(cl.N, V, cl.ior, perceptual_roughness);
    cl.N = -thickness_shape_intersect(thickness, cl.N, L).hit_N;
    V = -L;
  }
  /* Ad-hoc solution to reuse the reflection LUT. To be eventually replaced by own precomputed
   * table. */
  vec3 R = refract(-V, cl.N, (thickness != 0.0) ? cl.ior : (1.0 / cl.ior));
  float cos_theta = dot(-cl.N, R);

  ClosureLight light;
  light.ltc_mat = utility_tx_sample_lut(
      utility_tx, cos_theta, perceptual_roughness, UTIL_LTC_MAT_LAYER);
  light.N = -cl.N;
  light.type = LIGHT_TRANSMISSION;
  return light;
}

#endif

/** \} */
