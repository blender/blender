/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_prepass_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_prepass)
FRAGMENT_SHADER_CREATE_INFO(workbench_transparent_accum)
FRAGMENT_SHADER_CREATE_INFO(workbench_lighting_matcap)

#include "draw_view_lib.glsl"
#include "workbench_common_lib.glsl"
#include "workbench_image_lib.glsl"
#include "workbench_matcap_lib.glsl"
#include "workbench_world_light_lib.glsl"

/* Special function only to be used with calculate_transparent_weight(). */
float linear_zdepth(float depth, float4x4 proj_mat)
{
  if (proj_mat[3][3] == 0.0f) {
    float d = 2.0f * depth - 1.0f;
    return -proj_mat[3][2] / (d + proj_mat[2][2]);
  }
  else {
    /* Return depth from near plane. */
    float z_delta = -2.0f / proj_mat[2][2];
    return depth * z_delta;
  }
}

/* Based on :
 * McGuire and Bavoil, Weighted Blended Order-Independent Transparency, Journal of
 * Computer Graphics Techniques (JCGT), vol. 2, no. 2, 122–141, 2013
 */
float calculate_transparent_weight()
{
  float z = linear_zdepth(gl_FragCoord.z, drw_view().winmat);
#if 0
  /* Eq 10 : Good for surfaces with varying opacity (like particles) */
  float a = min(1.0f, alpha * 10.0f) + 0.01f;
  float b = -gl_FragCoord.z * 0.95f + 1.0f;
  float w = a * a * a * 3e2f * b * b * b;
#else
  /* Eq 7 put more emphasis on surfaces closer to the view. */
  // float w = 10.0f / (1e-5f + pow(abs(z) / 5.0f, 2.0f) + pow(abs(z) / 200.0f, 6.0f)); /* Eq 7 */
  // float w = 10.0f / (1e-5f + pow(abs(z) / 10.0f, 3.0f) + pow(abs(z) / 200.0f, 6.0f)); /* Eq 8 */
  // float w = 10.0f / (1e-5f + pow(abs(z) / 200.0f, 4.0f)); /* Eq 9 */
  /* Same as eq 7, but optimized. */
  float a = abs(z) / 5.0f;
  float b = abs(z) / 200.0f;
  b *= b;
  float w = 10.0f / ((1e-5f + a * a) + b * (b * b)); /* Eq 7 */
#endif
  return clamp(w, 1e-2f, 3e2f);
}

void main()
{
  /* Normal and Incident vector are in view-space. Lighting is evaluated in view-space. */
  float2 uv_viewport = gl_FragCoord.xy * world_data.viewport_size_inv;
  float3 vP = drw_point_screen_to_view(float3(uv_viewport, 0.5f));
  float3 I = drw_view_incident_vector(vP);
  float3 N = normalize(normal_interp);

  float3 color = color_interp;

#ifdef WORKBENCH_COLOR_TEXTURE
  color = workbench_image_color(uv_interp);
#endif

#ifdef WORKBENCH_LIGHTING_MATCAP
  float3 shaded_color = get_matcap_lighting(matcap_tx, color, N, I);
#endif

#ifdef WORKBENCH_LIGHTING_STUDIO
  float3 shaded_color = get_world_lighting(color, _roughness, metallic, N, I);
#endif

#ifdef WORKBENCH_LIGHTING_FLAT
  float3 shaded_color = color;
#endif

  shaded_color *= get_shadow(N, force_shadowing);

  /* Listing 4 */
  float alpha = alpha_interp * world_data.xray_alpha;
  float weight = calculate_transparent_weight() * alpha;
  out_transparent_accum = float4(shaded_color * weight, alpha);
  out_revealage_accum = float4(weight);

  out_object_id = uint(object_id);
}
