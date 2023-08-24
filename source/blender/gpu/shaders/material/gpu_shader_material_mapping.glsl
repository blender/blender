/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)

void mapping_mat4(
    vec3 vec, vec4 m0, vec4 m1, vec4 m2, vec4 m3, vec3 minvec, vec3 maxvec, out vec3 outvec)
{
  mat4 mat = mat4(m0, m1, m2, m3);
  outvec = (mat * vec4(vec, 1.0)).xyz;
  outvec = clamp(outvec, minvec, maxvec);
}

void mapping_point(vec3 vector, vec3 location, vec3 rotation, vec3 scale, out vec3 result)
{
  result = (euler_to_mat3(rotation) * (vector * scale)) + location;
}

void mapping_texture(vec3 vector, vec3 location, vec3 rotation, vec3 scale, out vec3 result)
{
  result = safe_divide(transpose(euler_to_mat3(rotation)) * (vector - location), scale);
}

void mapping_vector(vec3 vector, vec3 location, vec3 rotation, vec3 scale, out vec3 result)
{
  result = euler_to_mat3(rotation) * (vector * scale);
}

void mapping_normal(vec3 vector, vec3 location, vec3 rotation, vec3 scale, out vec3 result)
{
  result = normalize(euler_to_mat3(rotation) * safe_divide(vector, scale));
}
