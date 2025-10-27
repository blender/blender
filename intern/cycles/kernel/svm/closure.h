/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/bssrdf.h"
#include "kernel/closure/emissive.h"
#include "kernel/closure/volume.h"

#include "kernel/geom/curve.h"
#include "kernel/geom/object.h"
#include "kernel/geom/primitive.h"

#include "kernel/svm/math_util.h"
#include "kernel/svm/util.h"

#include "kernel/util/colorspace.h"

CCL_NAMESPACE_BEGIN

/* Closure Nodes */

ccl_device_inline int svm_node_closure_bsdf_skip(KernelGlobals kg, int offset, const uint type)
{
  if (type == CLOSURE_BSDF_PRINCIPLED_ID) {
    /* Read all principled BSDF extra data to get the right offset. */
    read_node(kg, &offset);
    read_node(kg, &offset);
  }

  return offset;
}

template<uint node_feature_mask, ShaderType shader_type>
#ifndef __KERNEL_ONEAPI__
ccl_device_noinline
#else
ccl_device
#endif
    int
    svm_node_closure_bsdf(KernelGlobals kg,
                          ccl_private ShaderData *sd,
                          ccl_private float *stack,
                          Spectrum closure_weight,
                          const uint4 node,
                          const uint32_t path_flag,
                          int offset)
{
  uint type;
  uint param1_offset;
  uint param2_offset;

  uint mix_weight_offset;
  svm_unpack_node_uchar4(node.y, &type, &param1_offset, &param2_offset, &mix_weight_offset);
  const float mix_weight = (stack_valid(mix_weight_offset) ?
                                stack_load_float(stack, mix_weight_offset) :
                                1.0f);

  /* note we read this extra node before weight check, so offset is added */
  const uint4 data_node = read_node(kg, &offset);

  /* Only compute BSDF for surfaces, transparent variable is shared with volume extinction. */
  IF_KERNEL_NODES_FEATURE(BSDF)
  {
    if ((shader_type != SHADER_TYPE_SURFACE) || mix_weight == 0.0f) {
      return svm_node_closure_bsdf_skip(kg, offset, type);
    }
  }
  else IF_KERNEL_NODES_FEATURE(EMISSION) {
    if (type != CLOSURE_BSDF_PRINCIPLED_ID) {
      /* Only principled BSDF can have emission. */
      return svm_node_closure_bsdf_skip(kg, offset, type);
    }
  }
  else {
    return svm_node_closure_bsdf_skip(kg, offset, type);
  }

  float3 N = stack_valid(data_node.x) ? stack_load_float3(stack, data_node.x) : sd->N;
  N = safe_normalize_fallback(N, sd->N);

  const float param1 = (stack_valid(param1_offset)) ? stack_load_float(stack, param1_offset) :
                                                      __uint_as_float(node.z);
  const float param2 = (stack_valid(param2_offset)) ? stack_load_float(stack, param2_offset) :
                                                      __uint_as_float(node.w);

  switch (type) {
    case CLOSURE_BSDF_PRINCIPLED_ID: {
      uint base_color_offset;
      uint metallic_offset;
      uint alpha_offset;
      uint coat_normal_offset;
      uint distribution_uint;
      uint diffuse_roughness_offset;
      uint specular_ior_level_offset;
      uint specular_tint_offset;
      uint emission_strength_offset;
      uint emission_color_offset;
      uint anisotropic_offset;
      uint thin_film_thickness_offset;
      uint subsurface_weight_offset;
      uint coat_weight_offset;
      uint sheen_weight_offset;
      uint transmission_weight_offset;
      uint coat_roughness_offset;
      uint coat_ior_offset;
      uint coat_tint_offset;
      uint subsurface_method_uint;
      uint subsurface_radius_offset;
      uint subsurface_scale_offset;
      uint subsurface_ior_offset;
      uint subsurface_anisotropy_offset;
      uint sheen_roughness_offset;
      uint sheen_tint_offset;
      uint anisotropic_rotation_offset;
      uint tangent_offset;
      uint thin_film_ior_offset;
      ClosureType distribution;

      const uint4 data_node2 = read_node(kg, &offset);
      const uint4 data_node3 = read_node(kg, &offset);

      svm_unpack_node_uchar4(
          data_node.y, &base_color_offset, &metallic_offset, &alpha_offset, &coat_normal_offset);
      svm_unpack_node_uchar4(data_node.z,
                             &distribution_uint,
                             &diffuse_roughness_offset,
                             &specular_ior_level_offset,
                             &specular_tint_offset);
      svm_unpack_node_uchar4(data_node.w,
                             &emission_strength_offset,
                             &emission_color_offset,
                             &anisotropic_offset,
                             &thin_film_thickness_offset);
      svm_unpack_node_uchar4(data_node2.x,
                             &subsurface_weight_offset,
                             &coat_weight_offset,
                             &sheen_weight_offset,
                             &transmission_weight_offset);
      svm_unpack_node_uchar4(data_node2.y,
                             &coat_roughness_offset,
                             &coat_ior_offset,
                             &coat_tint_offset,
                             &subsurface_method_uint);
      svm_unpack_node_uchar4(data_node2.z,
                             &subsurface_radius_offset,
                             &subsurface_scale_offset,
                             &subsurface_ior_offset,
                             &subsurface_anisotropy_offset);
      svm_unpack_node_uchar4(data_node2.w,
                             &sheen_roughness_offset,
                             &sheen_tint_offset,
                             &anisotropic_rotation_offset,
                             &tangent_offset);
      thin_film_ior_offset = data_node3.x;

      const float3 default_base_color = make_float3(__uint_as_float(data_node3.y),
                                                    __uint_as_float(data_node3.z),
                                                    __uint_as_float(data_node3.w));
      const float3 base_color = max(
          stack_load_float3_default(stack, base_color_offset, default_base_color), zero_float3());
      const float3 clamped_base_color = min(base_color, one_float3());
      const float ior = fmaxf(param1, 1e-5f);
      const float roughness = saturatef(param2);
      const float metallic = saturatef(stack_load_float_default(stack, metallic_offset, 0.0f));

      const float alpha = saturatef(stack_load_float_default(stack, alpha_offset, 1.0f));
      const float3 valid_reflection_N = maybe_ensure_valid_specular_reflection(sd, N);
      const float3 coat_normal = safe_normalize_fallback(
          stack_load_float3_default(stack, coat_normal_offset, N), sd->N);

      distribution = (ClosureType)distribution_uint;

      const float diffuse_roughness = saturatef(
          stack_load_float_default(stack, diffuse_roughness_offset, 0.0f));
      const float specular_ior_level = max(
          stack_load_float_default(stack, specular_ior_level_offset, 0.5f), 0.0f);
      const Spectrum specular_tint = rgb_to_spectrum(max(
          stack_load_float3_default(stack, specular_tint_offset, one_float3()), zero_float3()));

      const float3 emission = rgb_to_spectrum(stack_load_float3_default(
                                  stack, emission_color_offset, zero_float3())) *
                              stack_load_float_default(stack, emission_strength_offset, 0.0f);
      const float anisotropic = saturatef(
          stack_load_float_default(stack, anisotropic_offset, 0.0f));
      const float thinfilm_thickness = stack_load_float_default(
          stack, thin_film_thickness_offset, 0.0f);

#ifdef __SUBSURFACE__
      const float subsurface_weight = saturatef(
          stack_load_float_default(stack, subsurface_weight_offset, 0.0f));
#else
      const float subsurface_weight = 0.0f;
#endif
      const float coat_weight = fmaxf(stack_load_float_default(stack, coat_weight_offset, 0.0f),
                                      0.0f);
      const float sheen_weight = fmaxf(stack_load_float_default(stack, sheen_weight_offset, 0.0f),
                                       0.0f);
      const float transmission_weight = saturatef(
          stack_load_float_default(stack, transmission_weight_offset, 0.0f));

      float thinfilm_ior = 0.0f;
      if (thinfilm_thickness > THINFILM_THICKNESS_CUTOFF) {
        thinfilm_ior = fmaxf(stack_load_float(stack, thin_film_ior_offset), 1e-5f);
      }

      /* We're ignoring closure_weight here since it's always 1 for the Principled BSDF, so there's
       * no point in setting it. */
      Spectrum weight = make_spectrum(mix_weight);

      float alpha_x = sqr(roughness);
      float alpha_y = sqr(roughness);
      float3 T = zero_float3();
      if (anisotropic > 0.0f && stack_valid(tangent_offset)) {
        T = stack_load_float3(stack, tangent_offset);
        const float aspect = sqrtf(1.0f - anisotropic * 0.9f);
        alpha_x /= aspect;
        alpha_y *= aspect;
        const float anisotropic_rotation = stack_load_float_default(
            stack, anisotropic_rotation_offset, 0.0f);
        if (anisotropic_rotation != 0.0f) {
          T = rotate_around_axis(T, N, anisotropic_rotation * M_2PI_F);
        }
      }

#ifdef __CAUSTICS_TRICKS__
      const bool reflective_caustics = (kernel_data.integrator.caustics_reflective ||
                                        (path_flag & PATH_RAY_DIFFUSE) == 0);
      const bool refractive_caustics = (kernel_data.integrator.caustics_refractive ||
                                        (path_flag & PATH_RAY_DIFFUSE) == 0);
#else
      const bool reflective_caustics = true;
      const bool refractive_caustics = true;
#endif

      /* Before any actual shader components, apply transparency. */
      if (alpha < 1.0f) {
        bsdf_transparent_setup(sd, weight * (1.0f - alpha), path_flag);
        weight *= alpha;
      }

      /* First layer: Sheen */
      if (sheen_weight > CLOSURE_WEIGHT_CUTOFF) {
        const float3 sheen_tint = max(
            stack_load_float3_default(stack, sheen_tint_offset, one_float3()), zero_float3());
        const float sheen_roughness = saturatef(stack_load_float(stack, sheen_roughness_offset));

        ccl_private SheenBsdf *bsdf = (ccl_private SheenBsdf *)bsdf_alloc(
            sd, sizeof(SheenBsdf), sheen_weight * rgb_to_spectrum(sheen_tint) * weight);

        if (bsdf) {
          bsdf->N = safe_normalize(mix(N, coat_normal, saturatef(coat_weight)));
          bsdf->roughness = sheen_roughness;

          /* setup bsdf */
          const int sheen_flag = bsdf_sheen_setup(kg, sd, bsdf);

          if (sheen_flag) {
            sd->flag |= sheen_flag;

            /* Attenuate lower layers */
            const Spectrum albedo = bsdf_albedo(
                kg, sd, (ccl_private ShaderClosure *)bsdf, true, false);
            weight = closure_layering_weight(albedo, weight);
          }
        }
      }

      /* Second layer: Coat */
      if (coat_weight > CLOSURE_WEIGHT_CUTOFF) {
        const float coat_roughness = saturatef(stack_load_float(stack, coat_roughness_offset));
        const float coat_ior = fmaxf(stack_load_float(stack, coat_ior_offset), 1.0f);
        const float3 coat_tint = max(
            stack_load_float3_default(stack, coat_tint_offset, one_float3()), zero_float3());

        const float3 valid_coat_normal = maybe_ensure_valid_specular_reflection(sd, coat_normal);
        if (reflective_caustics) {
          ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
              sd, sizeof(MicrofacetBsdf), coat_weight * weight);

          if (bsdf) {
            bsdf->N = valid_coat_normal;
            bsdf->T = zero_float3();
            bsdf->ior = coat_ior;

            bsdf->alpha_x = bsdf->alpha_y = sqr(coat_roughness);

            /* setup bsdf */
            sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
            bsdf_microfacet_setup_fresnel_dielectric(kg, bsdf, sd);

            /* Attenuate lower layers */
            const Spectrum albedo = bsdf_albedo(
                kg, sd, (ccl_private ShaderClosure *)bsdf, true, false);
            weight = closure_layering_weight(albedo, weight);
          }
        }

        if (!isequal(coat_tint, one_float3())) {
          /* Tint is normalized to perpendicular incidence.
           * Therefore, if we define the coat thickness as length 1, the length along the ray is
           * t = sqrt(1+tan^2(angle(N, I))) = sqrt(1+tan^2(acos(dotNI))) = 1 / dotNI.
           * From Beer's law, we have T = exp(-sigma_e * t).
           * Therefore, tint = exp(-sigma_e * 1) (per def.), so -sigma_e = log(tint).
           * From this, T = exp(log(tint) * t) = exp(log(tint)) ^ t = tint ^ t;
           *
           * Note that this is only an approximation - it assumes that the outgoing ray
           * follows the same angle, and that there aren't multiple internal bounces.
           * In particular, things that could be improved:
           * - For transmissive materials, there should not be an outgoing path at all if the path
           *   is transmitted.
           * - For rough materials, we could blend towards a view-independent average path length
           *   (e.g. 2 for diffuse reflection) for the outgoing direction.
           * However, there's also an argument to be made for keeping parameters independent of
           * each other for more intuitive control, in particular main roughness not affecting the
           * coat.
           */
          const float cosNI = dot(sd->wi, valid_coat_normal);
          /* Refract incoming direction into coat material.
           * TIR is no concern here since we're always coming from the outside. */
          const float cosNT = sqrtf(1.0f - sqr(1.0f / coat_ior) * (1 - sqr(cosNI)));
          const float optical_depth = 1.0f / cosNT;
          weight *= mix(
              one_spectrum(), power(rgb_to_spectrum(coat_tint), optical_depth), coat_weight);
        }
      }

      /* Emission (attenuated by sheen and coat) */
      if (!is_zero(emission)) {
        emission_setup(sd, rgb_to_spectrum(emission) * weight);
      }

      /* Metallic component */
      if (metallic > CLOSURE_WEIGHT_CUTOFF) {
        if (reflective_caustics) {
          ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
              sd, sizeof(MicrofacetBsdf), metallic * weight);
          ccl_private FresnelF82Tint *fresnel =
              (bsdf != nullptr) ?
                  (ccl_private FresnelF82Tint *)closure_alloc_extra(sd, sizeof(FresnelF82Tint)) :
                  nullptr;

          if (bsdf && fresnel) {
            bsdf->N = valid_reflection_N;
            bsdf->ior = 1.0f;
            bsdf->T = T;
            bsdf->alpha_x = alpha_x;
            bsdf->alpha_y = alpha_y;

            fresnel->f0 = rgb_to_spectrum(clamped_base_color);
            const Spectrum f82 = min(specular_tint, one_spectrum());

            fresnel->thin_film.thickness = thinfilm_thickness;
            fresnel->thin_film.ior = thinfilm_ior;

            /* setup bsdf */
            sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
            const bool is_multiggx = (distribution == CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
            bsdf_microfacet_setup_fresnel_f82_tint(kg, bsdf, sd, fresnel, f82, is_multiggx);
          }
        }
        /* Attenuate other components */
        weight *= (1.0f - metallic);
      }

      /* Transmission component */
      if (transmission_weight > CLOSURE_WEIGHT_CUTOFF) {
        if (reflective_caustics || refractive_caustics) {
          ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
              sd, sizeof(MicrofacetBsdf), transmission_weight * weight);
          ccl_private FresnelGeneralizedSchlick *fresnel =
              (bsdf != nullptr) ? (ccl_private FresnelGeneralizedSchlick *)closure_alloc_extra(
                                      sd, sizeof(FresnelGeneralizedSchlick)) :
                                  nullptr;

          if (bsdf && fresnel) {
            bsdf->N = valid_reflection_N;
            bsdf->T = zero_float3();

            bsdf->alpha_x = bsdf->alpha_y = sqr(roughness);
            bsdf->ior = (sd->flag & SD_BACKFACING) ? 1.0f / ior : ior;

            fresnel->f0 = make_float3(F0_from_ior(ior)) * specular_tint;
            fresnel->f90 = one_spectrum();
            fresnel->exponent = -ior;
            fresnel->reflection_tint = reflective_caustics ? one_spectrum() : zero_spectrum();
            fresnel->transmission_tint = refractive_caustics ?
                                             sqrt(rgb_to_spectrum(clamped_base_color)) :
                                             zero_spectrum();
            fresnel->thin_film.thickness = thinfilm_thickness;
            fresnel->thin_film.ior = (sd->flag & SD_BACKFACING) ? thinfilm_ior / ior :
                                                                  thinfilm_ior;

            /* setup bsdf */
            sd->flag |= bsdf_microfacet_ggx_glass_setup(bsdf);
            const bool is_multiggx = (distribution == CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
            bsdf_microfacet_setup_fresnel_generalized_schlick(kg, bsdf, sd, fresnel, is_multiggx);
          }
        }
        /* Attenuate other components */
        weight *= (1.0f - transmission_weight);
      }

      /* Apply IOR adjustment */
      float eta = ior;
      float f0 = F0_from_ior(eta);
      if (specular_ior_level != 0.5f) {
        f0 *= 2.0f * specular_ior_level;
        eta = ior_from_F0(f0);
        if (ior < 1.0f) {
          eta = 1.0f / eta;
        }
      }

      /* Specular component */
      if (reflective_caustics && (eta != 1.0f || thinfilm_thickness > 0.1f)) {
        ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
            sd, sizeof(MicrofacetBsdf), weight);
        ccl_private FresnelGeneralizedSchlick *fresnel =
            (bsdf != nullptr) ? (ccl_private FresnelGeneralizedSchlick *)closure_alloc_extra(
                                    sd, sizeof(FresnelGeneralizedSchlick)) :
                                nullptr;

        if (bsdf && fresnel) {
          bsdf->N = valid_reflection_N;
          bsdf->ior = eta;
          bsdf->T = T;
          bsdf->alpha_x = alpha_x;
          bsdf->alpha_y = alpha_y;

          fresnel->f0 = f0 * specular_tint;
          fresnel->f90 = one_spectrum();
          fresnel->exponent = -eta;
          fresnel->reflection_tint = one_spectrum();
          fresnel->transmission_tint = zero_spectrum();
          fresnel->thin_film.thickness = thinfilm_thickness;
          fresnel->thin_film.ior = thinfilm_ior;

          /* setup bsdf */
          sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
          const bool is_multiggx = (distribution == CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
          bsdf_microfacet_setup_fresnel_generalized_schlick(kg, bsdf, sd, fresnel, is_multiggx);

          /* Attenuate lower layers */
          const Spectrum albedo = bsdf_albedo(
              kg, sd, (ccl_private ShaderClosure *)bsdf, true, false);
          weight = closure_layering_weight(albedo, weight);
        }
      }

      /* Diffuse/Subsurface component */
#ifdef __SUBSURFACE__
      if (subsurface_weight > CLOSURE_WEIGHT_CUTOFF) {
        const ClosureType subsurface_method = (ClosureType)subsurface_method_uint;
        ccl_private Bssrdf *bssrdf = bssrdf_alloc(
            sd, rgb_to_spectrum(clamped_base_color) * subsurface_weight * weight);
        if (bssrdf) {
          const float3 subsurface_radius = stack_load_float3(stack, subsurface_radius_offset);
          const float subsurface_scale = stack_load_float(stack, subsurface_scale_offset);

          bssrdf->radius = rgb_to_spectrum(
              max(subsurface_radius * subsurface_scale, zero_float3()));
          bssrdf->albedo = rgb_to_spectrum(clamped_base_color);
          bssrdf->N = maybe_ensure_valid_specular_reflection(sd, N);
          bssrdf->alpha = sqr(roughness);
          /* IOR is clamped to [1.01..3.8] inside bssrdf_setup */
          bssrdf->ior = eta;
          /* Anisotropy is clamped to [0.0..0.9] inside bssrdf_setup */
          bssrdf->anisotropy = stack_load_float_default(stack, subsurface_anisotropy_offset, 0.0f);
          if (subsurface_method == CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID) {
            bssrdf->ior = stack_load_float_default(stack, subsurface_ior_offset, 1.4f);
          }

          /* setup bsdf */
          sd->flag |= bssrdf_setup(sd, bssrdf, path_flag, subsurface_method);
        }
      }
#else
      (void)data_subsurf;
#endif

      ccl_private OrenNayarBsdf *bsdf = (ccl_private OrenNayarBsdf *)bsdf_alloc(
          sd,
          sizeof(OrenNayarBsdf),
          rgb_to_spectrum(base_color) * (1.0f - subsurface_weight) * weight);
      if (bsdf) {
        bsdf->N = N;

        /* setup bsdf */
        if (diffuse_roughness < CLOSURE_WEIGHT_CUTOFF) {
          sd->flag |= bsdf_diffuse_setup((ccl_private DiffuseBsdf *)bsdf);
        }
        else {
          bsdf->roughness = diffuse_roughness;
          sd->flag |= bsdf_oren_nayar_setup(sd, bsdf, rgb_to_spectrum(base_color));
        }
      }

      break;
    }
    case CLOSURE_BSDF_DIFFUSE_ID: {
      const Spectrum weight = closure_weight * mix_weight;
      ccl_private OrenNayarBsdf *bsdf = (ccl_private OrenNayarBsdf *)bsdf_alloc(
          sd, sizeof(OrenNayarBsdf), weight);

      if (bsdf) {
        bsdf->N = N;

        const float roughness = param1;

        if (roughness < 1e-5f) {
          sd->flag |= bsdf_diffuse_setup((ccl_private DiffuseBsdf *)bsdf);
        }
        else {
          bsdf->roughness = roughness;
          const Spectrum color = saturate(rgb_to_spectrum(stack_load_float3(stack, data_node.y)));
          sd->flag |= bsdf_oren_nayar_setup(sd, bsdf, color);
        }
      }
      break;
    }
    case CLOSURE_BSDF_TRANSLUCENT_ID: {
      const Spectrum weight = closure_weight * mix_weight;
      ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
          sd, sizeof(DiffuseBsdf), weight);

      if (bsdf) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        sd->flag |= bsdf_translucent_setup(bsdf);
      }
      break;
    }
    case CLOSURE_BSDF_TRANSPARENT_ID: {
      const Spectrum weight = closure_weight * mix_weight;
      bsdf_transparent_setup(sd, weight, path_flag);
      break;
    }
    case CLOSURE_BSDF_PHYSICAL_CONDUCTOR:
    case CLOSURE_BSDF_F82_CONDUCTOR: {
#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE)) {
        break;
      }
#endif
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), rgb_to_spectrum(make_float3(mix_weight)));

      if (bsdf != nullptr) {
        uint base_ior_offset;
        uint edge_tint_k_offset;
        uint rotation_offset;
        uint tangent_offset;
        svm_unpack_node_uchar4(
            data_node.y, &base_ior_offset, &edge_tint_k_offset, &rotation_offset, &tangent_offset);

        const float3 valid_reflection_N = maybe_ensure_valid_specular_reflection(sd, N);
        const float anisotropy = saturatef(param2);
        const float roughness = saturatef(param1);
        bsdf->alpha_x = sqr(roughness);
        bsdf->alpha_y = sqr(roughness);
        if (anisotropy > 0.0f && stack_valid(tangent_offset)) {
          bsdf->T = stack_load_float3(stack, tangent_offset);
          const float aspect = sqrtf(1.0f - anisotropy * 0.9f);
          bsdf->alpha_x /= aspect;
          bsdf->alpha_y *= aspect;
          const float anisotropic_rotation = stack_load_float(stack, rotation_offset);
          if (anisotropic_rotation != 0.0f) {
            bsdf->T = rotate_around_axis(bsdf->T, N, anisotropic_rotation * M_2PI_F);
          }
        }
        else {
          bsdf->T = zero_float3();
        }

        bsdf->N = valid_reflection_N;
        bsdf->ior = 1.0f;

        uint distribution_int;
        uint thin_film_thickness_offset;
        uint thin_film_ior_offset;
        uint unused;
        svm_unpack_node_uchar4(data_node.z,
                               &distribution_int,
                               &thin_film_thickness_offset,
                               &thin_film_ior_offset,
                               &unused);

        const float thin_film_thickness = fmaxf(
            stack_load_float(stack, thin_film_thickness_offset), 1e-5f);
        const float thin_film_ior = fmaxf(stack_load_float(stack, thin_film_ior_offset), 1e-5f);

        const ClosureType distribution = (ClosureType)distribution_int;
        /* Setup BSDF */
        if (distribution == CLOSURE_BSDF_MICROFACET_BECKMANN_ID) {
          sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
        }
        else {
          sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
        }

        const bool is_multiggx = (distribution == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);

        if (type == CLOSURE_BSDF_PHYSICAL_CONDUCTOR) {
          ccl_private FresnelConductor *fresnel = (ccl_private FresnelConductor *)
              closure_alloc_extra(sd, sizeof(FresnelConductor));

          if (!fresnel) {
            break;
          }

          fresnel->thin_film.thickness = thin_film_thickness;
          fresnel->thin_film.ior = thin_film_ior;

          const float3 n = max(stack_load_float3(stack, base_ior_offset), zero_float3());
          const float3 k = max(stack_load_float3(stack, edge_tint_k_offset), zero_float3());

          fresnel->ior = {rgb_to_spectrum(n), rgb_to_spectrum(k)};
          bsdf_microfacet_setup_fresnel_conductor(kg, bsdf, sd, fresnel, is_multiggx);
        }
        else {
          ccl_private FresnelF82Tint *fresnel = (ccl_private FresnelF82Tint *)closure_alloc_extra(
              sd, sizeof(FresnelF82Tint));

          if (!fresnel) {
            break;
          }

          fresnel->thin_film.thickness = thin_film_thickness;
          fresnel->thin_film.ior = thin_film_ior;

          const float3 color = saturate(stack_load_float3(stack, base_ior_offset));
          const float3 tint = saturate(stack_load_float3(stack, edge_tint_k_offset));

          fresnel->f0 = rgb_to_spectrum(color);
          const Spectrum f82 = rgb_to_spectrum(tint);
          bsdf_microfacet_setup_fresnel_f82_tint(kg, bsdf, sd, fresnel, f82, is_multiggx);
        }
      }
      break;
    }
    case CLOSURE_BSDF_RAY_PORTAL_ID: {
      const Spectrum weight = closure_weight * mix_weight;
      const float3 position = stack_load_float3(stack, data_node.y);
      const float3 direction = stack_load_float3(stack, data_node.z);
      bsdf_ray_portal_setup(sd, weight, position, direction);
      break;
    }
    case CLOSURE_BSDF_MICROFACET_GGX_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
    case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
    case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID: {
#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE)) {
        break;
      }
#endif
      const Spectrum weight = closure_weight * mix_weight;
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), weight);

      if (!bsdf) {
        break;
      }

      const float roughness = sqr(saturatef(param1));

      bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
      bsdf->ior = 1.0f;

      /* compute roughness */
      const float anisotropy = clamp(param2, -0.99f, 0.99f);
      if (data_node.w == SVM_STACK_INVALID || fabsf(anisotropy) <= 1e-4f) {
        /* Isotropic case. */
        bsdf->T = zero_float3();
        bsdf->alpha_x = roughness;
        bsdf->alpha_y = roughness;
      }
      else {
        bsdf->T = stack_load_float3(stack, data_node.w);

        /* rotate tangent */
        const float rotation = stack_load_float(stack, data_node.y);
        if (rotation != 0.0f) {
          bsdf->T = rotate_around_axis(bsdf->T, bsdf->N, rotation * M_2PI_F);
        }

        if (anisotropy < 0.0f) {
          bsdf->alpha_x = roughness / (1.0f + anisotropy);
          bsdf->alpha_y = roughness * (1.0f + anisotropy);
        }
        else {
          bsdf->alpha_x = roughness * (1.0f - anisotropy);
          bsdf->alpha_y = roughness / (1.0f - anisotropy);
        }
      }

      /* setup bsdf */
      if (type == CLOSURE_BSDF_MICROFACET_BECKMANN_ID) {
        sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
      }
      else if (type == CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID) {
        sd->flag |= bsdf_ashikhmin_shirley_setup(bsdf);
      }
      else {
        sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
        if (type == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID) {
          kernel_assert(stack_valid(data_node.z));
          const Spectrum color = max(rgb_to_spectrum(stack_load_float3(stack, data_node.z)),
                                     zero_spectrum());
          bsdf_microfacet_setup_fresnel_constant(kg, bsdf, sd, color);
        }
      }

      break;
    }
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID: {
#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_refractive && (path_flag & PATH_RAY_DIFFUSE)) {
        break;
      }
#endif
      const Spectrum weight = closure_weight * mix_weight;
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), weight);

      if (bsdf) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bsdf->T = zero_float3();

        float eta = fmaxf(param2, 1e-5f);
        eta = (sd->flag & SD_BACKFACING) ? 1.0f / eta : eta;

        /* setup bsdf */
        const float roughness = sqr(param1);
        bsdf->alpha_x = roughness;
        bsdf->alpha_y = roughness;
        bsdf->ior = eta;

        if (type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID) {
          sd->flag |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
        }
        else {
          sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
        }
      }

      break;
    }
    case CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID:
    case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID: {
#ifdef __CAUSTICS_TRICKS__
      const bool reflective_caustics = (kernel_data.integrator.caustics_reflective ||
                                        (path_flag & PATH_RAY_DIFFUSE) == 0);
      const bool refractive_caustics = (kernel_data.integrator.caustics_refractive ||
                                        (path_flag & PATH_RAY_DIFFUSE) == 0);
      if (!(reflective_caustics || refractive_caustics)) {
        break;
      }
#else
      const bool reflective_caustics = true;
      const bool refractive_caustics = true;
#endif

      const float thinfilm_thickness = stack_load_float(stack, data_node.z);
      const float thinfilm_ior = fmaxf(stack_load_float(stack, data_node.w), 1e-5f);

      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), make_spectrum(mix_weight));
      ccl_private FresnelGeneralizedSchlick *fresnel =
          (bsdf != nullptr) ? (ccl_private FresnelGeneralizedSchlick *)closure_alloc_extra(
                                  sd, sizeof(FresnelGeneralizedSchlick)) :
                              nullptr;

      if (bsdf && fresnel) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bsdf->T = zero_float3();

        const float ior = fmaxf(param2, 1e-5f);
        bsdf->ior = (sd->flag & SD_BACKFACING) ? 1.0f / ior : ior;
        bsdf->alpha_x = bsdf->alpha_y = sqr(saturatef(param1));

        fresnel->f0 = make_float3(F0_from_ior(ior));
        fresnel->f90 = one_spectrum();
        fresnel->exponent = -ior;
        const float3 color = max(stack_load_float3(stack, data_node.y), zero_float3());
        fresnel->reflection_tint = reflective_caustics ? rgb_to_spectrum(color) : zero_spectrum();
        fresnel->transmission_tint = refractive_caustics ? rgb_to_spectrum(color) :
                                                           zero_spectrum();
        fresnel->thin_film.thickness = thinfilm_thickness;
        fresnel->thin_film.ior = (sd->flag & SD_BACKFACING) ? thinfilm_ior / ior : thinfilm_ior;
        /* setup bsdf */
        if (type == CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID) {
          sd->flag |= bsdf_microfacet_beckmann_glass_setup(bsdf);
        }
        else {
          sd->flag |= bsdf_microfacet_ggx_glass_setup(bsdf);
        }
        const bool is_multiggx = (type == CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID);
        bsdf_microfacet_setup_fresnel_generalized_schlick(kg, bsdf, sd, fresnel, is_multiggx);
      }
      break;
    }
    case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID: {
      const Spectrum weight = closure_weight * mix_weight;
      ccl_private VelvetBsdf *bsdf = (ccl_private VelvetBsdf *)bsdf_alloc(
          sd, sizeof(VelvetBsdf), weight);

      if (bsdf) {
        bsdf->N = N;

        bsdf->sigma = saturatef(param1);
        sd->flag |= bsdf_ashikhmin_velvet_setup(bsdf);
      }
      break;
    }
    case CLOSURE_BSDF_SHEEN_ID: {
      const Spectrum weight = closure_weight * mix_weight;
      ccl_private SheenBsdf *bsdf = (ccl_private SheenBsdf *)bsdf_alloc(
          sd, sizeof(SheenBsdf), weight);

      if (bsdf) {
        bsdf->N = N;
        bsdf->roughness = saturatef(param1);

        sd->flag |= bsdf_sheen_setup(kg, sd, bsdf);
      }
      break;
    }
    case CLOSURE_BSDF_GLOSSY_TOON_ID:
#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE)) {
        break;
      }
      ATTR_FALLTHROUGH;
#endif
    case CLOSURE_BSDF_DIFFUSE_TOON_ID: {
      const Spectrum weight = closure_weight * mix_weight;
      ccl_private ToonBsdf *bsdf = (ccl_private ToonBsdf *)bsdf_alloc(
          sd, sizeof(ToonBsdf), weight);

      if (bsdf) {
        bsdf->N = N;
        bsdf->size = param1;
        bsdf->smooth = param2;

        if (type == CLOSURE_BSDF_DIFFUSE_TOON_ID) {
          sd->flag |= bsdf_diffuse_toon_setup(bsdf);
        }
        else {
          sd->flag |= bsdf_glossy_toon_setup(bsdf);
        }
      }
      break;
    }
#ifdef __HAIR__
#  ifdef __PRINCIPLED_HAIR__
    case CLOSURE_BSDF_HAIR_CHIANG_ID:
    case CLOSURE_BSDF_HAIR_HUANG_ID: {
      const uint4 data_node2 = read_node(kg, &offset);
      const uint4 data_node3 = read_node(kg, &offset);
      const uint4 data_node4 = read_node(kg, &offset);

      const Spectrum weight = closure_weight * mix_weight;

      uint offset_ofs;
      uint ior_ofs;
      uint color_ofs;
      uint parametrization;
      svm_unpack_node_uchar4(data_node.y, &offset_ofs, &ior_ofs, &color_ofs, &parametrization);
      const float alpha = stack_load_float_default(stack, offset_ofs, data_node.z);
      const float ior = stack_load_float_default(stack, ior_ofs, data_node.w);

      uint tint_ofs;
      uint melanin_ofs;
      uint melanin_redness_ofs;
      uint absorption_coefficient_ofs;
      svm_unpack_node_uchar4(data_node2.x,
                             &tint_ofs,
                             &melanin_ofs,
                             &melanin_redness_ofs,
                             &absorption_coefficient_ofs);

      uint shared_ofs1;
      uint random_ofs;
      uint random_color_ofs;
      uint shared_ofs2;
      svm_unpack_node_uchar4(
          data_node3.x, &shared_ofs1, &random_ofs, &random_color_ofs, &shared_ofs2);

      const AttributeDescriptor attr_descr_random = find_attribute(kg, sd, data_node2.y);
      float random = 0.0f;
      if (attr_descr_random.offset != ATTR_STD_NOT_FOUND) {
        random = primitive_surface_attribute<float>(kg, sd, attr_descr_random).val;
      }
      else {
        random = stack_load_float_default(stack, random_ofs, data_node3.y);
      }

      /* Random factors range: [-randomization/2, +randomization/2]. */
      const float random_roughness = param2;
      const float factor_random_roughness = 1.0f + 2.0f * (random - 0.5f) * random_roughness;
      const float roughness = param1 * factor_random_roughness;
      const float radial_roughness = (type == CLOSURE_BSDF_HAIR_CHIANG_ID) ?
                                         stack_load_float_default(
                                             stack, shared_ofs2, data_node4.y) *
                                             factor_random_roughness :
                                         roughness;

      Spectrum sigma;
      switch (parametrization) {
        case NODE_PRINCIPLED_HAIR_DIRECT_ABSORPTION: {
          const float3 absorption_coefficient = stack_load_float3(stack,
                                                                  absorption_coefficient_ofs);
          sigma = rgb_to_spectrum(absorption_coefficient);
          break;
        }
        case NODE_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION: {
          float melanin = stack_load_float_default(stack, melanin_ofs, data_node2.z);
          const float melanin_redness = stack_load_float_default(
              stack, melanin_redness_ofs, data_node2.w);

          /* Randomize melanin. */
          float random_color = stack_load_float_default(stack, random_color_ofs, data_node3.z);
          random_color = clamp(random_color, 0.0f, 1.0f);
          const float factor_random_color = 1.0f + 2.0f * (random - 0.5f) * random_color;
          melanin *= factor_random_color;

          /* Map melanin 0..inf from more perceptually linear 0..1. */
          melanin = -logf(fmaxf(1.0f - melanin, 0.0001f));

          /* Benedikt Bitterli's melanin ratio remapping. */
          const float eumelanin = melanin * (1.0f - melanin_redness);
          const float pheomelanin = melanin * melanin_redness;
          const Spectrum melanin_sigma = bsdf_principled_hair_sigma_from_concentration(
              eumelanin, pheomelanin);

          /* Optional tint. */
          const float3 tint = stack_load_float3(stack, tint_ofs);
          const Spectrum tint_sigma = bsdf_principled_hair_sigma_from_reflectance(
              rgb_to_spectrum(tint), radial_roughness);

          sigma = melanin_sigma + tint_sigma;
          break;
        }
        case NODE_PRINCIPLED_HAIR_REFLECTANCE: {
          const float3 color = stack_load_float3(stack, color_ofs);
          sigma = bsdf_principled_hair_sigma_from_reflectance(rgb_to_spectrum(color),
                                                              radial_roughness);
          break;
        }
        default: {
          /* Fallback to brownish hair, same as defaults for melanin. */
          kernel_assert(!"Invalid Hair parametrization!");
          sigma = bsdf_principled_hair_sigma_from_concentration(0.0f, 0.8054375f);
          break;
        }
      }

      if (type == CLOSURE_BSDF_HAIR_CHIANG_ID) {
        ccl_private ChiangHairBSDF *bsdf = (ccl_private ChiangHairBSDF *)bsdf_alloc(
            sd, sizeof(ChiangHairBSDF), weight);
        if (bsdf) {
          /* Remap Coat value to [0, 100]% of Roughness. */
          const float coat = stack_load_float_default(stack, shared_ofs1, data_node3.w);
          const float m0_roughness = 1.0f - clamp(coat, 0.0f, 1.0f);

          bsdf->v = roughness;
          bsdf->s = radial_roughness;
          bsdf->m0_roughness = m0_roughness;
          bsdf->alpha = alpha;
          bsdf->eta = ior;
          bsdf->sigma = sigma;

          sd->flag |= bsdf_hair_chiang_setup(sd, bsdf);
        }
      }
      else {
        kernel_assert(type == CLOSURE_BSDF_HAIR_HUANG_ID);
        uint R_ofs;
        uint TT_ofs;
        uint TRT_ofs;
        uint unused;
        svm_unpack_node_uchar4(data_node4.x, &R_ofs, &TT_ofs, &TRT_ofs, &unused);
        const float R = stack_load_float_default(stack, R_ofs, data_node4.y);
        const float TT = stack_load_float_default(stack, TT_ofs, data_node4.z);
        const float TRT = stack_load_float_default(stack, TRT_ofs, data_node4.w);
        if (R <= 0.0f && TT <= 0.0f && TRT <= 0.0f) {
          break;
        }

        ccl_private HuangHairBSDF *bsdf = (ccl_private HuangHairBSDF *)bsdf_alloc(
            sd, sizeof(HuangHairBSDF), weight);
        if (bsdf) {
          ccl_private HuangHairExtra *extra = (ccl_private HuangHairExtra *)closure_alloc_extra(
              sd, sizeof(HuangHairExtra));

          if (!extra) {
            break;
          }

          bsdf->extra = extra;
          bsdf->extra->R = fmaxf(0.0f, R);
          bsdf->extra->TT = fmaxf(0.0f, TT);
          bsdf->extra->TRT = fmaxf(0.0f, TRT);

          bsdf->extra->pixel_coverage = 1.0f;

          /* For camera ray, check if the hair covers more than one pixel, in which case a
           * nearfield model is needed to prevent ribbon-like appearance. */
          if ((path_flag & PATH_RAY_CAMERA) && (sd->type & PRIMITIVE_CURVE)) {
            /* Interpolate radius between curve keys. */
            const KernelCurve kcurve = kernel_data_fetch(curves, sd->prim);
            const int k0 = kcurve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
            const int k1 = k0 + 1;
            const float radius = mix(
                kernel_data_fetch(curve_keys, k0).w, kernel_data_fetch(curve_keys, k1).w, sd->u);

            bsdf->extra->pixel_coverage = 0.5f * sd->dP / radius;
          }

          bsdf->aspect_ratio = stack_load_float_default(stack, shared_ofs1, data_node3.w);
          if (bsdf->aspect_ratio != 1.0f) {
            /* Align ellipse major axis with the curve normal direction. */
            const AttributeDescriptor attr_descr_normal = find_attribute(kg, sd, shared_ofs2);
            bsdf->N = curve_attribute<float3>(kg, sd, attr_descr_normal).val;
          }

          bsdf->roughness = roughness;
          bsdf->tilt = alpha;
          bsdf->eta = ior;
          bsdf->sigma = sigma;

          sd->flag |= bsdf_hair_huang_setup(sd, bsdf, path_flag);
        }
      }
      break;
    }
#  endif /* __PRINCIPLED_HAIR__ */
    case CLOSURE_BSDF_HAIR_REFLECTION_ID:
    case CLOSURE_BSDF_HAIR_TRANSMISSION_ID: {
      const Spectrum weight = closure_weight * mix_weight;

      ccl_private HairBsdf *bsdf = (ccl_private HairBsdf *)bsdf_alloc(
          sd, sizeof(HairBsdf), weight);

      if (bsdf) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bsdf->roughness1 = param1;
        bsdf->roughness2 = param2;
        bsdf->offset = -stack_load_float(stack, data_node.y);

        if (stack_valid(data_node.w)) {
          bsdf->T = normalize(stack_load_float3(stack, data_node.w));
        }
        else if (!(sd->type & PRIMITIVE_CURVE)) {
          bsdf->T = normalize(sd->dPdv);
          bsdf->offset = 0.0f;
        }
        else {
          bsdf->T = normalize(sd->dPdu);
        }

        if (type == CLOSURE_BSDF_HAIR_REFLECTION_ID) {
          sd->flag |= bsdf_hair_reflection_setup(bsdf);
        }
        else {
          sd->flag |= bsdf_hair_transmission_setup(bsdf);
        }
      }

      break;
    }
#endif /* __HAIR__ */

#ifdef __SUBSURFACE__
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID: {
      const Spectrum weight = closure_weight * mix_weight;
      ccl_private Bssrdf *bssrdf = bssrdf_alloc(sd, weight);

      if (bssrdf) {
        bssrdf->radius = max(rgb_to_spectrum(stack_load_float3(stack, data_node.y) * param1),
                             zero_spectrum());
        bssrdf->albedo = closure_weight;
        bssrdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bssrdf->ior = param2;
        bssrdf->alpha = saturatef(stack_load_float(stack, data_node.w));
        bssrdf->anisotropy = stack_load_float(stack, data_node.z);

        sd->flag |= bssrdf_setup(sd, bssrdf, path_flag, (ClosureType)type);
      }

      break;
    }
#endif
    default:
      break;
  }

  return offset;
}

ccl_device_inline void svm_alloc_closure_volume_scatter(ccl_private ShaderData *sd,
                                                        ccl_private float *stack,
                                                        Spectrum weight,
                                                        const uint type,
                                                        const uint param1_offset,
                                                        const uint param_extra)
{
  switch (type) {
    case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID: {
      ccl_private HenyeyGreensteinVolume *volume = (ccl_private HenyeyGreensteinVolume *)
          bsdf_alloc(sd, sizeof(HenyeyGreensteinVolume), weight);
      if (volume) {
        volume->g = stack_valid(param1_offset) ? stack_load_float(stack, param1_offset) :
                                                 __uint_as_float(param_extra);
        sd->flag |= volume_henyey_greenstein_setup(volume);
      }
    } break;
    case CLOSURE_VOLUME_FOURNIER_FORAND_ID: {
      ccl_private FournierForandVolume *volume = (ccl_private FournierForandVolume *)bsdf_alloc(
          sd, sizeof(FournierForandVolume), weight);
      if (volume) {
        const float IOR = stack_load_float(stack, param1_offset);
        const float B = stack_load_float(stack, param_extra);
        sd->flag |= volume_fournier_forand_setup(volume, B, IOR);
      }
    } break;
    case CLOSURE_VOLUME_RAYLEIGH_ID: {
      ccl_private RayleighVolume *volume = (ccl_private RayleighVolume *)bsdf_alloc(
          sd, sizeof(RayleighVolume), weight);
      if (volume) {
        sd->flag |= volume_rayleigh_setup(volume);
      }
      break;
    }
    case CLOSURE_VOLUME_DRAINE_ID: {
      ccl_private DraineVolume *volume = (ccl_private DraineVolume *)bsdf_alloc(
          sd, sizeof(DraineVolume), weight);
      if (volume) {
        volume->g = stack_load_float(stack, param1_offset);
        volume->alpha = stack_load_float(stack, param_extra);
        sd->flag |= volume_draine_setup(volume);
      }
    } break;
    case CLOSURE_VOLUME_MIE_ID: {
      const float d = stack_valid(param1_offset) ? stack_load_float(stack, param1_offset) :
                                                   __uint_as_float(param_extra);
      float g_HG;
      float g_D;
      float alpha;
      float mixture;
      phase_mie_fitted_parameters(d, &g_HG, &g_D, &alpha, &mixture);
      ccl_private HenyeyGreensteinVolume *hg = (ccl_private HenyeyGreensteinVolume *)bsdf_alloc(
          sd, sizeof(HenyeyGreensteinVolume), weight * (1.0f - mixture));
      if (hg) {
        hg->g = g_HG;
        sd->flag |= volume_henyey_greenstein_setup(hg);
      }
      ccl_private DraineVolume *draine = (ccl_private DraineVolume *)bsdf_alloc(
          sd, sizeof(DraineVolume), weight * mixture);
      if (draine) {
        draine->g = g_D;
        draine->alpha = alpha;
        sd->flag |= volume_draine_setup(draine);
      }
    } break;
    default: {
      kernel_assert(0);
      break;
    }
  }
}

template<ShaderType shader_type>
ccl_device_noinline void svm_node_closure_volume(KernelGlobals kg,
                                                 ccl_private ShaderData *sd,
                                                 ccl_private float *stack,
                                                 Spectrum closure_weight,
                                                 const uint4 node)
{
#ifdef __VOLUME__
  /* Only sum extinction for volumes, variable is shared with surface transparency. */
  if (shader_type != SHADER_TYPE_VOLUME) {
    return;
  }

  uint type;
  uint density_offset;
  uint param1_offset;
  uint mix_weight_offset;
  svm_unpack_node_uchar4(node.y, &type, &density_offset, &param1_offset, &mix_weight_offset);
  const float mix_weight = (stack_valid(mix_weight_offset) ?
                                stack_load_float(stack, mix_weight_offset) :
                                1.0f);
  if (mix_weight == 0.0f) {
    return;
  }

  float density = (stack_valid(density_offset)) ? stack_load_float(stack, density_offset) :
                                                  __uint_as_float(node.z);
  density = mix_weight * fmaxf(density, 0.0f) * object_volume_density(kg, sd->object);

  /* Compute scattering coefficient. */
  Spectrum weight = closure_weight;

  if (type == CLOSURE_VOLUME_ABSORPTION_ID) {
    weight = one_spectrum() - weight;
  }

  weight *= density;

  /* Add closure for volume scattering. */
  if (CLOSURE_IS_VOLUME_SCATTER(type)) {
    svm_alloc_closure_volume_scatter(sd, stack, weight, type, param1_offset, node.w);
  }

  /* Sum total extinction weight. */
  volume_extinction_setup(sd, weight);
#endif
}

template<ShaderType shader_type>
ccl_device_noinline void svm_node_volume_coefficients(KernelGlobals kg,
                                                      ccl_private ShaderData *sd,
                                                      ccl_private float *stack,
                                                      Spectrum scatter_coeffs,
                                                      const uint4 node,
                                                      const uint32_t path_flag)
{
#ifdef __VOLUME__
  /* Only sum extinction for volumes, variable is shared with surface transparency. */
  if (shader_type != SHADER_TYPE_VOLUME) {
    return;
  }

  uint type;
  uint empty_offset;
  uint param1_offset;
  uint mix_weight_offset;
  svm_unpack_node_uchar4(node.y, &type, &empty_offset, &param1_offset, &mix_weight_offset);
  const float mix_weight = (stack_valid(mix_weight_offset) ?
                                stack_load_float(stack, mix_weight_offset) :
                                1.0f);
  if (mix_weight == 0.0f) {
    return;
  }

  /* Compute scattering coefficient. */
  const float weight = mix_weight * object_volume_density(kg, sd->object);

  /* Add closure for volume scattering. */
  if (!is_zero(scatter_coeffs) && CLOSURE_IS_VOLUME_SCATTER(type)) {
    svm_alloc_closure_volume_scatter(
        sd, stack, weight * scatter_coeffs, type, param1_offset, node.z);
  }
  uint absorption_coeffs_offset;
  uint emission_coeffs_offset;
  svm_unpack_node_uchar4(
      node.w, &absorption_coeffs_offset, &emission_coeffs_offset, &empty_offset, &empty_offset);
  const float3 absorption_coeffs = stack_load_float3(stack, absorption_coeffs_offset);
  volume_extinction_setup(sd, weight * (scatter_coeffs + absorption_coeffs));

  const float3 emission_coeffs = stack_load_float3(stack, emission_coeffs_offset);
  /* Compute emission. */
  if (path_flag & PATH_RAY_SHADOW) {
    /* Don't need emission for shadows. */
    return;
  }

  if (is_zero(emission_coeffs)) {
    return;
  }
  emission_setup(sd, weight * emission_coeffs);

#endif
}

template<ShaderType shader_type>
ccl_device_noinline int svm_node_principled_volume(KernelGlobals kg,
                                                   ccl_private ShaderData *sd,
                                                   ccl_private float *stack,
                                                   const Spectrum closure_weight,
                                                   const uint4 node,
                                                   const uint32_t path_flag,
                                                   int offset)
{
#ifdef __VOLUME__
  const uint4 value_node = read_node(kg, &offset);
  const uint4 attr_node = read_node(kg, &offset);

  /* Only sum extinction for volumes, variable is shared with surface transparency. */
  if (shader_type != SHADER_TYPE_VOLUME) {
    return offset;
  }

  uint density_offset;
  uint anisotropy_offset;
  uint absorption_color_offset;
  uint mix_weight_offset;
  svm_unpack_node_uchar4(
      node.y, &density_offset, &anisotropy_offset, &absorption_color_offset, &mix_weight_offset);
  const float mix_weight = (stack_valid(mix_weight_offset) ?
                                stack_load_float(stack, mix_weight_offset) :
                                1.0f);

  if (mix_weight == 0.0f) {
    return offset;
  }

  /* Compute density. */
  const float weight = mix_weight * object_volume_density(kg, sd->object);
  float primitive_density = 1.0f;
  float density = (stack_valid(density_offset)) ? stack_load_float(stack, density_offset) :
                                                  __uint_as_float(value_node.x);
  density = weight * fmaxf(density, 0.0f);

  if (density > 0.0f) {
    /* Density and color attribute lookup if available. */
    const AttributeDescriptor attr_density = find_attribute(kg, sd, attr_node.x);
    if (attr_density.offset != ATTR_STD_NOT_FOUND) {
      primitive_density = primitive_volume_attribute<float>(kg, sd, attr_density, true);
      density = fmaxf(density * primitive_density, 0.0f);
    }
  }

  if (density > 0.0f) {
    /* Compute scattering color. */
    Spectrum color = closure_weight;

    const AttributeDescriptor attr_color = find_attribute(kg, sd, attr_node.y);
    if (attr_color.offset != ATTR_STD_NOT_FOUND) {
      color *= rgb_to_spectrum(primitive_volume_attribute<float3>(kg, sd, attr_color, true));
    }

    /* Add closure for volume scattering. */
    ccl_private HenyeyGreensteinVolume *volume = (ccl_private HenyeyGreensteinVolume *)bsdf_alloc(
        sd, sizeof(HenyeyGreensteinVolume), color * density);
    if (volume) {
      const float anisotropy = (stack_valid(anisotropy_offset)) ?
                                   stack_load_float(stack, anisotropy_offset) :
                                   __uint_as_float(value_node.y);
      volume->g = anisotropy;
      sd->flag |= volume_henyey_greenstein_setup(volume);
    }

    /* Add extinction weight. */
    const float3 absorption_color = max(sqrt(stack_load_float3(stack, absorption_color_offset)),
                                        zero_float3());

    const Spectrum zero = zero_spectrum();
    const Spectrum one = one_spectrum();
    const Spectrum absorption = max(one - color, zero) *
                                max(one - rgb_to_spectrum(absorption_color), zero);
    volume_extinction_setup(sd, (color + absorption) * density);
  }

  /* Compute emission. */
  if (path_flag & PATH_RAY_SHADOW) {
    /* Don't need emission for shadows. */
    return offset;
  }

  uint emission_offset;
  uint emission_color_offset;
  uint blackbody_offset;
  uint temperature_offset;
  svm_unpack_node_uchar4(
      node.z, &emission_offset, &emission_color_offset, &blackbody_offset, &temperature_offset);
  const float emission = (stack_valid(emission_offset)) ?
                             stack_load_float(stack, emission_offset) :
                             __uint_as_float(value_node.z);
  const float blackbody = (stack_valid(blackbody_offset)) ?
                              stack_load_float(stack, blackbody_offset) :
                              __uint_as_float(value_node.w);

  if (emission > 0.0f) {
    const float3 emission_color = stack_load_float3(stack, emission_color_offset);
    emission_setup(sd, rgb_to_spectrum(emission * emission_color * weight));
  }

  if (blackbody > 0.0f) {
    float T = stack_load_float(stack, temperature_offset);

    /* Add flame temperature from attribute if available. */
    const AttributeDescriptor attr_temperature = find_attribute(kg, sd, attr_node.z);
    if (attr_temperature.offset != ATTR_STD_NOT_FOUND) {
      const float temperature = primitive_volume_attribute<float>(kg, sd, attr_temperature, true);
      T *= fmaxf(temperature, 0.0f);
    }

    T = fmaxf(T, 0.0f);

    /* Stefan-Boltzmann law. */
    const float T4 = sqr(sqr(T));
    const float sigma = 5.670373e-8f * 1e-6f / M_PI_F;
    const float intensity = sigma * mix(1.0f, T4, blackbody);

    if (intensity > 0.0f) {
      const float3 blackbody_tint = stack_load_float3(stack, node.w);
      const float3 bb = blackbody_tint * intensity *
                        rec709_to_rgb(kg, svm_math_blackbody_color_rec709(T));
      emission_setup(sd, rgb_to_spectrum(bb * weight));
    }
  }
#endif
  return offset;
}

ccl_device_noinline void svm_node_closure_emission(KernelGlobals kg,
                                                   ccl_private ShaderData *sd,
                                                   ccl_private float *stack,
                                                   Spectrum closure_weight,
                                                   const uint4 node)
{
  const uint mix_weight_offset = node.y;
  Spectrum weight = closure_weight;

  if (stack_valid(mix_weight_offset)) {
    const float mix_weight = stack_load_float(stack, mix_weight_offset);

    if (mix_weight == 0.0f) {
      return;
    }

    weight *= mix_weight;
  }

  if (sd->flag & SD_IS_VOLUME_SHADER_EVAL) {
    weight *= object_volume_density(kg, sd->object);
  }

  emission_setup(sd, weight);
}

ccl_device_noinline void svm_node_closure_background(ccl_private ShaderData *sd,
                                                     ccl_private float *stack,
                                                     Spectrum closure_weight,
                                                     const uint4 node)
{
  const uint mix_weight_offset = node.y;
  Spectrum weight = closure_weight;

  if (stack_valid(mix_weight_offset)) {
    const float mix_weight = stack_load_float(stack, mix_weight_offset);

    if (mix_weight == 0.0f) {
      return;
    }

    weight *= mix_weight;
  }

  background_setup(sd, weight);
}

ccl_device_noinline void svm_node_closure_holdout(ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  Spectrum closure_weight,
                                                  const uint4 node)
{
  const uint mix_weight_offset = node.y;

  if (stack_valid(mix_weight_offset)) {
    const float mix_weight = stack_load_float(stack, mix_weight_offset);

    if (mix_weight == 0.0f) {
      return;
    }

    closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, closure_weight * mix_weight);
  }
  else {
    closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, closure_weight);
  }

  sd->flag |= SD_HOLDOUT;
}

/* Closure Nodes */

ccl_device void svm_node_closure_set_weight(ccl_private Spectrum *closure_weight,
                                            const uint r,
                                            uint g,
                                            const uint b)
{
  *closure_weight = rgb_to_spectrum(
      make_float3(__uint_as_float(r), __uint_as_float(g), __uint_as_float(b)));
}

ccl_device void svm_node_closure_weight(ccl_private float *stack,
                                        ccl_private Spectrum *closure_weight,
                                        const uint weight_offset)
{
  *closure_weight = rgb_to_spectrum(stack_load_float3(stack, weight_offset));
}

ccl_device void svm_node_emission_weight(ccl_private float *stack,
                                         ccl_private Spectrum *closure_weight,
                                         const uint4 node)
{
  const uint color_offset = node.y;
  const uint strength_offset = node.z;

  const float strength = (stack_valid(strength_offset)) ?
                             stack_load_float(stack, strength_offset) :
                             __uint_as_float(node.w);
  *closure_weight = rgb_to_spectrum(stack_load_float3(stack, color_offset)) * strength;
}

ccl_device_noinline void svm_node_mix_closure(ccl_private float *stack, const uint4 node)
{
  /* fetch weight from blend input, previous mix closures,
   * and write to stack to be used by closure nodes later */
  uint weight_offset;
  uint in_weight_offset;
  uint weight1_offset;
  uint weight2_offset;
  svm_unpack_node_uchar4(
      node.y, &weight_offset, &in_weight_offset, &weight1_offset, &weight2_offset);

  float weight = stack_load_float(stack, weight_offset);
  weight = saturatef(weight);

  const float in_weight = (stack_valid(in_weight_offset)) ?
                              stack_load_float(stack, in_weight_offset) :
                              1.0f;

  if (stack_valid(weight1_offset)) {
    stack_store_float(stack, weight1_offset, in_weight * (1.0f - weight));
  }
  if (stack_valid(weight2_offset)) {
    stack_store_float(stack, weight2_offset, in_weight * weight);
  }
}

/* (Bump) normal */

ccl_device void svm_node_set_normal(ccl_private ShaderData *sd,
                                    ccl_private float *stack,
                                    const uint in_direction,
                                    const uint out_normal)
{
  const float3 normal = stack_load_float3(stack, in_direction);
  sd->N = normal;
  stack_store_float3(stack, out_normal, normal);
}

CCL_NAMESPACE_END
