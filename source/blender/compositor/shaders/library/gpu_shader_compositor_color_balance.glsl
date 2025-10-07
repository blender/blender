/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_construct_lib.glsl"

#define CMP_NODE_COLOR_BALANCE_LGG 0
#define CMP_NODE_COLOR_BALANCE_ASC_CDL 1
#define CMP_NODE_COLOR_BALANCE_WHITEPOINT 2

float4 lift_gamma_gain(const float4 color,
                       const float base_lift,
                       const float4 color_lift,
                       const float base_gamma,
                       const float4 color_gamma,
                       const float base_gain,
                       const float4 color_gain)
{
  const float3 lift = base_lift + color_lift.xyz();
  const float3 lift_balanced = ((color.xyz() - 1.0f) * (2.0f - lift)) + 1.0f;

  const float3 gain = base_gain * color_gain.xyz();
  const float3 gain_balanced = max(float3(0.0f), lift_balanced * gain);

  const float3 gamma = base_gamma * color_gamma.xyz();
  const float3 gamma_balanced = pow(gain_balanced, 1.0f / max(gamma, float3(1e-6)));

  return float4(gamma_balanced, color.w);
}

float4 offset_power_slope(const float4 color,
                          const float base_offset,
                          const float4 color_offset,
                          const float base_power,
                          const float4 color_power,
                          const float base_slope,
                          const float4 color_slope)
{
  const float3 slope = base_slope * color_slope.xyz();
  const float3 slope_balanced = color.xyz() * slope;

  const float3 offset = base_offset + color_offset.xyz();
  const float3 offset_balanced = slope_balanced + offset;

  const float3 power = base_power * color_power.xyz();
  const float3 power_balanced = pow(max(offset_balanced, float3(0.0f)), power);

  return float4(power_balanced, color.w);
}

float4 white_point_constant(const float4 color, const float3x3 white_point_matrix)
{
  const float3 balanced = white_point_matrix * color.xyz();
  return float4(balanced, color.w);
}

float3 whitepoint_from_temp_tint(const float temperature, const float tint)
{
  /* Tabulated approximation of the Planckian locus. Based on:
   *
   *   http://www.brucelindbloom.com/Eqn_XYZ_to_T.html.
   *
   * Original source:
   *
   *   "Color Science: Concepts and Methods, Quantitative Data and Formulae", Second Edition,
   *   Gunter Wyszecki and W. S. Stiles, John Wiley & Sons, 1982, pp. 227, 228.
   *
   * Note that the inverse temperature table is multiplied by 10^6 compared to the reference
   * table. */
  constexpr float inverse_temperatures[31] = float_array(0.0f,
                                                         10.0f,
                                                         20.0f,
                                                         30.0f,
                                                         40.0f,
                                                         50.0f,
                                                         60.0f,
                                                         70.0f,
                                                         80.0f,
                                                         90.0f,
                                                         100.0f,
                                                         125.0f,
                                                         150.0f,
                                                         175.0f,
                                                         200.0f,
                                                         225.0f,
                                                         250.0f,
                                                         275.0f,
                                                         300.0f,
                                                         325.0f,
                                                         350.0f,
                                                         375.0f,
                                                         400.0f,
                                                         425.0f,
                                                         450.0f,
                                                         475.0f,
                                                         500.0f,
                                                         525.0f,
                                                         550.0f,
                                                         575.0f,
                                                         600.0f);

  constexpr float2 uv_coordinates[31] = float2_array(float2(0.18006f, 0.26352f),
                                                     float2(0.18066f, 0.26589f),
                                                     float2(0.18133f, 0.26846f),
                                                     float2(0.18208f, 0.27119f),
                                                     float2(0.18293f, 0.27407f),
                                                     float2(0.18388f, 0.27709f),
                                                     float2(0.18494f, 0.28021f),
                                                     float2(0.18611f, 0.28342f),
                                                     float2(0.18740f, 0.28668f),
                                                     float2(0.18880f, 0.28997f),
                                                     float2(0.19032f, 0.29326f),
                                                     float2(0.19462f, 0.30141f),
                                                     float2(0.19962f, 0.30921f),
                                                     float2(0.20525f, 0.31647f),
                                                     float2(0.21142f, 0.32312f),
                                                     float2(0.21807f, 0.32909f),
                                                     float2(0.22511f, 0.33439f),
                                                     float2(0.23247f, 0.33904f),
                                                     float2(0.24010f, 0.34308f),
                                                     float2(0.24792f, 0.34655f),
                                                     float2(0.25591f, 0.34951f),
                                                     float2(0.26400f, 0.35200f),
                                                     float2(0.27218f, 0.35407f),
                                                     float2(0.28039f, 0.35577f),
                                                     float2(0.28863f, 0.35714f),
                                                     float2(0.29685f, 0.35823f),
                                                     float2(0.30505f, 0.35907f),
                                                     float2(0.31320f, 0.35968f),
                                                     float2(0.32129f, 0.36011f),
                                                     float2(0.32931f, 0.36038f),
                                                     float2(0.33724f, 0.36051f));

  constexpr float isotherm_parameters[31] = float_array(-0.24341f,
                                                        -0.25479f,
                                                        -0.26876f,
                                                        -0.28539f,
                                                        -0.30470f,
                                                        -0.32675f,
                                                        -0.35156f,
                                                        -0.37915f,
                                                        -0.40955f,
                                                        -0.44278f,
                                                        -0.47888f,
                                                        -0.58204f,
                                                        -0.70471f,
                                                        -0.84901f,
                                                        -1.0182f,
                                                        -1.2168f,
                                                        -1.4512f,
                                                        -1.7298f,
                                                        -2.0637f,
                                                        -2.4681f,
                                                        -2.9641f,
                                                        -3.5814f,
                                                        -4.3633f,
                                                        -5.3762f,
                                                        -6.7262f,
                                                        -8.5955f,
                                                        -11.324f,
                                                        -15.628f,
                                                        -23.325f,
                                                        -40.770f,
                                                        -116.45f);

  /* Compute the inverse temperature, multiplying by 10^6 since the reference table is scaled by
   * that factor. We also make sure we don't divide by zero and are less than 600 for a simpler
   * algorithm as will be seen. */
  const float inverse_temperature = clamp(1e6f / max(1e-6f, temperature), 0.0f, 600.0f - 1e-6f);

  /* Find the index of the table entry that is less than or equal the inverse temperature. Note
   * that the table is two arithmetic sequences concatenated, [0, 10, 20, ...] followed by [100,
   * 125, 150, ...]. So the index in the first sequence is simply the floor division by 10, and the
   * second is simply the floor division by 25, while of course adjusting for the start value of
   * the sequence. */
  const int i = inverse_temperature < 100.0f ? (int(inverse_temperature) / 10) :
                                               (int(inverse_temperature - 100.0f) / 25 + 10);

  /* Find interpolation factor. */
  const float interpolation_factor = (inverse_temperature - inverse_temperatures[i]) /
                                     (inverse_temperatures[i + 1] - inverse_temperatures[i]);

  /* Interpolate point along Planckian locus. */
  const float2 uv = mix(uv_coordinates[i], uv_coordinates[i + 1], interpolation_factor);

  /* Compute and interpolate isotherm. */
  const float2 lower_isotherm = normalize(float2(1.0f, isotherm_parameters[i]));
  const float2 higher_isotherm = normalize(float2(1.0f, isotherm_parameters[i + 1]));
  const float2 isotherm = normalize(mix(lower_isotherm, higher_isotherm, interpolation_factor));

  /* Offset away from the Planckian locus according to the tint.
   * Tint is parameterized such that +-3000 tint corresponds to +-1 delta UV. */
  const float2 tinted_uv = uv - (isotherm * tint / 3000.0f);

  /* Convert CIE 1960 uv -> xyY. */
  const float x = 3.0f * tinted_uv.x / (2.0f * tinted_uv.x - 8.0f * tinted_uv.y + 4.0f);
  const float y = 2.0f * tinted_uv.y / (2.0f * tinted_uv.x - 8.0f * tinted_uv.y + 4.0f);

  /* Convert xyY -> XYZ (assuming Y=1). */
  return float3(x / y, 1.0f, (1.0f - x - y) / y);
}

float3x3 chromatic_adaption_matrix(const float3 from_XYZ, const float3 to_XYZ)
{
  /* Bradford transformation matrix (XYZ -> LMS). */
  const float3x3 bradford = float3x3(float3(0.8951f, -0.7502f, 0.0389f),
                                     float3(0.2664f, 1.7135f, -0.0685f),
                                     float3(-0.1614f, 0.0367f, 1.0296f));

  /* Compute white points in LMS space. */
  const float3 from_LMS = bradford * from_XYZ / from_XYZ.y;
  const float3 to_LMS = bradford * to_XYZ / to_XYZ.y;

  /* Assemble full transform: XYZ -> LMS -> adapted LMS -> adapted XYZ. */
  return inverse(bradford) * from_scale(to_LMS / from_LMS) * bradford;
}

float4 white_point_variable(const float4 color,
                            const float input_temperature,
                            const float input_tint,
                            const float output_temperature,
                            const float output_tint,
                            const float3x3 scene_to_xyz,
                            const float3x3 xyz_to_scene)
{
  const float3 input_white_point = whitepoint_from_temp_tint(input_temperature, input_tint);
  const float3 output_white_point = whitepoint_from_temp_tint(output_temperature, output_tint);
  const float3x3 adaption = chromatic_adaption_matrix(input_white_point, output_white_point);
  const float3x3 white_point_matrix = xyz_to_scene * adaption * scene_to_xyz;

  const float3 balanced = white_point_matrix * color.xyz;
  return float4(balanced, color.w);
}

void node_composite_color_balance(const float4 color,
                                  const float factor,
                                  const float type,
                                  const float base_lift,
                                  const float4 color_lift,
                                  const float base_gamma,
                                  const float4 color_gamma,
                                  const float base_gain,
                                  const float4 color_gain,
                                  const float base_offset,
                                  const float4 color_offset,
                                  const float base_power,
                                  const float4 color_power,
                                  const float base_slope,
                                  const float4 color_slope,
                                  const float input_temperature,
                                  const float input_tint,
                                  const float output_temperature,
                                  const float output_tint,
                                  const float4x4 scene_to_xyz,
                                  const float4x4 xyz_to_scene,
                                  out float4 result)
{
  switch (int(type)) {
    case CMP_NODE_COLOR_BALANCE_LGG:
      result = lift_gamma_gain(
          color, base_lift, color_lift, base_gamma, color_gamma, base_gain, color_gain);
      break;
    case CMP_NODE_COLOR_BALANCE_ASC_CDL:
      result = offset_power_slope(
          color, base_offset, color_offset, base_power, color_power, base_slope, color_slope);
      break;
    case CMP_NODE_COLOR_BALANCE_WHITEPOINT:
      result = white_point_variable(color,
                                    input_temperature,
                                    input_tint,
                                    output_temperature,
                                    output_tint,
                                    to_float3x3(scene_to_xyz),
                                    to_float3x3(xyz_to_scene));
      break;
  }

  result = float4(mix(color.xyz(), result.xyz(), min(factor, 1.0f)), color.w);
}

void node_composite_color_balance_white_point_constant(const float4 color,
                                                       const float factor,
                                                       const float type,
                                                       const float base_lift,
                                                       const float4 color_lift,
                                                       const float base_gamma,
                                                       const float4 color_gamma,
                                                       const float base_gain,
                                                       const float4 color_gain,
                                                       const float base_offset,
                                                       const float4 color_offset,
                                                       const float base_power,
                                                       const float4 color_power,
                                                       const float base_slope,
                                                       const float4 color_slope,
                                                       const float input_temperature,
                                                       const float input_tint,
                                                       const float output_temperature,
                                                       const float output_tint,
                                                       const float4x4 white_point_matrix,
                                                       out float4 result)
{
  const float3 balanced = to_float3x3(white_point_matrix) * color.xyz();
  result = float4(mix(color.xyz(), balanced.xyz(), min(factor, 1.0f)), color.w);
}

#undef CMP_NODE_COLOR_BALANCE_LGG
#undef CMP_NODE_COLOR_BALANCE_ASC_CDL
#undef CMP_NODE_COLOR_BALANCE_WHITEPOINT
