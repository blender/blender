/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Setup pass: CoC and luma aware downsample to half resolution of the input scene color buffer.
 *
 * An addition to the downsample CoC, we output the maximum slight out of focus CoC to be
 * sure we don't miss a pixel.
 *
 * Input:
 *  Full-resolution color & depth buffer
 * Output:
 *  Half-resolution Color, signed CoC (out_coc.x), and max slight focus abs CoC (out_coc.y).
 */

#include "infos/eevee_depth_of_field_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_depth_of_field_setup)

#include "draw_view_lib.glsl"
#include "eevee_colorspace_lib.glsl"
#include "eevee_depth_of_field_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"

void main()
{
  float2 fullres_texel_size = 1.0f / float2(textureSize(color_tx, 0).xy);
  /* Center uv around the 4 full-resolution pixels. */
  float2 quad_center = float2(gl_GlobalInvocationID.xy * 2 + 1) * fullres_texel_size;

  float4 colors[4];
  float4 cocs;
  for (int i = 0; i < 4; i++) {
    float2 sample_uv = quad_center + quad_offsets[i] * fullres_texel_size;
    float depth = reverse_z::read(textureLod(depth_tx, sample_uv, 0.0f).r);
    /* NOTE: We use samplers without filtering. */
    colors[i] = colorspace_safe_color(textureLod(color_tx, sample_uv, 0.0f));
    cocs[i] = dof_coc_from_depth(dof_buf, sample_uv, depth);
  }

  cocs = clamp(cocs, -dof_buf.coc_abs_max, dof_buf.coc_abs_max);

  float4 weights = dof_bilateral_coc_weights(cocs);
  weights *= dof_bilateral_color_weights(colors);
  /* Normalize so that the sum is 1. */
  weights *= safe_rcp(reduce_add(weights));

  int2 out_texel = int2(gl_GlobalInvocationID.xy);
  float4 out_color = weighted_sum_array(colors, weights);
  imageStore(out_color_img, out_texel, out_color);

  float out_coc = dot(cocs, weights);
  imageStore(out_coc_img, out_texel, float4(out_coc));
}
