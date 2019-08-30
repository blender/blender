void camera(vec3 co, out vec3 outview, out float outdepth, out float outdist)
{
  outdepth = abs(co.z);
  outdist = length(co);
  outview = normalize(co);
}
