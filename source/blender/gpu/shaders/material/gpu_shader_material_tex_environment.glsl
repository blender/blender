void node_tex_environment_texco(vec3 viewvec, out vec3 worldvec)
{
#ifdef MESH_SHADER
  worldvec = worldPosition;
#else
  vec4 v = (ProjectionMatrix[3][3] == 0.0) ? vec4(viewvec, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
  vec4 co_homogenous = (ProjectionMatrixInverse * v);

  vec3 co = co_homogenous.xyz / co_homogenous.w;
#  if defined(WORLD_BACKGROUND) || defined(PROBE_CAPTURE)
  worldvec = mat3(ViewMatrixInverse) * co;
#  else
  worldvec = mat3(ModelMatrixInverse) * (mat3(ViewMatrixInverse) * co);
#  endif
#endif
}

void node_tex_environment_equirectangular(vec3 co, out vec3 uv)
{
  vec3 nco = normalize(co);
  uv.x = -atan(nco.y, nco.x) / (2.0 * M_PI) + 0.5;
  uv.y = atan(nco.z, hypot(nco.x, nco.y)) / M_PI + 0.5;
}

void node_tex_environment_mirror_ball(vec3 co, out vec3 uv)
{
  vec3 nco = normalize(co);
  nco.y -= 1.0;

  float div = 2.0 * sqrt(max(-0.5 * nco.y, 0.0));
  nco /= max(1e-8, div);

  uv = 0.5 * nco.xzz + 0.5;
}

void node_tex_environment_empty(vec3 co, out vec4 color)
{
  color = vec4(1.0, 0.0, 1.0, 1.0);
}
