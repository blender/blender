/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(eevee_bxdf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_thickness_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_codegen_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Microfacet GGX distribution
 * \{ */

float bxdf_ggx_D(float NH, float a2)
{
  return a2 / (M_PI * square((a2 - 1.0) * square(NH) + 1.0));
}

float bxdf_ggx_smith_G1(float NX, float a2)
{
  return 2.0 / (1.0 + sqrt(1.0 + a2 * (1.0 / square(NX) - 1.0)));
}

/**
 * Returns a tangent space reflection or refraction direction following the GGX distribution.
 *
 * \param rand: random point on the unit cylinder (result of sample_cylinder).
 *              The Z component can be biased towards 1.
 * \param alpha: roughness parameter.
 * \param Vt: View vector in tangent space.
 * \param do_reflection: true is sampling reflection.
 *
 * \return pdf: the pdf of sampling the reflected/refracted ray. 0 if ray is invalid.
 */
BsdfSample bxdf_ggx_sample_visible_normals(
    vec3 rand, vec3 Vt, float alpha, float eta, const bool do_reflection)
{
  if (alpha < square(BSDF_ROUGHNESS_THRESHOLD)) {
    BsdfSample samp;
    samp.pdf = 1e6;
    if (do_reflection) {
      samp.direction = reflect(-Vt, vec3(0.0, 0.0, 1.0));
    }
    else {
      samp.direction = refract(-Vt, vec3(0.0, 0.0, 1.0), 1.0 / eta);
    }
    return samp;
  }
  /**
   * "Sampling Visible GGX Normals with Spherical Caps.""
   * Jonathan Dupuy and Anis Benyoub, HPG Vol. 42, No. 8, 2023.
   * https://diglib.eg.org/bitstream/handle/10.1111/cgf14867/v42i8_03_14867.pdf
   * View vector is expected to be in tangent space.
   *
   * "Bounded VNDF Sampling for Smith-GGX Reflections."
   * Eto, Kenta, and Yusuke Tokuyoshi.
   * SIGGRAPH Asia 2023 Technical Communications. 2023. 1-4.
   * https://gpuopen.com/download/publications/Bounded_VNDF_Sampling_for_Smith-GGX_Reflections.pdf
   */

  vec2 Vt_alpha = alpha * Vt.xy;
  float len_Vt_alpha_sqr = length_squared(Vt_alpha);

  /* Transforming the view direction to the hemisphere configuration. */
  vec3 Vh = vec3(Vt_alpha, Vt.z);
  float Vh_norm;
  /* Use optimal normalize code if norm is not needed.  */
  Vh = (do_reflection) ? normalize_and_get_length(Vh, Vh_norm) : normalize(Vh);

  /* Compute the bounded cap. */
  float a2 = square(alpha);
  float s2 = square(1.0 + length(Vt.xy));
  float k = (1.0 - a2) * s2 / (s2 + a2 * square(Vt.z));

  /* Sample a spherical cap in (-Vh.z, 1]. */
  float cos_theta = mix((do_reflection) ? -Vh.z * k : -Vh.z, 1.0, rand.x);
  float sin_theta = sqrt(saturate(1.0 - square(cos_theta)));
  vec3 Lh = vec3(sin_theta * rand.yz, cos_theta);

  /* Compute unnormalized halfway direction. */
  vec3 Hh = Vh + Lh;

  /* Transforming the normal back to the ellipsoid configuration. */
  vec3 Ht = normalize(vec3(alpha * Hh.xy, max(0.0, Hh.z)));

  /* Normal Distribution Function. */
  float D = bxdf_ggx_D(saturate(Ht.z), a2);

  float VH = dot(Vt, Ht);
  if (VH < 0.0) {
    BsdfSample samp;
    samp.direction = vec3(1.0, 0.0, 0.0);
    samp.pdf = 0.0;
    return samp;
  }

  vec3 Lt;
  float pdf;
  if (do_reflection) {
    Lt = reflect(-Vt, Ht);
    if (Vt.z >= 0.0) {
      pdf = D / (2.0 * (k * Vt.z + Vh_norm));
    }
    else {
      float t = sqrt(len_Vt_alpha_sqr + square(Vt.z));
      pdf = D * (t - Vt.z) / (2.0 * len_Vt_alpha_sqr);
    }
  }
  else {
    Lt = refract(-Vt, Ht, 1.0 / eta);
    float LH = dot(Lt, Ht);
    float Ht2 = square(eta * LH + VH);
    float G1_V = 2.0 * Vh.z / (1.0 + Vh.z);
    pdf = D * G1_V * abs(VH * LH) * square(eta) / (Vt.z * Ht2);
  }

  BsdfSample samp;
  samp.direction = Lt;
  samp.pdf = pdf;
  return samp;
}

BsdfSample bxdf_ggx_sample_reflection(vec3 rand, vec3 Vt, float alpha)
{
  return bxdf_ggx_sample_visible_normals(rand, Vt, alpha, 1.0, true);
}

BsdfSample bxdf_ggx_sample_transmission(
    vec3 rand, vec3 Vt, float alpha, float ior, float thickness)
{
  if (thickness != 0.0) {
    ior = 1.0 / ior;
  }
  return bxdf_ggx_sample_visible_normals(rand, Vt, alpha, ior, false);
}

/* Compute the GGX BxDF without the Fresnel term, multiplied by the cosine foreshortening term. */
BsdfEval bxdf_ggx_eval(vec3 N, vec3 L, vec3 V, float alpha, float eta, const bool do_reflection)
{
  alpha = max(square(BSDF_ROUGHNESS_THRESHOLD), alpha);

  float LV = dot(L, V);
  float NV = dot(N, V);
  /* For transmission, `L` lies in the opposite hemisphere as `H`, therefore negate `L`. */
  float NL = max(dot(N, do_reflection ? L : -L), 1e-8);

  if (do_reflection) {
    if (NV <= 0.0) {
#if 0 /* TODO(fclem): Creates black areas when denoising. Find out why. */
      /* Impossible configuration for reflection. */
      BsdfEval eval;
      eval.throughput = 0.0;
      eval.pdf = 0.0;
      return eval;
#endif
    }
  }
  else {
    if (is_equal(eta, 1.0, 1e-4)) {
      /* Only valid when `L` and `V` point in the opposite directions. */
      BsdfEval eval;
      eval.throughput = float(is_equal(LV, -1.0, 1e-3));
      eval.pdf = 1e6;
      return eval;
    }

    bool valid = (eta < 1.0) ? (LV < -eta) : (LV * eta < -1.0);
    if (!valid) {
      /* Impossible configuration for transmission due to total internal reflection. */
      BsdfEval eval;
      eval.throughput = 0.0;
      eval.pdf = 0.0;
      return eval;
    }
  }

  vec3 H = do_reflection ? L + V : eta * L + V;
  H = (do_reflection || eta < 1.0) ? H : -H;
  float inv_len_H = safe_rcp(length(H));
  H *= inv_len_H;

  float NH = max(dot(N, H), 1e-8);
  float VH = saturate(dot(V, H));
  float LH = saturate(dot(do_reflection ? L : -L, H));

  float a2 = square(alpha);
  float G_V = bxdf_ggx_smith_G1(NV, a2);
  float G_L = bxdf_ggx_smith_G1(NL, a2);
  float D = bxdf_ggx_D(NH, a2);

  mat3 tangent_to_world = from_up_axis(N);
  vec3 Vt = V * tangent_to_world;
  vec3 Lt = L * tangent_to_world;

  BsdfEval eval;
  if (do_reflection) {
    /**
     * Eto, Kenta, and Yusuke Tokuyoshi. "Bounded VNDF Sampling for Smith-GGX Reflections."
     * SIGGRAPH Asia 2023 Technical Communications. 2023. 1-4.
     * https://gpuopen.com/download/publications/Bounded_VNDF_Sampling_for_Smith-GGX_Reflections.pdf
     * Listing 2.
     */
    float s2 = square(1.0 + length(Vt.xy));
    float len_ai_sqr = length_squared(alpha * Vt.xy);
    float t = sqrt(len_ai_sqr + square(Vt.z));
    if (Vt.z >= 0.0) {
      float k = (1.0 - a2) * s2 / (s2 + a2 * square(Vt.z));
      eval.pdf = D / (2.0 * (k * Vt.z + t));
    }
    eval.pdf = D * (t - Vt.z) / (2.0 * len_ai_sqr);

#if 0 /* Should work without going into tangent space. But is currently wrong. */
    float sin_theta = sqrt(1.0 - square(NV));
    if (VH >= 0.0) {
      float Vh_norm = length(vec2(sin_theta, NV));
      float s2 = square(1.0 + sin_theta);
      float k = (1.0 - a2) * s2 / (s2 + a2 * square(NV));
      eval.pdf = D / (2.0 * (k * VH + Vh_norm));
    }
    else {
      float len_Vt_alpha_sqr = square(alpha * sin_theta);
      float t = sqrt(len_Vt_alpha_sqr + square(VH));
      eval.pdf = D * (t - VH) / (2.0 * len_Vt_alpha_sqr);
    }
#endif
    /* TODO: But also unused for now. */
    eval.throughput = 1.0;
  }
  else {
    vec3 Vh = normalize(vec3(alpha * Vt.xy, Vt.z));
    float G1_V = 2.0 * Vh.z / (1.0 + Vh.z);

    float Ht2 = square(eta * LH + VH);

    eval.pdf = (D * G1_V * abs(VH * LH) * square(eta)) / (NV * Ht2);

#if 0 /* Should work without going into tangent space. But is currently wrong. */
    float Ht2 = square(eta * LH + VH);
    eval.pdf = D * G_V * abs(VH * LH) * square(eta) / (NV * Ht2);
#endif
    /* `btdf * NL = abs(VH * LH) * ior^2 * D * G(V) * G(L) / (Ht2 * NV * NL) * NL`. */
    eval.throughput = (D * G_V * G_L * VH * LH * square(eta * inv_len_H)) / NV;
  }
  return eval;
}

BsdfEval bxdf_ggx_eval_reflection(vec3 N, vec3 L, vec3 V, float alpha)
{
  return bxdf_ggx_eval(N, L, V, alpha, 1.0, true);
}

BsdfEval bxdf_ggx_eval_transmission(
    vec3 N, vec3 L, vec3 V, float alpha, float ior, float thickness)
{
  if (thickness != 0.0) {
    ior = 1.0 / ior;
  }
  return bxdf_ggx_eval(N, L, V, alpha, ior, false);
}

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
