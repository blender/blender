
uniform float sizeVertex;

in vec3 pos;

#ifndef UNIFORM_ID
uniform int offset;
in uint color;

flat out uint id;
#endif

void main()
{
#ifndef UNIFORM_ID
  id = floatBitsToUint(intBitsToFloat(offset)) + color;
#endif

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  gl_PointSize = sizeVertex;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
