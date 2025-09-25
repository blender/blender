/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 */

#include "infos/workbench_effect_dof_infos.hh"

#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "workbench_effect_dof_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(workbench_effect_dof_downsample)

/**
 * ----------------- STEP 0.5 ------------------
 * Custom COC aware down-sampling. Quarter res pass.
 */

void main()
{
  float4 texel = float4(gl_FragCoord.xyxy) * 2.0f + float4(0.0f, 0.0f, 1.0f, 1.0f);
  texel = (texel - 0.5f) / float4(textureSize(scene_color_tx, 0).xyxy);

  /* Using texelFetch can bypass the mip range setting on some platform.
   * Using texture LOD fixes this issue. Note that we need to disable filtering to get the right
   * texel values. */
  float4 color1 = textureLod(scene_color_tx, texel.xy, 0.0f);
  float4 color2 = textureLod(scene_color_tx, texel.zw, 0.0f);
  float4 color3 = textureLod(scene_color_tx, texel.zy, 0.0f);
  float4 color4 = textureLod(scene_color_tx, texel.xw, 0.0f);

  float2 cocs1 = textureLod(input_coc_tx, texel.xy, 0.0f).rg;
  float2 cocs2 = textureLod(input_coc_tx, texel.zw, 0.0f).rg;
  float2 cocs3 = textureLod(input_coc_tx, texel.zy, 0.0f).rg;
  float2 cocs4 = textureLod(input_coc_tx, texel.xw, 0.0f).rg;

  float4 cocs_near = float4(cocs1.r, cocs2.r, cocs3.r, cocs4.r) * MAX_COC_SIZE;
  float4 cocs_far = float4(cocs1.g, cocs2.g, cocs3.g, cocs4.g) * MAX_COC_SIZE;

  float coc_near = reduce_max(cocs_near);
  float coc_far = reduce_max(cocs_far);

  /* Now we need to write the near-far fields pre-multiplied by the COC
   * also use bilateral weighting by each COC values to avoid bleeding. */
  float4 near_weights = step(0.0f, cocs_near) *
                        clamp(1.0f - abs(coc_near - cocs_near), 0.0f, 1.0f);
  float4 far_weights = step(0.0f, cocs_far) * clamp(1.0f - abs(coc_far - cocs_far), 0.0f, 1.0f);

  /* now write output to weighted buffers. */
  float4 w = any(notEqual(far_weights, float4(0.0f))) ? far_weights : near_weights;
  outColor = weighted_sum(color1, color2, color3, color4, w);

  outCocs = dof_encode_coc(coc_near, coc_far);
}
