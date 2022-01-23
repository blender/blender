#pragma BLENDER_REQUIRE(gpu_shader_cfg_world_clip_lib.glsl)

#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;
#  ifdef USE_WORLD_CLIP_PLANES
uniform mat4 ModelMatrix;
#  endif

in vec3 pos;
in vec4 color;

flat out vec4 finalColor;
#endif

void main()
{
  vec4 pos_4d = vec4(pos, 1.0);
  gl_Position = ModelViewProjectionMatrix * pos_4d;
  finalColor = color;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((clipPlanes.ModelMatrix * pos_4d).xyz);
#endif
}
