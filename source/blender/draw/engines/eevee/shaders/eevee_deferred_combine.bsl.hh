/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view.bsl.hh"
#include "eevee_closure.bsl.hh"
#include "eevee_colorspace_lib.bsl.hh"
#include "eevee_gbuffer_read.bsl.hh"
#include "eevee_hiz.bsl.hh"
#include "eevee_renderpass.bsl.hh"
#include "gpu_shader_fullscreen_lib.glsl"
#include "gpu_shader_shared_exponent_lib.glsl"

namespace eevee::deferred {

struct Combine {
  /* NOTE: Both light IDs have a valid specialized assignment of '-1' so only when default is
   * present will we instead dynamically look-up ID from the uniform buffer. */
  [[specialization_constant(false)]] bool render_pass_diffuse_light_enabled;
  [[specialization_constant(false)]] bool render_pass_specular_light_enabled;
  [[specialization_constant(false)]] bool render_pass_normal_enabled;
  [[specialization_constant(false)]] bool render_pass_position_enabled;
  [[specialization_constant(false)]] bool render_passes_denoising_depth_enabled;
  [[specialization_constant(false)]] bool render_passes_denoising_normal_enabled;
  [[specialization_constant(false)]] bool render_passes_denoising_roughness_enabled;
  [[specialization_constant(false)]] bool render_passes_denoising_diffuse_albedo_enabled;
  [[specialization_constant(false)]] bool render_passes_denoising_specular_albedo_enabled;
  [[specialization_constant(false)]] bool use_albedo_roughness_weighting;
  [[specialization_constant(false)]] bool use_radiance_feedback;
  [[specialization_constant(true)]] bool use_split_radiance;

  /* Inputs. */
  [[sampler(2)]] usampler2D direct_radiance_1_tx;
  [[sampler(4)]] usampler2D direct_radiance_2_tx;
  [[sampler(5)]] usampler2D direct_radiance_3_tx;
  [[sampler(6)]] sampler2D indirect_radiance_1_tx;
  [[sampler(7)]] sampler2D indirect_radiance_2_tx;
  [[sampler(8)]] sampler2D indirect_radiance_3_tx;

  [[image(5, read_write, SFLOAT_16_16_16_16)]] image2D radiance_feedback_img;

  float3 load_radiance_direct(int2 texel, uchar i) const
  {
    uint data = 0u;
    switch (i) {
      case 0:
        data = texelFetch(direct_radiance_1_tx, texel, 0).r;
        break;
      case 1:
        data = texelFetch(direct_radiance_2_tx, texel, 0).r;
        break;
      case 2:
        data = texelFetch(direct_radiance_3_tx, texel, 0).r;
        break;
      default:
        break;
    }
    return rgb9e5_decode(data);
  }

  float3 load_radiance_indirect(int2 texel, uchar i) const
  {
    switch (i) {
      case 0:
        return texelFetch(indirect_radiance_1_tx, texel, 0).rgb;
      case 1:
        return texelFetch(indirect_radiance_2_tx, texel, 0).rgb;
      case 2:
        return texelFetch(indirect_radiance_3_tx, texel, 0).rgb;
      default:
        return float3(0);
    }
    return float3(0);
  }
};

struct CombineVertOut {
  [[smooth]] float2 screen_uv;
};

[[vertex]]
void combine_vert([[vertex_id]] const int vert_id,
                  [[position]] float4 &out_position,
                  [[out]] CombineVertOut &v_out)
{
  fullscreen_vertex(vert_id, out_position, v_out.screen_uv);
}

struct CombineFragOut {
  [[frag_color(0)]] float4 combined;
};

/**
 * Combine light passes to the combined color target and apply surface colors.
 * This also fills the different render passes.
 */
/* Early fragment test is needed to avoid processing fragments background fragments. */
[[fragment, early_fragment_tests]]
void combine_frag([[resource_table]] Combine &srt,
                  [[resource_table]] RenderPassOutput &render_passes,
                  [[resource_table]] const draw::View &views,
                  [[resource_table]] const Uniform &uni,
                  [[resource_table]] const HiZ &hiz,
                  [[resource_table]] const ::gbuffer::Reader &reader,
                  [[frag_coord]] const float4 frag_co,
                  [[in]] const CombineVertOut &v_out,
                  [[out]] CombineFragOut &frag_out)
{
  int2 texel = int2(frag_co.xy);

  const gbuffer::Layers gbuf = reader.read_layers(texel);
  const uchar closure_count = gbuf.header.closure_len();
  const uint3 bin_indices = gbuf.header.bin_index_per_layer();

  float sum_weight = 0.0f;
  float3 diffuse_color = float3(0.0f);
  float3 diffuse_direct = float3(0.0f);
  float3 diffuse_indirect = float3(0.0f);
  float3 specular_color = float3(0.0f);
  float3 specular_direct = float3(0.0f);
  float3 specular_indirect = float3(0.0f);
  float3 out_direct = float3(0.0f);
  float3 out_indirect = float3(0.0f);
  float3 average_normal = float3(0.0f);
  /* Denoising render pass data. */
  float average_roughness = 0.0f;
  float3 diffuse_albedo = float3(0.0f);
  float3 specular_albedo = float3(0.0f);

  /* Unroll needed for gbuf.layer access. */
  for (int i = 0; i < 3 /* GBUFFER_LAYER_MAX */; i++) [[unroll]] {
    if (i < closure_count) {
      ClosureUndetermined cl = gbuf.layer[i];
      if (cl.type != CLOSURE_NONE_ID) {

        uchar layer_index = bin_indices[i];
        float3 closure_direct_light = srt.load_radiance_direct(texel, layer_index);
        float3 closure_indirect_light = float3(0.0f);

        if (srt.use_split_radiance) {
          closure_indirect_light = srt.load_radiance_indirect(texel, layer_index);
        }

        float closure_weight = reduce_add(cl.color);
        sum_weight += closure_weight;

        average_normal += cl.N * closure_weight;

        if (srt.render_passes_denoising_diffuse_albedo_enabled ||
            srt.render_passes_denoising_specular_albedo_enabled ||
            srt.render_passes_denoising_roughness_enabled)
        {
          /* These two values are equivalent between Cycles and EEVEE:
           * - Cycles: sqrtf(bsdf_get_specular_roughness_squared(sc))
           * - EEVEE: square(closure_apparent_roughness_get(cl)) */
          float closure_roughness = closure_apparent_roughness_get(cl);
          average_roughness += closure_roughness * closure_weight;
          if (srt.use_albedo_roughness_weighting) {
            float roughness_sq = square(closure_roughness);
            float diffuse_weight = smoothstep(0.0f, 0.15f, roughness_sq);
            diffuse_albedo += diffuse_weight * cl.color;
            specular_albedo += (1.0 - diffuse_weight) * cl.color;
          }
        }

        switch (cl.type) {
          case CLOSURE_BSDF_TRANSLUCENT_ID:
          case CLOSURE_BSSRDF_BURLEY_ID:
          case CLOSURE_BSDF_DIFFUSE_ID:
            diffuse_color += cl.color;
            diffuse_direct += closure_direct_light * cl.color;
            diffuse_indirect += closure_indirect_light * cl.color;
            if (srt.render_passes_denoising_diffuse_albedo_enabled &&
                !srt.use_albedo_roughness_weighting)
            {
              diffuse_albedo += cl.color;
            }
            break;
          case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
          case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
          case CLOSURE_BSDF_THIN_GLASS_TRANSMISSION_ID:
            specular_color += cl.color;
            specular_direct += closure_direct_light * cl.color;
            specular_indirect += closure_indirect_light * cl.color;
            if (srt.render_passes_denoising_specular_albedo_enabled &&
                !srt.use_albedo_roughness_weighting)
            {
              specular_albedo += cl.color;
            }
            break;
          case CLOSURE_NONE_ID:
            assert(false);
            break;
        }

        if ((cl.type == CLOSURE_BSDF_TRANSLUCENT_ID ||
             cl.type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID) &&
            (reader.read_thickness(gbuf.header, texel).value() != 0.0f))
        {
          /* We model two transmission event, so the surface color need to be applied twice. */
          cl.color *= cl.color;
        }

        out_direct += closure_direct_light * cl.color;
        out_indirect += closure_indirect_light * cl.color;
      }
    }
  }

  if (srt.use_radiance_feedback) {
    /* Output unmodified radiance for indirect lighting. */
    float3 out_radiance = imageLoad(srt.radiance_feedback_img, texel).rgb;
    out_radiance += out_direct + out_indirect;
    /* Prevent NaNs from propagating. */
    out_radiance = any(isnan(out_radiance)) ? float3(0.0f) : out_radiance;
    imageStore(srt.radiance_feedback_img, texel, float4(out_radiance, 0.0f));
  }

  /* Light clamping. */
  float clamp_direct = uni.uniform_buf.clamp.surface_direct;
  float clamp_indirect = uni.uniform_buf.clamp.surface_indirect;
  out_direct = colorspace::brightness_clamp_max(out_direct, clamp_direct);
  out_indirect = colorspace::brightness_clamp_max(out_indirect, clamp_indirect);
  /* Apply contribution scaling after clamping (compositing-equivalent). */
  out_direct *= uni.uniform_buf.clamp.direct_scale;
  out_indirect *= uni.uniform_buf.clamp.indirect_scale;

  /* TODO(@fclem): Shouldn't we clamp these relative the main clamp? */
  diffuse_direct = colorspace::brightness_clamp_max(diffuse_direct, clamp_direct);
  diffuse_indirect = colorspace::brightness_clamp_max(diffuse_indirect, clamp_indirect);
  specular_direct = colorspace::brightness_clamp_max(specular_direct, clamp_direct);
  specular_indirect = colorspace::brightness_clamp_max(specular_indirect, clamp_indirect);

  diffuse_direct *= uni.uniform_buf.clamp.direct_scale;
  diffuse_indirect *= uni.uniform_buf.clamp.indirect_scale;
  specular_direct *= uni.uniform_buf.clamp.direct_scale;
  specular_indirect *= uni.uniform_buf.clamp.indirect_scale;

  /* Light passes. */
  if (srt.render_pass_diffuse_light_enabled) {
    float3 diffuse_light = diffuse_direct + diffuse_indirect;
    render_passes.store_color(
        texel, uni.uniform_buf.render_pass.diffuse_color_id, float4(diffuse_color, 1.0f));
    render_passes.store_color(
        texel, uni.uniform_buf.render_pass.diffuse_light_id, float4(diffuse_light, 1.0f));
  }
  if (srt.render_pass_specular_light_enabled) {
    float3 specular_light = specular_direct + specular_indirect;
    render_passes.store_color(
        texel, uni.uniform_buf.render_pass.specular_color_id, float4(specular_color, 1.0f));
    render_passes.store_color(
        texel, uni.uniform_buf.render_pass.specular_light_id, float4(specular_light, 1.0f));
  }
  if (srt.render_pass_normal_enabled || srt.render_passes_denoising_normal_enabled) {
    float normal_len = length(average_normal);
    /* Normalize or fallback to default normal. */
    average_normal = (normal_len < 1e-5f) ? gbuf.surface_N() : (average_normal / normal_len);
  }
  if (srt.render_pass_normal_enabled) {
    render_passes.store_color(
        texel, uni.uniform_buf.render_pass.normal_id, float4(average_normal, 1.0f));
  }
  if (srt.render_pass_position_enabled) {
    const ViewMatrices view = views.get(0);
    float depth = texelFetch(hiz.hiz_tx, texel, 0).r;
    float3 P = view.point_screen_to_world(float3(v_out.screen_uv, depth));
    render_passes.store_color(texel, uni.uniform_buf.render_pass.position_id, float4(P, 1.0f));
  }
  if (srt.render_passes_denoising_normal_enabled) {
    const ViewMatrices view = views.get(0);
    average_normal = view.normal_world_to_view(average_normal);
    /* For compatibility with Cycles */
    average_normal.z *= -1.0f;
    render_passes.store_color(
        texel, uni.uniform_buf.render_pass.denoising_normal_id, float4(average_normal, 1.0f));
  }
  if (srt.render_passes_denoising_diffuse_albedo_enabled) {
    render_passes.store_color(texel,
                              uni.uniform_buf.render_pass.denoising_diffuse_albedo_id,
                              float4(diffuse_albedo, 1.0f));
  }
  if (srt.render_passes_denoising_specular_albedo_enabled) {
    render_passes.store_color(texel,
                              uni.uniform_buf.render_pass.denoising_specular_albedo_id,
                              float4(specular_albedo, 1.0f));
  }
  if (srt.render_passes_denoising_roughness_enabled) {
    if (sum_weight > 0.0f) {
      average_roughness /= sum_weight;
    }
    render_passes.store_value(
        texel, uni.uniform_buf.render_pass.denoising_roughness_id, average_roughness);
  }

  frag_out.combined = float4(out_direct + out_indirect, 0.0f);
  frag_out.combined = any(isnan(frag_out.combined)) ? float4(1.0f, 0.0f, 1.0f, 0.0f) :
                                                      frag_out.combined;
  frag_out.combined = colorspace::safe_color(frag_out.combined);
}

}  // namespace eevee::deferred

PipelineGraphic eevee_deferred_combine(eevee::deferred::combine_vert,
                                       eevee::deferred::combine_frag);
