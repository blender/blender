/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf_types.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"

/* -------------------------------------------------------------------- */
/** \name Sheen BSDF
 * "Practical Multiple-Scattering Sheen Using Linearly Transformed Cosines" (2022)
 * https://tizianzeltner.com/projects/Zeltner2022Practical/
 * by Tizian Zeltner, Brent Burley, Matt Jen-Yuan Chiang
 * \{ */

struct Sheen {
  /* Entries of the LTC matrix. */
  float A, B;
  /* Directional hemispherical reflectance. */
  float R;

  static Sheen init(const UtilityTexture &util_tx, const float roughness, const float NV)
  {
    const float2 uv = float2(NV, roughness);
    const float4 data = util_tx.sample_lut(uv, UTIL_LTC_SHEEN_LAYER, UTIL_SHEEN_TEX_SIZE);

    return {.A = data.x, .B = data.y, .R = data.z};
  }

  /* Inverse of the linear transform, which is directly stored in the LTC LUT. */
  float3x3 M_inv() const
  {
    return float3x3(A, 0.0f, 0.0f, 0.0f, A, 0.0f, B, 0.0f, 1.0f);
  }

  /* Original linear transform. */
  float3x3 M() const
  {
    const float inv_A = 1.0f / A;
    return float3x3(inv_A, 0.0f, 0.0f, 0.0f, inv_A, 0.0f, B * inv_A, 0.0f, 1.0f);
  }

  /* Determinant of M^{-1}. */
  float det_M_inv() const
  {
    return square(A);
  }

  /* Probability of sampling direction L with normal direction N. */
  float pdf(const float3 L, const float3 N) const
  {
    return pdf_ltc(L, N, M(), M_inv(), det_M_inv());
  }
};

struct BsdfSheen : BsdfClosure::Interface {
  Sheen sheen;

  static BsdfSheen unpack(const BsdfClosure cl)
  {
    BsdfSheen bsdf = {};
    bsdf.color = cl.color;
    bsdf.N = cl.N;
    bsdf.sheen.A = bsdf.sheen.R = 1.0f;
    bsdf.sheen.B = 0.0f;
    return bsdf;
  }

  static BsdfSheen unpack(const UtilityTexture &util_tx, const float3 V, const BsdfClosure cl)
  {
    BsdfSheen bsdf = unpack(cl);
    bsdf.sheen = Sheen::init(util_tx, cl.data.x, dot(cl.N, V));
    return bsdf;
  }

  BsdfSample sample(const float3 rand, const float3 /*Vt*/, Thickness /*thickness*/) const final
  {
    BsdfSample result;
    /* Same as `sample_ltc(M, rand)` but unnormalized, so we can compute the Jacobian for pdf. */
    result.direction = sheen.M() * sample_cos_hemisphere(rand);
    const float l = length(result.direction);
    result.direction /= l;

    /* Same as `pdf_ltc()`, but saves a few matrix multiplications. */
    const float Jacobian = pow3f(l) * sheen.det_M_inv();
    result.pdf = M_1_PI * saturate(result.direction.z) * Jacobian;

    return result;
  }

  BsdfEval eval(const float3 /*V*/, const float3 L, Thickness /*thickness*/) const final
  {
    const float pdf = sheen.pdf(L, N);
    return {.throughput = pdf * sheen.R, .pdf = pdf, .weight = sheen.R};
  }

  LightProbeRay lightprobe_ray(float3 /*V*/, Thickness /*thickness*/) const final
  {
    LightProbeRay probe;
    probe.perceptual_roughness = perceived_roughness();
    /* According to "Real-Time Polygonal-Light Shading with Linearly Transformed Cosines"
     * by Eric Heitz et. al, the transformed median vector is the median vector of the transformed
     * distribution. */
    probe.dominant_direction = normalize(sheen.M() * float3(N));
    return probe;
  }

  LightEvalContext light_eval_context(const UtilityTexture & /*util_tx*/,
                                      float3 V,
                                      Thickness /*thickness*/) const final
  {
    const eevee::LTCData ltc_data = eevee::LTCData::from_M_inv(sheen.M_inv(), N, V);

    return {.N = N, .type = LIGHT_DIFFUSE, .ltc_data_packed = ltc_data.pack()};
  }

  float perceived_roughness() const final
  {
    return 1.0f;
  }
};

/** \} */
