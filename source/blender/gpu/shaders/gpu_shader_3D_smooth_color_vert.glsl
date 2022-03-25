#pragma BLENDER_REQUIRE(gpu_shader_cfg_world_clip_lib.glsl)

#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;

#  ifdef USE_WORLD_CLIP_PLANES
uniform mat4 ModelMatrix;
#  endif

in vec3 pos;
in vec4 color;

out vec4 finalColor;
#endif

void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
  finalColor = color;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((clipPlanes.ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
