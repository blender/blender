/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Sampling of Normal Distribution Function for various BxDF.
 */

#include "eevee_bxdf_diffuse.bsl.hh"     // IWYU pragma: export
#include "eevee_bxdf_microfacet.bsl.hh"  // IWYU pragma: export
#include "eevee_bxdf_types.bsl.hh"       // IWYU pragma: export

BsdfSample BsdfClosure::sample(float3 rand, float3 Vt, Thickness thickness) const
{
  switch (this->type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return BsdfTranslucent::unpack(*this).sample(rand, Vt, thickness);
    case CLOSURE_BSSRDF_BURLEY_ID:
      /* Reuse diffuse. */
    case CLOSURE_BSDF_DIFFUSE_ID:
      return BsdfDiffuse::unpack(*this).sample(rand, Vt, thickness);
    case CLOSURE_BSDF_OREN_NAYAR_ID:
      return BsdfOrenNayar::unpack(*this).sample(rand, Vt, thickness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return BsdfMicrofacetReflect::unpack(*this).sample(rand, Vt, thickness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return BsdfMicrofacetRefract::unpack(*this).sample(rand, Vt, thickness);
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
      return BsdfThinGlassTransmission::unpack(*this).sample(rand, Vt, thickness);
    case CLOSURE_NONE_ID:
      assert(false);
      return {};
  }
  return {};
}

BsdfEval BsdfClosure::eval(float3 V, float3 L, Thickness thickness) const
{
  switch (this->type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return BsdfTranslucent::unpack(*this).eval(V, L, thickness);
    case CLOSURE_BSSRDF_BURLEY_ID:
      /* Reuse diffuse. */
    case CLOSURE_BSDF_DIFFUSE_ID:
      return BsdfDiffuse::unpack(*this).eval(V, L, thickness);
    case CLOSURE_BSDF_OREN_NAYAR_ID:
      return BsdfOrenNayar::unpack(*this).eval(V, L, thickness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return BsdfMicrofacetReflect::unpack(*this).eval(V, L, thickness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return BsdfMicrofacetRefract::unpack(*this).eval(V, L, thickness);
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
      return BsdfThinGlassTransmission::unpack(*this).eval(V, L, thickness);
    case CLOSURE_NONE_ID:
      assert(false);
      return {};
  }
  return {};
}

LightProbeRay BsdfClosure::lightprobe_ray(float3 V, Thickness thickness) const
{
  switch (this->type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return BsdfTranslucent::unpack(*this).lightprobe_ray(V, thickness);
    case CLOSURE_BSSRDF_BURLEY_ID:
      /* Reuse diffuse. */
    case CLOSURE_BSDF_DIFFUSE_ID:
      return BsdfDiffuse::unpack(*this).lightprobe_ray(V, thickness);
    case CLOSURE_BSDF_OREN_NAYAR_ID:
      return BsdfOrenNayar::unpack(*this).lightprobe_ray(V, thickness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return BsdfMicrofacetReflect::unpack(*this).lightprobe_ray(V, thickness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return BsdfMicrofacetRefract::unpack(*this).lightprobe_ray(V, thickness);
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
      return BsdfThinGlassTransmission::unpack(*this).lightprobe_ray(V, thickness);
    case CLOSURE_NONE_ID:
      assert(false);
      return {};
  }
  return {};
}

LightEvalContext BsdfClosure::light_eval_context(const UtilityTexture &util_tx,
                                                 float3 V,
                                                 Thickness thickness) const
{
  switch (this->type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return BsdfTranslucent::unpack(*this).light_eval_context(util_tx, V, thickness);
    case CLOSURE_BSSRDF_BURLEY_ID:
      /* Reuse diffuse. */
    case CLOSURE_BSDF_DIFFUSE_ID:
      return BsdfDiffuse::unpack(*this).light_eval_context(util_tx, V, thickness);
    case CLOSURE_BSDF_OREN_NAYAR_ID:
      return BsdfOrenNayar::unpack(*this).light_eval_context(util_tx, V, thickness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return BsdfMicrofacetReflect::unpack(*this).light_eval_context(util_tx, V, thickness);
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return BsdfMicrofacetRefract::unpack(*this).light_eval_context(util_tx, V, thickness);
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
      return BsdfThinGlassTransmission::unpack(*this).light_eval_context(util_tx, V, thickness);
    case CLOSURE_NONE_ID:
      assert(false);
      return {};
  }
  return {};
}

float BsdfClosure::perceived_roughness() const
{
  switch (this->type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return BsdfTranslucent::unpack(*this).perceived_roughness();
    case CLOSURE_BSSRDF_BURLEY_ID:
      /* Reuse diffuse. */
    case CLOSURE_BSDF_DIFFUSE_ID:
      return BsdfDiffuse::unpack(*this).perceived_roughness();
    case CLOSURE_BSDF_OREN_NAYAR_ID:
      return BsdfOrenNayar::unpack(*this).perceived_roughness();
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      return BsdfMicrofacetReflect::unpack(*this).perceived_roughness();
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return BsdfMicrofacetRefract::unpack(*this).perceived_roughness();
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
      return BsdfThinGlassTransmission::unpack(*this).perceived_roughness();
    case CLOSURE_NONE_ID:
      assert(false);
      return 0.0f;
  }
  return 0.0f;
}

void BsdfClosure::context_amend(float3 &V, Thickness thickness)
{
  switch (this->type) {
    case CLOSURE_BSDF_TRANSLUCENT_ID:
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_DIFFUSE_ID:
    case CLOSURE_BSDF_OREN_NAYAR_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID: {
      auto bsdf = BsdfMicrofacetRefract::unpack(*this);
      bsdf.context_amend(V, thickness);
      *this = bsdf.pack();
      break;
    }
    case CLOSURE_NONE_ID:
      assert(false);
      break;
  }
}

Ray BsdfClosure::ray_amend(float3 V, Ray ray, Thickness thickness) const
{
  switch (this->type) {
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
      return BsdfMicrofacetRefract::unpack(*this).ray_amend(V, ray, thickness);
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      return BsdfTranslucent::unpack(*this).ray_amend(V, ray, thickness);
    case CLOSURE_BSDF_DIFFUSE_ID:
    case CLOSURE_BSDF_OREN_NAYAR_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
      break;
    case CLOSURE_NONE_ID:
      assert(false);
      break;
  }
  return ray;
}
