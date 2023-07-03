
#pragma BLENDER_REQUIRE(eevee_cubemap_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_octahedron_lib.glsl)

vec3 light_world_sample(vec3 L, float lod)
{
  vec2 octahedral_uv = octahedral_uv_from_direction(L);
  return textureLod(reflectionProbes, vec3(octahedral_uv, 0.0), lod).rgb;
}
