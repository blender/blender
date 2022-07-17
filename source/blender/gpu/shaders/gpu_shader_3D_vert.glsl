#pragma BLENDER_REQUIRE(gpu_shader_cfg_world_clip_lib.glsl)

void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((clipPlanes.ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
