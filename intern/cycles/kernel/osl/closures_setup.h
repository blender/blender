/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

// clang-format off
#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/bsdf_ashikhmin_velvet.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_microfacet.h"
#include "kernel/closure/bsdf_microfacet_multi.h"
#include "kernel/closure/bsdf_oren_nayar.h"
#include "kernel/closure/bsdf_transparent.h"
#include "kernel/closure/bsdf_ashikhmin_shirley.h"
#include "kernel/closure/bsdf_toon.h"
#include "kernel/closure/bsdf_hair.h"
#include "kernel/closure/bsdf_hair_principled.h"
#include "kernel/closure/bsdf_principled_diffuse.h"
#include "kernel/closure/bsdf_principled_sheen.h"
#include "kernel/closure/volume.h"
#include "kernel/closure/bsdf_diffuse_ramp.h"
#include "kernel/closure/bsdf_phong_ramp.h"
#include "kernel/closure/bssrdf.h"
#include "kernel/closure/emissive.h"
// clang-format on

CCL_NAMESPACE_BEGIN

#define OSL_CLOSURE_STRUCT_BEGIN(Upper, lower) \
  struct ccl_align(8) Upper##Closure \
  { \
    const char *label;
#define OSL_CLOSURE_STRUCT_END(Upper, lower) \
  } \
  ;
#define OSL_CLOSURE_STRUCT_MEMBER(Upper, TYPE, type, name, key) type name;
#define OSL_CLOSURE_STRUCT_ARRAY_MEMBER(Upper, TYPE, type, name, key, size) type name[size];

#include "closures_template.h"

ccl_device_forceinline bool osl_closure_skip(KernelGlobals kg,
                                             ccl_private const ShaderData *sd,
                                             uint32_t path_flag,
                                             int scattering)
{
  /* caustic options */
  if ((scattering & LABEL_GLOSSY) && (path_flag & PATH_RAY_DIFFUSE)) {
    if ((!kernel_data.integrator.caustics_reflective && (scattering & LABEL_REFLECT)) ||
        (!kernel_data.integrator.caustics_refractive && (scattering & LABEL_TRANSMIT)))
    {
      return true;
    }
  }

  return false;
}

/* Diffuse */

ccl_device void osl_closure_diffuse_setup(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          uint32_t path_flag,
                                          float3 weight,
                                          ccl_private const DiffuseClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_DIFFUSE)) {
    return;
  }

  ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
      sd, sizeof(DiffuseBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = closure->N;

  sd->flag |= bsdf_diffuse_setup(bsdf);
}

ccl_device void osl_closure_oren_nayar_setup(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             uint32_t path_flag,
                                             float3 weight,
                                             ccl_private const OrenNayarClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_DIFFUSE)) {
    return;
  }

  ccl_private OrenNayarBsdf *bsdf = (ccl_private OrenNayarBsdf *)bsdf_alloc(
      sd, sizeof(OrenNayarBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = closure->N;
  bsdf->roughness = closure->roughness;

  sd->flag |= bsdf_oren_nayar_setup(bsdf);
}

ccl_device void osl_closure_translucent_setup(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              uint32_t path_flag,
                                              float3 weight,
                                              ccl_private const TranslucentClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_DIFFUSE)) {
    return;
  }

  ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
      sd, sizeof(DiffuseBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = closure->N;

  sd->flag |= bsdf_translucent_setup(bsdf);
}

ccl_device void osl_closure_reflection_setup(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             uint32_t path_flag,
                                             float3 weight,
                                             ccl_private const ReflectionClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_SINGULAR)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);

  sd->flag |= bsdf_reflection_setup(bsdf);
}

ccl_device void osl_closure_refraction_setup(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             uint32_t path_flag,
                                             float3 weight,
                                             ccl_private const RefractionClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_SINGULAR)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->ior = closure->ior;

  sd->flag |= bsdf_refraction_setup(bsdf);
}

ccl_device void osl_closure_transparent_setup(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              uint32_t path_flag,
                                              float3 weight,
                                              ccl_private const TransparentClosure *closure)
{
  bsdf_transparent_setup(sd, rgb_to_spectrum(weight), path_flag);
}

/* MaterialX closures */
ccl_device void osl_closure_dielectric_bsdf_setup(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  uint32_t path_flag,
                                                  float3 weight,
                                                  ccl_private const DielectricBSDFClosure *closure)
{
  const bool has_reflection = !is_zero(closure->reflection_tint);
  const bool has_transmission = !is_zero(closure->transmission_tint);

  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelDielectricTint *fresnel = (ccl_private FresnelDielectricTint *)
      closure_alloc_extra(sd, sizeof(FresnelDielectricTint));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->ior = closure->ior;
  if (sd->flag & SD_BACKFACING) {
    bsdf->ior = 1.0f / bsdf->ior;
  }
  bsdf->T = closure->T;

  /* GGX */
  if (closure->distribution == make_string("ggx", 11253504724482777663ull) ||
      closure->distribution == make_string("default", 4430693559278735917ull))
  {
    if (has_reflection && has_transmission) {
      sd->flag |= bsdf_microfacet_ggx_glass_setup(bsdf);
    }
    else if (has_transmission) {
      sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
    }
    else {
      sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
    }
  }
  /* Beckmann */
  else {
    if (has_reflection && has_transmission) {
      sd->flag |= bsdf_microfacet_beckmann_glass_setup(bsdf);
    }
    else if (has_transmission) {
      sd->flag |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
    }
    else {
      sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
    }
  }

  fresnel->reflection_tint = rgb_to_spectrum(closure->reflection_tint);
  fresnel->transmission_tint = rgb_to_spectrum(closure->transmission_tint);
  bsdf_microfacet_setup_fresnel_dielectric_tint(bsdf, sd, fresnel);
}

ccl_device void osl_closure_conductor_bsdf_setup(KernelGlobals kg,
                                                 ccl_private ShaderData *sd,
                                                 uint32_t path_flag,
                                                 float3 weight,
                                                 ccl_private const ConductorBSDFClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelConductor *fresnel = (ccl_private FresnelConductor *)closure_alloc_extra(
      sd, sizeof(FresnelConductor));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->ior = 0.0f;
  bsdf->T = closure->T;

  /* GGX */
  if (closure->distribution == make_string("ggx", 11253504724482777663ull) ||
      closure->distribution == make_string("default", 4430693559278735917ull))
  {
    sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
  }
  /* Beckmann */
  else {
    sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
  }

  fresnel->n = rgb_to_spectrum(closure->ior);
  fresnel->k = rgb_to_spectrum(closure->extinction);
  bsdf_microfacet_setup_fresnel_conductor(bsdf, sd, fresnel);
}

ccl_device void osl_closure_generalized_schlick_bsdf_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const GeneralizedSchlickBSDFClosure *closure)
{
  const bool has_reflection = !is_zero(closure->reflection_tint);
  const bool has_transmission = !is_zero(closure->transmission_tint);

  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelGeneralizedSchlick *fresnel = (ccl_private FresnelGeneralizedSchlick *)
      closure_alloc_extra(sd, sizeof(FresnelGeneralizedSchlick));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->ior = ior_from_F0(closure->f0);
  if (sd->flag & SD_BACKFACING) {
    bsdf->ior = 1.0f / bsdf->ior;
  }
  bsdf->T = closure->T;

  /* GGX */
  if (closure->distribution == make_string("ggx", 11253504724482777663ull) ||
      closure->distribution == make_string("default", 4430693559278735917ull))
  {
    if (has_reflection && has_transmission) {
      sd->flag |= bsdf_microfacet_ggx_glass_setup(bsdf);
    }
    else if (has_transmission) {
      sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
    }
    else {
      sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
    }
  }
  /* Beckmann */
  else {
    if (has_reflection && has_transmission) {
      sd->flag |= bsdf_microfacet_beckmann_glass_setup(bsdf);
    }
    else if (has_transmission) {
      sd->flag |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
    }
    else {
      sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
    }
  }

  fresnel->reflection_tint = rgb_to_spectrum(closure->reflection_tint);
  fresnel->transmission_tint = rgb_to_spectrum(closure->transmission_tint);
  fresnel->f0 = rgb_to_spectrum(closure->f0);
  fresnel->f90 = rgb_to_spectrum(closure->f90);
  fresnel->exponent = closure->exponent;
  bsdf_microfacet_setup_fresnel_generalized_schlick(bsdf, sd, fresnel);
}

/* Standard microfacet closures */

ccl_device void osl_closure_microfacet_setup(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             uint32_t path_flag,
                                             float3 weight,
                                             ccl_private const MicrofacetClosure *closure)
{
  const int label = (closure->refract) ? LABEL_TRANSMIT : LABEL_REFLECT;
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | label)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->ior = closure->ior;
  bsdf->T = closure->T;

  /* Beckmann */
  if (closure->distribution == make_string("beckmann", 14712237670914973463ull)) {
    if (closure->refract == 1) {
      sd->flag |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
    }
    else if (closure->refract == 2) {
      sd->flag |= bsdf_microfacet_beckmann_glass_setup(bsdf);
    }
    else {
      sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
    }
  }
  /* Sharp */
  else if (closure->distribution == make_string("sharp", 1870681295563127462ull)) {
    if (closure->refract == 1) {
      sd->flag |= bsdf_refraction_setup(bsdf);
    }
    else if (closure->refract == 2) {
      sd->flag |= bsdf_sharp_glass_setup(bsdf);
    }
    else {
      sd->flag |= bsdf_reflection_setup(bsdf);
    }
  }
  /* Ashikhmin-Shirley */
  else if (closure->distribution == make_string("ashikhmin_shirley", 11318482998918370922ull)) {
    sd->flag |= bsdf_ashikhmin_shirley_setup(bsdf);
  }
  /* GGX */
  else {
    if (closure->refract == 1) {
      sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
    }
    else if (closure->refract == 2) {
      sd->flag |= bsdf_microfacet_ggx_glass_setup(bsdf);
    }
    else {
      sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
    }
  }
}

ccl_device void osl_closure_microfacet_ggx_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetGGXIsotropicClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = bsdf->alpha_y = closure->alpha_x;

  sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
}

ccl_device void osl_closure_microfacet_ggx_aniso_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetGGXClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->T = closure->T;

  sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
}

ccl_device void osl_closure_microfacet_ggx_refraction_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetGGXRefractionClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_TRANSMIT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->ior = closure->ior;

  sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
}

/* GGX closures with Fresnel */

ccl_device void osl_closure_microfacet_ggx_fresnel_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetGGXFresnelClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)
      closure_alloc_extra(sd, sizeof(FresnelPrincipledV1));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = bsdf->alpha_x;
  bsdf->ior = closure->ior;
  bsdf->T = zero_float3();

  sd->flag |= bsdf_microfacet_ggx_setup(bsdf);

  fresnel->color = rgb_to_spectrum(closure->color);
  fresnel->cspec0 = rgb_to_spectrum(closure->cspec0);
  bsdf_microfacet_setup_fresnel_principledv1(bsdf, sd, fresnel);
}

ccl_device void osl_closure_microfacet_ggx_aniso_fresnel_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetGGXAnisoFresnelClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)
      closure_alloc_extra(sd, sizeof(FresnelPrincipledV1));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->ior = closure->ior;
  bsdf->T = closure->T;

  sd->flag |= bsdf_microfacet_ggx_setup(bsdf);

  fresnel->color = rgb_to_spectrum(closure->color);
  fresnel->cspec0 = rgb_to_spectrum(closure->cspec0);
  bsdf_microfacet_setup_fresnel_principledv1(bsdf, sd, fresnel);
}

/* Multi-scattering GGX closures */

ccl_device void osl_closure_microfacet_multi_ggx_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetMultiGGXClosure *closure)
{
  /* Technically, the MultiGGX closure may also transmit. However,
   * since this is set statically and only used for caustic flags, this
   * is probably as good as it gets. */
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)closure_alloc_extra(
      sd, sizeof(FresnelConstant));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = bsdf->alpha_x;
  bsdf->ior = 1.0f;

  bsdf->fresnel = fresnel;
  fresnel->color = rgb_to_spectrum(closure->color);

  bsdf->T = zero_float3();

  sd->flag |= bsdf_microfacet_multi_ggx_setup(bsdf);
}

ccl_device void osl_closure_microfacet_multi_ggx_glass_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetMultiGGXGlassClosure *closure)
{
  /* Technically, the MultiGGX closure may also transmit. However,
   * since this is set statically and only used for caustic flags, this
   * is probably as good as it gets. */
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)closure_alloc_extra(
      sd, sizeof(FresnelConstant));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = bsdf->alpha_x;
  bsdf->ior = closure->ior;

  bsdf->fresnel = fresnel;
  fresnel->color = rgb_to_spectrum(closure->color);

  bsdf->T = zero_float3();

  sd->flag |= bsdf_microfacet_multi_ggx_glass_setup(bsdf);
}

ccl_device void osl_closure_microfacet_multi_ggx_aniso_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetMultiGGXAnisoClosure *closure)
{
  /* Technically, the MultiGGX closure may also transmit. However,
   * since this is set statically and only used for caustic flags, this
   * is probably as good as it gets. */
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)closure_alloc_extra(
      sd, sizeof(FresnelConstant));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->ior = 1.0f;

  bsdf->fresnel = fresnel;
  fresnel->color = rgb_to_spectrum(closure->color);

  bsdf->T = closure->T;

  sd->flag |= bsdf_microfacet_multi_ggx_setup(bsdf);
}

/* Multi-scattering GGX closures with Fresnel */

ccl_device void osl_closure_microfacet_multi_ggx_fresnel_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetMultiGGXFresnelClosure *closure)
{
  /* Technically, the MultiGGX closure may also transmit. However,
   * since this is set statically and only used for caustic flags, this
   * is probably as good as it gets. */
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)
      closure_alloc_extra(sd, sizeof(FresnelPrincipledV1));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = bsdf->alpha_x;
  bsdf->ior = closure->ior;

  bsdf->fresnel = fresnel;
  fresnel->color = rgb_to_spectrum(closure->color);
  fresnel->cspec0 = rgb_to_spectrum(closure->cspec0);

  bsdf->T = zero_float3();

  sd->flag |= bsdf_microfacet_multi_ggx_fresnel_setup(bsdf, sd);
}

ccl_device void osl_closure_microfacet_multi_ggx_glass_fresnel_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetMultiGGXGlassFresnelClosure *closure)
{
  /* Technically, the MultiGGX closure may also transmit. However,
   * since this is set statically and only used for caustic flags, this
   * is probably as good as it gets. */
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)
      closure_alloc_extra(sd, sizeof(FresnelPrincipledV1));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = bsdf->alpha_x;
  bsdf->ior = closure->ior;

  bsdf->fresnel = fresnel;
  fresnel->color = rgb_to_spectrum(closure->color);
  fresnel->cspec0 = rgb_to_spectrum(closure->cspec0);

  bsdf->T = zero_float3();

  sd->flag |= bsdf_microfacet_multi_ggx_glass_fresnel_setup(bsdf, sd);
}

ccl_device void osl_closure_microfacet_multi_ggx_aniso_fresnel_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetMultiGGXAnisoFresnelClosure *closure)
{
  /* Technically, the MultiGGX closure may also transmit. However,
   * since this is set statically and only used for caustic flags, this
   * is probably as good as it gets. */
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private FresnelPrincipledV1 *fresnel = (ccl_private FresnelPrincipledV1 *)
      closure_alloc_extra(sd, sizeof(FresnelPrincipledV1));
  if (!fresnel) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->ior = closure->ior;

  bsdf->fresnel = fresnel;
  fresnel->color = rgb_to_spectrum(closure->color);
  fresnel->cspec0 = rgb_to_spectrum(closure->cspec0);

  bsdf->T = closure->T;

  sd->flag |= bsdf_microfacet_multi_ggx_fresnel_setup(bsdf, sd);
}

/* Beckmann closures */

ccl_device void osl_closure_microfacet_beckmann_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetBeckmannIsotropicClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = bsdf->alpha_y = closure->alpha_x;

  sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
}

ccl_device void osl_closure_microfacet_beckmann_aniso_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetBeckmannClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->T = closure->T;

  sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
}

ccl_device void osl_closure_microfacet_beckmann_refraction_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const MicrofacetBeckmannRefractionClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_TRANSMIT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->ior = closure->ior;

  sd->flag |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
}

/* Ashikhmin closures */

ccl_device void osl_closure_ashikhmin_velvet_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const AshikhminVelvetClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_DIFFUSE)) {
    return;
  }

  ccl_private VelvetBsdf *bsdf = (ccl_private VelvetBsdf *)bsdf_alloc(
      sd, sizeof(VelvetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->sigma = closure->sigma;

  sd->flag |= bsdf_ashikhmin_velvet_setup(bsdf);
}

ccl_device void osl_closure_ashikhmin_shirley_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const AshikhminShirleyClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY | LABEL_REFLECT)) {
    return;
  }

  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->alpha_x;
  bsdf->alpha_y = closure->alpha_y;
  bsdf->T = closure->T;

  sd->flag |= bsdf_ashikhmin_shirley_setup(bsdf);
}

ccl_device void osl_closure_diffuse_toon_setup(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               uint32_t path_flag,
                                               float3 weight,
                                               ccl_private const DiffuseToonClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_DIFFUSE)) {
    return;
  }

  ccl_private ToonBsdf *bsdf = (ccl_private ToonBsdf *)bsdf_alloc(
      sd, sizeof(ToonBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->size = closure->size;
  bsdf->smooth = closure->smooth;

  sd->flag |= bsdf_diffuse_toon_setup(bsdf);
}

ccl_device void osl_closure_glossy_toon_setup(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              uint32_t path_flag,
                                              float3 weight,
                                              ccl_private const GlossyToonClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY)) {
    return;
  }

  ccl_private ToonBsdf *bsdf = (ccl_private ToonBsdf *)bsdf_alloc(
      sd, sizeof(ToonBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->size = closure->size;
  bsdf->smooth = closure->smooth;

  sd->flag |= bsdf_glossy_toon_setup(bsdf);
}

/* Disney principled closures */

ccl_device void osl_closure_principled_diffuse_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const PrincipledDiffuseClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_DIFFUSE)) {
    return;
  }

  ccl_private PrincipledDiffuseBsdf *bsdf = (ccl_private PrincipledDiffuseBsdf *)bsdf_alloc(
      sd, sizeof(PrincipledDiffuseBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = closure->N;
  bsdf->roughness = closure->roughness;

  sd->flag |= bsdf_principled_diffuse_setup(bsdf);
}

ccl_device void osl_closure_principled_sheen_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const PrincipledSheenClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_DIFFUSE)) {
    return;
  }

  ccl_private PrincipledSheenBsdf *bsdf = (ccl_private PrincipledSheenBsdf *)bsdf_alloc(
      sd, sizeof(PrincipledSheenBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = closure->N;
  bsdf->avg_value = 0.0f;

  sd->flag |= bsdf_principled_sheen_setup(sd, bsdf);
}

ccl_device void osl_closure_principled_clearcoat_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const PrincipledClearcoatClosure *closure)
{
  weight *= 0.25f * closure->clearcoat;
  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
      sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->alpha_x = closure->clearcoat_roughness;
  bsdf->alpha_y = closure->clearcoat_roughness;
  bsdf->ior = 1.5f;

  bsdf->T = zero_float3();

  sd->flag |= bsdf_microfacet_ggx_clearcoat_setup(bsdf, sd);
}

/* Variable cone emissive closure
 *
 * This primitive emits in a cone having a configurable penumbra area where the light decays to 0
 * reaching the outer_angle limit. It can also behave as a lambertian emitter if the provided
 * angles are PI/2, which is the default
 */
ccl_device void osl_closure_emission_setup(KernelGlobals kg,
                                           ccl_private ShaderData *sd,
                                           uint32_t /* path_flag */,
                                           float3 weight,
                                           ccl_private const GenericEmissiveClosure *closure)
{
  emission_setup(sd, rgb_to_spectrum(weight));
}

/* Generic background closure
 *
 * We only have a background closure for the shaders to return a color in background shaders. No
 * methods, only the weight is taking into account
 */
ccl_device void osl_closure_background_setup(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             uint32_t /* path_flag */,
                                             float3 weight,
                                             ccl_private const GenericBackgroundClosure *closure)
{
  background_setup(sd, rgb_to_spectrum(weight));
}

/* Holdout closure
 *
 * This will be used by the shader to mark the amount of holdout for the current shading point. No
 * parameters, only the weight will be used
 */
ccl_device void osl_closure_holdout_setup(KernelGlobals kg,
                                          ccl_private ShaderData *sd,
                                          uint32_t /* path_flag */,
                                          float3 weight,
                                          ccl_private const HoldoutClosure *closure)
{
  closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, rgb_to_spectrum(weight));
  sd->flag |= SD_HOLDOUT;
}

ccl_device void osl_closure_diffuse_ramp_setup(KernelGlobals kg,
                                               ccl_private ShaderData *sd,
                                               uint32_t /* path_flag */,
                                               float3 weight,
                                               ccl_private const DiffuseRampClosure *closure)
{
  ccl_private DiffuseRampBsdf *bsdf = (ccl_private DiffuseRampBsdf *)bsdf_alloc(
      sd, sizeof(DiffuseRampBsdf), rgb_to_spectrum(weight));

  if (!bsdf) {
    return;
  }

  bsdf->N = closure->N;

  bsdf->colors = (float3 *)closure_alloc_extra(sd, sizeof(float3) * 8);
  if (!bsdf->colors) {
    return;
  }

  for (int i = 0; i < 8; i++)
    bsdf->colors[i] = closure->colors[i];

  sd->flag |= bsdf_diffuse_ramp_setup(bsdf);
}

ccl_device void osl_closure_phong_ramp_setup(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             uint32_t /* path_flag */,
                                             float3 weight,
                                             ccl_private const PhongRampClosure *closure)
{
  ccl_private PhongRampBsdf *bsdf = (ccl_private PhongRampBsdf *)bsdf_alloc(
      sd, sizeof(PhongRampBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->exponent = closure->exponent;

  bsdf->colors = (float3 *)closure_alloc_extra(sd, sizeof(float3) * 8);
  if (!bsdf->colors) {
    return;
  }

  for (int i = 0; i < 8; i++)
    bsdf->colors[i] = closure->colors[i];

  sd->flag |= bsdf_phong_ramp_setup(bsdf);
}

ccl_device void osl_closure_bssrdf_setup(KernelGlobals kg,
                                         ccl_private ShaderData *sd,
                                         uint32_t path_flag,
                                         float3 weight,
                                         ccl_private const BSSRDFClosure *closure)
{
  ClosureType type;
  if (closure->method == make_string("burley", 186330084368958868ull)) {
    type = CLOSURE_BSSRDF_BURLEY_ID;
  }
  else if (closure->method == make_string("random_walk_fixed_radius", 5695810351010063150ull)) {
    type = CLOSURE_BSSRDF_RANDOM_WALK_FIXED_RADIUS_ID;
  }
  else if (closure->method == make_string("random_walk", 11360609267673527222ull)) {
    type = CLOSURE_BSSRDF_RANDOM_WALK_ID;
  }
  else {
    return;
  }

  ccl_private Bssrdf *bssrdf = bssrdf_alloc(sd, rgb_to_spectrum(weight));
  if (!bssrdf) {
    return;
  }

  /* disable in case of diffuse ancestor, can't see it well then and
   * adds considerably noise due to probabilities of continuing path
   * getting lower and lower */
  if (path_flag & PATH_RAY_DIFFUSE_ANCESTOR) {
    bssrdf->radius = zero_spectrum();
  }
  else {
    bssrdf->radius = closure->radius;
  }

  /* create one closure per color channel */
  bssrdf->albedo = closure->albedo;
  bssrdf->N = closure->N;
  bssrdf->roughness = closure->roughness;
  bssrdf->anisotropy = clamp(closure->anisotropy, 0.0f, 0.9f);

  sd->flag |= bssrdf_setup(sd, bssrdf, type, clamp(closure->ior, 1.01f, 3.8f));
}

/* Hair */

ccl_device void osl_closure_hair_reflection_setup(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  uint32_t path_flag,
                                                  float3 weight,
                                                  ccl_private const HairReflectionClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY)) {
    return;
  }

  ccl_private HairBsdf *bsdf = (ccl_private HairBsdf *)bsdf_alloc(
      sd, sizeof(HairBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->T = closure->T;
  bsdf->roughness1 = closure->roughness1;
  bsdf->roughness2 = closure->roughness2;
  bsdf->offset = closure->offset;

  sd->flag |= bsdf_hair_reflection_setup(bsdf);
}

ccl_device void osl_closure_hair_transmission_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const HairTransmissionClosure *closure)
{
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY)) {
    return;
  }

  ccl_private HairBsdf *bsdf = (ccl_private HairBsdf *)bsdf_alloc(
      sd, sizeof(HairBsdf), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->T = closure->T;
  bsdf->roughness1 = closure->roughness1;
  bsdf->roughness2 = closure->roughness2;
  bsdf->offset = closure->offset;

  sd->flag |= bsdf_hair_transmission_setup(bsdf);
}

ccl_device void osl_closure_principled_hair_setup(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  uint32_t path_flag,
                                                  float3 weight,
                                                  ccl_private const PrincipledHairClosure *closure)
{
#ifdef __HAIR__
  if (osl_closure_skip(kg, sd, path_flag, LABEL_GLOSSY)) {
    return;
  }

  ccl_private PrincipledHairBSDF *bsdf = (ccl_private PrincipledHairBSDF *)bsdf_alloc(
      sd, sizeof(PrincipledHairBSDF), rgb_to_spectrum(weight));
  if (!bsdf) {
    return;
  }

  ccl_private PrincipledHairExtra *extra = (ccl_private PrincipledHairExtra *)closure_alloc_extra(
      sd, sizeof(PrincipledHairExtra));
  if (!extra) {
    return;
  }

  bsdf->N = ensure_valid_specular_reflection(sd->Ng, sd->wi, closure->N);
  bsdf->sigma = closure->sigma;
  bsdf->v = closure->v;
  bsdf->s = closure->s;
  bsdf->alpha = closure->alpha;
  bsdf->eta = closure->eta;
  bsdf->m0_roughness = closure->m0_roughness;

  bsdf->extra = extra;

  sd->flag |= bsdf_principled_hair_setup(sd, bsdf);
#endif
}

/* Volume */

ccl_device void osl_closure_absorption_setup(KernelGlobals kg,
                                             ccl_private ShaderData *sd,
                                             uint32_t path_flag,
                                             float3 weight,
                                             ccl_private const VolumeAbsorptionClosure *closure)
{
  volume_extinction_setup(sd, rgb_to_spectrum(weight));
}

ccl_device void osl_closure_henyey_greenstein_setup(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    uint32_t path_flag,
    float3 weight,
    ccl_private const VolumeHenyeyGreensteinClosure *closure)
{
  volume_extinction_setup(sd, rgb_to_spectrum(weight));

  ccl_private HenyeyGreensteinVolume *volume = (ccl_private HenyeyGreensteinVolume *)bsdf_alloc(
      sd, sizeof(HenyeyGreensteinVolume), rgb_to_spectrum(weight));
  if (!volume) {
    return;
  }

  volume->g = closure->g;

  sd->flag |= volume_henyey_greenstein_setup(volume);
}

CCL_NAMESPACE_END
