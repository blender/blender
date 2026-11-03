/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf_diffuse.bsl.hh"
#include "eevee_bxdf_types.bsl.hh"
#include "eevee_ltc_lut_lib.bsl.hh"

/* -------------------------------------------------------------------- */
/** \name Oren Nayar BSDF
 * \{ */

struct OrenNayar {
  /* Oren Nayar parameters. */
  float A;
  float B;

  static OrenNayar init(const float roughness)
  {
    const float A_ = 1.0f / (1.0f + (0.5f - 2.0f / 3.0f * M_1_PI) * roughness);
    return {.A = A_, .B = roughness * A_};
  }

  BsdfSample bsdf_sample(const float3 rand, const float3 /*Vt*/) const
  {
    return bxdf_diffuse_sample(rand);
  }

  /* Geometry term, [portsmouth2024] Eq. (13). */
  float G(const float cos_theta) const
  {
    if (cos_theta < 1e-6f) {
      /* Fall back to Taylor expansion at low `cos_theta` to avoid numerical problems. */
      return (M_PI_2 - 2.0f / 3.0f) - cos_theta;
    }
    const float sin_theta = sin_from_cos(cos_theta);
    const float theta = safe_acos(cos_theta);
    return sin_theta * (theta - 2.0f / 3.0f - sin_theta * cos_theta) +
           2.0f / 3.0f * (sin_theta / cos_theta) * (1.0f - square(sin_theta) * sin_theta);
  }

  float E(const float cos_theta) const
  {
    return A + B * M_1_PI * G(cos_theta);
  }

  /* Single-scatter term, [portsmouth2024] Eq. (10). */
  float single_scatter(const float3 L, const float3 V, const float NL, const float NV) const
  {
    const float s = dot(L, V) - NL * NV;
    const float s_over_t = (s > 0) ? s / (max(NL, NV) + FLT_MIN) : s;
    return A + B * s_over_t;
  }

  BsdfEval bsdf_eval(const float3 N, const float3 L, const float3 V) const
  {
    const float NL = dot(N, L);
    if (NL <= 0.0f) {
      /* Fall back to diffuse. */
      return bxdf_diffuse_eval(N, L);
    }

    const float NV = max(dot(N, V), 0.0f);

    const float Eavg = A + ((2.0f - 5.6f * M_1_PI) / 3.0f) * B;
    /* Assume color is 1. */
    const float multi_scatter = (1.0f - E(NV)) * safe_divide(1.0f - E(NL), 1.0f - Eavg);

    BsdfEval eval;
    eval.weight = single_scatter(L, V, NL, NV) + multi_scatter;
    eval.pdf = M_1_PI * NL;
    eval.throughput = eval.weight * eval.pdf;
    return eval;
  }
};

float bxdf_oren_nayar_perceived_roughness()
{
  return 1.0f;
}

LightProbeRay bxdf_oren_nayar_lightprobe(float3 N)
{
  LightProbeRay probe;
  probe.perceptual_roughness = bxdf_oren_nayar_perceived_roughness();
  probe.dominant_direction = N;
  return probe;
}

ClosureLight bxdf_oren_nayar_light(const UtilityTexture &util_tx, ClosureOrenNayar cl, float3 V)
{
  ClosureLight light;
  light.N = cl.N;
  light.type = LIGHT_DIFFUSE;
  eevee::LTCData::sample_oren_nayar_lut(util_tx, light.N, V, dot(cl.N, V), cl.roughness)
      .pack_to(light);
  return light;
}

/** \} */
