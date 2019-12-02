
in vec3 pos;
in vec4 nor; /* select flag on the 4th component */

out vec4 finalColor;

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  bool is_select = (nor.w > 0.0);
  bool is_hidden = (nor.w < 0.0);

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  /* Add offset in Z to avoid zfighting and render selected wires on top. */
  /* TODO scale this bias using znear and zfar range. */
  gl_Position.z -= (is_select ? 2e-4 : 1e-4);

  if (is_hidden) {
    gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
  }

  finalColor = (is_select) ? vec4(1.0) : colorWire;
  finalColor.a = nor.w;

  gl_PointSize = sizeVertex * 2.0;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
