
in vec3 pos;
in vec4 nor; /* select flag on the 4th component */

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  bool is_select = (nor.w > 0.0);
  bool is_hidden = (nor.w < 0.0);

  /* Don't draw faces that are selected. */
  if (is_hidden || is_select) {
    gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
  }
  else {
#ifdef USE_WORLD_CLIP_PLANES
    world_clip_planes_calc_clip_distance(world_pos);
#endif
  }
}
