/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * NPR Evaluation.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_ambient_occlusion_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

#pragma BLENDER_REQUIRE(eevee_deferred_combine_lib.glsl)

vec4 closure_to_rgba(Closure cl)
{
  return vec4(0.0);
}

void main()
{
  init_globals();

  ivec2 texel = ivec2(gl_FragCoord.xy);

  DeferredCombine dc = deferred_combine(texel);
  deferred_combine_clamp(dc);

  g_combined_color = deferred_combine_final_output(dc);
  g_combined_color.a = saturate(1.0 - g_combined_color.a);
  g_diffuse_color = vec4(dc.diffuse_color, 1.0);
  g_diffuse_direct = vec4(dc.diffuse_direct, 1.0);
  g_diffuse_indirect = vec4(dc.diffuse_indirect, 1.0);
  g_specular_color = vec4(dc.specular_color, 1.0);
  g_specular_direct = vec4(dc.specular_direct, 1.0);
  g_specular_indirect = vec4(dc.specular_indirect, 1.0);

  out_color.rgb = nodetree_npr().rgb;
  out_color.a = 1.0 - g_combined_color.a;
}
