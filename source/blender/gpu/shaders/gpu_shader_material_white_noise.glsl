/* White Noise */

void node_white_noise_1d(vec3 vector, float w, out float value)
{
  value = hash_float_to_float(w);
}

void node_white_noise_2d(vec3 vector, float w, out float value)
{
  value = hash_vec2_to_float(vector.xy);
}

void node_white_noise_3d(vec3 vector, float w, out float value)
{
  value = hash_vec3_to_float(vector);
}

void node_white_noise_4d(vec3 vector, float w, out float value)
{
  value = hash_vec4_to_float(vec4(vector, w));
}
