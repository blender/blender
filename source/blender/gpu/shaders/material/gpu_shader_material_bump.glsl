
void differentiate_texco(vec3 v, out vec3 df)
{
  /* Implementation defined. */
  df = v + dF_impl(v);
}

/* Overload for UVs which are loaded as generic attributes. */
void differentiate_texco(vec4 v, out vec3 df)
{
  /* Implementation defined. */
  df = v.xyz + dF_impl(v.xyz);
}

void node_bump(float strength,
               float dist,
               float height,
               vec3 N,
               vec2 height_xy,
               float invert,
               out vec3 result)
{
  N = normalize(N);
  dist *= FrontFacing ? invert : -invert;

#ifdef GPU_FRAGMENT_SHADER
  vec3 dPdx = dFdx(g_data.P);
  vec3 dPdy = dFdy(g_data.P);

  /* Get surface tangents from normal. */
  vec3 Rx = cross(dPdy, N);
  vec3 Ry = cross(N, dPdx);

  /* Compute surface gradient and determinant. */
  float det = dot(dPdx, Rx);

  vec2 dHd = height_xy - vec2(height);
  vec3 surfgrad = dHd.x * Rx + dHd.y * Ry;

  strength = max(strength, 0.0);

  result = normalize(abs(det) * N - dist * sign(det) * surfgrad);
  result = normalize(mix(N, result, strength));
#else
  result = N;
#endif
}
