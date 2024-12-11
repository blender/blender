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

FRAGMENT_SHADER_CREATE_INFO(workbench_effect_dof_downsample)

/**
 * ----------------- STEP 0.5 ------------------
 * Custom COC aware down-sampling. Quarter res pass.
 */

void main()
{
  vec4 texel = vec4(gl_FragCoord.xyxy) * 2.0 + vec4(0.0, 0.0, 1.0, 1.0);
  texel = (texel - 0.5) / vec4(textureSize(sceneColorTex, 0).xyxy);

  /* Using texelFetch can bypass the mip range setting on some platform.
   * Using texture LOD fixes this issue. Note that we need to disable filtering to get the right
   * texel values. */
  vec4 color1 = textureLod(sceneColorTex, texel.xy, 0.0);
  vec4 color2 = textureLod(sceneColorTex, texel.zw, 0.0);
  vec4 color3 = textureLod(sceneColorTex, texel.zy, 0.0);
  vec4 color4 = textureLod(sceneColorTex, texel.xw, 0.0);

  vec2 cocs1 = textureLod(inputCocTex, texel.xy, 0.0).rg;
  vec2 cocs2 = textureLod(inputCocTex, texel.zw, 0.0).rg;
  vec2 cocs3 = textureLod(inputCocTex, texel.zy, 0.0).rg;
  vec2 cocs4 = textureLod(inputCocTex, texel.xw, 0.0).rg;

  vec4 cocs_near = vec4(cocs1.r, cocs2.r, cocs3.r, cocs4.r) * MAX_COC_SIZE;
  vec4 cocs_far = vec4(cocs1.g, cocs2.g, cocs3.g, cocs4.g) * MAX_COC_SIZE;

  float coc_near = reduce_max(cocs_near);
  float coc_far = reduce_max(cocs_far);

  /* Now we need to write the near-far fields pre-multiplied by the COC
   * also use bilateral weighting by each COC values to avoid bleeding. */
  vec4 near_weights = step(0.0, cocs_near) * clamp(1.0 - abs(coc_near - cocs_near), 0.0, 1.0);
  vec4 far_weights = step(0.0, cocs_far) * clamp(1.0 - abs(coc_far - cocs_far), 0.0, 1.0);

  /* now write output to weighted buffers. */
  vec4 w = any(notEqual(far_weights, vec4(0.0))) ? far_weights : near_weights;
  outColor = weighted_sum(color1, color2, color3, color4, w);

  outCocs = dof_encode_coc(coc_near, coc_far);
}
