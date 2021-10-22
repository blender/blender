uniform bool useMoire;

uniform float maskOpacity;
uniform float faceSetsOpacity;

in vec3 pos;
in vec3 fset;
in float msk;

out vec4 finalColor;

flat out vec3 faceset_color;
out float mask_color;

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  faceset_color = !useMoire ? mix(vec3(1.0), fset, sqrt(faceSetsOpacity)) : fset;
  mask_color = 1.0 - (msk * maskOpacity);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
