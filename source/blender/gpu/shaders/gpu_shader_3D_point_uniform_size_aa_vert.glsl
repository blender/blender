#pragma BLENDER_REQUIRE(gpu_shader_cfg_world_clip_lib.glsl)

#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;
#  ifdef USE_WORLD_CLIP_PLANES
uniform mat4 ModelMatrix;
#  endif
uniform float size;

in vec3 pos;
out vec2 radii;
#endif

void main()
{
  vec4 pos_4d = vec4(pos, 1.0);
  gl_Position = ModelViewProjectionMatrix * pos_4d;
  gl_PointSize = size;

  /* Calculate concentric radii in pixels. */
  float radius = 0.5 * size;

  /* Start at the outside and progress toward the center. */
  radii[0] = radius;
  radii[1] = radius - 1.0;

  /* Convert to PointCoord units. */
  radii /= size;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((clipPlanes.ModelMatrix * pos_4d).xyz);
#endif
}
