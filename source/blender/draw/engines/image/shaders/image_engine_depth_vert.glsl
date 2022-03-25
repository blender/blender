#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 image_pos = vec3(pos, 0.0);
  uv_image = uv;

  vec3 world_pos = point_object_to_world(image_pos);
  vec4 position = point_world_to_ndc(world_pos);
  gl_Position = position;
}
