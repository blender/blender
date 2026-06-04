/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Requires all common matrices declared. */

[[node]]
void normal_transform_object_to_world(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  /* Expansion of NormalMatrix. */
  vout = vin * to_float3x3(obj.model_inverse);
}

[[node]]
void normal_transform_world_to_object(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  /* Expansion of NormalMatrixInverse. */
  vout = vin * to_float3x3(obj.model);
}

[[node]]
void direction_transform_object_to_world(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  vout = to_float3x3(obj.model) * vin;
}

[[node]]
void direction_transform_object_to_view(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  const ViewMatrices view = view_matrices_get();
  vout = to_float3x3(obj.model) * vin;
  vout = to_float3x3(view.viewmat) * vout;
}

[[node]]
void direction_transform_view_to_world(float3 vin, float3 &vout)
{
  const ViewMatrices view = view_matrices_get();
  vout = to_float3x3(view.viewinv) * vin;
}

[[node]]
void direction_transform_view_to_object(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  const ViewMatrices view = view_matrices_get();
  vout = to_float3x3(view.viewinv) * vin;
  vout = to_float3x3(obj.model_inverse) * vout;
}

[[node]]
void direction_transform_world_to_view(float3 vin, float3 &vout)
{
  const ViewMatrices view = view_matrices_get();
  vout = to_float3x3(view.viewmat) * vin;
}

[[node]]
void direction_transform_world_to_object(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  vout = to_float3x3(obj.model_inverse) * vin;
}

[[node]]
void point_transform_object_to_world(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  vout = (obj.model * float4(vin, 1.0f)).xyz;
}

[[node]]
void point_transform_object_to_view(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  const ViewMatrices view = view_matrices_get();
  vout = (view.viewmat * (obj.model * float4(vin, 1.0f))).xyz;
}

[[node]]
void point_transform_view_to_world(float3 vin, float3 &vout)
{
  const ViewMatrices view = view_matrices_get();
  vout = (view.viewinv * float4(vin, 1.0f)).xyz;
}

[[node]]
void point_transform_view_to_object(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  const ViewMatrices view = view_matrices_get();
  vout = (obj.model_inverse * (view.viewinv * float4(vin, 1.0f))).xyz;
}

[[node]]
void point_transform_world_to_view(float3 vin, float3 &vout)
{
  const ViewMatrices view = view_matrices_get();
  vout = (view.viewmat * float4(vin, 1.0f)).xyz;
}

[[node]]
void point_transform_world_to_object(float3 vin, float3 &vout)
{
  const ObjectMatrices obj = object_matrices_get();
  vout = (obj.model_inverse * float4(vin, 1.0f)).xyz;
}
