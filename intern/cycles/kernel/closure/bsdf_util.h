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

CCL_NAMESPACE_BEGIN

/* Compute fresnel reflectance for perpendicular (aka S-) and parallel (aka P-) polarized light.
 * If requested by the caller, r_phi is set to the phase shift on reflection.
 * Also returns the dot product of the refracted ray and the normal as `cos_theta_t`, as it is
 * used when computing the direction of the refracted ray. */
ccl_device float2 fresnel_dielectric_polarized(float cos_theta_i,
                                               const float eta,
                                               ccl_private float *r_cos_theta_t,
                                               ccl_private float2 *r_phi)
{
  kernel_assert(!isnan_safe(cos_theta_i));

  /* Using Snell's law, calculate the squared cosine of the angle between the surface normal and
   * the transmitted ray. */
  const float eta_cos_theta_t_sq = sqr(eta) - (1.0f - sqr(cos_theta_i));
  if (eta_cos_theta_t_sq <= 0) {
    /* Total internal reflection. */
    if (r_phi) {
      /* The following code would compute the proper phase shift on TIR.
       * However, for the current user of this computation (the iridescence code),
       * this doesn't actually affect the result, so don't bother with the computation for now.
       *
       * `const float fac = sqrtf(1.0f - sqr(cosThetaI) - sqr(eta));`
       * `r_phi->x = -2.0f * atanf(fac / cosThetaI);`
       * `r_phi->y = -2.0f * atanf(fac / (cosThetaI * sqr(eta)));`
       */
      *r_phi = zero_float2();
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

  if (r_phi) {
    *r_phi = make_float2(r_s < 0.0f, r_p < 0.0f) * M_PI_F;
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

ccl_device Spectrum fresnel_conductor(const float cosi, const Spectrum eta, const Spectrum k)
{
  const Spectrum cosi2 = make_spectrum(cosi * cosi);
  const Spectrum one = make_spectrum(1.0f);
  const Spectrum tmp_f = eta * eta + k * k;
  const Spectrum tmp = tmp_f * cosi2;
  const Spectrum Rparl2 = (tmp - (2.0f * eta * cosi) + one) / (tmp + (2.0f * eta * cosi) + one);
  const Spectrum Rperp2 = (tmp_f - (2.0f * eta * cosi) + cosi2) /
                          (tmp_f + (2.0f * eta * cosi) + cosi2);
  return (Rparl2 + Rperp2) * 0.5f;
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

/* Evaluate the F82 metallic model for the given parameters. */
ccl_device_inline Spectrum fresnel_f82(const float cosi, const Spectrum F0, const Spectrum B)
{
  const float s = saturatef(1.0f - cosi);
  const float s5 = sqr(sqr(s)) * s;
  const Spectrum F_schlick = mix(F0, one_spectrum(), s5);
  return saturate(F_schlick - B * cosi * s5 * s);
}

/* Approximates the average single-scattering Fresnel for a physical conductor. */
ccl_device_inline Spectrum fresnel_conductor_Fss(const Spectrum eta, const Spectrum k)
{
  /* In order to estimate Fss of the conductor, we fit the F82 model to it based on the
   * value at 0° and ~82° and then use the analytic expression for its Fss. */
  const Spectrum F0 = fresnel_conductor(1.0f, eta, k);
  const Spectrum F82 = fresnel_conductor(1.0f / 7.0f, eta, k);
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
 * The code here uses the Gaussian fit for the CIE XYZ curves that is provided
 * in the reference implementation.
 * For details on what this actually represents, see the paper.
 * In theory we should pre-compute the sensitivity functions for the working RGB
 * color-space, remap them to be functions of (light) frequency, take their Fourier
 * transform and store them as a LUT that gets looked up here.
 * In practice, using the XYZ fit and converting the result from XYZ to RGB is easier.
 */
ccl_device_inline Spectrum iridescence_lookup_sensitivity(KernelGlobals kg,
                                                          const float OPD,
                                                          const float shift)
{
  /* The LUT covers 0 to 60 um. */
  float x = M_2PI_F * OPD / 60000.0f;
  const int size = THIN_FILM_TABLE_SIZE;

  const float3 mag = make_float3(
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 0 * size, size),
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 1 * size, size),
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 2 * size, size));
  const float3 phase = make_float3(
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 3 * size, size),
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 4 * size, size),
      lookup_table_read(kg, x, kernel_data.tables.thin_film_table + 5 * size, size));

  return mag * cos(phase - shift);
}

ccl_device_inline float3 iridescence_airy_summation(KernelGlobals kg,
                                                    const float T121,
                                                    const float R12,
                                                    const float R23,
                                                    const float OPD,
                                                    const float phi)
{
  if (R23 == 1.0f) {
    /* Shortcut for TIR on the bottom interface. */
    return one_float3();
  }

  const float R123 = R12 * R23;
  const float r123 = sqrtf(R123);
  const float Rs = sqr(T121) * R23 / (1.0f - R123);

  /* Perform summation over path order differences (equation 10). */
  float3 R = make_float3(R12 + Rs); /* C0 */
  float Cm = (Rs - T121);
  /* Truncate after m=3, higher differences have barely any impact. */
  for (int m = 1; m < 4; m++) {
    Cm *= r123;
    R += Cm * 2.0f * iridescence_lookup_sensitivity(kg, m * OPD, m * phi);
  }
  return R;
}

ccl_device Spectrum fresnel_iridescence(KernelGlobals kg,
                                        float eta1,
                                        float eta2,
                                        float eta3,
                                        float cos_theta_1,
                                        const float thickness,
                                        ccl_private float *r_cos_theta_3)
{
  /* For films below 30nm, the wave-optic-based Airy summation approach no longer applies,
   * so blend towards the case without coating. */
  if (thickness < 30.0f) {
    eta2 = mix(eta1, eta2, smoothstep(0.0f, 30.0f, thickness));
  }

  float cos_theta_2;
  float2 phi12;
  float2 phi23;

  /* Compute reflection at the top interface (ambient to film). */
  const float2 R12 = fresnel_dielectric_polarized(cos_theta_1, eta2 / eta1, &cos_theta_2, &phi12);
  if (isequal(R12, one_float2())) {
    /* TIR at the top interface. */
    return one_spectrum();
  }

  /* Compute optical path difference inside the thin film. */
  const float OPD = -2.0f * eta2 * thickness * cos_theta_2;

  /* Compute reflection at the bottom interface (film to medium). */
  const float2 R23 = fresnel_dielectric_polarized(
      -cos_theta_2, eta3 / eta2, r_cos_theta_3, &phi23);
  if (isequal(R23, one_float2())) {
    /* TIR at the bottom interface.
     * All the Airy summation math still simplifies to 1.0 in this case. */
    return one_spectrum();
  }

  /* Compute helper parameters. */
  const float2 T121 = one_float2() - R12;
  const float2 phi = make_float2(M_PI_F, M_PI_F) - phi12 + phi23;

  /* Perform Airy summation and average the polarizations. */
  float3 R = mix(iridescence_airy_summation(kg, T121.x, R12.x, R23.x, OPD, phi.x),
                 iridescence_airy_summation(kg, T121.y, R12.y, R23.y, OPD, phi.y),
                 0.5f);

  return saturate(R);
}

CCL_NAMESPACE_END
