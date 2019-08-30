void node_bump(
    float strength, float dist, float height, vec3 N, vec3 surf_pos, float invert, out vec3 result)
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

  float dHdx = dFdx(height);
  float dHdy = dFdy(height);
  vec3 surfgrad = dHdx * Rx + dHdy * Ry;

  strength = max(strength, 0.0);

  result = normalize(abs(det) * N - dist * sign(det) * surfgrad);
  result = normalize(mix(N, result, strength));

  result = mat3(ViewMatrixInverse) * result;
}
