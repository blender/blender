/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

CCL_NAMESPACE_BEGIN

/* Compute fresnel reflectance for perpendicular (aka S-) and parallel (aka P-) polarized light.
 * If requested by the caller, r_phi is set to the phase shift on reflection.
 * Also returns the dot product of the refracted ray and the normal as `cos_theta_t`, as it is
 * used when computing the direction of the refracted ray. */
ccl_device float2 fresnel_dielectric_polarized(float cos_theta_i,
                                               float eta,
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
ccl_device_forceinline float fresnel_dielectric(float cos_theta_i,
                                                float eta,
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
ccl_device_inline Spectrum iridescence_lookup_sensitivity(float OPD, float shift)
{
  float phase = M_2PI_F * OPD * 1e-9f;
  float3 val = make_float3(5.4856e-13f, 4.4201e-13f, 5.2481e-13f);
  float3 pos = make_float3(1.6810e+06f, 1.7953e+06f, 2.2084e+06f);
  float3 var = make_float3(4.3278e+09f, 9.3046e+09f, 6.6121e+09f);

  float3 xyz = val * sqrt(M_2PI_F * var) * cos(pos * phase + shift) * exp(-sqr(phase) * var);
  xyz.x += 1.64408e-8f * cosf(2.2399e+06f * phase + shift) * expf(-4.5282e+09f * sqr(phase));
  return xyz / 1.0685e-7f;
}

ccl_device_inline float3
iridescence_airy_summation(float T121, float R12, float R23, float OPD, float phi)
{
  if (R23 == 1.0f) {
    /* Shortcut for TIR on the bottom interface. */
    return one_float3();
  }

  float R123 = R12 * R23;
  float r123 = sqrtf(R123);
  float Rs = sqr(T121) * R23 / (1.0f - R123);

  /* Perform summation over path order differences (equation 10). */
  float3 R = make_float3(R12 + Rs); /* C0 */
  float Cm = (Rs - T121);
  /* Truncate after m=3, higher differences have barely any impact. */
  for (int m = 1; m < 4; m++) {
    Cm *= r123;
    R += Cm * 2.0f * iridescence_lookup_sensitivity(m * OPD, m * phi);
  }
  return R;
}

ccl_device Spectrum fresnel_iridescence(KernelGlobals kg,
                                        float eta1,
                                        float eta2,
                                        float eta3,
                                        float cos_theta_1,
                                        float thickness,
                                        ccl_private float *r_cos_theta_3)
{
  /* For films below 30nm, the wave-optic-based Airy summation approach no longer applies,
   * so blend towards the case without coating. */
  if (thickness < 30.0f) {
    eta2 = mix(eta1, eta2, smoothstep(0.0f, 30.0f, thickness));
  }

  float cos_theta_2;
  float2 phi12, phi23;

  /* Compute reflection at the top interface (ambient to film). */
  float2 R12 = fresnel_dielectric_polarized(cos_theta_1, eta2 / eta1, &cos_theta_2, &phi12);
  if (isequal(R12, one_float2())) {
    /* TIR at the top interface. */
    return one_spectrum();
  }

  /* Compute optical path difference inside the thin film. */
  float OPD = -2.0f * eta2 * thickness * cos_theta_2;

  /* Compute reflection at the bottom interface (film to medium). */
  float2 R23 = fresnel_dielectric_polarized(-cos_theta_2, eta3 / eta2, r_cos_theta_3, &phi23);
  if (isequal(R23, one_float2())) {
    /* TIR at the bottom interface.
     * All the Airy summation math still simplifies to 1.0 in this case. */
    return one_spectrum();
  }

  /* Compute helper parameters. */
  float2 T121 = one_float2() - R12;
  float2 phi = make_float2(M_PI_F, M_PI_F) - phi12 + phi23;

  /* Perform Airy summation and average the polarizations. */
  float3 R = mix(iridescence_airy_summation(T121.x, R12.x, R23.x, OPD, phi.x),
                 iridescence_airy_summation(T121.y, R12.y, R23.y, OPD, phi.y),
                 0.5f);

  /* Color space conversion here is tricky.
   * In theory, the correct thing would be to compute the spectral color matching functions
   * for the RGB channels, take their Fourier transform in wavelength parametrization, and
   * then use that in iridescence_lookup_sensitivity().
   * To avoid this complexity, the code here instead uses the reference implementation's
   * Gaussian fit of the CIE XYZ curves. However, this means that at this point, R is in
   * XYZ values, not RGB.
   * Additionally, since I is a reflectivity, not a luminance, the spectral color matching
   * functions should be multiplied by the reference illuminant. Since the fit is based on
   * the "raw" CIE XYZ curves, the reference illuminant implicitly is a constant spectrum,
   * meaning Illuminant E.
   * Therefore, we can't just use the regular XYZ->RGB conversion here, we need to include
   * a chromatic adaption from E to whatever the white point of the working color space is.
   * The proper way to do this would be a Von Kries-style transform, but to keep it simple,
   * we just multiply by the white point here.
   *
   * Note: The reference implementation sidesteps all this by just hard-coding a XYZ->CIE RGB
   * matrix. Since CIE RGB uses E as its white point, this sidesteps the chromatic adaption
   * topic, but the primary colors don't match (unless you happen to actually work in CIE RGB.)
   */
  R *= float4_to_float3(kernel_data.film.white_xyz);
  return saturate(xyz_to_rgb(kg, R));
}

CCL_NAMESPACE_END
