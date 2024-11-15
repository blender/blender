/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_composite_info.hh"

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
  vec2 uv = uvcoordsvar.xy;

  float depth = texture(depth_tx, uv).r;
  if (depth == 1.0) {
    /* Skip the background. */
    discard;
    return;
  }

  /* Normal and Incident vector are in view-space. Lighting is evaluated in view-space. */
  vec3 P = drw_point_screen_to_view(vec3(uv, 0.5));
  vec3 V = drw_view_incident_vector(P);
  vec3 N = workbench_normal_decode(texture(normal_tx, uv));
  vec4 mat_data = texture(material_tx, uv);

  vec3 base_color = mat_data.rgb;
  vec4 color = vec4(1.0);

#ifdef WORKBENCH_LIGHTING_MATCAP
  /* When using matcaps, mat_data.a is the back-face sign. */
  N = (mat_data.a > 0.0) ? N : -N;
  color.rgb = get_matcap_lighting(matcap_tx, base_color, N, V);
#endif

#ifdef WORKBENCH_LIGHTING_STUDIO
  float roughness = 0.0, metallic = 0.0;
  workbench_float_pair_decode(mat_data.a, roughness, metallic);
  color.rgb = get_world_lighting(base_color, roughness, metallic, N, V);
#endif

#ifdef WORKBENCH_LIGHTING_FLAT
  color.rgb = base_color;
#endif

#if defined(WORKBENCH_CAVITY) || defined(WORKBENCH_CURVATURE)
  float cavity = 0.0, edges = 0.0, curvature = 0.0;

#  ifdef WORKBENCH_CAVITY
  cavity_compute(uv, depth_tx, normal_tx, cavity, edges);
#  endif

#  ifdef WORKBENCH_CURVATURE
  curvature_compute(uv, object_id_tx, normal_tx, curvature);
#  endif

  float final_cavity_factor = clamp((1.0 - cavity) * (1.0 + edges) * (1.0 + curvature), 0.0, 4.0);

  color.rgb *= final_cavity_factor;
#endif

#ifdef WORKBENCH_SHADOW
  bool shadow = texture(stencil_tx, uv).r != 0;
  color.rgb *= get_shadow(N, shadow);
#endif

  fragColor = color;
}
