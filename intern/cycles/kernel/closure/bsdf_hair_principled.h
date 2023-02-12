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
  float3 Y, Z;
  float gamma_o, gamma_t;
  /* Precomputed Transmission and Fresnel term */
  Spectrum T;
  float f;
} PrincipledHairExtra;

typedef struct PrincipledHairBSDF {
  SHADER_CLOSURE_BASE;

  /* Absorption coefficient. */
  Spectrum sigma;
  /* Variance of the underlying logistic distribution, based on longitudinal roughness. */
  float v;
  /* Scale factor of the underlying logistic distribution, based on azimuthal roughness. */
  float s;
  /* Alternative value for v, used for the R lobe. */
  float v_R;
  /* Cuticle tilt angle. */
  float alpha;
  /* IOR. */
  float eta;

  /* Extra closure. */
  ccl_private PrincipledHairExtra *extra;
} PrincipledHairBSDF;

static_assert(sizeof(ShaderClosure) >= sizeof(PrincipledHairBSDF),
              "PrincipledHairBSDF is too large!");
static_assert(sizeof(ShaderClosure) >= sizeof(PrincipledHairExtra),
              "PrincipledHairExtra is too large!");

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
  if (p == 3) {
    return M_1_2PI_F;
  }
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

ccl_device_forceinline float hair_get_lobe_v(const ccl_private PrincipledHairBSDF *bsdf,
                                             const int lobe)
{
  if (lobe == 0) {
    return bsdf->v_R;
  }
  else if (lobe == 1) {
    return 0.25f * bsdf->v;
  }
  else {
    return 4.0f * bsdf->v;
  }
}

#ifdef __HAIR__
/* Set up the hair closure. */
ccl_device int bsdf_principled_hair_setup(ccl_private ShaderData *sd,
                                          ccl_private PrincipledHairBSDF *bsdf,
                                          float u_rough,
                                          float u_coat_rough,
                                          float v_rough)
{
  u_rough = clamp(u_rough, 0.001f, 1.0f);
  v_rough = clamp(v_rough, 0.001f, 1.0f);
  /* u_coat_rough is a multiplier that modifies u_rough for the R lobe. */
  float u_R_roughness = clamp(u_coat_rough * u_rough, 0.001f, 1.0f);

  bsdf->type = CLOSURE_BSDF_HAIR_PRINCIPLED_ID;

  /* Map from the azimuthal and the two longitudinal roughnesses to variance and scale factor. */
  bsdf->v = sqr(0.726f * u_rough + 0.812f * sqr(u_rough) + 3.700f * pow20(u_rough));
  bsdf->v_R = sqr(0.726f * u_R_roughness + 0.812f * sqr(u_R_roughness) +
                  3.700f * pow20(u_R_roughness));
  bsdf->s = (0.265f * v_rough + 1.194f * sqr(v_rough) + 5.372f * pow22(v_rough)) * M_SQRT_PI_8_F;

  /* Compute local frame, aligned to curve tangent and ray direction. */
  float3 X = safe_normalize(sd->dPdu);
  bsdf->extra->Y = safe_normalize(cross(X, sd->wi));
  bsdf->extra->Z = safe_normalize(cross(X, bsdf->extra->Y));

  /* h -1..0..1 means the rays goes from grazing the hair, to hitting it at
   * the center, to grazing the other edge. This is the sine of the angle
   * between sd->Ng and Z, as seen from the tangent X. */

  /* TODO: we convert this value to a cosine later and discard the sign, so
   * we could probably save some operations. */
  float h = (sd->type & PRIMITIVE_CURVE_RIBBON) ? -sd->v : dot(cross(sd->Ng, X), bsdf->extra->Z);

  kernel_assert(fabsf(h) < 1.0f + 1e-4f);
  kernel_assert(isfinite_safe(Y));
  kernel_assert(isfinite_safe(h));

  const float sin_theta_o = dot(sd->wi, X);
  const float cos_theta_o = cos_from_sin(sin_theta_o);

  const float sin_theta_t = sin_theta_o / bsdf->eta;
  const float cos_theta_t = cos_from_sin(sin_theta_t);

  const float sin_gamma_o = h;
  const float cos_gamma_o = cos_from_sin(sin_gamma_o);
  bsdf->extra->gamma_o = safe_asinf(sin_gamma_o);

  const float sin_gamma_t = sin_gamma_o * cos_theta_o / sqrtf(sqr(bsdf->eta) - sqr(sin_theta_o));
  const float cos_gamma_t = cos_from_sin(sin_gamma_t);
  bsdf->extra->gamma_t = safe_asinf(sin_gamma_t);

  bsdf->extra->T = exp(-bsdf->sigma * (2.0f * cos_gamma_t / cos_theta_t));
  bsdf->extra->f = fresnel_dielectric_cos(cos_theta_o * cos_gamma_o, bsdf->eta);

  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_NEEDS_LCG | SD_BSDF_HAS_TRANSMISSION;
}

#endif /* __HAIR__ */

/* Given the Fresnel term and transmittance, generate the attenuation terms for each bounce. */
ccl_device_inline void hair_attenuation(
    KernelGlobals kg, float f, Spectrum T, ccl_private Spectrum *Ap, ccl_private float *lobe_pdf)
{
  /* Primary specular (R). */
  Ap[0] = make_spectrum(f);
  lobe_pdf[0] = f;

  /* Transmission (TT). */
  Spectrum col = sqr(1.0f - f) * T;
  Ap[1] = col;
  lobe_pdf[1] = spectrum_to_gray(kg, col);

  /* Secondary specular (TRT). */
  col *= T * f;
  Ap[2] = col;
  lobe_pdf[2] = spectrum_to_gray(kg, col);

  /* Residual component (TRRT+). */
  col *= safe_divide(T * f, one_spectrum() - T * f);
  Ap[3] = col;
  lobe_pdf[3] = spectrum_to_gray(kg, col);

  /* Normalize sampling weights. */
  float totweight = lobe_pdf[0] + lobe_pdf[1] + lobe_pdf[2] + lobe_pdf[3];
  float fac = safe_divide(1.0f, totweight);

  lobe_pdf[0] *= fac;
  lobe_pdf[1] *= fac;
  lobe_pdf[2] *= fac;
  lobe_pdf[3] *= fac;
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
  angles[6] = sin_theta_i;
  angles[7] = cos_theta_i;
}

/* Since most of the implementation is the same between sampling and evaluation,
 * this shared function implements both.
 * For evaluation, wo is an input, and randu/randv are ignored.
 * For sampling, wo is an output, and randu/randv are used to pick it.
 */
template<bool do_sample>
ccl_device int bsdf_principled_hair_impl(KernelGlobals kg,
                                         ccl_private const PrincipledHairBSDF *bsdf,
                                         ccl_private ShaderData *sd,
                                         ccl_private float3 *wo,
                                         ccl_private Spectrum *F,
                                         ccl_private float *pdf,
                                         float randu,
                                         float randv)
{
  const float3 X = safe_normalize(sd->dPdu);
  const float3 Y = bsdf->extra->Y;
  const float3 Z = bsdf->extra->Z;
  kernel_assert(fabsf(dot(X, Y)) < 1e-3f);

  const float gamma_o = bsdf->extra->gamma_o;
  const float gamma_t = bsdf->extra->gamma_t;

  const float3 local_O = make_float3(dot(sd->wi, X), dot(sd->wi, Y), dot(sd->wi, Z));
  const float sin_theta_o = local_O.x;
  const float cos_theta_o = cos_from_sin(sin_theta_o);
  const float phi_o = atan2f(local_O.z, local_O.y);

  Spectrum Ap[4];
  float lobe_pdf[4];
  hair_attenuation(kg, bsdf->extra->f, bsdf->extra->T, Ap, lobe_pdf);

  float sin_theta_i, cos_theta_i, phi;
  int sampled_p = 0;
  if (do_sample) {
    /* Pick lobe for sampline */
    for (; sampled_p < 3; sampled_p++) {
      if (randu < lobe_pdf[sampled_p]) {
        break;
      }
      randu -= lobe_pdf[sampled_p];
    }

    /* Sample incoming direction */
    float v = hair_get_lobe_v(bsdf, sampled_p);
    float randw = lcg_step_float(&sd->lcg_state), randx = lcg_step_float(&sd->lcg_state);
    randw = max(randw, 1e-5f);
    const float fac = 1.0f + v * logf(randw + (1.0f - randw) * expf(-2.0f / v));
    sin_theta_i = -fac * sin_theta_o + cos_from_sin(fac) * cosf(M_2PI_F * randx) * cos_theta_o;
    cos_theta_i = cos_from_sin(sin_theta_i);

    if (sampled_p < 3) {
      float angles[8];
      hair_alpha_angles(sin_theta_i, cos_theta_i, -bsdf->alpha, angles);
      sin_theta_i = angles[2 * sampled_p];
      cos_theta_i = angles[2 * sampled_p + 1];

      phi = delta_phi(sampled_p, gamma_o, gamma_t) + sample_trimmed_logistic(randv, bsdf->s);
    }
    else {
      phi = M_2PI_F * randv;
    }

    const float phi_i = phi_o + phi;
    *wo = X * sin_theta_i + Y * cos_theta_i * cosf(phi_i) + Z * cos_theta_i * sinf(phi_i);
  }
  else {
    const float3 local_I = make_float3(dot(*wo, X), dot(*wo, Y), dot(*wo, Z));

    sin_theta_i = local_I.x;
    cos_theta_i = cos_from_sin(sin_theta_i);

    const float phi_i = atan2f(local_I.z, local_I.y);
    phi = phi_i - phi_o;
  }

  /* Evaluate throughput. */
  float angles[8];
  hair_alpha_angles(sin_theta_i, cos_theta_i, bsdf->alpha, angles);

  *F = zero_spectrum();
  *pdf = 0.0f;

  for (int p = 0; p < 4; p++) {
    const float Mp = longitudinal_scattering(
        angles[2 * p], angles[2 * p + 1], sin_theta_o, cos_theta_o, hair_get_lobe_v(bsdf, p));
    const float Np = azimuthal_scattering(phi, p, bsdf->s, gamma_o, gamma_t);
    *F += Ap[p] * Mp * Np;
    *pdf += lobe_pdf[p] * Mp * Np;
    kernel_assert(isfinite_safe(*F) && isfinite_safe(*pdf));
  }

  return sampled_p;
}

ccl_device Spectrum bsdf_principled_hair_eval(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              ccl_private const ShaderClosure *sc,
                                              float3 wo,
                                              ccl_private float *pdf)
{
  kernel_assert(isfinite_safe(sd->P) && isfinite_safe(sd->ray_length));

  ccl_private const PrincipledHairBSDF *bsdf = (ccl_private const PrincipledHairBSDF *)sc;

  Spectrum eval;
  bsdf_principled_hair_impl<false>(kg, bsdf, sd, &wo, &eval, pdf, 0.0f, 0.0f);
  return eval;
}

ccl_device int bsdf_principled_hair_sample(KernelGlobals kg,
                                           ccl_private const ShaderClosure *sc,
                                           ccl_private ShaderData *sd,
                                           float randu,
                                           float randv,
                                           ccl_private Spectrum *eval,
                                           ccl_private float3 *wo,
                                           ccl_private float *pdf,
                                           ccl_private float2 *sampled_roughness,
                                           ccl_private float *eta)
{
  ccl_private const PrincipledHairBSDF *bsdf = (ccl_private const PrincipledHairBSDF *)sc;

  int p = bsdf_principled_hair_impl<true>(kg, bsdf, sd, wo, eval, pdf, randu, randv);

  *sampled_roughness = make_float2(bsdf->v_R, bsdf->v_R);
  *eta = bsdf->eta;

  return LABEL_GLOSSY | ((p == 0) ? LABEL_REFLECT : LABEL_TRANSMIT);
}

/* Implements Filter Glossy by capping the effective roughness. */
ccl_device void bsdf_principled_hair_blur(ccl_private ShaderClosure *sc, float roughness)
{
  ccl_private PrincipledHairBSDF *bsdf = (ccl_private PrincipledHairBSDF *)sc;

  bsdf->v = fmaxf(roughness, bsdf->v);
  bsdf->s = fmaxf(roughness, bsdf->s);
  bsdf->v_R = fmaxf(roughness, bsdf->v_R);
}

/* Hair Albedo */

ccl_device_inline float bsdf_principled_hair_albedo_roughness_scale(const float u_rough)
{
  const float x = u_rough;
  return (((((0.245f * x) + 5.574f) * x - 10.73f) * x + 2.532f) * x - 0.215f) * x + 5.969f;
}

ccl_device Spectrum bsdf_principled_hair_albedo(ccl_private const ShaderClosure *sc)
{
  ccl_private PrincipledHairBSDF *bsdf = (ccl_private PrincipledHairBSDF *)sc;
  /* This is simply the sum of the four Ap terms in hair_attenuation. */
  const float3 T = bsdf->extra->T;
  const float f = bsdf->extra->f;
  return safe_divide(T * (1.0f - 2.0f * f) + make_spectrum(f), one_spectrum() - f * T);
}

ccl_device_inline Spectrum bsdf_principled_hair_sigma_from_reflectance(const Spectrum color,
                                                                       const float u_rough)
{
  const Spectrum sigma = log(color) / bsdf_principled_hair_albedo_roughness_scale(u_rough);
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
