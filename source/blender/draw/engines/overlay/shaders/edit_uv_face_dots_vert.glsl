#pragma BLENDER_REQUIRE(common_globals_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

uniform float pointSize;

in vec2 au;
in int flag;

flat out vec4 finalColor;

void main()
{
  vec3 world_pos = point_object_to_world(vec3(au, 0.0));
  gl_Position = point_world_to_ndc(world_pos);

  finalColor = ((flag & FACE_UV_SELECT) != 0) ? colorVertexSelect : vec4(colorWire.rgb, 1.0);
  gl_PointSize = pointSize;
}
