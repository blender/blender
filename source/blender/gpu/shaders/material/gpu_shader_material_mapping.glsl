void mapping(
    vec3 vec, vec4 m0, vec4 m1, vec4 m2, vec4 m3, vec3 minvec, vec3 maxvec, out vec3 outvec)
{
  mat4 mat = mat4(m0, m1, m2, m3);
  outvec = (mat * vec4(vec, 1.0)).xyz;
  outvec = clamp(outvec, minvec, maxvec);
}
