#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 input_size = texture_size(input_tx);

  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(input_size);

  vec2 vector_to_source = source - coordinates;
  float distance_to_source = length(vector_to_source);
  vec2 direction_to_source = vector_to_source / distance_to_source;

  /* We integrate from the current pixel to the source pixel, but up until the user specified
   * maximum ray length. The number of integration steps is roughly equivalent to the number of
   * pixels along the integration path. Assume a minimum number of steps of 1 to avoid zero
   * division handling and return source pixels as is. */
  float integration_length = min(distance_to_source, max_ray_length);
  float integration_length_in_pixels = length(vec2(input_size)) * integration_length;
  int steps = max(1, int(integration_length_in_pixels));
  vec2 step_vector = (direction_to_source * integration_length) / steps;

  vec4 accumulated_color = vec4(0.0);
  for (int i = 0; i < steps; i++) {
    /* Attenuate the contributions of pixels that are further away from the source using a
     * quadratic falloff. */
    float weight = pow(1.0f - i / integration_length_in_pixels, 2.0);
    accumulated_color += texture(input_tx, coordinates + i * step_vector) * weight;
  }

  imageStore(output_img, texel, accumulated_color / steps);
}
