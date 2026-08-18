/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"
#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"

/* Float */

[[node]]
void compare_float_less_than(float a, float b, out bool result)
{
  result = (a < b);
}

[[node]]
void compare_float_less_equal(float a, float b, out bool result)
{
  result = (a <= b);
}

[[node]]
void compare_float_greater_than(float a, float b, out bool result)
{
  result = (a > b);
}

[[node]]
void compare_float_greater_equal(float a, float b, out bool result)
{
  result = (a >= b);
}

[[node]]
void compare_float_equal(float a, float b, float epsilon, out bool result)
{
  result = (abs(a - b) <= epsilon);
}

[[node]]
void compare_float_not_equal(float a, float b, float epsilon, out bool result)
{
  result = (abs(a - b) > epsilon);
}

/* Integer */

[[node]]
void compare_int_less_than(int a, int b, out bool result)
{
  result = (a < b);
}

[[node]]
void compare_int_less_equal(int a, int b, out bool result)
{
  result = (a <= b);
}

[[node]]
void compare_int_greater_than(int a, int b, out bool result)
{
  result = (a > b);
}

[[node]]
void compare_int_greater_equal(int a, int b, out bool result)
{
  result = (a >= b);
}

[[node]]
void compare_int_equal(int a, int b, out bool result)
{
  result = (a == b);
}

[[node]]
void compare_int_not_equal(int a, int b, out bool result)
{
  result = (a != b);
}

/* Vector - Less Than */

[[node]]
void compare_vector_average_less_than(float3 a, float3 b, out bool result)
{
  result = (average(a) < average(b));
}

[[node]]
void compare_vector_dot_less_than(float3 a, float3 b, float comp, out bool result)
{
  result = (dot(a, b) < comp);
}

[[node]]
void compare_vector_direction_less_than(float3 a, float3 b, float angle, out bool result)
{
  result = (angle_normalized(a, b) < angle);
}

[[node]]
void compare_vector_element_less_than(float3 a, float3 b, out bool result)
{
  result = (a.x < b.x && a.y < b.y && a.z < b.z);
}

[[node]]
void compare_vector_length_less_than(float3 a, float3 b, out bool result)
{
  result = (length(a) < length(b));
}

/* Vector - Less Equal */

[[node]]
void compare_vector_average_less_equal(float3 a, float3 b, out bool result)
{
  result = (average(a) <= average(b));
}

[[node]]
void compare_vector_dot_less_equal(float3 a, float3 b, float comp, out bool result)
{
  result = (dot(a, b) <= comp);
}

[[node]]
void compare_vector_direction_less_equal(float3 a, float3 b, float angle, out bool result)
{
  result = (angle_normalized(a, b) <= angle);
}

[[node]]
void compare_vector_element_less_equal(float3 a, float3 b, out bool result)
{
  result = (a.x <= b.x && a.y <= b.y && a.z <= b.z);
}

[[node]]
void compare_vector_length_less_equal(float3 a, float3 b, out bool result)
{
  result = (length(a) <= length(b));
}

/* Vector - Greater Than */

[[node]]
void compare_vector_average_greater_than(float3 a, float3 b, out bool result)
{
  result = (average(a) > average(b));
}

[[node]]
void compare_vector_dot_greater_than(float3 a, float3 b, float comp, out bool result)
{
  result = (dot(a, b) > comp);
}

[[node]]
void compare_vector_direction_greater_than(float3 a, float3 b, float angle, out bool result)
{
  result = (angle_normalized(a, b) > angle);
}

[[node]]
void compare_vector_element_greater_than(float3 a, float3 b, out bool result)
{
  result = (a.x > b.x && a.y > b.y && a.z > b.z);
}

[[node]]
void compare_vector_length_greater_than(float3 a, float3 b, out bool result)
{
  result = (length(a) > length(b));
}

/* Vector - Greater Equal */

[[node]]
void compare_vector_average_greater_equal(float3 a, float3 b, out bool result)
{
  result = (average(a) >= average(b));
}

[[node]]
void compare_vector_dot_greater_equal(float3 a, float3 b, float comp, out bool result)
{
  result = (dot(a, b) >= comp);
}

[[node]]
void compare_vector_direction_greater_equal(float3 a, float3 b, float angle, out bool result)
{
  result = (angle_normalized(a, b) >= angle);
}

[[node]]
void compare_vector_element_greater_equal(float3 a, float3 b, out bool result)
{
  result = (a.x >= b.x && a.y >= b.y && a.z >= b.z);
}

[[node]]
void compare_vector_length_greater_equal(float3 a, float3 b, out bool result)
{
  result = (length(a) >= length(b));
}

/* Vector - Equal */

[[node]]
void compare_vector_average_equal(float3 a, float3 b, float epsilon, out bool result)
{
  result = (abs(average(a) - average(b)) <= epsilon);
}

[[node]]
void compare_vector_dot_equal(float3 a, float3 b, float comp, float epsilon, out bool result)
{
  result = (abs(dot(a, b) - comp) <= epsilon);
}

[[node]]
void compare_vector_direction_equal(
    float3 a, float3 b, float angle, float epsilon, out bool result)
{
  result = (abs(angle_normalized(a, b) - angle) <= epsilon);
}

[[node]]
void compare_vector_element_equal(float3 a, float3 b, float epsilon, out bool result)
{
  result = (abs(a.x - b.x) <= epsilon && abs(a.y - b.y) <= epsilon && abs(a.z - b.z) <= epsilon);
}

[[node]]
void compare_vector_length_equal(float3 a, float3 b, float epsilon, out bool result)
{
  result = (abs(length(a) - length(b)) <= epsilon);
}

/* Vector - Not Equal */

[[node]]
void compare_vector_average_not_equal(float3 a, float3 b, float epsilon, out bool result)
{
  result = (abs(average(a) - average(b)) > epsilon);
}

[[node]]
void compare_vector_dot_not_equal(float3 a, float3 b, float comp, float epsilon, out bool result)
{
  result = (abs(dot(a, b) - comp) > epsilon);
}

[[node]]
void compare_vector_direction_not_equal(
    float3 a, float3 b, float angle, float epsilon, out bool result)
{
  result = (abs(angle_normalized(a, b) - angle) > epsilon);
}

[[node]]
void compare_vector_element_not_equal(float3 a, float3 b, float epsilon, out bool result)
{
  result = (abs(a.x - b.x) > epsilon || abs(a.y - b.y) > epsilon || abs(a.z - b.z) > epsilon);
}

[[node]]
void compare_vector_length_not_equal(float3 a, float3 b, float epsilon, out bool result)
{
  result = (abs(length(a) - length(b)) > epsilon);
}

/* Color. */

[[node]]
void compare_color_equal(float4 a, float4 b, float epsilon, out bool result)
{
  result = (abs(a.x - b.x) <= epsilon && abs(a.y - b.y) <= epsilon && abs(a.z - b.z) <= epsilon);
}

[[node]]
void compare_color_not_equal(float4 a, float4 b, float epsilon, out bool result)
{
  result = (abs(a.x - b.x) > epsilon || abs(a.y - b.y) > epsilon || abs(a.z - b.z) > epsilon);
}

[[node]]
void compare_color_brighter(float4 a, float4 b, float3 luminance_coefficients, out bool result)
{
  float luminance_a = get_luminance(a.rgb, luminance_coefficients);
  float luminance_b = get_luminance(b.rgb, luminance_coefficients);
  result = (luminance_a > luminance_b);
}

[[node]]
void compare_color_darker(float4 a, float4 b, float3 luminance_coefficients, out bool result)
{
  float luminance_a = get_luminance(a.rgb, luminance_coefficients);
  float luminance_b = get_luminance(b.rgb, luminance_coefficients);
  result = (luminance_a < luminance_b);
}
