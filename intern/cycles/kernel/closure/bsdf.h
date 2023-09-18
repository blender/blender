/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

// clang-format off
#include "kernel/closure/bsdf_ashikhmin_velvet.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_oren_nayar.h"
#include "kernel/closure/bsdf_phong_ramp.h"
#include "kernel/closure/bsdf_diffuse_ramp.h"
#include "kernel/closure/bsdf_microfacet.h"
#include "kernel/closure/bsdf_sheen.h"
#include "kernel/closure/bsdf_transparent.h"
#include "kernel/closure/bsdf_ashikhmin_shirley.h"
#include "kernel/closure/bsdf_toon.h"
#include "kernel/closure/bsdf_hair.h"
#include "kernel/closure/bsdf_principled_hair_chiang.h"
#include "kernel/closure/bsdf_principled_hair_huang.h"
#include "kernel/closure/bssrdf.h"
#include "kernel/closure/volume.h"
// clang-format on

CCL_NAMESPACE_BEGIN

/* Returns the square of the roughness of the closure if it has roughness,
 * 0 for singular closures and 1 otherwise. */
ccl_device_inline float bsdf_get_specular_roughness_squared(ccl_private const ShaderClosure *sc)
{
  if (CLOSURE_IS_BSDF_SINGULAR(sc->type)) {
    return 0.0f;
  }

  if (CLOSURE_IS_BSDF_MICROFACET(sc->type)) {
    ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)sc;
    return bsdf->alpha_x * bsdf->alpha_y;
  }

  return 1.0f;
}

ccl_device_inline float bsdf_get_roughness_squared(ccl_private const ShaderClosure *sc)
{
  /* This version includes diffuse, mainly for baking Principled BSDF
   * where specular and metallic zero otherwise does not bake the
   * specified roughness parameter. */
  if (sc->type == CLOSURE_BSDF_OREN_NAYAR_ID) {
    ccl_private OrenNayarBsdf *bsdf = (ccl_private OrenNayarBsdf *)sc;
    return sqr(sqr(bsdf->roughness));
  }

  if (CLOSURE_IS_BSDF_DIFFUSE(sc->type)) {
    return 0.0f;
  }

  return bsdf_get_specular_roughness_squared(sc);
}

/* An additional term to smooth illumination on grazing angles when using bump mapping.
 * Based on "Taming the Shadow Terminator" by Matt Jen-Yuan Chiang,
 * Yining Karl Li and Brent Burley. */
ccl_device_inline float bump_shadowing_term(float3 Ng, float3 N, float3 I)
{
  const float cosNI = dot(N, I);
  if (cosNI < 0.0f) {
    Ng = -Ng;
  }
  float g = safe_divide(dot(Ng, I), cosNI * dot(Ng, N));

  /* If the incoming light is on the unshadowed side, return full brightness. */
  if (g >= 1.0f) {
    return 1.0f;
  }

  /* If the incoming light points away from the surface, return black. */
  if (g < 0.0f) {
    return 0.0f;
  }

  /* Return smoothed value to avoid discontinuity at perpendicular angle. */
  float g2 = sqr(g);
  return -g2 * g + g2 + g;
}

ccl_device_inline float shift_cos_in(float cos_in, const float frequency_multiplier)
{
  /* Shadow terminator workaround, taken from Appleseed.
   * SPDX-License-Identifier: MIT
   * Copyright (c) 2019 Francois Beaune, The appleseedhq Organization */
  cos_in = min(cos_in, 1.0f);

  const float angle = fast_acosf(cos_in);
  const float val = max(cosf(angle * frequency_multiplier), 0.0f) / cos_in;
  return val;
}

ccl_device_inline bool bsdf_is_transmission(ccl_private const ShaderClosure *sc, const float3 wo)
{
  return dot(sc->N, wo) < 0.0f;
}

ccl_device_inline int bsdf_sample(KernelGlobals kg,
                                  ccl_private ShaderData *sd,
                                  ccl_private const ShaderClosure *sc,
                                  const int path_flag,
                                  const float3 rand,
                                  ccl_private Spectrum *eval,
                                  ccl_private float3 *wo,
                                  ccl_private float *pdf,
                                  ccl_private float2 *sampled_roughness,
                                  ccl_private float *eta)
{
  /* For curves use the smooth normal, particularly for ribbons the geometric
   * normal gives too much darkening otherwise. */
  *eval = zero_spectrum();
  *pdf = 0.f;
  int label = LABEL_NONE;
  const float3 Ng = (sd->type & PRIMITIVE_CURVE) ? sc->N : sd->Ng;
  const float2 rand_xy = float3_to_float2(rand);

  switch (sc->type) {
    case CLOSURE_BSDF_DIFFUSE_ID:
      label = bsdf_diffuse_sample(sc, Ng, sd->wi, rand_xy, eval, wo, pdf);
      *sampled_roughness = one_float2();
      *eta = 1.0f;
      break;
#if defined(__SVM__) || defined(__OSL__)
    case CLOSURE_BSDF_OREN_NAYAR_ID:
      label = bsdf_oren_nayar_sample(sc, Ng, sd->wi, rand_xy, eval, wo, pdf);
      *sampled_roughness = one_float2();
      *eta = 1.0f;
      break;
#  ifdef __OSL__
    case CLOSURE_BSDF_PHONG_RAMP_ID:
      label = bsdf_phong_ramp_sample(sc, Ng, sd->wi, rand_xy, eval, wo, pdf, sampled_roughness);
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
      label = bsdf_diffuse_ramp_sample(sc, Ng, sd->wi, rand_xy, eval, wo, pdf);
      *sampled_roughness = one_float2();
      *eta = 1.0f;
      break;
#  endif
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      label = bsdf_translucent_sample(sc, Ng, sd->wi, rand_xy, eval, wo, pdf);
      *sampled_roughness = one_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_TRANSPARENT_ID:
      label = bsdf_transparent_sample(sc, Ng, sd->wi, eval, wo, pdf);
      *sampled_roughness = zero_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID:
      label = bsdf_microfacet_ggx_sample(
          sc, Ng, sd->wi, rand, eval, wo, pdf, sampled_roughness, eta);
      break;
    case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID:
      label = bsdf_microfacet_beckmann_sample(
          sc, Ng, sd->wi, rand, eval, wo, pdf, sampled_roughness, eta);
      break;
    case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
      label = bsdf_ashikhmin_shirley_sample(
          sc, Ng, sd->wi, rand_xy, eval, wo, pdf, sampled_roughness);
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
      label = bsdf_ashikhmin_velvet_sample(sc, Ng, sd->wi, rand_xy, eval, wo, pdf);
      *sampled_roughness = one_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_DIFFUSE_TOON_ID:
      label = bsdf_diffuse_toon_sample(sc, Ng, sd->wi, rand_xy, eval, wo, pdf);
      *sampled_roughness = one_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_GLOSSY_TOON_ID:
      label = bsdf_glossy_toon_sample(sc, Ng, sd->wi, rand_xy, eval, wo, pdf);
      // double check if this is valid
      *sampled_roughness = one_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_HAIR_REFLECTION_ID:
      label = bsdf_hair_reflection_sample(
          sc, Ng, sd->wi, rand_xy, eval, wo, pdf, sampled_roughness);
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
      label = bsdf_hair_transmission_sample(
          sc, Ng, sd->wi, rand_xy, eval, wo, pdf, sampled_roughness);
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_HAIR_CHIANG_ID:
      label = bsdf_hair_chiang_sample(kg, sc, sd, rand, eval, wo, pdf, sampled_roughness, eta);
      break;
    case CLOSURE_BSDF_HAIR_HUANG_ID:
      label = bsdf_hair_huang_sample(kg, sc, sd, rand, eval, wo, pdf, sampled_roughness, eta);
      break;
    case CLOSURE_BSDF_SHEEN_ID:
      label = bsdf_sheen_sample(sc, Ng, sd->wi, rand_xy, eval, wo, pdf);
      *sampled_roughness = one_float2();
      *eta = 1.0f;
      break;
#endif
    default:
      label = LABEL_NONE;
      break;
  }

  /* Test if BSDF sample should be treated as transparent for background. */
  if (label & LABEL_TRANSMIT) {
    float threshold_squared = kernel_data.background.transparent_roughness_squared_threshold;

    if (threshold_squared >= 0.0f && !(label & LABEL_DIFFUSE)) {
      if (bsdf_get_specular_roughness_squared(sc) <= threshold_squared) {
        label |= LABEL_TRANSMIT_TRANSPARENT;
      }
    }
  }
  else {
    /* Shadow terminator offset. */
    const float frequency_multiplier =
        kernel_data_fetch(objects, sd->object).shadow_terminator_shading_offset;
    if (frequency_multiplier > 1.0f) {
      const float cosNO = dot(*wo, sc->N);
      *eval *= shift_cos_in(cosNO, frequency_multiplier);
    }
    if (label & LABEL_DIFFUSE) {
      if (!isequal(sc->N, sd->N)) {
        *eval *= bump_shadowing_term(sd->N, sc->N, *wo);
      }
    }
  }

#ifdef WITH_CYCLES_DEBUG
  kernel_assert(*pdf >= 0.0f);
  kernel_assert(eval->x >= 0.0f && eval->y >= 0.0f && eval->z >= 0.0f);
#endif

  return label;
}

ccl_device_inline void bsdf_roughness_eta(const KernelGlobals kg,
                                          ccl_private const ShaderClosure *sc,
                                          ccl_private float2 *roughness,
                                          ccl_private float *eta)
{
#ifdef __SVM__
  float alpha = 1.0f;
#endif
  switch (sc->type) {
    case CLOSURE_BSDF_DIFFUSE_ID:
      *roughness = one_float2();
      *eta = 1.0f;
      break;
#ifdef __SVM__
    case CLOSURE_BSDF_OREN_NAYAR_ID:
      *roughness = one_float2();
      *eta = 1.0f;
      break;
#  ifdef __OSL__
    case CLOSURE_BSDF_PHONG_RAMP_ID:
      alpha = phong_ramp_exponent_to_roughness(((ccl_private const PhongRampBsdf *)sc)->exponent);
      *roughness = make_float2(alpha, alpha);
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
      *roughness = one_float2();
      *eta = 1.0f;
      break;
#  endif
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      *roughness = one_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_TRANSPARENT_ID:
      *roughness = zero_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID: {
      ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
      *roughness = make_float2(bsdf->alpha_x, bsdf->alpha_y);
      if (CLOSURE_IS_REFRACTION(bsdf->type) || CLOSURE_IS_GLASS(bsdf->type)) {
        *eta = 1.0f / bsdf->ior;
      }
      else {
        *eta = bsdf->ior;
      }
      break;
    }
    case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID: {
      ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
      *roughness = make_float2(bsdf->alpha_x, bsdf->alpha_y);
      *eta = 1.0f;
      break;
    }
    case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
      *roughness = one_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_DIFFUSE_TOON_ID:
      *roughness = one_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_GLOSSY_TOON_ID:
      // double check if this is valid
      *roughness = one_float2();
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_HAIR_REFLECTION_ID:
      *roughness = make_float2(((ccl_private HairBsdf *)sc)->roughness1,
                               ((ccl_private HairBsdf *)sc)->roughness2);
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
      *roughness = make_float2(((ccl_private HairBsdf *)sc)->roughness1,
                               ((ccl_private HairBsdf *)sc)->roughness2);
      *eta = 1.0f;
      break;
    case CLOSURE_BSDF_HAIR_CHIANG_ID:
      alpha = ((ccl_private ChiangHairBSDF *)sc)->m0_roughness;
      *roughness = make_float2(alpha, alpha);
      *eta = ((ccl_private ChiangHairBSDF *)sc)->eta;
      break;
    case CLOSURE_BSDF_HAIR_HUANG_ID:
      alpha = ((ccl_private HuangHairBSDF *)sc)->roughness;
      *roughness = make_float2(alpha, alpha);
      *eta = ((ccl_private HuangHairBSDF *)sc)->eta;
      break;
    case CLOSURE_BSDF_SHEEN_ID:
      alpha = ((ccl_private SheenBsdf *)sc)->roughness;
      *roughness = make_float2(alpha, alpha);
      *eta = 1.0f;
      break;
#endif
    default:
      *roughness = one_float2();
      *eta = 1.0f;
      break;
  }
}

ccl_device_inline int bsdf_label(const KernelGlobals kg,
                                 ccl_private const ShaderClosure *sc,
                                 const float3 wo)
{
  /* For curves use the smooth normal, particularly for ribbons the geometric
   * normal gives too much darkening otherwise. */
  int label;
  switch (sc->type) {
    case CLOSURE_BSDF_DIFFUSE_ID:
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_FIXED_RADIUS_ID:
      label = LABEL_REFLECT | LABEL_DIFFUSE;
      break;
#ifdef __SVM__
    case CLOSURE_BSDF_OREN_NAYAR_ID:
      label = LABEL_REFLECT | LABEL_DIFFUSE;
      break;
#  ifdef __OSL__
    case CLOSURE_BSDF_PHONG_RAMP_ID:
      label = LABEL_REFLECT | LABEL_GLOSSY;
      break;
    case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
      label = LABEL_REFLECT | LABEL_DIFFUSE;
      break;
#  endif
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      label = LABEL_TRANSMIT | LABEL_DIFFUSE;
      break;
    case CLOSURE_BSDF_TRANSPARENT_ID:
      label = LABEL_TRANSMIT | LABEL_TRANSPARENT;
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID: {
      ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
      label = ((bsdf_is_transmission(sc, wo)) ? LABEL_TRANSMIT : LABEL_REFLECT) |
              ((bsdf_microfacet_eval_flag(bsdf)) ? LABEL_GLOSSY : LABEL_SINGULAR);
      break;
    }
    case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
      label = LABEL_REFLECT | LABEL_GLOSSY;
      break;
    case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
      label = LABEL_REFLECT | LABEL_DIFFUSE;
      break;
    case CLOSURE_BSDF_DIFFUSE_TOON_ID:
      label = LABEL_REFLECT | LABEL_DIFFUSE;
      break;
    case CLOSURE_BSDF_GLOSSY_TOON_ID:
      label = LABEL_REFLECT | LABEL_GLOSSY;
      break;
    case CLOSURE_BSDF_HAIR_REFLECTION_ID:
      label = LABEL_REFLECT | LABEL_GLOSSY;
      break;
    case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
      label = LABEL_TRANSMIT | LABEL_GLOSSY;
      break;
    case CLOSURE_BSDF_HAIR_CHIANG_ID:
      if (bsdf_is_transmission(sc, wo))
        label = LABEL_TRANSMIT | LABEL_GLOSSY;
      else
        label = LABEL_REFLECT | LABEL_GLOSSY;
      break;
    case CLOSURE_BSDF_HAIR_HUANG_ID:
      label = LABEL_REFLECT | LABEL_GLOSSY;
      break;
    case CLOSURE_BSDF_SHEEN_ID:
      label = LABEL_REFLECT | LABEL_DIFFUSE;
      break;
#endif
    default:
      label = LABEL_NONE;
      break;
  }

  /* Test if BSDF sample should be treated as transparent for background. */
  if (label & LABEL_TRANSMIT) {
    float threshold_squared = kernel_data.background.transparent_roughness_squared_threshold;

    if (threshold_squared >= 0.0f) {
      if (bsdf_get_specular_roughness_squared(sc) <= threshold_squared) {
        label |= LABEL_TRANSMIT_TRANSPARENT;
      }
    }
  }
  return label;
}

#ifndef __KERNEL_CUDA__
ccl_device
#else
ccl_device_inline
#endif
    Spectrum
    bsdf_eval(KernelGlobals kg,
              ccl_private ShaderData *sd,
              ccl_private const ShaderClosure *sc,
              const float3 wo,
              ccl_private float *pdf)
{
  Spectrum eval = zero_spectrum();
  *pdf = 0.f;
  const float3 Ng = (sd->type & PRIMITIVE_CURVE) ? sc->N : sd->Ng;

  switch (sc->type) {
    case CLOSURE_BSDF_DIFFUSE_ID:
      eval = bsdf_diffuse_eval(sc, sd->wi, wo, pdf);
      break;
#if defined(__SVM__) || defined(__OSL__)
    case CLOSURE_BSDF_OREN_NAYAR_ID:
      eval = bsdf_oren_nayar_eval(sc, sd->wi, wo, pdf);
      break;
#  ifdef __OSL__
    case CLOSURE_BSDF_PHONG_RAMP_ID:
      eval = bsdf_phong_ramp_eval(sc, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
      eval = bsdf_diffuse_ramp_eval(sc, sd->wi, wo, pdf);
      break;
#  endif
    case CLOSURE_BSDF_TRANSLUCENT_ID:
      eval = bsdf_translucent_eval(sc, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_TRANSPARENT_ID:
      eval = bsdf_transparent_eval(sc, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_MICROFACET_GGX_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID:
      eval = bsdf_microfacet_ggx_eval(sc, Ng, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID:
      eval = bsdf_microfacet_beckmann_eval(sc, Ng, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
      eval = bsdf_ashikhmin_shirley_eval(sc, Ng, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
      eval = bsdf_ashikhmin_velvet_eval(sc, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_DIFFUSE_TOON_ID:
      eval = bsdf_diffuse_toon_eval(sc, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_GLOSSY_TOON_ID:
      eval = bsdf_glossy_toon_eval(sc, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_HAIR_CHIANG_ID:
      eval = bsdf_hair_chiang_eval(kg, sd, sc, wo, pdf);
      break;
    case CLOSURE_BSDF_HAIR_HUANG_ID:
      eval = bsdf_hair_huang_eval(kg, sd, sc, wo, pdf);
      break;
    case CLOSURE_BSDF_HAIR_REFLECTION_ID:
      eval = bsdf_hair_reflection_eval(sc, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
      eval = bsdf_hair_transmission_eval(sc, sd->wi, wo, pdf);
      break;
    case CLOSURE_BSDF_SHEEN_ID:
      eval = bsdf_sheen_eval(sc, sd->wi, wo, pdf);
      break;
#endif
    default:
      break;
  }

  if (CLOSURE_IS_BSDF_DIFFUSE(sc->type)) {
    if (!isequal(sc->N, sd->N)) {
      eval *= bump_shadowing_term(sd->N, sc->N, wo);
    }
  }

  /* Shadow terminator offset. */
  const float frequency_multiplier =
      kernel_data_fetch(objects, sd->object).shadow_terminator_shading_offset;
  if (frequency_multiplier > 1.0f) {
    const float cosNO = dot(wo, sc->N);
    if (cosNO >= 0.0f) {
      eval *= shift_cos_in(cosNO, frequency_multiplier);
    }
  }

#ifdef WITH_CYCLES_DEBUG
  kernel_assert(*pdf >= 0.0f);
  kernel_assert(eval.x >= 0.0f && eval.y >= 0.0f && eval.z >= 0.0f);
#endif
  return eval;
}

ccl_device void bsdf_blur(KernelGlobals kg, ccl_private ShaderClosure *sc, float roughness)
{
  /* TODO: do we want to blur volume closures? */
#if defined(__SVM__) || defined(__OSL__)
  switch (sc->type) {
    case CLOSURE_BSDF_MICROFACET_GGX_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID:
      /* TODO: Recompute energy preservation after blur? */
      bsdf_microfacet_blur(sc, roughness);
      break;
    case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
      bsdf_ashikhmin_shirley_blur(sc, roughness);
      break;
    case CLOSURE_BSDF_HAIR_CHIANG_ID:
      bsdf_hair_chiang_blur(sc, roughness);
      break;
    case CLOSURE_BSDF_HAIR_HUANG_ID:
      bsdf_hair_huang_blur(sc, roughness);
      break;
    default:
      break;
  }
#endif
}

ccl_device_inline Spectrum bsdf_albedo(KernelGlobals kg,
                                       ccl_private const ShaderData *sd,
                                       ccl_private const ShaderClosure *sc,
                                       const bool reflection,
                                       const bool transmission)
{
  Spectrum albedo = sc->weight;
  /* Some closures include additional components such as Fresnel terms that cause their albedo to
   * be below 1. The point of this function is to return a best-effort estimation of their albedo,
   * meaning the amount of reflected/refracted light that would be expected when illuminated by a
   * uniform white background.
   * This is used for the denoising albedo pass and diffuse/glossy/transmission color passes.
   * NOTE: This should always match the sample_weight of the closure - as in, if there's an albedo
   * adjustment in here, the sample_weight should also be reduced accordingly.
   * TODO(lukas): Consider calling this function to determine the sample_weight? Would be a bit of
   * extra overhead though. */
#if defined(__SVM__) || defined(__OSL__)
  if (CLOSURE_IS_BSDF_MICROFACET(sc->type)) {
    albedo *= bsdf_microfacet_estimate_albedo(
        kg, sd, (ccl_private const MicrofacetBsdf *)sc, reflection, transmission);
  }
  else if (sc->type == CLOSURE_BSDF_HAIR_CHIANG_ID) {
    /* TODO(lukas): Principled Hair could also be split into a glossy and a transmission component,
     * similar to Glass BSDFs. */
    albedo *= bsdf_hair_chiang_albedo(sd, sc);
  }
  else if (sc->type == CLOSURE_BSDF_HAIR_HUANG_ID) {
    albedo *= bsdf_hair_huang_albedo(sd, sc);
  }
#endif
  return albedo;
}

CCL_NAMESPACE_END
