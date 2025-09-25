/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_euler_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"

float3 rotate_around_axis(float3 p, float3 axis, float angle)
{
  float costheta = cos(angle);
  float sintheta = sin(angle);
  float3 r;

  r.x = ((costheta + (1.0f - costheta) * axis.x * axis.x) * p.x) +
        (((1.0f - costheta) * axis.x * axis.y - axis.z * sintheta) * p.y) +
        (((1.0f - costheta) * axis.x * axis.z + axis.y * sintheta) * p.z);

  r.y = (((1.0f - costheta) * axis.x * axis.y + axis.z * sintheta) * p.x) +
        ((costheta + (1.0f - costheta) * axis.y * axis.y) * p.y) +
        (((1.0f - costheta) * axis.y * axis.z - axis.x * sintheta) * p.z);

  r.z = (((1.0f - costheta) * axis.x * axis.z - axis.y * sintheta) * p.x) +
        (((1.0f - costheta) * axis.y * axis.z + axis.x * sintheta) * p.y) +
        ((costheta + (1.0f - costheta) * axis.z * axis.z) * p.z);

  return r;
}

void node_vector_rotate_axis_angle(float3 vector_in,
                                   float3 center,
                                   float3 axis,
                                   float angle,
                                   float3 rotation,
                                   float invert,
                                   out float3 vec)
{
  vec = (length(axis) != 0.0f) ?
            rotate_around_axis(vector_in - center, normalize(axis), angle * invert) + center :
            vector_in;
}

void node_vector_rotate_axis_x(float3 vector_in,
                               float3 center,
                               float3 axis,
                               float angle,
                               float3 rotation,
                               float invert,
                               out float3 vec)
{
  vec = rotate_around_axis(vector_in - center, float3(1.0f, 0.0f, 0.0f), angle * invert) + center;
}

void node_vector_rotate_axis_y(float3 vector_in,
                               float3 center,
                               float3 axis,
                               float angle,
                               float3 rotation,
                               float invert,
                               out float3 vec)
{
  vec = rotate_around_axis(vector_in - center, float3(0.0f, 1.0f, 0.0f), angle * invert) + center;
}

void node_vector_rotate_axis_z(float3 vector_in,
                               float3 center,
                               float3 axis,
                               float angle,
                               float3 rotation,
                               float invert,
                               out float3 vec)
{
  vec = rotate_around_axis(vector_in - center, float3(0.0f, 0.0f, 1.0f), angle * invert) + center;
}

void node_vector_rotate_euler_xyz(float3 vector_in,
                                  float3 center,
                                  float3 axis,
                                  float angle,
                                  float3 rotation,
                                  float invert,
                                  out float3 vec)
{
  float3x3 rmat = (invert < 0.0f) ? transpose(from_rotation(EulerXYZ::from_float3(rotation))) :
                                    from_rotation(EulerXYZ::from_float3(rotation));
  vec = rmat * (vector_in - center) + center;
}
