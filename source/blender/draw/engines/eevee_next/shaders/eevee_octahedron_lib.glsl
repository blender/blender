/**
 * Convert from a cubemap vector to an octahedron UV coordinate.
 */
vec2 octahedral_uv_from_direction(vec3 co)
{
  /* Projection onto octahedron. */
  co /= dot(vec3(1.0), abs(co));

  /* Out-folding of the downward faces. */
  if (co.z < 0.0) {
    vec2 sign = step(0.0, co.xy) * 2.0 - 1.0;
    co.xy = (1.0 - abs(co.yx)) * sign;
  }

  /* Mapping to [0;1]^2 texture space. */
  vec2 uvs = co.xy * (0.5) + 0.5;

  return uvs;
}

vec3 octahedral_uv_to_direction(vec2 co)
{
  co = co * 2.0 - 1.0;

  vec2 abs_co = abs(co);
  vec3 v = vec3(co, 1.0 - (abs_co.x + abs_co.y));

  if (abs_co.x + abs_co.y > 1.0) {
    v.xy = (abs(co.yx) - 1.0) * -sign(co.xy);
  }

  return v;
}
