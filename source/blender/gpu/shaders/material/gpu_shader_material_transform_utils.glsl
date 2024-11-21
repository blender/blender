/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Requires all common matrices declared. */

void normal_transform_object_to_world(vec3 vin, out vec3 vout)
{
  /* Expansion of NormalMatrix. */
  vout = vin * to_float3x3(ModelMatrixInverse);
}

void normal_transform_world_to_object(vec3 vin, out vec3 vout)
{
  /* Expansion of NormalMatrixInverse. */
  vout = vin * to_float3x3(ModelMatrix);
}

void direction_transform_object_to_world(vec3 vin, out vec3 vout)
{
  vout = to_float3x3(ModelMatrix) * vin;
}

void direction_transform_object_to_view(vec3 vin, out vec3 vout)
{
  vout = to_float3x3(ModelMatrix) * vin;
  vout = to_float3x3(ViewMatrix) * vout;
}

void direction_transform_view_to_world(vec3 vin, out vec3 vout)
{
  vout = to_float3x3(ViewMatrixInverse) * vin;
}

void direction_transform_view_to_object(vec3 vin, out vec3 vout)
{
  vout = to_float3x3(ViewMatrixInverse) * vin;
  vout = to_float3x3(ModelMatrixInverse) * vout;
}

void direction_transform_world_to_view(vec3 vin, out vec3 vout)
{
  vout = to_float3x3(ViewMatrix) * vin;
}

void direction_transform_world_to_object(vec3 vin, out vec3 vout)
{
  vout = to_float3x3(ModelMatrixInverse) * vin;
}

void point_transform_object_to_world(vec3 vin, out vec3 vout)
{
  vout = (ModelMatrix * vec4(vin, 1.0)).xyz;
}

void point_transform_object_to_view(vec3 vin, out vec3 vout)
{
  vout = (ViewMatrix * (ModelMatrix * vec4(vin, 1.0))).xyz;
}

void point_transform_view_to_world(vec3 vin, out vec3 vout)
{
  vout = (ViewMatrixInverse * vec4(vin, 1.0)).xyz;
}

void point_transform_view_to_object(vec3 vin, out vec3 vout)
{
  vout = (ModelMatrixInverse * (ViewMatrixInverse * vec4(vin, 1.0))).xyz;
}

void point_transform_world_to_view(vec3 vin, out vec3 vout)
{
  vout = (ViewMatrix * vec4(vin, 1.0)).xyz;
}

void point_transform_world_to_object(vec3 vin, out vec3 vout)
{
  vout = (ModelMatrixInverse * vec4(vin, 1.0)).xyz;
}
