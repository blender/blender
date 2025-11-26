/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Forward lighting evaluation: Lighting is evaluated during the geometry rasterization.
 *
 * This is used by alpha blended materials and materials using Shader to RGB nodes.
 */

#include "infos/eevee_forward_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_transparency_resolve)

#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  int2 texel = int2(gl_FragCoord.xy);

  if (uniform_buf.pipeline.use_monochromatic_transmittance) {
    float4 data = texelFetch(transparency_r_tx, texel, 0);
    out_radiance = float4(data.rgb, 0.0f);
    out_transmittance = float4(data.aaa, 1.0f);
  }
  else {
    /* The data is stored "transposed". */
    float2 channel_r = texelFetch(transparency_r_tx, texel, 0).xy;
    float2 channel_g = texelFetch(transparency_g_tx, texel, 0).xy;
    float2 channel_b = texelFetch(transparency_b_tx, texel, 0).xy;
    float2 channel_a = texelFetch(transparency_a_tx, texel, 0).xy;

    /* out_transmittance gets multiplied to the framebuffer color. */
    out_transmittance.r = channel_r.y;
    out_transmittance.g = channel_g.y;
    out_transmittance.b = channel_b.y;
    out_transmittance.a = channel_a.y;

    /* out_radiance gets added to the framebuffer color after the transmittance multiplication. */
    out_radiance.r = channel_r.x;
    out_radiance.g = channel_g.x;
    out_radiance.b = channel_b.x;
    out_radiance.a = channel_a.x;
  }

  if (uniform_buf.render_pass.transparent_id != -1) {
    imageStore(rp_color_img,
               int3(texel, uniform_buf.render_pass.transparent_id),
               float4(out_radiance.rgb, out_transmittance.a));
  }
}
