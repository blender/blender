/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)

float smootherstep(float edge0, float edge1, float x)
{
  x = clamp(safe_divide((x - edge0), (edge1 - edge0)), 0.0, 1.0);
  return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

vec3 smootherstep(vec3 edge0, vec3 edge1, vec3 x)
{
  x = clamp(safe_divide((x - edge0), (edge1 - edge0)), 0.0, 1.0);
  return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

void vector_map_range_linear(float value,
                             float fromMin,
                             float fromMax,
                             float toMin,
                             float toMax,
                             float steps,
                             vec3 v_value,
                             vec3 v_from_min,
                             vec3 v_from_max,
                             vec3 v_to_min,
                             vec3 v_to_max,
                             vec3 v_steps,
                             float use_clamp,
                             out float result,
                             out vec3 v_result)
{
  vec3 factor = safe_divide((v_value - v_from_min), (v_from_max - v_from_min));
  v_result = v_to_min + factor * (v_to_max - v_to_min);
  if (use_clamp > 0.0) {
    v_result.x = (v_to_min.x > v_to_max.x) ? clamp(v_result.x, v_to_max.x, v_to_min.x) :
                                             clamp(v_result.x, v_to_min.x, v_to_max.x);
    v_result.y = (v_to_min.y > v_to_max.y) ? clamp(v_result.y, v_to_max.y, v_to_min.y) :
                                             clamp(v_result.y, v_to_min.y, v_to_max.y);
    v_result.z = (v_to_min.z > v_to_max.z) ? clamp(v_result.z, v_to_max.z, v_to_min.z) :
                                             clamp(v_result.z, v_to_min.z, v_to_max.z);
  }
}

void vector_map_range_stepped(float value,
                              float fromMin,
                              float fromMax,
                              float toMin,
                              float toMax,
                              float steps,
                              vec3 v_value,
                              vec3 v_from_min,
                              vec3 v_from_max,
                              vec3 v_to_min,
                              vec3 v_to_max,
                              vec3 v_steps,
                              float use_clamp,
                              out float result,
                              out vec3 v_result)
{
  vec3 factor = safe_divide((v_value - v_from_min), (v_from_max - v_from_min));
  factor = safe_divide(floor(factor * (v_steps + 1.0)), v_steps);
  v_result = v_to_min + factor * (v_to_max - v_to_min);
  if (use_clamp > 0.0) {
    v_result.x = (v_to_min.x > v_to_max.x) ? clamp(v_result.x, v_to_max.x, v_to_min.x) :
                                             clamp(v_result.x, v_to_min.x, v_to_max.x);
    v_result.y = (v_to_min.y > v_to_max.y) ? clamp(v_result.y, v_to_max.y, v_to_min.y) :
                                             clamp(v_result.y, v_to_min.y, v_to_max.y);
    v_result.z = (v_to_min.z > v_to_max.z) ? clamp(v_result.z, v_to_max.z, v_to_min.z) :
                                             clamp(v_result.z, v_to_min.z, v_to_max.z);
  }
}

void vector_map_range_smoothstep(float value,
                                 float fromMin,
                                 float fromMax,
                                 float toMin,
                                 float toMax,
                                 float steps,
                                 vec3 v_value,
                                 vec3 v_from_min,
                                 vec3 v_from_max,
                                 vec3 v_to_min,
                                 vec3 v_to_max,
                                 vec3 v_steps,
                                 float use_clamp,
                                 out float result,
                                 out vec3 v_result)
{
  vec3 factor = safe_divide((v_value - v_from_min), (v_from_max - v_from_min));
  factor = clamp(factor, 0.0, 1.0);
  factor = (3.0 - 2.0 * factor) * (factor * factor);
  v_result = v_to_min + factor * (v_to_max - v_to_min);
}

void vector_map_range_smootherstep(float value,
                                   float fromMin,
                                   float fromMax,
                                   float toMin,
                                   float toMax,
                                   float steps,
                                   vec3 v_value,
                                   vec3 v_from_min,
                                   vec3 v_from_max,
                                   vec3 v_to_min,
                                   vec3 v_to_max,
                                   vec3 v_steps,
                                   float use_clamp,
                                   out float result,
                                   out vec3 v_result)
{
  vec3 factor = safe_divide((v_value - v_from_min), (v_from_max - v_from_min));
  factor = clamp(factor, 0.0, 1.0);
  factor = factor * factor * factor * (factor * (factor * 6.0 - 15.0) + 10.0);
  v_result = v_to_min + factor * (v_to_max - v_to_min);
}

void map_range_linear(float value,
                      float fromMin,
                      float fromMax,
                      float toMin,
                      float toMax,
                      float steps,
                      vec3 v_value,
                      vec3 v_from_min,
                      vec3 v_from_max,
                      vec3 v_to_min,
                      vec3 v_to_max,
                      vec3 v_steps,
                      float use_clamp,
                      out float result,
                      out vec3 v_result)
{
  if (fromMax != fromMin) {
    result = toMin + ((value - fromMin) / (fromMax - fromMin)) * (toMax - toMin);
  }
  else {
    result = 0.0;
  }
}

void map_range_stepped(float value,
                       float fromMin,
                       float fromMax,
                       float toMin,
                       float toMax,
                       float steps,
                       vec3 v_value,
                       vec3 v_from_min,
                       vec3 v_from_max,
                       vec3 v_to_min,
                       vec3 v_to_max,
                       vec3 v_steps,
                       float use_clamp,
                       out float result,
                       out vec3 v_result)
{
  if (fromMax != fromMin) {
    float factor = (value - fromMin) / (fromMax - fromMin);
    factor = (steps > 0.0) ? floor(factor * (steps + 1.0)) / steps : 0.0;
    result = toMin + factor * (toMax - toMin);
  }
  else {
    result = 0.0;
  }
}

void map_range_smoothstep(float value,
                          float fromMin,
                          float fromMax,
                          float toMin,
                          float toMax,
                          float steps,
                          vec3 v_value,
                          vec3 v_from_min,
                          vec3 v_from_max,
                          vec3 v_to_min,
                          vec3 v_to_max,
                          vec3 v_steps,
                          float use_clamp,
                          out float result,
                          out vec3 v_result)
{
  if (fromMax != fromMin) {
    float factor = (fromMin > fromMax) ? 1.0 - smoothstep(fromMax, fromMin, value) :
                                         smoothstep(fromMin, fromMax, value);
    result = toMin + factor * (toMax - toMin);
  }
  else {
    result = 0.0;
  }
}

void map_range_smootherstep(float value,
                            float fromMin,
                            float fromMax,
                            float toMin,
                            float toMax,
                            float steps,
                            vec3 v_value,
                            vec3 v_from_min,
                            vec3 v_from_max,
                            vec3 v_to_min,
                            vec3 v_to_max,
                            vec3 v_steps,
                            float use_clamp,
                            out float result,
                            out vec3 v_result)
{
  if (fromMax != fromMin) {
    float factor = (fromMin > fromMax) ? 1.0 - smootherstep(fromMax, fromMin, value) :
                                         smootherstep(fromMin, fromMax, value);
    result = toMin + factor * (toMax - toMin);
  }
  else {
    result = 0.0;
  }
}
