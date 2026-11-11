/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_ltc_lut_lib.bsl.hh"
#include "eevee_thickness_lib.bsl.hh"
#include "eevee_uniform_shared.hh"
#include "eevee_utility_tx.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_ray.bsl.hh"

/**
 * BxDF evaluation functions.
 */

struct BsdfSample {
  packed_float3 direction;
  float pdf;
};

struct BsdfEval {
  /* BSDF * NL. */
  float throughput;
  /* Probability of sampling a light direction. */
  float pdf;
  /* `throughput / pdf`. */
  float weight;
};

struct LightEvalContext {
  /* Shading normal. */
  packed_float3 N;
  /* Enum used as index to fetch which light intensity to use [0..3]. */
  LightingType type;
  /* Output both shadowed and unshadowed for shadow denoising. */
  packed_float3 light_shadowed;
  packed_float3 light_unshadowed;
  /* Packed LTC matrix and attenuation data. */
  eevee::LTCData::Packed ltc_data_packed;
};

/* Represent an approximation of a bunch of rays from a BSDF. */
struct LightProbeRay {
  /* Average direction of sampled rays or its approximation.
   * Magnitude will reduce directionality of spherical harmonic evaluation. */
  packed_float3 dominant_direction;
  /* Perceptual roughness in [0..1] range.
   * Modulate blur level of spherical probe and blend between sphere probe and spherical harmonic
   * evaluation at higher roughness. */
  float perceptual_roughness;
};

/* Similar to ClosureUndetermined but doesn't contain color / throughput. */
struct BsdfClosure {
  /* TODO(fclem): Avoid carbon copy of ClosureUndetermined. */
  packed_float3 color;
  packed_float3 N;
  ClosureType type;
  /* Additional data different for each closure type. */
  packed_float4 data;

  static BsdfClosure make(ClosureUndetermined cl)
  {
    return {.color = cl.color, .N = cl.N, .type = cl.type, .data = cl.data};
  }

  ClosureUndetermined to_closure_undetermined() const
  {
    return {.color = color, .N = N, .type = type, .data = data};
  }

  /* High level functions. Will call the correct override for each class. */

  /* Return the result of the BSDF sample function. */
  BsdfSample sample(float3 rand, float3 Vt, Thickness thickness) const;
  /* Return the result of the BSDF eval function. */
  BsdfEval eval(float3 V, float3 L, Thickness thickness) const;
  /* Create a lightprobe ray that best represents the BSDF. */
  LightProbeRay lightprobe_ray(float3 V, Thickness thickness) const;
  /* Create a context for light evaluation. */
  LightEvalContext light_eval_context(const UtilityTexture &util_tx,
                                      float3 V,
                                      Thickness thickness) const;
  /* Return the perceived linear roughness of the BSDF. */
  float perceived_roughness() const;
  /* Optionally amend the view vector and the BSDF itself for a given thickness. */
  void context_amend(float3 &V, Thickness thickness);
  /* Optionally amend a ray after ray generation for a given thickness. */
  Ray ray_amend(float3 V, Ray ray, Thickness thickness) const;

  /* Interface to implement for each BSDF.
   * It has to be called directly from the derived type. */
  struct Interface {
    packed_float3 color;
    packed_float3 N;

    /* NOTE: Static functions cannot be virtual. Needs to be implemented nonetheless */
    /* Convert from untyped BsdfClosure. */
    // static virtual BsdfT unpack(BsdfClosure cl) = 0;
    /* Convert to untyped BsdfClosure. */
    virtual BsdfClosure pack() const = 0;

    virtual BsdfSample sample(float3 rand, float3 Vt, Thickness thickness) const = 0;
    virtual BsdfEval eval(float3 V, float3 L, Thickness thickness) const = 0;
    virtual LightProbeRay lightprobe_ray(float3 V, Thickness thickness) const = 0;
    virtual LightEvalContext light_eval_context(const UtilityTexture &util_tx,
                                                float3 V,
                                                Thickness thickness) const = 0;
    virtual float perceived_roughness() const = 0;

    /* Optional. */
    virtual void context_amend(float3 & /*V*/, Thickness /*thickness*/) {}
    virtual Ray ray_amend(float3 /*V*/, Ray ray, Thickness /*thickness*/) const
    {
      return ray;
    }
  };
};

/* -------------------------------------------------------------------- */
/** \name Utils
 * \{ */

/* Fresnel monochromatic, perfect mirror */
float F_eta(float eta, float cos_theta)
{
  /* Compute fresnel reflectance without explicitly computing
   * the refracted direction. */
  float c = abs(cos_theta);
  float g = eta * eta - 1.0f + c * c;
  if (g > 0.0f) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1.0f) / (c * (g - c) + 1.0f);
    return 0.5f * A * A * (1.0f + B * B);
  }
  /* Total internal reflections. */
  return 1.0f;
}

/** \} */
