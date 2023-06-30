#pragma BLENDER_REQUIRE(eevee_cubemap_lib.glsl)

vec3 light_world_sample(vec3 L, float lod)
{
  return textureLod_cubemapArray(reflectionProbes, vec4(L, 0.0), lod).rgb;
}
