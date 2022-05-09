
#pragma BLENDER_REQUIRE(common_hair_lib.glsl) /* TODO rename to curve. */
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)

void main()
{
  init_interface();

  vec3 T;

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

  interp.N = cross(T, interp.curves_binormal);
  interp.curves_strand_id = hair_get_strand_id();
  interp.barycentric_coords = hair_get_barycentric();

  init_globals();
  attrib_load();

  interp.P += nodetree_displacement();

  gl_Position = point_world_to_ndc(interp.P);
}
