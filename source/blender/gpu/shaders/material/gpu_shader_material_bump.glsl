void dfdx_v3(vec3 v, out vec3 dy)
{
  dy = v + DFDX_SIGN * dFdx(v);
}

void dfdy_v3(vec3 v, out vec3 dy)
{
  dy = v + DFDY_SIGN * dFdy(v);
}

void node_bump(float strength,
               float dist,
               float height,
               float height_dx,
               float height_dy,
               vec3 N,
               vec3 surf_pos,
               float invert,
               out vec3 result)
{
  N = mat3(ViewMatrix) * normalize(N);
  dist *= gl_FrontFacing ? invert : -invert;

  vec3 dPdx = dFdx(surf_pos);
  vec3 dPdy = dFdy(surf_pos);

  /* Get surface tangents from normal. */
  vec3 Rx = cross(dPdy, N);
  vec3 Ry = cross(N, dPdx);

  /* Compute surface gradient and determinant. */
  float det = dot(dPdx, Rx);

  float dHdx = height_dx - height;
  float dHdy = height_dy - height;
  vec3 surfgrad = dHdx * Rx + dHdy * Ry;

  strength = max(strength, 0.0);

  result = normalize(abs(det) * N - dist * sign(det) * surfgrad);
  result = normalize(mix(N, result, strength));

  result = mat3(ViewMatrixInverse) * result;
}
