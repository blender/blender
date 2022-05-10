
#pragma BLENDER_REQUIRE(common_gpencil_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)

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

  init_globals();
  attrib_load();

  interp.P += nodetree_displacement();
}
