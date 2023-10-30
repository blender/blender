/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void normal_transform_object_to_world(vec3 vin, out vec3 vout)
{
  vout = normal_object_to_world(vin);
}

void normal_transform_world_to_object(vec3 vin, out vec3 vout)
{
  vout = normal_world_to_object(vin);
}

void direction_transform_object_to_world(vec3 vin, out vec3 vout)
{
  vout = transform_direction(ModelMatrix, vin);
}

void direction_transform_object_to_view(vec3 vin, out vec3 vout)
{
  vout = transform_direction(ModelMatrix, vin);
  vout = transform_direction(ViewMatrix, vout);
}

void direction_transform_view_to_world(vec3 vin, out vec3 vout)
{
  vout = transform_direction(ViewMatrixInverse, vin);
}

void direction_transform_view_to_object(vec3 vin, out vec3 vout)
{
  vout = transform_direction(ViewMatrixInverse, vin);
  vout = transform_direction(ModelMatrixInverse, vout);
}

void direction_transform_world_to_view(vec3 vin, out vec3 vout)
{
  vout = transform_direction(ViewMatrix, vin);
}

void direction_transform_world_to_object(vec3 vin, out vec3 vout)
{
  vout = transform_direction(ModelMatrixInverse, vin);
}

void point_transform_object_to_world(vec3 vin, out vec3 vout)
{
  vout = point_object_to_world(vin);
}

void point_transform_object_to_view(vec3 vin, out vec3 vout)
{
  vout = point_object_to_view(vin);
}

void point_transform_view_to_world(vec3 vin, out vec3 vout)
{
  vout = point_view_to_world(vin);
}

void point_transform_view_to_object(vec3 vin, out vec3 vout)
{
  vout = point_view_to_object(vin);
}

void point_transform_world_to_view(vec3 vin, out vec3 vout)
{
  vout = point_world_to_view(vin);
}

void point_transform_world_to_object(vec3 vin, out vec3 vout)
{
  vout = point_world_to_object(vin);
}
