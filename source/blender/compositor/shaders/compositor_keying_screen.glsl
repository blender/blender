/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_base_lib.glsl"

#define CACHE_SIZE (gl_WorkGroupSize.x * gl_WorkGroupSize.y)
shared vec2 cached_marker_positions[CACHE_SIZE];
shared vec4 cached_marker_colors[CACHE_SIZE];

/* Cache the initial part of the marker SSBOs in shared memory to make the interpolation loop
 * faster. */
void populate_cache()
{
  if (int(gl_LocalInvocationIndex) < number_of_markers) {
    cached_marker_positions[gl_LocalInvocationIndex] = marker_positions[gl_LocalInvocationIndex];
    cached_marker_colors[gl_LocalInvocationIndex] = marker_colors[gl_LocalInvocationIndex];
  }
  barrier();
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  ivec2 size = imageSize(output_img);
  vec2 normalized_pixel_location = (vec2(texel) + vec2(0.5)) / vec2(size);
  float squared_shape_parameter = square(1.0 / smoothness);

  populate_cache();

  /* Interpolate the markers using a Gaussian Radial Basis Function Interpolation with the
   * reciprocal of the smoothness as the shaping parameter. Equal weights are assigned to all
   * markers, so no RBF fitting is required. */
  float sum_of_weights = 0.0;
  vec4 weighted_sum = vec4(0.0);
  for (int i = 0; i < number_of_markers; i++) {
    bool use_cache = i < int(CACHE_SIZE);

    vec2 marker_position = use_cache ? cached_marker_positions[i] : marker_positions[i];
    vec2 difference = normalized_pixel_location - marker_position;
    float squared_distance = dot(difference, difference);
    float gaussian = exp(-squared_distance * squared_shape_parameter);

    vec4 marker_color = use_cache ? cached_marker_colors[i] : marker_colors[i];
    weighted_sum += marker_color * gaussian;
    sum_of_weights += gaussian;
  }
  weighted_sum /= sum_of_weights;

  imageStore(output_img, texel, weighted_sum);
}
