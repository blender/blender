/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/emissive.h"

#include "kernel/util/color.h"

CCL_NAMESPACE_BEGIN

/* Closure Nodes */

ccl_device_inline int svm_node_closure_bsdf_skip(KernelGlobals kg, int offset, uint type)
{
  if (type == CLOSURE_BSDF_PRINCIPLED_ID) {
    /* Read all principled BSDF extra data to get the right offset. */
    read_node(kg, &offset);
    read_node(kg, &offset);
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
                          uint4 node,
                          uint32_t path_flag,
                          int offset)
{
  uint type, param1_offset, param2_offset;

  uint mix_weight_offset;
  svm_unpack_node_uchar4(node.y, &type, &param1_offset, &param2_offset, &mix_weight_offset);
  float mix_weight = (stack_valid(mix_weight_offset) ? stack_load_float(stack, mix_weight_offset) :
                                                       1.0f);

  /* note we read this extra node before weight check, so offset is added */
  uint4 data_node = read_node(kg, &offset);

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

  float param1 = (stack_valid(param1_offset)) ? stack_load_float(stack, param1_offset) :
                                                __uint_as_float(node.z);
  float param2 = (stack_valid(param2_offset)) ? stack_load_float(stack, param2_offset) :
                                                __uint_as_float(node.w);

  switch (type) {
    case CLOSURE_BSDF_PRINCIPLED_ID: {
      uint specular_ior_level_offset, roughness_offset, specular_tint_offset, anisotropic_offset,
          sheen_weight_offset, sheen_tint_offset, sheen_roughness_offset, coat_weight_offset,
          coat_roughness_offset, coat_ior_offset, eta_offset, transmission_weight_offset,
          anisotropic_rotation_offset, coat_tint_offset, coat_normal_offset, alpha_offset,
          emission_strength_offset, emission_offset, thinfilm_thickness_offset, unused;
      uint4 data_node2 = read_node(kg, &offset);

      float3 T = stack_load_float3(stack, data_node.y);
      svm_unpack_node_uchar4(data_node.z,
                             &specular_ior_level_offset,
                             &roughness_offset,
                             &specular_tint_offset,
                             &anisotropic_offset);
      svm_unpack_node_uchar4(
          data_node.w, &sheen_weight_offset, &sheen_tint_offset, &sheen_roughness_offset, &unused);
      svm_unpack_node_uchar4(data_node2.x,
                             &eta_offset,
                             &transmission_weight_offset,
                             &anisotropic_rotation_offset,
                             &coat_normal_offset);
      svm_unpack_node_uchar4(data_node2.w,
                             &coat_weight_offset,
                             &coat_roughness_offset,
                             &coat_ior_offset,
                             &coat_tint_offset);

      // get Disney principled parameters
      float metallic = saturatef(param1);
      float subsurface_weight = saturatef(param2);
      float specular_ior_level = max(stack_load_float(stack, specular_ior_level_offset), 0.0f);
      float roughness = saturatef(stack_load_float(stack, roughness_offset));
      Spectrum specular_tint = rgb_to_spectrum(
          max(stack_load_float3(stack, specular_tint_offset), zero_float3()));
      float anisotropic = saturatef(stack_load_float(stack, anisotropic_offset));
      float sheen_weight = max(stack_load_float(stack, sheen_weight_offset), 0.0f);
      float3 sheen_tint = max(stack_load_float3(stack, sheen_tint_offset), zero_float3());
      float sheen_roughness = saturatef(stack_load_float(stack, sheen_roughness_offset));
      float coat_weight = fmaxf(stack_load_float(stack, coat_weight_offset), 0.0f);
      float coat_roughness = saturatef(stack_load_float(stack, coat_roughness_offset));
      float coat_ior = fmaxf(stack_load_float(stack, coat_ior_offset), 1.0f);
      float3 coat_tint = max(stack_load_float3(stack, coat_tint_offset), zero_float3());
      float transmission_weight = saturatef(stack_load_float(stack, transmission_weight_offset));
      float anisotropic_rotation = stack_load_float(stack, anisotropic_rotation_offset);
      float ior = fmaxf(stack_load_float(stack, eta_offset), 1e-5f);

      ClosureType distribution = (ClosureType)data_node2.y;
      ClosureType subsurface_method = (ClosureType)data_node2.z;

      float3 valid_reflection_N = maybe_ensure_valid_specular_reflection(sd, N);
      float3 coat_normal = stack_valid(coat_normal_offset) ?
                               stack_load_float3(stack, coat_normal_offset) :
                               sd->N;
      coat_normal = safe_normalize_fallback(coat_normal, sd->N);

      // get the base color
      uint4 data_base_color = read_node(kg, &offset);
      float3 base_color = stack_valid(data_base_color.x) ?
                              stack_load_float3(stack, data_base_color.x) :
                              make_float3(__uint_as_float(data_base_color.y),
                                          __uint_as_float(data_base_color.z),
                                          __uint_as_float(data_base_color.w));
      base_color = max(base_color, zero_float3());
      const float3 clamped_base_color = min(base_color, one_float3());

      // get the subsurface scattering data
      uint4 data_subsurf = read_node(kg, &offset);

      uint4 data_alpha_emission_thin = read_node(kg, &offset);
      svm_unpack_node_uchar4(data_alpha_emission_thin.x,
                             &alpha_offset,
                             &emission_strength_offset,
                             &emission_offset,
                             &thinfilm_thickness_offset);
      float alpha = stack_valid(alpha_offset) ? stack_load_float(stack, alpha_offset) :
                                                __uint_as_float(data_alpha_emission_thin.y);
      alpha = saturatef(alpha);

      float emission_strength = stack_valid(emission_strength_offset) ?
                                    stack_load_float(stack, emission_strength_offset) :
                                    __uint_as_float(data_alpha_emission_thin.z);
      float3 emission = stack_load_float3(stack, emission_offset) * emission_strength;

      float thinfilm_thickness = fmaxf(stack_load_float(stack, thinfilm_thickness_offset), 1e-5f);
      float thinfilm_ior = fmaxf(stack_load_float(stack, data_alpha_emission_thin.w), 1e-5f);

      Spectrum weight = closure_weight * mix_weight;

      float alpha_x = sqr(roughness), alpha_y = sqr(roughness);
      if (anisotropic > 0.0f) {
        float aspect = sqrtf(1.0f - anisotropic * 0.9f);
        alpha_x /= aspect;
        alpha_y *= aspect;
        if (anisotropic_rotation != 0.0f)
          T = rotate_around_axis(T, N, anisotropic_rotation * M_2PI_F);
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
            Spectrum albedo = bsdf_albedo(kg, sd, (ccl_private ShaderClosure *)bsdf, true, false);
            weight = closure_layering_weight(albedo, weight);
          }
        }
      }

      /* Second layer: Coat */
      if (coat_weight > CLOSURE_WEIGHT_CUTOFF) {
        coat_normal = maybe_ensure_valid_specular_reflection(sd, coat_normal);
        if (reflective_caustics) {
          ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
              sd, sizeof(MicrofacetBsdf), coat_weight * weight);

          if (bsdf) {
            bsdf->N = coat_normal;
            bsdf->T = zero_float3();
            bsdf->ior = coat_ior;

            bsdf->alpha_x = bsdf->alpha_y = sqr(coat_roughness);

            /* setup bsdf */
            sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
            bsdf_microfacet_setup_fresnel_dielectric(kg, bsdf, sd);

            /* Attenuate lower layers */
            Spectrum albedo = bsdf_albedo(kg, sd, (ccl_private ShaderClosure *)bsdf, true, false);
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
          float cosNI = dot(sd->wi, coat_normal);
          /* Refract incoming direction into coat material.
           * TIR is no concern here since we're always coming from the outside. */
          float cosNT = sqrtf(1.0f - sqr(1.0f / coat_ior) * (1 - sqr(cosNI)));
          float optical_depth = 1.0f / cosNT;
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
              (bsdf != NULL) ?
                  (ccl_private FresnelF82Tint *)closure_alloc_extra(sd, sizeof(FresnelF82Tint)) :
                  NULL;

          if (bsdf && fresnel) {
            bsdf->N = valid_reflection_N;
            bsdf->ior = 1.0f;
            bsdf->T = T;
            bsdf->alpha_x = alpha_x;
            bsdf->alpha_y = alpha_y;

            fresnel->f0 = rgb_to_spectrum(clamped_base_color);
            const Spectrum f82 = min(specular_tint, one_spectrum());

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
              (bsdf != NULL) ? (ccl_private FresnelGeneralizedSchlick *)closure_alloc_extra(
                                   sd, sizeof(FresnelGeneralizedSchlick)) :
                               NULL;

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
            (bsdf != NULL) ? (ccl_private FresnelGeneralizedSchlick *)closure_alloc_extra(
                                 sd, sizeof(FresnelGeneralizedSchlick)) :
                             NULL;

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
          Spectrum albedo = bsdf_albedo(kg, sd, (ccl_private ShaderClosure *)bsdf, true, false);
          weight = closure_layering_weight(albedo, weight);
        }
      }

      /* Diffuse/Subsurface component */
#ifdef __SUBSURFACE__
      ccl_private Bssrdf *bssrdf = bssrdf_alloc(
          sd, rgb_to_spectrum(clamped_base_color) * subsurface_weight * weight);
      if (bssrdf) {
        float3 subsurface_radius = stack_load_float3(stack, data_subsurf.y);
        float subsurface_scale = stack_load_float(stack, data_subsurf.z);

        bssrdf->radius = rgb_to_spectrum(max(subsurface_radius * subsurface_scale, zero_float3()));
        bssrdf->albedo = rgb_to_spectrum(clamped_base_color);
        bssrdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bssrdf->alpha = sqr(roughness);
        /* IOR is clamped to [1.01..3.8] inside bssrdf_setup */
        bssrdf->ior = eta;
        /* Anisotropy is clamped to [0.0..0.9] inside bssrdf_setup */
        bssrdf->anisotropy = stack_load_float(stack, data_subsurf.w);
        if (subsurface_method == CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID) {
          bssrdf->ior = stack_load_float(stack, data_subsurf.x);
        }

        /* setup bsdf */
        sd->flag |= bssrdf_setup(sd, bssrdf, path_flag, subsurface_method);
      }
#else
      subsurface_weight = 0.0f;
#endif

      ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
          sd,
          sizeof(DiffuseBsdf),
          rgb_to_spectrum(base_color) * (1.0f - subsurface_weight) * weight);
      if (bsdf) {
        bsdf->N = N;

        /* setup bsdf */
        sd->flag |= bsdf_diffuse_setup(bsdf);
      }

      break;
    }
    case CLOSURE_BSDF_DIFFUSE_ID: {
      Spectrum weight = closure_weight * mix_weight;
      ccl_private OrenNayarBsdf *bsdf = (ccl_private OrenNayarBsdf *)bsdf_alloc(
          sd, sizeof(OrenNayarBsdf), weight);

      if (bsdf) {
        bsdf->N = N;

        float roughness = param1;

        if (roughness == 0.0f) {
          sd->flag |= bsdf_diffuse_setup((ccl_private DiffuseBsdf *)bsdf);
        }
        else {
          bsdf->roughness = roughness;
          sd->flag |= bsdf_oren_nayar_setup(bsdf);
        }
      }
      break;
    }
    case CLOSURE_BSDF_TRANSLUCENT_ID: {
      Spectrum weight = closure_weight * mix_weight;
      ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
          sd, sizeof(DiffuseBsdf), weight);

      if (bsdf) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        sd->flag |= bsdf_translucent_setup(bsdf);
      }
      break;
    }
    case CLOSURE_BSDF_TRANSPARENT_ID: {
      Spectrum weight = closure_weight * mix_weight;
      bsdf_transparent_setup(sd, weight, path_flag);
      break;
    }
    case CLOSURE_BSDF_RAY_PORTAL_ID: {
      Spectrum weight = closure_weight * mix_weight;
      float3 position = stack_load_float3(stack, data_node.y);
      float3 direction = stack_load_float3(stack, data_node.z);
      bsdf_ray_portal_setup(sd, weight, path_flag, position, direction);
      break;
    }
    case CLOSURE_BSDF_MICROFACET_GGX_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
    case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
    case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID: {
#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE))
        break;
#endif
      Spectrum weight = closure_weight * mix_weight;
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), weight);

      if (!bsdf) {
        break;
      }

      float roughness = sqr(saturatef(param1));

      bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
      bsdf->ior = 1.0f;

      /* compute roughness */
      float anisotropy = clamp(param2, -0.99f, 0.99f);
      if (data_node.w == SVM_STACK_INVALID || fabsf(anisotropy) <= 1e-4f) {
        /* Isotropic case. */
        bsdf->T = zero_float3();
        bsdf->alpha_x = roughness;
        bsdf->alpha_y = roughness;
      }
      else {
        bsdf->T = stack_load_float3(stack, data_node.w);

        /* rotate tangent */
        float rotation = stack_load_float(stack, data_node.y);
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
      if (!kernel_data.integrator.caustics_refractive && (path_flag & PATH_RAY_DIFFUSE))
        break;
#endif
      Spectrum weight = closure_weight * mix_weight;
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), weight);

      if (bsdf) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bsdf->T = zero_float3();

        float eta = fmaxf(param2, 1e-5f);
        eta = (sd->flag & SD_BACKFACING) ? 1.0f / eta : eta;

        /* setup bsdf */
        float roughness = sqr(param1);
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
      if (!(reflective_caustics || refractive_caustics))
        break;
#else
      const bool reflective_caustics = true;
      const bool refractive_caustics = true;
#endif
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), make_spectrum(mix_weight));
      ccl_private FresnelGeneralizedSchlick *fresnel =
          (bsdf != NULL) ? (ccl_private FresnelGeneralizedSchlick *)closure_alloc_extra(
                               sd, sizeof(FresnelGeneralizedSchlick)) :
                           NULL;

      if (bsdf && fresnel) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bsdf->T = zero_float3();

        float ior = fmaxf(param2, 1e-5f);
        bsdf->ior = (sd->flag & SD_BACKFACING) ? 1.0f / ior : ior;
        bsdf->alpha_x = bsdf->alpha_y = sqr(saturatef(param1));

        fresnel->f0 = make_float3(F0_from_ior(ior));
        fresnel->f90 = one_spectrum();
        fresnel->exponent = -ior;
        const float3 color = max(stack_load_float3(stack, data_node.y), zero_float3());
        fresnel->reflection_tint = reflective_caustics ? rgb_to_spectrum(color) : zero_spectrum();
        fresnel->transmission_tint = refractive_caustics ? rgb_to_spectrum(color) :
                                                           zero_spectrum();
        fresnel->thin_film.thickness = 0.0f;
        fresnel->thin_film.ior = 0.0f;

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
      Spectrum weight = closure_weight * mix_weight;
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
      Spectrum weight = closure_weight * mix_weight;
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
      if (!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE))
        break;
      ATTR_FALLTHROUGH;
#endif
    case CLOSURE_BSDF_DIFFUSE_TOON_ID: {
      Spectrum weight = closure_weight * mix_weight;
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
      uint4 data_node2 = read_node(kg, &offset);
      uint4 data_node3 = read_node(kg, &offset);
      uint4 data_node4 = read_node(kg, &offset);

      Spectrum weight = closure_weight * mix_weight;

      uint offset_ofs, ior_ofs, color_ofs, parametrization;
      svm_unpack_node_uchar4(data_node.y, &offset_ofs, &ior_ofs, &color_ofs, &parametrization);
      float alpha = stack_load_float_default(stack, offset_ofs, data_node.z);
      float ior = stack_load_float_default(stack, ior_ofs, data_node.w);

      uint tint_ofs, melanin_ofs, melanin_redness_ofs, absorption_coefficient_ofs;
      svm_unpack_node_uchar4(data_node2.x,
                             &tint_ofs,
                             &melanin_ofs,
                             &melanin_redness_ofs,
                             &absorption_coefficient_ofs);

      uint shared_ofs1, random_ofs, random_color_ofs, shared_ofs2;
      svm_unpack_node_uchar4(
          data_node3.x, &shared_ofs1, &random_ofs, &random_color_ofs, &shared_ofs2);

      const AttributeDescriptor attr_descr_random = find_attribute(kg, sd, data_node2.y);
      float random = 0.0f;
      if (attr_descr_random.offset != ATTR_STD_NOT_FOUND) {
        random = primitive_surface_attribute_float(kg, sd, attr_descr_random, NULL, NULL);
      }
      else {
        random = stack_load_float_default(stack, random_ofs, data_node3.y);
      }

      /* Random factors range: [-randomization/2, +randomization/2]. */
      float random_roughness = param2;
      float factor_random_roughness = 1.0f + 2.0f * (random - 0.5f) * random_roughness;
      float roughness = param1 * factor_random_roughness;
      float radial_roughness = (type == CLOSURE_BSDF_HAIR_CHIANG_ID) ?
                                   stack_load_float_default(stack, shared_ofs2, data_node4.y) *
                                       factor_random_roughness :
                                   roughness;

      Spectrum sigma;
      switch (parametrization) {
        case NODE_PRINCIPLED_HAIR_DIRECT_ABSORPTION: {
          float3 absorption_coefficient = stack_load_float3(stack, absorption_coefficient_ofs);
          sigma = rgb_to_spectrum(absorption_coefficient);
          break;
        }
        case NODE_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION: {
          float melanin = stack_load_float_default(stack, melanin_ofs, data_node2.z);
          float melanin_redness = stack_load_float_default(
              stack, melanin_redness_ofs, data_node2.w);

          /* Randomize melanin. */
          float random_color = stack_load_float_default(stack, random_color_ofs, data_node3.z);
          random_color = clamp(random_color, 0.0f, 1.0f);
          float factor_random_color = 1.0f + 2.0f * (random - 0.5f) * random_color;
          melanin *= factor_random_color;

          /* Map melanin 0..inf from more perceptually linear 0..1. */
          melanin = -logf(fmaxf(1.0f - melanin, 0.0001f));

          /* Benedikt Bitterli's melanin ratio remapping. */
          float eumelanin = melanin * (1.0f - melanin_redness);
          float pheomelanin = melanin * melanin_redness;
          Spectrum melanin_sigma = bsdf_principled_hair_sigma_from_concentration(eumelanin,
                                                                                 pheomelanin);

          /* Optional tint. */
          float3 tint = stack_load_float3(stack, tint_ofs);
          Spectrum tint_sigma = bsdf_principled_hair_sigma_from_reflectance(rgb_to_spectrum(tint),
                                                                            radial_roughness);

          sigma = melanin_sigma + tint_sigma;
          break;
        }
        case NODE_PRINCIPLED_HAIR_REFLECTANCE: {
          float3 color = stack_load_float3(stack, color_ofs);
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
          float coat = stack_load_float_default(stack, shared_ofs1, data_node3.w);
          float m0_roughness = 1.0f - clamp(coat, 0.0f, 1.0f);

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
        uint R_ofs, TT_ofs, TRT_ofs, unused;
        svm_unpack_node_uchar4(data_node4.x, &R_ofs, &TT_ofs, &TRT_ofs, &unused);
        float R = stack_load_float_default(stack, R_ofs, data_node4.y);
        float TT = stack_load_float_default(stack, TT_ofs, data_node4.z);
        float TRT = stack_load_float_default(stack, TRT_ofs, data_node4.w);
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
            bsdf->N = curve_attribute_float3(kg, sd, attr_descr_normal, NULL, NULL);
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
      Spectrum weight = closure_weight * mix_weight;

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
        else
          bsdf->T = normalize(sd->dPdu);

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
      Spectrum weight = closure_weight * mix_weight;
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

template<ShaderType shader_type>
ccl_device_noinline void svm_node_closure_volume(KernelGlobals kg,
                                                 ccl_private ShaderData *sd,
                                                 ccl_private float *stack,
                                                 Spectrum closure_weight,
                                                 uint4 node)
{
#ifdef __VOLUME__
  /* Only sum extinction for volumes, variable is shared with surface transparency. */
  if (shader_type != SHADER_TYPE_VOLUME) {
    return;
  }

  uint type, density_offset, anisotropy_offset;

  uint mix_weight_offset;
  svm_unpack_node_uchar4(node.y, &type, &density_offset, &anisotropy_offset, &mix_weight_offset);
  float mix_weight = (stack_valid(mix_weight_offset) ? stack_load_float(stack, mix_weight_offset) :
                                                       1.0f);

  if (mix_weight == 0.0f) {
    return;
  }

  float density = (stack_valid(density_offset)) ? stack_load_float(stack, density_offset) :
                                                  __uint_as_float(node.z);
  density = mix_weight * fmaxf(density, 0.0f);

  /* Compute scattering coefficient. */
  Spectrum weight = closure_weight;

  if (type == CLOSURE_VOLUME_ABSORPTION_ID) {
    weight = one_spectrum() - weight;
  }

  weight *= density;

  /* Add closure for volume scattering. */
  if (type == CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID) {
    ccl_private HenyeyGreensteinVolume *volume = (ccl_private HenyeyGreensteinVolume *)bsdf_alloc(
        sd, sizeof(HenyeyGreensteinVolume), weight);

    if (volume) {
      float anisotropy = (stack_valid(anisotropy_offset)) ?
                             stack_load_float(stack, anisotropy_offset) :
                             __uint_as_float(node.w);
      volume->g = anisotropy; /* g */
      sd->flag |= volume_henyey_greenstein_setup(volume);
    }
  }

  /* Sum total extinction weight. */
  volume_extinction_setup(sd, weight);
#endif
}

template<ShaderType shader_type>
ccl_device_noinline int svm_node_principled_volume(KernelGlobals kg,
                                                   ccl_private ShaderData *sd,
                                                   ccl_private float *stack,
                                                   Spectrum closure_weight,
                                                   uint4 node,
                                                   uint32_t path_flag,
                                                   int offset)
{
#ifdef __VOLUME__
  uint4 value_node = read_node(kg, &offset);
  uint4 attr_node = read_node(kg, &offset);

  /* Only sum extinction for volumes, variable is shared with surface transparency. */
  if (shader_type != SHADER_TYPE_VOLUME) {
    return offset;
  }

  uint density_offset, anisotropy_offset, absorption_color_offset, mix_weight_offset;
  svm_unpack_node_uchar4(
      node.y, &density_offset, &anisotropy_offset, &absorption_color_offset, &mix_weight_offset);
  float mix_weight = (stack_valid(mix_weight_offset) ? stack_load_float(stack, mix_weight_offset) :
                                                       1.0f);

  if (mix_weight == 0.0f) {
    return offset;
  }

  /* Compute density. */
  float primitive_density = 1.0f;
  float density = (stack_valid(density_offset)) ? stack_load_float(stack, density_offset) :
                                                  __uint_as_float(value_node.x);
  density = mix_weight * fmaxf(density, 0.0f);

  if (density > CLOSURE_WEIGHT_CUTOFF) {
    /* Density and color attribute lookup if available. */
    const AttributeDescriptor attr_density = find_attribute(kg, sd, attr_node.x);
    if (attr_density.offset != ATTR_STD_NOT_FOUND) {
      primitive_density = primitive_volume_attribute_float(kg, sd, attr_density);
      density = fmaxf(density * primitive_density, 0.0f);
    }
  }

  if (density > CLOSURE_WEIGHT_CUTOFF) {
    /* Compute scattering color. */
    Spectrum color = closure_weight;

    const AttributeDescriptor attr_color = find_attribute(kg, sd, attr_node.y);
    if (attr_color.offset != ATTR_STD_NOT_FOUND) {
      color *= rgb_to_spectrum(primitive_volume_attribute_float3(kg, sd, attr_color));
    }

    /* Add closure for volume scattering. */
    ccl_private HenyeyGreensteinVolume *volume = (ccl_private HenyeyGreensteinVolume *)bsdf_alloc(
        sd, sizeof(HenyeyGreensteinVolume), color * density);
    if (volume) {
      float anisotropy = (stack_valid(anisotropy_offset)) ?
                             stack_load_float(stack, anisotropy_offset) :
                             __uint_as_float(value_node.y);
      volume->g = anisotropy;
      sd->flag |= volume_henyey_greenstein_setup(volume);
    }

    /* Add extinction weight. */
    float3 absorption_color = max(sqrt(stack_load_float3(stack, absorption_color_offset)),
                                  zero_float3());

    Spectrum zero = zero_spectrum();
    Spectrum one = one_spectrum();
    Spectrum absorption = max(one - color, zero) *
                          max(one - rgb_to_spectrum(absorption_color), zero);
    volume_extinction_setup(sd, (color + absorption) * density);
  }

  /* Compute emission. */
  if (path_flag & PATH_RAY_SHADOW) {
    /* Don't need emission for shadows. */
    return offset;
  }

  uint emission_offset, emission_color_offset, blackbody_offset, temperature_offset;
  svm_unpack_node_uchar4(
      node.z, &emission_offset, &emission_color_offset, &blackbody_offset, &temperature_offset);
  float emission = (stack_valid(emission_offset)) ? stack_load_float(stack, emission_offset) :
                                                    __uint_as_float(value_node.z);
  float blackbody = (stack_valid(blackbody_offset)) ? stack_load_float(stack, blackbody_offset) :
                                                      __uint_as_float(value_node.w);

  if (emission > CLOSURE_WEIGHT_CUTOFF) {
    float3 emission_color = stack_load_float3(stack, emission_color_offset);
    emission_setup(sd, rgb_to_spectrum(emission * emission_color));
  }

  if (blackbody > CLOSURE_WEIGHT_CUTOFF) {
    float T = stack_load_float(stack, temperature_offset);

    /* Add flame temperature from attribute if available. */
    const AttributeDescriptor attr_temperature = find_attribute(kg, sd, attr_node.z);
    if (attr_temperature.offset != ATTR_STD_NOT_FOUND) {
      float temperature = primitive_volume_attribute_float(kg, sd, attr_temperature);
      T *= fmaxf(temperature, 0.0f);
    }

    T = fmaxf(T, 0.0f);

    /* Stefan-Boltzmann law. */
    float T4 = sqr(sqr(T));
    float sigma = 5.670373e-8f * 1e-6f / M_PI_F;
    float intensity = sigma * mix(1.0f, T4, blackbody);

    if (intensity > CLOSURE_WEIGHT_CUTOFF) {
      float3 blackbody_tint = stack_load_float3(stack, node.w);
      float3 bb = blackbody_tint * intensity *
                  rec709_to_rgb(kg, svm_math_blackbody_color_rec709(T));
      emission_setup(sd, rgb_to_spectrum(bb));
    }
  }
#endif
  return offset;
}

ccl_device_noinline void svm_node_closure_emission(ccl_private ShaderData *sd,
                                                   ccl_private float *stack,
                                                   Spectrum closure_weight,
                                                   uint4 node)
{
  uint mix_weight_offset = node.y;
  Spectrum weight = closure_weight;

  if (stack_valid(mix_weight_offset)) {
    float mix_weight = stack_load_float(stack, mix_weight_offset);

    if (mix_weight == 0.0f) {
      return;
    }

    weight *= mix_weight;
  }

  emission_setup(sd, weight);
}

ccl_device_noinline void svm_node_closure_background(ccl_private ShaderData *sd,
                                                     ccl_private float *stack,
                                                     Spectrum closure_weight,
                                                     uint4 node)
{
  uint mix_weight_offset = node.y;
  Spectrum weight = closure_weight;

  if (stack_valid(mix_weight_offset)) {
    float mix_weight = stack_load_float(stack, mix_weight_offset);

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
                                                  uint4 node)
{
  uint mix_weight_offset = node.y;

  if (stack_valid(mix_weight_offset)) {
    float mix_weight = stack_load_float(stack, mix_weight_offset);

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

ccl_device void svm_node_closure_set_weight(
    ccl_private ShaderData *sd, ccl_private Spectrum *closure_weight, uint r, uint g, uint b)
{
  *closure_weight = rgb_to_spectrum(
      make_float3(__uint_as_float(r), __uint_as_float(g), __uint_as_float(b)));
}

ccl_device void svm_node_closure_weight(ccl_private ShaderData *sd,
                                        ccl_private float *stack,
                                        ccl_private Spectrum *closure_weight,
                                        uint weight_offset)
{
  *closure_weight = rgb_to_spectrum(stack_load_float3(stack, weight_offset));
}

ccl_device_noinline void svm_node_emission_weight(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  ccl_private Spectrum *closure_weight,
                                                  uint4 node)
{
  uint color_offset = node.y;
  uint strength_offset = node.z;

  float strength = stack_load_float(stack, strength_offset);
  *closure_weight = rgb_to_spectrum(stack_load_float3(stack, color_offset)) * strength;
}

ccl_device_noinline void svm_node_mix_closure(ccl_private ShaderData *sd,
                                              ccl_private float *stack,
                                              uint4 node)
{
  /* fetch weight from blend input, previous mix closures,
   * and write to stack to be used by closure nodes later */
  uint weight_offset, in_weight_offset, weight1_offset, weight2_offset;
  svm_unpack_node_uchar4(
      node.y, &weight_offset, &in_weight_offset, &weight1_offset, &weight2_offset);

  float weight = stack_load_float(stack, weight_offset);
  weight = saturatef(weight);

  float in_weight = (stack_valid(in_weight_offset)) ? stack_load_float(stack, in_weight_offset) :
                                                      1.0f;

  if (stack_valid(weight1_offset))
    stack_store_float(stack, weight1_offset, in_weight * (1.0f - weight));
  if (stack_valid(weight2_offset))
    stack_store_float(stack, weight2_offset, in_weight * weight);
}

/* (Bump) normal */

ccl_device void svm_node_set_normal(KernelGlobals kg,
                                    ccl_private ShaderData *sd,
                                    ccl_private float *stack,
                                    uint in_direction,
                                    uint out_normal)
{
  float3 normal = stack_load_float3(stack, in_direction);
  sd->N = normal;
  stack_store_float3(stack, out_normal, normal);
}

CCL_NAMESPACE_END
