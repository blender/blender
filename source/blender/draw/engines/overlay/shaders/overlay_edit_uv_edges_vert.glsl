#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 world_pos = point_object_to_world(vec3(au, 0.0));
  gl_Position = point_world_to_ndc(world_pos);
  /* Snap vertices to the pixel grid to reduce artifacts. */
  vec2 half_viewport_res = sizeViewport * 0.5;
  vec2 half_pixel_offset = sizeViewportInv * 0.5;
  gl_Position.xy = floor(gl_Position.xy * half_viewport_res) / half_viewport_res +
                   half_pixel_offset;

#ifdef USE_EDGE_SELECT
  bool is_select = (flag & int(EDGE_UV_SELECT)) != 0;
#else
  bool is_select = (flag & int(VERT_UV_SELECT)) != 0;
#endif
  geom_in.selectionFac = is_select ? 1.0 : 0.0;
  /* Move selected edges to the top
   * Vertices are between 0.0 and 0.2, Edges between 0.2 and 0.4
   * actual pixels are at 0.75, 1.0 is used for the background. */
  float depth = is_select ? 0.25 : 0.35;
  gl_Position.z = depth;

  /* Avoid precision loss. */
  geom_flat_in.stippleStart = geom_noperspective_in.stipplePos = 500.0 + 500.0 * (gl_Position.xy /
                                                                                  gl_Position.w);
}
