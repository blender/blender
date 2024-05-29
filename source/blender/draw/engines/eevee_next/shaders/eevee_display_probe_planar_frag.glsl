/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_sphere_lib.glsl)

void main()
{
  vec2 uv = gl_FragCoord.xy / vec2(textureSize(planar_radiance_tx, 0).xy);
  float depth = texture(planar_depth_tx, vec3(uv, probe_index)).r;
  if (depth == 1.0) {
    vec3 ndc = drw_screen_to_ndc(vec3(uv, 0.0));
    vec3 wP = drw_point_ndc_to_world(ndc);
    vec3 V = drw_world_incident_vector(wP);
    vec3 R = -reflect(V, probe_normal);

    SphereProbeUvArea world_atlas_coord = reinterpret_as_atlas_coord(world_coord_packed);
    out_color = lightprobe_spheres_sample(R, 0.0, world_atlas_coord);
  }
  else {
    out_color = texture(planar_radiance_tx, vec3(uv, probe_index));
  }
  out_color.a = 0.0;
}
