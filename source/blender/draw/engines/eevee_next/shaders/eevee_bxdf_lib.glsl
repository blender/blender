/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * BxDF evaluation functions.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_fast_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name GGX
 *
 * \{ */

float bxdf_ggx_D(float NH, float a2)
{
  return a2 / (M_PI * square((a2 - 1.0) * (NH * NH) + 1.0));
}

float bxdf_ggx_D_opti(float NH, float a2)
{
  float tmp = (NH * a2 - NH) * NH + 1.0;
  /* Doing RCP and multiply a2 at the end. */
  return M_PI * tmp * tmp;
}

float bxdf_ggx_smith_G1(float NX, float a2)
{
  return 2.0 / (1.0 + sqrt(1.0 + a2 * (1.0 / (NX * NX) - 1.0)));
}

float bxdf_ggx_smith_G1_opti(float NX, float a2)
{
  /* Using Brian Karis approach and refactoring by NX/NX
   * this way the `(2*NL)*(2*NV)` in `G = G1(V) * G1(L)` gets canceled by the BRDF denominator
   * `4*NL*NV` RCP is done on the whole G later. Note that this is not convenient for the
   * transmission formula. */
  return NX + sqrt(NX * (NX - NX * a2) + a2);
}

/* Compute the GGX BRDF without the Fresnel term, multiplied by the cosine foreshortening term. */
float bsdf_ggx_reflect(vec3 N, vec3 L, vec3 V, float roughness, out float pdf)
{
  float a2 = square(roughness);

  vec3 H = normalize(L + V);
  float NH = max(dot(N, H), 1e-8);
  float NL = max(dot(N, L), 1e-8);
  float NV = max(dot(N, V), 1e-8);

  /* TODO: maybe implement non-separable shadowing-masking term following Cycles. */
  float G_V = bxdf_ggx_smith_G1(NV, a2);
  float G_L = bxdf_ggx_smith_G1(NL, a2);
  float D = bxdf_ggx_D(NH, a2);

  pdf = D / ((1.0 + G_V) * 4.0 * NV);
  /* BRDF * NL =  `((D * G) / (4 * NV * NL)) * NL`. */
  return (D * (G_V * G_L)) / (4.0 * NV);
}

/* Compute the GGX BTDF without the Fresnel term, multiplied by the cosine foreshortening term. */
float bsdf_ggx_refract(vec3 N, vec3 L, vec3 V, float roughness, float eta, out float pdf)
{
  float LV = dot(L, V);
  if (is_equal(eta, 1.0, 1e-4)) {
    pdf = 1.0;
    /* Only valid when `L` and `V` point in the opposite directions. */
    return float(is_equal(LV, -1.0, 1e-3));
  }

  bool valid = (eta < 1.0) ? (LV < -eta) : (LV * eta < -1.0);
  if (!valid) {
    /* Impossible configuration for transmission due to total internal reflection. */
    pdf = 0.0;
    return 0.0;
  }

  vec3 H = eta * L + V;
  H = (eta < 1.0) ? H : -H;
  float inv_len_H = safe_rcp(length(H));
  H *= inv_len_H;

  /* For transmission, `L` lies in the opposite hemisphere as `H`, therefore negate `L`. */
  float NH = max(dot(N, H), 1e-8);
  float NL = max(dot(N, -L), 1e-8);
  float NV = max(dot(N, V), 1e-8);
  float VH = saturate(dot(V, H));
  float LH = saturate(dot(-L, H));

  float a2 = square(roughness);
  float G_V = bxdf_ggx_smith_G1(NV, a2);
  float G_L = bxdf_ggx_smith_G1(NL, a2);
  float D = bxdf_ggx_D(NH, a2);
  float common = D * VH * LH * square(eta * inv_len_H);

  pdf = common / ((1.0 + G_V) * NV);
  /* `btdf * NL = abs(VH * LH) * ior^2 * D * G(V) * G(L) / (Ht2 * NV * NL) * NL`. */
  return (G_V * G_L * common) / NV;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lambert
 *
 * Not really a microfacet model but fits this file.
 * \{ */

float bsdf_lambert(vec3 N, vec3 L, out float pdf)
{
  float cos_theta = saturate(dot(N, L));
  pdf = cos_theta;
  return cos_theta;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utils
 * \{ */

/* Fresnel monochromatic, perfect mirror */
float F_eta(float eta, float cos_theta)
{
  /* Compute fresnel reflectance without explicitly computing
   * the refracted direction. */
  float c = abs(cos_theta);
  float g = eta * eta - 1.0 + c * c;
  if (g > 0.0) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1.0) / (c * (g - c) + 1.0);
    return 0.5 * A * A * (1.0 + B * B);
  }
  /* Total internal reflections. */
  return 1.0;
}

/* Return the equivalent reflective roughness resulting in a similar lobe. */
float refraction_roughness_remapping(float roughness, float ior)
{
  /* This is a very rough mapping used by manually curve fitting the apparent roughness
   * (blurriness) of GGX reflections and GGX refraction.
   * A better fit is desirable if it is in the same order of complexity.  */
  if (ior > 1.0) {
    return roughness * sqrt_fast(1.0 - 1.0 / ior);
  }
  else {
    return roughness * sqrt_fast(saturate(1.0 - ior)) * 0.8;
  }
}

/**
 * `roughness` is expected to be the linear (from UI) roughness from.
 */
vec3 reflection_dominant_dir(vec3 N, vec3 V, float roughness)
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
 * `roughness` is expected to be the reflection roughness from `refraction_roughness_remapping`.
 */
vec3 refraction_dominant_dir(vec3 N, vec3 V, float ior, float roughness)
{
  /* Reusing same thing as reflection_dominant_dir for now with the roughness mapped to
   * reflection roughness. */
  float m = square(roughness);
  vec3 R = refract(-V, N, 1.0 / ior);
  float smoothness = 1.0 - m;
  float fac = smoothness * (sqrt(smoothness) + m);
  return normalize(mix(-N, R, fac));
}

/** \} */
