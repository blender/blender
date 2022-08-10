void node_composite_normal(vec3 input_vector,
                           vec3 input_normal,
                           out vec3 result_normal,
                           out float result_dot)
{
  vec3 normal = normalize(input_normal);
  result_normal = normal;
  result_dot = -dot(input_vector, normal);
}
