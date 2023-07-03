
/* Shader to convert cubemap to octahedral projection. */

#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)

void main()
{
  ivec3 octahedral_coord = ivec3(gl_GlobalInvocationID.xyz);
  ivec3 octahedral_size = imageSize(octahedral_img);
  /* Group doesn't fit in output texture. */
  if (any(greaterThanEqual(octahedral_coord.xy, octahedral_size.xy))) {
    return;
  }
  vec2 octahedral_uv = vec2(octahedral_coord.xy) / vec2(octahedral_size.xy);
  vec3 R = octahedral_uv_to_direction(octahedral_uv);

  vec4 col = textureLod(cubemap_tx, R, 0.0);
  imageStore(octahedral_img, octahedral_coord, col);
}