void node_vector_displacement_tangent(vec4 vector,
                                      float midlevel,
                                      float scale,
                                      vec4 tangent,
                                      vec3 normal,
                                      mat4 obmat,
                                      mat4 viewmat,
                                      out vec3 result)
{
  /* TODO(fclem): this is broken. revisit latter. */
  vec3 N_object = normalize(((vec4(normal, 0.0) * viewmat) * obmat).xyz);
  vec3 T_object = normalize(((vec4(tangent.xyz, 0.0) * viewmat) * obmat).xyz);
  vec3 B_object = tangent.w * normalize(cross(N_object, T_object));

  vec3 offset = (vector.xyz - vec3(midlevel)) * scale;
  result = offset.x * T_object + offset.y * N_object + offset.z * B_object;
  result = (obmat * vec4(result, 0.0)).xyz;
}

void node_vector_displacement_object(
    vec4 vector, float midlevel, float scale, mat4 obmat, out vec3 result)
{
  result = (vector.xyz - vec3(midlevel)) * scale;
  result = (obmat * vec4(result, 0.0)).xyz;
}

void node_vector_displacement_world(vec4 vector, float midlevel, float scale, out vec3 result)
{
  result = (vector.xyz - vec3(midlevel)) * scale;
}
