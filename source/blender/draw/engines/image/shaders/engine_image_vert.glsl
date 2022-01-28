#pragma BLENDER_REQUIRE(common_view_lib.glsl)

in vec2 pos;
in vec2 uv;

/* Normalized screen space uv coordinates. */
out vec2 uv_screen;
out vec2 uv_image;

void main()
{
  vec3 image_pos = vec3(pos, 0.0);
  uv_screen = image_pos.xy;
  uv_image = uv;

  vec3 world_pos = point_object_to_world(image_pos);
  vec4 position = point_world_to_ndc(world_pos);
  gl_Position = position;
}
