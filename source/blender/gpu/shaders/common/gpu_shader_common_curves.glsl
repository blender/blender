/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

vec4 white_balance(vec4 color, vec4 black_level, vec4 white_level)
{
  vec4 range = max(white_level - black_level, vec4(1e-5f));
  return (color - black_level) / range;
}

float extrapolate_if_needed(float parameter, float value, float start_slope, float end_slope)
{
  if (parameter < 0.0) {
    return value + parameter * start_slope;
  }

  if (parameter > 1.0) {
    return value + (parameter - 1.0) * end_slope;
  }

  return value;
}

/* Same as extrapolate_if_needed but vectorized. */
vec3 extrapolate_if_needed(vec3 parameters, vec3 values, vec3 start_slopes, vec3 end_slopes)
{
  vec3 end_or_zero_slopes = mix(vec3(0.0), end_slopes, greaterThan(parameters, vec3(1.0)));
  vec3 slopes = mix(end_or_zero_slopes, start_slopes, lessThan(parameters, vec3(0.0)));
  parameters = parameters - mix(vec3(0.0), vec3(1.0), greaterThan(parameters, vec3(1.0)));
  return values + parameters * slopes;
}

/* Curve maps are stored in texture samplers that are evaluated in the [0, 1] range, so normalize
 * the parameters accordingly. Additionally, ensure that the parameters evaluate the sampler at the
 * center of the pixels, because samplers are evaluated using linear interpolation. */
float normalize_parameter(float parameter, float minimum, float range_divider)
{
  float normalized_parameter = (parameter - minimum) * range_divider;

  /* Curve maps have a fixed width of 257. We offset by the equivalent of half a pixel and scale
   * down such that the normalized parameter 1.0 corresponds to the center of the last pixel. */
  float sampler_offset = 0.5 / 257.0;
  float sampler_scale = 1.0 - (1.0 / 257.0);
  return normalized_parameter * sampler_scale + sampler_offset;
}

/* Same as normalize_parameter but vectorized. */
vec3 normalize_parameters(vec3 parameters, vec3 minimums, vec3 range_dividers)
{
  vec3 normalized_parameters = (parameters - minimums) * range_dividers;

  float sampler_offset = 0.5 / 257.0;
  float sampler_scale = 1.0 - (1.0 / 257.0);
  return normalized_parameters * sampler_scale + sampler_offset;
}

void curves_combined_rgb(float factor,
                         vec4 color,
                         vec4 black_level,
                         vec4 white_level,
                         sampler1DArray curve_map,
                         const float layer,
                         vec4 range_minimums,
                         vec4 range_dividers,
                         vec4 start_slopes,
                         vec4 end_slopes,
                         out vec4 result)
{
  vec4 balanced = white_balance(color, black_level, white_level);

  /* First, evaluate alpha curve map at all channels. The alpha curve is the Combined curve in the
   * UI. */
  vec3 parameters = normalize_parameters(balanced.rgb, range_minimums.aaa, range_dividers.aaa);
  result.r = texture(curve_map, vec2(parameters.x, layer)).a;
  result.g = texture(curve_map, vec2(parameters.y, layer)).a;
  result.b = texture(curve_map, vec2(parameters.z, layer)).a;

  /* Then, extrapolate if needed. */
  result.rgb = extrapolate_if_needed(parameters, result.rgb, start_slopes.aaa, end_slopes.aaa);

  /* Then, evaluate each channel on its curve map. */
  parameters = normalize_parameters(result.rgb, range_minimums.rgb, range_dividers.rgb);
  result.r = texture(curve_map, vec2(parameters.r, layer)).r;
  result.g = texture(curve_map, vec2(parameters.g, layer)).g;
  result.b = texture(curve_map, vec2(parameters.b, layer)).b;

  /* Then, extrapolate again if needed. */
  result.rgb = extrapolate_if_needed(parameters, result.rgb, start_slopes.rgb, end_slopes.rgb);

  result.a = color.a;

  result = mix(color, result, factor);
}

void curves_combined_only(float factor,
                          vec4 color,
                          vec4 black_level,
                          vec4 white_level,
                          sampler1DArray curve_map,
                          const float layer,
                          float range_minimum,
                          float range_divider,
                          float start_slope,
                          float end_slope,
                          out vec4 result)
{
  vec4 balanced = white_balance(color, black_level, white_level);

  /* Evaluate alpha curve map at all channels. The alpha curve is the Combined curve in the
   * UI. */
  vec3 parameters = normalize_parameters(balanced.rgb, vec3(range_minimum), vec3(range_divider));
  result.r = texture(curve_map, vec2(parameters.x, layer)).a;
  result.g = texture(curve_map, vec2(parameters.y, layer)).a;
  result.b = texture(curve_map, vec2(parameters.z, layer)).a;

  /* Then, extrapolate if needed. */
  result.rgb = extrapolate_if_needed(parameters, result.rgb, vec3(start_slope), vec3(end_slope));

  result.a = color.a;

  result = mix(color, result, factor);
}

/* Contrary to standard tone curve implementations, the film-like implementation tries to preserve
 * the hue of the colors as much as possible. To understand why this might be a problem, consider
 * the violet color (0.5, 0.0, 1.0). If this color was to be evaluated at a power curve x^4, the
 * color will be blue (0.0625, 0.0, 1.0). So the color changes and not just its luminosity, which
 * is what film-like tone curves tries to avoid.
 *
 * First, the channels with the lowest and highest values are identified and evaluated at the
 * curve. Then, the third channel---the median---is computed while maintaining the original hue of
 * the color. To do that, we look at the equation for deriving the hue from RGB values. Assuming
 * the maximum, minimum, and median channels are known, and ignoring the 1/3 period offset of the
 * hue, the equation is:
 *
 *   hue = (median - min) / (max - min)                                  [1]
 *
 * Since we have the new values for the minimum and maximum after evaluating at the curve, we also
 * have:
 *
 *   hue = (new_median - new_min) / (new_max - new_min)                  [2]
 *
 * Since we want the hue to be equivalent, by equating [1] and [2] and rearranging:
 *
 *   (new_median - new_min) / (new_max - new_min) = (median - min) / (max - min)
 *   new_median - new_min = (new_max - new_min) * (median - min) / (max - min)
 *   new_median = new_min + (new_max - new_min) * (median - min) / (max - min)
 *   new_median = new_min + (median - min) * ((new_max - new_min) / (max - min))  [QED]
 *
 * Which gives us the median color that preserves the hue. More intuitively, the median is computed
 * such that the change in the distance from the median to the minimum is proportional to the
 * change in the distance from the minimum to the maximum. Finally, each of the new minimum,
 * maximum, and median values are written to the color channel that they were originally extracted
 * from. */
void curves_film_like(float factor,
                      vec4 color,
                      vec4 black_level,
                      vec4 white_level,
                      sampler1DArray curve_map,
                      const float layer,
                      float range_minimum,
                      float range_divider,
                      float start_slope,
                      float end_slope,
                      out vec4 result)
{
  vec4 balanced = white_balance(color, black_level, white_level);

  /* Find the maximum, minimum, and median of the color channels. */
  float minimum = min(balanced.r, min(balanced.g, balanced.b));
  float maximum = max(balanced.r, max(balanced.g, balanced.b));
  float median = max(min(balanced.r, balanced.g), min(balanced.b, max(balanced.r, balanced.g)));

  /* Evaluate alpha curve map at the maximum and minimum channels. The alpha curve is the Combined
   * curve in the UI. */
  float min_parameter = normalize_parameter(minimum, range_minimum, range_divider);
  float max_parameter = normalize_parameter(maximum, range_minimum, range_divider);
  float new_min = texture(curve_map, vec2(min_parameter, layer)).a;
  float new_max = texture(curve_map, vec2(max_parameter, layer)).a;

  /* Then, extrapolate if needed. */
  new_min = extrapolate_if_needed(min_parameter, new_min, start_slope, end_slope);
  new_max = extrapolate_if_needed(max_parameter, new_max, start_slope, end_slope);

  /* Compute the new median using the ratio between the new and the original range. */
  float scaling_ratio = (new_max - new_min) / (maximum - minimum);
  float new_median = new_min + (median - minimum) * scaling_ratio;

  /* Write each value to its original channel. */
  bvec3 channel_is_min = equal(balanced.rgb, vec3(minimum));
  vec3 median_or_min = mix(vec3(new_median), vec3(new_min), channel_is_min);
  bvec3 channel_is_max = equal(balanced.rgb, vec3(maximum));
  result.rgb = mix(median_or_min, vec3(new_max), channel_is_max);

  result.a = color.a;

  result = mix(color, result, clamp(factor, 0.0, 1.0));
}

void curves_vector(vec3 vector,
                   sampler1DArray curve_map,
                   const float layer,
                   vec3 range_minimums,
                   vec3 range_dividers,
                   vec3 start_slopes,
                   vec3 end_slopes,
                   out vec3 result)
{
  /* Evaluate each component on its curve map. */
  vec3 parameters = normalize_parameters(vector, range_minimums, range_dividers);
  result.x = texture(curve_map, vec2(parameters.x, layer)).x;
  result.y = texture(curve_map, vec2(parameters.y, layer)).y;
  result.z = texture(curve_map, vec2(parameters.z, layer)).z;

  /* Then, extrapolate if needed. */
  result = extrapolate_if_needed(parameters, result, start_slopes, end_slopes);
}

void curves_vector_mixed(float factor,
                         vec3 vector,
                         sampler1DArray curve_map,
                         const float layer,
                         vec3 range_minimums,
                         vec3 range_dividers,
                         vec3 start_slopes,
                         vec3 end_slopes,
                         out vec3 result)
{
  curves_vector(
      vector, curve_map, layer, range_minimums, range_dividers, start_slopes, end_slopes, result);
  result = mix(vector, result, factor);
}

void curves_float(float value,
                  sampler1DArray curve_map,
                  const float layer,
                  float range_minimum,
                  float range_divider,
                  float start_slope,
                  float end_slope,
                  out float result)
{
  /* Evaluate the normalized value on the first curve map. */
  float parameter = normalize_parameter(value, range_minimum, range_divider);
  result = texture(curve_map, vec2(parameter, layer)).x;

  /* Then, extrapolate if needed. */
  result = extrapolate_if_needed(parameter, result, start_slope, end_slope);
}

void curves_float_mixed(float factor,
                        float value,
                        sampler1DArray curve_map,
                        const float layer,
                        float range_minimum,
                        float range_divider,
                        float start_slope,
                        float end_slope,
                        out float result)
{
  curves_float(
      value, curve_map, layer, range_minimum, range_divider, start_slope, end_slope, result);
  result = mix(value, result, factor);
}
