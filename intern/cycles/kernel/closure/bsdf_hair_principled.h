/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2018-2022 Blender Foundation */

#pragma once

#ifndef __KERNEL_GPU__
#  include <fenv.h>
#endif

#include "kernel/util/color.h"

CCL_NAMESPACE_BEGIN

typedef struct PrincipledHairExtra {
  /* Geometry data. */
  float4 geom;
} PrincipledHairExtra;

typedef struct PrincipledHairBSDF {
  SHADER_CLOSURE_BASE;

  /* Absorption coefficient. */
  Spectrum sigma;
  /* Variance of the underlying logistic distribution. */
  float v;
  /* Scale factor of the underlying logistic distribution. */
  float s;
  /* Cuticle tilt angle. */
  float alpha;
  /* IOR. */
  float eta;
  /* Effective variance for the diffuse bounce only. */
  float m0_roughness;

  /* Extra closure. */
  ccl_private PrincipledHairExtra *extra;
} PrincipledHairBSDF;

static_assert(sizeof(ShaderClosure) >= sizeof(PrincipledHairBSDF),
              "PrincipledHairBSDF is too large!");
static_assert(sizeof(ShaderClosure) >= sizeof(PrincipledHairExtra),
              "PrincipledHairExtra is too large!");

ccl_device_inline float cos_from_sin(const float s)
{
  return safe_sqrtf(1.0f - s * s);
}

/* Gives the change in direction in the normal plane for the given angles and p-th-order
 * scattering. */
ccl_device_inline float delta_phi(int p, float gamma_o, float gamma_t)
{
  return 2.0f * p * gamma_t - 2.0f * gamma_o + p * M_PI_F;
}

/* Remaps the given angle to [-pi, pi]. */
ccl_device_inline float wrap_angle(float a)
{
  return (a + M_PI_F) - M_2PI_F * floorf((a + M_PI_F) / M_2PI_F) - M_PI_F;
}

/* Logistic distribution function. */
ccl_device_inline float logistic(float x, float s)
{
  float v = expf(-fabsf(x) / s);
  return v / (s * sqr(1.0f + v));
}

/* Logistic cumulative density function. */
ccl_device_inline float logistic_cdf(float x, float s)
{
  float arg = -x / s;
  /* expf() overflows if arg >= 89.0. */
  if (arg > 88.0f) {
    return 0.0f;
  }
  else {
    return 1.0f / (1.0f + expf(arg));
  }
}

/* Numerical approximation to the Bessel function of the first kind. */
ccl_device_inline float bessel_I0(float x)
{
  x = sqr(x);
  float val = 1.0f + 0.25f * x;
  float pow_x_2i = sqr(x);
  uint64_t i_fac_2 = 1;
  int pow_4_i = 16;
  for (int i = 2; i < 10; i++) {
    i_fac_2 *= i * i;
    float newval = val + pow_x_2i / (pow_4_i * i_fac_2);
    if (val == newval) {
      return val;
    }
    val = newval;
    pow_x_2i *= x;
    pow_4_i *= 4;
  }
  return val;
}

/* Logarithm of the Bessel function of the first kind. */
ccl_device_inline float log_bessel_I0(float x)
{
  if (x > 12.0f) {
    /* log(1/x) == -log(x) if x > 0.
     * This is only used with positive cosines. */
    return x + 0.5f * (1.f / (8.0f * x) - M_LN_2PI_F - logf(x));
  }
  else {
    return logf(bessel_I0(x));
  }
}

/* Logistic distribution limited to the interval [-pi, pi]. */
ccl_device_inline float trimmed_logistic(float x, float s)
{
  /* The logistic distribution is symmetric and centered around zero,
   * so logistic_cdf(x, s) = 1 - logistic_cdf(-x, s).
   * Therefore, logistic_cdf(x, s)-logistic_cdf(-x, s) = 1 - 2*logistic_cdf(-x, s) */
  float scaling_fac = 1.0f - 2.0f * logistic_cdf(-M_PI_F, s);
  float val = logistic(x, s);
  return safe_divide(val, scaling_fac);
}

/* Sampling function for the trimmed logistic function. */
ccl_device_inline float sample_trimmed_logistic(float u, float s)
{
  float cdf_minuspi = logistic_cdf(-M_PI_F, s);
  float x = -s * logf(1.0f / (u * (1.0f - 2.0f * cdf_minuspi) + cdf_minuspi) - 1.0f);
  return clamp(x, -M_PI_F, M_PI_F);
}

/* Azimuthal scattering function Np. */
ccl_device_inline float azimuthal_scattering(
    float phi, int p, float s, float gamma_o, float gamma_t)
{
  float phi_o = wrap_angle(phi - delta_phi(p, gamma_o, gamma_t));
  float val = trimmed_logistic(phi_o, s);
  return val;
}

/* Longitudinal scattering function Mp. */
ccl_device_inline float longitudinal_scattering(
    float sin_theta_i, float cos_theta_i, float sin_theta_o, float cos_theta_o, float v)
{
  float inv_v = 1.0f / v;
  float cos_arg = cos_theta_i * cos_theta_o * inv_v;
  float sin_arg = sin_theta_i * sin_theta_o * inv_v;
  if (v <= 0.1f) {
    float i0 = log_bessel_I0(cos_arg);
    float val = expf(i0 - sin_arg - inv_v + 0.6931f + logf(0.5f * inv_v));
    return val;
  }
  else {
    float i0 = bessel_I0(cos_arg);
    float val = (expf(-sin_arg) * i0) / (sinhf(inv_v) * 2.0f * v);
    return val;
  }
}

#ifdef __HAIR__
/* Set up the hair closure. */
ccl_device int bsdf_principled_hair_setup(ccl_private ShaderData *sd,
                                          ccl_private PrincipledHairBSDF *bsdf)
{
  bsdf->type = CLOSURE_BSDF_HAIR_PRINCIPLED_ID;
  bsdf->v = clamp(bsdf->v, 0.001f, 1.0f);
  bsdf->s = clamp(bsdf->s, 0.001f, 1.0f);
  /* Apply Primary Reflection Roughness modifier. */
  bsdf->m0_roughness = clamp(bsdf->m0_roughness * bsdf->v, 0.001f, 1.0f);

  /* Map from roughness_u and roughness_v to variance and scale factor. */
  bsdf->v = sqr(0.726f * bsdf->v + 0.812f * sqr(bsdf->v) + 3.700f * pow20(bsdf->v));
  bsdf->s = (0.265f * bsdf->s + 1.194f * sqr(bsdf->s) + 5.372f * pow22(bsdf->s)) * M_SQRT_PI_8_F;
  bsdf->m0_roughness = sqr(0.726f * bsdf->m0_roughness + 0.812f * sqr(bsdf->m0_roughness) +
                           3.700f * pow20(bsdf->m0_roughness));

  /* Compute local frame, aligned to curve tangent and ray direction. */
  float3 X = safe_normalize(sd->dPdu);
  float3 Y = safe_normalize(cross(X, sd->I));
  float3 Z = safe_normalize(cross(X, Y));

  /* h -1..0..1 means the rays goes from grazing the hair, to hitting it at
   * the center, to grazing the other edge. This is the sine of the angle
   * between sd->Ng and Z, as seen from the tangent X. */

  /* TODO: we convert this value to a cosine later and discard the sign, so
   * we could probably save some operations. */
  float h = (sd->type & PRIMITIVE_CURVE_RIBBON) ? -sd->v : dot(cross(sd->Ng, X), Z);

  kernel_assert(fabsf(h) < 1.0f + 1e-4f);
  kernel_assert(isfinite_safe(Y));
  kernel_assert(isfinite_safe(h));

  bsdf->extra->geom = make_float4(Y.x, Y.y, Y.z, h);

  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_NEEDS_LCG | SD_BSDF_HAS_TRANSMISSION;
}

#endif /* __HAIR__ */

/* Given the Fresnel term and transmittance, generate the attenuation terms for each bounce. */
ccl_device_inline void hair_attenuation(
    KernelGlobals kg, float f, Spectrum T, ccl_private Spectrum *Ap, ccl_private float *Ap_energy)
{
  /* Primary specular (R). */
  Ap[0] = make_spectrum(f);
  Ap_energy[0] = f;

  /* Transmission (TT). */
  Spectrum col = sqr(1.0f - f) * T;
  Ap[1] = col;
  Ap_energy[1] = spectrum_to_gray(kg, col);

  /* Secondary specular (TRT). */
  col *= T * f;
  Ap[2] = col;
  Ap_energy[2] = spectrum_to_gray(kg, col);

  /* Residual component (TRRT+). */
  col *= safe_divide(T * f, one_spectrum() - T * f);
  Ap[3] = col;
  Ap_energy[3] = spectrum_to_gray(kg, col);

  /* Normalize sampling weights. */
  float totweight = Ap_energy[0] + Ap_energy[1] + Ap_energy[2] + Ap_energy[3];
  float fac = safe_divide(1.0f, totweight);

  Ap_energy[0] *= fac;
  Ap_energy[1] *= fac;
  Ap_energy[2] *= fac;
  Ap_energy[3] *= fac;
}

/* Given the tilt angle, generate the rotated theta_i for the different bounces. */
ccl_device_inline void hair_alpha_angles(float sin_theta_i,
                                         float cos_theta_i,
                                         float alpha,
                                         ccl_private float *angles)
{
  float sin_1alpha = sinf(alpha);
  float cos_1alpha = cos_from_sin(sin_1alpha);
  float sin_2alpha = 2.0f * sin_1alpha * cos_1alpha;
  float cos_2alpha = sqr(cos_1alpha) - sqr(sin_1alpha);
  float sin_4alpha = 2.0f * sin_2alpha * cos_2alpha;
  float cos_4alpha = sqr(cos_2alpha) - sqr(sin_2alpha);

  angles[0] = sin_theta_i * cos_2alpha + cos_theta_i * sin_2alpha;
  angles[1] = fabsf(cos_theta_i * cos_2alpha - sin_theta_i * sin_2alpha);
  angles[2] = sin_theta_i * cos_1alpha - cos_theta_i * sin_1alpha;
  angles[3] = fabsf(cos_theta_i * cos_1alpha + sin_theta_i * sin_1alpha);
  angles[4] = sin_theta_i * cos_4alpha - cos_theta_i * sin_4alpha;
  angles[5] = fabsf(cos_theta_i * cos_4alpha + sin_theta_i * sin_4alpha);
}

/* Evaluation function for our shader. */
ccl_device Spectrum bsdf_principled_hair_eval(KernelGlobals kg,
                                              ccl_private const ShaderData *sd,
                                              ccl_private const ShaderClosure *sc,
                                              const float3 omega_in,
                                              ccl_private float *pdf)
{
  kernel_assert(isfinite_safe(sd->P) && isfinite_safe(sd->ray_length));

  ccl_private const PrincipledHairBSDF *bsdf = (ccl_private const PrincipledHairBSDF *)sc;
  const float3 Y = float4_to_float3(bsdf->extra->geom);

  const float3 X = safe_normalize(sd->dPdu);
  kernel_assert(fabsf(dot(X, Y)) < 1e-3f);
  const float3 Z = safe_normalize(cross(X, Y));

  const float3 wo = make_float3(dot(sd->I, X), dot(sd->I, Y), dot(sd->I, Z));
  const float3 wi = make_float3(dot(omega_in, X), dot(omega_in, Y), dot(omega_in, Z));

  const float sin_theta_o = wo.x;
  const float cos_theta_o = cos_from_sin(sin_theta_o);
  const float phi_o = atan2f(wo.z, wo.y);

  const float sin_theta_t = sin_theta_o / bsdf->eta;
  const float cos_theta_t = cos_from_sin(sin_theta_t);

  const float sin_gamma_o = bsdf->extra->geom.w;
  const float cos_gamma_o = cos_from_sin(sin_gamma_o);
  const float gamma_o = safe_asinf(sin_gamma_o);

  const float sin_gamma_t = sin_gamma_o * cos_theta_o / sqrtf(sqr(bsdf->eta) - sqr(sin_theta_o));
  const float cos_gamma_t = cos_from_sin(sin_gamma_t);
  const float gamma_t = safe_asinf(sin_gamma_t);

  const Spectrum T = exp(-bsdf->sigma * (2.0f * cos_gamma_t / cos_theta_t));
  Spectrum Ap[4];
  float Ap_energy[4];
  hair_attenuation(
      kg, fresnel_dielectric_cos(cos_theta_o * cos_gamma_o, bsdf->eta), T, Ap, Ap_energy);

  const float sin_theta_i = wi.x;
  const float cos_theta_i = cos_from_sin(sin_theta_i);
  const float phi_i = atan2f(wi.z, wi.y);

  const float phi = phi_i - phi_o;

  float angles[6];
  hair_alpha_angles(sin_theta_i, cos_theta_i, bsdf->alpha, angles);

  Spectrum F = zero_spectrum();
  float F_energy = 0.0f;

  /* Primary specular (R), Transmission (TT) and Secondary Specular (TRT). */
  for (int i = 0; i < 3; i++) {
    const float Mp = longitudinal_scattering(angles[2 * i],
                                             angles[2 * i + 1],
                                             sin_theta_o,
                                             cos_theta_o,
                                             (i == 0) ? bsdf->m0_roughness :
                                             (i == 1) ? 0.25f * bsdf->v :
                                                        4.0f * bsdf->v);
    const float Np = azimuthal_scattering(phi, i, bsdf->s, gamma_o, gamma_t);
    F += Ap[i] * Mp * Np;
    F_energy += Ap_energy[i] * Mp * Np;
    kernel_assert(isfinite_safe(F) && isfinite_safe(F_energy));
  }

  /* Residual component (TRRT+). */
  {
    const float Mp = longitudinal_scattering(
        sin_theta_i, cos_theta_i, sin_theta_o, cos_theta_o, 4.0f * bsdf->v);
    const float Np = M_1_2PI_F;
    F += Ap[3] * Mp * Np;
    F_energy += Ap_energy[3] * Mp * Np;
    kernel_assert(isfinite_safe(F) && isfinite_safe(F_energy));
  }

  *pdf = F_energy;
  return F;
}

/* Sampling function for the hair shader. */
ccl_device int bsdf_principled_hair_sample(KernelGlobals kg,
                                           ccl_private const ShaderClosure *sc,
                                           ccl_private ShaderData *sd,
                                           float randu,
                                           float randv,
                                           ccl_private Spectrum *eval,
                                           ccl_private float3 *omega_in,
                                           ccl_private float *pdf,
                                           ccl_private float2 *sampled_roughness,
                                           ccl_private float *eta)
{
  ccl_private PrincipledHairBSDF *bsdf = (ccl_private PrincipledHairBSDF *)sc;

  *sampled_roughness = make_float2(bsdf->m0_roughness, bsdf->m0_roughness);
  *eta = bsdf->eta;

  const float3 Y = float4_to_float3(bsdf->extra->geom);

  const float3 X = safe_normalize(sd->dPdu);
  kernel_assert(fabsf(dot(X, Y)) < 1e-3f);
  const float3 Z = safe_normalize(cross(X, Y));

  const float3 wo = make_float3(dot(sd->I, X), dot(sd->I, Y), dot(sd->I, Z));

  float2 u[2];
  u[0] = make_float2(randu, randv);
  u[1].x = lcg_step_float(&sd->lcg_state);
  u[1].y = lcg_step_float(&sd->lcg_state);

  const float sin_theta_o = wo.x;
  const float cos_theta_o = cos_from_sin(sin_theta_o);
  const float phi_o = atan2f(wo.z, wo.y);

  const float sin_theta_t = sin_theta_o / bsdf->eta;
  const float cos_theta_t = cos_from_sin(sin_theta_t);

  const float sin_gamma_o = bsdf->extra->geom.w;
  const float cos_gamma_o = cos_from_sin(sin_gamma_o);
  const float gamma_o = safe_asinf(sin_gamma_o);

  const float sin_gamma_t = sin_gamma_o * cos_theta_o / sqrtf(sqr(bsdf->eta) - sqr(sin_theta_o));
  const float cos_gamma_t = cos_from_sin(sin_gamma_t);
  const float gamma_t = safe_asinf(sin_gamma_t);

  const Spectrum T = exp(-bsdf->sigma * (2.0f * cos_gamma_t / cos_theta_t));
  Spectrum Ap[4];
  float Ap_energy[4];
  hair_attenuation(
      kg, fresnel_dielectric_cos(cos_theta_o * cos_gamma_o, bsdf->eta), T, Ap, Ap_energy);

  int p = 0;
  for (; p < 3; p++) {
    if (u[0].x < Ap_energy[p]) {
      break;
    }
    u[0].x -= Ap_energy[p];
  }

  float v = bsdf->v;
  if (p == 1) {
    v *= 0.25f;
  }
  if (p >= 2) {
    v *= 4.0f;
  }

  u[1].x = max(u[1].x, 1e-5f);
  const float fac = 1.0f + v * logf(u[1].x + (1.0f - u[1].x) * expf(-2.0f / v));
  float sin_theta_i = -fac * sin_theta_o +
                      cos_from_sin(fac) * cosf(M_2PI_F * u[1].y) * cos_theta_o;
  float cos_theta_i = cos_from_sin(sin_theta_i);

  float angles[6];
  if (p < 3) {
    hair_alpha_angles(sin_theta_i, cos_theta_i, -bsdf->alpha, angles);
    sin_theta_i = angles[2 * p];
    cos_theta_i = angles[2 * p + 1];
  }

  float phi;
  if (p < 3) {
    phi = delta_phi(p, gamma_o, gamma_t) + sample_trimmed_logistic(u[0].y, bsdf->s);
  }
  else {
    phi = M_2PI_F * u[0].y;
  }
  const float phi_i = phi_o + phi;

  hair_alpha_angles(sin_theta_i, cos_theta_i, bsdf->alpha, angles);

  Spectrum F = zero_spectrum();
  float F_energy = 0.0f;

  /* Primary specular (R), Transmission (TT) and Secondary Specular (TRT). */
  for (int i = 0; i < 3; i++) {
    const float Mp = longitudinal_scattering(angles[2 * i],
                                             angles[2 * i + 1],
                                             sin_theta_o,
                                             cos_theta_o,
                                             (i == 0) ? bsdf->m0_roughness :
                                             (i == 1) ? 0.25f * bsdf->v :
                                                        4.0f * bsdf->v);
    const float Np = azimuthal_scattering(phi, i, bsdf->s, gamma_o, gamma_t);
    F += Ap[i] * Mp * Np;
    F_energy += Ap_energy[i] * Mp * Np;
    kernel_assert(isfinite_safe(F) && isfinite_safe(F_energy));
  }

  /* Residual component (TRRT+). */
  {
    const float Mp = longitudinal_scattering(
        sin_theta_i, cos_theta_i, sin_theta_o, cos_theta_o, 4.0f * bsdf->v);
    const float Np = M_1_2PI_F;
    F += Ap[3] * Mp * Np;
    F_energy += Ap_energy[3] * Mp * Np;
    kernel_assert(isfinite_safe(F) && isfinite_safe(F_energy));
  }

  *eval = F;
  *pdf = F_energy;

  *omega_in = X * sin_theta_i + Y * cos_theta_i * cosf(phi_i) + Z * cos_theta_i * sinf(phi_i);

  return LABEL_GLOSSY | ((p == 0) ? LABEL_REFLECT : LABEL_TRANSMIT);
}

/* Implements Filter Glossy by capping the effective roughness. */
ccl_device void bsdf_principled_hair_blur(ccl_private ShaderClosure *sc, float roughness)
{
  ccl_private PrincipledHairBSDF *bsdf = (ccl_private PrincipledHairBSDF *)sc;

  bsdf->v = fmaxf(roughness, bsdf->v);
  bsdf->s = fmaxf(roughness, bsdf->s);
  bsdf->m0_roughness = fmaxf(roughness, bsdf->m0_roughness);
}

/* Hair Albedo */

ccl_device_inline float bsdf_principled_hair_albedo_roughness_scale(
    const float azimuthal_roughness)
{
  const float x = azimuthal_roughness;
  return (((((0.245f * x) + 5.574f) * x - 10.73f) * x + 2.532f) * x - 0.215f) * x + 5.969f;
}

ccl_device Spectrum bsdf_principled_hair_albedo(ccl_private const ShaderClosure *sc)
{
  ccl_private PrincipledHairBSDF *bsdf = (ccl_private PrincipledHairBSDF *)sc;
  return exp(-sqrt(bsdf->sigma) * bsdf_principled_hair_albedo_roughness_scale(bsdf->v));
}

ccl_device_inline Spectrum
bsdf_principled_hair_sigma_from_reflectance(const Spectrum color, const float azimuthal_roughness)
{
  const Spectrum sigma = log(color) /
                         bsdf_principled_hair_albedo_roughness_scale(azimuthal_roughness);
  return sigma * sigma;
}

ccl_device_inline Spectrum bsdf_principled_hair_sigma_from_concentration(const float eumelanin,
                                                                         const float pheomelanin)
{
  const float3 eumelanin_color = make_float3(0.506f, 0.841f, 1.653f);
  const float3 pheomelanin_color = make_float3(0.343f, 0.733f, 1.924f);

  return eumelanin * rgb_to_spectrum(eumelanin_color) +
         pheomelanin * rgb_to_spectrum(pheomelanin_color);
}

CCL_NAMESPACE_END
