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

FRAGMENT_SHADER_CREATE_INFO(workbench_effect_dof_prepare)

/**
 * ----------------- STEP 0 ------------------
 * Custom COC aware down-sampling. Half res pass.
 */

void main()
{
  ivec4 texel = ivec4(gl_FragCoord.xyxy) * 2 + ivec4(0, 0, 1, 1);

  vec4 color1 = texelFetch(sceneColorTex, texel.xy, 0);
  vec4 color2 = texelFetch(sceneColorTex, texel.zw, 0);
  vec4 color3 = texelFetch(sceneColorTex, texel.zy, 0);
  vec4 color4 = texelFetch(sceneColorTex, texel.xw, 0);

  vec4 depths;
  depths.x = texelFetch(sceneDepthTex, texel.xy, 0).x;
  depths.y = texelFetch(sceneDepthTex, texel.zw, 0).x;
  depths.z = texelFetch(sceneDepthTex, texel.zy, 0).x;
  depths.w = texelFetch(sceneDepthTex, texel.xw, 0).x;

  vec4 zdepths = dof_linear_depth(depths);
  vec4 cocs_near = dof_calculate_coc(zdepths);
  vec4 cocs_far = -cocs_near;

  float coc_near = max(reduce_max(cocs_near), 0.0);
  float coc_far = max(reduce_max(cocs_far), 0.0);

  /* now we need to write the near-far fields premultiplied by the coc
   * also use bilateral weighting by each coc values to avoid bleeding. */
  vec4 near_weights = step(0.0, cocs_near) * clamp(1.0 - abs(coc_near - cocs_near), 0.0, 1.0);
  vec4 far_weights = step(0.0, cocs_far) * clamp(1.0 - abs(coc_far - cocs_far), 0.0, 1.0);

  /* now write output to weighted buffers. */
  /* Take far plane pixels in priority. */
  vec4 w = any(notEqual(far_weights, vec4(0.0))) ? far_weights : near_weights;
  halfResColor = weighted_sum(color1, color2, color3, color4, w);
  halfResColor = clamp(halfResColor, 0.0, 3.0);

  normalizedCoc = dof_encode_coc(coc_near, coc_far);
}
