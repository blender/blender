
#pragma BLENDER_REQUIRE(common_gpencil_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_velocity_lib.glsl)

void main()
{
  init_interface();

  /* TODO(fclem): Expose through a node? */
  vec4 sspos;
  vec2 aspect;
  float strength;
  float hardness;
  vec2 thickness;

  gl_Position = gpencil_vertex(ma,
                               ma1,
                               ma2,
                               ma3,
                               pos,
                               pos1,
                               pos2,
                               pos3,
                               uv1,
                               uv2,
                               col1,
                               col2,
                               fcol1,
                               vec4(drw_view.viewport_size, drw_view.viewport_size_inverse),
                               interp.P,
                               interp.N,
                               g_color,
                               strength,
                               g_uvs,
                               sspos,
                               aspect,
                               thickness,
                               hardness);
#ifdef MAT_VELOCITY
  /* GPencil do not support deformation motion blur. */
  vec3 lP_curr = transform_point(ModelMatrixInverse, interp.P);
  /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
   * ignores motion from animated displacement. Supporting animated displacement motion vectors
   * would require evaluating the nodetree multiple time with different nodetree UBOs evaluated at
   * different times, but also with different attributes (maybe we could assume static attribute at
   * least). */
  velocity_vertex(lP_curr, lP_curr, lP_curr, motion.prev, motion.next);
#endif

  init_globals();
  attrib_load();

  interp.P += nodetree_displacement();
}
