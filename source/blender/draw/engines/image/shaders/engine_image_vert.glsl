#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#define SIMA_DRAW_FLAG_DO_REPEAT (1 << 4)

#define DEPTH_IMAGE 0.75

uniform int drawFlags;

in vec3 pos;
out vec2 uvs;

void main()
{
  /* `pos` contains the coordinates of a quad (-1..1). but we need the coordinates of an image
   * plane (0..1) */
  vec3 image_pos = pos * 0.5 + 0.5;

  if ((drawFlags & SIMA_DRAW_FLAG_DO_REPEAT) != 0) {
    gl_Position = vec4(pos.xy, DEPTH_IMAGE, 1.0);
    uvs = point_view_to_object(image_pos).xy;
  }
  else {
    vec3 world_pos = point_object_to_world(image_pos);
    vec4 position = point_world_to_ndc(world_pos);
    /* Move drawn pixels to the front. In the overlay engine the depth is used
     * to detect if a transparency texture or the background color should be drawn.
     * Vertices are between 0.0 and 0.2, Edges between 0.2 and 0.4
     * actual pixels are at 0.75, 1.0 is used for the background. */
    position.z = DEPTH_IMAGE;
    gl_Position = position;
    uvs = world_pos.xy;
  }
}
