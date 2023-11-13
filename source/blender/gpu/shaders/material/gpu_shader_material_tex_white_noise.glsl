/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)

/* White Noise */

void node_white_noise_1d(vec3 vector, float w, out float value, out vec4 color)
{
  value = hash_float_to_float(w);
  color.xyz = hash_float_to_vec3(w);
}

void node_white_noise_2d(vec3 vector, float w, out float value, out vec4 color)
{
  value = hash_vec2_to_float(vector.xy);
  color.xyz = hash_vec2_to_vec3(vector.xy);
}

void node_white_noise_3d(vec3 vector, float w, out float value, out vec4 color)
{
  value = hash_vec3_to_float(vector);
  color.xyz = hash_vec3_to_vec3(vector);
}

void node_white_noise_4d(vec3 vector, float w, out float value, out vec4 color)
{
  value = hash_vec4_to_float(vec4(vector, w));
  color.xyz = hash_vec4_to_vec3(vec4(vector, w));
}
