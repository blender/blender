void node_displacement_object(
    float height, float midlevel, float scale, vec3 N, mat4 obmat, out vec3 result)
{
  N = (vec4(N, 0.0) * obmat).xyz;
  result = (height - midlevel) * scale * normalize(N);
  result = (obmat * vec4(result, 0.0)).xyz;
}

void node_displacement_world(float height, float midlevel, float scale, vec3 N, out vec3 result)
{
  result = (height - midlevel) * scale * normalize(N);
}
