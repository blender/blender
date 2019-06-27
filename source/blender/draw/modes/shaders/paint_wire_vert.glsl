
in vec3 pos;
in vec4 nor; /* flag stored in w */

flat out vec4 finalColor;

void main()
{
#ifdef USE_SELECT
  bool is_select = (nor.w > 0.0);
  bool is_hidden = (nor.w < 0.0);
#else
  bool is_select = false;
  bool is_hidden = false;
#endif
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  /* Add offset in Z to avoid zfighting and render selected wires on top. */
  /* TODO scale this bias using znear and zfar range. */
  gl_Position.z -= (is_select ? 2e-4 : 1e-4);

  if (is_hidden) {
    gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
  }

#ifdef VERTEX_MODE
  vec4 colSel = colorEdgeSelect;
  colSel.rgb = clamp(colSel.rgb - 0.2, 0.0, 1.0);
#else
  const vec4 colSel = vec4(1.0, 1.0, 1.0, 1.0);
#endif

#ifdef USE_SELECT
  finalColor = (is_select) ? colSel : colorWire;
#else
#  ifdef VERTEX_MODE
  finalColor = colorWire;
#  else
  /* Weight paint needs a light color to contrasts with dark weights. */
  finalColor.xyz = vec3(0.8, 0.8, 0.8);
#  endif
#endif

  finalColor.a = nor.w;
  gl_PointSize = sizeVertex * 2.0;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
