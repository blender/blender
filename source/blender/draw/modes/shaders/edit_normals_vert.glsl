
uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;

uniform float normalSize;

in vec3 pos;

#ifdef LOOP_NORMALS
in vec3 lnor;
#  define nor lnor

#elif defined(FACE_NORMALS)
in vec4 norAndFlag;
#  define nor norAndFlag.xyz
#else

in vec3 vnor;
#  define nor vnor
#endif

flat out vec4 v1;
flat out vec4 v2;

void main()
{
  vec3 n = normalize(normal_object_to_world(nor));

  vec3 world_pos = point_object_to_world(pos);

  v1 = point_world_to_ndc(world_pos);
  v2 = point_world_to_ndc(world_pos + n * normalSize);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
