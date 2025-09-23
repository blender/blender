/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

float4 white_balance(float4 color, float4 black_level, float4 white_level)
{
  float4 range = max(white_level - black_level, float4(1e-5f));
  return (color - black_level) / range;
}

float extrapolate_if_needed(float parameter, float value, float start_slope, float end_slope)
{
  if (parameter < 0.0f) {
    return value + parameter * start_slope;
  }

  if (parameter > 1.0f) {
    return value + (parameter - 1.0f) * end_slope;
  }

  return value;
}

/* Same as extrapolate_if_needed but vectorized. */
float3 extrapolate_if_needed(float3 parameters,
                             float3 values,
                             float3 start_slopes,
                             float3 end_slopes)
{
  float3 end_or_zero_slopes = mix(float3(0.0f), end_slopes, greaterThan(parameters, float3(1.0f)));
  float3 slopes = mix(end_or_zero_slopes, start_slopes, lessThan(parameters, float3(0.0f)));
  parameters = parameters - mix(float3(0.0f), float3(1.0f), greaterThan(parameters, float3(1.0f)));
  return values + parameters * slopes;
}

/* Curve maps are stored in texture samplers, so ensure that the parameters evaluate the sampler at
 * the center of the pixels, because samplers are evaluated using linear interpolation. Given the
 * parameter in the [0, 1] range. */
float compute_curve_map_coordinates(float parameter)
{
  /* Curve maps have a fixed width of 257. We offset by the equivalent of half a pixel and scale
   * down such that the normalized parameter 1.0 corresponds to the center of the last pixel. */
  float sampler_offset = 0.5f / 257.0f;
  float sampler_scale = 1.0f - (1.0f / 257.0f);
  return parameter * sampler_scale + sampler_offset;
}

/* Same as compute_curve_map_coordinates but vectorized. */
float3 compute_curve_map_coordinates(float3 parameters)
{
  float sampler_offset = 0.5f / 257.0f;
  float sampler_scale = 1.0f - (1.0f / 257.0f);
  return parameters * sampler_scale + sampler_offset;
}

void curves_combined_rgb(float factor,
                         float4 color,
                         float4 black_level,
                         float4 white_level,
                         sampler1DArray curve_map,
                         const float layer,
                         float4 range_minimums,
                         float4 range_dividers,
                         float4 start_slopes,
                         float4 end_slopes,
                         out float4 result)
{
  float4 balanced = white_balance(color, black_level, white_level);

  /* First, evaluate alpha curve map at all channels. The alpha curve is the Combined curve in the
   * UI. The channels are first normalized into the [0, 1] range. */
  float3 parameters = (balanced.rgb - range_minimums.aaa) * range_dividers.aaa;
  float3 coordinates = compute_curve_map_coordinates(parameters);
  result.r = texture(curve_map, float2(coordinates.x, layer)).a;
  result.g = texture(curve_map, float2(coordinates.y, layer)).a;
  result.b = texture(curve_map, float2(coordinates.z, layer)).a;

  /* Then, extrapolate if needed. */
  result.rgb = extrapolate_if_needed(parameters, result.rgb, start_slopes.aaa, end_slopes.aaa);

  /* Then, evaluate each channel on its curve map. The channels are first normalized into the
   * [0, 1] range. */
  parameters = (result.rgb - range_minimums.rgb) * range_dividers.rgb;
  coordinates = compute_curve_map_coordinates(parameters);
  result.r = texture(curve_map, float2(coordinates.r, layer)).r;
  result.g = texture(curve_map, float2(coordinates.g, layer)).g;
  result.b = texture(curve_map, float2(coordinates.b, layer)).b;

  /* Then, extrapolate again if needed. */
  result.rgb = extrapolate_if_needed(parameters, result.rgb, start_slopes.rgb, end_slopes.rgb);

  result.a = color.a;

  result = mix(color, result, factor);
}

void curves_combined_rgb_compositor(float4 color,
                                    float factor,
                                    float4 black_level,
                                    float4 white_level,
                                    sampler1DArray curve_map,
                                    const float layer,
                                    float4 range_minimums,
                                    float4 range_dividers,
                                    float4 start_slopes,
                                    float4 end_slopes,
                                    out float4 result)
{
  curves_combined_rgb(factor,
                      color,
                      black_level,
                      white_level,
                      curve_map,
                      layer,
                      range_minimums,
                      range_dividers,
                      start_slopes,
                      end_slopes,
                      result);
}

void curves_combined_only(float factor,
                          float4 color,
                          float4 black_level,
                          float4 white_level,
                          sampler1DArray curve_map,
                          const float layer,
                          float range_minimum,
                          float range_divider,
                          float start_slope,
                          float end_slope,
                          out float4 result)
{
  float4 balanced = white_balance(color, black_level, white_level);

  /* Evaluate alpha curve map at all channels. The alpha curve is the Combined curve in the
   * UI. The channels are first normalized into the [0, 1] range. */
  float3 parameters = (balanced.rgb - float3(range_minimum)) * float3(range_divider);
  float3 coordinates = compute_curve_map_coordinates(parameters);
  result.r = texture(curve_map, float2(coordinates.x, layer)).a;
  result.g = texture(curve_map, float2(coordinates.y, layer)).a;
  result.b = texture(curve_map, float2(coordinates.z, layer)).a;

  /* Then, extrapolate if needed. */
  result.rgb = extrapolate_if_needed(
      parameters, result.rgb, float3(start_slope), float3(end_slope));

  result.a = color.a;

  result = mix(color, result, factor);
}

void curves_combined_only_compositor(float4 color,
                                     float factor,
                                     float4 black_level,
                                     float4 white_level,
                                     sampler1DArray curve_map,
                                     const float layer,
                                     float range_minimum,
                                     float range_divider,
                                     float start_slope,
                                     float end_slope,
                                     out float4 result)
{
  curves_combined_only(factor,
                       color,
                       black_level,
                       white_level,
                       curve_map,
                       layer,
                       range_minimum,
                       range_divider,
                       start_slope,
                       end_slope,
                       result);
}

/* Contrary to standard tone curve implementations, the film-like implementation tries to preserve
 * the hue of the colors as much as possible. To understand why this might be a problem, consider
 * the violet color (0.5, 0.0, 1.0). If this color was to be evaluated at a power curve x^4, the
 * color will be blue (0.0625, 0.0, 1.0). So the color changes and not just its luminosity,
 * which is what film-like tone curves tries to avoid.
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
                      float4 color,
                      float4 black_level,
                      float4 white_level,
                      sampler1DArray curve_map,
                      const float layer,
                      float range_minimum,
                      float range_divider,
                      float start_slope,
                      float end_slope,
                      out float4 result)
{
  float4 balanced = white_balance(color, black_level, white_level);

  /* Find the maximum, minimum, and median of the color channels. */
  float minimum = min(balanced.r, min(balanced.g, balanced.b));
  float maximum = max(balanced.r, max(balanced.g, balanced.b));
  float median = max(min(balanced.r, balanced.g), min(balanced.b, max(balanced.r, balanced.g)));

  /* Evaluate alpha curve map at the maximum and minimum channels. The alpha curve is the Combined
   * curve in the UI. The channels are first normalized into the [0, 1] range. */
  float min_parameter = (minimum - range_minimum) * range_divider;
  float max_parameter = (maximum - range_minimum) * range_divider;
  float min_coordinates = compute_curve_map_coordinates(min_parameter);
  float max_coordinates = compute_curve_map_coordinates(max_parameter);
  float new_min = texture(curve_map, float2(min_coordinates, layer)).a;
  float new_max = texture(curve_map, float2(max_coordinates, layer)).a;

  /* Then, extrapolate if needed. */
  new_min = extrapolate_if_needed(min_parameter, new_min, start_slope, end_slope);
  new_max = extrapolate_if_needed(max_parameter, new_max, start_slope, end_slope);

  /* Compute the new median using the ratio between the new and the original range. */
  float scaling_ratio = (new_max - new_min) / (maximum - minimum);
  float new_median = new_min + (median - minimum) * scaling_ratio;

  /* Write each value to its original channel. */
  bool3 channel_is_min = equal(balanced.rgb, float3(minimum));
  float3 median_or_min = mix(float3(new_median), float3(new_min), channel_is_min);
  bool3 channel_is_max = equal(balanced.rgb, float3(maximum));
  result.rgb = mix(median_or_min, float3(new_max), channel_is_max);

  result.a = color.a;

  result = mix(color, result, clamp(factor, 0.0f, 1.0f));
}

void curves_film_like_compositor(float4 color,
                                 float factor,
                                 float4 black_level,
                                 float4 white_level,
                                 sampler1DArray curve_map,
                                 const float layer,
                                 float range_minimum,
                                 float range_divider,
                                 float start_slope,
                                 float end_slope,
                                 out float4 result)
{
  curves_film_like(factor,
                   color,
                   black_level,
                   white_level,
                   curve_map,
                   layer,
                   range_minimum,
                   range_divider,
                   start_slope,
                   end_slope,
                   result);
}

void curves_vector(float3 vector,
                   sampler1DArray curve_map,
                   const float layer,
                   float3 range_minimums,
                   float3 range_dividers,
                   float3 start_slopes,
                   float3 end_slopes,
                   out float3 result)
{
  /* Evaluate each component on its curve map.
   * The components are first normalized into the [0, 1] range. */
  float3 parameters = (vector - range_minimums) * range_dividers;
  float3 coordinates = compute_curve_map_coordinates(parameters);
  result.x = texture(curve_map, float2(coordinates.x, layer)).x;
  result.y = texture(curve_map, float2(coordinates.y, layer)).y;
  result.z = texture(curve_map, float2(coordinates.z, layer)).z;

  /* Then, extrapolate if needed. */
  result = extrapolate_if_needed(parameters, result, start_slopes, end_slopes);
}

void curves_vector_mixed(float factor,
                         float3 vector,
                         sampler1DArray curve_map,
                         const float layer,
                         float3 range_minimums,
                         float3 range_dividers,
                         float3 start_slopes,
                         float3 end_slopes,
                         out float3 result)
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
  float parameter = (value - range_minimum) * range_divider;
  float coordinates = compute_curve_map_coordinates(parameter);
  result = texture(curve_map, float2(coordinates, layer)).x;

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
