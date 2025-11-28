/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/types.h"

#include "kernel/util/colorspace.h"
#include "kernel/util/lookup_table.h"

#include "util/color.h"
#include "util/types_spectrum.h"

CCL_NAMESPACE_BEGIN

struct FresnelThinFilm {
  float thickness;
  float ior;
};

template<typename T> struct complex {
  T re;
  T im;

  ccl_device_inline_method complex<T> operator*=(ccl_private const complex<T> &other)
  {
    const T im = this->re * other.im + this->im * other.re;
    this->re = this->re * other.re - this->im * other.im;
    this->im = im;
    return *this;
  }

  ccl_device_inline_method complex<T> operator*(ccl_private const float &other)
  {
    return complex<T>{this->re * other, this->im * other};
  }
};

/* Compute fresnel reflectance for perpendicular (aka S-) and parallel (aka P-) polarized light.
 * If requested by the caller, r_cos_phi is set to the cosine of the phase shift on reflection.
 * Also returns the dot product of the refracted ray and the normal as `cos_theta_t`, as it is
 * used when computing the direction of the refracted ray. */
ccl_device float2 fresnel_dielectric_polarized(float cos_theta_i,
                                               const float eta,
                                               ccl_private float *r_cos_theta_t,
                                               ccl_private float2 *r_cos_phi)
{
  kernel_assert(!isnan_safe(cos_theta_i));

  /* Using Snell's law, calculate the squared cosine of the angle between the surface normal and
   * the transmitted ray. */
  const float eta_cos_theta_t_sq = sqr(eta) - (1.0f - sqr(cos_theta_i));
  if (eta_cos_theta_t_sq <= 0) {
    /* Total internal reflection. */
    if (r_cos_phi) {
      /* The following code would compute the proper phase shift on TIR.
       * However, for the current user of this computation (the iridescence code),
       * this doesn't actually affect the result, so don't bother with the computation for now.
       *
       * `const float fac = sqrtf(1.0f - sqr(cosThetaI) - sqr(eta));`
       * `r_phi->x = -2.0f * atanf(fac / cosThetaI);`
       * `r_phi->y = -2.0f * atanf(fac / (cosThetaI * sqr(eta)));`
       */
      *r_cos_phi = one_float2();
    }
    return one_float2();
  }

  cos_theta_i = fabsf(cos_theta_i);
  /* Relative to the surface normal. */
  const float cos_theta_t = -safe_sqrtf(eta_cos_theta_t_sq) / eta;

  if (r_cos_theta_t) {
    *r_cos_theta_t = cos_theta_t;
  }

  /* Amplitudes of reflected waves. */
  const float r_s = (cos_theta_i + eta * cos_theta_t) / (cos_theta_i - eta * cos_theta_t);
  const float r_p = (cos_theta_t + eta * cos_theta_i) / (eta * cos_theta_i - cos_theta_t);

  if (r_cos_phi) {
    *r_cos_phi = make_float2(2 * (r_s >= 0.0f) - 1, 2 * (r_p >= 0.0f) - 1);
  }

  /* Return squared amplitude to get the fraction of reflected energy. */
  return make_float2(sqr(r_s), sqr(r_p));
}

/* Compute fresnel reflectance for unpolarized light. */
ccl_device_forceinline float fresnel_dielectric(const float cos_theta_i,
                                                const float eta,
                                                ccl_private float *r_cos_theta_t)
{
  return average(fresnel_dielectric_polarized(cos_theta_i, eta, r_cos_theta_t, nullptr));
}

/* Refract the incident ray, given the cosine of the refraction angle and the relative refractive
 * index of the incoming medium w.r.t. the outgoing medium. */
ccl_device_inline float3 refract_angle(const float3 incident,
                                       const float3 normal,
                                       const float cos_theta_t,
                                       const float inv_eta)
{
  return (inv_eta * dot(normal, incident) + cos_theta_t) * normal - inv_eta * incident;
}

ccl_device float fresnel_dielectric_cos(const float cosi, const float eta)
{
  // compute fresnel reflectance without explicitly computing
  // the refracted direction
  const float c = fabsf(cosi);
  float g = eta * eta - 1 + c * c;
  if (g > 0) {
    g = sqrtf(g);
    const float A = (g - c) / (g + c);
    const float B = (c * (g + c) - 1) / (c * (g - c) + 1);
    return 0.5f * A * A * (1 + B * B);
  }
  return 1.0f;  // TIR(no refracted component)
}

/* Approximates the average single-scattering Fresnel for a given IOR.
 * This is defined as the integral over 0...1 of 2*cosI * F(cosI, eta) d_cosI, with F being
 * the real dielectric Fresnel.
 * The implementation here uses a numerical fit from "Revisiting Physically Based Shading
 * at Imageworks" by Christopher Kulla and Alejandro Conty. */
ccl_device_inline float fresnel_dielectric_Fss(const float eta)
{
  if (eta < 1.0f) {
    return 0.997118f + eta * (0.1014f - eta * (0.965241f + eta * 0.130607f));
  }
  return (eta - 1.0f) / (4.08567f + 1.00071f * eta);
}

/* Computes the average single-scattering Fresnel for the F82 metallic model. */
ccl_device_inline Spectrum fresnel_f82_Fss(const Spectrum F0, const Spectrum B)
{
  return mix(F0, one_spectrum(), 1.0f / 21.0f) - B * (1.0f / 126.0f);
}

/* Precompute the B term for the F82 metallic model, given a tint factor. */
ccl_device_inline Spectrum fresnel_f82tint_B(const Spectrum F0, const Spectrum tint)
{
  /* In the classic F82 model, the F82 input directly determines the value of the Fresnel
   * model at ~82°, similar to F0 and F90.
   * With F82-Tint, on the other hand, the value at 82° is the value of the classic Schlick
   * model multiplied by the tint input.
   * Therefore, the factor follows by setting F82Tint(cosI) = FSchlick(cosI) - b*cosI*(1-cosI)^6
   * and F82Tint(acos(1/7)) = FSchlick(acos(1/7)) * f82_tint and solving for b. */
  const float f = 6.0f / 7.0f;
  const float f5 = sqr(sqr(f)) * f;
  const Spectrum F_schlick = mix(F0, one_spectrum(), f5);
  return F_schlick * (7.0f / (f5 * f)) * (one_spectrum() - tint);
}

/* Precompute the B term for the F82 metallic model, given the F82 value. */
ccl_device_inline Spectrum fresnel_f82_B(const Spectrum F0, const Spectrum F82)
{
  const float f = 6.0f / 7.0f;
  const float f5 = sqr(sqr(f)) * f;
  const Spectrum F_schlick = mix(F0, one_spectrum(), f5);
  return (7.0f / (f5 * f)) * (F_schlick - F82);
}

ccl_device_inline float fresnel_f82_B(const float F0, const float F82)
{
  const float f = 6.0f / 7.0f;
  const float f5 = sqr(sqr(f)) * f;
  const float F_schlick = mix(F0, 1.0f, f5);
  return (7.0f / (f5 * f)) * (F_schlick - F82);
}

/* Evaluate the F82 metallic model for the given parameters. */
ccl_device_inline Spectrum fresnel_f82(const float cosi, const Spectrum F0, const Spectrum B)
{
  const float s = saturatef(1.0f - cosi);
  const float s5 = sqr(sqr(s)) * s;
  const Spectrum F_schlick = mix(F0, one_spectrum(), s5);
  return saturate(F_schlick - B * cosi * s5 * s);
}

ccl_device_inline float fresnel_f82(const float cosi, const float F0, const float B)
{
  const float s = saturatef(1.0f - cosi);
  const float s5 = sqr(sqr(s)) * s;
  const float F_schlick = mix(F0, 1.0f, s5);
  return clamp(F_schlick - B * cosi * s5 * s, 0.0f, 1.0);
}

/* Evaluates the Fresnel equations at a dielectric-conductor interface, calculating reflectances
 * and phase shifts due to reflection if requested. The phase shifts phi_s and phi_p are returned
 * as phasor_s = exp(i * phi_s) and phasor_p = exp(i * phi_p).
 * This code is based on equations from section 14.4.1 of Principles of Optics 7th ed. by Born and
 * Wolf, but uses `n + ik` instead of `n(1 + ik)` for IOR. The phase shifts are calculated so that
 * phi_p = phi_s at 90 degree incidence to match fresnel_dielectric_polarized. */
ccl_device_forceinline void fresnel_conductor_polarized(
    const float cosi,
    const float ambient_ior,
    const complex<float> conductor_ior,
    const float F82,
    ccl_private float &r_R_s,
    ccl_private float &r_R_p,
    ccl_private complex<float> *r_phasor_s = nullptr,
    ccl_private complex<float> *r_phasor_p = nullptr)
{
  const float eta1 = ambient_ior;
  const float eta2 = conductor_ior.re;
  const float k2 = conductor_ior.im;

  const float eta1_sq = sqr(eta1);
  const float eta2_sq = sqr(eta2);
  const float k2_sq = sqr(k2);
  const float two_eta2_k2 = 2.0f * eta2 * k2;

  const float t1 = eta2_sq - k2_sq - eta1_sq * (1.0f - sqr(cosi));
  const float t2 = sqrt(sqr(t1) + sqr(two_eta2_k2));

  const float u_sq = max(0.5f * (t2 + t1), 0.0f);
  const float v_sq = max(0.5f * (t2 - t1), 0.0f);
  const float u = sqrt(u_sq);
  const float v = sqrt(v_sq);

  if (F82 >= 0.0f) {
    /* Calculate reflectance using the F82 model if the caller requested it. */
    /* Scale n and k by the film ior, and recompute F0. */
    const float n = eta2 / eta1;
    const float k_sq = sqr(k2 / eta1);
    const float F0 = (sqr(n - 1.0f) + k_sq) / (sqr(n + 1.0f) + k_sq);
    r_R_s = fresnel_f82(cosi, F0, fresnel_f82_B(F0, F82));
    r_R_p = r_R_s;
  }
  else {
    r_R_s = safe_divide(sqr(eta1 * cosi - u) + v_sq, sqr(eta1 * cosi + u) + v_sq);

    const float t3 = (eta2_sq - k2_sq) * cosi;
    const float t4 = two_eta2_k2 * cosi;
    r_R_p = safe_divide(sqr(t3 - eta1 * u) + sqr(t4 - eta1 * v),
                        sqr(t3 + eta1 * u) + sqr(t4 + eta1 * v));
  }

  if (r_phasor_s && r_phasor_p) {
    const float re_s = -u_sq - v_sq + sqr(eta1 * cosi);
    const float im_s = -2.0f * eta1 * cosi * v;
    const float mag_s = sqrt(sqr(re_s) + sqr(im_s));
    r_phasor_s->re = (mag_s == 0.0f) ? 1.0f : re_s / mag_s;
    r_phasor_s->im = (mag_s == 0.0f) ? 0.0f : im_s / mag_s;

    const float re_p = sqr((eta2_sq + k2_sq) * cosi) - eta1_sq * (u_sq + v_sq);
    const float im_p = 2.0f * eta1 * cosi * (two_eta2_k2 * u - (eta2_sq - k2_sq) * v);
    const float mag_p = sqrt(sqr(re_p) + sqr(im_p));
    r_phasor_p->re = mag_p == 0.0f ? 1.0f : re_p / mag_p;
    r_phasor_p->im = mag_p == 0.0f ? 0.0f : im_p / mag_p;
  }
}

ccl_device_forceinline void fresnel_conductor_polarized(const float cosi,
                                                        const float ambient_ior,
                                                        const complex<Spectrum> conductor_ior,
                                                        const ccl_private Spectrum F82,
                                                        ccl_private Spectrum &r_R_s,
                                                        ccl_private Spectrum &r_R_p,
                                                        ccl_private complex<Spectrum> &r_phasor_s,
                                                        ccl_private complex<Spectrum> &r_phasor_p)
{
  /* One component at a time to reduce GPU register pressure. */
  complex<float> phasor23_s_x, phasor23_p_x;
  complex<float> phasor23_s_y, phasor23_p_y;
  complex<float> phasor23_s_z, phasor23_p_z;
  float R_s_x, R_s_y, R_s_z, R_p_x, R_p_y, R_p_z;

  fresnel_conductor_polarized(cosi,
                              ambient_ior,
                              {conductor_ior.re.x, conductor_ior.im.x},
                              F82.x,
                              R_s_x,
                              R_p_x,
                              &phasor23_s_x,
                              &phasor23_p_x);
  fresnel_conductor_polarized(cosi,
                              ambient_ior,
                              {conductor_ior.re.y, conductor_ior.im.y},
                              F82.y,
                              R_s_y,
                              R_p_y,
                              &phasor23_s_y,
                              &phasor23_p_y);
  fresnel_conductor_polarized(cosi,
                              ambient_ior,
                              {conductor_ior.re.z, conductor_ior.im.z},
                              F82.z,
                              R_s_z,
                              R_p_z,
                              &phasor23_s_z,
                              &phasor23_p_z);

  r_R_s = make_float3(R_s_x, R_s_y, R_s_z);
  r_R_p = make_float3(R_p_x, R_p_y, R_p_z);
  r_phasor_s = {make_float3(phasor23_s_x.re, phasor23_s_y.re, phasor23_s_z.re),
                make_float3(phasor23_s_x.im, phasor23_s_y.im, phasor23_s_z.im)};
  r_phasor_p = {make_float3(phasor23_p_x.re, phasor23_p_y.re, phasor23_p_z.re),
                make_float3(phasor23_p_x.im, phasor23_p_y.im, phasor23_p_z.im)};
}

/* Calculates Fresnel reflectance at a dielectric-conductor interface given the relative IOR.
 */
ccl_device Spectrum fresnel_conductor(const float cosi, const complex<Spectrum> ior)
{
  float R_s_x, R_s_y, R_s_z, R_p_x, R_p_y, R_p_z;
  fresnel_conductor_polarized(
      cosi, 1.0f, {ior.re.x, ior.im.x}, -1.0f, R_s_x, R_p_x, nullptr, nullptr);
  fresnel_conductor_polarized(
      cosi, 1.0f, {ior.re.y, ior.im.y}, -1.0f, R_s_y, R_p_y, nullptr, nullptr);
  fresnel_conductor_polarized(
      cosi, 1.0f, {ior.re.z, ior.im.z}, -1.0f, R_s_z, R_p_z, nullptr, nullptr);
  return (make_float3(R_s_x, R_s_y, R_s_z) + make_float3(R_p_x, R_p_y, R_p_z)) * 0.5f;
}

/* Approximates the average single-scattering Fresnel for a physical conductor. */
ccl_device_inline Spectrum fresnel_conductor_Fss(const complex<Spectrum> ior)
{
  /* In order to estimate Fss of the conductor, we fit the F82 model to it based on the
   * value at 0° and ~82° and then use the analytic expression for its Fss. */
  const Spectrum F0 = fresnel_conductor(1.0f, ior);
  const Spectrum F82 = fresnel_conductor(1.0f / 7.0f, ior);
  return saturate(fresnel_f82_Fss(F0, fresnel_f82_B(F0, F82)));
}

ccl_device float ior_from_F0(const float f0)
{
  const float sqrt_f0 = sqrtf(clamp(f0, 0.0f, 0.99f));
  return (1.0f + sqrt_f0) / (1.0f - sqrt_f0);
}

ccl_device float F0_from_ior(const float ior)
{
  return sqr((ior - 1.0f) / (ior + 1.0f));
}

ccl_device float schlick_fresnel(const float u)
{
  const float m = clamp(1.0f - u, 0.0f, 1.0f);
  const float m2 = m * m;
  return m2 * m2 * m;  // pow(m, 5)
}

/* Calculate the fresnel color, which is a blend between white and the F0 color */
ccl_device_forceinline Spectrum interpolate_fresnel_color(const float3 L,
                                                          const float3 H,
                                                          const float ior,
                                                          Spectrum F0)
{
  /* Compute the real Fresnel term and remap it from real_F0..1 to F0..1.
   * The reason why we use this remapping instead of directly doing the
   * Schlick approximation mix(F0, 1.0, (1.0-cosLH)^5) is that for cases
   * with similar IORs (e.g. ice in water), the relative IOR can be close
   * enough to 1.0 that the Schlick approximation becomes inaccurate. */
  const float real_F = fresnel_dielectric_cos(dot(L, H), ior);
  const float real_F0 = fresnel_dielectric_cos(1.0f, ior);

  return mix(F0, one_spectrum(), inverse_lerp(real_F0, 1.0f, real_F));
}

/* If the shading normal results in specular reflection in the lower hemisphere, raise the shading
 * normal towards the geometry normal so that the specular reflection is just above the surface.
 * Only used for glossy materials. */
ccl_device float3 ensure_valid_specular_reflection(const float3 Ng, const float3 I, float3 N)
{
  const float3 R = 2 * dot(N, I) * N - I;

  const float Iz = dot(I, Ng);
  kernel_assert(Iz >= 0);

  /* Reflection rays may always be at least as shallow as the incoming ray. */
  const float threshold = min(0.9f * Iz, 0.01f);
  if (dot(Ng, R) >= threshold) {
    return N;
  }

  /* Form coordinate system with Ng as the Z axis and N inside the X-Z-plane.
   * The X axis is found by normalizing the component of N that's orthogonal to Ng.
   * The Y axis isn't actually needed.
   */
  const float3 X = safe_normalize_fallback(N - dot(N, Ng) * Ng, N);

  /* Calculate N.z and N.x in the local coordinate system.
   *
   * The goal of this computation is to find a N' that is rotated towards Ng just enough
   * to lift R' above the threshold (here called t), therefore dot(R', Ng) = t.
   *
   * According to the standard reflection equation,
   * this means that we want dot(2*dot(N', I)*N' - I, Ng) = t.
   *
   * Since the Z axis of our local coordinate system is Ng, dot(x, Ng) is just x.z, so we get
   * 2*dot(N', I)*N'.z - I.z = t.
   *
   * The rotation is simple to express in the coordinate system we formed -
   * since N lies in the X-Z-plane, we know that N' will also lie in the X-Z-plane,
   * so N'.y = 0 and therefore dot(N', I) = N'.x*I.x + N'.z*I.z .
   *
   * Furthermore, we want N' to be normalized, so N'.x = sqrt(1 - N'.z^2).
   *
   * With these simplifications, we get the equation
   * 2*(sqrt(1 - N'.z^2)*I.x + N'.z*I.z)*N'.z - I.z = t,
   * or
   * 2*sqrt(1 - N'.z^2)*I.x*N'.z = t + I.z * (1 - 2*N'.z^2),
   * after rearranging terms.
   * Raise both sides to the power of two and substitute terms with
   * a = I.x^2 + I.z^2,
   * b = 2*(a + Iz*t),
   * c = (Iz + t)^2,
   * we obtain
   * 4*a*N'.z^4 - 2*b*N'.z^2 + c = 0.
   *
   * The only unknown here is N'.z, so we can solve for that.
   *
   * The equation has four solutions in general, two can immediately be discarded because they're
   * negative so N' would lie in the lower hemisphere; one solves
   * 2*sqrt(1 - N'.z^2)*I.x*N'.z = -(t + I.z * (1 - 2*N'.z^2))
   * instead of the original equation (before squaring both sides).
   * Therefore only one root is valid.
   */

  const float Ix = dot(I, X);

  const float a = sqr(Ix) + sqr(Iz);
  const float b = 2.0f * (a + Iz * threshold);
  const float c = sqr(threshold + Iz);

  /* In order that the root formula solves 2*sqrt(1 - N'.z^2)*I.x*N'.z = t + I.z - 2*I.z*N'.z^2,
   * Ix and (t + I.z * (1 - 2*N'.z^2)) must have the same sign (the rest terms are non-negative by
   * definition). */
  const float Nz2 = (Ix < 0) ? 0.25f * (b + safe_sqrtf(sqr(b) - 4.0f * a * c)) / a :
                               0.25f * (b - safe_sqrtf(sqr(b) - 4.0f * a * c)) / a;

  const float Nx = safe_sqrtf(1.0f - Nz2);
  const float Nz = safe_sqrtf(Nz2);

  return Nx * X + Nz * Ng;
}

/* Do not call #ensure_valid_specular_reflection if the primitive type is curve or if the geometry
 * normal and the shading normal is the same. */
ccl_device float3 maybe_ensure_valid_specular_reflection(ccl_private ShaderData *sd,
                                                         const float3 N)
{
  if ((sd->flag & SD_USE_BUMP_MAP_CORRECTION) == 0) {
    return N;
  }
  if ((sd->type & PRIMITIVE_CURVE) || isequal(sd->Ng, N)) {
    return N;
  }
  return ensure_valid_specular_reflection(sd->Ng, sd->wi, N);
}

/* Principled Hair albedo and absorption coefficients. */
ccl_device_inline float bsdf_principled_hair_albedo_roughness_scale(
    const float azimuthal_roughness)
{
  const float x = azimuthal_roughness;
  return (((((0.245f * x) + 5.574f) * x - 10.73f) * x + 2.532f) * x - 0.215f) * x + 5.969f;
}

ccl_device_inline Spectrum
bsdf_principled_hair_sigma_from_reflectance(const Spectrum color, const float azimuthal_roughness)
{
  const Spectrum sigma = log(max(color, zero_spectrum())) /
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

/* Computes the weight for base closure(s) which are layered under another closure.
 * layer_albedo is an estimate of the top layer's reflectivity, while weight is the closure weight
 * of the entire base+top combination. */
ccl_device_inline Spectrum closure_layering_weight(const Spectrum layer_albedo,
                                                   const Spectrum weight)
{
  return weight * saturatef(1.0f - reduce_max(safe_divide_color(layer_albedo, weight)));
}

/* ******** Thin-film iridescence implementation ********
 *
 * Based on "A Practical Extension to Microfacet Theory for the Modeling of Varying Iridescence"
 * by Laurent Belcour and Pascal Barla.
 * https://belcour.github.io/blog/research/publication/2017/05/01/brdf-thin-film.html.
 */

/**
 * Evaluate the sensitivity functions for the Fourier-space spectral integration.
 */
ccl_device_inline complex<Spectrum> iridescence_lookup_sensitivity(KernelGlobals kg,
                                                                   const float OPD)
{
  /* The LUT covers 0 to 60 um. */
  float x = M_2PI_F * OPD / 60000.0f;
  const int size = THIN_FILM_TABLE_SIZE;

  const float3 re = make_float3(
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 0 * size, size),
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 1 * size, size),
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 2 * size, size));
  const float3 im = make_float3(
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 3 * size, size),
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 4 * size, size),
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 5 * size, size));

  return {re, im};
}

template<typename SpectrumOrFloat>
ccl_device_inline float3 iridescence_airy_summation(KernelGlobals kg,
                                                    const float R12,
                                                    const SpectrumOrFloat R23,
                                                    const float OPD,
                                                    const complex<SpectrumOrFloat> phasor)
{
  const float T121 = 1.0f - R12;

  const SpectrumOrFloat R123 = R12 * R23;
  const SpectrumOrFloat r123 = sqrt(R123);
  const SpectrumOrFloat Rs = sqr(T121) * R23 / (1.0f - R123);

  /* Initialize complex number for exp(i * phi)^m, equivalent to {cos(m * phi), sin(m * phi)} as
   * used in equation 10. */
  complex<SpectrumOrFloat> accumulator = phasor;

  /* Perform summation over path order differences (equation 10). */
  Spectrum R = make_spectrum(Rs + R12); /* C0 */
  SpectrumOrFloat Cm = (Rs - T121);

  /* Truncate after m=3, higher differences have barely any impact. */
  for (int m = 1; m < 4; m++) {
    Cm *= r123;
    const complex<Spectrum> S = iridescence_lookup_sensitivity(kg, m * OPD);
    R += Cm * 2.0f * (accumulator.re * S.re + accumulator.im * S.im);
    accumulator *= phasor;
  }
  return R;
}

/* Template meta-programming helper to be able to have an if-constexpr expression
 * to switch between conductive (for Spectrum) or dielectric (for float) Fresnel.
 * Essentially std::is_same<T, Spectrum>, but also works on GPU. */
template<class T> struct fresnel_info;
template<> struct fresnel_info<float> {
  ccl_static_constexpr bool conductive = false;
};
template<> struct fresnel_info<Spectrum> {
  ccl_static_constexpr bool conductive = true;
};

template<typename SpectrumOrFloat>
ccl_device Spectrum fresnel_iridescence(KernelGlobals kg,
                                        const float ambient_ior,
                                        const FresnelThinFilm thin_film,
                                        const complex<SpectrumOrFloat> substrate_ior,
                                        ccl_private const Spectrum *F82,
                                        const float cos_theta_1,
                                        ccl_private float *r_cos_theta_3)
{
  /* For films below 1nm, the wave-optic-based Airy summation approach no longer applies,
   * so blend towards the case without coating. */
  float film_ior = thin_film.ior;
  if (thin_film.thickness < 1.0f) {
    film_ior = mix(ambient_ior, film_ior, smoothstep(0.0f, 1.0f, thin_film.thickness));
  }

  float cos_theta_2;
  /* The real component of exp(i * phi12), equivalent to cos(phi12). */
  float2 phasor12_real;

  /* Compute reflection at the top interface (ambient to film). */
  const float2 R12 = fresnel_dielectric_polarized(
      cos_theta_1, film_ior / ambient_ior, &cos_theta_2, &phasor12_real);
  if (isequal(R12, one_float2())) {
    /* TIR at the top interface. */
    return one_spectrum();
  }

  /* Compute reflection at the bottom interface (film to substrate). */
  SpectrumOrFloat R23_s, R23_p;
  complex<SpectrumOrFloat> phasor23_s, phasor23_p;
  if constexpr (fresnel_info<SpectrumOrFloat>::conductive) {
    /* Material is a conductor. */
    fresnel_conductor_polarized(-cos_theta_2,
                                film_ior,
                                substrate_ior,
                                (F82) ? *F82 : make_spectrum(-1.0f),
                                R23_s,
                                R23_p,
                                phasor23_s,
                                phasor23_p);
  }
  else {
    /* Material is a dielectric. */
    float2 phasor23_real;
    const float2 R23 = fresnel_dielectric_polarized(
        -cos_theta_2, substrate_ior.re / film_ior, r_cos_theta_3, &phasor23_real);

    if (isequal(R23, one_float2())) {
      /* TIR at the bottom interface.
       * All the Airy summation math still simplifies to 1.0 in this case. */
      return one_spectrum();
    }

    R23_s = R23.x;
    R23_p = R23.y;
    phasor23_s = {phasor23_real.x, 0.0f};
    phasor23_p = {phasor23_real.y, 0.0f};
  }

  /* Compute optical path difference inside the thin film. */
  const float OPD = -2.0f * film_ior * thin_film.thickness * cos_theta_2;

  /* Compute full phase shifts due to reflection, as a complex number exp(i * (phi23 + phi21)).
   * This complex form avoids the atan2 and cos calls needed to directly get the phase shift. */
  const complex<SpectrumOrFloat> phasor_s = phasor23_s * -phasor12_real.x;
  const complex<SpectrumOrFloat> phasor_p = phasor23_p * -phasor12_real.y;

  /* Perform Airy summation and average the polarizations. */
  const Spectrum R_s = iridescence_airy_summation(kg, R12.x, R23_s, OPD, phasor_s);
  const Spectrum R_p = iridescence_airy_summation(kg, R12.y, R23_p, OPD, phasor_p);

  return saturate(mix(R_s, R_p, 0.5f));
}

CCL_NAMESPACE_END
