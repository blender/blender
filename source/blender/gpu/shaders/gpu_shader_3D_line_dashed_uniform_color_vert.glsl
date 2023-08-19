/*
 * Vertex Shader for dashed lines with 3D coordinates,
 * with uniform multi-colors or uniform single-color, and unary thickness.
 *
 * Dashed is performed in screen space.
 */

#pragma BLENDER_REQUIRE(gpu_shader_cfg_world_clip_lib.glsl)

void main()
{
  vec4 pos_4d = vec4(pos, 1.0);
  gl_Position = ModelViewProjectionMatrix * pos_4d;
  stipple_start = stipple_pos = viewport_size * 0.5 * (gl_Position.xy / gl_Position.w);
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((ModelMatrix * pos_4d).xyz);
#endif
}
