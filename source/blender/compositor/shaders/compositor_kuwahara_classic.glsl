/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_summed_area_table_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

#if defined(VARIABLE_SIZE)
  int radius = max(0, int(texture_load(size_tx, texel).x));
#elif defined(CONSTANT_SIZE)
  int radius = max(0, size);
#endif

  float4 mean_of_squared_color_of_quadrants[4] = float4_array(
      float4(0.0f), float4(0.0f), float4(0.0f), float4(0.0f));
  float4 mean_of_color_of_quadrants[4] = float4_array(
      float4(0.0f), float4(0.0f), float4(0.0f), float4(0.0f));

  /* Compute the above statistics for each of the quadrants around the current pixel. */
  for (int q = 0; q < 4; q++) {
    /* A fancy expression to compute the sign of the quadrant q. */
    int2 sign = int2((q % 2) * 2 - 1, ((q / 2) * 2 - 1));

    int2 lower_bound = texel - int2(sign.x > 0 ? 0 : radius, sign.y > 0 ? 0 : radius);
    int2 upper_bound = texel + int2(sign.x < 0 ? 0 : radius, sign.y < 0 ? 0 : radius);

    /* Limit the quadrants to the image bounds. */
    int2 image_bound = imageSize(output_img) - int2(1);
    int2 corrected_lower_bound = min(image_bound, max(int2(0), lower_bound));
    int2 corrected_upper_bound = min(image_bound, max(int2(0), upper_bound));
    int2 region_size = corrected_upper_bound - corrected_lower_bound + int2(1);
    int quadrant_pixel_count = region_size.x * region_size.y;

#if defined(SUMMED_AREA_TABLE)
    mean_of_color_of_quadrants[q] = summed_area_table_sum(table_tx, lower_bound, upper_bound);
    mean_of_squared_color_of_quadrants[q] = summed_area_table_sum(
        squared_table_tx, lower_bound, upper_bound);
#else
    for (int j = 0; j <= radius; j++) {
      for (int i = 0; i <= radius; i++) {
        float4 color = texture_load(input_tx, texel + int2(i, j) * sign, float4(0.0f));
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
  float4 mean_color_of_chosen_quadrant = mean_of_color_of_quadrants[0];
  for (int q = 0; q < 4; q++) {
    float4 color_mean = mean_of_color_of_quadrants[q];
    float4 squared_color_mean = mean_of_squared_color_of_quadrants[q];
    float4 color_variance = squared_color_mean - color_mean * color_mean;

    float variance = dot(color_variance.rgb, float3(1.0f));
    if (variance < minimum_variance) {
      minimum_variance = variance;
      mean_color_of_chosen_quadrant = color_mean;
    }
  }

  imageStore(output_img, texel, mean_color_of_chosen_quadrant);
}
