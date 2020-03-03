
in vec3 pos;

/* Instance */
in vec3 inst_pos;

flat out vec4 finalColor;
flat out vec2 edgeStart;
noperspective out vec2 edgePos;

void main()
{
  finalColor = colorLight;

  /* Relative to DPI scaling. Have constant screen size. */
  vec3 screen_pos = screenVecs[0].xyz * pos.x + screenVecs[1].xyz * pos.y;
  vec3 p = inst_pos;
  p.z *= (pos.z == 0.0) ? 0.0 : 1.0;
  float screen_size = mul_project_m4_v3_zfac(p) * sizePixel;
  vec3 world_pos = p + screen_pos * screen_size;

  gl_Position = point_world_to_ndc(world_pos);

  /* Convert to screen position [0..sizeVp]. */
  edgePos = edgeStart = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
