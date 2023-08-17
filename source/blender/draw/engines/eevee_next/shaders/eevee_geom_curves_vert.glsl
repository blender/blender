
#pragma BLENDER_REQUIRE(common_hair_lib.glsl) /* TODO rename to curve. */
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_velocity_lib.glsl)

void main()
{
  DRW_VIEW_FROM_RESOURCE_ID;
#ifdef MAT_SHADOW
  shadow_viewport_layer_set(int(drw_view_id), int(viewport_index_buf[drw_view_id]));
#endif

  init_interface();

  bool is_persp = (ProjectionMatrix[3][3] == 0.0);
  hair_get_pos_tan_binor_time(is_persp,
                              ModelMatrixInverse,
                              ViewMatrixInverse[3].xyz,
                              ViewMatrixInverse[2].xyz,
                              interp.P,
                              interp.curves_tangent,
                              interp.curves_binormal,
                              interp.curves_time,
                              interp.curves_thickness,
                              interp.curves_time_width);

  interp.N = cross(interp.curves_tangent, interp.curves_binormal);
  interp_flat.curves_strand_id = hair_get_strand_id();
  interp.barycentric_coords = hair_get_barycentric();
#ifdef MAT_VELOCITY
  /* Due to the screen space nature of the vertex positioning, we compute only the motion of curve
   * strand, not its cylinder. Otherwise we would add the rotation velocity. */
  int vert_idx = hair_get_base_id();
  vec3 prv, nxt;
  vec3 pos = texelFetch(hairPointBuffer, vert_idx).point_position;
  velocity_local_pos_get(pos, vert_idx, prv, nxt);
  /* FIXME(fclem): Evaluating before displacement avoid displacement being treated as motion but
   * ignores motion from animated displacement. Supporting animated displacement motion vectors
   * would require evaluating the node-tree multiple time with different node-tree UBOs evaluated
   * at different times, but also with different attributes (maybe we could assume static attribute
   * at least). */
  velocity_vertex(prv, pos, nxt, motion.prev, motion.next);
#endif

  init_globals();
  attrib_load();

  interp.P += nodetree_displacement();

  gl_Position = point_world_to_ndc(interp.P);
}
