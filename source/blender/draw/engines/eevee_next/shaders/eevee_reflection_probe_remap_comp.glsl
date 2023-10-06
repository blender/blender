/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to convert cube-map to octahedral projection. */

#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)

ivec2 probe_area_offset(ReflectionProbeData probe_data, ivec3 texture_size)
{
  ivec2 octahedral_size = ivec2(texture_size.x >> probe_data.layer_subdivision,
                                texture_size.y >> probe_data.layer_subdivision);
  int probes_per_dimension = 1 << probe_data.layer_subdivision;
  ivec2 area_coord = ivec2(probe_data.area_index % probes_per_dimension,
                           probe_data.area_index / probes_per_dimension);
  ivec2 area_offset = area_coord * octahedral_size;
  return area_offset;
}

void main()
{
  ReflectionProbeData probe_data = reflection_probe_buf[reflection_probe_index];

  ivec3 texture_coord = ivec3(gl_GlobalInvocationID.xyz);
  ivec3 texture_size = imageSize(octahedral_img);

  ivec3 octahedral_coord = ivec3(gl_GlobalInvocationID.xyz);
  ivec2 octahedral_size = ivec2(texture_size.x >> probe_data.layer_subdivision,
                                texture_size.y >> probe_data.layer_subdivision);
  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */
  if (any(greaterThanEqual(octahedral_coord.xy, octahedral_size.xy))) {
    return;
  }

  vec2 texel_size = vec2(1.0) / vec2(octahedral_size);

  vec2 uv = vec2(octahedral_coord.xy) / vec2(octahedral_size.xy);
  vec2 octahedral_uv = octahedral_uv_from_layer_texture_coords(uv, probe_data, texel_size);
  vec3 R = octahedral_uv_to_direction(octahedral_uv);

  vec4 col = textureLod(cubemap_tx, R, float(probe_data.layer_subdivision));

  /* Convert transmittance to transparency. */
  col.a = 1.0 - col.a;

  /* Composite world into reflection probes. */
  bool is_reflection_probe = reflection_probe_index != 0;
  if (is_reflection_probe && col.a != 1.0) {
    ReflectionProbeData world_probe_data = reflection_probe_buf[0];
    vec2 world_octahedral_size = vec2(texture_size.x >> world_probe_data.layer_subdivision,
                                      texture_size.y >> world_probe_data.layer_subdivision);
    ivec3 world_octahedral_coord = ivec3(ivec2(uv * world_octahedral_size), 0.0);
    ivec2 world_area_offset = probe_area_offset(world_probe_data, texture_size);
    vec4 world_col = imageLoad(
        octahedral_img, world_octahedral_coord + ivec3(world_area_offset, world_probe_data.layer));
    col.rgb = mix(world_col.rgb, col.rgb, col.a);
  }

  ivec2 area_offset = probe_area_offset(probe_data, texture_size);
  imageStore(octahedral_img, octahedral_coord + ivec3(area_offset, probe_data.layer), col);
}
