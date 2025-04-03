/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_hash.glsl"

/* White Noise */

void node_white_noise_1d(vec3 vector, float w, out float value, out vec4 color)
{
  value = hash_float_to_float(w);
  color = vec4(hash_float_to_vec3(w), 1.0);
}

void node_white_noise_2d(vec3 vector, float w, out float value, out vec4 color)
{
  value = hash_vec2_to_float(vector.xy);
  color = vec4(hash_vec2_to_vec3(vector.xy), 1.0);
}

void node_white_noise_3d(vec3 vector, float w, out float value, out vec4 color)
{
  value = hash_vec3_to_float(vector);
  color = vec4(hash_vec3_to_vec3(vector), 1.0);
}

void node_white_noise_4d(vec3 vector, float w, out float value, out vec4 color)
{
  value = hash_vec4_to_float(vec4(vector, w));
  color = vec4(hash_vec4_to_vec3(vec4(vector, w)), 1.0);
}
