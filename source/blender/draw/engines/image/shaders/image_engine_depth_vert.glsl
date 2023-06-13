#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 image_pos = vec3(pos.x, pos.y, 0.0);
  uv_image = uv;

  vec4 position = point_world_to_ndc(image_pos);
  gl_Position = position;
}
