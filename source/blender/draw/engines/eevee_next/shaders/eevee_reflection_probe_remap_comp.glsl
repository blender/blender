/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Shader to convert cube-map to octahedral projection. */

#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)

ivec2 probe_area_offset(ReflectionProbeAtlasCoordinate atlas_coord, ivec3 texture_size)
{
  ivec2 octahedral_size = ivec2(texture_size.x >> atlas_coord.layer_subdivision,
                                texture_size.y >> atlas_coord.layer_subdivision);
  int probes_per_dimension = 1 << atlas_coord.layer_subdivision;
  ivec2 area_coord = ivec2(atlas_coord.area_index % probes_per_dimension,
                           atlas_coord.area_index / probes_per_dimension);
  ivec2 area_offset = area_coord * octahedral_size;
  return area_offset;
}

void main()
{
  ReflectionProbeAtlasCoordinate probe_coord = reinterpret_as_atlas_coord(probe_coord_packed);
  ReflectionProbeAtlasCoordinate world_coord = reinterpret_as_atlas_coord(world_coord_packed);

  ivec3 texture_coord = ivec3(gl_GlobalInvocationID.xyz);
  ivec3 texture_size = imageSize(octahedral_img);

  ivec3 octahedral_coord = ivec3(gl_GlobalInvocationID.xyz);
  ivec2 octahedral_size = texture_size.xy >> probe_coord.layer_subdivision;
  /* Exit when pixel being written doesn't fit in the area reserved for the probe. */
  if (any(greaterThanEqual(octahedral_coord.xy, octahedral_size.xy))) {
    return;
  }

  vec2 texel_size = vec2(1.0) / vec2(octahedral_size);

  vec2 uv = vec2(octahedral_coord.xy) / vec2(octahedral_size.xy);
  vec2 octahedral_uv = octahedral_uv_from_layer_texture_coords(uv, texel_size);
  vec3 R = octahedral_uv_to_direction(octahedral_uv);

  vec4 col = textureLod(cubemap_tx, R, float(probe_coord.layer_subdivision));

  /* Convert transmittance to transparency. */
  col.a = 1.0 - col.a;

  /* Composite world into reflection probes. */
  bool is_world = all(equal(probe_coord_packed, world_coord_packed));
  if (!is_world && col.a != 1.0) {
    vec2 world_octahedral_size = vec2(texture_size.x >> world_coord.layer_subdivision,
                                      texture_size.y >> world_coord.layer_subdivision);
    ivec3 world_octahedral_coord = ivec3(ivec2(uv * world_octahedral_size), 0.0);
    ivec2 world_area_offset = probe_area_offset(world_coord, texture_size);
    vec4 world_col = imageLoad(
        octahedral_img, world_octahedral_coord + ivec3(world_area_offset, world_coord.layer));
    col.rgb = mix(world_col.rgb, col.rgb, col.a);
  }

  ivec2 area_offset = probe_area_offset(probe_coord, texture_size);
  imageStore(octahedral_img, octahedral_coord + ivec3(area_offset, probe_coord.layer), col);
}
