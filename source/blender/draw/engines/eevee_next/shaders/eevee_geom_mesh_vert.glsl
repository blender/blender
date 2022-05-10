
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)

void main()
{
  init_interface();

  interp.P = point_object_to_world(pos);
  interp.N = normal_object_to_world(nor);

  init_globals();
  attrib_load();

  interp.P += nodetree_displacement();

  gl_Position = point_world_to_ndc(interp.P);
}
