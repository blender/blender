
uniform mat4 ModelMatrix;

in vec3 pos;
in vec3 c; /* active color */

out vec3 finalColor;

vec3 srgb_to_linear_attr(vec3 c)
{
  c = max(c, vec3(0.0));
  vec3 c1 = c * (1.0 / 12.92);
  vec3 c2 = pow((c + 0.055) * (1.0 / 1.055), vec3(2.4));
  return mix(c1, c2, step(vec3(0.04045), c));
}

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  finalColor = srgb_to_linear_attr(c);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
