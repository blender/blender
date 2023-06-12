/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/sample/lcg.h"
#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

/* Most of the code is based on the supplemental implementations from
 * https://eheitzresearch.wordpress.com/240-2/. */

/* === GGX Microfacet distribution functions === */

/* Isotropic GGX microfacet distribution */
ccl_device_forceinline float D_ggx(float3 wm, float alpha)
{
  wm.z *= wm.z;
  alpha *= alpha;
  float tmp = (1.0f - wm.z) + alpha * wm.z;
  return alpha / max(M_PI_F * tmp * tmp, 1e-7f);
}

/* Anisotropic GGX microfacet distribution */
ccl_device_forceinline float D_ggx_aniso(const float3 wm, const float2 alpha)
{
  float slope_x = -wm.x / alpha.x;
  float slope_y = -wm.y / alpha.y;
  float tmp = wm.z * wm.z + slope_x * slope_x + slope_y * slope_y;

  return 1.0f / max(M_PI_F * tmp * tmp * alpha.x * alpha.y, 1e-7f);
}

/* Sample slope distribution (based on page 14 of the supplemental implementation). */
ccl_device_forceinline float2 mf_sampleP22_11(const float cosI,
                                              const float randx,
                                              const float randy)
{
  if (cosI > 0.9999f || fabsf(cosI) < 1e-6f) {
    const float r = sqrtf(randx / max(1.0f - randx, 1e-7f));
    const float phi = M_2PI_F * randy;
    return make_float2(r * cosf(phi), r * sinf(phi));
  }

  const float sinI = sin_from_cos(cosI);
  const float tanI = sinI / cosI;
  const float projA = 0.5f * (cosI + 1.0f);
  if (projA < 0.0001f)
    return make_float2(0.0f, 0.0f);
  const float A = 2.0f * randx * projA / cosI - 1.0f;
  float tmp = A * A - 1.0f;
  if (fabsf(tmp) < 1e-7f)
    return make_float2(0.0f, 0.0f);
  tmp = 1.0f / tmp;
  const float D = safe_sqrtf(tanI * tanI * tmp * tmp - (A * A - tanI * tanI) * tmp);

  const float slopeX2 = tanI * tmp + D;
  const float slopeX = (A < 0.0f || slopeX2 > 1.0f / tanI) ? (tanI * tmp - D) : slopeX2;

  float U2;
  if (randy >= 0.5f)
    U2 = 2.0f * (randy - 0.5f);
  else
    U2 = 2.0f * (0.5f - randy);
  const float z = (U2 * (U2 * (U2 * 0.27385f - 0.73369f) + 0.46341f)) /
                  (U2 * (U2 * (U2 * 0.093073f + 0.309420f) - 1.0f) + 0.597999f);
  const float slopeY = z * sqrtf(1.0f + slopeX * slopeX);

  if (randy >= 0.5f)
    return make_float2(slopeX, slopeY);
  else
    return make_float2(slopeX, -slopeY);
}

/* Visible normal sampling for the GGX distribution
 * (based on page 7 of the supplemental implementation). */
ccl_device_forceinline float3 mf_sample_vndf(const float3 wi,
                                             const float2 alpha,
                                             const float randx,
                                             const float randy)
{
  const float3 wi_11 = normalize(make_float3(alpha.x * wi.x, alpha.y * wi.y, wi.z));
  const float2 slope_11 = mf_sampleP22_11(wi_11.z, randx, randy);

  const float3 cossin_phi = safe_normalize(make_float3(wi_11.x, wi_11.y, 0.0f));
  const float slope_x = alpha.x * (cossin_phi.x * slope_11.x - cossin_phi.y * slope_11.y);
  const float slope_y = alpha.y * (cossin_phi.y * slope_11.x + cossin_phi.x * slope_11.y);

  kernel_assert(isfinite(slope_x));
  return normalize(make_float3(-slope_x, -slope_y, 1.0f));
}

/* === Phase functions: Glossy and Glass === */

/* Phase function for reflective materials. */
ccl_device_forceinline float3 mf_sample_phase_glossy(const float3 wi,
                                                     ccl_private Spectrum *weight,
                                                     const float3 wm)
{
  return -wi + 2.0f * wm * dot(wi, wm);
}

ccl_device_forceinline Spectrum mf_eval_phase_glossy(const float3 w,
                                                     const float lambda,
                                                     const float3 wo,
                                                     const float2 alpha)
{
  if (w.z > 0.9999f)
    return zero_spectrum();

  const float3 wh = normalize(wo - w);
  if (wh.z < 0.0f)
    return zero_spectrum();

  float pArea = (w.z < -0.9999f) ? 1.0f : lambda * w.z;

  const float dotW_WH = dot(-w, wh);
  if (dotW_WH < 0.0f)
    return zero_spectrum();

  float phase = max(0.0f, dotW_WH) * 0.25f / max(pArea * dotW_WH, 1e-7f);
  if (alpha.x == alpha.y)
    phase *= D_ggx(wh, alpha.x);
  else
    phase *= D_ggx_aniso(wh, alpha);

  return make_spectrum(phase);
}

/* Phase function for dielectric transmissive materials, including both reflection and refraction
 * according to the dielectric fresnel term. */
ccl_device_forceinline float3 mf_sample_phase_glass(const float3 wi,
                                                    const float eta,
                                                    const float3 wm,
                                                    const float randV,
                                                    ccl_private bool *outside)
{
  float cosI = dot(wi, wm);
  float f = fresnel_dielectric_cos(cosI, eta);
  if (randV < f) {
    *outside = true;
    return -wi + 2.0f * wm * cosI;
  }
  *outside = false;
  float inv_eta = 1.0f / eta;
  float cosT = -safe_sqrtf(1.0f - (1.0f - cosI * cosI) * inv_eta * inv_eta);
  return normalize(wm * (cosI * inv_eta + cosT) - wi * inv_eta);
}

ccl_device_forceinline Spectrum mf_eval_phase_glass(const float3 w,
                                                    const float lambda,
                                                    const float3 wo,
                                                    const bool wo_outside,
                                                    const float2 alpha,
                                                    const float eta)
{
  if (w.z > 0.9999f)
    return zero_spectrum();

  float pArea = (w.z < -0.9999f) ? 1.0f : lambda * w.z;
  float v;
  if (wo_outside) {
    const float3 wh = normalize(wo - w);
    if (wh.z < 0.0f)
      return zero_spectrum();

    const float dotW_WH = dot(-w, wh);
    v = fresnel_dielectric_cos(dotW_WH, eta) * max(0.0f, dotW_WH) * D_ggx(wh, alpha.x) * 0.25f /
        (pArea * dotW_WH);
  }
  else {
    float3 wh = normalize(wo * eta - w);
    if (wh.z < 0.0f)
      wh = -wh;
    const float dotW_WH = dot(-w, wh), dotWO_WH = dot(wo, wh);
    if (dotW_WH < 0.0f)
      return zero_spectrum();

    float temp = dotW_WH + eta * dotWO_WH;
    v = (1.0f - fresnel_dielectric_cos(dotW_WH, eta)) * max(0.0f, dotW_WH) * max(0.0f, -dotWO_WH) *
        D_ggx(wh, alpha.x) / (pArea * temp * temp);
  }

  return make_spectrum(v);
}

/* === Utility functions for the random walks === */

/* Smith Lambda function for GGX (based on page 12 of the supplemental implementation). */
ccl_device_forceinline float mf_lambda(const float3 w, const float2 alpha)
{
  if (w.z > 0.9999f)
    return 0.0f;
  else if (w.z < -0.9999f)
    return -0.9999f;

  const float inv_wz2 = 1.0f / max(w.z * w.z, 1e-7f);
  const float2 wa = make_float2(w.x, w.y) * alpha;
  float v = sqrtf(1.0f + dot(wa, wa) * inv_wz2);
  if (w.z <= 0.0f)
    v = -v;

  return 0.5f * (v - 1.0f);
}

/* Height distribution CDF (based on page 4 of the supplemental implementation). */
ccl_device_forceinline float mf_invC1(const float h)
{
  return 2.0f * saturatef(h) - 1.0f;
}

ccl_device_forceinline float mf_C1(const float h)
{
  return saturatef(0.5f * (h + 1.0f));
}

/* Masking function (based on page 16 of the supplemental implementation). */
ccl_device_forceinline float mf_G1(const float3 w, const float C1, const float lambda)
{
  if (w.z > 0.9999f)
    return 1.0f;
  if (w.z < 1e-5f)
    return 0.0f;
  return powf(C1, lambda);
}

/* Sampling from the visible height distribution (based on page 17 of the supplemental
 * implementation). */
ccl_device_forceinline bool mf_sample_height(const float3 w,
                                             ccl_private float *h,
                                             ccl_private float *C1,
                                             ccl_private float *G1,
                                             ccl_private float *lambda,
                                             const float U)
{
  if (w.z > 0.9999f)
    return false;
  if (w.z < -0.9999f) {
    *C1 *= U;
    *h = mf_invC1(*C1);
    *G1 = mf_G1(w, *C1, *lambda);
  }
  else if (fabsf(w.z) >= 0.0001f) {
    if (U > 1.0f - *G1)
      return false;
    if (*lambda >= 0.0f) {
      *C1 = 1.0f;
    }
    else {
      *C1 *= powf(1.0f - U, -1.0f / *lambda);
    }
    *h = mf_invC1(*C1);
    *G1 = mf_G1(w, *C1, *lambda);
  }
  return true;
}

/* === PDF approximations for the different phase functions. ===
 * As explained in bsdf_microfacet_multi_impl.h, using approximations with MIS still produces an
 * unbiased result. */

/* Approximation for the albedo of the single-scattering GGX distribution,
 * the missing energy is then approximated as a diffuse reflection for the PDF. */
ccl_device_forceinline float mf_ggx_albedo(float r)
{
  float albedo = 0.806495f * expf(-1.98712f * r * r) + 0.199531f;
  albedo -= ((((((1.76741f * r - 8.43891f) * r + 15.784f) * r - 14.398f) * r + 6.45221f) * r -
              1.19722f) *
                 r +
             0.027803f) *
                r +
            0.00568739f;
  return saturatef(albedo);
}

ccl_device_inline float mf_ggx_transmission_albedo(float a, float ior)
{
  if (ior < 1.0f) {
    ior = 1.0f / ior;
  }
  a = saturatef(a);
  ior = clamp(ior, 1.0f, 3.0f);
  float I_1 = 0.0476898f * expf(-0.978352f * (ior - 0.65657f) * (ior - 0.65657f)) -
              0.033756f * ior + 0.993261f;
  float R_1 = (((0.116991f * a - 0.270369f) * a + 0.0501366f) * a - 0.00411511f) * a + 1.00008f;
  float I_2 = (((-2.08704f * ior + 26.3298f) * ior - 127.906f) * ior + 292.958f) * ior - 287.946f +
              199.803f / (ior * ior) - 101.668f / (ior * ior * ior);
  float R_2 = ((((5.3725f * a - 24.9307f) * a + 22.7437f) * a - 3.40751f) * a + 0.0986325f) * a +
              0.00493504f;

  return saturatef(1.0f + I_2 * R_2 * 0.0019127f - (1.0f - I_1) * (1.0f - R_1) * 9.3205f);
}

ccl_device_forceinline float mf_ggx_pdf(const float3 wi, const float3 wo, const float alpha)
{
  float D = D_ggx(normalize(wi + wo), alpha);
  float lambda = mf_lambda(wi, make_float2(alpha, alpha));
  float singlescatter = 0.25f * D / max((1.0f + lambda) * wi.z, 1e-7f);

  float multiscatter = wo.z * M_1_PI_F;

  float albedo = mf_ggx_albedo(alpha);
  return albedo * singlescatter + (1.0f - albedo) * multiscatter;
}

ccl_device_forceinline float mf_ggx_aniso_pdf(const float3 wi, const float3 wo, const float2 alpha)
{
  float D = D_ggx_aniso(normalize(wi + wo), alpha);
  float lambda = mf_lambda(wi, alpha);
  float singlescatter = 0.25f * D / max((1.0f + lambda) * wi.z, 1e-7f);

  float multiscatter = wo.z * M_1_PI_F;

  float albedo = mf_ggx_albedo(sqrtf(alpha.x * alpha.y));
  return albedo * singlescatter + (1.0f - albedo) * multiscatter;
}

ccl_device_forceinline float mf_glass_pdf(const float3 wi,
                                          const float3 wo,
                                          const float alpha,
                                          const float eta)
{
  bool reflective = (wi.z * wo.z > 0.0f);

  float wh_len;
  float3 wh = normalize_len(wi + (reflective ? wo : (wo * eta)), &wh_len);
  if (wh.z < 0.0f)
    wh = -wh;
  float3 r_wi = (wi.z < 0.0f) ? -wi : wi;
  float lambda = mf_lambda(r_wi, make_float2(alpha, alpha));
  float D = D_ggx(wh, alpha);
  float fresnel = fresnel_dielectric_cos(dot(r_wi, wh), eta);

  float multiscatter = fabsf(wo.z * M_1_PI_F);
  if (reflective) {
    float singlescatter = 0.25f * D / max((1.0f + lambda) * r_wi.z, 1e-7f);
    float albedo = mf_ggx_albedo(alpha);
    return fresnel * (albedo * singlescatter + (1.0f - albedo) * multiscatter);
  }
  else {
    float singlescatter = fabsf(dot(r_wi, wh) * dot(wo, wh) * D * eta * eta /
                                max((1.0f + lambda) * r_wi.z * wh_len * wh_len, 1e-7f));
    float albedo = mf_ggx_transmission_albedo(alpha, eta);
    return (1.0f - fresnel) * (albedo * singlescatter + (1.0f - albedo) * multiscatter);
  }
}

/* === Actual random walk implementations === */
/* One version of mf_eval and mf_sample per phase function. */

#define MF_NAME_JOIN(x, y) x##_##y
#define MF_NAME_EVAL(x, y) MF_NAME_JOIN(x, y)
#define MF_FUNCTION_FULL_NAME(prefix) MF_NAME_EVAL(prefix, MF_PHASE_FUNCTION)

#define MF_PHASE_FUNCTION glass
#define MF_MULTI_GLASS
#include "kernel/closure/bsdf_microfacet_multi_impl.h"

#define MF_PHASE_FUNCTION glossy
#define MF_MULTI_GLOSSY
#include "kernel/closure/bsdf_microfacet_multi_impl.h"

ccl_device void bsdf_microfacet_multi_ggx_blur(ccl_private ShaderClosure *sc, float roughness)
{
  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)sc;

  bsdf->alpha_x = fmaxf(roughness, bsdf->alpha_x);
  bsdf->alpha_y = fmaxf(roughness, bsdf->alpha_y);
}

/* === Closure implementations === */

/* Multi-scattering GGX Glossy closure */

ccl_device int bsdf_microfacet_multi_ggx_common_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = clamp(bsdf->alpha_x, 1e-4f, 1.0f);
  bsdf->alpha_y = clamp(bsdf->alpha_y, 1e-4f, 1.0f);

  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_NEEDS_LCG;
}

ccl_device int bsdf_microfacet_multi_ggx_setup(ccl_private MicrofacetBsdf *bsdf)
{
  if (is_zero(bsdf->T))
    bsdf->T = make_float3(1.0f, 0.0f, 0.0f);

  ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)bsdf->fresnel;
  fresnel->color = saturate(fresnel->color);

  bsdf->fresnel_type = MicrofacetFresnel::CONSTANT;
  bsdf->type = CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID;

  return bsdf_microfacet_multi_ggx_common_setup(bsdf);
}

ccl_device int bsdf_microfacet_multi_ggx_fresnel_setup(ccl_private MicrofacetBsdf *bsdf,
                                                       ccl_private const ShaderData *sd)
{
  if (is_zero(bsdf->T))
    bsdf->T = make_float3(1.0f, 0.0f, 0.0f);

  ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)bsdf->fresnel;
  fresnel->color = saturate(fresnel->color);
  fresnel->cspec0 = saturate(fresnel->cspec0);

  bsdf->fresnel_type = MicrofacetFresnel::PRINCIPLED_V1;
  bsdf->type = CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID;
  bsdf->sample_weight *= average(bsdf_microfacet_estimate_fresnel(sd, bsdf, true, true));

  return bsdf_microfacet_multi_ggx_common_setup(bsdf);
}

ccl_device int bsdf_microfacet_multi_ggx_refraction_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_y = bsdf->alpha_x;

  ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)bsdf->fresnel;
  fresnel->color = saturate(fresnel->color);

  bsdf->fresnel_type = MicrofacetFresnel::CONSTANT;
  bsdf->type = CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID;

  return bsdf_microfacet_multi_ggx_common_setup(bsdf);
}

ccl_device Spectrum bsdf_microfacet_multi_ggx_eval(ccl_private const ShaderClosure *sc,
                                                   const float3 Ng,
                                                   const float3 wi,
                                                   const float3 wo,
                                                   ccl_private float *pdf,
                                                   ccl_private uint *lcg_state)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  const float cosNgO = dot(Ng, wo);

  if ((cosNgO < 0.0f) || bsdf->alpha_x * bsdf->alpha_y < 1e-7f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float3 X, Y, Z;
  Z = bsdf->N;

  /* Ensure that the both directions are on the outside w.r.t. the shading normal. */
  if (dot(Z, wi) <= 0.0f || dot(Z, wo) <= 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  Spectrum color, cspec0;
  bool use_fresnel;
  if (bsdf->fresnel_type == MicrofacetFresnel::PRINCIPLED_V1) {
    ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)bsdf->fresnel;
    use_fresnel = true;
    color = fresnel->color;
    cspec0 = fresnel->cspec0;
  }
  else {
    kernel_assert(bsdf->fresnel_type == MicrofacetFresnel::CONSTANT);
    ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)bsdf->fresnel;
    use_fresnel = false;
    color = fresnel->color;
    cspec0 = zero_spectrum();
  }

  bool is_aniso = (bsdf->alpha_x != bsdf->alpha_y);
  if (is_aniso)
    make_orthonormals_tangent(Z, bsdf->T, &X, &Y);
  else
    make_orthonormals(Z, &X, &Y);

  float3 local_I = make_float3(dot(wi, X), dot(wi, Y), dot(wi, Z));
  float3 local_O = make_float3(dot(wo, X), dot(wo, Y), dot(wo, Z));

  if (is_aniso)
    *pdf = mf_ggx_aniso_pdf(local_I, local_O, make_float2(bsdf->alpha_x, bsdf->alpha_y));
  else
    *pdf = mf_ggx_pdf(local_I, local_O, bsdf->alpha_x);

  if (*pdf <= 0.f) {
    *pdf = 0.f;
    return make_float3(0.f, 0.f, 0.f);
  }

  return mf_eval_glossy(local_I,
                        local_O,
                        true,
                        color,
                        bsdf->alpha_x,
                        bsdf->alpha_y,
                        lcg_state,
                        bsdf->ior,
                        use_fresnel,
                        cspec0);
}

ccl_device int bsdf_microfacet_multi_ggx_sample(KernelGlobals kg,
                                                ccl_private const ShaderClosure *sc,
                                                float3 Ng,
                                                float3 wi,
                                                float randu,
                                                float randv,
                                                ccl_private Spectrum *eval,
                                                ccl_private float3 *wo,
                                                ccl_private float *pdf,
                                                ccl_private uint *lcg_state,
                                                ccl_private float2 *sampled_roughness,
                                                ccl_private float *eta)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;

  float3 X, Y, Z;
  Z = bsdf->N;

  /* Ensure that the view direction is on the outside w.r.t. the shading normal. */
  if (dot(Z, wi) <= 0.0f) {
    *pdf = 0.0f;
    return LABEL_NONE;
  }

  /* Special case: Extremely low roughness.
   * Don't bother with microfacets, just do specular reflection. */
  if (bsdf->alpha_x * bsdf->alpha_y < 1e-7f) {
    *wo = 2 * dot(Z, wi) * Z - wi;
    if (dot(Ng, *wo) <= 0.0f) {
      *pdf = 0.0f;
      return LABEL_NONE;
    }
    *pdf = 1e6f;
    *eval = make_spectrum(1e6f);
    return LABEL_REFLECT | LABEL_SINGULAR;
  }

  Spectrum color, cspec0;
  bool use_fresnel;
  if (bsdf->fresnel_type == MicrofacetFresnel::PRINCIPLED_V1) {
    ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)bsdf->fresnel;
    use_fresnel = true;
    color = fresnel->color;
    cspec0 = fresnel->cspec0;
  }
  else {
    kernel_assert(bsdf->fresnel_type == MicrofacetFresnel::CONSTANT);
    ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)bsdf->fresnel;
    use_fresnel = false;
    color = fresnel->color;
    cspec0 = zero_spectrum();
  }

  *eta = bsdf->ior;
  *sampled_roughness = make_float2(bsdf->alpha_x, bsdf->alpha_y);

  bool is_aniso = (bsdf->alpha_x != bsdf->alpha_y);
  if (is_aniso)
    make_orthonormals_tangent(Z, bsdf->T, &X, &Y);
  else
    make_orthonormals(Z, &X, &Y);

  float3 local_I = make_float3(dot(wi, X), dot(wi, Y), dot(wi, Z));
  float3 local_O;

  *eval = mf_sample_glossy(local_I,
                           &local_O,
                           color,
                           bsdf->alpha_x,
                           bsdf->alpha_y,
                           lcg_state,
                           bsdf->ior,
                           use_fresnel,
                           cspec0);
  *wo = X * local_O.x + Y * local_O.y + Z * local_O.z;

  /* Ensure that the light direction is on the outside w.r.t. the geometry normal. */
  if (dot(Ng, *wo) <= 0.0f) {
    *pdf = 0.0f;
    return LABEL_NONE;
  }

  if (is_aniso)
    *pdf = mf_ggx_aniso_pdf(local_I, local_O, make_float2(bsdf->alpha_x, bsdf->alpha_y));
  else
    *pdf = mf_ggx_pdf(local_I, local_O, bsdf->alpha_x);
  *pdf = fmaxf(0.f, *pdf);
  *eval *= *pdf;

  return LABEL_REFLECT | LABEL_GLOSSY;
}

/* Multi-scattering GGX Glass closure */

ccl_device int bsdf_microfacet_multi_ggx_glass_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = clamp(bsdf->alpha_x, 1e-4f, 1.0f);
  bsdf->alpha_y = bsdf->alpha_x;
  bsdf->ior = max(0.0f, bsdf->ior);

  ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)bsdf->fresnel;
  fresnel->color = saturate(fresnel->color);

  bsdf->fresnel_type = MicrofacetFresnel::CONSTANT;
  bsdf->type = CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID;

  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_NEEDS_LCG | SD_BSDF_HAS_TRANSMISSION;
}

ccl_device int bsdf_microfacet_multi_ggx_glass_fresnel_setup(ccl_private MicrofacetBsdf *bsdf,
                                                             ccl_private const ShaderData *sd)
{
  bsdf->alpha_x = clamp(bsdf->alpha_x, 1e-4f, 1.0f);
  bsdf->alpha_y = bsdf->alpha_x;
  bsdf->ior = max(0.0f, bsdf->ior);

  ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)bsdf->fresnel;
  fresnel->color = saturate(fresnel->color);
  fresnel->cspec0 = saturate(fresnel->cspec0);

  bsdf->fresnel_type = MicrofacetFresnel::PRINCIPLED_V1;
  bsdf->type = CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID;
  bsdf->sample_weight *= average(bsdf_microfacet_estimate_fresnel(sd, bsdf, true, true));

  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_NEEDS_LCG;
}

ccl_device Spectrum bsdf_microfacet_multi_ggx_glass_eval(ccl_private const ShaderClosure *sc,
                                                         const float3 wi,
                                                         const float3 wo,
                                                         ccl_private float *pdf,
                                                         ccl_private uint *lcg_state)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;

  if (bsdf->alpha_x * bsdf->alpha_y < 1e-7f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float3 X, Y, Z;
  Z = bsdf->N;
  make_orthonormals(Z, &X, &Y);

  float3 local_I = make_float3(dot(wi, X), dot(wi, Y), dot(wi, Z));
  float3 local_O = make_float3(dot(wo, X), dot(wo, Y), dot(wo, Z));

  const bool is_transmission = local_O.z < 0.0f;

  Spectrum color, cspec0;
  bool use_fresnel;
  if (bsdf->fresnel_type == MicrofacetFresnel::PRINCIPLED_V1) {
    ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)bsdf->fresnel;
    use_fresnel = true;
    color = fresnel->color;
    cspec0 = is_transmission ? fresnel->color : fresnel->cspec0;
  }
  else {
    kernel_assert(bsdf->fresnel_type == MicrofacetFresnel::CONSTANT);
    ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)bsdf->fresnel;
    use_fresnel = false;
    color = fresnel->color;
    cspec0 = zero_spectrum();
  }

  *pdf = mf_glass_pdf(local_I, local_O, bsdf->alpha_x, bsdf->ior);
  kernel_assert(*pdf >= 0.f);
  return mf_eval_glass(local_I,
                       local_O,
                       !is_transmission,
                       color,
                       bsdf->alpha_x,
                       bsdf->alpha_y,
                       lcg_state,
                       bsdf->ior,
                       !is_transmission && use_fresnel,
                       cspec0);
}

ccl_device int bsdf_microfacet_multi_ggx_glass_sample(KernelGlobals kg,
                                                      ccl_private const ShaderClosure *sc,
                                                      float3 Ng,
                                                      float3 wi,
                                                      float randu,
                                                      float randv,
                                                      ccl_private Spectrum *eval,
                                                      ccl_private float3 *wo,
                                                      ccl_private float *pdf,
                                                      ccl_private uint *lcg_state,
                                                      ccl_private float2 *sampled_roughness,
                                                      ccl_private float *eta)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;

  float3 X, Y, Z;
  Z = bsdf->N;

  *eta = bsdf->ior;
  *sampled_roughness = make_float2(bsdf->alpha_x, bsdf->alpha_y);

  if (bsdf->alpha_x * bsdf->alpha_y < 1e-7f) {
    float3 T;
    bool inside;
    float fresnel = fresnel_dielectric(bsdf->ior, Z, wi, &T, &inside);

    *pdf = 1e6f;
    *eval = make_spectrum(1e6f);
    if (randu < fresnel) {
      *wo = 2 * dot(Z, wi) * Z - wi;
      return LABEL_REFLECT | LABEL_SINGULAR;
    }
    else {
      *wo = T;
      return LABEL_TRANSMIT | LABEL_SINGULAR;
    }
  }

  Spectrum color, cspec0;
  bool use_fresnel;
  if (bsdf->fresnel_type == MicrofacetFresnel::PRINCIPLED_V1) {
    ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)bsdf->fresnel;
    use_fresnel = true;
    color = fresnel->color;
    cspec0 = fresnel->cspec0;
  }
  else {
    kernel_assert(bsdf->fresnel_type == MicrofacetFresnel::CONSTANT);
    ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)bsdf->fresnel;
    use_fresnel = false;
    color = fresnel->color;
    cspec0 = zero_spectrum();
  }

  make_orthonormals(Z, &X, &Y);

  float3 local_I = make_float3(dot(wi, X), dot(wi, Y), dot(wi, Z));
  float3 local_O;

  *eval = mf_sample_glass(local_I,
                          &local_O,
                          color,
                          bsdf->alpha_x,
                          bsdf->alpha_y,
                          lcg_state,
                          bsdf->ior,
                          use_fresnel,
                          cspec0);
  *pdf = mf_glass_pdf(local_I, local_O, bsdf->alpha_x, bsdf->ior);
  kernel_assert(*pdf >= 0.f);
  *eval *= *pdf;

  *wo = X * local_O.x + Y * local_O.y + Z * local_O.z;
  if (local_O.z * local_I.z > 0.0f) {
    return LABEL_REFLECT | LABEL_GLOSSY;
  }
  else {
    return LABEL_TRANSMIT | LABEL_GLOSSY;
  }
}

CCL_NAMESPACE_END
