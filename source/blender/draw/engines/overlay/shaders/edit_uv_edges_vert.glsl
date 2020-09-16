#pragma BLENDER_REQUIRE(common_globals_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

in vec3 pos;
in vec2 au;
in int flag;

out float selectionFac;
noperspective out vec2 stipplePos;
flat out vec2 stippleStart;

void main()
{
  vec3 world_pos = point_object_to_world(vec3(au, 0.0));
  gl_Position = point_world_to_ndc(world_pos);
  /* Snap vertices to the pixel grid to reduce artifacts. */
  vec2 half_viewport_res = sizeViewport.xy * 0.5;
  vec2 half_pixel_offset = sizeViewportInv * 0.5;
  gl_Position.xy = floor(gl_Position.xy * half_viewport_res) / half_viewport_res +
                   half_pixel_offset;

  bool is_select = (flag & VERT_UV_SELECT) != 0;
  selectionFac = is_select ? 1.0 : 0.0;
  /* Move selected edges to the top
   * Vertices are between 0.0 and 0.2, Edges between 0.2 and 0.4
   * actual pixels are at 0.75, 1.0 is used for the background. */
  float depth = is_select ? 0.25 : 0.35;
  gl_Position.z = depth;

  /* Avoid precision loss. */
  stippleStart = stipplePos = 500.0 + 500.0 * (gl_Position.xy / gl_Position.w);
}
