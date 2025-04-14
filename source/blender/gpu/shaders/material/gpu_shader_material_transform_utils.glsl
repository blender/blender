/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Requires all common matrices declared. */

void normal_transform_object_to_world(float3 vin, out float3 vout)
{
  /* Expansion of NormalMatrix. */
  vout = vin * to_float3x3(drw_modelinv());
}

void normal_transform_world_to_object(float3 vin, out float3 vout)
{
  /* Expansion of NormalMatrixInverse. */
  vout = vin * to_float3x3(drw_modelmat());
}

void direction_transform_object_to_world(float3 vin, out float3 vout)
{
  vout = to_float3x3(drw_modelmat()) * vin;
}

void direction_transform_object_to_view(float3 vin, out float3 vout)
{
  vout = to_float3x3(drw_modelmat()) * vin;
  vout = to_float3x3(drw_view().viewmat) * vout;
}

void direction_transform_view_to_world(float3 vin, out float3 vout)
{
  vout = to_float3x3(drw_view().viewinv) * vin;
}

void direction_transform_view_to_object(float3 vin, out float3 vout)
{
  vout = to_float3x3(drw_view().viewinv) * vin;
  vout = to_float3x3(drw_modelinv()) * vout;
}

void direction_transform_world_to_view(float3 vin, out float3 vout)
{
  vout = to_float3x3(drw_view().viewmat) * vin;
}

void direction_transform_world_to_object(float3 vin, out float3 vout)
{
  vout = to_float3x3(drw_modelinv()) * vin;
}

void point_transform_object_to_world(float3 vin, out float3 vout)
{
  vout = (drw_modelmat() * float4(vin, 1.0f)).xyz;
}

void point_transform_object_to_view(float3 vin, out float3 vout)
{
  vout = (drw_view().viewmat * (drw_modelmat() * float4(vin, 1.0f))).xyz;
}

void point_transform_view_to_world(float3 vin, out float3 vout)
{
  vout = (drw_view().viewinv * float4(vin, 1.0f)).xyz;
}

void point_transform_view_to_object(float3 vin, out float3 vout)
{
  vout = (drw_modelinv() * (drw_view().viewinv * float4(vin, 1.0f))).xyz;
}

void point_transform_world_to_view(float3 vin, out float3 vout)
{
  vout = (drw_view().viewmat * float4(vin, 1.0f)).xyz;
}

void point_transform_world_to_object(float3 vin, out float3 vout)
{
  vout = (drw_modelinv() * float4(vin, 1.0f)).xyz;
}
