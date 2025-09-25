/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 */

#include "infos/workbench_effect_dof_infos.hh"

#include "draw_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "workbench_effect_dof_lib.glsl"

/**
 * ----------------- STEP 4 ------------------
 */

FRAGMENT_SHADER_CREATE_INFO(workbench_effect_dof_resolve)

void main()
{
  /* Full-screen pass. */
  float2 pixel_size = 0.5f / float2(textureSize(half_res_color_tx, 0).xy);
  float2 uv = gl_FragCoord.xy * pixel_size;

  /* TODO: MAKE SURE TO ALIGN SAMPLE POSITION TO AVOID OFFSET IN THE BOKEH. */
  float depth = texelFetch(scene_depth_tx, int2(gl_FragCoord.xy), 0).r;
  float zdepth = dof_linear_depth(depth);
  float coc = dof_calculate_coc(zdepth);

  float blend = smoothstep(1.0f, 3.0f, abs(coc));
  final_colorAdd = texture(half_res_color_tx, uv) * blend;
  final_colorMul = float4(1.0f - blend);
}
