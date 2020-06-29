#ifndef VOLUMETRICS
void node_wireframe(float size, vec2 barycentric, vec3 barycentric_dist, out float fac)
{
  vec3 barys = barycentric.xyy;
  barys.z = 1.0 - barycentric.x - barycentric.y;

  size *= 0.5;
  vec3 s = step(-size, -barys * barycentric_dist);

  fac = max(s.x, max(s.y, s.z));
}

void node_wireframe_screenspace(float size, vec2 barycentric, out float fac)
{
  vec3 barys = barycentric.xyy;
  barys.z = 1.0 - barycentric.x - barycentric.y;

  size *= (1.0 / 3.0);
  vec3 dx = dFdx(barys);
  vec3 dy = dFdy(barys);
  vec3 deltas = sqrt(dx * dx + dy * dy);

  vec3 s = step(-deltas * size, -barys);

  fac = max(s.x, max(s.y, s.z));
}
#else
/* Stub wireframe because it is not compatible with volumetrics. */
#  define node_wireframe(a, b, c, d) (d = 0.0)
#  define node_wireframe_screenspace(a, b, c) (c = 0.0)
#endif
