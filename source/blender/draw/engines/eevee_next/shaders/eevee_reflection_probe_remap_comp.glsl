/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to convert cube-map to octahedral projection. */

#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)

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

  int probes_per_dimension = 1 << probe_data.layer_subdivision;
  ivec2 area_coord = ivec2(probe_data.area_index % probes_per_dimension,
                           probe_data.area_index / probes_per_dimension);
  ivec2 area_offset = area_coord * octahedral_size;

  /* Convert transmittance to transparency. */
  col.a = 1.0 - col.a;

  imageStore(octahedral_img, octahedral_coord + ivec3(area_offset, probe_data.layer), col);
}
