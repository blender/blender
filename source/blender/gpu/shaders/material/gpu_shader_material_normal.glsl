void normal_new_shading(vec3 nor, vec3 dir, out vec3 outnor, out float outdot)
{
  outnor = dir;
  outdot = dot(normalize(nor), dir);
}
