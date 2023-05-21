/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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
ccl_device_noinline int svm_node_closure_bsdf(KernelGlobals kg,
                                              ccl_private ShaderData *sd,
                                              ccl_private float *stack,
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
  else {
    return svm_node_closure_bsdf_skip(kg, offset, type);
  }

  float3 N = stack_valid(data_node.x) ? safe_normalize(stack_load_float3(stack, data_node.x)) :
                                        sd->N;

  float param1 = (stack_valid(param1_offset)) ? stack_load_float(stack, param1_offset) :
                                                __uint_as_float(node.z);
  float param2 = (stack_valid(param2_offset)) ? stack_load_float(stack, param2_offset) :
                                                __uint_as_float(node.w);

  switch (type) {
    case CLOSURE_BSDF_PRINCIPLED_ID: {
      uint specular_offset, roughness_offset, specular_tint_offset, anisotropic_offset,
          sheen_offset, sheen_tint_offset, clearcoat_offset, clearcoat_roughness_offset,
          eta_offset, transmission_offset, anisotropic_rotation_offset,
          transmission_roughness_offset;
      uint4 data_node2 = read_node(kg, &offset);

      float3 T = stack_load_float3(stack, data_node.y);
      svm_unpack_node_uchar4(data_node.z,
                             &specular_offset,
                             &roughness_offset,
                             &specular_tint_offset,
                             &anisotropic_offset);
      svm_unpack_node_uchar4(data_node.w,
                             &sheen_offset,
                             &sheen_tint_offset,
                             &clearcoat_offset,
                             &clearcoat_roughness_offset);
      svm_unpack_node_uchar4(data_node2.x,
                             &eta_offset,
                             &transmission_offset,
                             &anisotropic_rotation_offset,
                             &transmission_roughness_offset);

      // get Disney principled parameters
      float metallic = param1;
      float subsurface = param2;
      float specular = stack_load_float(stack, specular_offset);
      float roughness = stack_load_float(stack, roughness_offset);
      float specular_tint = stack_load_float(stack, specular_tint_offset);
      float anisotropic = stack_load_float(stack, anisotropic_offset);
      float sheen = stack_load_float(stack, sheen_offset);
      float sheen_tint = stack_load_float(stack, sheen_tint_offset);
      float clearcoat = stack_load_float(stack, clearcoat_offset);
      float clearcoat_roughness = stack_load_float(stack, clearcoat_roughness_offset);
      float transmission = stack_load_float(stack, transmission_offset);
      float anisotropic_rotation = stack_load_float(stack, anisotropic_rotation_offset);
      float transmission_roughness = stack_load_float(stack, transmission_roughness_offset);
      float eta = fmaxf(stack_load_float(stack, eta_offset), 1e-5f);

      ClosureType distribution = (ClosureType)data_node2.y;
      ClosureType subsurface_method = (ClosureType)data_node2.z;

      /* rotate tangent */
      if (anisotropic_rotation != 0.0f)
        T = rotate_around_axis(T, N, anisotropic_rotation * M_2PI_F);

      /* calculate ior */
      float ior = (sd->flag & SD_BACKFACING) ? 1.0f / eta : eta;

      /* Calculate fresnel for refraction. */
      float3 valid_reflection_N = maybe_ensure_valid_specular_reflection(sd, N);
      float cosNI = dot(valid_reflection_N, sd->wi);
      float fresnel = fresnel_dielectric_cos(cosNI, ior);

      // calculate weights of the diffuse and specular part
      float diffuse_weight = (1.0f - saturatef(metallic)) * (1.0f - saturatef(transmission));

      float final_transmission = saturatef(transmission) * (1.0f - saturatef(metallic));
      float specular_weight = (1.0f - final_transmission);

      // get the base color
      uint4 data_base_color = read_node(kg, &offset);
      float3 base_color = stack_valid(data_base_color.x) ?
                              stack_load_float3(stack, data_base_color.x) :
                              make_float3(__uint_as_float(data_base_color.y),
                                          __uint_as_float(data_base_color.z),
                                          __uint_as_float(data_base_color.w));

      // get the additional clearcoat normal and subsurface scattering radius
      uint4 data_cn_ssr = read_node(kg, &offset);
      float3 clearcoat_normal = stack_valid(data_cn_ssr.x) ?
                                    stack_load_float3(stack, data_cn_ssr.x) :
                                    sd->N;
      clearcoat_normal = maybe_ensure_valid_specular_reflection(sd, clearcoat_normal);
      float3 subsurface_radius = stack_valid(data_cn_ssr.y) ?
                                     stack_load_float3(stack, data_cn_ssr.y) :
                                     one_float3();
      float subsurface_ior = stack_valid(data_cn_ssr.z) ? stack_load_float(stack, data_cn_ssr.z) :
                                                          1.4f;
      float subsurface_anisotropy = stack_valid(data_cn_ssr.w) ?
                                        stack_load_float(stack, data_cn_ssr.w) :
                                        0.0f;

      // get the subsurface color
      uint4 data_subsurface_color = read_node(kg, &offset);
      float3 subsurface_color = stack_valid(data_subsurface_color.x) ?
                                    stack_load_float3(stack, data_subsurface_color.x) :
                                    make_float3(__uint_as_float(data_subsurface_color.y),
                                                __uint_as_float(data_subsurface_color.z),
                                                __uint_as_float(data_subsurface_color.w));

      Spectrum weight = sd->svm_closure_weight * mix_weight;

#ifdef __SUBSURFACE__
      float3 mixed_ss_base_color = subsurface_color * subsurface +
                                   base_color * (1.0f - subsurface);
      Spectrum subsurf_weight = weight * rgb_to_spectrum(mixed_ss_base_color) * diffuse_weight;

      /* disable in case of diffuse ancestor, can't see it well then and
       * adds considerably noise due to probabilities of continuing path
       * getting lower and lower */
      if (path_flag & PATH_RAY_DIFFUSE_ANCESTOR) {
        subsurface = 0.0f;

        /* need to set the base color in this case such that the
         * rays get the correctly mixed color after transmitting
         * the object */
        base_color = mixed_ss_base_color;
      }

      /* diffuse */
      if (fabsf(average(mixed_ss_base_color)) > CLOSURE_WEIGHT_CUTOFF) {
        if (subsurface <= CLOSURE_WEIGHT_CUTOFF && diffuse_weight > CLOSURE_WEIGHT_CUTOFF) {
          Spectrum diff_weight = weight * rgb_to_spectrum(base_color) * diffuse_weight;

          ccl_private PrincipledDiffuseBsdf *bsdf = (ccl_private PrincipledDiffuseBsdf *)
              bsdf_alloc(sd, sizeof(PrincipledDiffuseBsdf), diff_weight);

          if (bsdf) {
            bsdf->N = N;
            bsdf->roughness = roughness;

            /* setup bsdf */
            sd->flag |= bsdf_principled_diffuse_setup(bsdf, PRINCIPLED_DIFFUSE_FULL);
          }
        }
        else if (subsurface > CLOSURE_WEIGHT_CUTOFF) {
          ccl_private Bssrdf *bssrdf = bssrdf_alloc(sd, subsurf_weight);

          if (bssrdf) {
            bssrdf->radius = rgb_to_spectrum(subsurface_radius * subsurface);
            bssrdf->albedo = rgb_to_spectrum(mixed_ss_base_color);
            bssrdf->N = N;
            bssrdf->roughness = roughness;

            /* Clamps protecting against bad/extreme and non physical values. */
            subsurface_ior = clamp(subsurface_ior, 1.01f, 3.8f);
            bssrdf->anisotropy = clamp(subsurface_anisotropy, 0.0f, 0.9f);

            /* setup bsdf */
            sd->flag |= bssrdf_setup(sd, bssrdf, subsurface_method, subsurface_ior);
          }
        }
      }
#else
      /* diffuse */
      if (diffuse_weight > CLOSURE_WEIGHT_CUTOFF) {
        Spectrum diff_weight = weight * rgb_to_spectrum(base_color) * diffuse_weight;

        ccl_private PrincipledDiffuseBsdf *bsdf = (ccl_private PrincipledDiffuseBsdf *)bsdf_alloc(
            sd, sizeof(PrincipledDiffuseBsdf), diff_weight);

        if (bsdf) {
          bsdf->N = N;
          bsdf->roughness = roughness;

          /* setup bsdf */
          sd->flag |= bsdf_principled_diffuse_setup(bsdf, PRINCIPLED_DIFFUSE_FULL);
        }
      }
#endif

      /* sheen */
      if (diffuse_weight > CLOSURE_WEIGHT_CUTOFF && sheen > CLOSURE_WEIGHT_CUTOFF) {
        float m_cdlum = linear_rgb_to_gray(kg, base_color);
        float3 m_ctint = m_cdlum > 0.0f ? base_color / m_cdlum :
                                          one_float3();  // normalize lum. to isolate hue+sat

        /* color of the sheen component */
        float3 sheen_color = make_float3(1.0f - sheen_tint) + m_ctint * sheen_tint;

        Spectrum sheen_weight = weight * sheen * rgb_to_spectrum(sheen_color) * diffuse_weight;

        ccl_private PrincipledSheenBsdf *bsdf = (ccl_private PrincipledSheenBsdf *)bsdf_alloc(
            sd, sizeof(PrincipledSheenBsdf), sheen_weight);

        if (bsdf) {
          bsdf->N = N;

          /* setup bsdf */
          sd->flag |= bsdf_principled_sheen_setup(sd, bsdf);
        }
      }

      /* specular reflection */
#ifdef __CAUSTICS_TRICKS__
      if (kernel_data.integrator.caustics_reflective || (path_flag & PATH_RAY_DIFFUSE) == 0) {
#endif
        if (specular_weight > CLOSURE_WEIGHT_CUTOFF &&
            (specular > CLOSURE_WEIGHT_CUTOFF || metallic > CLOSURE_WEIGHT_CUTOFF))
        {
          Spectrum spec_weight = weight * specular_weight;

          ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
              sd, sizeof(MicrofacetBsdf), spec_weight);
          ccl_private FresnelPrincipledV1 *fresnel =
              (bsdf != NULL) ? (ccl_private FresnelPrincipledV1 *)closure_alloc_extra(
                                   sd, sizeof(FresnelPrincipledV1)) :
                               NULL;

          if (bsdf && fresnel) {
            bsdf->N = valid_reflection_N;
            bsdf->ior = (2.0f / (1.0f - safe_sqrtf(0.08f * specular))) - 1.0f;
            bsdf->T = T;

            float aspect = safe_sqrtf(1.0f - anisotropic * 0.9f);
            float r2 = roughness * roughness;

            bsdf->alpha_x = r2 / aspect;
            bsdf->alpha_y = r2 * aspect;

            float m_cdlum = 0.3f * base_color.x + 0.6f * base_color.y +
                            0.1f * base_color.z;  // luminance approx.
            float3 m_ctint = m_cdlum > 0.0f ? base_color / m_cdlum :
                                              one_float3();  // normalize lum. to isolate hue+sat
            float3 tmp_col = make_float3(1.0f - specular_tint) + m_ctint * specular_tint;

            fresnel->cspec0 = rgb_to_spectrum((specular * 0.08f * tmp_col) * (1.0f - metallic) +
                                              base_color * metallic);
            fresnel->color = rgb_to_spectrum(base_color);

            /* setup bsdf */

            /* Use single-scatter GGX. */
            if (distribution == CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID || roughness <= 0.075f) {
              sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
              bsdf_microfacet_setup_fresnel_principledv1(bsdf, sd, fresnel);
            } /* Use multi-scatter GGX. */
            else {

              bsdf->fresnel = fresnel;
              sd->flag |= bsdf_microfacet_multi_ggx_fresnel_setup(bsdf, sd);
            }
          }
        }
#ifdef __CAUSTICS_TRICKS__
      }
#endif

      /* BSDF */
#ifdef __CAUSTICS_TRICKS__
      if (kernel_data.integrator.caustics_reflective ||
          kernel_data.integrator.caustics_refractive || (path_flag & PATH_RAY_DIFFUSE) == 0)
      {
#endif
        if (final_transmission > CLOSURE_WEIGHT_CUTOFF) {
          Spectrum glass_weight = weight * final_transmission;
          float3 cspec0 = base_color * specular_tint + make_float3(1.0f - specular_tint);

          /* Use single-scatter GGX. */
          if (roughness <= 5e-2f || distribution == CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID) {

            float refl_roughness = roughness;

            /* reflection */
#ifdef __CAUSTICS_TRICKS__
            if (kernel_data.integrator.caustics_reflective || (path_flag & PATH_RAY_DIFFUSE) == 0)
#endif
            {
              ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
                  sd, sizeof(MicrofacetBsdf), glass_weight * fresnel);
              ccl_private FresnelPrincipledV1 *fresnel =
                  (bsdf != NULL) ? (ccl_private FresnelPrincipledV1 *)closure_alloc_extra(
                                       sd, sizeof(FresnelPrincipledV1)) :
                                   NULL;

              if (bsdf && fresnel) {
                bsdf->N = valid_reflection_N;
                bsdf->T = zero_float3();
                bsdf->fresnel = fresnel;

                bsdf->alpha_x = refl_roughness * refl_roughness;
                bsdf->alpha_y = refl_roughness * refl_roughness;
                bsdf->ior = ior;

                /* setup bsdf */
                sd->flag |= bsdf_microfacet_ggx_setup(bsdf);

                fresnel->color = rgb_to_spectrum(base_color);
                fresnel->cspec0 = rgb_to_spectrum(cspec0);
                bsdf_microfacet_setup_fresnel_principledv1(bsdf, sd, fresnel);
              }
            }

            /* refraction */
#ifdef __CAUSTICS_TRICKS__
            if (kernel_data.integrator.caustics_refractive || (path_flag & PATH_RAY_DIFFUSE) == 0)
#endif
            {
              /* This is to prevent MNEE from receiving a null BSDF. */
              float refraction_fresnel = fmaxf(0.0001f, 1.0f - fresnel);
              ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
                  sd,
                  sizeof(MicrofacetBsdf),
                  rgb_to_spectrum(base_color) * glass_weight * refraction_fresnel);
              if (bsdf) {
                bsdf->N = valid_reflection_N;
                bsdf->T = zero_float3();
                bsdf->fresnel = NULL;

                if (distribution == CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID)
                  transmission_roughness = 1.0f - (1.0f - refl_roughness) *
                                                      (1.0f - transmission_roughness);
                else
                  transmission_roughness = refl_roughness;

                bsdf->alpha_x = transmission_roughness * transmission_roughness;
                bsdf->alpha_y = transmission_roughness * transmission_roughness;
                bsdf->ior = ior;

                /* setup bsdf */
                sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
              }
            }
          } /* Use multi-scatter GGX. */
          else {
            ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
                sd, sizeof(MicrofacetBsdf), glass_weight);
            ccl_private FresnelPrincipledV1 *fresnel =
                (bsdf != NULL) ? (ccl_private FresnelPrincipledV1 *)closure_alloc_extra(
                                     sd, sizeof(FresnelPrincipledV1)) :
                                 NULL;

            if (bsdf && fresnel) {
              bsdf->N = valid_reflection_N;
              bsdf->fresnel = fresnel;
              bsdf->T = zero_float3();

              bsdf->alpha_x = roughness * roughness;
              bsdf->alpha_y = roughness * roughness;
              bsdf->ior = ior;

              fresnel->color = rgb_to_spectrum(base_color);
              fresnel->cspec0 = rgb_to_spectrum(cspec0);

              /* setup bsdf */
              sd->flag |= bsdf_microfacet_multi_ggx_glass_fresnel_setup(bsdf, sd);
            }
          }
        }
#ifdef __CAUSTICS_TRICKS__
      }
#endif

      /* clearcoat */
#ifdef __CAUSTICS_TRICKS__
      if (kernel_data.integrator.caustics_reflective || (path_flag & PATH_RAY_DIFFUSE) == 0) {
#endif
        Spectrum clearcoat_weight = 0.25f * clearcoat * weight;
        ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
            sd, sizeof(MicrofacetBsdf), clearcoat_weight);

        if (bsdf) {
          bsdf->N = clearcoat_normal;
          bsdf->T = zero_float3();
          bsdf->ior = 1.5f;

          bsdf->alpha_x = clearcoat_roughness * clearcoat_roughness;
          bsdf->alpha_y = clearcoat_roughness * clearcoat_roughness;

          /* setup bsdf */
          sd->flag |= bsdf_microfacet_ggx_clearcoat_setup(bsdf, sd);
        }
#ifdef __CAUSTICS_TRICKS__
      }
#endif

      break;
    }
    case CLOSURE_BSDF_DIFFUSE_ID: {
      Spectrum weight = sd->svm_closure_weight * mix_weight;
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
      Spectrum weight = sd->svm_closure_weight * mix_weight;
      ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
          sd, sizeof(DiffuseBsdf), weight);

      if (bsdf) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        sd->flag |= bsdf_translucent_setup(bsdf);
      }
      break;
    }
    case CLOSURE_BSDF_TRANSPARENT_ID: {
      Spectrum weight = sd->svm_closure_weight * mix_weight;
      bsdf_transparent_setup(sd, weight, path_flag);
      break;
    }
    case CLOSURE_BSDF_REFLECTION_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
    case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
    case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID: {
#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE))
        break;
#endif
      Spectrum weight = sd->svm_closure_weight * mix_weight;
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), weight);

      if (!bsdf) {
        break;
      }

      float roughness = sqr(param1);

      bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
      bsdf->ior = 1.0f;
      bsdf->fresnel = NULL;

      /* compute roughness */
      float anisotropy = clamp(param2, -0.99f, 0.99f);
      if (data_node.y == SVM_STACK_INVALID || fabsf(anisotropy) <= 1e-4f) {
        /* Isotropic case. */
        bsdf->T = zero_float3();
        bsdf->alpha_x = roughness;
        bsdf->alpha_y = roughness;
      }
      else {
        bsdf->T = stack_load_float3(stack, data_node.y);

        /* rotate tangent */
        float rotation = stack_load_float(stack, data_node.z);
        if (rotation != 0.0f)
          bsdf->T = rotate_around_axis(bsdf->T, bsdf->N, rotation * M_2PI_F);

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
      if (type == CLOSURE_BSDF_REFLECTION_ID)
        sd->flag |= bsdf_reflection_setup(bsdf);
      else if (type == CLOSURE_BSDF_MICROFACET_BECKMANN_ID)
        sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
      else if (type == CLOSURE_BSDF_MICROFACET_GGX_ID)
        sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
      else if (type == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID) {
        kernel_assert(stack_valid(data_node.w));
        ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)closure_alloc_extra(
            sd, sizeof(FresnelConstant));
        if (fresnel) {
          bsdf->fresnel = fresnel;
          fresnel->color = rgb_to_spectrum(stack_load_float3(stack, data_node.w));
          sd->flag |= bsdf_microfacet_multi_ggx_setup(bsdf);
        }
      }
      else {
        sd->flag |= bsdf_ashikhmin_shirley_setup(bsdf);
      }

      break;
    }
    case CLOSURE_BSDF_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID: {
#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_refractive && (path_flag & PATH_RAY_DIFFUSE))
        break;
#endif
      Spectrum weight = sd->svm_closure_weight * mix_weight;
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), weight);

      if (bsdf) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bsdf->T = zero_float3();
        bsdf->fresnel = NULL;

        float eta = fmaxf(param2, 1e-5f);
        eta = (sd->flag & SD_BACKFACING) ? 1.0f / eta : eta;

        /* setup bsdf */
        if (type == CLOSURE_BSDF_REFRACTION_ID) {
          bsdf->alpha_x = 0.0f;
          bsdf->alpha_y = 0.0f;
          bsdf->ior = eta;

          sd->flag |= bsdf_refraction_setup(bsdf);
        }
        else {
          float roughness = sqr(param1);
          bsdf->alpha_x = roughness;
          bsdf->alpha_y = roughness;
          bsdf->ior = eta;

          if (type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID)
            sd->flag |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
          else
            sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
        }
      }

      break;
    }
    case CLOSURE_BSDF_SHARP_GLASS_ID:
    case CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID:
    case CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID: {
#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_reflective &&
          !kernel_data.integrator.caustics_refractive && (path_flag & PATH_RAY_DIFFUSE))
        break;
#endif
      Spectrum weight = sd->svm_closure_weight * mix_weight;
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), weight);

      if (bsdf) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bsdf->T = zero_float3();
        bsdf->fresnel = NULL;

        float eta = fmaxf(param2, 1e-5f);
        eta = (sd->flag & SD_BACKFACING) ? 1.0f / eta : eta;

        /* setup bsdf */
        if (type == CLOSURE_BSDF_SHARP_GLASS_ID) {
          bsdf->alpha_x = 0.0f;
          bsdf->alpha_y = 0.0f;
          bsdf->ior = eta;

          sd->flag |= bsdf_sharp_glass_setup(bsdf);
        }
        else {
          float roughness = sqr(param1);
          bsdf->alpha_x = roughness;
          bsdf->alpha_y = roughness;
          bsdf->ior = eta;

          if (type == CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID)
            sd->flag |= bsdf_microfacet_beckmann_glass_setup(bsdf);
          else
            sd->flag |= bsdf_microfacet_ggx_glass_setup(bsdf);
        }
      }

      break;
    }
    case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID: {
#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_reflective &&
          !kernel_data.integrator.caustics_refractive && (path_flag & PATH_RAY_DIFFUSE))
        break;
#endif
      Spectrum weight = sd->svm_closure_weight * mix_weight;
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), weight);
      if (!bsdf) {
        break;
      }

      ccl_private FresnelConstant *fresnel = (ccl_private FresnelConstant *)closure_alloc_extra(
          sd, sizeof(FresnelConstant));
      if (!fresnel) {
        break;
      }

      bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
      bsdf->fresnel = fresnel;
      bsdf->T = zero_float3();

      float roughness = sqr(param1);
      bsdf->alpha_x = roughness;
      bsdf->alpha_y = roughness;
      float eta = fmaxf(param2, 1e-5f);
      bsdf->ior = (sd->flag & SD_BACKFACING) ? 1.0f / eta : eta;

      kernel_assert(stack_valid(data_node.z));
      fresnel->color = rgb_to_spectrum(stack_load_float3(stack, data_node.z));

      /* setup bsdf */
      sd->flag |= bsdf_microfacet_multi_ggx_glass_setup(bsdf);
      break;
    }
    case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID: {
      Spectrum weight = sd->svm_closure_weight * mix_weight;
      ccl_private VelvetBsdf *bsdf = (ccl_private VelvetBsdf *)bsdf_alloc(
          sd, sizeof(VelvetBsdf), weight);

      if (bsdf) {
        bsdf->N = N;

        bsdf->sigma = saturatef(param1);
        sd->flag |= bsdf_ashikhmin_velvet_setup(bsdf);
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
      Spectrum weight = sd->svm_closure_weight * mix_weight;
      ccl_private ToonBsdf *bsdf = (ccl_private ToonBsdf *)bsdf_alloc(
          sd, sizeof(ToonBsdf), weight);

      if (bsdf) {
        bsdf->N = N;
        bsdf->size = param1;
        bsdf->smooth = param2;

        if (type == CLOSURE_BSDF_DIFFUSE_TOON_ID)
          sd->flag |= bsdf_diffuse_toon_setup(bsdf);
        else
          sd->flag |= bsdf_glossy_toon_setup(bsdf);
      }
      break;
    }
#ifdef __HAIR__
    case CLOSURE_BSDF_HAIR_PRINCIPLED_ID: {
      uint4 data_node2 = read_node(kg, &offset);
      uint4 data_node3 = read_node(kg, &offset);
      uint4 data_node4 = read_node(kg, &offset);

      Spectrum weight = sd->svm_closure_weight * mix_weight;

      uint offset_ofs, ior_ofs, color_ofs, parametrization;
      svm_unpack_node_uchar4(data_node.y, &offset_ofs, &ior_ofs, &color_ofs, &parametrization);
      float alpha = stack_load_float_default(stack, offset_ofs, data_node.z);
      float ior = stack_load_float_default(stack, ior_ofs, data_node.w);

      uint coat_ofs, melanin_ofs, melanin_redness_ofs, absorption_coefficient_ofs;
      svm_unpack_node_uchar4(data_node2.x,
                             &coat_ofs,
                             &melanin_ofs,
                             &melanin_redness_ofs,
                             &absorption_coefficient_ofs);

      uint tint_ofs, random_ofs, random_color_ofs, random_roughness_ofs;
      svm_unpack_node_uchar4(
          data_node3.x, &tint_ofs, &random_ofs, &random_color_ofs, &random_roughness_ofs);

      const AttributeDescriptor attr_descr_random = find_attribute(kg, sd, data_node4.y);
      float random = 0.0f;
      if (attr_descr_random.offset != ATTR_STD_NOT_FOUND) {
        random = primitive_surface_attribute_float(kg, sd, attr_descr_random, NULL, NULL);
      }
      else {
        random = stack_load_float_default(stack, random_ofs, data_node3.y);
      }

      ccl_private PrincipledHairBSDF *bsdf = (ccl_private PrincipledHairBSDF *)bsdf_alloc(
          sd, sizeof(PrincipledHairBSDF), weight);
      if (bsdf) {
        ccl_private PrincipledHairExtra *extra = (ccl_private PrincipledHairExtra *)
            closure_alloc_extra(sd, sizeof(PrincipledHairExtra));

        if (!extra)
          break;

        /* Random factors range: [-randomization/2, +randomization/2]. */
        float random_roughness = stack_load_float_default(
            stack, random_roughness_ofs, data_node3.w);
        float factor_random_roughness = 1.0f + 2.0f * (random - 0.5f) * random_roughness;
        float roughness = param1 * factor_random_roughness;
        float radial_roughness = param2 * factor_random_roughness;

        /* Remap Coat value to [0, 100]% of Roughness. */
        float coat = stack_load_float_default(stack, coat_ofs, data_node2.y);
        float m0_roughness = 1.0f - clamp(coat, 0.0f, 1.0f);

        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bsdf->v = roughness;
        bsdf->s = radial_roughness;
        bsdf->m0_roughness = m0_roughness;
        bsdf->alpha = alpha;
        bsdf->eta = ior;
        bsdf->extra = extra;

        switch (parametrization) {
          case NODE_PRINCIPLED_HAIR_DIRECT_ABSORPTION: {
            float3 absorption_coefficient = stack_load_float3(stack, absorption_coefficient_ofs);
            bsdf->sigma = rgb_to_spectrum(absorption_coefficient);
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
            Spectrum tint_sigma = bsdf_principled_hair_sigma_from_reflectance(
                rgb_to_spectrum(tint), radial_roughness);

            bsdf->sigma = melanin_sigma + tint_sigma;
            break;
          }
          case NODE_PRINCIPLED_HAIR_REFLECTANCE: {
            float3 color = stack_load_float3(stack, color_ofs);
            bsdf->sigma = bsdf_principled_hair_sigma_from_reflectance(rgb_to_spectrum(color),
                                                                      radial_roughness);
            break;
          }
          default: {
            /* Fallback to brownish hair, same as defaults for melanin. */
            kernel_assert(!"Invalid Principled Hair parametrization!");
            bsdf->sigma = bsdf_principled_hair_sigma_from_concentration(0.0f, 0.8054375f);
            break;
          }
        }

        sd->flag |= bsdf_principled_hair_setup(sd, bsdf);
      }
      break;
    }
    case CLOSURE_BSDF_HAIR_REFLECTION_ID:
    case CLOSURE_BSDF_HAIR_TRANSMISSION_ID: {
      Spectrum weight = sd->svm_closure_weight * mix_weight;

      ccl_private HairBsdf *bsdf = (ccl_private HairBsdf *)bsdf_alloc(
          sd, sizeof(HairBsdf), weight);

      if (bsdf) {
        bsdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bsdf->roughness1 = param1;
        bsdf->roughness2 = param2;
        bsdf->offset = -stack_load_float(stack, data_node.z);

        if (stack_valid(data_node.y)) {
          bsdf->T = normalize(stack_load_float3(stack, data_node.y));
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
    case CLOSURE_BSSRDF_RANDOM_WALK_FIXED_RADIUS_ID: {
      Spectrum weight = sd->svm_closure_weight * mix_weight;
      ccl_private Bssrdf *bssrdf = bssrdf_alloc(sd, weight);

      if (bssrdf) {
        /* disable in case of diffuse ancestor, can't see it well then and
         * adds considerably noise due to probabilities of continuing path
         * getting lower and lower */
        if (path_flag & PATH_RAY_DIFFUSE_ANCESTOR)
          param1 = 0.0f;

        bssrdf->radius = rgb_to_spectrum(stack_load_float3(stack, data_node.z) * param1);
        bssrdf->albedo = sd->svm_closure_weight;
        bssrdf->N = N;
        bssrdf->roughness = FLT_MAX;

        const float subsurface_ior = clamp(param2, 1.01f, 3.8f);
        const float subsurface_anisotropy = stack_load_float(stack, data_node.w);
        bssrdf->anisotropy = clamp(subsurface_anisotropy, 0.0f, 0.9f);

        sd->flag |= bssrdf_setup(sd, bssrdf, (ClosureType)type, subsurface_ior);
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
  Spectrum weight = sd->svm_closure_weight;

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
    Spectrum color = sd->svm_closure_weight;

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
                                                   uint4 node)
{
  uint mix_weight_offset = node.y;
  Spectrum weight = sd->svm_closure_weight;

  if (stack_valid(mix_weight_offset)) {
    float mix_weight = stack_load_float(stack, mix_weight_offset);

    if (mix_weight == 0.0f)
      return;

    weight *= mix_weight;
  }

  emission_setup(sd, weight);
}

ccl_device_noinline void svm_node_closure_background(ccl_private ShaderData *sd,
                                                     ccl_private float *stack,
                                                     uint4 node)
{
  uint mix_weight_offset = node.y;
  Spectrum weight = sd->svm_closure_weight;

  if (stack_valid(mix_weight_offset)) {
    float mix_weight = stack_load_float(stack, mix_weight_offset);

    if (mix_weight == 0.0f)
      return;

    weight *= mix_weight;
  }

  background_setup(sd, weight);
}

ccl_device_noinline void svm_node_closure_holdout(ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  uint4 node)
{
  uint mix_weight_offset = node.y;

  if (stack_valid(mix_weight_offset)) {
    float mix_weight = stack_load_float(stack, mix_weight_offset);

    if (mix_weight == 0.0f)
      return;

    closure_alloc(
        sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, sd->svm_closure_weight * mix_weight);
  }
  else
    closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, sd->svm_closure_weight);

  sd->flag |= SD_HOLDOUT;
}

/* Closure Nodes */

ccl_device_inline void svm_node_closure_store_weight(ccl_private ShaderData *sd, Spectrum weight)
{
  sd->svm_closure_weight = weight;
}

ccl_device void svm_node_closure_set_weight(ccl_private ShaderData *sd, uint r, uint g, uint b)
{
  Spectrum weight = rgb_to_spectrum(
      make_float3(__uint_as_float(r), __uint_as_float(g), __uint_as_float(b)));
  svm_node_closure_store_weight(sd, weight);
}

ccl_device void svm_node_closure_weight(ccl_private ShaderData *sd,
                                        ccl_private float *stack,
                                        uint weight_offset)
{
  Spectrum weight = rgb_to_spectrum(stack_load_float3(stack, weight_offset));
  svm_node_closure_store_weight(sd, weight);
}

ccl_device_noinline void svm_node_emission_weight(KernelGlobals kg,
                                                  ccl_private ShaderData *sd,
                                                  ccl_private float *stack,
                                                  uint4 node)
{
  uint color_offset = node.y;
  uint strength_offset = node.z;

  float strength = stack_load_float(stack, strength_offset);
  Spectrum weight = rgb_to_spectrum(stack_load_float3(stack, color_offset)) * strength;

  svm_node_closure_store_weight(sd, weight);
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
