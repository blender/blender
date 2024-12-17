/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 */

#include "infos/workbench_effect_dof_info.hh"

#include "draw_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "workbench_effect_dof_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(workbench_effect_dof_blur1)

/**
 * ----------------- STEP 2 ------------------
 * Blur vertically and diagonally.
 * Outputs vertical blur and combined blur in MRT
 */

vec2 get_random_vector(float offset)
{
  /* Interleaved gradient noise by Jorge Jimenez
   * http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare */
  float ign = fract(offset +
                    52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
  float bn = texelFetch(noiseTex, ivec2(gl_FragCoord.xy) % 64, 0).a;
  float ang = M_PI * 2.0 * fract(bn + offset);
  return vec2(cos(ang), sin(ang)) * sqrt(ign);
  // return noise.rg * sqrt(ign);
}

void main()
{
  vec2 uv = gl_FragCoord.xy * invertedViewportSize * 2.0;

  vec2 size = vec2(textureSize(halfResColorTex, 0).xy);
  ivec2 texel = ivec2(uv * size);

  vec4 color = vec4(0.0);
  float tot = 0.0;

  float coc = dof_decode_coc(texelFetch(inputCocTex, texel, 0).rg);
  float max_radius = coc;
  vec2 noise = get_random_vector(noiseOffset) * 0.2 * clamp(max_radius * 0.2 - 4.0, 0.0, 1.0);
  for (int i = 0; i < NUM_SAMPLES; i++) {
    vec2 tc = uv + (noise + samples[i].xy) * invertedViewportSize * max_radius;

    /* decode_signed_coc return biggest coc. */
    coc = abs(dof_decode_signed_coc(texture(inputCocTex, tc).rg));

    float lod = log2(clamp((coc + min(coc, max_radius)) * 0.5 - 21.0, 0.0, 16.0) * 0.25);
    vec4 samp = textureLod(halfResColorTex, tc, lod);

    float radius = samples[i].z * max_radius;
    float weight = abs(coc) * smoothstep(radius - 0.5, radius + 0.5, abs(coc));

    color += samp * weight;
    tot += weight;
  }

  if (tot > 0.0) {
    blurColor = color / tot;
  }
  else {
    blurColor = textureLod(halfResColorTex, uv, 0.0);
  }
}
