/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 */

#include "infos/workbench_effect_dof_infos.hh"

#include "gpu_shader_math_constants_lib.glsl"
#include "workbench_effect_dof_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(workbench_effect_dof_blur1)

/**
 * ----------------- STEP 2 ------------------
 * Blur vertically and diagonally.
 * Outputs vertical blur and combined blur in MRT
 */

float2 get_random_vector(float offset)
{
  /* Interleaved gradient noise by Jorge Jimenez
   * http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare */
  float ign = fract(
      offset + 52.9829189f * fract(0.06711056f * gl_FragCoord.x + 0.00583715f * gl_FragCoord.y));
  float bn = texelFetch(noise_tx, int2(gl_FragCoord.xy) % 64, 0).a;
  float ang = M_PI * 2.0f * fract(bn + offset);
  return float2(cos(ang), sin(ang)) * sqrt(ign);
  // return noise.rg * sqrt(ign);
}

void main()
{
  float2 uv = gl_FragCoord.xy * inverted_viewport_size * 2.0f;

  float2 size = float2(textureSize(half_res_color_tx, 0).xy);
  int2 texel = int2(uv * size);

  float4 color = float4(0.0f);
  float tot = 0.0f;

  float coc = dof_decode_coc(texelFetch(input_coc_tx, texel, 0).rg);
  float max_radius = coc;
  float2 noise = get_random_vector(noise_offset) * 0.2f *
                 clamp(max_radius * 0.2f - 4.0f, 0.0f, 1.0f);
  for (int i = 0; i < NUM_SAMPLES; i++) {
    float2 tc = uv + (noise + samples[i].xy) * inverted_viewport_size * max_radius;

    /* decode_signed_coc return biggest coc. */
    coc = abs(dof_decode_signed_coc(texture(input_coc_tx, tc).rg));

    float lod = log2(clamp((coc + min(coc, max_radius)) * 0.5f - 21.0f, 0.0f, 16.0f) * 0.25f);
    float4 samp = textureLod(half_res_color_tx, tc, lod);

    float radius = samples[i].z * max_radius;
    float weight = abs(coc) * smoothstep(radius - 0.5f, radius + 0.5f, abs(coc));

    color += samp * weight;
    tot += weight;
  }

  if (tot > 0.0f) {
    blurColor = color / tot;
  }
  else {
    blurColor = textureLod(half_res_color_tx, uv, 0.0f);
  }
}
