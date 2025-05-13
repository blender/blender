/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Combine light passes to the combined color target and apply surface colors.
 * This also fills the different render passes.
 */

#include "infos/eevee_deferred_info.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_deferred_combine)

#include "eevee_colorspace_lib.glsl"
#include "eevee_gbuffer_lib.glsl"
#include "eevee_renderpass_lib.glsl"
#include "gpu_shader_shared_exponent_lib.glsl"

float3 load_radiance_direct(int2 texel, uchar i)
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

float3 load_radiance_indirect(int2 texel, uchar i)
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

void main()
{
  int2 texel = int2(gl_FragCoord.xy);

  GBufferReader gbuf = gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel);

  float3 diffuse_color = float3(0.0f);
  float3 diffuse_direct = float3(0.0f);
  float3 diffuse_indirect = float3(0.0f);
  float3 specular_color = float3(0.0f);
  float3 specular_direct = float3(0.0f);
  float3 specular_indirect = float3(0.0f);
  float3 out_direct = float3(0.0f);
  float3 out_indirect = float3(0.0f);
  float3 average_normal = float3(0.0f);

  for (uchar i = 0; i < GBUFFER_LAYER_MAX && i < gbuf.closure_count; i++) {
    ClosureUndetermined cl = gbuffer_closure_get(gbuf, i);
    if (cl.type == CLOSURE_NONE_ID) {
      continue;
    }
    uchar layer_index = gbuffer_closure_get_bin_index(gbuf, i);
    float3 closure_direct_light = load_radiance_direct(texel, layer_index);
    float3 closure_indirect_light = float3(0.0f);

    if (use_split_radiance) {
      closure_indirect_light = load_radiance_indirect(texel, layer_index);
    }

    average_normal += cl.N * reduce_add(cl.color);

    switch (cl.type) {
      case CLOSURE_BSDF_TRANSLUCENT_ID:
      case CLOSURE_BSSRDF_BURLEY_ID:
      case CLOSURE_BSDF_DIFFUSE_ID:
        diffuse_color += cl.color;
        diffuse_direct += closure_direct_light;
        diffuse_indirect += closure_indirect_light;
        break;
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        specular_color += cl.color;
        specular_direct += closure_direct_light;
        specular_indirect += closure_indirect_light;
        break;
      case CLOSURE_NONE_ID:
        assert(0);
        break;
    }

    if ((cl.type == CLOSURE_BSDF_TRANSLUCENT_ID ||
         cl.type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID) &&
        (gbuf.thickness != 0.0f))
    {
      /* We model two transmission event, so the surface color need to be applied twice. */
      cl.color *= cl.color;
    }

    out_direct += closure_direct_light * cl.color;
    out_indirect += closure_indirect_light * cl.color;
  }

  if (use_radiance_feedback) {
    /* Output unmodified radiance for indirect lighting. */
    float3 out_radiance = imageLoad(radiance_feedback_img, texel).rgb;
    out_radiance += out_direct + out_indirect;
    imageStore(radiance_feedback_img, texel, float4(out_radiance, 0.0f));
  }

  /* Light clamping. */
  float clamp_direct = uniform_buf.clamp.surface_direct;
  float clamp_indirect = uniform_buf.clamp.surface_indirect;
  out_direct = colorspace_brightness_clamp_max(out_direct, clamp_direct);
  out_indirect = colorspace_brightness_clamp_max(out_indirect, clamp_indirect);
  /* TODO(@fclem): Shouldn't we clamp these relative the main clamp? */
  diffuse_direct = colorspace_brightness_clamp_max(diffuse_direct, clamp_direct);
  diffuse_indirect = colorspace_brightness_clamp_max(diffuse_indirect, clamp_indirect);
  specular_direct = colorspace_brightness_clamp_max(specular_direct, clamp_direct);
  specular_indirect = colorspace_brightness_clamp_max(specular_indirect, clamp_indirect);

  /* Light passes. */
  if (render_pass_diffuse_light_enabled) {
    float3 diffuse_light = diffuse_direct + diffuse_indirect;
    output_renderpass_color(uniform_buf.render_pass.diffuse_color_id, float4(diffuse_color, 1.0f));
    output_renderpass_color(uniform_buf.render_pass.diffuse_light_id, float4(diffuse_light, 1.0f));
  }
  if (render_pass_specular_light_enabled) {
    float3 specular_light = specular_direct + specular_indirect;
    output_renderpass_color(uniform_buf.render_pass.specular_color_id,
                            float4(specular_color, 1.0f));
    output_renderpass_color(uniform_buf.render_pass.specular_light_id,
                            float4(specular_light, 1.0f));
  }
  if (render_pass_normal_enabled) {
    float normal_len = length(average_normal);
    /* Normalize or fallback to default normal. */
    average_normal = (normal_len < 1e-5f) ? gbuf.surface_N : (average_normal / normal_len);
    output_renderpass_color(uniform_buf.render_pass.normal_id, float4(average_normal, 1.0f));
  }

  out_combined = float4(out_direct + out_indirect, 0.0f);
  out_combined = any(isnan(out_combined)) ? float4(1.0f, 0.0f, 1.0f, 0.0f) : out_combined;
  out_combined = colorspace_safe_color(out_combined);
}
