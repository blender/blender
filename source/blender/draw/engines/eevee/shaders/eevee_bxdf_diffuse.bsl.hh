/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf_types.bsl.hh"
#include "eevee_ltc_lut_lib.bsl.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_thickness_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_safe.bsl.hh"
#include "gpu_shader_ray.bsl.hh"

/* -------------------------------------------------------------------- */
/** \name Diffuse BSDF
 * \{ */

struct BsdfDiffuse : BsdfClosure::Interface {
  static BsdfDiffuse unpack(const BsdfClosure cl)
  {
    BsdfDiffuse bsdf = {};
    bsdf.color = cl.color;
    bsdf.N = cl.N;
    return bsdf;
  }

  BsdfClosure pack() const final
  {
    return {.color = color, .N = N, .type = CLOSURE_BSDF_DIFFUSE_ID};
  }

  /**
   * Returns a tangent space diffuse direction following the Lambertian distribution.
   *
   * \param rand_cylinder: random point on the unit cylinder (result of sample_cylinder).
   * The Z component can be biased towards 1.
   * \return pdf: the pdf of sampling the reflected/refracted ray. 0 if ray is invalid.
   */
  BsdfSample sample(const float3 rand_cylinder,
                    const float3 /*Vt*/,
                    Thickness /*thickness*/) const final
  {
    BsdfSample samp;
    samp.direction = sample_cos_hemisphere(rand_cylinder);
    samp.pdf = samp.direction.z * M_1_PI;
    return samp;
  }

  BsdfEval eval(float3 /*V*/, float3 L, Thickness /*thickness*/) const final
  {
    BsdfEval eval;
    eval.throughput = eval.pdf = saturate(dot(N, L)) * M_1_PI;
    eval.weight = 1.0f;
    return eval;
  }

  LightEvalContext light_eval_context(const UtilityTexture & /*util_tx*/,
                                      float3 V,
                                      Thickness /*thickness*/) const final
  {
    return {
        .N = N,
        .type = LIGHT_DIFFUSE,
        .ltc_data_packed = eevee::LTCData::identity(N, V).pack(),
    };
  }

  LightProbeRay lightprobe_ray(float3 /*V*/, Thickness /*thickness*/) const final
  {
    LightProbeRay probe;
    probe.perceptual_roughness = perceived_roughness();
    probe.dominant_direction = N;
    return probe;
  }

  float perceived_roughness() const final
  {
    return 1.0f;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Translucent BSDF
 * \{ */

struct BsdfTranslucent : BsdfClosure::Interface {
  static BsdfTranslucent unpack(const BsdfClosure cl)
  {
    BsdfTranslucent bsdf = {};
    bsdf.color = cl.color;
    bsdf.N = cl.N;
    return bsdf;
  }

  BsdfClosure pack() const final
  {
    return {.color = color, .N = N, .type = CLOSURE_BSDF_TRANSLUCENT_ID};
  }

  /**
   * Returns a tangent space diffuse direction following the Lambertian distribution.
   *
   * \param rand_cylinder: random point on the unit cylinder (result of sample_cylinder).
   * The Z component can be biased towards 1.
   * \return pdf: the pdf of sampling the reflected/refracted ray. 0 if ray is invalid.
   */
  BsdfSample sample(const float3 rand_cylinder,
                    const float3 /*Vt*/,
                    const Thickness thickness) const final
  {
    if (thickness.is_sphere()) {
      /* Two transmission events inside a sphere is a uniform sphere distribution. */
      float cos_theta = rand_cylinder.x * 2.0f - 1.0f;
      BsdfSample samp;
      samp.direction = float3(rand_cylinder.yz * sin_from_cos(cos_theta), -cos_theta);
      samp.pdf = 0.25f * M_1_PI;
      return samp;
    }

    /* Inverted cosine distribution. */
    BsdfSample samp;
    samp.direction = -sample_cos_hemisphere(rand_cylinder);
    samp.pdf = -samp.direction.z * M_1_PI;
    return samp;
  }

  BsdfEval eval(const float3 /*V*/, const float3 L, const Thickness thickness) const final
  {
    BsdfEval eval;
    /* Two transmission events inside a sphere is a uniform sphere distribution. */
    eval.throughput = eval.pdf = (thickness.is_sphere() ? 0.25f * M_1_PI : saturate(dot(-N, L)));
    eval.weight = 1.0f;
    return eval;
  }

  LightEvalContext light_eval_context(const UtilityTexture & /*util_tx*/,
                                      const float3 V,
                                      const Thickness thickness) const final
  {
    /* A translucent sphere lit by a light outside the sphere transmits the
     * light uniformly over the sphere. To mimic this phenomenon, the LTC evaluation
     * does not horizon-clip lights if we set `LTCFormFactorType::TwoSidedCosineSphere`.
     *
     * For slab model, the approximation has little to no impact on the lighting in practice,
     * only focusing the light a tiny bit. Using the flipped normal is good enough approximation.
     */
    const bool sphere_with_thickness = thickness.is_sphere();

    const LTCFormFactorType form_factor_type = sphere_with_thickness ?
                                                   LTCFormFactorType::TwoSidedCosineSphere :
                                                   LTCFormFactorType::OneSidedCosineSphereClipped;

    auto ltc_data = eevee::LTCData::identity(-N, V, form_factor_type);
    return {
        .N = -N,
        .type = sphere_with_thickness ? LIGHT_TRANSLUCENT_WITH_THICKNESS : LIGHT_DIFFUSE,
        .ltc_data_packed = ltc_data.pack(),
    };
  }

  LightProbeRay lightprobe_ray(float3 /*V*/, const Thickness thickness) const final
  {
    LightProbeRay probe;
    probe.perceptual_roughness = perceived_roughness();
    /* If using the spherical assumption, discard any directionality from the lighting. */
    probe.dominant_direction = thickness.is_sphere() ? float3(0.0f) : -N;
    return probe;
  }

  float perceived_roughness() const final
  {
    return 1.0f;
  }

  Ray ray_amend(float3 /*V*/, Ray ray, const Thickness thickness) const final
  {
    if (thickness.is_sphere()) {
      /* Ray direction is distributed on the whole sphere.
       * Move the ray origin to the sphere surface (with bias to avoid self-intersection). */
      constexpr float bias = 0.005f;
      ray.origin += (ray.direction - N) * thickness.value() * (0.5f + bias);
    }
    return ray;
  }
};

/** \} */

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

  /* Geometry term, [portsmouth2024] Eq. (13). */
  static float G(const float cos_theta)
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

  float multi_scatter(const float NL, const float NV) const
  {
    const float Eavg = A + ((2.0f - 5.6f * M_1_PI) / 3.0f) * B;
    /* Assume color is 1. */
    return (1.0f - E(NV)) * safe_divide(1.0f - E(NL), 1.0f - Eavg);
  }

  BsdfSample bsdf_sample(float3 rand, float3 /*V*/) const
  {
    BsdfSample samp;
    samp.direction = sample_cos_hemisphere(rand);
    samp.pdf = samp.direction.z * M_1_PI;
    return samp;
  }

  BsdfEval bsdf_eval(float3 N, float3 V, float3 L) const
  {
    const float NL = dot(N, L);
    if (NL <= 0.0f) {
      /* Fall back to diffuse. */
      BsdfEval eval;
      eval.throughput = eval.pdf = saturate(dot(N, L)) * M_1_PI;
      eval.weight = 1.0f;
      return eval;
    }

    const float NV = max(dot(N, V), 0.0f);

    BsdfEval eval;
    eval.weight = single_scatter(L, V, NL, NV) + multi_scatter(NL, NV);
    eval.pdf = M_1_PI * NL;
    eval.throughput = eval.weight * eval.pdf;
    return eval;
  }
};

struct BsdfOrenNayar : BsdfClosure::Interface {
  float roughness;

  static BsdfOrenNayar unpack(const BsdfClosure cl)
  {
    BsdfOrenNayar bsdf = {};
    bsdf.color = cl.color;
    bsdf.N = cl.N;
    bsdf.roughness = cl.data.x;
    return bsdf;
  }

  BsdfClosure pack() const final
  {
    return {
        .color = color,
        .N = N,
        .type = CLOSURE_BSDF_OREN_NAYAR_ID,
        .data = float4(roughness),
    };
  }

  BsdfSample sample(const float3 rand, const float3 Vt, Thickness /*thickness*/) const final
  {
    return OrenNayar::init(roughness).bsdf_sample(rand, Vt);
  }

  BsdfEval eval(float3 V, float3 L, Thickness /*thickness*/) const final
  {
    return OrenNayar::init(roughness).bsdf_eval(N, V, L);
  }

  LightProbeRay lightprobe_ray(float3 /*V*/, Thickness /*thickness*/) const final
  {
    LightProbeRay probe;
    probe.perceptual_roughness = perceived_roughness();
    probe.dominant_direction = N;
    return probe;
  }

  LightEvalContext light_eval_context(const UtilityTexture &util_tx,
                                      float3 V,
                                      Thickness /*thickness*/) const final
  {
    return {
        .N = N,
        .type = LIGHT_DIFFUSE,
        .ltc_data_packed =
            eevee::LTCData::sample_oren_nayar_lut(util_tx, N, V, dot(N, V), roughness).pack(),
    };
  }

  float perceived_roughness() const final
  {
    return 1.0f;
  }
};

/** \} */
