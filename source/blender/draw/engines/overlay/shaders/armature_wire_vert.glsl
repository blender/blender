
in vec3 color;
in vec3 pos;

flat out vec4 finalColor;
flat out vec2 edgeStart;
noperspective out vec2 edgePos;

void main()
{
  finalColor.rgb = color;
  finalColor.a = 1.0;

  vec3 worldPosition = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(worldPosition);

  edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(worldPosition);
#endif
}
