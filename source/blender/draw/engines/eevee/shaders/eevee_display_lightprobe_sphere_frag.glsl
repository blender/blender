/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lightprobe_sphere_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_display_lightprobe_sphere)

#include "draw_view_lib.glsl"
#include "eevee_lightprobe_sphere_lib.glsl"

void main()
{
  float dist_sqr = dot(lP, lP);

  /* Discard outside the circle. */
  if (dist_sqr > 1.0f) {
    gpu_discard_fragment();
    return;
  }

  float3 vN = float3(lP, sqrt(max(0.0f, 1.0f - dist_sqr)));
  float3 N = drw_normal_view_to_world(vN);
  float3 V = drw_world_incident_vector(P);
  float3 L = reflect(-V, N);

  out_color = lightprobe_spheres_sample(L, 0, lightprobe_sphere_buf[probe_index].atlas_coord);
  out_color.a = 0.0f;
}
