/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_reflection_probe_lib.glsl)

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

  out_color = reflection_probes_sample(L, 0, reflection_probe_buf[probe_index].atlas_coord);
  out_color.a = 0.0;
}
