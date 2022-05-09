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

/* Curve maps are stored in sampler objects that are evaluated in the [0, 1] range, so normalize
 * parameters accordingly. */
#define NORMALIZE_PARAMETER(parameter, minimum, range) ((parameter - minimum) * range)

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
  vec3 parameters = NORMALIZE_PARAMETER(balanced.rgb, range_minimums.aaa, range_dividers.aaa);
  result.r = texture(curve_map, vec2(parameters.x, layer)).a;
  result.g = texture(curve_map, vec2(parameters.y, layer)).a;
  result.b = texture(curve_map, vec2(parameters.z, layer)).a;

  /* Then, extrapolate if needed. */
  result.rgb = extrapolate_if_needed(parameters, result.rgb, start_slopes.aaa, end_slopes.aaa);

  /* Then, evaluate each channel on its curve map. */
  parameters = NORMALIZE_PARAMETER(result.rgb, range_minimums.rgb, range_dividers.rgb);
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
  vec3 parameters = NORMALIZE_PARAMETER(balanced.rgb, range_minimum, range_divider);
  result.r = texture(curve_map, vec2(parameters.x, layer)).a;
  result.g = texture(curve_map, vec2(parameters.y, layer)).a;
  result.b = texture(curve_map, vec2(parameters.z, layer)).a;

  /* Then, extrapolate if needed. */
  result.rgb = extrapolate_if_needed(parameters, result.rgb, vec3(start_slope), vec3(end_slope));
  result.a = color.a;

  result = mix(color, result, factor);
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
  vec3 parameters = NORMALIZE_PARAMETER(vector, range_minimums, range_dividers);
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
  float parameter = NORMALIZE_PARAMETER(value, range_minimum, range_divider);
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
