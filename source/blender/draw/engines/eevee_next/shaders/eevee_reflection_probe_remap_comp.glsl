
/* Shader to convert cubemap to octahedral projection. */

#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)

void main()
{
  ReflectionProbeData probe_data = reflection_probe_buf[0];

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
  // col.xy = octahedral_uv;

  int probes_per_dimension = 1 << probe_data.layer_subdivision;
  ivec2 area_coord = ivec2(probe_data.area_index % probes_per_dimension,
                           probe_data.area_index / probes_per_dimension);
  ivec2 area_offset = area_coord * octahedral_size;

  imageStore(octahedral_img, octahedral_coord + ivec3(area_offset, 0), col);
}
