/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

CCL_NAMESPACE_BEGIN

/* Compute fresnel reflectance. Also return the dot product of the refracted ray and the normal as
 * `cos_theta_t`, as it is used when computing the direction of the refracted ray. */
ccl_device float fresnel_dielectric(float cos_theta_i, float eta, ccl_private float *r_cos_theta_t)
{
  kernel_assert(!isnan_safe(cos_theta_i));

  /* Using Snell's law, calculate the squared cosine of the angle between the surface normal and
   * the transmitted ray. */
  const float eta_cos_theta_t_sq = sqr(eta) - (1.0f - sqr(cos_theta_i));
  if (eta_cos_theta_t_sq <= 0) {
    /* Total internal reflection. */
    *r_cos_theta_t = 0.0f;
    return 1.0f;
  }

  cos_theta_i = fabsf(cos_theta_i);
  /* Relative to the surface normal. */
  const float cos_theta_t = -safe_sqrtf(eta_cos_theta_t_sq) / eta;

  if (r_cos_theta_t) {
    *r_cos_theta_t = cos_theta_t;
  }

  /* Amplitudes of reflected waves. */
  const float r_s = (cos_theta_i + eta * cos_theta_t) / (cos_theta_i - eta * cos_theta_t);
  const float r_p = (cos_theta_t + eta * cos_theta_i) / (cos_theta_t - eta * cos_theta_i);

  return 0.5f * (sqr(r_s) + sqr(r_p));
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

ccl_device float fresnel_dielectric_cos(float cosi, float eta)
{
  // compute fresnel reflectance without explicitly computing
  // the refracted direction
  float c = fabsf(cosi);
  float g = eta * eta - 1 + c * c;
  if (g > 0) {
    g = sqrtf(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1) / (c * (g - c) + 1);
    return 0.5f * A * A * (1 + B * B);
  }
  return 1.0f;  // TIR(no refracted component)
}

ccl_device Spectrum fresnel_conductor(float cosi, const Spectrum eta, const Spectrum k)
{
  Spectrum cosi2 = make_spectrum(cosi * cosi);
  Spectrum one = make_spectrum(1.0f);
  Spectrum tmp_f = eta * eta + k * k;
  Spectrum tmp = tmp_f * cosi2;
  Spectrum Rparl2 = (tmp - (2.0f * eta * cosi) + one) / (tmp + (2.0f * eta * cosi) + one);
  Spectrum Rperp2 = (tmp_f - (2.0f * eta * cosi) + cosi2) / (tmp_f + (2.0f * eta * cosi) + cosi2);
  return (Rparl2 + Rperp2) * 0.5f;
}

ccl_device float ior_from_F0(float f0)
{
  const float sqrt_f0 = sqrtf(clamp(f0, 0.0f, 0.99f));
  return (1.0f + sqrt_f0) / (1.0f - sqrt_f0);
}

ccl_device float F0_from_ior(float ior)
{
  return sqr((ior - 1.0f) / (ior + 1.0f));
}

ccl_device float schlick_fresnel(float u)
{
  float m = clamp(1.0f - u, 0.0f, 1.0f);
  float m2 = m * m;
  return m2 * m2 * m;  // pow(m, 5)
}

/* Calculate the fresnel color, which is a blend between white and the F0 color */
ccl_device_forceinline Spectrum interpolate_fresnel_color(float3 L,
                                                          float3 H,
                                                          float ior,
                                                          Spectrum F0)
{
  /* Compute the real Fresnel term and remap it from real_F0..1 to F0..1.
   * The reason why we use this remapping instead of directly doing the
   * Schlick approximation mix(F0, 1.0, (1.0-cosLH)^5) is that for cases
   * with similar IORs (e.g. ice in water), the relative IOR can be close
   * enough to 1.0 that the Schlick approximation becomes inaccurate. */
  float real_F = fresnel_dielectric_cos(dot(L, H), ior);
  float real_F0 = fresnel_dielectric_cos(1.0f, ior);

  return mix(F0, one_spectrum(), inverse_lerp(real_F0, 1.0f, real_F));
}

/* If the shading normal results in specular reflection in the lower hemisphere, raise the shading
 * normal towards the geometry normal so that the specular reflection is just above the surface.
 * Only used for glossy materials. */
ccl_device float3 ensure_valid_specular_reflection(float3 Ng, float3 I, float3 N)
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
ccl_device float3 maybe_ensure_valid_specular_reflection(ccl_private ShaderData *sd, float3 N)
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

/* Computes the weight for base closure(s) which are layered under another closure.
 * layer_albedo is an estimate of the top layer's reflectivity, while weight is the closure weight
 * of the entire base+top combination. */
ccl_device_inline Spectrum closure_layering_weight(const Spectrum layer_albedo,
                                                   const Spectrum weight)
{
  return weight * saturatef(1.0f - reduce_max(safe_divide_color(layer_albedo, weight)));
}

CCL_NAMESPACE_END
