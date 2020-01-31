
in vec3 pos;
in float color;

out vec4 finalColor;

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  finalColor = mix(colorWire, colorVertexSelect, color);

  gl_PointSize = sizeVertex * 2.0;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
