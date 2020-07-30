
vec3 calc_barycentric_distances(vec3 pos0, vec3 pos1, vec3 pos2)
{
  vec3 edge21 = pos2 - pos1;
  vec3 edge10 = pos1 - pos0;
  vec3 edge02 = pos0 - pos2;
  vec3 d21 = normalize(edge21);
  vec3 d10 = normalize(edge10);
  vec3 d02 = normalize(edge02);

  vec3 dists;
  float d = dot(d21, edge02);
  dists.x = sqrt(dot(edge02, edge02) - d * d);
  d = dot(d02, edge10);
  dists.y = sqrt(dot(edge10, edge10) - d * d);
  d = dot(d10, edge21);
  dists.z = sqrt(dot(edge21, edge21) - d * d);
  return dists;
}

vec2 calc_barycentric_co(int vertid)
{
  vec2 bary;
  bary.x = float((vertid % 3) == 0);
  bary.y = float((vertid % 3) == 1);
  return bary;
}

#ifdef HAIR_SHADER

/* Hairs uv and col attributes are passed by bufferTextures. */
#  define DEFINE_ATTR(type, attr) uniform samplerBuffer attr
#  define GET_ATTR(type, attr) hair_get_customdata_##type(attr)

#  define barycentric_get() hair_get_barycentric()
#  define barycentric_resolve(bary) hair_resolve_barycentric(bary)

vec3 orco_get(vec3 local_pos, mat4 modelmatinv, vec4 orco_madd[2], const samplerBuffer orco_samp)
{
  /* TODO: fix ORCO with modifiers. */
  vec3 orco = (modelmatinv * vec4(local_pos, 1.0)).xyz;
  return orco_madd[0].xyz + orco * orco_madd[1].xyz;
}

vec4 tangent_get(const samplerBuffer attr, mat3 normalmat)
{
  /* Unsupported */
  return vec4(0.0);
}

#else /* MESH_SHADER */

#  define DEFINE_ATTR(type, attr) in type attr
#  define GET_ATTR(type, attr) attr

/* Calculated in geom shader later with calc_barycentric_co. */
#  define barycentric_get() vec2(0)
#  define barycentric_resolve(bary) bary

vec3 orco_get(vec3 local_pos, mat4 modelmatinv, vec4 orco_madd[2], vec4 orco)
{
  /* If the object does not have any deformation, the orco layer calculation is done on the fly
   * using the orco_madd factors.
   * We know when there is no orco layer when orco.w is 1.0 because it uses the generic vertex
   * attrib (which is [0,0,0,1]). */
  if (orco.w == 0.0) {
    return orco.xyz * 0.5 + 0.5;
  }
  else {
    return orco_madd[0].xyz + local_pos * orco_madd[1].xyz;
  }
}

vec4 tangent_get(vec4 attr, mat3 normalmat)
{
  vec4 tangent;
  tangent.xyz = normalmat * attr.xyz;
  tangent.w = attr.w;
  float len_sqr = dot(tangent.xyz, tangent.xyz);
  /* Normalize only if vector is not null. */
  if (len_sqr > 0.0) {
    tangent.xyz *= inversesqrt(len_sqr);
  }
  return tangent;
}

#endif
