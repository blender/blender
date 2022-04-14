void camera(out vec3 outview, out float outdepth, out float outdist)
{
  outdepth = abs(transform_point(ViewMatrix, g_data.P).z);
  outdist = distance(g_data.P, cameraPos);
  outview = normalize(g_data.P - cameraPos);
}
