void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the size to get the
   * coordinates into the sampler's expected [0, 1] range. We choose the maximum between both
   * output sizes because one of the outputs might be a dummy 1x1 image. */
  ivec2 output_size = max(imageSize(output_img), imageSize(mask_img));
  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(output_size);

  vec4 accumulated_color = vec4(0.0);
  for (int i = 0; i < number_of_motion_blur_samples; i++) {
    mat3 homography_matrix = mat3(homography_matrices[i]);

    vec3 transformed_coordinates = homography_matrix * vec3(coordinates, 1.0);
    vec2 projected_coordinates = transformed_coordinates.xy / transformed_coordinates.z;

    /* The derivatives of the projected coordinates with respect to x and y are the first and
     * second columns respectively, divided by the z projection factor as can be shown by
     * differentiating the above matrix multiplication with respect to x and y. */
    vec2 x_gradient = homography_matrix[0].xy / transformed_coordinates.z;
    vec2 y_gradient = homography_matrix[1].xy / transformed_coordinates.z;

    accumulated_color += textureGrad(input_tx, projected_coordinates, x_gradient, y_gradient);
  }

  accumulated_color /= number_of_motion_blur_samples;

  imageStore(output_img, texel, accumulated_color);
  imageStore(mask_img, texel, accumulated_color.aaaa);
}
