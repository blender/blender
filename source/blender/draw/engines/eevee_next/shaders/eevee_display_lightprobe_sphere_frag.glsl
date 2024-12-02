/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lightprobe_sphere_info.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_display_lightprobe_sphere)

#include "draw_view_lib.glsl"
#include "eevee_lightprobe_sphere_lib.glsl"

void main()
{
  float dist_sqr = dot(lP, lP);

  /* Discard outside the circle. */
  if (dist_sqr > 1.0) {
    discard;
    return;
  }

  vec3 vN = vec3(lP, sqrt(max(0.0, 1.0 - dist_sqr)));
  vec3 N = drw_normal_view_to_world(vN);
  vec3 V = drw_world_incident_vector(P);
  vec3 L = reflect(-V, N);

  out_color = lightprobe_spheres_sample(L, 0, lightprobe_sphere_buf[probe_index].atlas_coord);
  out_color.a = 0.0;
}
