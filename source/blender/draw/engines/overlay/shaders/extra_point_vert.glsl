
uniform vec4 color;

in vec3 pos;

out vec4 radii;
out vec4 fillColor;
out vec4 outlineColor;

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  gl_PointSize = sizeObjectCenter;
  float radius = 0.5 * sizeObjectCenter;
  float outline_width = sizePixel;
  radii[0] = radius;
  radii[1] = radius - 1.0;
  radii[2] = radius - outline_width;
  radii[3] = radius - outline_width - 1.0;
  radii /= sizeObjectCenter;

  fillColor = color;
  outlineColor = colorOutline;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
