
uniform mat4 ModelViewProjectionMatrix;
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
  v1 = ModelViewProjectionMatrix * vec4(pos, 1.0);
  vec3 n = normalize(transform_normal_object_to_view(nor));
  v2 = v1 + ProjectionMatrix * vec4(n * normalSize, 0.0);
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
