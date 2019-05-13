
in float weight;
in vec3 pos;

out vec2 weight_interp; /* (weight, alert) */

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  /* Separate actual weight and alerts for independent interpolation */
  weight_interp = max(vec2(weight, -weight), 0.0);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
