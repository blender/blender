
in vec3 pos;

out vec4 finalColor;

void main()
{
  /* Extract data packed inside the unused mat4 members. */
  mat4 obmat = ModelMatrix;
  finalColor = vec4(obmat[0][3], obmat[1][3], obmat[2][3], obmat[3][3]);

  vec3 world_pos = (ModelMatrix * vec4(pos, 1.0)).xyz;
  gl_Position = point_world_to_ndc(world_pos);

  gl_PointSize = sizeVertex * 2.0;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
