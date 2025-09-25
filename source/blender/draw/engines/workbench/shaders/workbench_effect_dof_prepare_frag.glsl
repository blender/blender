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
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "workbench_effect_dof_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(workbench_effect_dof_prepare)

/**
 * ----------------- STEP 0 ------------------
 * Custom COC aware down-sampling. Half res pass.
 */

void main()
{
  int4 texel = int4(gl_FragCoord.xyxy) * 2 + int4(0, 0, 1, 1);

  float4 color1 = texelFetch(scene_color_tx, texel.xy, 0);
  float4 color2 = texelFetch(scene_color_tx, texel.zw, 0);
  float4 color3 = texelFetch(scene_color_tx, texel.zy, 0);
  float4 color4 = texelFetch(scene_color_tx, texel.xw, 0);

  float4 depths;
  depths.x = texelFetch(scene_depth_tx, texel.xy, 0).x;
  depths.y = texelFetch(scene_depth_tx, texel.zw, 0).x;
  depths.z = texelFetch(scene_depth_tx, texel.zy, 0).x;
  depths.w = texelFetch(scene_depth_tx, texel.xw, 0).x;

  float4 zdepths = dof_linear_depth(depths);
  float4 cocs_near = dof_calculate_coc(zdepths);
  float4 cocs_far = -cocs_near;

  float coc_near = max(reduce_max(cocs_near), 0.0f);
  float coc_far = max(reduce_max(cocs_far), 0.0f);

  /* now we need to write the near-far fields premultiplied by the coc
   * also use bilateral weighting by each coc values to avoid bleeding. */
  float4 near_weights = step(0.0f, cocs_near) *
                        clamp(1.0f - abs(coc_near - cocs_near), 0.0f, 1.0f);
  float4 far_weights = step(0.0f, cocs_far) * clamp(1.0f - abs(coc_far - cocs_far), 0.0f, 1.0f);

  /* now write output to weighted buffers. */
  /* Take far plane pixels in priority. */
  float4 w = any(notEqual(far_weights, float4(0.0f))) ? far_weights : near_weights;
  halfResColor = weighted_sum(color1, color2, color3, color4, w);
  halfResColor = clamp(halfResColor, 0.0f, 3.0f);

  normalizedCoc = dof_encode_coc(coc_near, coc_far);
}
