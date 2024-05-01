/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Filtering utilities.
 */

#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)

/**
 * Return the factor to filter_gaussian_weight. This is handy utility function to compute your
 * gaussian parameter in a documented manner.
 * - `linear_distance` is the distance at which the filter will have covered the given amount of
 *   `standard_deviation`. Must not be null.
 * - `standard_deviation` is the shape of the bell. Higher values sharpens the filter.
 *
 * https://en.wikipedia.org/wiki/Standard_deviation#/media/File:Standard_deviation_diagram.svg
 *
 * Example: for a 5px 1d gaussian filter, one would set `linear_distance` of 2.5.
 * `standard_deviation = 1.0` will cover 68% of the gaussian weight inside the 5px radius.
 * `standard_deviation = 2.0` will cover 95% of the gaussian weight inside the 5px radius.
 */
float filter_gaussian_factor(float linear_distance, float standard_deviation)
{
  /* Account for `filter_gaussian_factor` using `exp2` for speed (`exp(x) = exp2(x / log(2))`). */
  const float log_2_inv = 1.442695041;
  return log_2_inv * standard_deviation / square(linear_distance);
}

/**
 * Gaussian distance weighting. Allow weighting based on distance without null weight whatever the
 * distance. `factor` is supposed to be a scaling parameter given by `filter_gaussian_factor`.
 */
float filter_gaussian_weight(float factor, float square_distance)
{
  /* Using exp2 since it is faster on GPU. `filter_gaussian_factor` account for that. */
  return exp2(-factor * square_distance);
}

/**
 * Planar distance weighting. Allow to weight based on geometric neighborhood.
 */
float filter_planar_weight(vec3 plane_N, vec3 plane_P, vec3 P, float scale)
{
  vec4 plane_eq = vec4(plane_N, -dot(plane_N, plane_P));
  float plane_distance = dot(plane_eq, vec4(P, 1.0));
  return filter_gaussian_weight(scale, square(plane_distance));
}

/**
 * Angle weighting. Mostly used for normals.
 * Expects both normals to be normalized.
 */
float filter_angle_weight(vec3 center_N, vec3 sample_N)
{
  float facing_ratio = dot(center_N, sample_N);
  return saturate(pow8f(facing_ratio));
}
