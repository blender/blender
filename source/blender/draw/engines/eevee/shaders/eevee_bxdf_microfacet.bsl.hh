/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_bxdf_types.bsl.hh"
#include "eevee_ltc_lut_lib.bsl.hh"
#include "eevee_thickness_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_fast.bsl.hh"
#include "gpu_shader_math_vector.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"
#include "gpu_shader_ray.bsl.hh"

/* -------------------------------------------------------------------- */
/** \name Microfacet GGX distribution
 * \{ */

struct GGX {
  static float D(float NH, float a2)
  {
    float NH2 = square(NH);
    return a2 / (M_PI * square((1.0f - NH2) + a2 * NH2));
  }

  static float smith_G1(float NX, float a2)
  {
    return 2.0f / (1.0f + sqrt(1.0f + a2 * (1.0f / square(NX) - 1.0f)));
  }

  /* -------------------------------------------------------------------- */
  /** \name Bounded VNDF Sampling for Smith-GGX Reflections
   *
   * Eto, Kenta, and Yusuke Tokuyoshi. SIGGRAPH Asia 2023 Technical Communications. 2023. 1-4.
   * https://gpuopen.com/download/publications/Bounded_VNDF_Sampling_for_Smith-GGX_Reflections.pdf
   * \{ */

  /**
   * Returns a tangent space reflection direction following the GGX distribution.
   *
   * \param rand: random point on the unit cylinder (result of sample_cylinder).
   *              The Z component can be biased towards 1.
   * \param Vt: View vector in tangent space.
   * \param alpha: roughness parameter.
   * \param do_clamp: clamp for numerical stability during ray tracing.
   *
   * \return: the sampled direction and the pdf of sampling the direction.
   */
  static BsdfSample sample_reflection(float3 rand, float3 Vt, float alpha, bool do_clamp)
  {
    if (do_clamp && alpha < square(BSDF_ROUGHNESS_THRESHOLD)) {
      BsdfSample samp;
      samp.pdf = 1e6f;
      samp.direction = reflect(-Vt, float3(0.0f, 0.0f, 1.0f));
      return samp;
    }

    /* Transforming the view direction to the hemisphere configuration. */
    float2 Vt_alpha = alpha * Vt.xy;
    float Vh_norm;
    float3 Vh = normalize_and_get_length(float3(Vt_alpha, Vt.z), Vh_norm);

    /* Compute the bounded cap. */
    float a2 = square(alpha);
    float s2 = square(1.0f + length(Vt.xy));
    float k = (1.0f - a2) * s2 / (s2 + a2 * square(Vt.z));

    /* Sample a spherical cap in (-Vh.z * k, 1]. */
    float cos_theta = mix(-Vh.z * k, 1.0f, rand.x);
    float sin_theta = sqrt(saturate(1.0f - square(cos_theta)));
    float3 Lh = float3(sin_theta * rand.yz, cos_theta);

    /* Compute unnormalized halfway direction. */
    float3 Hh = Vh + Lh;

    /* Transforming the normal back to the ellipsoid configuration. */
    float3 Ht = normalize(float3(alpha * Hh.xy, max(0.0f, Hh.z)));

    /* Normal Distribution Function. */
    float D = GGX::D(saturate(Ht.z), a2);

    float pdf;
    if (Vt.z >= 0.0f) {
      pdf = D / (2.0f * (k * Vt.z + Vh_norm));
    }
    else {
      float len_Vt_alpha_sqr = length_squared(Vt_alpha);
      float t = sqrt(len_Vt_alpha_sqr + square(Vt.z));
      pdf = D * (t - Vt.z) / (2.0f * len_Vt_alpha_sqr);
    }

    BsdfSample samp;
    samp.direction = reflect(-Vt, Ht);
    samp.pdf = pdf;
    return samp;
  }

  /* Evaluate the GGX BRDF without the Fresnel term, multiplied by the cosine foreshortening term.
   * Also evaluate the probability of sampling the reflection direction. */
  static BsdfEval eval_reflection(float3 N, float3 L, float3 V, float alpha, bool do_clamp)
  {
    float NV = dot(N, V);
    if (NV <= 0.0f) {
#if 0 /* TODO(fclem): Creates black areas when denoising. Find out why. */
        /* Impossible configuration for reflection. */
        BsdfEval eval;
        eval.throughput = 0.0f;
        eval.pdf = 0.0f;
        eval.weight = 0.0f;
        return eval;
#endif
    }

    float3 H = safe_normalize(L + V);

    /* This threshold was computed based on precision of NVidia compiler (see #118997).
     * These drivers tend to produce NaNs in the computation of the NDF (`D`) if alpha is close to
     * 0.
     */
    if (do_clamp) {
      alpha = max(1e-3f, alpha);
    }

    float a2 = square(alpha);

    BsdfEval eval;

    float NV2 = square(NV);
    float one_minus_NV2 = saturate(1.0f - NV2);
    float s2 = square(1.0f + sqrt(one_minus_NV2));
    float len_ai_sqr = a2 * one_minus_NV2;
    float t = sqrt(len_ai_sqr + NV2);
    if (NV >= 0.0f) {
      float k = (1.0f - a2) * s2 / (s2 + a2 * NV2);
      eval.pdf = 1.0f / (2.0f * (k * NV + t));
    }
    else {
      eval.pdf = (t - NV) / (2.0f * len_ai_sqr);
    }

    float NH = dot(N, H);
    float NL = dot(N, L);
    if (do_clamp) {
      NH = max(NH, 1e-8f);
      NL = max(NL, 1e-8f);
    }

    float D = GGX::D(NH, a2);
    float G_V = smith_G1(NV, a2);
    float G_L = smith_G1(NL, a2);

    eval.throughput = (0.25f * G_V * G_L) / NV;
    eval.weight = eval.throughput / eval.pdf;

    /* Multiply `D` after computing the weighted throughput for numerical stability. */
    eval.throughput *= D;
    eval.pdf *= D;

    return eval;
  }
  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Sampling Visible GGX Normals with Spherical Caps
   *
   * Jonathan Dupuy and Anis Benyoub, HPG Vol. 42, No. 8, 2023.
   * https://diglib.eg.org/bitstream/handle/10.1111f/cgf14867/v42i8_03_14867.pdf
   * \{ */

  static float3 sample_vndf(float3 rand, float3 Vt, float alpha, float &G_V)
  {
    /* Transforming the view direction to the hemisphere configuration. */
    float3 Vh = normalize(float3(alpha * Vt.xy, Vt.z));

    /* Smith shadowing-masking term. */
    G_V = 2.0f * Vh.z / (1.0f + Vh.z);

    /* Sample a spherical cap in (-Vh.z, 1]. */
    float cos_theta = mix(-Vh.z, 1.0f, rand.x);
    float sin_theta = sqrt(saturate(1.0f - square(cos_theta)));
    float3 Lh = float3(sin_theta * rand.yz, cos_theta);

    /* Compute unnormalized halfway direction. */
    float3 Hh = Vh + Lh;

    /* Transforming the normal back to the ellipsoid configuration. */
    return normalize(float3(alpha * Hh.xy, max(0.0f, Hh.z)));
  }

  /**
   * Returns a tangent space refraction direction following the GGX distribution.
   *
   * \param rand: random point on the unit cylinder (result of sample_cylinder).
   *              The Z component can be biased towards 1.
   * \param Vt: View vector in tangent space.
   * \param alpha: roughness parameter
   * \param ior: refractive index of the material.
   * \param do_clamp: clamp for numerical stability during ray tracing.
   *
   * \return: the sampled direction and the pdf of sampling the direction.
   */
  static BsdfSample sample_refraction(
      float3 rand, float3 Vt, float alpha, float ior, Thickness thickness, const bool do_clamp)
  {
    if (thickness.value() != 0.0f) {
      /* The incoming ray is inside the material for the second refraction event. */
      ior = 1.0f / ior;
    }

    if (do_clamp && alpha < square(BSDF_ROUGHNESS_THRESHOLD)) {
      BsdfSample samp;
      samp.pdf = 1e6f;
      samp.direction = refract(-Vt, float3(0.0f, 0.0f, 1.0f), 1.0f / ior);
      return samp;
    }

    float G_V;
    float3 Ht = sample_vndf(rand, Vt, alpha, G_V);

    float3 Lt = refract(-Vt, Ht, 1.0f / ior);

    /* Normal Distribution Function. */
    float D = GGX::D(saturate(Ht.z), square(alpha));

    float VH = dot(Vt, Ht);
    float LH = dot(Lt, Ht);
    float Ht2 = square(ior * LH + VH);
    float pdf = D * G_V * abs(VH * LH) * square(ior) / (Vt.z * Ht2);

    BsdfSample samp;
    samp.direction = Lt;
    samp.pdf = pdf;
    return samp;
  }

  /* Evaluate the GGX BTDF without the Fresnel term, multiplied by the cosine foreshortening term.
   * Also evaluate the probability of sampling the refraction direction. */
  static BsdfEval eval_refraction(float3 N,
                                  float3 L,
                                  float3 V,
                                  float alpha,
                                  float ior,
                                  Thickness thickness,
                                  const bool do_clamp)
  {
    if (thickness.value() != 0.0f) {
      ior = 1.0f / ior;
    }

    float LV = dot(L, V);
    if (do_clamp && is_equal(ior, 1.0f, 1e-4f)) {
      /* Only valid when `L` and `V` point in the opposite directions. */
      BsdfEval eval;
      eval.throughput = float(is_equal(LV, -1.0f, 1e-3f));
      eval.pdf = 1e6f;
      eval.weight = eval.throughput;
      return eval;
    }

    bool valid = (ior < 1.0f) ? (LV < -ior) : (LV * ior < -1.0f);
    if (!valid) {
      /* Impossible configuration for transmission due to total internal reflection. */
      BsdfEval eval;
      eval.throughput = 0.0f;
      eval.pdf = 0.0f;
      eval.weight = 0.0f;
      return eval;
    }

    float3 H = ior * L + V;
    /* Ensure `H` is on the same hemisphere as `V`. */
    H = (ior < 1.0f) ? H : -H;
    float inv_len_H = safe_rcp(length(H));
    H *= inv_len_H;

    /* For transmission, `L` lies in the opposite hemisphere as `H`, therefore negate `L`. */
    float NL = dot(N, -L);
    float NH = dot(N, H);
    float NV = dot(N, V);

    if (do_clamp) {
      NL = max(NL, 1e-8f);
      NH = max(NH, 1e-8f);
      /* This threshold was computed based on precision of NVidia compiler (see #118997).
       * These drivers tend to produce NaNs in the computation of the NDF (`D`) if alpha is close
       * to 0. */
      alpha = max(1e-3f, alpha);
    }

    float a2 = square(alpha);
    float G_V = smith_G1(NV, a2);
    float G_L = smith_G1(NL, a2);
    float D = GGX::D(NH, a2);

    float VH = saturate(dot(V, H));
    float LH = saturate(dot(-L, H));

    BsdfEval eval;
    eval.pdf = (D * G_V * VH * LH * square(ior * inv_len_H)) / NV;
    eval.throughput = eval.pdf * G_L;
    eval.weight = G_L;

    return eval;
  }

  /** \} */
};

struct BsdfMicrofacetReflect : BsdfClosure::Interface {
  float roughness;

  static BsdfMicrofacetReflect unpack(const BsdfClosure cl)
  {
    BsdfMicrofacetReflect bsdf = {};
    bsdf.color = cl.color;
    bsdf.N = cl.N;
    bsdf.roughness = cl.data.x;
    return bsdf;
  }

  BsdfClosure pack() const final
  {
    return {.color = color,
            .N = N,
            .type = CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID,
            .data = float4(roughness)};
  }

  BsdfSample sample(float3 rand, float3 Vt, Thickness /*thickness*/) const final
  {
    return GGX::sample_reflection(rand, Vt, square(roughness), true);
  }

  BsdfEval eval(float3 V, float3 L, Thickness /*thickness*/) const final
  {
    return GGX::eval_reflection(N, L, V, square(roughness), true);
  }

  LightProbeRay lightprobe_ray(float3 V, Thickness /*thickness*/) const final
  {
    LightProbeRay probe;
    probe.perceptual_roughness = roughness;
    probe.dominant_direction = dominant_direction(N, V, roughness);
    return probe;
  }

  LightEvalContext light_eval_context(const UtilityTexture &util_tx,
                                      float3 V,
                                      Thickness /*thickness*/) const final
  {
    const float cos_theta = dot(N, V);
    return {
        .N = N,
        .type = LIGHT_SPECULAR,
        .ltc_data_packed =
            eevee::LTCData::sample_ggx_lut(util_tx, N, V, cos_theta, roughness).pack(),
    };
  }

  /**
   * `roughness` is expected to be the linear (from UI) roughness.
   */
  float perceived_roughness() const final
  {
    return roughness;
  }

  /**
   * Returns the dominant direction for one reflection event.
   * `roughness` is expected to be the linear (from UI) roughness.
   */
  static float3 dominant_direction(float3 N, float3 V, float roughness)
  {
    /* From Frostbite PBR Course
     * http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
     * Listing 22.
     * Note that the reference labels squared roughness (GGX input) as roughness. */
    float m = square(roughness);
    float3 R = -reflect(V, N);
    float smoothness = 1.0f - m;
    float fac = smoothness * (sqrt(smoothness) + m);
    return normalize(mix(N, R, fac));
  }
};

struct BsdfMicrofacetRefract : BsdfClosure::Interface {
  float roughness;
  float ior;

  static BsdfMicrofacetRefract unpack(const BsdfClosure cl)
  {
    BsdfMicrofacetRefract bsdf = {};
    bsdf.color = cl.color;
    bsdf.N = cl.N;
    bsdf.roughness = cl.data.x;
    bsdf.ior = cl.data.y;
    return bsdf;
  }

  BsdfClosure pack() const final
  {
    return {.color = color,
            .N = N,
            .type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID,
            .data = float4(roughness, ior, 0, 0)};
  }

  BsdfSample sample(float3 rand, float3 Vt, Thickness thickness) const final
  {
    return GGX::sample_refraction(rand, Vt, square(roughness), ior, thickness, true);
  }

  BsdfEval eval(float3 V, float3 L, Thickness thickness) const final
  {
    return GGX::eval_refraction(N, L, V, square(roughness), ior, thickness, true);
  }

  LightProbeRay lightprobe_ray(float3 V, Thickness thickness) const final
  {
    const float perceptual_roughness = perceived_roughness();
    return {
        .dominant_direction = dominant_direction(N, V, eta(thickness), perceptual_roughness),
        .perceptual_roughness = perceptual_roughness,
    };
  }

  LightEvalContext light_eval_context(const UtilityTexture &util_tx,
                                      float3 V,
                                      Thickness thickness) const final
  {
    const float perceptual_roughness = perceived_roughness();

    float3 N_tmp = N;
    /* Given non-zero thickness, simulate transmission through a slab/sphere, and output
     * an amended view vector as the direction of the first scatter. */
    if (thickness.value() != 0.0f) {
      float3 L = dominant_direction(N, V, 1.0f / ior, perceptual_roughness);
      N_tmp = -thickness.shape_intersect(N, L).hit_N;
      V = -L;
    }

    /* Reuse the isotropic LTC LUT by using the refracted exitant view as direction. */
    const float3 R = refract(-V, N_tmp, eta(thickness));
    const float cos_theta = dot(-N_tmp, R);

    return {
        .N = -N_tmp,
        .type = LIGHT_TRANSMISSION,
        .ltc_data_packed = eevee::LTCData::sample_ggx_lut(
                               util_tx, -N_tmp, V, cos_theta, perceptual_roughness)
                               .pack(),
    };
  }

  /**
   * Return the equivalent reflective roughness resulting in a similar lobe.
   * `roughness` is expected to be the linear (from UI) roughness.
   */
  float perceived_roughness() const final
  {
    /* This is a very rough mapping used by manually curve fitting the apparent roughness
     * (blurriness) of GGX reflections and GGX refraction.
     * A better fit is desirable if it is in the same order of complexity. */
    return roughness *
           sqrt_fast((ior > 1.0f) ? (1.0f - 1.0f / ior) : (saturate(1.0f - ior) * 0.64f));
  }

  void context_amend(float3 &V, Thickness thickness) final
  {
    if (thickness.value() != 0.0f) {
      float3 L = dominant_direction(N, V, 1.0f / ior, perceived_roughness());
      N = -thickness.shape_intersect(N, L).hit_N;
      V = -L;
    }
  }

  Ray ray_amend(float3 V, Ray ray, Thickness thickness) const final
  {
    if (thickness.value() != 0.0f) {
      float3 L = dominant_direction(N, V, 1.0f / ior, perceived_roughness());
      ray.origin += thickness.shape_intersect(N, L).hit_P;
    }
    return ray;
  }

 private:
  /**
   * Returns the dominant direction for one transmission event.
   * `perceptual_roughness` is expected to be the reflection roughness from
   * `perceived_roughness()`.
   */
  float3 dominant_direction(float3 N, float3 V, float eta, float perceptual_roughness) const
  {
    /* Reusing same thing as BsdfMicrofacetReflect::dominant_direction for now with the
     * roughness mapped to reflection roughness. */
    float m = square(perceptual_roughness);
    float3 R = refract(-V, N, eta);
    float smoothness = 1.0f - m;
    float fac = smoothness * (sqrt(smoothness) + m);
    return normalize(mix(-N, R, fac));
  }

  float eta(Thickness thickness) const
  {
    return thickness.value() != 0.0f ? ior : (1.0f / ior);
  }
};

/* Approximate double refraction by a reflection lobe flipped around the normal plane. */
struct BsdfThinGlassTransmission : BsdfClosure::Interface {
  float roughness;

  static BsdfThinGlassTransmission unpack(const BsdfClosure cl)
  {
    BsdfThinGlassTransmission bsdf = {};
    bsdf.color = cl.color;
    bsdf.N = cl.N;
    bsdf.roughness = cl.data.x;
    return bsdf;
  }

  BsdfClosure pack() const final
  {
    return {.color = color,
            .N = N,
            .type = CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID,
            .data = float4(roughness)};
  }

  BsdfSample sample(float3 rand, float3 Vt, Thickness /*thickness*/) const final
  {
    BsdfSample samp = GGX::sample_reflection(rand, Vt, square(roughness), true);
    /* Flip around the normal plane since the sample is in tangent space. */
    samp.direction.z = -samp.direction.z;
    return samp;
  }

  BsdfEval eval(float3 V, float3 L, Thickness /*thickness*/) const final
  {
    /* Flip view vector around the normal plane. */
    const float3 V_flipped = reflect(V, N);
    /* Flip around the normal plane (negate N). */
    return GGX::eval_reflection(-N, L, V_flipped, square(roughness), true);
  }

  LightProbeRay lightprobe_ray(float3 V, Thickness /*thickness*/) const final
  {
    /* Flip view vector around the normal plane. */
    const float3 V_flipped = reflect(V, N);
    return {
        /* Flip around the normal plane (negate N). */
        .dominant_direction = BsdfMicrofacetReflect::dominant_direction(-N, V_flipped, roughness),
        .perceptual_roughness = roughness,
    };
  }

  LightEvalContext light_eval_context(const UtilityTexture &util_tx,
                                      float3 V,
                                      Thickness /*thickness*/) const final
  {
    const float cos_theta = dot(N, V);
    return {
        /* Flip around the normal plane (negate N). */
        .N = -N,
        .type = LIGHT_TRANSMISSION,
        .ltc_data_packed =
            eevee::LTCData::sample_ggx_lut(util_tx, -N, V, cos_theta, roughness).pack(),
    };
  }

  /**
   * `roughness` is expected to be the linear (from UI) roughness.
   */
  float perceived_roughness() const final
  {
    return roughness;
  }
};

/** \} */
