/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_composite_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_composite)
FRAGMENT_SHADER_CREATE_INFO(workbench_resolve_opaque_matcap)
FRAGMENT_SHADER_CREATE_INFO(workbench_resolve_curvature)
FRAGMENT_SHADER_CREATE_INFO(workbench_resolve_shadow)

#include "draw_view_lib.glsl"
#include "workbench_cavity_lib.glsl"
#include "workbench_common_lib.glsl"
#include "workbench_curvature_lib.glsl"
#include "workbench_matcap_lib.glsl"
#include "workbench_world_light_lib.glsl"

void main()
{
  float2 uv = screen_uv;

  float depth = texture(depth_tx, uv).r;
  if (depth == 1.0f) {
    /* Skip the background. */
    gpu_discard_fragment();
    return;
  }

  /* Normal and Incident vector are in view-space. Lighting is evaluated in view-space. */
  float3 P = drw_point_screen_to_view(float3(uv, 0.5f));
  float3 V = drw_view_incident_vector(P);
  float3 N = workbench_normal_decode(texture(normal_tx, uv));
  float4 mat_data = texture(material_tx, uv);

  float3 base_color = mat_data.rgb;
  float4 color = float4(1.0f);

#ifdef WORKBENCH_LIGHTING_MATCAP
  /* When using matcaps, mat_data.a is the back-face sign. */
  N = (mat_data.a > 0.0f) ? N : -N;
  color.rgb = get_matcap_lighting(matcap_tx, base_color, N, V);
#endif

#ifdef WORKBENCH_LIGHTING_STUDIO
  float roughness = 0.0f, metallic = 0.0f;
  workbench_float_pair_decode(mat_data.a, roughness, metallic);
  color.rgb = get_world_lighting(base_color, roughness, metallic, N, V);
#endif

#ifdef WORKBENCH_LIGHTING_FLAT
  color.rgb = base_color;
#endif

#if defined(WORKBENCH_CAVITY) || defined(WORKBENCH_CURVATURE)
  float cavity = 0.0f, edges = 0.0f, curvature = 0.0f;

#  ifdef WORKBENCH_CAVITY
  cavity_compute(uv, depth_tx, normal_tx, cavity, edges);
#  endif

#  ifdef WORKBENCH_CURVATURE
  curvature_compute(uv, object_id_tx, normal_tx, curvature);
#  endif

  float final_cavity_factor = clamp(
      (1.0f - cavity) * (1.0f + edges) * (1.0f + curvature), 0.0f, 4.0f);

  color.rgb *= final_cavity_factor;
#endif

#ifdef WORKBENCH_SHADOW
  bool shadow = texture(stencil_tx, uv).r != 0;
  color.rgb *= get_shadow(N, shadow);
#endif

  frag_color = color;
}
