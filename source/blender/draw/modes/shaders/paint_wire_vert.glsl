
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
  finalColor.a = nor.w;
#else
#  ifdef VERTEX_MODE
  finalColor.xyz = colorWire.xyz;
  finalColor.a = 1.0;
#  else
  /* Weight paint needs a light color to contrasts with dark weights. */
  finalColor = vec4(1, 1, 1, 0.2);
#  endif
#endif

  /* Needed for Radeon (TM) RX 480 Graphics. */
#if defined(GPU_ATI)
  gl_PointSize = sizeVertex * 2.0;
#endif

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
