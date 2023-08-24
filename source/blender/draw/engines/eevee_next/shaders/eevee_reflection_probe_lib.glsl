/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(eevee_cubemap_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)

vec4 reflection_probes_sample(vec3 L, float lod, ReflectionProbeData probe_data)
{
  vec2 octahedral_uv_packed = octahedral_uv_from_direction(L);
  vec2 texel_size = vec2(1.0 / float(1 << (11 - probe_data.layer_subdivision)));
  vec2 octahedral_uv = octahedral_uv_to_layer_texture_coords(
      octahedral_uv_packed, probe_data, texel_size);
  return textureLod(reflectionProbes, vec3(octahedral_uv, probe_data.layer), lod);
}

vec3 reflection_probes_world_sample(vec3 L, float lod)
{
  ReflectionProbeData probe_data = reflection_probe_buf[0];
  return reflection_probes_sample(L, lod, probe_data).rgb;
}
