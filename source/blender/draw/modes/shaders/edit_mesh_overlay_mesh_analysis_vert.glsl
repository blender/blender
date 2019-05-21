
in vec3 pos;
in vec4 weight_color;

#ifdef FACE_COLOR
flat out vec4 weightColor;
#endif

#ifdef VERTEX_COLOR
out vec4 weightColor;
#endif

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  weightColor = vec4(weight_color.rgb, 1.0);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
