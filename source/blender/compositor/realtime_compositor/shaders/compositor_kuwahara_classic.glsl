#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_summed_area_table_lib.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 mean_of_squared_color_of_quadrants[4] = vec4[](vec4(0.0), vec4(0.0), vec4(0.0), vec4(0.0));
  vec4 mean_of_color_of_quadrants[4] = vec4[](vec4(0.0), vec4(0.0), vec4(0.0), vec4(0.0));

  /* Compute the above statistics for each of the quadrants around the current pixel. */
  for (int q = 0; q < 4; q++) {
    /* A fancy expression to compute the sign of the quadrant q. */
    ivec2 sign = ivec2((q % 2) * 2 - 1, ((q / 2) * 2 - 1));

    ivec2 lower_bound = texel - ivec2(sign.x > 0 ? 0 : radius, sign.y > 0 ? 0 : radius);
    ivec2 upper_bound = texel + ivec2(sign.x < 0 ? 0 : radius, sign.y < 0 ? 0 : radius);

    /* Limit the quadrants to the image bounds. */
    ivec2 image_bound = imageSize(output_img) - ivec2(1);
    ivec2 corrected_lower_bound = min(image_bound, max(ivec2(0), lower_bound));
    ivec2 corrected_upper_bound = min(image_bound, max(ivec2(0), upper_bound));
    ivec2 region_size = corrected_upper_bound - corrected_lower_bound + ivec2(1);
    int quadrant_pixel_count = region_size.x * region_size.y;

#if defined(SUMMED_AREA_TABLE)
    mean_of_color_of_quadrants[q] = summed_area_table_sum(table_tx, lower_bound, upper_bound);
    mean_of_squared_color_of_quadrants[q] = summed_area_table_sum(
        squared_table_tx, lower_bound, upper_bound);
#else
    for (int j = 0; j <= radius; j++) {
      for (int i = 0; i <= radius; i++) {
        vec4 color = texture_load(input_tx, texel + ivec2(i, j) * sign, vec4(0.0));
        mean_of_color_of_quadrants[q] += color;
        mean_of_squared_color_of_quadrants[q] += color * color;
      }
    }
#endif
    mean_of_color_of_quadrants[q] /= quadrant_pixel_count;
    mean_of_squared_color_of_quadrants[q] /= quadrant_pixel_count;
  }

  /* Find the quadrant which has the minimum variance. */
  float minimum_variance = FLT_MAX;
  vec4 mean_color_of_chosen_quadrant = mean_of_color_of_quadrants[0];
  for (int q = 0; q < 4; q++) {
    vec4 color_mean = mean_of_color_of_quadrants[q];
    vec4 squared_color_mean = mean_of_squared_color_of_quadrants[q];
    vec4 color_variance = squared_color_mean - color_mean * color_mean;

    float variance = dot(color_variance.rgb, vec3(1.0));
    if (variance < minimum_variance) {
      minimum_variance = variance;
      mean_color_of_chosen_quadrant = color_mean;
    }
  }

  imageStore(output_img, texel, mean_color_of_chosen_quadrant);
}
