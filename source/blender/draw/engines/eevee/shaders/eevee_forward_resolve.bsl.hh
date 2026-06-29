/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#pragma once

#include "eevee_renderpass.bsl.hh"
#include "gpu_shader_fullscreen_lib.glsl"

namespace eevee::forward {

struct ForwardResolve {

  [[sampler(0)]] sampler2D transparency_r_tx;
  [[sampler(1)]] sampler2D transparency_g_tx;
  [[sampler(2)]] sampler2D transparency_b_tx;
  [[sampler(3)]] sampler2D transparency_a_tx;
};

[[vertex]]
void fullscreen_vert([[vertex_id]] const int vert_id, [[position]] float4 &out_position)
{
  fullscreen_vertex(vert_id, out_position);
}

struct ForwardResolveFragOut {
  [[frag_color(0), index(0)]] float4 radiance;
  [[frag_color(0), index(1)]] float4 transmittance;
};

[[fragment]]
void resolve_frag([[resource_table]] const ForwardResolve &srt,
                  [[resource_table]] const eevee::Uniform &uni,
                  [[resource_table]] RenderPassOutput &render_passes,
                  [[frag_coord]] const float4 frag_co,
                  [[out]] ForwardResolveFragOut &frag_out)
{
  int2 texel = int2(frag_co.xy);

  if (uni.pipeline_buf.use_monochromatic_transmittance) {
    float4 data = texelFetch(srt.transparency_r_tx, texel, 0);
    frag_out.radiance = float4(data.rgb, 0.0f);
    frag_out.transmittance = data.aaaa;
  }
  else {
    /* The data is stored "transposed". */
    float2 channel_r = texelFetch(srt.transparency_r_tx, texel, 0).xy;
    float2 channel_g = texelFetch(srt.transparency_g_tx, texel, 0).xy;
    float2 channel_b = texelFetch(srt.transparency_b_tx, texel, 0).xy;
    float2 channel_a = texelFetch(srt.transparency_a_tx, texel, 0).xy;

    /* frag_out.transmittance gets multiplied to the frame-buffer color. */
    frag_out.transmittance.r = channel_r.y;
    frag_out.transmittance.g = channel_g.y;
    frag_out.transmittance.b = channel_b.y;
    frag_out.transmittance.a = channel_a.y;

    /* frag_out.radiance gets added to the frame-buffer color after the transmittance
     * multiplication. */
    frag_out.radiance.r = channel_r.x;
    frag_out.radiance.g = channel_g.x;
    frag_out.radiance.b = channel_b.x;
    frag_out.radiance.a = channel_a.x;
  }

  /* Since Forward.Transparent is additive, overlapping high emissive values can reach the float16
   * limit and end up stored as INF, so here we just clamp it to representable values. */
  const float half_max = 65504.0f;
  frag_out.radiance = min(frag_out.radiance, half_max);

  render_passes.store_color(texel,
                            uni.uniform_buf.render_pass.transparent_id,
                            float4(frag_out.radiance.rgb, frag_out.transmittance.a));
}

}  // namespace eevee::forward

PipelineGraphic eevee_forward_resolve(eevee::forward::fullscreen_vert,
                                      eevee::forward::resolve_frag);
