
uniform bool useSelect;

in vec3 pos;
in vec4 nor; /* flag stored in w */

flat out vec4 finalColor;

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  bool is_select = (nor.w > 0.0) && useSelect;
  bool is_hidden = (nor.w < 0.0) && useSelect;

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  /* Add offset in Z to avoid zfighting and render selected wires on top. */
  /* TODO scale this bias using znear and zfar range. */
  gl_Position.z -= (is_select ? 2e-4 : 1e-4);

  if (is_hidden) {
    gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
  }

  const vec4 colSel = vec4(1.0);

  finalColor = (is_select) ? colSel : colorWire;

  /* Weight paint needs a light color to contrasts with dark weights. */
  if (!useSelect) {
    finalColor = vec4(1.0, 1.0, 1.0, 0.3);
  }

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
